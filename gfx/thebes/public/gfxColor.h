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

#ifndef _GFX_COLOR_H
#define _GFX_COLOR_H

#include "nsPrintfCString.h"

#include "gfxTypes.h"

/**
 * A color value, storing red, green, blue and alpha components.
 * This class does not use premultiplied alpha.
 *
 * XXX should this use doubles (instead of gfxFloat), for consistency with
 * cairo?
 */
struct gfxRGBA {
    gfxFloat r, g, b, a;

    gfxRGBA() { }
    gfxRGBA(const gfxRGBA& c) : r(c.r), g(c.g), b(c.b), a(c.a) {}
    /**
     * Intialize this color using explicit red, green, blue and alpha
     * values.
     */
    gfxRGBA(gfxFloat _r, gfxFloat _g, gfxFloat _b, gfxFloat _a=1.0) : r(_r), g(_g), b(_b), a(_a) {}
    /**
     * Initialize this color from a packed 32-bit color.
     * Premultiplied alpha isn't used.
     * This uses ABGR byte order in the native endianness.
     *
     * @see gfxRGBA::Packed
     */
    gfxRGBA(PRUint32 c) {
        r = ((c >> 0) & 0xff) / 255.0;
        g = ((c >> 8) & 0xff) / 255.0;
        b = ((c >> 16) & 0xff) / 255.0;
        a = ((c >> 24) & 0xff) / 255.0;
    }
    /**
     * Initialize this color by parsing the given string.
     */
    gfxRGBA(const char* str) {
        a = 1.0;
        // if aString[0] is a #, parse it as hex
        // if aString[0] is a letter, parse it as a color name
        // if aString[0] is a number, parse it loosely as hex
    }

    /**
     * Returns this color value as a packed 32-bit integer. This uses ABGR
     * byte order in the native endianness, as accepted by the corresponding
     * constructor of this class.
     */
    PRUint32 Packed() const {
        return (((PRUint8)(a * 255.0) << 24) |
                ((PRUint8)(b * 255.0) << 16) |
                ((PRUint8)(g * 255.0) << 8) |
                ((PRUint8)(r * 255.0)));
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
