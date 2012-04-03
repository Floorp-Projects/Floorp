/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#ifndef GFX_COLOR_H
#define GFX_COLOR_H

#include "gfxTypes.h"

#include "prbit.h" // for PR_ROTATE_(LEFT,RIGHT)32
#include "prio.h"  // for ntohl

#define GFX_UINT32_FROM_BPTR(pbptr,i) (((PRUint32*)(pbptr))[i])

#if defined(IS_BIG_ENDIAN)
  #define GFX_NTOHL(x) (x)
  #define GFX_HAVE_CHEAP_NTOHL
#elif defined(_WIN32)
  #if (_MSC_VER >= 1300) // also excludes MinGW
    #include <stdlib.h>
    #pragma intrinsic(_byteswap_ulong)
    #define GFX_NTOHL(x) _byteswap_ulong(x)
    #define GFX_HAVE_CHEAP_NTOHL
  #else
    // A reasonably fast generic little-endian implementation.
    #define GFX_NTOHL(x) \
         ( (PR_ROTATE_RIGHT32((x),8) & 0xFF00FF00) | \
           (PR_ROTATE_LEFT32((x),8)  & 0x00FF00FF) )
  #endif
#else
  #define GFX_NTOHL(x) ntohl(x)
  #define GFX_HAVE_CHEAP_NTOHL
#endif

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
#if defined(GFX_HAVE_CHEAP_NTOHL)
  #define GFX_0XFF_PPIXEL_FROM_UINT32(x) \
       ( (GFX_NTOHL(x) >> 8) | (0xFF << 24) )
#else
  // A reasonably fast generic little-endian implementation.
  #define GFX_0XFF_PPIXEL_FROM_UINT32(x) \
       ( (PR_ROTATE_LEFT32((x),16) | 0xFF00FF00) & ((x) | 0xFFFF00FF) )
#endif

#define GFX_0XFF_PPIXEL_FROM_BPTR(x) \
     ( GFX_0XFF_PPIXEL_FROM_UINT32(GFX_UINT32_FROM_BPTR((x),0)) )

/**
 * GFX_BLOCK_RGB_TO_FRGB(from,to)
 *   sizeof(*from) == sizeof(char)
 *   sizeof(*to)   == sizeof(PRUint32)
 *
 * Copy 4 pixels at a time, reading blocks of 12 bytes (RGB x4)
 *   and writing blocks of 16 bytes (FRGB x4)
 */
#define GFX_BLOCK_RGB_TO_FRGB(from,to) \
  PR_BEGIN_MACRO \
    PRUint32 m0 = GFX_UINT32_FROM_BPTR(from,0), \
             m1 = GFX_UINT32_FROM_BPTR(from,1), \
             m2 = GFX_UINT32_FROM_BPTR(from,2), \
             rgbr = GFX_NTOHL(m0), \
             gbrg = GFX_NTOHL(m1), \
             brgb = GFX_NTOHL(m2), \
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
 * Fast premultiply macro
 *
 * equivalent to (((c)*(a))/255)
 */
#define GFX_PREMULTIPLY(c,a) GFX_DIVIDE_BY_255((c)*(a))

/** 
 * Macro to pack the 4 8-bit channels (A,R,G,B) 
 * into a 32-bit packed premultiplied pixel.
 *
 * The checks for 0 alpha or max alpha ensure that the
 * compiler selects the quicked calculation when alpha is constant.
 */
#define GFX_PACKED_PIXEL(a,r,g,b)                                       \
    ((a) == 0x00) ? 0x00000000 :                                        \
    ((a) == 0xFF) ? ((0xFF << 24) | ((r) << 16) | ((g) << 8) | (b))     \
                  : ((a) << 24) |                                       \
                    (GFX_PREMULTIPLY(r,a) << 16) |                      \
                    (GFX_PREMULTIPLY(g,a) << 8) |                       \
                    (GFX_PREMULTIPLY(b,a))

/** 
 * Macro to pack the 4 8-bit channels (A,R,G,B) 
 * into a 32-bit packed NON-premultiplied pixel.
 */
#define GFX_PACKED_PIXEL_NO_PREMULTIPLY(a,r,g,b)                        \
    (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))


/**
 * A color value, storing red, green, blue and alpha components.
 * This class does not use premultiplied alpha.
 *
 * XXX should this use doubles (instead of gfxFloat), for consistency with
 * cairo?
 */
struct THEBES_API gfxRGBA {
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
    gfxRGBA(gfxFloat _r, gfxFloat _g, gfxFloat _b, gfxFloat _a=1.0) : r(_r), g(_g), b(_b), a(_a) {}

    /**
     * Initialize this color from a packed 32-bit color.
     * The color value is interpreted based on colorType;
     * all values use the native platform endianness.
     *
     * Resulting gfxRGBA stores non-premultiplied data.
     *
     * @see gfxRGBA::Packed
     */
    gfxRGBA(PRUint32 c, PackedColorType colorType = PACKED_ABGR) {
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
     * the int32 based on the given colorType, always in the native byte order.
     *
     * Note: gcc 4.2.3 on at least Ubuntu (x86) does something strange with
     * (PRUint8)(c * 255.0) << x, where the result is different than
     * double d = c * 255.0; v = ((PRUint8) d) << x. 
     */
    PRUint32 Packed(PackedColorType colorType = PACKED_ABGR) const {
        gfxFloat rb = (r * 255.0);
        gfxFloat gb = (g * 255.0);
        gfxFloat bb = (b * 255.0);
        gfxFloat ab = (a * 255.0);

        if (colorType == PACKED_ABGR) {
            return (PRUint8(ab) << 24) |
                   (PRUint8(bb) << 16) |
                   (PRUint8(gb) << 8) |
                   (PRUint8(rb) << 0);
        }
        if (colorType == PACKED_ARGB || colorType == PACKED_XRGB) {
            return (PRUint8(ab) << 24) |
                   (PRUint8(rb) << 16) |
                   (PRUint8(gb) << 8) |
                   (PRUint8(bb) << 0);
        }

        rb *= a;
        gb *= a;
        bb *= a;

        if (colorType == PACKED_ABGR_PREMULTIPLIED) {
            return (((PRUint8)(ab) << 24) |
                    ((PRUint8)(bb) << 16) |
                    ((PRUint8)(gb) << 8) |
                    ((PRUint8)(rb) << 0));
        }
        if (colorType == PACKED_ARGB_PREMULTIPLIED) {
            return (((PRUint8)(ab) << 24) |
                    ((PRUint8)(rb) << 16) |
                    ((PRUint8)(gb) << 8) |
                    ((PRUint8)(bb) << 0));
        }

        return 0;
    }
};

#endif /* _GFX_COLOR_H */
