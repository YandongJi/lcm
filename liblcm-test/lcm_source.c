// file: lcm_source.c
// desc: utility to basically spew garbage on the LC channels to use up 
//       bandwidth.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/time.h>
#include <time.h>

#include <lcm/lcm.h>

#define DEFAULT_TRANSMIT_INTERVAL_USEC 10000

static void
make_msg_channel (char *buf, int maxlen) 
{
    char *cset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;
    int ind;
    int nchars = strlen(cset);
    if (0 == maxlen) return;

    for (i=0; i<maxlen-1; i++) {
        ind = (int) (nchars * ((double)rand() / (RAND_MAX + 1.0)));
        buf[i] = cset[ind];
    }
    buf[maxlen-1] = 0;
}

static void 
usage()
{
    printf("usage: lcm_source [OPTIONS]\n"
           "\n"
           "periodically transmits LC messages.\n"
           "\n"
           "  -h     prints this help text and exits\n"
           "  -m s   transmits messages on channel s (otherwise random channel)\n"
           "  -p t   wait t microseconds between transmissions (default %d)\n"
           "  -s n   transmit packets of size n bytes.  If n is 0 (default),\n"
           "         then packets are randomly sized between 1 and 65,000\n"
           "         bytes\n"
           "  -v     verbose mode. Prints a summary of each packet\n",
           DEFAULT_TRANSMIT_INTERVAL_USEC);
    exit(1);
}

int main(int argc, char **argv)
{
    int status;
    int verbose = 0;
    int random_channel = 1;
    char channel[80];
    char data[65536];
    int transmit_interval_usec = DEFAULT_TRANSMIT_INTERVAL_USEC;
    int packet_sz = 0;
    memset (data, 0, sizeof(data));

    char *optstring = "hm:p:s:v";
    char c;

    while ((c = getopt_long (argc, argv, optstring, NULL, 0)) >= 0)
    {
        switch (c) {
            case 'h':
                usage();
                break;
            case 'm':
                random_channel = 0;
                strncpy (channel, optarg, sizeof(channel));
                break;
            case 'p':
                transmit_interval_usec = atoi(optarg);
                if (transmit_interval_usec < 0) {
                    usage();
                }
                break;
            case 's':
                packet_sz = atoi (optarg);
                if (packet_sz < 0) usage();
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                usage();
                break;
        };
    }

    lcm_params_t lcm_args;
    lcm_params_init_defaults (&lcm_args);
    lcm_args.transmit_only = 1;

    lcm_t *lcm = lcm_create();
    if (! lcm) {
        fprintf(stderr, "couldn't allocate lcm_t\n");
        return 1;
    }
    status = lcm_init (lcm, &lcm_args);
    if (0 != status) {
        fprintf(stderr, "error initializing lcm context\n");
        return 1;
    }

    srand(time(NULL));

    uint32_t seqno = 0;
    while(1) {
        int sz = 0;

        // pick a random message type if no fixed type was specified...
        if (random_channel) {
            make_msg_channel (channel, 10);
        }

        // pick a random or fixed packet size (depending on cmd-line option)
        if (0 == packet_sz) {
            sz = 1 + (int) (65000 * ((double)rand() / (RAND_MAX + 1.0)));
        } else {
            sz = packet_sz;
        }

        if (sz >= sizeof (seqno)) memcpy (data, &seqno, sizeof (seqno));

        // spew
        lcm_publish (lcm, channel, data, sz);

        if (verbose) {
            printf("packet type: [%s] size: %d\n", channel, sz);
        }

        if (transmit_interval_usec > 0) {
            // sleep...
            struct timespec ts = {
                .tv_sec = (int) (transmit_interval_usec / 1000000),
                .tv_nsec = (transmit_interval_usec % 1000000) * 1000
            };
            if (0 != nanosleep (&ts, NULL)) {
                perror ("nanosleep");
            }
        }
        seqno++;
    }

    lcm_destroy (lcm);
    
    return 0;
}
