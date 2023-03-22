/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberRangeFormat_h_
#define intl_components_NumberRangeFormat_h_

#include "mozilla/FloatingPoint.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/intl/NumberFormat.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"

#include <stdint.h>
#include <string_view>

#include "unicode/utypes.h"

struct UFormattedNumberRange;
struct UNumberRangeFormatter;
struct UPluralRules;

namespace mozilla::intl {

/**
 * NumberRangeFormatOptions supports the same set of options as
 * NumberFormatOptions and additionally allows to control how to display ranges.
 */
struct MOZ_STACK_CLASS NumberRangeFormatOptions : public NumberFormatOptions {
  /**
   * Controls if and how to collapse identical parts in a range.
   */
  enum class RangeCollapse {
    /**
     * Apply locale-specific heuristics.
     */
    Auto,

    /**
     * Never collapse identical parts.
     */
    None,

    /**
     * Collapse identical unit parts.
     */
    Unit,

    /**
     * Collapse all identical parts.
     */
    All,
  } mRangeCollapse = RangeCollapse::Auto;

  /**
   * Controls how to display identical numbers.
   */
  enum class RangeIdentityFallback {
    /**
     * Display the range as a single value.
     */
    SingleValue,

    /**
     * Display the range as a single value if both numbers were equal before
     * rounding. Otherwise display with a locale-sensitive approximation
     * pattern.
     */
    ApproximatelyOrSingleValue,

    /**
     * Display with a locale-sensitive approximation pattern.
     */
    Approximately,

    /**
     * Display as a range expression.
     */
    Range,
  } mRangeIdentityFallback = RangeIdentityFallback::SingleValue;
};

/**
 * A NumberRangeFormat implementation that roughly mirrors the API provided by
 * the ECMA-402 Intl.NumberFormat object for formatting number ranges.
 *
 * https://tc39.es/ecma402/#numberformat-objects
 */
class NumberRangeFormat final {
 public:
  /**
   * Initialize a new NumberRangeFormat for the provided locale and using the
   * provided options.
   *
   * https://tc39.es/ecma402/#sec-initializenumberformat
   */
  static Result<UniquePtr<NumberRangeFormat>, ICUError> TryCreate(
      std::string_view aLocale, const NumberRangeFormatOptions& aOptions);

  NumberRangeFormat() = default;
  NumberRangeFormat(const NumberRangeFormat&) = delete;
  NumberRangeFormat& operator=(const NumberRangeFormat&) = delete;

  ~NumberRangeFormat();

  /**
   * Formats a double range to a utf-16 string. The string view is valid until
   * another number range is formatted. Accessing the string view after this
   * event is undefined behavior.
   *
   * https://tc39.es/ecma402/#sec-formatnumericrange
   */
  Result<std::u16string_view, ICUError> format(double start, double end) const {
    if (!formatInternal(start, end)) {
      return Err(ICUError::InternalError);
    }

    return formatResult();
  }

  /**
   * Formats a double range to a utf-16 string, and fills the provided parts
   * vector. The string view is valid until another number is formatted.
   * Accessing the string view after this event is undefined behavior.
   *
   * https://tc39.es/ecma402/#sec-partitionnumberrangepattern
   */
  Result<std::u16string_view, ICUError> formatToParts(
      double start, double end, NumberPartVector& parts) const {
    if (!formatInternal(start, end)) {
      return Err(ICUError::InternalError);
    }

    bool isNegativeStart = !std::isnan(start) && IsNegative(start);
    bool isNegativeEnd = !std::isnan(end) && IsNegative(end);

    return formatResultToParts(Some(start), isNegativeStart, Some(end),
                               isNegativeEnd, parts);
  }

  /**
   * Formats a decimal number range to a utf-16 string. The string view is valid
   * until another number range is formatted. Accessing the string view after
   * this event is undefined behavior.
   *
   * https://tc39.es/ecma402/#sec-formatnumericrange
   */
  Result<std::u16string_view, ICUError> format(std::string_view start,
                                               std::string_view end) const {
    if (!formatInternal(start, end)) {
      return Err(ICUError::InternalError);
    }

    return formatResult();
  }

  /**
   * Formats a string encoded decimal number range to a utf-16 string, and fills
   * the provided parts vector. The string view is valid until another number is
   * formatted. Accessing the string view after this event is undefined
   * behavior.
   *
   * https://tc39.es/ecma402/#sec-partitionnumberrangepattern
   */
  Result<std::u16string_view, ICUError> formatToParts(
      std::string_view start, std::string_view end,
      NumberPartVector& parts) const {
    if (!formatInternal(start, end)) {
      return Err(ICUError::InternalError);
    }

    Maybe<double> numStart = Nothing();
    if (start == "Infinity" || start == "+Infinity") {
      numStart.emplace(PositiveInfinity<double>());
    } else if (start == "-Infinity") {
      numStart.emplace(NegativeInfinity<double>());
    } else {
      // Not currently expected, so we assert here.
      MOZ_ASSERT(start != "NaN");
    }

    Maybe<double> numEnd = Nothing();
    if (end == "Infinity" || end == "+Infinity") {
      numEnd.emplace(PositiveInfinity<double>());
    } else if (end == "-Infinity") {
      numEnd.emplace(NegativeInfinity<double>());
    } else {
      // Not currently expected, so we assert here.
      MOZ_ASSERT(end != "NaN");
    }

    bool isNegativeStart = !start.empty() && start[0] == '-';
    bool isNegativeEnd = !end.empty() && end[0] == '-';

    return formatResultToParts(numStart, isNegativeStart, numEnd, isNegativeEnd,
                               parts);
  }

  /**
   * Formats the number range and selects the keyword by using a provided
   * UPluralRules object.
   *
   * https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.selectrange
   *
   * TODO(1713917) This is necessary because both PluralRules and
   * NumberRangeFormat have a shared dependency on the raw UFormattedNumberRange
   * type. Once we transition to using ICU4X, the FFI calls should no
   * longer require such shared dependencies. At that time, this
   * functionality should be removed from NumberRangeFormat and invoked
   * solely from PluralRules.
   */
  Result<int32_t, ICUError> selectForRange(
      double start, double end, char16_t* keyword, int32_t keywordSize,
      const UPluralRules* pluralRules) const;

 private:
  UNumberRangeFormatter* mNumberRangeFormatter = nullptr;
  UFormattedNumberRange* mFormattedNumberRange = nullptr;
  bool mFormatForUnit = false;

  Result<Ok, ICUError> initialize(std::string_view aLocale,
                                  const NumberRangeFormatOptions& aOptions);

  [[nodiscard]] bool formatInternal(double start, double end) const;

  [[nodiscard]] bool formatInternal(std::string_view start,
                                    std::string_view end) const;

  Result<std::u16string_view, ICUError> formatResult() const;

  Result<std::u16string_view, ICUError> formatResultToParts(
      Maybe<double> start, bool startIsNegative, Maybe<double> end,
      bool endIsNegative, NumberPartVector& parts) const;
};

}  // namespace mozilla::intl

#endif
