/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_UNICODEPROPERTIES_H
#define NS_UNICODEPROPERTIES_H

#include "mozilla/intl/UnicodeProperties.h"

#include "mozilla/Span.h"
#include "nsBidiUtils.h"
#include "nsUGenCategory.h"
#include "harfbuzz/hb.h"

struct nsCharProps2 {
  // Currently only 4 bits are defined here, so 4 more could be added without
  // affecting the storage requirements for this struct. Or we could pack two
  // records per byte, at the cost of a slightly more complex accessor.
  unsigned char mVertOrient : 2;
  unsigned char mIdType : 2;
};

const nsCharProps2& GetCharProps2(uint32_t aCh);

namespace mozilla {

namespace unicode {

extern const nsUGenCategory sDetailedToGeneralCategory[];

/* This MUST match the values assigned by genUnicodePropertyData.pl! */
enum VerticalOrientation {
  VERTICAL_ORIENTATION_U = 0,
  VERTICAL_ORIENTATION_R = 1,
  VERTICAL_ORIENTATION_Tu = 2,
  VERTICAL_ORIENTATION_Tr = 3
};

/* This MUST match the values assigned by genUnicodePropertyData.pl! */
enum PairedBracketType {
  PAIRED_BRACKET_TYPE_NONE = 0,
  PAIRED_BRACKET_TYPE_OPEN = 1,
  PAIRED_BRACKET_TYPE_CLOSE = 2
};

/* Flags for Unicode security IdentifierType.txt attributes. Only a subset
   of these are currently checked by Gecko, so we only define flags for the
   ones we need. */
enum IdentifierType {
  IDTYPE_RESTRICTED = 0,
  IDTYPE_ALLOWED = 1,
};

enum EmojiPresentation { TextOnly = 0, TextDefault = 1, EmojiDefault = 2 };

const uint32_t kVariationSelector15 = 0xFE0E;  // text presentation
const uint32_t kVariationSelector16 = 0xFE0F;  // emoji presentation

// Unicode values for EMOJI MODIFIER FITZPATRICK TYPE-*
const uint32_t kEmojiSkinToneFirst = 0x1f3fb;
const uint32_t kEmojiSkinToneLast = 0x1f3ff;

extern const hb_unicode_general_category_t sICUtoHBcategory[];

// NOTE: This returns values matching harfbuzz HB_UNICODE_GENERAL_CATEGORY_*
// constants, NOT the mozilla::intl::GeneralCategory enum.
// For the GeneralCategory enum, use intl::UnicodeProperties::CharType itself.
inline uint8_t GetGeneralCategory(uint32_t aCh) {
  return sICUtoHBcategory[unsigned(intl::UnicodeProperties::CharType(aCh))];
}

inline int8_t GetNumericValue(uint32_t aCh) {
  return intl::UnicodeProperties::GetNumericValue(aCh);
}

inline uint8_t GetLineBreakClass(uint32_t aCh) {
  return intl::UnicodeProperties::GetIntPropertyValue(
      aCh, intl::UnicodeProperties::IntProperty::LineBreak);
}

inline uint32_t GetScriptTagForCode(intl::Script aScriptCode) {
  const char* tag = intl::UnicodeProperties::GetScriptShortName(aScriptCode);
  if (tag) {
    return HB_TAG(tag[0], tag[1], tag[2], tag[3]);
  }
  // return UNKNOWN script tag (running with older ICU?)
  return HB_SCRIPT_UNKNOWN;
}

inline PairedBracketType GetPairedBracketType(uint32_t aCh) {
  return PairedBracketType(intl::UnicodeProperties::GetIntPropertyValue(
      aCh, intl::UnicodeProperties::IntProperty::BidiPairedBracketType));
}

inline uint32_t GetTitlecaseForLower(
    uint32_t aCh)  // maps LC to titlecase, UC unchanged
{
  return intl::UnicodeProperties::IsLowercase(aCh)
             ? intl::UnicodeProperties::ToTitle(aCh)
             : aCh;
}

inline uint32_t GetTitlecaseForAll(
    uint32_t aCh)  // maps both UC and LC to titlecase
{
  return intl::UnicodeProperties::ToTitle(aCh);
}

inline uint32_t GetFoldedcase(uint32_t aCh) {
  // Handle dotted capital I and dotless small i specially because we want to
  // use a combination of ordinary case-folding rules and Turkish case-folding
  // rules.
  if (aCh == 0x0130 || aCh == 0x0131) {
    return 'i';
  }
  return intl::UnicodeProperties::FoldCase(aCh);
}

inline bool IsDefaultIgnorable(uint32_t aCh) {
  return intl::UnicodeProperties::HasBinaryProperty(
      aCh, intl::UnicodeProperties::BinaryProperty::DefaultIgnorableCodePoint);
}

inline EmojiPresentation GetEmojiPresentation(uint32_t aCh) {
  if (!intl::UnicodeProperties::HasBinaryProperty(
          aCh, intl::UnicodeProperties::BinaryProperty::Emoji)) {
    return TextOnly;
  }

  if (intl::UnicodeProperties::HasBinaryProperty(
          aCh, intl::UnicodeProperties::BinaryProperty::EmojiPresentation)) {
    return EmojiDefault;
  }
  return TextDefault;
}

// returns the simplified Gen Category as defined in nsUGenCategory
inline nsUGenCategory GetGenCategory(uint32_t aCh) {
  return sDetailedToGeneralCategory[GetGeneralCategory(aCh)];
}

inline VerticalOrientation GetVerticalOrientation(uint32_t aCh) {
  return VerticalOrientation(GetCharProps2(aCh).mVertOrient);
}

inline IdentifierType GetIdentifierType(uint32_t aCh) {
  return IdentifierType(GetCharProps2(aCh).mIdType);
}

uint32_t GetFullWidth(uint32_t aCh);
// This is the reverse function of GetFullWidth which guarantees that
// for every codepoint c, GetFullWidthInverse(GetFullWidth(c)) == c.
// Note that, this function does not guarantee to convert all wide
// form characters to their possible narrow form.
uint32_t GetFullWidthInverse(uint32_t aCh);

bool IsClusterExtender(uint32_t aCh, uint8_t aCategory);

inline bool IsClusterExtender(uint32_t aCh) {
  // There are no cluster-extender characters before the first combining-
  // character block at U+03xx, so we short-circuit here to avoid the cost
  // of calling GetGeneralCategory for Latin-1 letters etc.
  return aCh >= 0x0300 && IsClusterExtender(aCh, GetGeneralCategory(aCh));
}

bool IsClusterExtenderExcludingJoiners(uint32_t aCh, uint8_t aCategory);

inline bool IsClusterExtenderExcludingJoiners(uint32_t aCh) {
  return aCh >= 0x0300 &&
         IsClusterExtenderExcludingJoiners(aCh, GetGeneralCategory(aCh));
}

// Count the number of grapheme clusters in the given string
uint32_t CountGraphemeClusters(Span<const char16_t> aText);

// Determine whether a character is a "combining diacritic" for the purpose
// of diacritic-insensitive text search. Examples of such characters include
// European accents and Hebrew niqqud, but not Hangul components or Thaana
// vowels, even though Thaana vowels are combining nonspacing marks that could
// be considered diacritics.
// As an exception to strictly following Unicode properties, we exclude the
// Japanese kana voicing marks
//   3099;COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK;Mn;8;NSM
//   309A;COMBINING KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK;Mn;8;NSM
// which users report should not be ignored (bug 1624244).
// See is_combining_diacritic in base_chars.py and is_combining_diacritic.py.
//
// TODO: once ICU4X is integrated (replacing ICU4C) as the source of Unicode
// properties, re-evaluate whether building the static bitset is worthwhile
// or if we can revert to simply getting the combining class and comparing
// to the values we care about at runtime.
bool IsCombiningDiacritic(uint32_t aCh);

// Remove diacritics from a character
uint32_t GetNaked(uint32_t aCh);

}  // end namespace unicode

}  // end namespace mozilla

#endif /* NS_UNICODEPROPERTIES_H */
