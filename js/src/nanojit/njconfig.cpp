/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

#include "nanojit.h"

#ifdef FEATURE_NANOJIT

namespace nanojit
{
#ifdef NANOJIT_IA32
    static int getCpuFeatures()
    {
        int features = 0;
    #if defined _MSC_VER
        __asm
        {
            pushad
            mov eax, 1
            cpuid
            mov features, edx
            popad
        }
    #elif defined __GNUC__
        asm("xchg %%esi, %%ebx\n" /* we can't clobber ebx on gcc (PIC register) */
            "mov $0x01, %%eax\n"
            "cpuid\n"
            "mov %%edx, %0\n"
            "xchg %%esi, %%ebx\n"
            : "=m" (features)
            : /* We have no inputs */
            : "%eax", "%esi", "%ecx", "%edx"
           );
    #elif defined __SUNPRO_C || defined __SUNPRO_CC
        asm("push %%ebx\n"
            "mov $0x01, %%eax\n"
            "cpuid\n"
            "pop %%ebx\n"
            : "=d" (features)
            : /* We have no inputs */
            : "%eax", "%ecx"
           );
    #endif
        return features;
    }
#endif

    Config::Config()
    {
        VMPI_memset(this, 0, sizeof(*this));

        cseopt = true;

#ifdef NANOJIT_IA32
        int const features = getCpuFeatures();
        i386_sse2 = (features & (1<<26)) != 0;
        i386_use_cmov = (features & (1<<15)) != 0;
        i386_fixed_esp = false;
#endif
        harden_function_alignment = false;
        harden_nop_insertion = false;

#if defined(NANOJIT_ARM)
        NanoStaticAssert(NJ_COMPILER_ARM_ARCH >= 5 && NJ_COMPILER_ARM_ARCH <= 7);
        arm_arch = NJ_COMPILER_ARM_ARCH;
        arm_vfp = (arm_arch >= 7);

    #if defined(DEBUG) || defined(_DEBUG)
        arm_show_stats = true;
    #else
        arm_show_stats = false;
    #endif

        soft_float = !arm_vfp;

#endif // NANOJIT_ARM
    }
}
#endif /* FEATURE_NANOJIT */
