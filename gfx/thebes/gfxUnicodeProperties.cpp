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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
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

#include "gfxUnicodeProperties.h"

#include "gfxUnicodePropertyData.cpp"

#include "harfbuzz/hb-unicode.h"

#define UNICODE_BMP_LIMIT 0x10000
#define UNICODE_LIMIT     0x110000

/*
To store properties for a million Unicode codepoints compactly, we use
a three-level array structure, with the Unicode values considered as
three elements: Plane, Page, and Char.

Space optimization happens because multiple Planes can refer to the same
Page array, and multiple Pages can refer to the same Char array holding
the actual values. In practice, most of the higher planes are empty and
thus share the same data; and within the BMP, there are also many pages
that repeat the same data for any given property.

Plane is usually zero, so we skip a lookup in this case, and require
that the Plane 0 pages are always the first set of entries in the Page
array.

The division of the remaining 16 bits into Page and Char fields is
adjusted for each property (by experiment using the generation tool)
to provide the most compact storage, depending on the distribution
of values.
*/

PRUint32
gfxUnicodeProperties::GetMirroredChar(PRUint32 aCh)
{
    // all mirrored chars are in plane 0
    if (aCh < UNICODE_BMP_LIMIT) {
        int v = sMirrorValues[sMirrorPages[0][aCh >> kMirrorCharBits]]
                             [aCh & ((1 << kMirrorCharBits) - 1)];
        // The mirror value is stored as either an offset (if less than
        // kSmallMirrorOffset) from the input character code, or as
        // an index into the sDistantMirrors list. This allows the
        // mirrored codes to be stored as 8-bit values, as most of them
        // are references to nearby character codes.
        if (v < kSmallMirrorOffset) {
            return aCh + v;
        }
        return sDistantMirrors[v - kSmallMirrorOffset];
    }
    return aCh;
}

PRUint8
gfxUnicodeProperties::GetCombiningClass(PRUint32 aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCClassValues[sCClassPages[0][aCh >> kCClassCharBits]]
                            [aCh & ((1 << kCClassCharBits) - 1)];
    }
    if (aCh >= UNICODE_LIMIT) {
        return 0;
    }
    return sCClassValues[sCClassPages[sCClassPlanes[(aCh >> 16) - 1]]
                                     [(aCh & 0xffff) >> kCClassCharBits]]
                        [aCh & ((1 << kCClassCharBits) - 1)];
}

PRUint8
gfxUnicodeProperties::GetGeneralCategory(PRUint32 aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCatEAWValues[sCatEAWPages[0][aCh >> kCatEAWCharBits]]
                            [aCh & ((1 << kCatEAWCharBits) - 1)].mCategory;
    }
    if (aCh >= UNICODE_LIMIT) {
        return PRUint8(HB_CATEGORY_UNASSIGNED);
    }
    return sCatEAWValues[sCatEAWPages[sCatEAWPlanes[(aCh >> 16) - 1]]
                                     [(aCh & 0xffff) >> kCatEAWCharBits]]
                        [aCh & ((1 << kCatEAWCharBits) - 1)].mCategory;
}

PRUint8
gfxUnicodeProperties::GetEastAsianWidth(PRUint32 aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCatEAWValues[sCatEAWPages[0][aCh >> kCatEAWCharBits]]
                            [aCh & ((1 << kCatEAWCharBits) - 1)].mEAW;
    }
    if (aCh >= UNICODE_LIMIT) {
        return 0;
    }
    return sCatEAWValues[sCatEAWPages[sCatEAWPlanes[(aCh >> 16) - 1]]
                                     [(aCh & 0xffff) >> kCatEAWCharBits]]
                        [aCh & ((1 << kCatEAWCharBits) - 1)].mEAW;
}

PRInt32
gfxUnicodeProperties::GetScriptCode(PRUint32 aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sScriptValues[sScriptPages[0][aCh >> kScriptCharBits]]
                            [aCh & ((1 << kScriptCharBits) - 1)];
    }
    if (aCh >= UNICODE_LIMIT) {
        return PRInt32(HB_SCRIPT_UNKNOWN);
    }
    return sScriptValues[sScriptPages[sScriptPlanes[(aCh >> 16) - 1]]
                                     [(aCh & 0xffff) >> kScriptCharBits]]
                        [aCh & ((1 << kScriptCharBits) - 1)];
}

gfxUnicodeProperties::HSType
gfxUnicodeProperties::GetHangulSyllableType(PRUint32 aCh)
{
    // all Hangul chars are in plane 0
    if (aCh < UNICODE_BMP_LIMIT) {
        return HSType(sHangulValues[sHangulPages[0][aCh >> kHangulCharBits]]
                                   [aCh & ((1 << kHangulCharBits) - 1)]);
    }
    return HST_NONE;
}

// TODO: replace this with a properties file or similar;
// expect this to evolve as harfbuzz shaping support matures.
//
// The "shaping level" of each script run, as returned by this
// function, is compared to the gfx.font_rendering.harfbuzz.level
// preference to decide whether to use the harfbuzz shaper.
//
// Currently, we only distinguish "simple" (level 1) scripts,
// Arabic-style cursive scripts (level 2),
// and "Indic or other complex scripts" (level 3),
// but may subdivide this further in the future.
PRInt32
gfxUnicodeProperties::ScriptShapingLevel(PRInt32 aScriptCode)
{
    switch (aScriptCode) {
    default:
        return 1; // scripts not explicitly listed here are considered
                  // level 1: default shaping behavior is adequate

    case HB_SCRIPT_ARABIC:
    case HB_SCRIPT_SYRIAC:
    case HB_SCRIPT_NKO:
        return 2; // level 2: bidi scripts with Arabic-style shaping

    case HB_SCRIPT_HEBREW:
    case HB_SCRIPT_HANGUL:
    case HB_SCRIPT_BENGALI:
    case HB_SCRIPT_DEVANAGARI:
    case HB_SCRIPT_GUJARATI:
    case HB_SCRIPT_GURMUKHI:
    case HB_SCRIPT_KANNADA:
    case HB_SCRIPT_MALAYALAM:
    case HB_SCRIPT_ORIYA:
    case HB_SCRIPT_SINHALA:
    case HB_SCRIPT_TAMIL:
    case HB_SCRIPT_TELUGU:
    case HB_SCRIPT_KHMER:
    case HB_SCRIPT_THAI:
    case HB_SCRIPT_LAO:
    case HB_SCRIPT_TIBETAN:
    case HB_SCRIPT_NEW_TAI_LUE:
    case HB_SCRIPT_TAI_LE:
    case HB_SCRIPT_MONGOLIAN:
    case HB_SCRIPT_MYANMAR:
    case HB_SCRIPT_PHAGS_PA:
    case HB_SCRIPT_BATAK:
    case HB_SCRIPT_BRAHMI:
        return 3; // scripts that require Indic or other "special" shaping
    }
}
