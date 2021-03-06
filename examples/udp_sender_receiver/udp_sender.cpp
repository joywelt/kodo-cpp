// Copyright Steinwurf ApS 2016.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <sys/time.h>
#endif

#include <sys/types.h>

#include <kodocpp/kodocpp.hpp>

#ifdef _WIN32
#include <windows.h>

void sleep_here(uint32_t milliseconds)
{
    Sleep(milliseconds);
}
#else
void sleep_here(uint32_t milliseconds)
{
    usleep(milliseconds * 1000); // takes microseconds
}
#endif


int main(int argc, char* argv[])
{
    // Variables needed for the network / socket usage
    int32_t socket_descriptor;
    int32_t return_code;
    uint32_t i;

    struct sockaddr_in remote_address;
    struct hostent* host;

    uint32_t delay = 0; // Delay between packets

    // Initialize winsock if on Windows
#ifdef _WIN32
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;

    return_code = WSAStartup(versionWanted, &wsaData);

    if (return_code != 0)
    {
        // Tell the user that we could not find a usable
        // Winsock DLL.
        printf("WSAStartup failed with error: %d\n", return_code);
        exit(1);
    }
#endif

    // Check command line args
    if (argc != 6)
    {
        printf("usage: %s <server> <port> <symbols> <packets> <delay_ms>\n",
               argv[0]);

        exit(1);
    }

    // Get the delay
    delay = atol(argv[5]);
    printf("Delay is: %u milliseconds\n", delay);

    // Get server IP address (no check if input is IP address or DNS name)
    host = gethostbyname(argv[1]);
    if (host == NULL)
    {
        printf("%s: unknown host '%s' \n", argv[0], argv[1]);
        exit(1);
    }

    printf("Sending data to '%s:%d' (IP: %s) \n", host->h_name,
           atoi(argv[2]), inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));

    remote_address.sin_family = host->h_addrtype;
    memcpy((char*) &remote_address.sin_addr.s_addr,
           host->h_addr_list[0], host->h_length);
    remote_address.sin_port = htons(atoi(argv[2]));

    // Socket creation
    socket_descriptor = (int32_t)socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        printf("%s: cannot open socket \n", argv[0]);
        exit(1);
    }

    // Variables needed for the coding
    uint32_t symbols = atoi(argv[3]);
    uint32_t symbol_size = 160;
    uint32_t packets = atoi(argv[4]);

    if (packets < symbols)
    {
        printf("%s: number of packets should be higher than %d \n",
               argv[0], symbols);
        exit(1);
    }

    // Initialize the factory with the chosen symbols and symbol size
    kodocpp::encoder_factory encoder_factory(
        kodocpp::codec::on_the_fly,
        kodocpp::field::binary8,
        symbols, symbol_size);

    kodocpp::encoder encoder = encoder_factory.build();

    // Create the buffer needed for the payload
    uint32_t payload_size = encoder.payload_size();
    std::vector<uint8_t> payload(payload_size);

    // Create some data to encode
    std::vector<uint8_t> data_in(encoder.block_size());
    std::generate(data_in.begin(), data_in.end(), rand);

    // Send data
    for (i = 0; i < packets; ++i)
    {
        // Add a new symbol if the encoder rank is less than the maximum number
        // of symbols
        if (encoder.rank() < encoder.symbols())
        {
            // The rank of an encoder  indicates how many symbols have been
            // added, i.e. how many symbols are available for encoding
            uint32_t rank = encoder.rank();

            // Calculate the offset to the next symbol to insert
            uint8_t* symbol = data_in.data() + rank * encoder.symbol_size();
            encoder.set_const_symbol(rank, symbol, encoder.symbol_size());
        }

        uint32_t bytes_used = encoder.write_payload(payload.data());
        printf("Payload generated by encoder, rank = %d, bytes used = %d\n",
               encoder.rank(), bytes_used);

        return_code = sendto(socket_descriptor, (const char*)payload.data(),
                             bytes_used, 0, (struct sockaddr*) &remote_address,
                             sizeof(remote_address));

        if (return_code < 0)
        {
            printf("%s: cannot send packet %d \n", argv[0], i - 1);
            exit(1);
        }

        sleep_here(delay);
    }

    return 0;
}
