/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeProperties.h"
#include "nsUnicodePropertyData.cpp"

#include "mozilla/ArrayUtils.h"
#include "mozilla/HashTable.h"
#include "mozilla/intl/Segmenter.h"

#include "BaseChars.h"
#include "IsCombiningDiacritic.h"

#define UNICODE_BMP_LIMIT 0x10000
#define UNICODE_LIMIT 0x110000

const nsCharProps2& GetCharProps2(uint32_t aCh) {
  if (aCh < UNICODE_BMP_LIMIT) {
    return sCharProp2Values[sCharProp2Pages[0][aCh >> kCharProp2CharBits]]
                           [aCh & ((1 << kCharProp2CharBits) - 1)];
  }
  if (aCh < (kCharProp2MaxPlane + 1) * 0x10000) {
    return sCharProp2Values[sCharProp2Pages[sCharProp2Planes[(aCh >> 16) - 1]]
                                           [(aCh & 0xffff) >>
                                            kCharProp2CharBits]]
                           [aCh & ((1 << kCharProp2CharBits) - 1)];
  }

  MOZ_ASSERT_UNREACHABLE(
      "Getting CharProps for codepoint outside Unicode "
      "range");

  // Default values for unassigned
  using namespace mozilla::unicode;
  static const nsCharProps2 undefined = {
      VERTICAL_ORIENTATION_R,
      0  // IdentifierType
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

const nsUGenCategory sDetailedToGeneralCategory[] = {
    // clang-format off
  /*
   * The order here corresponds to the HB_UNICODE_GENERAL_CATEGORY_* constants
   * of the hb_unicode_general_category_t enum in gfx/harfbuzz/src/hb-unicode.h.
   */
  /* CONTROL */             nsUGenCategory::kOther,
  /* FORMAT */              nsUGenCategory::kOther,
  /* UNASSIGNED */          nsUGenCategory::kOther,
  /* PRIVATE_USE */         nsUGenCategory::kOther,
  /* SURROGATE */           nsUGenCategory::kOther,
  /* LOWERCASE_LETTER */    nsUGenCategory::kLetter,
  /* MODIFIER_LETTER */     nsUGenCategory::kLetter,
  /* OTHER_LETTER */        nsUGenCategory::kLetter,
  /* TITLECASE_LETTER */    nsUGenCategory::kLetter,
  /* UPPERCASE_LETTER */    nsUGenCategory::kLetter,
  /* COMBINING_MARK */      nsUGenCategory::kMark,
  /* ENCLOSING_MARK */      nsUGenCategory::kMark,
  /* NON_SPACING_MARK */    nsUGenCategory::kMark,
  /* DECIMAL_NUMBER */      nsUGenCategory::kNumber,
  /* LETTER_NUMBER */       nsUGenCategory::kNumber,
  /* OTHER_NUMBER */        nsUGenCategory::kNumber,
  /* CONNECT_PUNCTUATION */ nsUGenCategory::kPunctuation,
  /* DASH_PUNCTUATION */    nsUGenCategory::kPunctuation,
  /* CLOSE_PUNCTUATION */   nsUGenCategory::kPunctuation,
  /* FINAL_PUNCTUATION */   nsUGenCategory::kPunctuation,
  /* INITIAL_PUNCTUATION */ nsUGenCategory::kPunctuation,
  /* OTHER_PUNCTUATION */   nsUGenCategory::kPunctuation,
  /* OPEN_PUNCTUATION */    nsUGenCategory::kPunctuation,
  /* CURRENCY_SYMBOL */     nsUGenCategory::kSymbol,
  /* MODIFIER_SYMBOL */     nsUGenCategory::kSymbol,
  /* MATH_SYMBOL */         nsUGenCategory::kSymbol,
  /* OTHER_SYMBOL */        nsUGenCategory::kSymbol,
  /* LINE_SEPARATOR */      nsUGenCategory::kSeparator,
  /* PARAGRAPH_SEPARATOR */ nsUGenCategory::kSeparator,
  /* SPACE_SEPARATOR */     nsUGenCategory::kSeparator
    // clang-format on
};

const hb_unicode_general_category_t sICUtoHBcategory[U_CHAR_CATEGORY_COUNT] = {
    // clang-format off
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
    // clang-format on
};

#define DEFINE_BMP_1PLANE_MAPPING_GET_FUNC(prefix_)             \
  uint32_t Get##prefix_(uint32_t aCh) {                         \
    if (aCh >= UNICODE_BMP_LIMIT) {                             \
      return aCh;                                               \
    }                                                           \
    auto page = s##prefix_##Pages[aCh >> k##prefix_##CharBits]; \
    auto index = aCh & ((1 << k##prefix_##CharBits) - 1);       \
    uint32_t v = s##prefix_##Values[page][index];               \
    return v ? v : aCh;                                         \
  }

// full-width mappings only exist for BMP characters; all others are
// returned unchanged
DEFINE_BMP_1PLANE_MAPPING_GET_FUNC(FullWidth)
DEFINE_BMP_1PLANE_MAPPING_GET_FUNC(FullWidthInverse)

bool IsClusterExtender(uint32_t aCh, uint8_t aCategory) {
  return (
      (aCategory >= HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK &&
       aCategory <= HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) ||
      (aCh >= 0x200c && aCh <= 0x200d) ||    // ZWJ, ZWNJ
      (aCh >= 0xff9e && aCh <= 0xff9f) ||    // katakana sound marks
      (aCh >= 0x1F3FB && aCh <= 0x1F3FF) ||  // fitzpatrick skin tone modifiers
      (aCh >= 0xe0020 && aCh <= 0xe007f));   // emoji (flag) tag characters
}

uint32_t CountGraphemeClusters(Span<const char16_t> aText) {
  intl::GraphemeClusterBreakIteratorUtf16 iter(aText);
  uint32_t result = 0;
  while (iter.Next()) {
    ++result;
  }
  return result;
}

uint32_t GetNaked(uint32_t aCh) {
  uint32_t index = aCh >> 8;
  if (index >= MOZ_ARRAY_LENGTH(BASE_CHAR_MAPPING_BLOCK_INDEX)) {
    return aCh;
  }
  index = BASE_CHAR_MAPPING_BLOCK_INDEX[index];
  if (index == 0xff) {
    return aCh;
  }
  const BaseCharMappingBlock& block = BASE_CHAR_MAPPING_BLOCKS[index];
  uint8_t lo = aCh & 0xff;
  if (lo < block.mFirst || lo > block.mLast) {
    return aCh;
  }
  return (aCh & 0xffff0000) |
         BASE_CHAR_MAPPING_LIST[block.mMappingStartOffset + lo - block.mFirst];
}

bool IsCombiningDiacritic(uint32_t aCh) {
  return sCombiningDiacriticsSet->test(aCh);
}

}  // end namespace unicode

}  // end namespace mozilla
