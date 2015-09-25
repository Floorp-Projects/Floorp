/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COLOR_H
#define GFX_COLOR_H

#include "mozilla/Attributes.h" // for MOZ_ALWAYS_INLINE
#include "mozilla/Endian.h" // for mozilla::NativeEndian::swapToBigEndian

/**
 * GFX_BLOCK_RGB_TO_FRGB(from,to)
 *   sizeof(*from) == sizeof(char)
 *   sizeof(*to)   == sizeof(uint32_t)
 *
 * Copy 4 pixels at a time, reading blocks of 12 bytes (RGB x4)
 *   and writing blocks of 16 bytes (FRGB x4)
 */
#define GFX_BLOCK_RGB_TO_FRGB(from,to) \
  PR_BEGIN_MACRO \
    uint32_t m0 = ((uint32_t*)from)[0], \
             m1 = ((uint32_t*)from)[1], \
             m2 = ((uint32_t*)from)[2], \
             rgbr = mozilla::NativeEndian::swapToBigEndian(m0), \
             gbrg = mozilla::NativeEndian::swapToBigEndian(m1), \
             brgb = mozilla::NativeEndian::swapToBigEndian(m2), \
             p0, p1, p2, p3; \
    p0 = 0xFF000000 | ((rgbr) >>  8); \
    p1 = 0xFF000000 | ((rgbr) << 16) | ((gbrg) >> 16); \
    p2 = 0xFF000000 | ((gbrg) <<  8) | ((brgb) >> 24); \
    p3 = 0xFF000000 | (brgb); \
    to[0] = p0; to[1] = p1; to[2] = p2; to[3] = p3; \
  PR_END_MACRO

/**
 * Fast approximate division by 255. It has the property that
 * for all 0 <= n <= 255*255, GFX_DIVIDE_BY_255(n) == n/255.
 * But it only uses two adds and two shifts instead of an
 * integer division (which is expensive on many processors).
 *
 * equivalent to ((v)/255)
 */
#define GFX_DIVIDE_BY_255(v)  \
     (((((unsigned)(v)) << 8) + ((unsigned)(v)) + 255) >> 16)

/**
 * Fast premultiply
 *
 * equivalent to (((c)*(a))/255)
 */
uint8_t MOZ_ALWAYS_INLINE gfxPreMultiply(uint8_t c, uint8_t a) {
    return GFX_DIVIDE_BY_255((c)*(a));
}

/**
 * Pack the 4 8-bit channels (A,R,G,B)
 * into a 32-bit packed NON-premultiplied pixel.
 */
uint32_t MOZ_ALWAYS_INLINE
gfxPackedPixelNoPreMultiply(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (((a) << 24) | ((r) << 16) | ((g) << 8) | (b));
}

/**
 * Pack the 4 8-bit channels (A,R,G,B)
 * into a 32-bit packed premultiplied pixel.
 */
uint32_t MOZ_ALWAYS_INLINE
gfxPackedPixel(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    if (a == 0x00)
        return 0x00000000;
    else if (a == 0xFF) {
        return gfxPackedPixelNoPreMultiply(a, r, g, b);
    } else {
        return  ((a) << 24) |
                (gfxPreMultiply(r,a) << 16) |
                (gfxPreMultiply(g,a) << 8)  |
                (gfxPreMultiply(b,a));
    }
}

#endif /* _GFX_COLOR_H */
