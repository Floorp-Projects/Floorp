/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_String_h_
#define intl_components_String_h_

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"

#include "unicode/uchar.h"
#include "unicode/unorm2.h"
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
   * Normalization form constants to describe which normalization algorithm
   * should be performed.
   *
   * Also see:
   * - Unicode Standard, §2.12 Equivalent Sequences
   * - Unicode Standard, §3.11 Normalization Forms
   * - https://unicode.org/reports/tr15/
   */
  enum class NormalizationForm {
    /**
     * Normalization Form C
     */
    NFC,

    /**
     * Normalization Form D
     */
    NFD,

    /**
     * Normalization Form KC
     */
    NFKC,

    /**
     * Normalization Form KD
     */
    NFKD,
  };

  enum class AlreadyNormalized : bool { No, Yes };

  /**
   * Normalize the input string according to requested normalization form.
   *
   * Returns `AlreadyNormalized::Yes` when the string is already in normalized
   * form. The output buffer is unchanged in this case. Otherwise returns
   * `AlreadyNormalized::No` and places the normalized string into the output
   * buffer.
   */
  template <typename B>
  static Result<AlreadyNormalized, ICUError> Normalize(
      NormalizationForm aForm, Span<const char16_t> aString, B& aBuffer) {
    // The unorm2_getXXXInstance() methods return a shared instance which must
    // not be deleted.
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer;
    switch (aForm) {
      case NormalizationForm::NFC:
        normalizer = unorm2_getNFCInstance(&status);
        break;
      case NormalizationForm::NFD:
        normalizer = unorm2_getNFDInstance(&status);
        break;
      case NormalizationForm::NFKC:
        normalizer = unorm2_getNFKCInstance(&status);
        break;
      case NormalizationForm::NFKD:
        normalizer = unorm2_getNFKDInstance(&status);
        break;
    }
    if (U_FAILURE(status)) {
      return Err(ToICUError(status));
    }

    int32_t spanLengthInt = unorm2_spanQuickCheckYes(normalizer, aString.data(),
                                                     aString.size(), &status);
    if (U_FAILURE(status)) {
      return Err(ToICUError(status));
    }

    size_t spanLength = AssertedCast<size_t>(spanLengthInt);
    MOZ_ASSERT(spanLength <= aString.size());

    // Return if the input string is already normalized.
    if (spanLength == aString.size()) {
      return AlreadyNormalized::Yes;
    }

    if (!aBuffer.reserve(aString.size())) {
      return Err(ICUError::OutOfMemory);
    }

    // Copy the already normalized prefix.
    if (spanLength > 0) {
      PodCopy(aBuffer.data(), aString.data(), spanLength);
    }

    MOZ_TRY(FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          Span<const char16_t> remaining = aString.From(spanLength);
          return unorm2_normalizeSecondAndAppend(normalizer, target, spanLength,
                                                 length, remaining.data(),
                                                 remaining.size(), status);
        }));

    return AlreadyNormalized::No;
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
