/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_Collator_h_
#define intl_components_Collator_h_

#ifndef JS_STANDALONE
#  include "gtest/MozGtestFriend.h"
#endif

#include "unicode/ucol.h"

#include "mozilla/Compiler.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/ICUError.h"
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

  /**
   * Get a sort key with the provided UTF-16 string, and store the sort key into
   * the provided buffer of byte array.
   * Every sort key ends with 0x00, and the terminating 0x00 byte is counted
   * when calculating the length of buffer. For the purpose of other byte
   * values, check the "Special Byte Values" document from ICU.
   *
   * https://icu.unicode.org/design/collation/bytes
   */
  template <typename B>
  ICUResult GetSortKey(Span<const char16_t> aString, B& aBuffer) const {
    return FillBufferWithICUCall(
        aBuffer,
        [this, aString](uint8_t* target, int32_t length, UErrorCode* status) {
          // ucol_getSortKey doesn't use the error code to report
          // U_BUFFER_OVERFLOW_ERROR, instead it uses the return value to
          // indicate the desired length to store the key. So we update the
          // UErrorCode accordingly to let FillBufferWithICUCall resize the
          // buffer.
          int32_t len = ucol_getSortKey(mCollator.GetConst(), aString.data(),
                                        static_cast<int32_t>(aString.size()),
                                        target, length);
          if (len == 0) {
            // Returns 0 means there's an internal error.
            *status = U_INTERNAL_PROGRAM_ERROR;
          } else if (len > length) {
            *status = U_BUFFER_OVERFLOW_ERROR;
          } else {
            *status = U_ZERO_ERROR;
          }
          return len;
        });
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

  /**
   * Return the case first option of this collator.
   */
  Result<CaseFirst, ICUError> GetCaseFirst() const;

  /**
   * Return the "ignores punctuation" option of this collator.
   */
  Result<bool, ICUError> GetIgnorePunctuation() const;

  /**
   * Map keywords to their BCP 47 equivalents.
   */
  static SpanResult<char> KeywordValueToBcp47Extension(const char* aKeyword,
                                                       int32_t aLength);

  enum class CommonlyUsed : bool {
    /**
     * Select all possible values, even when not commonly used by a locale.
     */
    No,

    /**
     * Only select the values which are commonly used by a locale.
     */
    Yes,
  };

  using Bcp47ExtEnumeration =
      Enumeration<char, SpanResult<char>,
                  Collator::KeywordValueToBcp47Extension>;

  /**
   * Returns an iterator of collator locale extensions in the preferred order.
   * These extensions can be used in BCP 47 locales. For instance this
   * iterator could return "phonebk" and could be appled to the German locale
   * "de" as "de-co-phonebk" for a phonebook-style collation.
   *
   * The collation extensions can be found here:
   * http://cldr.unicode.org/core-spec/#Key_Type_Definitions
   */
  static Result<Bcp47ExtEnumeration, ICUError> GetBcp47KeywordValuesForLocale(
      const char* aLocale, CommonlyUsed aCommonlyUsed = CommonlyUsed::No);

  /**
   * Returns an iterator over all possible collator locale extensions.
   * These extensions can be used in BCP 47 locales. For instance this
   * iterator could return "phonebk" and could be appled to the German locale
   * "de" as "de-co-phonebk" for a phonebook-style collation.
   *
   * The collation extensions can be found here:
   * http://cldr.unicode.org/core-spec/#Key_Type_Definitions
   */
  static Result<Bcp47ExtEnumeration, ICUError> GetBcp47KeywordValues();

  /**
   * Returns an iterator over all supported collator locales.
   *
   * The returned strings are ICU locale identifiers and NOT BCP 47 language
   * tags.
   *
   * Also see <https://unicode-org.github.io/icu/userguide/locale>.
   */
  static auto GetAvailableLocales() {
    return AvailableLocalesEnumeration<ucol_countAvailable,
                                       ucol_getAvailable>();
  }

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

  static constexpr auto ToUColAttributeValue(Feature aFeature) {
    switch (aFeature) {
      case Collator::Feature::On:
        return UCOL_ON;
      case Collator::Feature::Off:
        return UCOL_OFF;
      case Collator::Feature::Default:
        return UCOL_DEFAULT;
    }
#if MOZ_IS_GCC
#  if !MOZ_GCC_VERSION_AT_LEAST(9, 1, 0)
    return UCOL_DEFAULT;
#  else
    MOZ_CRASH("invalid collator feature");
#  endif
#else
    MOZ_CRASH("invalid collator feature");
#endif
  }

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
