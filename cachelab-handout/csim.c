#include <getopt.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "cachelab.h"

typedef unsigned char u_int8;

typedef struct
{
    unsigned long tag;
    short op;
    u_int8 valid;
} Line;

int c2i(char ch)
{
    if (isdigit(ch))
    {
        return ch - 48;
    }
    else if (ch < 'A' || (ch > 'F' && ch < 'a') || ch > 'z')
    {
        return -1;
    }
    else if (isalpha(ch))
    {
        return isupper(ch) ? ch - 55 : ch - 87;
    }
    else
    {
        return -1;
    }
}
unsigned long hex2int(char *hex)
{
    int len, bits, i;
    len = strlen(hex);
    unsigned long temp, num = 0;
    for (i = 0; i < len; i++)
    {
        temp = c2i(hex[i]);
        bits = (len - i - 1) * 4;
        temp <<= bits;
        num |= temp;
    }
    return num;
}

int main(int argc, char *argv[])
{
    int verbose = 0;
    int set_bit, lines, block_bit, S;
    unsigned long address;
    int opt;
    FILE *fp;
    char line[100];
    char *pch;
    char mode;
    int op = 0, op_min, line_min;
    int hits = 0, misses = 0, evictions = 0;
    int satisfy = 0;

    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            printf("-h: Optional help flag that prints usage info\n");
            printf("-v: Optional verbose flag that displays trace info\n");
            printf("-s <s>: Number of set index bits (S = 2s is the number of sets)\n");
            printf("-E <E>: Associativity (number of lines per set)\n");
            printf("-b <b>: Number of block bits (B = 2b is the block size)\n");
            printf("-t <tracefile>: Name of the valgrind trace to replay\n");
            return 0;
            break;
        case 'v':
            verbose = 1;
            break;
        case 's':
            set_bit = atoi(optarg);
            satisfy++;
            break;
        case 'E':
            lines = atoi(optarg);
            satisfy++;
            break;
        case 'b':
            block_bit = atoi(optarg);
            satisfy++;
            break;
        case 't':
            fp = fopen(optarg, "r");
            satisfy++;
            break;
        default:
            printf("option %d is not supported try use -h.", opt);
            break;
        }
    }
    if (satisfy < 4)
    {
        return 0;
    }
    S = 1 << set_bit;
    Line **cache = malloc(sizeof(int *) * S);

    if (cache)
    {
        for (size_t i = 0; i < S; i++)
        {
            cache[i] = malloc(sizeof(Line) * lines);
        }
    }

    while (fgets(line, sizeof(line), fp))
    {
        char ins[20];
        strcpy(ins, line);
        if (line[0] == 'I')
            continue;
        u_int8 instruction = 0;
        op++;
        op_min = 0;
        pch = strtok(line, " ,");
        while (pch)
        {
            if (instruction == 0)
            {
                mode = pch[0];
            }
            else if (instruction == 1)
            {
                address = hex2int(pch);
            }
            instruction++;
            pch = strtok(0, " ,");
        }
        // execute cache
        unsigned long ea = address >> block_bit; // remove offset bits
        int si = (S - 1) & ea;                   // set index
        unsigned long tag = ea >> set_bit;       // get tag id;
        if (verbose)
            printf("##add: %lu, effective address %lu, set index %d, tag id %lu\n", address, ea, si, tag);
        u_int8 found = 0;
        int amount = 0;
        int ui = 0; // usable index
        Line *cache_set = cache[si];
        for (size_t i = 0; i < lines; i++)
        {
            if (cache_set[i].valid == 1)
            {
                amount++;
            }
            else
            {
                ui = i;
                continue;
            }
            if (cache_set[i].tag == tag)
            {
                found = 1;
                cache_set[i].op = op;
                break;
            }
            if (op_min == 0 || cache_set[i].op < op_min)
            {
                op_min = cache_set[i].op;
                line_min = i;
            }
        }
        if (found)
        {
            if (verbose)
                printf("%s found ", ins);
            hits++;
        }
        else if (amount < lines)
        {
            // add one
            cache_set[ui].tag = tag;
            cache_set[ui].valid = 1;
            cache_set[ui].op = op;
            misses++;
            if (verbose)
                printf("%s misses ", ins);
        }
        else
        {
            // eject
            cache_set[line_min].tag = tag;
            cache_set[line_min].valid = 1;
            cache_set[line_min].op = op;
            misses++;
            evictions++;
            if (verbose)
                printf("%s misses evicted ", ins);
        }
        if (mode == 'M')
        {
            hits++;
            if (verbose)
                printf("hits (M)\n");
        }
        else
        {
            if (verbose)
                printf("\n");
        }
    }
    fclose(fp);
    printSummary(hits, misses, evictions);
    return 0;
}

