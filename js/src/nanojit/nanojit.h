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

#ifndef __nanojit_h__
#define __nanojit_h__

#include "avmplus.h"

#ifdef FEATURE_NANOJIT

#if defined AVMPLUS_IA32
    #define NANOJIT_IA32
#elif defined AVMPLUS_ARM
    #define NANOJIT_ARM
#elif defined AVMPLUS_PPC
    #define NANOJIT_PPC
#elif defined AVMPLUS_SPARC
    #define NANOJIT_SPARC
#elif defined AVMPLUS_AMD64
    #define NANOJIT_X64
#elif defined AVMPLUS_MIPS
    #define NANOJIT_MIPS
#else
    #error "unknown nanojit architecture"
#endif

#ifdef AVMPLUS_64BIT
#define NANOJIT_64BIT
#endif

#if defined NANOJIT_64BIT
    #define IF_64BIT(...) __VA_ARGS__
    #define UNLESS_64BIT(...)
    #define CASE32(x)
    #define CASE64(x)   case x
#else
    #define IF_64BIT(...)
    #define UNLESS_64BIT(...) __VA_ARGS__
    #define CASE32(x)   case x
    #define CASE64(x)
#endif

#if defined NANOJIT_IA32 || defined NANOJIT_X64
    #define CASE86(x)   case x
#else
    #define CASE86(x)
#endif

// Embed no-op macros that let Valgrind work with the JIT.
#ifdef MOZ_VALGRIND
#  define JS_VALGRIND
#endif
#ifdef JS_VALGRIND
#  include <valgrind/valgrind.h>
#else
#  define VALGRIND_DISCARD_TRANSLATIONS(addr, szB)
#endif

namespace nanojit
{
    /**
     * -------------------------------------------
     * START AVM bridging definitions
     * -------------------------------------------
     */
    typedef avmplus::AvmCore AvmCore;

    const uint32_t MAXARGS = 8;

    #ifdef NJ_NO_VARIADIC_MACROS
        inline void NanoAssertMsgf(bool a,const char *f,...) {}
        inline void NanoAssertMsg(bool a,const char *m) {}
        inline void NanoAssert(bool a) {}
    #elif defined(_DEBUG)

        #define __NanoAssertMsgf(a, file_, line_, f, ...)  \
            if (!(a)) { \
                avmplus::AvmLog("Assertion failed: " f "%s (%s:%d)\n", __VA_ARGS__, #a, file_, line_); \
                NanoAssertFail(); \
            }

        #define _NanoAssertMsgf(a, file_, line_, f, ...)   __NanoAssertMsgf(a, file_, line_, f, __VA_ARGS__)

        #define NanoAssertMsgf(a,f,...)   do { __NanoAssertMsgf(a, __FILE__, __LINE__, f ": ", __VA_ARGS__); } while (0)
        #define NanoAssertMsg(a,m)        do { __NanoAssertMsgf(a, __FILE__, __LINE__, "\"%s\": ", m); } while (0)
        #define NanoAssert(a)             do { __NanoAssertMsgf(a, __FILE__, __LINE__, "%s", ""); } while (0)
    #else
        #define NanoAssertMsgf(a,f,...)   do { } while (0) /* no semi */
        #define NanoAssertMsg(a,m)        do { } while (0) /* no semi */
        #define NanoAssert(a)             do { } while (0) /* no semi */
    #endif

    /*
     * Sun Studio C++ compiler has a bug
     * "sizeof expression not accepted as size of array parameter"
     * The bug number is 6688515. It is not public yet.
     * Turn off this assert for Sun Studio until this bug is fixed.
     */
    #ifdef __SUNPRO_CC
        #define NanoStaticAssert(condition)
    #else
        #define NanoStaticAssert(condition) \
            extern void nano_static_assert(int arg[(condition) ? 1 : -1])
    #endif


    /**
     * -------------------------------------------
     * END AVM bridging definitions
     * -------------------------------------------
     */
}

#ifdef AVMPLUS_VERBOSE
#ifndef NJ_VERBOSE_DISABLED
    #define NJ_VERBOSE 1
#endif
#ifndef NJ_PROFILE_DISABLED
    #define NJ_PROFILE 1
#endif
#endif

#ifdef NJ_NO_VARIADIC_MACROS
    #include <stdio.h>
    #define verbose_outputf            if (_logc->lcbits & LC_Assembly) \
                                        Assembler::outputf
    #define verbose_only(x)            x
#elif defined(NJ_VERBOSE)
    #include <stdio.h>
    #define verbose_outputf            if (_logc->lcbits & LC_Assembly) \
                                        Assembler::outputf
    #define verbose_only(...)        __VA_ARGS__
#else
    #define verbose_outputf
    #define verbose_only(...)
#endif /*NJ_VERBOSE*/

#ifdef _DEBUG
    #define debug_only(x)           x
#else
    #define debug_only(x)
#endif /* DEBUG */

#ifdef NJ_PROFILE
    #define counter_struct_begin()  struct {
    #define counter_struct_end()    } _stats;
    #define counter_define(x)       int32_t x
    #define counter_value(x)        _stats.x
    #define counter_set(x,v)        (counter_value(x)=(v))
    #define counter_adjust(x,i)     (counter_value(x)+=(int32_t)(i))
    #define counter_reset(x)        counter_set(x,0)
    #define counter_increment(x)    counter_adjust(x,1)
    #define counter_decrement(x)    counter_adjust(x,-1)
    #define profile_only(x)         x
#else
    #define counter_struct_begin()
    #define counter_struct_end()
    #define counter_define(x)
    #define counter_value(x)
    #define counter_set(x,v)
    #define counter_adjust(x,i)
    #define counter_reset(x)
    #define counter_increment(x)
    #define counter_decrement(x)
    #define profile_only(x)
#endif /* NJ_PROFILE */

#define isS8(i)  ( int32_t(i) == int8_t(i) )
#define isU8(i)  ( int32_t(i) == uint8_t(i) )
#define isS16(i) ( int32_t(i) == int16_t(i) )
#define isU16(i) ( int32_t(i) == uint16_t(i) )
#define isS24(i) ( (int32_t((i)<<8)>>8) == (i) )

static inline bool isS32(intptr_t i) {
    return int32_t(i) == i;
}

static inline bool isU32(uintptr_t i) {
    return uint32_t(i) == i;
}

#define alignTo(x,s)        ((((uintptr_t)(x)))&~(((uintptr_t)s)-1))
#define alignUp(x,s)        ((((uintptr_t)(x))+(((uintptr_t)s)-1))&~(((uintptr_t)s)-1))

// -------------------------------------------------------------------
// START debug-logging definitions
// -------------------------------------------------------------------

/* Debug printing stuff.  All Nanojit and jstracer debug printing
   should be routed through LogControl::printf.  Don't use
   ad-hoc calls to printf, fprintf(stderr, ...) etc.

   Similarly, don't use ad-hoc getenvs etc to decide whether or not to
   print debug output.  Instead consult the relevant control bit in
   LogControl::lcbits in the LogControl object you are supplied with.
*/

# if defined(__GNUC__)
# define PRINTF_CHECK(x, y) __attribute__((format(__printf__, x, y)))
# else
# define PRINTF_CHECK(x, y)
# endif

namespace nanojit {

    // LogControl, a class for controlling and routing debug output

    enum LC_Bits {
        /* Output control bits for Nanojit code.  Only use bits 15
           and below, so that callers can use bits 16 and above for
           themselves. */
        // TODO: add entries for the writer pipeline
        LC_FragProfile = 1<<6, // collect per-frag usage counts
        LC_Activation  = 1<<5, // enable printActivationState
        LC_Liveness    = 1<<4, // (show LIR liveness analysis)
        LC_ReadLIR     = 1<<3, // As read from LirBuffer
        LC_AfterSF     = 1<<2, // After StackFilter
        LC_RegAlloc    = 1<<1, // stuff to do with reg alloc
        LC_Assembly    = 1<<0  // final assembly
    };

    class LogControl
    {
    public:
        // All Nanojit and jstracer printing should be routed through
        // this function.
        void printf( const char* format, ... ) PRINTF_CHECK(2,3);

        // An OR of LC_Bits values, indicating what should be output
        uint32_t lcbits;
    };
}

// -------------------------------------------------------------------
// END debug-logging definitions
// -------------------------------------------------------------------


#include "njconfig.h"
#include "Allocator.h"
#include "Containers.h"
#include "Native.h"
#include "CodeAlloc.h"
#include "LIR.h"
#include "RegAlloc.h"
#include "Fragmento.h"
#include "Assembler.h"

#endif // FEATURE_NANOJIT
#endif // __nanojit_h__
