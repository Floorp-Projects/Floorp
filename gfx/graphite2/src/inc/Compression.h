/*  GRAPHITE2 LICENSING

    Copyright 2015, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street, 
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/

#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>

#include <iterator>

#if ((defined GCC_VERSION && GCC_VERSION >= 302) || (defined __INTEL_COMPILER && __INTEL_COMPILER >= 800) || defined(__clang__))
    #define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
    #define expect(expr,value)    (expr)
#endif

#define likely(expr)     expect((expr) != 0, 1)
#define unlikely(expr)   expect((expr) != 0, 0)


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
size_t align(size_t p) {
    return (p + sizeof(unsigned long)-1) & ~(sizeof(unsigned long)-1);
}

inline
u8 * overrun_copy(u8 * d, u8 const * s, size_t n) {
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
u8 * fast_copy(u8 * d, u8 const * s, size_t n) {
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
u8 * copy(u8 * d, u8 const * s, size_t n) {
    if (likely(d>s+sizeof(unsigned long)))
        return overrun_copy(d,s,n);
    else 
        while (n--) *d++ = *s++;
    return d;
}

} // end of anonymous namespace


