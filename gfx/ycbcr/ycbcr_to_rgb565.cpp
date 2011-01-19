/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Tom Brinkman <reportbase@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "ycbcr_to_rgb565.h"

//The logic for have_ycbcr_to_rgb565 is taken from pixman-cpu.c

#if !defined (__arm__)

int have_ycbcr_to_rgb565 ()
{
    return 0;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <elf.h>

#ifdef ANDROID

int have_ycbcr_to_rgb565 ()
{
    static int have_ycbcr_to_rgb565_initialized = 0;
    static int arm_has_neon = 0;
    if (!have_ycbcr_to_rgb565_initialized)
    {
        have_ycbcr_to_rgb565_initialized = 1;

        char buf[1024];
        const char* ver_token = "CPU architecture: ";
        FILE* f = fopen("/proc/cpuinfo", "r");
        if (!f) {
	        return 0;
        }

        fread(buf, sizeof(char), 1024, f);
        arm_has_neon = strstr(buf, "neon") != NULL;
        fclose(f);
    }
    return arm_has_neon;
}

#else

int have_ycbcr_to_rgb565 ()
{
    static int have_ycbcr_to_rgb565_initialized = 0;
    static int arm_has_neon = 0;
    if (!have_ycbcr_to_rgb565_initialized)
    {
        have_ycbcr_to_rgb565_initialized = 1;
        int fd;
        Elf32_auxv_t aux;

        fd = open ("/proc/self/auxv", O_RDONLY);
        if (fd >= 0)
        {
            while (read (fd, &aux, sizeof(Elf32_auxv_t)) == sizeof(Elf32_auxv_t))
            {
                if (aux.a_type == AT_HWCAP)
                {
                    uint32_t hwcap = aux.a_un.a_val;
                    arm_has_neon = (hwcap & 4096) != 0;
                    break;
                }
            }
            close (fd);
         }
    }

    return arm_has_neon;
}

#endif //ANDROID

#endif //_MSC_VER

