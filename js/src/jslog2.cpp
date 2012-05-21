/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsutil.h"

/*
 * Check that we can use js_bitscan_clz32 to implement JS_FLOOR_LOG2 and
 * JS_FLOOR_LOG2W and js_bitscan_clz64 to implement JS_FLOOR_LOG2W on 64-bit
 * systems.
 */
#ifdef JS_HAS_BUILTIN_BITSCAN32
JS_STATIC_ASSERT(sizeof(unsigned int) == sizeof(uint32_t));
JS_STATIC_ASSERT_IF(JS_BYTES_PER_WORD == 4,
                    sizeof(unsigned int) == sizeof(uintptr_t));
#endif
#ifdef JS_HAS_BUILTIN_BITSCAN64
JS_STATIC_ASSERT_IF(JS_BYTES_PER_WORD == 8,
                    sizeof(unsigned long long) == sizeof(uintptr_t));
#endif

#if !defined(JS_HAS_BUILTIN_BITSCAN32) && JS_BYTES_PER_WORD == 4

size_t
js_FloorLog2wImpl(size_t n)
{
    size_t log2;

    JS_FLOOR_LOG2(log2, n);
    return log2;
}
#endif
/*
 * js_FloorLog2wImpl has to be defined only for 64-bit non-GCC case.
 */
#if !defined(JS_HAS_BUILTIN_BITSCAN64) && JS_BYTES_PER_WORD == 8

size_t
js_FloorLog2wImpl(size_t n)
{
    size_t log2, m;

    JS_ASSERT(n != 0);

    log2 = 0;
    m = n >> 32;
    if (m != 0) { n = m; log2 = 32; }
    m = n >> 16;
    if (m != 0) { n = m; log2 |= 16; }
    m = n >> 8;
    if (m != 0) { n = m; log2 |= 8; }
    m = n >> 4;
    if (m != 0) { n = m; log2 |= 4; }
    m = n >> 2;
    if (m != 0) { n = m; log2 |= 2; }
    log2 |= (n >> 1);

    return log2;
}

#endif
