/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "assembler/assembler/MacroAssemblerX86Common.h"

#include "assembler/wtf/Platform.h"

#ifdef _MSC_VER
#ifdef JS_CODEGEN_X64
/* for __cpuid */
#include <intrin.h>
#endif
#endif

using namespace JSC;
MacroAssemblerX86Common::SSECheckState MacroAssemblerX86Common::s_sseCheckState = NotCheckedSSE;

#ifdef DEBUG
bool MacroAssemblerX86Common::s_floatingPointDisabled = false;
bool MacroAssemblerX86Common::s_SSE3Disabled = false;
bool MacroAssemblerX86Common::s_SSE4Disabled = false;
#endif

void MacroAssemblerX86Common::setSSECheckState()
{
    // Default the flags value to zero; if the compiler is
    // not MSVC or GCC we will read this as SSE2 not present.
    int flags_edx = 0;
    int flags_ecx = 0;
#ifdef _MSC_VER
#ifdef JS_CODEGEN_X64
    int cpuinfo[4];

    __cpuid(cpuinfo, 1);
    flags_ecx = cpuinfo[2];
    flags_edx = cpuinfo[3];
#else
    _asm {
        mov eax, 1 // cpuid function 1 gives us the standard feature set
        cpuid;
        mov flags_ecx, ecx;
        mov flags_edx, edx;
    }
#endif
#elif defined(__GNUC__)
#ifdef JS_CODEGEN_X64
    asm (
         "movl $0x1, %%eax;"
         "cpuid;"
         : "=c" (flags_ecx), "=d" (flags_edx)
         :
         : "%eax", "%ebx"
         );
#else
    // On 32-bit x86, we must preserve ebx; the compiler needs it for PIC mode.
    asm (
         "movl $0x1, %%eax;"
         "pushl %%ebx;"
         "cpuid;"
         "popl %%ebx;"
         : "=c" (flags_ecx), "=d" (flags_edx)
         :
         : "%eax"
         );
#endif
#endif

#ifdef DEBUG
    if (s_floatingPointDisabled) {
        // Disable SSE2.
        s_sseCheckState = HasSSE;
        return;
    }
#endif

    static const int SSEFeatureBit = 1 << 25;
    static const int SSE2FeatureBit = 1 << 26;
    static const int SSE3FeatureBit = 1 << 0;
    static const int SSSE3FeatureBit = 1 << 9;
    static const int SSE41FeatureBit = 1 << 19;
    static const int SSE42FeatureBit = 1 << 20;
    if (flags_ecx & SSE42FeatureBit)
        s_sseCheckState = HasSSE4_2;
    else if (flags_ecx & SSE41FeatureBit)
        s_sseCheckState = HasSSE4_1;
    else if (flags_ecx & SSSE3FeatureBit)
        s_sseCheckState = HasSSSE3;
    else if (flags_ecx & SSE3FeatureBit)
        s_sseCheckState = HasSSE3;
    else if (flags_edx & SSE2FeatureBit)
        s_sseCheckState = HasSSE2;
    else if (flags_edx & SSEFeatureBit)
        s_sseCheckState = HasSSE;
    else
        s_sseCheckState = NoSSE;

#ifdef DEBUG
    if (s_sseCheckState >= HasSSE4_1 && s_SSE4Disabled)
        s_sseCheckState = HasSSE3;
    if (s_sseCheckState >= HasSSE3 && s_SSE3Disabled)
        s_sseCheckState = HasSSE2;
#endif
}
