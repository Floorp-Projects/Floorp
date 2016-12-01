/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DenormalDisabler_h
#define DenormalDisabler_h

#include <cmath>
#include <float.h>

namespace WebCore {

// Deal with denormals. They can very seriously impact performance on x86.

// Define HAVE_DENORMAL if we support flushing denormals to zero.

#if defined (XP_WIN) && defined(_MSC_VER)
// Windows compiled using MSVC with SSE2
#define HAVE_DENORMAL 1
#endif

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
// X86 chips can flush denormals
#define HAVE_DENORMAL 1
#endif

#if defined(__arm__) || defined(__aarch64__)
#define HAVE_DENORMAL 1
#endif

#ifdef HAVE_DENORMAL
class DenormalDisabler {
public:
    DenormalDisabler()
            : m_savedCSR(0)
    {
        disableDenormals();
    }

    ~DenormalDisabler()
    {
        restoreState();
    }

    // This is a nop if we can flush denormals to zero in hardware.
    static inline float flushDenormalFloatToZero(float f)
    {
        return f;
    }
private:
    unsigned m_savedCSR;

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    inline void disableDenormals()
    {
        m_savedCSR = getCSR();
        setCSR(m_savedCSR | 0x8040);
    }

    inline void restoreState()
    {
        setCSR(m_savedCSR);
    }

    inline int getCSR()
    {
        int result;
        asm volatile("stmxcsr %0" : "=m" (result));
        return result;
    }

    inline void setCSR(int a)
    {
        int temp = a;
        asm volatile("ldmxcsr %0" : : "m" (temp));
    }

#elif defined (XP_WIN) && defined(_MSC_VER)
    inline void disableDenormals()
    {
        // Save the current state, and set mode to flush denormals.
        //
        // http://stackoverflow.com/questions/637175/possible-bug-in-controlfp-s-may-not-restore-control-word-correctly
        _controlfp_s(&m_savedCSR, 0, 0);
        unsigned unused;
        _controlfp_s(&unused, _DN_FLUSH, _MCW_DN);
    }

    inline void restoreState()
    {
        unsigned unused;
        _controlfp_s(&unused, m_savedCSR, _MCW_DN);
    }
#elif defined(__arm__) || defined(__aarch64__)
    inline void disableDenormals()
    {
        m_savedCSR = getStatusWord();
        // Bit 24 is the flush-to-zero mode control bit. Setting it to 1 flushes denormals to 0.
        setStatusWord(m_savedCSR | (1 << 24));
    }

    inline void restoreState()
    {
        setStatusWord(m_savedCSR);
    }

    inline int getStatusWord()
    {
        int result;
#if defined(__aarch64__)
        asm volatile("mrs %x[result], FPCR" : [result] "=r" (result));
#else
        asm volatile("vmrs %[result], FPSCR" : [result] "=r" (result));
#endif
        return result;
    }

    inline void setStatusWord(int a)
    {
#if defined(__aarch64__)
        asm volatile("msr FPCR, %x[src]" : : [src] "r" (a));
#else
        asm volatile("vmsr FPSCR, %[src]" : : [src] "r" (a));
#endif
    }

#endif

};

#else
// FIXME: add implementations for other architectures and compilers
class DenormalDisabler {
public:
    DenormalDisabler() { }

    // Assume the worst case that other architectures and compilers
    // need to flush denormals to zero manually.
    static inline float flushDenormalFloatToZero(float f)
    {
        return (fabs(f) < FLT_MIN) ? 0.0f : f;
    }
};

#endif

} // namespace WebCore
#endif // DenormalDisabler_h
