/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_DateTimeFormat_h_
#define intl_components_DateTimeFormat_h_
#include "unicode/udat.h"

#include "mozilla/Assertions.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Utf8.h"
#include "mozilla/Vector.h"

namespace mozilla::intl {

enum class DateTimeStyle { Full, Long, Medium, Short, None };

class Calendar;

/**
 * This component is a Mozilla-focused API for the date formatting provided by
 * ICU. The methods internally call out to ICU4C. This is responsible for and
 * owns any resources opened through ICU, through RAII.
 *
 * The construction of a DateTimeFormat contains the majority of the cost
 * of the DateTimeFormat operation. DateTimeFormat::TryFormat should be
 * relatively inexpensive after the initial construction.
 *
 * This class supports creating from Styles (a fixed set of options), and from
 * Skeletons (a list of fields and field widths to include).
 *
 * This API will also serve to back the ECMA-402 Intl.DateTimeFormat API.
 * See Bug 1709473.
 * https://tc39.es/ecma402/#datetimeformat-objects
 *
 * Intl.DateTimeFormat and ICU skeletons and patterns
 * ==================================================
 *
 * Different locales have different ways to display dates using the same
 * basic components. For example, en-US might use "Sept. 24, 2012" while
 * fr-FR might use "24 Sept. 2012". The intent of Intl.DateTimeFormat is to
 * permit production of a format for the locale that best matches the
 * set of date-time components and their desired representation as specified
 * by the API client.
 *
 * ICU supports specification of date and time formats in three ways:
 *
 * 1) A style is just one of the identifiers FULL, LONG, MEDIUM, or SHORT.
 *    The date-time components included in each style and their representation
 *    are defined by ICU using CLDR locale data (CLDR is the Unicode
 *    Consortium's Common Locale Data Repository).
 *
 * 2) A skeleton is a string specifying which date-time components to include,
 *    and which representations to use for them. For example, "yyyyMMMMdd"
 *    specifies a year with at least four digits, a full month name, and a
 *    two-digit day. It does not specify in which order the components appear,
 *    how they are separated, the localized strings for textual components
 *    (such as weekday or month), whether the month is in format or
 *    stand-alone form¹, or the numbering system used for numeric components.
 *    All that information is filled in by ICU using CLDR locale data.
 *    ¹ The format form is the one used in formatted strings that include a
 *    day; the stand-alone form is used when not including days, e.g., in
 *    calendar headers. The two forms differ at least in some Slavic languages,
 *    e.g. Russian: "22 марта 2013 г." vs. "Март 2013".
 *
 * 3) A pattern is a string specifying which date-time components to include,
 *    in which order, with which separators, in which grammatical case. For
 *    example, "EEEE, d MMMM y" specifies the full localized weekday name,
 *    followed by comma and space, followed by the day, followed by space,
 *    followed by the full month name in format form, followed by space,
 *    followed by the full year. It
 *    still does not specify localized strings for textual components and the
 *    numbering system - these are determined by ICU using CLDR locale data or
 *    possibly API parameters.
 *
 * All actual formatting in ICU is done with patterns; styles and skeletons
 * have to be mapped to patterns before processing.
 *
 * The options of DateTimeFormat most closely correspond to ICU skeletons. This
 * implementation therefore, in the toBestICUPattern function, converts
 * DateTimeFormat options to ICU skeletons, and then lets ICU map skeletons to
 * actual ICU patterns. The pattern may not directly correspond to what the
 * skeleton requests, as the mapper (UDateTimePatternGenerator) is constrained
 * by the available locale data for the locale. The resulting ICU pattern is
 * kept as the DateTimeFormat's [[pattern]] internal property and passed to ICU
 * in the format method.
 *
 * An ICU pattern represents the information of the following DateTimeFormat
 * internal properties described in the specification, which therefore don't
 * exist separately in the implementation:
 * - [[weekday]], [[era]], [[year]], [[month]], [[day]], [[hour]], [[minute]],
 *   [[second]], [[timeZoneName]]
 * - [[hour12]]
 * - [[hourCycle]]
 * - [[hourNo0]]
 * When needed for the resolvedOptions method, the resolveICUPattern function
 * maps the instance's ICU pattern back to the specified properties of the
 * object returned by resolvedOptions.
 *
 * ICU date-time skeletons and patterns aren't fully documented in the ICU
 * documentation (see http://bugs.icu-project.org/trac/ticket/9627). The best
 * documentation at this point is in UTR 35:
 * http://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns
 */
class DateTimeFormat final {
 public:
  // Do not allow copy as this class owns the ICU resource. Move is not
  // currently implemented, but a custom move operator could be created if
  // needed.
  DateTimeFormat(const DateTimeFormat&) = delete;
  DateTimeFormat& operator=(const DateTimeFormat&) = delete;

  /**
   * Create a DateTimeFormat from styles.
   *
   * The "style" model uses different options for formatting a date or time
   * based on how the result will be styled, rather than picking specific
   * fields or lengths.
   *
   * Takes an optional time zone which will override the user's default
   * time zone. This is a UTF-16 string that takes the form "GMT±hh:mm", or
   * an IANA time zone identifier, e.g. "America/Chicago".
   */
  static Result<UniquePtr<DateTimeFormat>, ICUError> TryCreateFromStyle(
      Span<const char> aLocale, DateTimeStyle aDateStyle,
      DateTimeStyle aTimeStyle,
      Maybe<Span<const char16_t>> aTimeZoneOverride = Nothing{});

  /**
   * Create a DateTimeFormat from a UTF-8 skeleton. See the UTF-16 version for
   * the full documentation of this function. This overload requires additional
   * work compared to the UTF-16 version.
   */
  static Result<UniquePtr<DateTimeFormat>, ICUError> TryCreateFromSkeleton(
      Span<const char> aLocale, Span<const char> aSkeleton,
      Maybe<Span<const char>> aTimeZoneOverride = Nothing{});

  /**
   * Create a DateTimeFormat from a UTF-16 skeleton.
   *
   * A skeleton is an unordered list of fields that are used to find an
   * appropriate date time format pattern. Example skeletons would be "yMd",
   * "yMMMd", "EBhm". If the skeleton includes string literals or other
   * information, it will be discarded when matching against skeletons.
   *
   * Takes an optional time zone which will override the user's default
   * time zone. This is a string that takes the form "GMT±hh:mm", or
   * an IANA time zone identifier, e.g. "America/Chicago".
   */
  static Result<UniquePtr<DateTimeFormat>, ICUError> TryCreateFromSkeleton(
      Span<const char> aLocale, Span<const char16_t> aSkeleton,
      Maybe<Span<const char16_t>> aTimeZoneOverride = Nothing{});

  static Result<UniquePtr<DateTimeFormat>, ICUError> TryCreateFromPattern(
      Span<const char> aLocale, Span<const char16_t> aPattern,
      Maybe<Span<const char16_t>> aTimeZoneOverride = Nothing{});

  /**
   * Use the format settings to format a date time into a string. The non-null
   * terminated string will be placed into the provided buffer. The idea behind
   * this API is that the constructor is expensive, and then the format
   * operation is cheap.
   *
   * aUnixEpoch is the number of milliseconds since 1 January 1970, UTC.
   */
  template <typename B>
  ICUResult TryFormat(double aUnixEpoch, B& aBuffer) const {
    static_assert(
        std::is_same<typename B::CharType, unsigned char>::value ||
            std::is_same<typename B::CharType, char>::value ||
            std::is_same<typename B::CharType, char16_t>::value,
        "The only buffer CharTypes supported by DateTimeFormat are char "
        "(for UTF-8 support) and char16_t (for UTF-16 support).");

    if constexpr (std::is_same<typename B::CharType, char>::value ||
                  std::is_same<typename B::CharType, unsigned char>::value) {
      // The output buffer is UTF-8, but ICU uses UTF-16 internally.

      // Write the formatted date into the u16Buffer.
      mozilla::Vector<char16_t, StackU16VectorSize> u16Vec;

      auto result = FillVectorWithICUCall(
          u16Vec, [this, &aUnixEpoch](UChar* target, int32_t length,
                                      UErrorCode* status) {
            return udat_format(mDateFormat, aUnixEpoch, target, length,
                               /* UFieldPosition* */ nullptr, status);
          });
      if (result.isErr()) {
        return result;
      }

      if (!FillUTF8Buffer(u16Vec, aBuffer)) {
        return Err(ICUError::OutOfMemory);
      }
      return Ok{};
    } else {
      static_assert(std::is_same<typename B::CharType, char16_t>::value);

      // The output buffer is UTF-16. ICU can output directly into this buffer.
      return FillBufferWithICUCall(
          aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
            return udat_format(mDateFormat, aUnixEpoch, target, length, nullptr,
                               status);
          });
    }
  };

  /**
   * Copies the pattern for the current DateTimeFormat to a buffer.
   */
  template <typename B>
  ICUResult GetPattern(B& aBuffer) const {
    return FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          return udat_toPattern(mDateFormat, /* localized*/ false, target,
                                length, status);
        });
  }

  /**
   * Set the start time of the Gregorian calendar. This is useful for
   * ensuring the consistent use of a proleptic Gregorian calendar for ECMA-402.
   * https://en.wikipedia.org/wiki/Proleptic_Gregorian_calendar
   */
  void SetStartTimeIfGregorian(double aTime);

  ~DateTimeFormat();

  /**
   * TODO(Bug 1686965) - Temporarily get the underlying ICU object while
   * migrating to the unified API. This should be removed when completing the
   * migration.
   */
  UDateFormat* UnsafeGetUDateFormat() const { return mDateFormat; }

  /**
   * Clones the Calendar from a DateTimeFormat, and sets its time with the
   * relative milliseconds since 1 January 1970, UTC.
   */
  Result<UniquePtr<Calendar>, InternalError> CloneCalendar(
      double aUnixEpoch) const;

 private:
  explicit DateTimeFormat(UDateFormat* aDateFormat);

  // mozilla::Vector can avoid heap allocations for small transient buffers.
  static constexpr size_t StackU16VectorSize = 128;

  UDateFormat* mDateFormat = nullptr;
};

}  // namespace mozilla::intl

#endif
