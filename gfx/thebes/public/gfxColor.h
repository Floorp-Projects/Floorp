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

#include "nsPrintfCString.h"

#include "gfxTypes.h"

/**
 * Fast approximate division by 255. It has the property that
 * for all 0 <= n <= 255*255, FAST_DIVIDE_BY_255(n) == n/255.
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

        PACKED_XBGR,
        PACKED_XRGB
    };

    gfxRGBA() { }
    gfxRGBA(const gfxRGBA& c) : r(c.r), g(c.g), b(c.b), a(c.a) {}
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
     * @see gfxRGBA::Packed
     */
    gfxRGBA(PRUint32 c, PackedColorType colorType = PACKED_ABGR) {
        if (colorType == PACKED_ABGR ||
            colorType == PACKED_XBGR ||
            colorType == PACKED_ABGR_PREMULTIPLIED)
        {
            r = ((c >> 0) & 0xff) / 255.0;
            g = ((c >> 8) & 0xff) / 255.0;
            b = ((c >> 16) & 0xff) / 255.0;
            a = ((c >> 24) & 0xff) / 255.0;
        } else if (colorType == PACKED_ARGB ||
                   colorType == PACKED_XRGB ||
                   colorType == PACKED_ARGB_PREMULTIPLIED)
        {
            b = ((c >> 0) & 0xff) / 255.0;
            g = ((c >> 8) & 0xff) / 255.0;
            r = ((c >> 16) & 0xff) / 255.0;
            a = ((c >> 24) & 0xff) / 255.0;
        }

        if (colorType == PACKED_ABGR_PREMULTIPLIED ||
            colorType == PACKED_ARGB_PREMULTIPLIED)
        {
            if (a > 0.0) {
                r /= a;
                g /= a;
                b /= a;
            }
        } else if (colorType == PACKED_XBGR ||
                   colorType == PACKED_XRGB)
        {
            a = 1.0;
        }
    }

    /**
     * Initialize this color by parsing the given string.
     * XXX implement me!
     */
#if 0
    gfxRGBA(const char* str) {
        a = 1.0;
        // if aString[0] is a #, parse it as hex
        // if aString[0] is a letter, parse it as a color name
        // if aString[0] is a number, parse it loosely as hex
    }
#endif

    /**
     * Returns this color value as a packed 32-bit integer. This reconstructs
     * the int32 based on the given colorType, always in the native byte order.
     */
    PRUint32 Packed(PackedColorType colorType = PACKED_ABGR) const {
        if (colorType == PACKED_ABGR || colorType == PACKED_XBGR) {
            return (((PRUint8)(a * 255.0) << 24) |
                    ((PRUint8)(b * 255.0) << 16) |
                    ((PRUint8)(g * 255.0) << 8) |
                    ((PRUint8)(r * 255.0)));
        } else if (colorType == PACKED_ARGB || colorType == PACKED_XRGB) {
            return (((PRUint8)(a * 255.0) << 24) |
                    ((PRUint8)(r * 255.0) << 16) |
                    ((PRUint8)(g * 255.0) << 8) |
                    ((PRUint8)(b * 255.0)));
        } else if (colorType == PACKED_ABGR_PREMULTIPLIED) {
            return (((PRUint8)(a * 255.0) << 24) |
                    ((PRUint8)((b*a) * 255.0) << 16) |
                    ((PRUint8)((g*a) * 255.0) << 8) |
                    ((PRUint8)((r*a) * 255.0)));
        } else if (colorType == PACKED_ARGB_PREMULTIPLIED) {
            return (((PRUint8)(a * 255.0) << 24) |
                    ((PRUint8)((r*a) * 255.0) << 16) |
                    ((PRUint8)((g*a) * 255.0) << 8) |
                    ((PRUint8)((b*a) * 255.0)));
        } else {
            return 0;
        }
    }

    /**
     * Convert this color to a hex value. For example, for rgb(255,0,0),
     * this will return FF0000.
     */
    // XXX I'd really prefer to just have this return an nsACString
    // Does this function even make sense, since we're just ignoring the alpha value?
    void Hex(nsACString& result) const {
        nsPrintfCString hex(8, "%02x%02x%02x", PRUint8(r*255.0), PRUint8(g*255.0), PRUint8(b*255.0));
        result.Assign(hex);
    }

};

#endif /* _GFX_COLOR_H */
