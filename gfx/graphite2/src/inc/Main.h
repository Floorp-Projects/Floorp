/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
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

#include <cstdlib>
#include "graphite2/Types.h"

#ifdef GRAPHITE2_CUSTOM_HEADER
#include GRAPHITE2_CUSTOM_HEADER
#endif

namespace graphite2 {

typedef gr_uint8        uint8;
typedef gr_uint8        byte;
typedef gr_uint16       uint16;
typedef gr_uint32       uint32;
typedef gr_int8         int8;
typedef gr_int16        int16;
typedef gr_int32        int32;
typedef size_t          uintptr;

// typesafe wrapper around malloc for simple types
// use free(pointer) to deallocate
template <typename T> T * gralloc(size_t n)
{
    return reinterpret_cast<T*>(malloc(sizeof(T) * n));
}

template <typename T> T * grzeroalloc(size_t n)
{
    return reinterpret_cast<T*>(calloc(n, sizeof(T)));
}

} // namespace graphite2

#define CLASS_NEW_DELETE \
    void * operator new   (size_t size){ return malloc(size);} \
    void * operator new   (size_t, void * p) throw() { return p; } \
    void * operator new[] (size_t size) {return malloc(size);} \
    void * operator new[] (size_t, void * p) throw() { return p; } \
    void operator delete   (void * p) throw() { free(p);} \
    void operator delete   (void *, void *) throw() {} \
    void operator delete[] (void * p)throw() { free(p); } \
    void operator delete[] (void *, void *) throw() {}

#ifdef __GNUC__
#define GR_MAYBE_UNUSED __attribute__((unused))
#else
#define GR_MAYBE_UNUSED
#endif
