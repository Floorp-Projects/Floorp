/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COLOR_H
#define GFX_COLOR_H

#include "gfxTypes.h"

#include "mozilla/Attributes.h" // for MOZ_ALWAYS_INLINE
#include "mozilla/Endian.h" // for mozilla::NativeEndian::swapToBigEndian

#define GFX_UINT32_FROM_BPTR(pbptr,i) (((uint32_t*)(pbptr))[i])

/**
 * GFX_0XFF_PPIXEL_FROM_BPTR(x)
 *
 * Avoid tortured construction of 32-bit ARGB pixel from 3 individual bytes
 *   of memory plus constant 0xFF.  RGB bytes are already contiguous!
 * Equivalent to: GFX_PACKED_PIXEL(0xff,r,g,b)
 *
 * Attempt to use fast byte-swapping instruction(s), e.g. bswap on x86, in
 *   preference to a sequence of shift/or operations.
 */
#define GFX_0XFF_PPIXEL_FROM_UINT32(x) \
  ( (mozilla::NativeEndian::swapToBigEndian(uint32_t(x)) >> 8) | (0xFFU << 24) )

#define GFX_0XFF_PPIXEL_FROM_BPTR(x) \
     ( GFX_0XFF_PPIXEL_FROM_UINT32(GFX_UINT32_FROM_BPTR((x),0)) )

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
    uint32_t m0 = GFX_UINT32_FROM_BPTR(from,0), \
             m1 = GFX_UINT32_FROM_BPTR(from,1), \
             m2 = GFX_UINT32_FROM_BPTR(from,2), \
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

/**
 * A color value, storing red, green, blue and alpha components.
 * This class does not use premultiplied alpha.
 *
 * XXX should this use doubles (instead of gfxFloat), for consistency with
 * cairo?
 */
struct gfxRGBA {
    gfxFloat r, g, b, a;

    enum PackedColorType {
        PACKED_ABGR,
        PACKED_ABGR_PREMULTIPLIED,

        PACKED_ARGB,
        PACKED_ARGB_PREMULTIPLIED,

        PACKED_XRGB
    };

    gfxRGBA() { }
    /**
     * Intialize this color using explicit red, green, blue and alpha
     * values.
     */
    MOZ_CONSTEXPR gfxRGBA(gfxFloat _r, gfxFloat _g, gfxFloat _b, gfxFloat _a=1.0) : r(_r), g(_g), b(_b), a(_a) {}

    /**
     * Initialize this color from a packed 32-bit color.
     * The color value is interpreted based on colorType;
     * all values use the native platform endianness.
     *
     * Resulting gfxRGBA stores non-premultiplied data.
     *
     * @see gfxRGBA::Packed
     */
    gfxRGBA(uint32_t c, PackedColorType colorType = PACKED_ABGR) {
        if (colorType == PACKED_ABGR ||
            colorType == PACKED_ABGR_PREMULTIPLIED)
        {
            r = ((c >> 0) & 0xff) * (1.0 / 255.0);
            g = ((c >> 8) & 0xff) * (1.0 / 255.0);
            b = ((c >> 16) & 0xff) * (1.0 / 255.0);
            a = ((c >> 24) & 0xff) * (1.0 / 255.0);
        } else if (colorType == PACKED_ARGB ||
                   colorType == PACKED_XRGB ||
                   colorType == PACKED_ARGB_PREMULTIPLIED)
        {
            b = ((c >> 0) & 0xff) * (1.0 / 255.0);
            g = ((c >> 8) & 0xff) * (1.0 / 255.0);
            r = ((c >> 16) & 0xff) * (1.0 / 255.0);
            a = ((c >> 24) & 0xff) * (1.0 / 255.0);
        }

        if (colorType == PACKED_ABGR_PREMULTIPLIED ||
            colorType == PACKED_ARGB_PREMULTIPLIED)
        {
            if (a > 0.0) {
                r /= a;
                g /= a;
                b /= a;
            }
        } else if (colorType == PACKED_XRGB) {
            a = 1.0;
        }
    }

    bool operator==(const gfxRGBA& other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    bool operator!=(const gfxRGBA& other) const
    {
        return !(*this == other);
    }

    /**
     * Returns this color value as a packed 32-bit integer. This reconstructs
     * the int32_t based on the given colorType, always in the native byte order.
     *
     * Note: gcc 4.2.3 on at least Ubuntu (x86) does something strange with
     * (uint8_t)(c * 255.0) << x, where the result is different than
     * double d = c * 255.0; v = ((uint8_t) d) << x. 
     */
    uint32_t Packed(PackedColorType colorType = PACKED_ABGR) const {
        gfxFloat rb = (r * 255.0);
        gfxFloat gb = (g * 255.0);
        gfxFloat bb = (b * 255.0);
        gfxFloat ab = (a * 255.0);

        if (colorType == PACKED_ABGR) {
            return (uint8_t(ab) << 24) |
                   (uint8_t(bb) << 16) |
                   (uint8_t(gb) << 8) |
                   (uint8_t(rb) << 0);
        }
        if (colorType == PACKED_ARGB || colorType == PACKED_XRGB) {
            return (uint8_t(ab) << 24) |
                   (uint8_t(rb) << 16) |
                   (uint8_t(gb) << 8) |
                   (uint8_t(bb) << 0);
        }

        rb *= a;
        gb *= a;
        bb *= a;

        if (colorType == PACKED_ABGR_PREMULTIPLIED) {
            return (((uint8_t)(ab) << 24) |
                    ((uint8_t)(bb) << 16) |
                    ((uint8_t)(gb) << 8) |
                    ((uint8_t)(rb) << 0));
        }
        if (colorType == PACKED_ARGB_PREMULTIPLIED) {
            return (((uint8_t)(ab) << 24) |
                    ((uint8_t)(rb) << 16) |
                    ((uint8_t)(gb) << 8) |
                    ((uint8_t)(bb) << 0));
        }

        return 0;
    }
};

#endif /* _GFX_COLOR_H */
