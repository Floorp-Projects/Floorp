/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberFormat_h_
#define intl_components_NumberFormat_h_
#include <string_view>
#include <utility>
#include <vector>

#include "mozilla/FloatingPoint.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Result.h"
#include "mozilla/Utf8.h"
#include "mozilla/Vector.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/intl/NumberPart.h"

#include "unicode/ustring.h"
#include "unicode/unum.h"
#include "unicode/unumberformatter.h"

struct UPluralRules;

namespace mozilla::intl {

struct PluralRulesOptions;

/**
 * Configure NumberFormat options.
 * The supported display styles are:
 *   * Decimal (default)
 *   * Currency (controlled by mCurrency)
 *   * Unit (controlled by mUnit)
 *   * Percent (controlled by mPercent)
 *
 * Only one of mCurrency, mUnit or mPercent should be set. If none are set,
 * the number will formatted as a decimal.
 *
 * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
 */
struct MOZ_STACK_CLASS NumberFormatOptions {
  /**
   * Display a currency amount. |currency| must be a three-letter currency code.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit-width
   */
  enum class CurrencyDisplay {
    Symbol,
    Code,
    Name,
    NarrowSymbol,
  };
  Maybe<std::pair<std::string_view, CurrencyDisplay>> mCurrency;

  /**
   * Set the fraction digits settings. |min| can be zero, |max| must be
   * larger-or-equal to |min|.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#fraction-precision
   */
  Maybe<std::pair<uint32_t, uint32_t>> mFractionDigits;

  /**
   * Set the minimum number of integer digits. |min| must be a non-zero
   * number.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#integer-width
   */
  Maybe<uint32_t> mMinIntegerDigits;

  /**
   * Set the significant digits settings. |min| must be a non-zero number, |max|
   * must be larger-or-equal to |min|.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#significant-digits-precision
   */
  Maybe<std::pair<uint32_t, uint32_t>> mSignificantDigits;

  /**
   * Display a unit amount. |unit| must be a well-formed unit identifier.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#per-unit
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit-width
   */
  enum class UnitDisplay { Short, Narrow, Long };
  Maybe<std::pair<std::string_view, UnitDisplay>> mUnit;

  /**
   * Display a percent number.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#scale
   */
  bool mPercent = false;

  /**
   * Set to true to strip trailing zeros after the decimal point for integer
   * values.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#trailing-zero-display
   */
  bool mStripTrailingZero = false;

  /**
   * Enable or disable grouping.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#grouping
   */
  enum class Grouping {
    Auto,
    Always,
    Min2,
    Never,
  } mGrouping = Grouping::Auto;

  /**
   * Set the notation style.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#notation
   */
  enum class Notation {
    Standard,
    Scientific,
    Engineering,
    CompactShort,
    CompactLong
  } mNotation = Notation::Standard;

  /**
   * Set the sign-display.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#sign-display
   */
  enum class SignDisplay {
    Auto,
    Never,
    Always,
    ExceptZero,
    Negative,
    Accounting,
    AccountingAlways,
    AccountingExceptZero,
    AccountingNegative,
  } mSignDisplay = SignDisplay::Auto;

  /**
   * Set the rounding increment, which must be a non-zero number.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#precision
   */
  uint32_t mRoundingIncrement = 1;

  /**
   * Set the rounding mode.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#rounding-mode
   */
  enum class RoundingMode {
    Ceil,
    Floor,
    Expand,
    Trunc,
    HalfCeil,
    HalfFloor,
    HalfExpand,
    HalfTrunc,
    HalfEven,
    HalfOdd,
  } mRoundingMode = RoundingMode::HalfExpand;

  /**
   * Set the rounding priority. |mFractionDigits| and |mSignificantDigits| must
   * both be set if the rounding priority isn't equal to "auto".
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#fraction-precision
   */
  enum class RoundingPriority {
    Auto,
    MorePrecision,
    LessPrecision,
  } mRoundingPriority = RoundingPriority::Auto;
};

/**
 * According to http://userguide.icu-project.org/design, as long as we constrain
 * ourselves to const APIs ICU is const-correct.
 */

/**
 * A NumberFormat implementation that roughly mirrors the API provided by
 * the ECMA-402 Intl.NumberFormat object.
 *
 * https://tc39.es/ecma402/#numberformat-objects
 */
class NumberFormat final {
 public:
  /**
   * Initialize a new NumberFormat for the provided locale and using the
   * provided options.
   *
   * https://tc39.es/ecma402/#sec-initializenumberformat
   */
  static Result<UniquePtr<NumberFormat>, ICUError> TryCreate(
      std::string_view aLocale, const NumberFormatOptions& aOptions);

  NumberFormat() = default;
  NumberFormat(const NumberFormat&) = delete;
  NumberFormat& operator=(const NumberFormat&) = delete;
  ~NumberFormat();

  /**
   * Formats a double to a utf-16 string. The string view is valid until
   * another number is formatted. Accessing the string view after this event
   * is undefined behavior.
   *
   * https://tc39.es/ecma402/#sec-formatnumberstring
   */
  Result<std::u16string_view, ICUError> format(double number) const {
    if (!formatInternal(number)) {
      return Err(ICUError::InternalError);
    }

    return formatResult();
  }

  /**
   * Formats a double to a utf-16 string, and fills the provided parts vector.
   * The string view is valid until another number is formatted. Accessing the
   * string view after this event is undefined behavior.
   *
   * This is utf-16 only because the only current use case is in
   * SpiderMonkey. Supporting utf-8 would require recalculating the offsets
   * in NumberPartVector from fixed width to variable width, which might be
   * tricky to get right and is work that won't be necessary if we switch to
   * ICU4X (see Bug 1707035).
   *
   * https://tc39.es/ecma402/#sec-partitionnumberpattern
   */
  Result<std::u16string_view, ICUError> formatToParts(
      double number, NumberPartVector& parts) const;

  /**
   * Formats a double to the provider buffer (either utf-8 or utf-16)
   *
   * https://tc39.es/ecma402/#sec-formatnumberstring
   */
  template <typename B>
  Result<Ok, ICUError> format(double number, B& buffer) const {
    if (!formatInternal(number)) {
      return Err(ICUError::InternalError);
    }

    return formatResult<typename B::CharType, B>(buffer);
  }

  /**
   * Formats an int64_t to a utf-16 string. The string view is valid until
   * another number is formatted. Accessing the string view after this event is
   * undefined behavior.
   *
   * https://tc39.es/ecma402/#sec-formatnumberstring
   */
  Result<std::u16string_view, ICUError> format(int64_t number) const {
    if (!formatInternal(number)) {
      return Err(ICUError::InternalError);
    }

    return formatResult();
  }

  /**
   * Formats a int64_t to a utf-16 string, and fills the provided parts vector.
   * The string view is valid until another number is formatted. Accessing the
   * string view after this event is undefined behavior.
   *
   * This is utf-16 only because the only current use case is in
   * SpiderMonkey. Supporting utf-8 would require recalculating the offsets
   * in NumberPartVector from fixed width to variable width, which might be
   * tricky to get right and is work that won't be necessary if we switch to
   * ICU4X (see Bug 1707035).
   *
   * https://tc39.es/ecma402/#sec-partitionnumberpattern
   */
  Result<std::u16string_view, ICUError> formatToParts(
      int64_t number, NumberPartVector& parts) const;

  /**
   * Formats an int64_t to the provider buffer (either utf-8 or utf-16).
   *
   * https://tc39.es/ecma402/#sec-formatnumberstring
   */
  template <typename B>
  Result<Ok, ICUError> format(int64_t number, B& buffer) const {
    if (!formatInternal(number)) {
      return Err(ICUError::InternalError);
    }

    return formatResult<typename B::CharType, B>(buffer);
  }

  /**
   * Formats a string encoded decimal number to a utf-16 string. The string view
   * is valid until another number is formatted. Accessing the string view
   * after this event is undefined behavior.
   *
   * https://tc39.es/ecma402/#sec-formatnumberstring
   */
  Result<std::u16string_view, ICUError> format(std::string_view number) const {
    if (!formatInternal(number)) {
      return Err(ICUError::InternalError);
    }

    return formatResult();
  }

  /**
   * Formats a string encoded decimal number to a utf-16 string, and fills the
   * provided parts vector. The string view is valid until another number is
   * formatted. Accessing the string view after this event is undefined
   * behavior.
   *
   * This is utf-16 only because the only current use case is in
   * SpiderMonkey. Supporting utf-8 would require recalculating the offsets
   * in NumberPartVector from fixed width to variable width, which might be
   * tricky to get right and is work that won't be necessary if we switch to
   * ICU4X (see Bug 1707035).
   *
   * https://tc39.es/ecma402/#sec-partitionnumberpattern
   */
  Result<std::u16string_view, ICUError> formatToParts(
      std::string_view number, NumberPartVector& parts) const;

  /**
   * Formats a string encoded decimal number to the provider buffer
   * (either utf-8 or utf-16).
   *
   * https://tc39.es/ecma402/#sec-formatnumberstring
   */
  template <typename B>
  Result<Ok, ICUError> format(std::string_view number, B& buffer) const {
    if (!formatInternal(number)) {
      return Err(ICUError::InternalError);
    }

    return formatResult<typename B::CharType, B>(buffer);
  }

  /**
   * Formats the number and selects the keyword by using a provided
   * UPluralRules object.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.select
   *
   * TODO(1713917) This is necessary because both PluralRules and
   * NumberFormat have a shared dependency on the raw UFormattedNumber
   * type. Once we transition to using ICU4X, the FFI calls should no
   * longer require such shared dependencies. At that time, this
   * functionality should be removed from NumberFormat and invoked
   * solely from PluralRules.
   */
  Result<int32_t, ICUError> selectFormatted(double number, char16_t* keyword,
                                            int32_t keywordSize,
                                            UPluralRules* pluralRules) const;

  /**
   * Returns an iterator over all supported number formatter locales.
   *
   * The returned strings are ICU locale identifiers and NOT BCP 47 language
   * tags.
   *
   * Also see <https://unicode-org.github.io/icu/userguide/locale>.
   */
  static auto GetAvailableLocales() {
    return AvailableLocalesEnumeration<unum_countAvailable,
                                       unum_getAvailable>();
  }

 private:
  UNumberFormatter* mNumberFormatter = nullptr;
  UFormattedNumber* mFormattedNumber = nullptr;
  bool mFormatForUnit = false;

  Result<Ok, ICUError> initialize(std::string_view aLocale,
                                  const NumberFormatOptions& aOptions);

  [[nodiscard]] bool formatInternal(double number) const;
  [[nodiscard]] bool formatInternal(int64_t number) const;
  [[nodiscard]] bool formatInternal(std::string_view number) const;

  Result<std::u16string_view, ICUError> formatResult() const;

  template <typename C, typename B>
  Result<Ok, ICUError> formatResult(B& buffer) const {
    // We only support buffers with char or char16_t.
    static_assert(std::is_same_v<C, char> || std::is_same_v<C, char16_t>);

    return formatResult().andThen(
        [&buffer](std::u16string_view result) -> Result<Ok, ICUError> {
          if constexpr (std::is_same_v<C, char>) {
            if (!FillBuffer(Span(result.data(), result.size()), buffer)) {
              return Err(ICUError::OutOfMemory);
            }
            return Ok();
          } else {
            // ICU provides APIs which accept a buffer, but they just copy from
            // an internal buffer behind the scenes anyway.
            if (!buffer.reserve(result.size())) {
              return Err(ICUError::OutOfMemory);
            }
            PodCopy(static_cast<char16_t*>(buffer.data()), result.data(),
                    result.size());
            buffer.written(result.size());

            return Ok();
          }
        });
  }
};

}  // namespace mozilla::intl

#endif
