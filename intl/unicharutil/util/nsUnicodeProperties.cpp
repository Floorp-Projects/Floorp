/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeProperties.h"
#include "nsUnicodePropertyData.cpp"

#include "mozilla/Util.h"
#include "nsCharTraits.h"

#define UNICODE_BMP_LIMIT 0x10000
#define UNICODE_LIMIT     0x110000


const nsCharProps1&
GetCharProps1(uint32_t aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCharProp1Values[sCharProp1Pages[0][aCh >> kCharProp1CharBits]]
                               [aCh & ((1 << kCharProp1CharBits) - 1)];
    }
    if (aCh < (kCharProp1MaxPlane + 1) * 0x10000) {
        return sCharProp1Values[sCharProp1Pages[sCharProp1Planes[(aCh >> 16) - 1]]
                                               [(aCh & 0xffff) >> kCharProp1CharBits]]
                               [aCh & ((1 << kCharProp1CharBits) - 1)];
    }

    // Default values for unassigned
    static const nsCharProps1 undefined = {
        0,       // Index to mirrored char offsets
        0,       // Hangul Syllable type
        0        // Combining class
    };
    return undefined;
}

const nsCharProps2&
GetCharProps2(uint32_t aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCharProp2Values[sCharProp2Pages[0][aCh >> kCharProp2CharBits]]
                              [aCh & ((1 << kCharProp2CharBits) - 1)];
    }
    if (aCh < (kCharProp2MaxPlane + 1) * 0x10000) {
        return sCharProp2Values[sCharProp2Pages[sCharProp2Planes[(aCh >> 16) - 1]]
                                               [(aCh & 0xffff) >> kCharProp2CharBits]]
                               [aCh & ((1 << kCharProp2CharBits) - 1)];
    }

    NS_NOTREACHED("Getting CharProps for codepoint outside Unicode range");
    // Default values for unassigned
    static const nsCharProps2 undefined = {
        MOZ_SCRIPT_UNKNOWN,                      // Script code
        0,                                       // East Asian Width
        HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED,  // General Category
        eCharType_LeftToRight,                   // Bidi Category
        mozilla::unicode::XIDMOD_NOT_CHARS,      // Xidmod
        -1,                                      // Numeric Value
        mozilla::unicode::HVT_NotHan             // Han variant
    };
    return undefined;
}

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

nsIUGenCategory::nsUGenCategory sDetailedToGeneralCategory[] = {
  /*
   * The order here corresponds to the HB_UNICODE_GENERAL_CATEGORY_* constants
   * of the hb_unicode_general_category_t enum in gfx/harfbuzz/src/hb-common.h.
   */
  /* CONTROL */             nsIUGenCategory::kOther,
  /* FORMAT */              nsIUGenCategory::kOther,
  /* UNASSIGNED */          nsIUGenCategory::kOther,
  /* PRIVATE_USE */         nsIUGenCategory::kOther,
  /* SURROGATE */           nsIUGenCategory::kOther,
  /* LOWERCASE_LETTER */    nsIUGenCategory::kLetter,
  /* MODIFIER_LETTER */     nsIUGenCategory::kLetter,
  /* OTHER_LETTER */        nsIUGenCategory::kLetter,
  /* TITLECASE_LETTER */    nsIUGenCategory::kLetter,
  /* UPPERCASE_LETTER */    nsIUGenCategory::kLetter,
  /* COMBINING_MARK */      nsIUGenCategory::kMark,
  /* ENCLOSING_MARK */      nsIUGenCategory::kMark,
  /* NON_SPACING_MARK */    nsIUGenCategory::kMark,
  /* DECIMAL_NUMBER */      nsIUGenCategory::kNumber,
  /* LETTER_NUMBER */       nsIUGenCategory::kNumber,
  /* OTHER_NUMBER */        nsIUGenCategory::kNumber,
  /* CONNECT_PUNCTUATION */ nsIUGenCategory::kPunctuation,
  /* DASH_PUNCTUATION */    nsIUGenCategory::kPunctuation,
  /* CLOSE_PUNCTUATION */   nsIUGenCategory::kPunctuation,
  /* FINAL_PUNCTUATION */   nsIUGenCategory::kPunctuation,
  /* INITIAL_PUNCTUATION */ nsIUGenCategory::kPunctuation,
  /* OTHER_PUNCTUATION */   nsIUGenCategory::kPunctuation,
  /* OPEN_PUNCTUATION */    nsIUGenCategory::kPunctuation,
  /* CURRENCY_SYMBOL */     nsIUGenCategory::kSymbol,
  /* MODIFIER_SYMBOL */     nsIUGenCategory::kSymbol,
  /* MATH_SYMBOL */         nsIUGenCategory::kSymbol,
  /* OTHER_SYMBOL */        nsIUGenCategory::kSymbol,
  /* LINE_SEPARATOR */      nsIUGenCategory::kSeparator,
  /* PARAGRAPH_SEPARATOR */ nsIUGenCategory::kSeparator,
  /* SPACE_SEPARATOR */     nsIUGenCategory::kSeparator
};

uint32_t
GetMirroredChar(uint32_t aCh)
{
    return aCh + sMirrorOffsets[GetCharProps1(aCh).mMirrorOffsetIndex];
}

uint32_t
GetScriptTagForCode(int32_t aScriptCode)
{
    // this will safely return 0 for negative script codes, too :)
    if (uint32_t(aScriptCode) > ArrayLength(sScriptCodeToTag)) {
        return 0;
    }
    return sScriptCodeToTag[aScriptCode];
}

static inline uint32_t
GetCaseMapValue(uint32_t aCh)
{
    if (aCh < UNICODE_BMP_LIMIT) {
        return sCaseMapValues[sCaseMapPages[0][aCh >> kCaseMapCharBits]]
                             [aCh & ((1 << kCaseMapCharBits) - 1)];
    }
    if (aCh < (kCaseMapMaxPlane + 1) * 0x10000) {
        return sCaseMapValues[sCaseMapPages[sCaseMapPlanes[(aCh >> 16) - 1]]
                                           [(aCh & 0xffff) >> kCaseMapCharBits]]
                             [aCh & ((1 << kCaseMapCharBits) - 1)];
    }
    return 0;
}

uint32_t
GetUppercase(uint32_t aCh)
{
    uint32_t mapValue = GetCaseMapValue(aCh);
    if (mapValue & (kLowerToUpper | kTitleToUpper)) {
        return aCh ^ (mapValue & kCaseMapCharMask);
    }
    if (mapValue & kLowerToTitle) {
        return GetUppercase(aCh ^ (mapValue & kCaseMapCharMask));
    }
    return aCh;
}

uint32_t
GetLowercase(uint32_t aCh)
{
    uint32_t mapValue = GetCaseMapValue(aCh);
    if (mapValue & kUpperToLower) {
        return aCh ^ (mapValue & kCaseMapCharMask);
    }
    if (mapValue & kTitleToUpper) {
        return GetLowercase(aCh ^ (mapValue & kCaseMapCharMask));
    }
    return aCh;
}

uint32_t
GetTitlecaseForLower(uint32_t aCh)
{
    uint32_t mapValue = GetCaseMapValue(aCh);
    if (mapValue & (kLowerToTitle | kLowerToUpper)) {
        return aCh ^ (mapValue & kCaseMapCharMask);
    }
    return aCh;
}

uint32_t
GetTitlecaseForAll(uint32_t aCh)
{
    uint32_t mapValue = GetCaseMapValue(aCh);
    if (mapValue & (kLowerToTitle | kLowerToUpper)) {
        return aCh ^ (mapValue & kCaseMapCharMask);
    }
    if (mapValue & kUpperToLower) {
        return GetTitlecaseForLower(aCh ^ (mapValue & kCaseMapCharMask));
    }
    return aCh;
}

HanVariantType
GetHanVariant(uint32_t aCh)
{
    // In the sHanVariantValues array, data for 4 successive characters
    // (2 bits each) is packed in to each uint8_t entry, with the value
    // for the lowest character stored in the least significant bits.
    uint8_t v = 0;
    if (aCh < UNICODE_BMP_LIMIT) {
        v = sHanVariantValues[sHanVariantPages[0][aCh >> kHanVariantCharBits]]
                             [(aCh & ((1 << kHanVariantCharBits) - 1)) >> 2];
    } else if (aCh < (kHanVariantMaxPlane + 1) * 0x10000) {
        v = sHanVariantValues[sHanVariantPages[sHanVariantPlanes[(aCh >> 16) - 1]]
                                              [(aCh & 0xffff) >> kHanVariantCharBits]]
                             [(aCh & ((1 << kHanVariantCharBits) - 1)) >> 2];
    }
    // extract the appropriate 2-bit field from the value
    return HanVariantType((v >> ((aCh & 3) * 2)) & 3);
}

uint32_t
GetFullWidth(uint32_t aCh)
{
    // full-width mappings only exist for BMP characters; all others are
    // returned unchanged
    if (aCh < UNICODE_BMP_LIMIT) {
        uint32_t v =
            sFullWidthValues[sFullWidthPages[aCh >> kFullWidthCharBits]]
                            [aCh & ((1 << kFullWidthCharBits) - 1)];
        if (v) {
            // return the mapped value if non-zero; else return original char
            return v;
        }
    }
    return aCh;
}

bool
IsClusterExtender(uint32_t aCh, uint8_t aCategory)
{
    return ((aCategory >= HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK &&
             aCategory <= HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) ||
            (aCh >= 0x200c && aCh <= 0x200d) || // ZWJ, ZWNJ
            (aCh >= 0xff9e && aCh <= 0xff9f));  // katakana sound marks
}

// TODO: replace this with a properties file or similar;
// expect this to evolve as harfbuzz shaping support matures.
//
// The "shaping type" of each script run, as returned by this
// function, is compared to the bits set in the
// gfx.font_rendering.harfbuzz.scripts
// preference to decide whether to use the harfbuzz shaper.
//
int32_t
ScriptShapingType(int32_t aScriptCode)
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

void
ClusterIterator::Next()
{
    if (AtEnd()) {
        NS_WARNING("ClusterIterator has already reached the end");
        return;
    }

    uint32_t ch = *mPos++;

    if (NS_IS_HIGH_SURROGATE(ch) && mPos < mLimit &&
        NS_IS_LOW_SURROGATE(*mPos)) {
        ch = SURROGATE_TO_UCS4(ch, *mPos++);
    } else if ((ch & ~0xff) == 0x1100 ||
        (ch >= 0xa960 && ch <= 0xa97f) ||
        (ch >= 0xac00 && ch <= 0xd7ff)) {
        // Handle conjoining Jamo that make Hangul syllables
        HSType hangulState = GetHangulSyllableType(ch);
        while (mPos < mLimit) {
            ch = *mPos;
            HSType hangulType = GetHangulSyllableType(ch);
            switch (hangulType) {
            case HST_L:
            case HST_LV:
            case HST_LVT:
                if (hangulState == HST_L) {
                    hangulState = hangulType;
                    mPos++;
                    continue;
                }
                break;
            case HST_V:
                if ((hangulState != HST_NONE) && !(hangulState & HST_T)) {
                    hangulState = hangulType;
                    mPos++;
                    continue;
                }
                break;
            case HST_T:
                if (hangulState & (HST_V | HST_T)) {
                    hangulState = hangulType;
                    mPos++;
                    continue;
                }
                break;
            default:
                break;
            }
            break;
        }
    }

    while (mPos < mLimit) {
        ch = *mPos;

        // Check for surrogate pairs; note that isolated surrogates will just
        // be treated as generic (non-cluster-extending) characters here,
        // which is fine for cluster-iterating purposes
        if (NS_IS_HIGH_SURROGATE(ch) && mPos < mLimit - 1 &&
            NS_IS_LOW_SURROGATE(*(mPos + 1))) {
            ch = SURROGATE_TO_UCS4(ch, *(mPos + 1));
        }

        if (!IsClusterExtender(ch)) {
            break;
        }

        mPos++;
        if (!IS_IN_BMP(ch)) {
            mPos++;
        }
    }

    NS_ASSERTION(mText < mPos && mPos <= mLimit,
                 "ClusterIterator::Next has overshot the string!");
}

} // end namespace unicode

} // end namespace mozilla
