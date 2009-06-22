/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Generate CPU-specific bit-size and similar #defines.
 */
#include <stdio.h>
#include <stdlib.h>

#ifdef CROSS_COMPILE
#include <prtypes.h>
#endif

/************************************************************************/

#ifdef __GNUC__
#define NS_NEVER_INLINE __attribute__((noinline))
#else
#define NS_NEVER_INLINE
#endif

#ifdef __SUNPRO_C
static int StackGrowthDirection(int *dummy1addr);
#pragma no_inline(StackGrowthDirection)
#endif

static int NS_NEVER_INLINE StackGrowthDirection(int *dummy1addr)
{
    int dummy2;

    return (&dummy2 < dummy1addr) ? -1 : 1;
}

int main(int argc, char **argv)
{
    int dummy1;

    printf("#ifndef js_cpucfg___\n");
    printf("#define js_cpucfg___\n\n");

    printf("/* AUTOMATICALLY GENERATED - DO NOT EDIT */\n\n");

#ifdef CROSS_COMPILE
#if defined(__APPLE__)
    /*
     * Darwin NSPR uses the same MDCPUCFG (_darwin.cfg) for multiple
     * processors, and determines which processor to configure for based
     * on compiler predefined macros.  We do the same thing here.
     */
    printf("#ifdef __LITTLE_ENDIAN__\n");
    printf("#define IS_LITTLE_ENDIAN 1\n");
    printf("#undef  IS_BIG_ENDIAN\n");
    printf("#else\n");
    printf("#undef  IS_LITTLE_ENDIAN\n");
    printf("#define IS_BIG_ENDIAN 1\n");
    printf("#endif\n\n");
#elif defined(IS_LITTLE_ENDIAN)
    printf("#define IS_LITTLE_ENDIAN 1\n");
    printf("#undef  IS_BIG_ENDIAN\n\n");
#elif defined(IS_BIG_ENDIAN)
    printf("#undef  IS_LITTLE_ENDIAN\n");
    printf("#define IS_BIG_ENDIAN 1\n\n");
#else
#error "Endianess not defined."
#endif

#else

    /*
     * We don't handle PDP-endian or similar orders: if a short is big-endian,
     * so must int and long be big-endian for us to generate the IS_BIG_ENDIAN
     * #define and the IS_LITTLE_ENDIAN #undef.
     */
    {
        int big_endian = 0, little_endian = 0, ntests = 0;

        if (sizeof(short) == 2) {
            /* force |volatile| here to get rid of any compiler optimisations
             * (var in register etc.) which may be appiled to |auto| vars -
             * even those in |union|s...
             * (|static| is used to get the same functionality for compilers
             * which do not honor |volatile|...).
             */
            volatile static union {
                short i;
                char c[2];
            } u;

            u.i = 0x0102;
            big_endian += (u.c[0] == 0x01 && u.c[1] == 0x02);
            little_endian += (u.c[0] == 0x02 && u.c[1] == 0x01);
            ntests++;
        }

        if (sizeof(int) == 4) {
            /* force |volatile| here ... */
            volatile static union {
                int i;
                char c[4];
            } u;

            u.i = 0x01020304;
            big_endian += (u.c[0] == 0x01 && u.c[1] == 0x02 &&
                           u.c[2] == 0x03 && u.c[3] == 0x04);
            little_endian += (u.c[0] == 0x04 && u.c[1] == 0x03 &&
                              u.c[2] == 0x02 && u.c[3] == 0x01);
            ntests++;
        }

        if (sizeof(long) == 8) {
            /* force |volatile| here ... */
            volatile static union {
                long i;
                char c[8];
            } u;

            /*
             * Write this as portably as possible: avoid 0x0102030405060708L
             * and <<= 32.
             */
            u.i = 0x01020304;
            u.i <<= 16, u.i <<= 16;
            u.i |= 0x05060708;
            big_endian += (u.c[0] == 0x01 && u.c[1] == 0x02 &&
                           u.c[2] == 0x03 && u.c[3] == 0x04 &&
                           u.c[4] == 0x05 && u.c[5] == 0x06 &&
                           u.c[6] == 0x07 && u.c[7] == 0x08);
            little_endian += (u.c[0] == 0x08 && u.c[1] == 0x07 &&
                              u.c[2] == 0x06 && u.c[3] == 0x05 &&
                              u.c[4] == 0x04 && u.c[5] == 0x03 &&
                              u.c[6] == 0x02 && u.c[7] == 0x01);
            ntests++;
        }

        if (big_endian && big_endian == ntests) {
            printf("#undef  IS_LITTLE_ENDIAN\n");
            printf("#define IS_BIG_ENDIAN 1\n\n");
        } else if (little_endian && little_endian == ntests) {
            printf("#define IS_LITTLE_ENDIAN 1\n");
            printf("#undef  IS_BIG_ENDIAN\n\n");
        } else {
            fprintf(stderr, "%s: unknown byte order"
                    "(big_endian=%d, little_endian=%d, ntests=%d)!\n",
                    argv[0], big_endian, little_endian, ntests);
            return EXIT_FAILURE;
        }
    }

#endif /* CROSS_COMPILE */

    printf("#define JS_STACK_GROWTH_DIRECTION (%d)\n", StackGrowthDirection(&dummy1));

    printf("#endif /* js_cpucfg___ */\n");

    return EXIT_SUCCESS;
}

