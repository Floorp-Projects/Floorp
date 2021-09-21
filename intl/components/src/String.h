/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_String_h_
#define intl_components_String_h_

#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"

#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/utypes.h"

namespace mozilla::intl {

/**
 * This component is a Mozilla-focused API for working with strings in
 * internationalization code.
 */
class String final {
 public:
  String() = delete;

  /**
   * Return the locale-sensitive lower case string of the input.
   */
  template <typename B>
  static Result<Ok, ICUError> ToLocaleLowerCase(const char* aLocale,
                                                Span<const char16_t> aString,
                                                B& aBuffer) {
    if (!aBuffer.reserve(aString.size())) {
      return Err(ICUError::OutOfMemory);
    }
    return FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          return u_strToLower(target, length, aString.data(), aString.size(),
                              aLocale, status);
        });
  }

  /**
   * Return the locale-sensitive upper case string of the input.
   */
  template <typename B>
  static Result<Ok, ICUError> ToLocaleUpperCase(const char* aLocale,
                                                Span<const char16_t> aString,
                                                B& aBuffer) {
    if (!aBuffer.reserve(aString.size())) {
      return Err(ICUError::OutOfMemory);
    }
    return FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          return u_strToUpper(target, length, aString.data(), aString.size(),
                              aLocale, status);
        });
  }

  /**
   * Return true if the code point has the binary property "Cased".
   */
  static bool IsCased(char32_t codePoint) {
    return u_hasBinaryProperty(static_cast<UChar32>(codePoint), UCHAR_CASED);
  }

  /**
   * Return true if the code point has the binary property "Case_Ignorable".
   */
  static bool IsCaseIgnorable(char32_t codePoint) {
    return u_hasBinaryProperty(static_cast<UChar32>(codePoint),
                               UCHAR_CASE_IGNORABLE);
  }
};

}  // namespace mozilla::intl

#endif
