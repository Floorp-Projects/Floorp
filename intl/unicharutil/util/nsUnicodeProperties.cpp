/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeProperties.h"
#include "nsUnicodePropertyData.cpp"

#include "mozilla/ArrayUtils.h"
#include "nsCharTraits.h"

#define UNICODE_BMP_LIMIT 0x10000
#define UNICODE_LIMIT     0x110000

#ifndef ENABLE_INTL_API
static const nsCharProps1&
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
#endif

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
    using namespace mozilla::unicode;
    static const nsCharProps2 undefined = {
#if ENABLE_INTL_API
        PAIRED_BRACKET_TYPE_NONE,
        VERTICAL_ORIENTATION_R,
        XIDMOD_NOT_CHARS
#else
        uint8_t(Script::UNKNOWN),
        PAIRED_BRACKET_TYPE_NONE,
        HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED,
        eCharType_LeftToRight,
        XIDMOD_NOT_CHARS,
        -1, // Numeric Value
        VERTICAL_ORIENTATION_R
#endif
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

const nsIUGenCategory::nsUGenCategory sDetailedToGeneralCategory[] = {
  /*
   * The order here corresponds to the HB_UNICODE_GENERAL_CATEGORY_* constants
   * of the hb_unicode_general_category_t enum in gfx/harfbuzz/src/hb-unicode.h.
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

#ifdef ENABLE_INTL_API
const hb_unicode_general_category_t sICUtoHBcategory[U_CHAR_CATEGORY_COUNT] = {
  HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED, // U_GENERAL_OTHER_TYPES = 0,
  HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER, // U_UPPERCASE_LETTER = 1,
  HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER, // U_LOWERCASE_LETTER = 2,
  HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER, // U_TITLECASE_LETTER = 3,
  HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER, // U_MODIFIER_LETTER = 4,
  HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER, // U_OTHER_LETTER = 5,
  HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK, // U_NON_SPACING_MARK = 6,
  HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK, // U_ENCLOSING_MARK = 7,
  HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK, // U_COMBINING_SPACING_MARK = 8,
  HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER, // U_DECIMAL_DIGIT_NUMBER = 9,
  HB_UNICODE_GENERAL_CATEGORY_LETTER_NUMBER, // U_LETTER_NUMBER = 10,
  HB_UNICODE_GENERAL_CATEGORY_OTHER_NUMBER, // U_OTHER_NUMBER = 11,
  HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR, // U_SPACE_SEPARATOR = 12,
  HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR, // U_LINE_SEPARATOR = 13,
  HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR, // U_PARAGRAPH_SEPARATOR = 14,
  HB_UNICODE_GENERAL_CATEGORY_CONTROL, // U_CONTROL_CHAR = 15,
  HB_UNICODE_GENERAL_CATEGORY_FORMAT, // U_FORMAT_CHAR = 16,
  HB_UNICODE_GENERAL_CATEGORY_PRIVATE_USE, // U_PRIVATE_USE_CHAR = 17,
  HB_UNICODE_GENERAL_CATEGORY_SURROGATE, // U_SURROGATE = 18,
  HB_UNICODE_GENERAL_CATEGORY_DASH_PUNCTUATION, // U_DASH_PUNCTUATION = 19,
  HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION, // U_START_PUNCTUATION = 20,
  HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION, // U_END_PUNCTUATION = 21,
  HB_UNICODE_GENERAL_CATEGORY_CONNECT_PUNCTUATION, // U_CONNECTOR_PUNCTUATION = 22,
  HB_UNICODE_GENERAL_CATEGORY_OTHER_PUNCTUATION, // U_OTHER_PUNCTUATION = 23,
  HB_UNICODE_GENERAL_CATEGORY_MATH_SYMBOL, // U_MATH_SYMBOL = 24,
  HB_UNICODE_GENERAL_CATEGORY_CURRENCY_SYMBOL, // U_CURRENCY_SYMBOL = 25,
  HB_UNICODE_GENERAL_CATEGORY_MODIFIER_SYMBOL, // U_MODIFIER_SYMBOL = 26,
  HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL, // U_OTHER_SYMBOL = 27,
  HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION, // U_INITIAL_PUNCTUATION = 28,
  HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION, // U_FINAL_PUNCTUATION = 29,
};
#endif

#if !ENABLE_INTL_API
uint8_t GetGeneralCategory(uint32_t aCh) {
  return GetCharProps2(aCh).mCategory;
}

nsCharType GetBidiCat(uint32_t aCh) {
  return nsCharType(GetCharProps2(aCh).mBidiCategory);
}

int8_t GetNumericValue(uint32_t aCh) {
  return GetCharProps2(aCh).mNumericValue;
}

uint32_t
GetMirroredChar(uint32_t aCh)
{
    return aCh + sMirrorOffsets[GetCharProps1(aCh).mMirrorOffsetIndex];
}

bool
HasMirroredChar(uint32_t aCh)
{
    return GetCharProps1(aCh).mMirrorOffsetIndex != 0;
}

uint8_t
GetCombiningClass(uint32_t aCh)
{
    return GetCharProps1(aCh).mCombiningClass;
}

uint8_t
GetLineBreakClass(uint32_t aCh)
{
    return GetCharProps2(aCh).mLineBreak;
}

Script
GetScriptCode(uint32_t aCh)
{
    return Script(GetCharProps2(aCh).mScriptCode);
}

uint32_t
GetScriptTagForCode(Script aScriptCode)
{
    // this will safely return 0 for negative script codes, too :)
    if (static_cast<uint32_t>(aScriptCode) > ArrayLength(sScriptCodeToTag)) {
        return 0;
    }
    return sScriptCodeToTag[static_cast<uint32_t>(aScriptCode)];
}

PairedBracketType GetPairedBracketType(uint32_t aCh)
{
  return PairedBracketType(GetCharProps2(aCh).mPairedBracketType);
}

uint32_t GetPairedBracket(uint32_t aCh)
{
  return GetPairedBracketType(aCh) != PAIRED_BRACKET_TYPE_NONE
         ? GetMirroredChar(aCh) : aCh;
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
#endif

#define DEFINE_BMP_1PLANE_MAPPING_GET_FUNC(prefix_) \
  uint32_t Get##prefix_(uint32_t aCh) \
  { \
    if (aCh >= UNICODE_BMP_LIMIT) { \
      return aCh; \
    } \
    auto page = s##prefix_##Pages[aCh >> k##prefix_##CharBits]; \
    auto index = aCh & ((1 << k##prefix_##CharBits) - 1); \
    uint32_t v = s##prefix_##Values[page][index]; \
    return v ? v : aCh; \
  }

// full-width mappings only exist for BMP characters; all others are
// returned unchanged
DEFINE_BMP_1PLANE_MAPPING_GET_FUNC(FullWidth)
DEFINE_BMP_1PLANE_MAPPING_GET_FUNC(FullWidthInverse)

bool
IsClusterExtender(uint32_t aCh, uint8_t aCategory)
{
    return ((aCategory >= HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK &&
             aCategory <= HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) ||
            (aCh >= 0x200c && aCh <= 0x200d) || // ZWJ, ZWNJ
            (aCh >= 0xff9e && aCh <= 0xff9f));  // katakana sound marks
}

enum HSType {
#if ENABLE_INTL_API
    HST_NONE = U_HST_NOT_APPLICABLE,
    HST_L    = U_HST_LEADING_JAMO,
    HST_V    = U_HST_VOWEL_JAMO,
    HST_T    = U_HST_TRAILING_JAMO,
    HST_LV   = U_HST_LV_SYLLABLE,
    HST_LVT  = U_HST_LVT_SYLLABLE
#else
    HST_NONE = 0x00,
    HST_L    = 0x01,
    HST_V    = 0x02,
    HST_T    = 0x04,
    HST_LV   = 0x03,
    HST_LVT  = 0x07
#endif
};

static HSType
GetHangulSyllableType(uint32_t aCh)
{
#if ENABLE_INTL_API
    return HSType(u_getIntPropertyValue(aCh, UCHAR_HANGUL_SYLLABLE_TYPE));
#else
    return HSType(GetCharProps1(aCh).mHangulType);
#endif
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
                if ((hangulState != HST_NONE) && (hangulState != HST_T) &&
                    (hangulState != HST_LVT)) {
                    hangulState = hangulType;
                    mPos++;
                    continue;
                }
                break;
            case HST_T:
                if (hangulState != HST_NONE && hangulState != HST_L) {
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

void
ClusterReverseIterator::Next()
{
    if (AtEnd()) {
        NS_WARNING("ClusterReverseIterator has already reached the end");
        return;
    }

    uint32_t ch;
    do {
        ch = *--mPos;

        if (NS_IS_LOW_SURROGATE(ch) && mPos > mLimit &&
            NS_IS_HIGH_SURROGATE(*(mPos - 1))) {
            ch = SURROGATE_TO_UCS4(*--mPos, ch);
        }

        if (!IsClusterExtender(ch)) {
            break;
        }
    } while (mPos > mLimit);

    // XXX May need to handle conjoining Jamo

    NS_ASSERTION(mPos >= mLimit,
                 "ClusterReverseIterator::Next has overshot the string!");
}

uint32_t
CountGraphemeClusters(const char16_t* aText, uint32_t aLength)
{
  ClusterIterator iter(aText, aLength);
  uint32_t result = 0;
  while (!iter.AtEnd()) {
    ++result;
    iter.Next();
  }
  return result;
}

} // end namespace unicode

} // end namespace mozilla
