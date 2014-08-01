/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef assembler_assembler_MacroAssemblerX86Common_h
#define assembler_assembler_MacroAssemblerX86Common_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER

#include "assembler/assembler/X86Assembler.h"

namespace JSC {

class MacroAssemblerX86Common {
public:
    // As the SSE's were introduced in order, the presence of a later SSE implies
    // the presence of an earlier SSE. For example, SSE4_2 support implies SSE2 support.
    enum SSECheckState {
        NotCheckedSSE = 0,
        NoSSE = 1,
        HasSSE = 2,
        HasSSE2 = 3,
        HasSSE3 = 4,
        HasSSSE3 = 5,
        HasSSE4_1 = 6,
        HasSSE4_2 = 7
    };

    static SSECheckState getSSEState()
    {
        if (s_sseCheckState == NotCheckedSSE) {
            MacroAssemblerX86Common::setSSECheckState();
        }
        // Only check once.
        MOZ_ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState;
    }

private:
    static SSECheckState s_sseCheckState;

    static void setSSECheckState();

  public:
#if WTF_CPU_X86
    static bool isSSEPresent()
    {
#if defined(__SSE__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        MOZ_ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE;
#endif
    }

    static bool isSSE2Present()
    {
#if defined(__SSE2__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        MOZ_ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE2;
#endif
    }

#elif !defined(NDEBUG) // CPU(X86)

    // On x86-64 we should never be checking for SSE2 in a non-debug build,
    // but non debug add this method to keep the asserts above happy.
    static bool isSSE2Present()
    {
        return true;
    }

#endif
    static bool isSSE3Present()
    {
#if defined(__SSE3__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        MOZ_ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE3;
#endif
    }

    static bool isSSSE3Present()
    {
#if defined(__SSSE3__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        MOZ_ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSSE3;
#endif
    }

    static bool isSSE41Present()
    {
#if defined(__SSE4_1__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        MOZ_ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE4_1;
#endif
    }

    static bool isSSE42Present()
    {
#if defined(__SSE4_2__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        MOZ_ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE4_2;
#endif
    }

  private:
#ifdef DEBUG
    static bool s_floatingPointDisabled;
    static bool s_SSE3Disabled;
    static bool s_SSE4Disabled;

  public:
    static void SetFloatingPointDisabled() {
        s_floatingPointDisabled = true;
    }
    static void SetSSE3Disabled() {
        s_SSE3Disabled = true;
    }
    static void SetSSE4Disabled() {
        s_SSE4Disabled = true;
    }
#endif
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif /* assembler_assembler_MacroAssemblerX86Common_h */
