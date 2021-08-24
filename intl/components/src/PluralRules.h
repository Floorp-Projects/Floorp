/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_PluralRules_h_
#define intl_components_PluralRules_h_

#include <string_view>
#include <type_traits>
#include <utility>

#include "mozilla/intl/NumberFormat.h"
#include "mozilla/intl/NumberRangeFormat.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"

#include "unicode/utypes.h"

namespace mozilla {
namespace intl {

#ifndef U_HIDE_DRAFT_API
// SelectRange() requires ICU draft API. And because we try to reduce direct
// access to ICU definitions, add a separate pre-processor definition to guard
// the access to SelectRange() instead of directly using U_HIDE_DRAFT_API.
#  define MOZ_INTL_PLURAL_RULES_HAS_SELECT_RANGE
#endif

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

  /**
   * PluralRules error types.
   */
  enum class Error : uint8_t {
    FormatError,
    InternalError,
    OutOfMemory,
  };

  PluralRules(const PluralRules&) = delete;
  PluralRules& operator=(const PluralRules&) = delete;

  /**
   * Attempts to construct a PluralRules with the given locale and options.
   */
  // TODO(1709880) use mozilla::Span instead of std::string_view.
  static Result<UniquePtr<PluralRules>, PluralRules::Error> TryCreate(
      std::string_view aLocale, const PluralRulesOptions& aOptions);

  /**
   * Returns the PluralRules keyword that corresponds to the |aNumber|.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.select
   */
  Result<PluralRules::Keyword, PluralRules::Error> Select(double aNumber) const;

#ifdef MOZ_INTL_PLURAL_RULES_HAS_SELECT_RANGE
  /**
   * Returns the PluralRules keyword that corresponds to the range from |aStart|
   * to |aEnd|.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.selectrange
   */
  Result<PluralRules::Keyword, PluralRules::Error> SelectRange(
      double aStart, double aEnd) const;
#endif

  /**
   * Returns an EnumSet with the plural-rules categories that are supported by
   * the locale that the PluralRules instance was created with.
   */
  Result<EnumSet<PluralRules::Keyword>, PluralRules::Error> Categories() const;

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
   * Set the rounding priority. |mFractionDigits| and |mSignificantDigits| must
   * both be set if the rounding priority isn't equal to "auto".
   */
  enum class RoundingPriority {
    Auto,
    MorePrecision,
    LessPrecision,
  } mRoundingPriority = RoundingPriority::Auto;

  // Must be compatible with NumberFormatOptions::RoundingPriority.
  static_assert(std::is_same_v<
                std::underlying_type_t<RoundingPriority>,
                std::underlying_type_t<NumberFormatOptions::RoundingPriority>>);
  static_assert(RoundingPriority::Auto ==
                RoundingPriority(NumberFormatOptions::RoundingPriority::Auto));
  static_assert(
      RoundingPriority::LessPrecision ==
      RoundingPriority(NumberFormatOptions::RoundingPriority::LessPrecision));
  static_assert(
      RoundingPriority::MorePrecision ==
      RoundingPriority(NumberFormatOptions::RoundingPriority::MorePrecision));
};

}  // namespace intl
}  // namespace mozilla

#endif
