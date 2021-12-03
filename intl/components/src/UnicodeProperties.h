/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_UnicodeProperties_h_
#define intl_components_UnicodeProperties_h_

#include "mozilla/intl/BidiClass.h"

#include "unicode/uchar.h"
#include "unicode/uscript.h"

namespace mozilla::intl {

/**
 * This component is a Mozilla-focused API for working with text properties.
 */
class UnicodeProperties final {
 public:
  /**
   * Return the BidiClass for the character.
   */
  static inline BidiClass GetBidiClass(uint32_t aCh) {
    return BidiClass(u_charDirection(aCh));
  }

  /**
   * Maps the specified character to a "mirror-image" character.
   */
  static inline uint32_t CharMirror(uint32_t aCh) { return u_charMirror(aCh); }

  /**
   * Return the general category value for the code point.
   */
  static inline uint32_t CharType(uint32_t aCh) { return u_charType(aCh); }

  /**
   * Determine whether the code point has the Bidi_Mirrored property.
   */
  static inline bool IsMirrored(uint32_t aCh) { return u_isMirrored(aCh); }

  /**
   * Returns the combining class of the code point as specified in
   * UnicodeData.txt.
   */
  static inline uint8_t GetCombiningClass(uint32_t aCh) {
    return u_getCombiningClass(aCh);
  }

  enum class IntProperty {
    BidiPairedBracketType,
    EastAsianWidth,
    HangulSyllableType,
    LineBreak,
    NumericType,
  };

  /**
   * Get the property value for an enumerated or integer Unicode property for a
   * code point.
   */
  static inline int32_t GetIntPropertyValue(uint32_t aCh, IntProperty aProp) {
    UProperty prop;
    switch (aProp) {
      case IntProperty::BidiPairedBracketType:
        prop = UCHAR_BIDI_PAIRED_BRACKET_TYPE;
        break;
      case IntProperty::EastAsianWidth:
        prop = UCHAR_EAST_ASIAN_WIDTH;
        break;
      case IntProperty::HangulSyllableType:
        prop = UCHAR_HANGUL_SYLLABLE_TYPE;
        break;
      case IntProperty::LineBreak:
        prop = UCHAR_LINE_BREAK;
        break;
      case IntProperty::NumericType:
        prop = UCHAR_NUMERIC_TYPE;
        break;
    }
    return u_getIntPropertyValue(aCh, prop);
  }

  /**
   * Get the numeric value for a Unicode code point as defined in the
   * Unicode Character Database if the input is decimal or a digit,
   * otherwise, returns -1.
   */
  static inline int8_t GetNumericValue(uint32_t aCh) {
    UNumericType type =
        UNumericType(GetIntPropertyValue(aCh, IntProperty::NumericType));
    return type == U_NT_DECIMAL || type == U_NT_DIGIT
               ? int8_t(u_getNumericValue(aCh))
               : -1;
  }

  /**
   * Maps the specified character to its paired bracket character.
   */
  static inline uint32_t GetBidiPairedBracket(uint32_t aCh) {
    return u_getBidiPairedBracket(aCh);
  }

  /**
   * The given character is mapped to its uppercase equivalent according to
   * UnicodeData.txt; if the character has no uppercase equivalent, the
   * character itself is returned.
   */
  static inline uint32_t ToUpper(uint32_t aCh) { return u_toupper(aCh); }

  /**
   * The given character is mapped to its lowercase equivalent according to
   * UnicodeData.txt; if the character has no lowercase equivalent, the
   * character itself is returned.
   */
  static inline uint32_t ToLower(uint32_t aCh) { return u_tolower(aCh); }

  /**
   * Check if a code point has the Lowercase Unicode property.
   */
  static inline bool IsLowercase(uint32_t aCh) { return u_isULowercase(aCh); }

  /**
   * The given character is mapped to its titlecase equivalent according to
   * UnicodeData.txt; if the character has no titlecase equivalent, the
   * character itself is returned.
   */
  static inline uint32_t ToTitle(uint32_t aCh) { return u_totitle(aCh); }

  /**
   * The given character is mapped to its case folding equivalent according to
   * UnicodeData.txt and CaseFolding.txt;
   * if the character has no case folding equivalent, the character
   * itself is returned.
   */
  static inline uint32_t FoldCase(uint32_t aCh) {
    return u_foldCase(aCh, U_FOLD_CASE_DEFAULT);
  }

  enum class BinaryProperty {
    DefaultIgnorableCodePoint,
    Emoji,
    EmojiPresentation,
  };

  /**
   * Check a binary Unicode property for a code point.
   */
  static inline bool HasBinaryProperty(uint32_t aCh, BinaryProperty aProp) {
    UProperty prop;
    switch (aProp) {
      case BinaryProperty::DefaultIgnorableCodePoint:
        prop = UCHAR_DEFAULT_IGNORABLE_CODE_POINT;
        break;
      case BinaryProperty::Emoji:
        prop = UCHAR_EMOJI;
        break;
      case BinaryProperty::EmojiPresentation:
        prop = UCHAR_EMOJI_PRESENTATION;
        break;
    }
    return u_hasBinaryProperty(aCh, prop);
  }

  /**
   * Check if the width of aCh is full width, half width or wide
   * excluding emoji.
   */
  static inline bool IsEastAsianWidthFHWexcludingEmoji(uint32_t aCh) {
    switch (GetIntPropertyValue(aCh, IntProperty::EastAsianWidth)) {
      case U_EA_FULLWIDTH:
      case U_EA_HALFWIDTH:
        return true;
      case U_EA_WIDE:
        return HasBinaryProperty(aCh, BinaryProperty::Emoji) ? false : true;
      case U_EA_AMBIGUOUS:
      case U_EA_NARROW:
      case U_EA_NEUTRAL:
        return false;
    }
    return false;
  }

  /**
   * Check if the width of aCh is ambiguous, full width, or wide.
   */
  static inline bool IsEastAsianWidthAFW(uint32_t aCh) {
    switch (GetIntPropertyValue(aCh, IntProperty::EastAsianWidth)) {
      case U_EA_AMBIGUOUS:
      case U_EA_FULLWIDTH:
      case U_EA_WIDE:
        return true;
      case U_EA_HALFWIDTH:
      case U_EA_NARROW:
      case U_EA_NEUTRAL:
        return false;
    }
    return false;
  }

  /**
   * Check if the width of aCh is full width, or wide.
   */
  static inline bool IsEastAsianWidthFW(uint32_t aCh) {
    switch (GetIntPropertyValue(aCh, IntProperty::EastAsianWidth)) {
      case U_EA_FULLWIDTH:
      case U_EA_WIDE:
        return true;
      case U_EA_AMBIGUOUS:
      case U_EA_HALFWIDTH:
      case U_EA_NARROW:
      case U_EA_NEUTRAL:
        return false;
    }
    return false;
  }

  /**
   * Check if the CharType of aCh is math or other symbol.
   */
  static inline bool IsMathOrMusicSymbol(uint32_t aCh) {
    // Keep this function in sync with is_math_symbol in base_chars.py.
    return CharType(aCh) == U_MATH_SYMBOL || CharType(aCh) == U_OTHER_SYMBOL;
  }
};

}  // namespace mozilla::intl

#endif
