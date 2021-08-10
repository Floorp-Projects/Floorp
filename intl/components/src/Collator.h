/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_Collator_h_
#define intl_components_Collator_h_

#ifndef JS_STANDALONE
#  include "gtest/MozGtestFriend.h"
#endif

#include "unicode/ucol.h"

#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"

namespace mozilla::intl {

class Collator final {
 public:
  /**
   * Construct from a raw UCollator. This is public so that the UniquePtr can
   * access it.
   */
  explicit Collator(UCollator* aCollator);

  // Do not allow copy as this class owns the ICU resource. Move is not
  // currently implemented, but a custom move operator could be created if
  // needed.
  Collator(const Collator&) = delete;
  Collator& operator=(const Collator&) = delete;

  /**
   * Attempt to initialize a new collator.
   */
  static Result<UniquePtr<Collator>, ICUError> TryCreate(const char* aLocale);

  ~Collator();

  template <typename B>
  ICUResult GetSortKey(Span<const char16_t> aString, B& aBuffer) const {
    static_assert(std::is_same_v<typename B::CharType, uint8_t>,
                  "Expected a uint8_t* buffer.");
    // Do not use FillBufferWithICUCall, as this API does not report the
    // U_BUFFER_OVERFLOW_ERROR. The return value is always the number of bytes
    // needed, regardless of whether the result buffer was big enough.
    UErrorCode status = U_ZERO_ERROR;
    int32_t length =
        ucol_getSortKey(mCollator.GetConst(), aString.data(),
                        static_cast<int32_t>(aString.size()), nullptr, 0);
    if (U_FAILURE(status) || length == 0) {
      // If the length is 0, and internal error occurred according to the docs.
      return Err(ICUError::InternalError);
    }

    if (!aBuffer.reserve(length)) {
      return Err(ICUError::OutOfMemory);
    }

    length = ucol_getSortKey(mCollator.GetConst(), aString.data(),
                             aString.size(), aBuffer.data(), length);

    if (U_FAILURE(status) || length == 0) {
      return Err(ICUError::InternalError);
    }

    aBuffer.written(length);
    return Ok();
  }

  int32_t CompareStrings(Span<const char16_t> aSource,
                         Span<const char16_t> aTarget) const;

  int32_t CompareSortKeys(Span<const uint8_t> aKey1,
                          Span<const uint8_t> aKey2) const;

  /**
   * Determine how casing affects sorting. These options map to ECMA 402
   * collator options.
   *
   * https://tc39.es/ecma402/#sec-initializecollator
   */
  enum class CaseFirst {
    // Sort upper case first.
    Upper,
    // Sort lower case first.
    Lower,
    // Orders upper and lower case letters in accordance to their tertiary
    // weights.
    False,
  };

  /**
   * Which differences in the strings should lead to differences in collation
   * comparisons.
   *
   * This setting needs to be ECMA 402 compliant.
   * https://tc39.es/ecma402/#sec-collator-comparestrings
   */
  enum class Sensitivity {
    // Only strings that differ in base letters compare as unequal.
    // Examples: a ≠ b, a = á, a = A.
    Base,
    // Only strings that differ in base letters or accents and other diacritic
    // marks compare as unequal.
    // Examples: a ≠ b, a ≠ á, a = A.
    Accent,
    // Only strings that differ in base letters or case compare as unequal.
    // Examples: a ≠ b, a = á, a ≠ A.
    Case,
    // Strings that differ in base letters, accents and other diacritic marks,
    // or case compare as unequal. Other differences may also be taken into
    // consideration.
    // Examples: a ≠ b, a ≠ á, a ≠ A.
    Variant,
  };

  /**
   * These options map to ECMA 402 collator options. Make sure the defaults map
   * to the default initialized values of ECMA 402.
   *
   * https://tc39.es/ecma402/#sec-initializecollator
   */
  struct Options {
    Sensitivity sensitivity = Sensitivity::Variant;
    CaseFirst caseFirst = CaseFirst::False;
    bool ignorePunctuation = false;
    bool numeric = false;
  };

  /**
   * Change the configuraton of the options.
   */
  ICUResult SetOptions(const Options& aOptions,
                       const Maybe<Options&> aPrevOptions = Nothing());

 private:
  /**
   * Toggle features, or use the default setting.
   */
  enum class Feature {
    // Turn the feature off.
    On,
    // Turn the feature off.
    Off,
    // Use the default setting for the feature.
    Default,
  };

  /**
   * Attribute for handling variable elements.
   */
  enum class AlternateHandling {
    // Treats all the codepoints with non-ignorable primary weights in the
    // same way (default)
    NonIgnorable,
    // Causes codepoints with primary weights that are equal or below the
    // variable top value to be ignored on primary level and moved to the
    // quaternary level.
    Shifted,
    Default,
  };

  /**
   * The strength attribute.
   *
   * The usual strength for most locales (except Japanese) is tertiary.
   *
   * Quaternary strength is useful when combined with shifted setting for
   * alternate handling attribute and for JIS X 4061 collation, when it is used
   * to distinguish between Katakana and Hiragana. Otherwise, quaternary level
   * is affected only by the number of non-ignorable code points in the string.
   *
   * Identical strength is rarely useful, as it amounts to codepoints of the NFD
   * form of the string.
   */
  enum class Strength {
    // Primary collation strength.
    Primary,
    // Secondary collation strength.
    Secondary,
    // Tertiary collation strength.
    Tertiary,
    // Quaternary collation strength.
    Quaternary,
    // Identical collation strength.
    Identical,
    Default,
  };

  /**
   * Configure the Collation::Strength
   */
  void SetStrength(Strength strength);

  /**
   * Configure Collation::AlternateHandling.
   */
  ICUResult SetAlternateHandling(AlternateHandling aAlternateHandling);

  /**
   * Controls whether an extra case level (positioned before the third level) is
   * generated or not.
   *
   * Contents of the case level are affected by the value of CaseFirst
   * attribute. A simple way to ignore accent differences in a string is to set
   * the strength to Primary and enable case level.
   */
  ICUResult SetCaseLevel(Feature aFeature);

  /**
   * When turned on, this attribute makes substrings of digits sort according to
   * their numeric values.
   *
   * This is a way to get '100' to sort AFTER '2'. Note that the longest digit
   * substring that can be treated as a single unit is 254 digits (not counting
   * leading zeros). If a digit substring is longer than that, the digits beyond
   * the limit will be treated as a separate digit substring.
   *
   * A "digit" in this sense is a code point with General_Category=Nd, which
   * does not include circled numbers, roman numerals, etc. Only a contiguous
   * digit substring is considered, that is, non-negative integers without
   * separators. There is no support for plus/minus signs, decimals, exponents,
   * etc.
   */
  ICUResult SetNumericCollation(Feature aFeature);

  /**
   * Controls whether the normalization check and necessary normalizations are
   * performed.
   *
   * When off (default), no normalization check is performed. The correctness of
   * the result is guaranteed only if the input data is in so-called FCD form
   * When set to on, an incremental check is performed to see whether the input
   * data is in the FCD form. If the data is not in the FCD form, incremental
   * NFD normalization is performed.
   */
  ICUResult SetNormalizationMode(Feature aFeature);

  /**
   * Configure Collation::CaseFirst.
   */
  ICUResult SetCaseFirst(CaseFirst aCaseFirst);

#ifndef JS_STANDALONE
  FRIEND_TEST(IntlCollator, SetAttributesInternal);
#endif

  ICUPointer<UCollator> mCollator = ICUPointer<UCollator>(nullptr);
  Maybe<Sensitivity> mLastStrategy = Nothing();
};

}  // namespace mozilla::intl

#endif
