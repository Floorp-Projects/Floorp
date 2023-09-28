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
#include "mozilla/Span.h"
#include "mozilla/Try.h"

#include "unicode/uchar.h"
#include "unicode/unorm2.h"
#include "unicode/ustring.h"
#include "unicode/utext.h"
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

      aBuffer.written(spanLength);
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

  /**
   * Return the NFC pairwise composition of the two input characters, if any;
   * returns 0 (which we know is not a composed char!) if none exists.
   */
  static char32_t ComposePairNFC(char32_t a, char32_t b) {
    // unorm2_getNFCInstance returns a static instance that does not have to be
    // released here. If it fails, we just return 0 (no composition) always.
    static UErrorCode status = U_ZERO_ERROR;
    static const UNormalizer2* normalizer = unorm2_getNFCInstance(&status);
    if (U_FAILURE(status)) {
      return 0;
    }
    UChar32 ch = unorm2_composePair(normalizer, static_cast<UChar32>(a),
                                    static_cast<UChar32>(b));
    return ch < 0 ? 0 : static_cast<char32_t>(ch);
  }

  /**
   * Put the "raw" (single-level) canonical decomposition of the input char, if
   * any, into the provided buffer. Canonical decomps are never more than two
   * chars in length (although full normalization may result in longer output
   * due to recursion).
   * Returns the length of the decomposition (0 if none, else 1 or 2).
   */
  static int DecomposeRawNFD(char32_t ab, char32_t decomp[2]) {
    // unorm2_getNFCInstance returns a static instance that does not have to be
    // released here. If it fails, we just return 0 (no decomposition) always.
    // Although we are using it to query for a decomposition, the mode of the
    // Normalizer2 is irrelevant here, so we may as well use the same singleton
    // instance as ComposePairNFC.
    static UErrorCode status = U_ZERO_ERROR;
    static const UNormalizer2* normalizer = unorm2_getNFCInstance(&status);
    if (U_FAILURE(status)) {
      return 0;
    }

    // Canonical decompositions are never more than two Unicode characters,
    // or a maximum of 4 utf-16 code units.
    const unsigned MAX_DECOMP_LENGTH = 4;
    UErrorCode error = U_ZERO_ERROR;
    UChar decompUtf16[MAX_DECOMP_LENGTH];
    int32_t len =
        unorm2_getRawDecomposition(normalizer, static_cast<UChar32>(ab),
                                   decompUtf16, MAX_DECOMP_LENGTH, &error);
    if (U_FAILURE(error) || len < 0) {
      return 0;
    }
    UText text = UTEXT_INITIALIZER;
    utext_openUChars(&text, decompUtf16, len, &error);
    MOZ_ASSERT(U_SUCCESS(error));
    UChar32 ch = UTEXT_NEXT32(&text);
    len = 0;
    if (ch != U_SENTINEL) {
      decomp[0] = static_cast<char32_t>(ch);
      ++len;
      ch = UTEXT_NEXT32(&text);
      if (ch != U_SENTINEL) {
        decomp[1] = static_cast<char32_t>(ch);
        ++len;
      }
    }
    utext_close(&text);
    return len;
  }

  /**
   * Return the Unicode version, for example "13.0".
   */
  static Span<const char> GetUnicodeVersion();
};

}  // namespace mozilla::intl

#endif
