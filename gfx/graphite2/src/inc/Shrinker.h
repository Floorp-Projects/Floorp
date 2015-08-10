/*  Copyright (c) 2012, Siyuan Fu <fusiyuan2010@gmail.com>
    Copyright (c) 2015, SIL International
    All rights reserved.
    
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    
    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    
    3. Neither the name of the copyright holder nor the names of its
       contributors may be used to endorse or promote products derived from
       this software without specific prior written permission.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
    POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>

#include <iterator>

//the code from LZ4
#if (GCC_VERSION >= 302) || (__INTEL_COMPILER >= 800) || defined(__clang__)
# define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
# define expect(expr,value)    (expr)
#endif
#define likely(expr)     expect((expr) != 0, 1)
#define unlikely(expr)   expect((expr) != 0, 0)
////////////////////


namespace
{

#if defined(_MSC_VER)
typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;
#else
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif

ptrdiff_t const     MINMATCH  = 4;

template<int S>
inline 
void unaligned_copy(void * d, void const * s) {
  ::memcpy(d, s, S);
}

inline
u8 * memcpy_nooverlap(u8 * d, u8 const * s, size_t n) {
    size_t const WS = sizeof(unsigned long);
    u8 const * e = s + n;
    do 
    {
        unaligned_copy<WS>(d, s);
        d += WS;
        s += WS;
    }
    while (s < e);
    d-=(s-e);
    
    return d;
}


inline
u8 * memcpy_nooverlap_surpass(u8 * d, u8 const * s, size_t n) {
    size_t const WS = sizeof(unsigned long);
    size_t wn = n/WS;
    while (wn--) 
    {
        unaligned_copy<WS>(d, s);
        d += WS;
        s += WS;
    }
    n &= WS-1;
    while (n--) {*d++ = *s++; }
    
    return d;
}


inline 
u8 * memcpy_(u8 * d, u8 const * s, size_t n) {
    if (likely(d>s+sizeof(unsigned long)))
        return memcpy_nooverlap(d,s,n);
    else while (n--) *d++ = *s++;
    return d;
}

} // end of anonymous namespace


