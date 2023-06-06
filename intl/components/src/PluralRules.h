/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_PluralRules_h_
#define intl_components_PluralRules_h_

#include <string_view>
#include <utility>

#include "mozilla/intl/ICUError.h"
#include "mozilla/intl/NumberFormat.h"
#include "mozilla/intl/NumberRangeFormat.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"

#include "unicode/utypes.h"

namespace mozilla::intl {

class PluralRules final {
 public:
  /**
   * The set of keywords that a PluralRules object uses.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
   */
  enum class Keyword : uint8_t {
    Few,
    Many,
    One,
    Other,
    Two,
    Zero,
  };

  /**
   * The two different types of PluralRules objects that can be created.
   *
   * https://tc39.es/ecma402/#sec-properties-of-intl-pluralrules-instances
   */
  enum class Type : uint8_t {
    Cardinal,
    Ordinal,
  };

  PluralRules(const PluralRules&) = delete;
  PluralRules& operator=(const PluralRules&) = delete;

  /**
   * Attempts to construct a PluralRules with the given locale and options.
   */
  // TODO(1709880) use mozilla::Span instead of std::string_view.
  static Result<UniquePtr<PluralRules>, ICUError> TryCreate(
      std::string_view aLocale, const PluralRulesOptions& aOptions);

  /**
   * Returns the PluralRules keyword that corresponds to the |aNumber|.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.select
   */
  Result<PluralRules::Keyword, ICUError> Select(double aNumber) const;

  /**
   * Returns the PluralRules keyword that corresponds to the range from |aStart|
   * to |aEnd|.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.selectrange
   */
  Result<PluralRules::Keyword, ICUError> SelectRange(double aStart,
                                                     double aEnd) const;

  /**
   * Returns an EnumSet with the plural-rules categories that are supported by
   * the locale that the PluralRules instance was created with.
   */
  Result<EnumSet<PluralRules::Keyword>, ICUError> Categories() const;

  ~PluralRules();

 private:
  // The longest keyword is "other"
  static const size_t MAX_KEYWORD_LENGTH = 5;

  UPluralRules* mPluralRules = nullptr;
  UniquePtr<NumberFormat> mNumberFormat;
  UniquePtr<NumberRangeFormat> mNumberRangeFormat;

  PluralRules(UPluralRules*&, UniquePtr<NumberFormat>&&,
              UniquePtr<NumberRangeFormat>&&);

  /**
   * Returns the PluralRules::Keyword that matches the UTF-16 string.
   * Strings must be [u"few", u"many", u"one", u"other", u"two", u"zero"]
   */
  static PluralRules::Keyword KeywordFromUtf16(Span<const char16_t> aKeyword);

  /**
   * Returns the PluralRules::Keyword that matches the ASCII string.
   * Strings must be ["few", "many", "one", "other", "two", "zero"]
   */
  static PluralRules::Keyword KeywordFromAscii(Span<const char> aKeyword);
};

/**
 * Options required for constructing a PluralRules object.
 */
struct MOZ_STACK_CLASS PluralRulesOptions {
  /**
   * Creates a NumberFormatOptions from the PluralRulesOptions.
   */
  NumberFormatOptions ToNumberFormatOptions() const {
    NumberFormatOptions options;
    options.mRoundingMode = NumberFormatOptions::RoundingMode::HalfExpand;

    if (mFractionDigits.isSome()) {
      options.mFractionDigits.emplace(mFractionDigits.ref());
    }

    if (mMinIntegerDigits.isSome()) {
      options.mMinIntegerDigits.emplace(mMinIntegerDigits.ref());
    }

    if (mSignificantDigits.isSome()) {
      options.mSignificantDigits.emplace(mSignificantDigits.ref());
    }

    options.mStripTrailingZero = mStripTrailingZero;

    options.mRoundingIncrement = mRoundingIncrement;

    options.mRoundingMode = NumberFormatOptions::RoundingMode(mRoundingMode);

    options.mRoundingPriority =
        NumberFormatOptions::RoundingPriority(mRoundingPriority);

    return options;
  }
  /**
   * Creates a NumberFormatOptions from the PluralRulesOptions.
   */
  NumberRangeFormatOptions ToNumberRangeFormatOptions() const {
    NumberRangeFormatOptions options;
    options.mRoundingMode = NumberRangeFormatOptions::RoundingMode::HalfExpand;
    options.mRangeCollapse = NumberRangeFormatOptions::RangeCollapse::None;
    options.mRangeIdentityFallback =
        NumberRangeFormatOptions::RangeIdentityFallback::Range;

    if (mFractionDigits.isSome()) {
      options.mFractionDigits.emplace(mFractionDigits.ref());
    }

    if (mMinIntegerDigits.isSome()) {
      options.mMinIntegerDigits.emplace(mMinIntegerDigits.ref());
    }

    if (mSignificantDigits.isSome()) {
      options.mSignificantDigits.emplace(mSignificantDigits.ref());
    }

    options.mStripTrailingZero = mStripTrailingZero;

    options.mRoundingIncrement = mRoundingIncrement;

    options.mRoundingMode = NumberFormatOptions::RoundingMode(mRoundingMode);

    options.mRoundingPriority =
        NumberFormatOptions::RoundingPriority(mRoundingPriority);

    return options;
  }

  /**
   * Set the plural type between cardinal and ordinal.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
   */
  PluralRules::Type mPluralType = PluralRules::Type::Cardinal;

  /**
   * Set the minimum number of integer digits. |min| must be a non-zero
   * number.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
   */
  Maybe<uint32_t> mMinIntegerDigits;

  /**
   * Set the fraction digits settings. |min| can be zero, |max| must be
   * larger-or-equal to |min|.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
   */
  Maybe<std::pair<uint32_t, uint32_t>> mFractionDigits;

  /**
   * Set the significant digits settings. |min| must be a non-zero number, |max|
   * must be larger-or-equal to |min|.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
   */
  Maybe<std::pair<uint32_t, uint32_t>> mSignificantDigits;

  /**
   * Set to true to strip trailing zeros after the decimal point for integer
   * values.
   */
  bool mStripTrailingZero = false;

  /**
   * Set the rounding increment, which must be a non-zero number.
   */
  uint32_t mRoundingIncrement = 1;

  /**
   * Set the rounding mode.
   */
  using RoundingMode = NumberFormatOptions::RoundingMode;
  RoundingMode mRoundingMode = RoundingMode::HalfExpand;

  /**
   * Set the rounding priority. |mFractionDigits| and |mSignificantDigits| must
   * both be set if the rounding priority isn't equal to "auto".
   */
  using RoundingPriority = NumberFormatOptions::RoundingPriority;
  RoundingPriority mRoundingPriority = RoundingPriority::Auto;
};

}  // namespace mozilla::intl

#endif
