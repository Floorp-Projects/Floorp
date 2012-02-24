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

#include "nsUnicodeProperties.h"
#include "nsUnicodeScriptCodes.h"
#include "nsUnicodePropertyData.cpp"

#include "mozilla/Util.h"
#include "nsMemory.h"

#include "harfbuzz/hb-unicode.h"

#define UNICODE_BMP_LIMIT 0x10000
#define UNICODE_LIMIT     0x110000

namespace mozilla {

namespace unicode {

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
GetMirroredChar(PRUint32 aCh)
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
GetCombiningClass(PRUint32 aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCClassValues[sCClassPages[0][aCh >> kCClassCharBits]]
                            [aCh & ((1 << kCClassCharBits) - 1)];
    }
    if (aCh < UNICODE_LIMIT) {
        return sCClassValues[sCClassPages[sCClassPlanes[(aCh >> 16) - 1]]
                                         [(aCh & 0xffff) >> kCClassCharBits]]
                            [aCh & ((1 << kCClassCharBits) - 1)];
    }
    NS_NOTREACHED("invalid Unicode character!");
    return 0;
}

PRUint8
GetGeneralCategory(PRUint32 aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCatEAWValues[sCatEAWPages[0][aCh >> kCatEAWCharBits]]
                            [aCh & ((1 << kCatEAWCharBits) - 1)].mCategory;
    }
    if (aCh < UNICODE_LIMIT) {
        return sCatEAWValues[sCatEAWPages[sCatEAWPlanes[(aCh >> 16) - 1]]
                                         [(aCh & 0xffff) >> kCatEAWCharBits]]
                            [aCh & ((1 << kCatEAWCharBits) - 1)].mCategory;
    }
    NS_NOTREACHED("invalid Unicode character!");
    return PRUint8(HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED);
}

PRUint8
GetEastAsianWidth(PRUint32 aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCatEAWValues[sCatEAWPages[0][aCh >> kCatEAWCharBits]]
                            [aCh & ((1 << kCatEAWCharBits) - 1)].mEAW;
    }
    if (aCh < UNICODE_LIMIT) {
        return sCatEAWValues[sCatEAWPages[sCatEAWPlanes[(aCh >> 16) - 1]]
                                         [(aCh & 0xffff) >> kCatEAWCharBits]]
                            [aCh & ((1 << kCatEAWCharBits) - 1)].mEAW;
    }
    NS_NOTREACHED("invalid Unicode character!");
    return 0;
}

PRInt32
GetScriptCode(PRUint32 aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sScriptValues[sScriptPages[0][aCh >> kScriptCharBits]]
                            [aCh & ((1 << kScriptCharBits) - 1)];
    }
    if (aCh < UNICODE_LIMIT) {
        return sScriptValues[sScriptPages[sScriptPlanes[(aCh >> 16) - 1]]
                                         [(aCh & 0xffff) >> kScriptCharBits]]
                            [aCh & ((1 << kScriptCharBits) - 1)];
    }
    NS_NOTREACHED("invalid Unicode character!");
    return MOZ_SCRIPT_UNKNOWN;
}

PRUint32
GetScriptTagForCode(PRInt32 aScriptCode)
{
    // this will safely return 0 for negative script codes, too :)
    if (PRUint32(aScriptCode) > ArrayLength(sScriptCodeToTag)) {
        return 0;
    }
    return sScriptCodeToTag[aScriptCode];
}

HSType
GetHangulSyllableType(PRUint32 aCh)
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
// The "shaping type" of each script run, as returned by this
// function, is compared to the bits set in the
// gfx.font_rendering.harfbuzz.scripts
// preference to decide whether to use the harfbuzz shaper.
//
PRInt32
ScriptShapingType(PRInt32 aScriptCode)
{
    switch (aScriptCode) {
    default:
        return SHAPING_DEFAULT; // scripts not explicitly listed here are
                                // assumed to just use default shaping

    case MOZ_SCRIPT_ARABIC:
    case MOZ_SCRIPT_SYRIAC:
    case MOZ_SCRIPT_NKO:
    case MOZ_SCRIPT_MANDAIC:
        return SHAPING_ARABIC; // bidi scripts with Arabic-style shaping

    case MOZ_SCRIPT_HEBREW:
        return SHAPING_HEBREW;

    case MOZ_SCRIPT_HANGUL:
        return SHAPING_HANGUL;

    case MOZ_SCRIPT_MONGOLIAN: // to be supported by the Arabic shaper?
        return SHAPING_MONGOLIAN;

    case MOZ_SCRIPT_THAI: // no complex OT features, but MS engines like to do
                          // sequence checking
        return SHAPING_THAI;

    case MOZ_SCRIPT_BENGALI:
    case MOZ_SCRIPT_DEVANAGARI:
    case MOZ_SCRIPT_GUJARATI:
    case MOZ_SCRIPT_GURMUKHI:
    case MOZ_SCRIPT_KANNADA:
    case MOZ_SCRIPT_MALAYALAM:
    case MOZ_SCRIPT_ORIYA:
    case MOZ_SCRIPT_SINHALA:
    case MOZ_SCRIPT_TAMIL:
    case MOZ_SCRIPT_TELUGU:
    case MOZ_SCRIPT_KHMER:
    case MOZ_SCRIPT_LAO:
    case MOZ_SCRIPT_TIBETAN:
    case MOZ_SCRIPT_NEW_TAI_LUE:
    case MOZ_SCRIPT_TAI_LE:
    case MOZ_SCRIPT_MYANMAR:
    case MOZ_SCRIPT_PHAGS_PA:
    case MOZ_SCRIPT_BATAK:
    case MOZ_SCRIPT_BRAHMI:
        return SHAPING_INDIC; // scripts that require Indic or other "special" shaping
    }
}

} // end namespace unicode

} // end namespace mozilla
