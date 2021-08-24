/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberRangeFormat_h_
#define intl_components_NumberRangeFormat_h_

#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/UniquePtr.h"

#include <stdint.h>
#include <string_view>

#include "unicode/utypes.h"

struct UFormattedNumberRange;
struct UNumberRangeFormatter;
struct UPluralRules;

namespace mozilla::intl {

struct NumberFormatOptions;

#ifndef U_HIDE_DRAFT_API
// UNumberRangeFormatter requires ICU draft API. And because we try to reduce
// direct access to ICU definitions, add a separate pre-processor definition to
// guard the access to number range formatting instead of directly using
// U_HIDE_DRAFT_API.
#  define MOZ_INTL_HAS_NUMBER_RANGE_FORMAT
#endif

/**
 * A NumberRangeFormat implementation that roughly mirrors the API provided by
 * the ECMA-402 Intl.NumberFormat object for formatting number ranges.
 *
 * https://tc39.es/ecma402/#numberformat-objects
 */
class NumberRangeFormat final {
 public:
  enum class FormatError {
    InternalError,
    OutOfMemory,
  };

  /**
   * Initialize a new NumberRangeFormat for the provided locale and using the
   * provided options.
   *
   * https://tc39.es/ecma402/#sec-initializenumberformat
   */
  static Result<UniquePtr<NumberRangeFormat>, FormatError> TryCreate(
      std::string_view aLocale, const NumberFormatOptions& aOptions);

  NumberRangeFormat() = default;
  NumberRangeFormat(const NumberRangeFormat&) = delete;
  NumberRangeFormat& operator=(const NumberRangeFormat&) = delete;

#ifdef MOZ_INTL_HAS_NUMBER_RANGE_FORMAT
  ~NumberRangeFormat();

  /**
   * Formats a double range to a utf-16 string. The string view is valid until
   * another number range is formatted. Accessing the string view after this
   * event is undefined behavior.
   *
   * https://tc39.es/ecma402/#sec-formatnumericrange
   */
  Result<std::u16string_view, FormatError> format(double start,
                                                  double end) const {
    if (!formatInternal(start, end)) {
      return Err(FormatError::InternalError);
    }

    return formatResult();
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
  Result<int32_t, FormatError> selectForRange(
      double start, double end, char16_t* keyword, int32_t keywordSize,
      const UPluralRules* pluralRules) const;

 private:
  UNumberRangeFormatter* mNumberRangeFormatter = nullptr;
  UFormattedNumberRange* mFormattedNumberRange = nullptr;

  Result<Ok, FormatError> initialize(std::string_view aLocale,
                                     const NumberFormatOptions& aOptions);

  [[nodiscard]] bool formatInternal(double start, double end) const;

  Result<std::u16string_view, FormatError> formatResult() const;
#endif
};

}  // namespace mozilla::intl

#endif
