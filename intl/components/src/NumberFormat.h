/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberFormat_h_
#define intl_components_NumberFormat_h_
#include <string_view>
#include <utility>
#include <vector>

#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Utf8.h"
#include "mozilla/Vector.h"

#include "unicode/ustring.h"
#include "unicode/unum.h"
#include "unicode/unumberformatter.h"

namespace mozilla {
namespace intl {

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
  enum class CurrencyDisplayStyle {
    Symbol,
    Code,
    Name,
    NarrowSymbol,
  };
  Maybe<std::pair<std::string_view, CurrencyDisplayStyle>> mCurrency;

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
   * Enable or disable grouping.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#grouping
   */
  bool mUseGrouping = true;

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
    Accounting,
    AccountingAlways,
    AccountingExceptZero
  } mSignDisplay = SignDisplay::Auto;

  /**
   * Set the rounding mode to 'half-up'.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#rounding-mode
   */
  bool mRoundingModeHalfUp = true;
};

/**
 * According to http://userguide.icu-project.org/design, as long as we constrain
 * ourselves to const APIs ICU is const-correct.
 */

class NumberFormat final {
 public:
  explicit NumberFormat(std::string_view aLocale,
                        const NumberFormatOptions& aOptions = {});

  ~NumberFormat();

  const char16_t* format(double number) const {
    if (!mIsInitialized || !formatInternal(number)) {
      return nullptr;
    }

    return formatResult().data();
  }

  template <typename B>
  bool format(double number, B& buffer) const {
    if (!mIsInitialized || !formatInternal(number)) {
      return false;
    }

    return formatResult<typename B::CharType, B>(buffer);
  }

  const char16_t* format(int64_t number) const {
    if (!mIsInitialized || !formatInternal(number)) {
      return nullptr;
    }

    return formatResult().data();
  }

  template <typename B>
  bool format(int64_t number, B& buffer) const {
    if (!mIsInitialized || !formatInternal(number)) {
      return false;
    }

    return formatResult<typename B::CharType, B>(buffer);
  }

  const char16_t* format(std::string_view number) const {
    if (!mIsInitialized || !formatInternal(number)) {
      return nullptr;
    }

    return formatResult().data();
  }

  template <typename B>
  bool format(std::string_view number, B& buffer) const {
    if (!mIsInitialized || !formatInternal(number)) {
      return false;
    }

    return formatResult<typename B::CharType, B>(buffer);
  }

 private:
  UNumberFormatter* mNumberFormatter = nullptr;
  UFormattedNumber* mFormattedNumber = nullptr;
  bool mIsInitialized = false;

  bool formatInternal(double number) const;
  bool formatInternal(int64_t number) const;
  bool formatInternal(std::string_view number) const;
  std::u16string_view formatResult() const;

  template <typename C, typename B>
  bool formatResult(B& buffer) const {
    std::u16string_view result = formatResult();

    if (result.empty()) {
      return false;
    }

    if constexpr (std::is_same<C, uint8_t>::value) {
      // Reserve 3 * the UTF-16 length to guarantee enough space for the UTF-8
      // result.
      if (!buffer.allocate(3 * result.size())) {
        return false;
      }
      size_t amount = ConvertUtf16toUtf8(
          Span(result.data(), result.size()),
          Span(static_cast<char*>(std::data(buffer)), std::size(buffer)));
      buffer.written(amount);
    } else {
      // ICU provides APIs which accept a buffer, but they just copy from an
      // internal buffer behind the scenes anyway.
      if (!buffer.allocate(result.size())) {
        return false;
      }
      PodCopy(static_cast<char16_t*>(buffer.data()), result.data(),
              result.size());
      buffer.written(result.size());
    }

    return true;
  }
};

}  // namespace intl
}  // namespace mozilla

#endif
