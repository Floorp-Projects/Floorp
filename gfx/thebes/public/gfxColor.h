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

#ifndef GFXCOLOR_H
#define GFXCOLOR_H

struct gfxRGBA {
    double r, g, b, a;

    gfxRGBA() { }
    gfxRGBA(const gfxRGBA& c) : r(c.r), g(c.g), b(c.b), a(c.a) {}
    gfxRGBA(double _r, double _g, double _b, double _a=1.0) : r(_r), g(_g), b(_b), a(_a) {}
    gfxRGBA(unsigned long c) {
        a = (c & 0xff) / 255.0;
        r = ((c >> 8) & 0xff) / 255.0;
        g = ((c >> 16) & 0xff) / 255.0;
        b = ((c >> 24) & 0xff) / 255.0;
    }
    gfxRGBA(const char* str) {
        a = 1.0;
        // if aString[0] is a #, parse it as hex
        // if aString[0] is a letter, parse it as a color name
        // if aString[0] is a number, parse it loosely as hex
    }

    unsigned long Packed() {
        return (((unsigned char)(a * 255.0) << 24) |
                 ((unsigned char)(b * 255.0) << 16) |
                 ((unsigned char)(g * 255.0) << 8) |
                 ((unsigned char)(r * 255.0)));
    }

    const char* Hex() const {
    }

};

#endif /* GFXCOLOR_H */
