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


#ifndef __nanojit_Native__
#define __nanojit_Native__

// define PEDANTIC=1 to ignore specialized forms, force general forms
// for everything, far branches, extra page-linking, etc.  This will
// flush out many corner cases.

#define PEDANTIC 0
#if PEDANTIC
#  define UNLESS_PEDANTIC(...)
#  define IF_PEDANTIC(...) __VA_ARGS__
#else
#  define UNLESS_PEDANTIC(...) __VA_ARGS__
#  define IF_PEDANTIC(...)
#endif

#ifdef NANOJIT_IA32
#include "Nativei386.h"
#elif defined(NANOJIT_ARM)
#include "NativeARM.h"
#elif defined(NANOJIT_PPC)
#include "NativePPC.h"
#elif defined(NANOJIT_SPARC)
#include "NativeSparc.h"
#elif defined(NANOJIT_X64)
#include "NativeX64.h"
#elif defined(NANOJIT_SH4)
#include "NativeSH4.h"
#elif defined(NANOJIT_MIPS)
#include "NativeMIPS.h"
#else
#error "unknown nanojit architecture"
#endif

#ifndef NJ_USES_IMMD_POOL
#  define NJ_USES_IMMD_POOL 0
#endif

#ifndef NJ_JTBL_SUPPORTED
#  define NJ_JTBL_SUPPORTED 0
#endif

#ifndef NJ_EXPANDED_LOADSTORE_SUPPORTED
#  define NJ_EXPANDED_LOADSTORE_SUPPORTED 0
#endif

#ifndef NJ_F2I_SUPPORTED
#  define NJ_F2I_SUPPORTED 0
#endif

#ifndef NJ_SOFTFLOAT_SUPPORTED
#  define NJ_SOFTFLOAT_SUPPORTED 0
#endif

#ifndef NJ_DIVI_SUPPORTED
#  define NJ_DIVI_SUPPORTED 0
#endif

#if NJ_SOFTFLOAT_SUPPORTED
    #define CASESF(x)   case x
#else
    #define CASESF(x)
#endif

namespace nanojit {

    class Fragment;
    struct SideExit;

    struct GuardRecord
    {
        void* jmp;
        GuardRecord* next;
        SideExit* exit;
        // profiling stuff
        verbose_only( uint32_t profCount; )
        verbose_only( uint32_t profGuardID; )
        verbose_only( GuardRecord* nextInFrag; )
    };

    struct SideExit
    {
        GuardRecord* guards;
        Fragment* from;
        Fragment* target;

        void addGuard(GuardRecord* gr)
        {
            NanoAssert(gr->next == NULL);
            NanoAssert(guards != gr);
            gr->next = guards;
            guards = gr;
        }
    };
}

    #define isSPorFP(r)     ( (r)==SP || (r)==FP )

    #if defined(NJ_VERBOSE)
        inline char cvaltoa(unsigned char u) {
            return u<10 ? u+'0' : u+'a'-10;
        }

        inline char* appendHexVals(char* str, char* valFrom, char* valTo) {
            NanoAssert(valFrom <= valTo);
            str += VMPI_strlen(str);
            for(char* ch = valFrom; ch < valTo; ch++) {
                unsigned char u = (unsigned char)*ch;
                *str++ = cvaltoa(u >> 4);
                *str++ = cvaltoa(u &  0xf);
                *str++ = ' ';
            }
            *str = '\0';
            return str;
        }

        inline char* padTo(char* str, int n, char c=' ') {
            char* start = str + VMPI_strlen(str);
            char* end = &str[n];
            while(start < end)
                *start++ = c;
            *end = '\0';
            return end;
        }

        // Used for printing native instructions.  Like Assembler::outputf(),
        // but only outputs if LC_Native is set.  Also prepends the output
        // with the address of the current native instruction.
        #define asm_output(...) do {                                            \
            if (_logc->lcbits & LC_Native) {                                    \
                outline[0]='\0';                                                \
                VMPI_sprintf(outline, "%p  ", _nIns);                           \
                if (_logc->lcbits & LC_Bytes) {                                 \
                    appendHexVals(outline, (char*)_nIns, (char*)_nInsAfter);    \
                    padTo(outline, 3*15);                                       \
                }                                                               \
                VMPI_sprintf(outline + VMPI_strlen(outline), ##__VA_ARGS__);    \
                output();                                                       \
                _nInsAfter = _nIns;                                             \
            }                                                                   \
        } while (0) /* no semi */
        #define gpn(r)                  regNames[(REGNUM(r))]
    #else
        #define asm_output(...)
        #define gpn(r)
    #endif /* NJ_VERBOSE */

#endif // __nanojit_Native__
