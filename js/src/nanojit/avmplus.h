/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the
 * "License"); you may not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific
 * language governing rights and limitations under the License.
 *
 * The Original Code is [Open Source Virtual Machine.]
 *
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights
 * Reserved.
 *
 * Contributor(s): Adobe AS3 Team
 *                 Andreas Gal <gal@mozilla.com>
 *                 Asko Tontti <atontti@cc.hut.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of either the GNU
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions
 * above and replace them with the notice and other provisions required by the GPL or the
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file
 * under the terms of any one of the MPL, the GPL or the LGPL.
 *
 ***** END LICENSE BLOCK ***** */

#ifndef avm_h___
#define avm_h___

#include "VMPI.h"
#include "njcpudetect.h"
#include "njconfig.h"

#if !defined(AVMPLUS_LITTLE_ENDIAN) && !defined(AVMPLUS_BIG_ENDIAN)
#ifdef IS_BIG_ENDIAN
#define AVMPLUS_BIG_ENDIAN
#else
#define AVMPLUS_LITTLE_ENDIAN
#endif
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
#define FASTCALL __fastcall
#elif defined(__GNUC__) && defined(__i386__) &&                 \
    ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
#define FASTCALL __attribute__((fastcall))
#else
#define FASTCALL
#define NO_FASTCALL
#endif

#if defined(NO_FASTCALL)
#if defined(AVMPLUS_IA32)
#define SIMULATE_FASTCALL(lr, state_ptr, frag_ptr, func_addr)   \
    asm volatile(                                               \
        "call *%%esi"                                           \
        : "=a" (lr)                                             \
        : "c" (state_ptr), "d" (frag_ptr), "S" (func_addr)      \
        : "memory", "cc"                                        \
    );
#endif /* defined(AVMPLUS_IA32) */
#endif /* defined(NO_FASTCALL) */

#ifdef WIN32
#include <windows.h>
#elif defined(AVMPLUS_OS2)
#define INCL_DOSMEMMGR
#include <os2.h>
#endif

#if defined(DEBUG) || defined(NJ_NO_VARIADIC_MACROS)
#if !defined _DEBUG
#define _DEBUG
#endif
#define NJ_VERBOSE 1
#define NJ_PROFILE 1
#include <stdarg.h>
#endif

#ifdef _DEBUG
void NanoAssertFail();
#endif

#if defined(AVMPLUS_IA32)
#if defined(_MSC_VER)

# define AVMPLUS_HAS_RDTSC 1

__declspec(naked) static inline __int64 rdtsc()
{
    __asm
    {
        rdtsc;
        ret;
    }
}

#elif defined(SOLARIS)

# define AVMPLUS_HAS_RDTSC 1

static inline unsigned long long rdtsc(void)
{
    unsigned long long int x;
    asm volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

#elif defined(__i386__)

# define AVMPLUS_HAS_RDTSC 1

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}

#endif /* compilers */

#elif defined(__x86_64__)

# define AVMPLUS_HAS_RDTSC 1

static __inline__ uint64_t rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
}

#elif defined(_MSC_VER) && defined(_M_AMD64)

# define AVMPLUS_HAS_RDTSC 1

#include <intrin.h>
#pragma intrinsic(__rdtsc)

static inline unsigned __int64 rdtsc(void)
{
    return __rdtsc();
}

#elif defined(__powerpc__)

# define AVMPLUS_HAS_RDTSC 1

typedef unsigned long long int unsigned long long;

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int result=0;
  unsigned long int upper, lower,tmp;
  __asm__ volatile(
                "0:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b         \n"
                : "=r"(upper),"=r"(lower),"=r"(tmp)
                );
  result = upper;
  result = result<<32;
  result = result|lower;

  return(result);
}

#endif /* architecture */

#ifndef AVMPLUS_HAS_RDTSC
# define AVMPLUS_HAS_RDTSC 0
#endif

struct JSContext;

#ifdef PERFM
# define PERFM_NVPROF(n,v) _nvprof(n,v)
# define PERFM_NTPROF(n) _ntprof(n)
# define PERFM_TPROF_END() _tprof_end()
#else
# define PERFM_NVPROF(n,v)
# define PERFM_NTPROF(n)
# define PERFM_TPROF_END()
#endif

namespace avmplus {

    typedef int FunctionID;

    extern void AvmLog(char const *msg, ...);

    static const int kstrconst_emptyString = 0;

    class AvmInterpreter
    {
        class Labels {
        public:
            const char* format(const void* ip)
            {
                static char buf[33];
                sprintf(buf, "%p", ip);
                return buf;
            }
        };

        Labels _labels;
    public:
        Labels* labels;

        AvmInterpreter()
        {
            labels = &_labels;
        }

    };

    class AvmConsole
    {
    public:
        AvmConsole& operator<<(const char* s)
        {
            fprintf(stdout, "%s", s);
            return *this;
        }
    };

    class AvmCore
    {
    public:
        AvmInterpreter interp;
        AvmConsole console;

        static nanojit::Config config;

#ifdef AVMPLUS_IA32
        static inline bool
        use_sse2()
        {
            return config.i386_sse2;
        }
#endif

        static inline bool
        use_cmov()
        {
#ifdef AVMPLUS_IA32
            return config.i386_use_cmov;
#else
        return true;
#endif
        }
    };

    /**
     * Bit vectors are an efficent method of keeping True/False information
     * on a set of items or conditions. Class BitSet provides functions
     * to manipulate individual bits in the vector.
     *
     * This object is not optimized for a fixed sized bit vector
     * it instead allows for dynamically growing the bit vector.
     */
    class BitSet
    {
        public:
            enum {  kUnit = 8*sizeof(long),
                    kDefaultCapacity = 4   };

            BitSet()
            {
                capacity = kDefaultCapacity;
                ar = (long*)calloc(capacity, sizeof(long));
                reset();
            }

            ~BitSet()
            {
                free(ar);
            }

            void reset()
            {
                for (int i = 0; i < capacity; i++)
                    ar[i] = 0;
            }

            void set(int bitNbr)
            {
                int index = bitNbr / kUnit;
                int bit = bitNbr % kUnit;
                if (index >= capacity)
                    grow(index+1);

                ar[index] |= (1<<bit);
            }

            void clear(int bitNbr)
            {
                int index = bitNbr / kUnit;
                int bit = bitNbr % kUnit;
                if (index < capacity)
                    ar[index] &= ~(1<<bit);
            }

            bool get(int bitNbr) const
            {
                int index = bitNbr / kUnit;
                int bit = bitNbr % kUnit;
                bool value = false;
                if (index < capacity)
                    value = ( ar[index] & (1<<bit) ) ? true : false;
                return value;
            }

        private:
            // Grow the array until at least newCapacity big
            void grow(int newCapacity)
            {
                // create vector that is 2x bigger than requested
                newCapacity *= 2;
                //MEMTAG("BitVector::Grow - long[]");
                long* newAr = (long*)calloc(newCapacity, sizeof(long));

                // copy the old one
                for (int i = 0; i < capacity; i++)
                    newAr[i] = ar[i];

                // in with the new out with the old
                free(ar);

                ar = newAr;
                capacity = newCapacity;
            }

            int capacity;
            long* ar;
    };
}

#endif
