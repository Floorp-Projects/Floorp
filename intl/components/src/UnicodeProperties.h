/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_UnicodeProperties_h_
#define intl_components_UnicodeProperties_h_

#include "mozilla/intl/BidiClass.h"
#include "mozilla/intl/GeneralCategory.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/UnicodeScriptCodes.h"
#include "mozilla/Vector.h"

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
  static inline GeneralCategory CharType(uint32_t aCh) {
    return GeneralCategory(u_charType(aCh));
  }

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
    return CharType(aCh) == GeneralCategory::Math_Symbol ||
           CharType(aCh) == GeneralCategory::Other_Symbol;
  }

  static inline Script GetScriptCode(uint32_t aCh) {
    // We can safely ignore the error code here because uscript_getScript
    // returns USCRIPT_INVALID_CODE in the event of an error.
    UErrorCode err = U_ZERO_ERROR;
    return Script(uscript_getScript(aCh, &err));
  }

  static inline bool HasScript(uint32_t aCh, Script aScript) {
    return uscript_hasScript(aCh, UScriptCode(aScript));
  }

  static inline const char* GetScriptShortName(Script aScript) {
    return uscript_getShortName(UScriptCode(aScript));
  }

  static inline int32_t GetMaxNumberOfScripts() {
    return u_getIntPropertyMaxValue(UCHAR_SCRIPT);
  }

  // The code point which has the most script extensions is 0x0965, which has 21
  // script extensions, so choose the vector size as 32 to prevent heap
  // allocation.
  static constexpr size_t kMaxScripts = 32;

  using ScriptExtensionVector = Vector<Script, kMaxScripts>;

  /**
   * Get the script extensions for the given code point, and write the script
   * extensions to aExtensions vector. If the code point has script extensions,
   * the script code (Script::COMMON or Script::INHERITED) will be excluded.
   *
   * If the code point doesn't have any script extension, then its script code
   * will be written to aExtensions vector.
   *
   * If the code point is invalid, Script::UNKNOWN will be written to
   * aExtensions vector.
   *
   * Note: aExtensions will be cleared after calling this method regardless of
   * failure.
   *
   * See [1] for the script code of the code point, [2] for the script
   * extensions.
   *
   * https://www.unicode.org/Public/UNIDATA/Scripts.txt
   * https://www.unicode.org/Public/UNIDATA/ScriptExtensions.txt
   */
  static ICUResult GetExtensions(char32_t aCodePoint,
                                 ScriptExtensionVector& aExtensions) {
    // Clear the vector first.
    aExtensions.clear();

    // We cannot pass aExtensions to uscript_getScriptExtension as USCriptCode
    // takes 4 bytes, so create a local UScriptCode array to get the extensions.
    UScriptCode ext[kMaxScripts];
    UErrorCode status = U_ZERO_ERROR;
    int32_t len = uscript_getScriptExtensions(static_cast<UChar32>(aCodePoint),
                                              ext, kMaxScripts, &status);
    if (U_FAILURE(status)) {
      // kMaxScripts should be large enough to hold the maximun number of script
      // extensions.
      MOZ_DIAGNOSTIC_ASSERT(status != U_BUFFER_OVERFLOW_ERROR);
      return Err(ToICUError(status));
    }

    if (!aExtensions.reserve(len)) {
      return Err(ICUError::OutOfMemory);
    }

    for (int32_t i = 0; i < len; i++) {
      aExtensions.infallibleAppend(Script(ext[i]));
    }

    return Ok();
  }
};

}  // namespace mozilla::intl

#endif
