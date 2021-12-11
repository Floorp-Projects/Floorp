/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_Calendar_h_
#define intl_components_Calendar_h_

#include "mozilla/Assertions.h"
#include "mozilla/EnumSet.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"

using UCalendar = void*;

namespace mozilla::intl {

/**
 * Weekdays in the ISO-8601 calendar.
 */
enum class Weekday : uint8_t {
  Monday = 1,
  Tuesday,
  Wednesday,
  Thursday,
  Friday,
  Saturday,
  Sunday,
};

/**
 * This component is a Mozilla-focused API for working with calendar systems in
 * internationalization code. It is used in coordination with other operations
 * such as datetime formatting.
 */
class Calendar final {
 public:
  explicit Calendar(UCalendar* aCalendar) : mCalendar(aCalendar) {
    MOZ_ASSERT(aCalendar);
  };

  // Do not allow copy as this class owns the ICU resource. Move is not
  // currently implemented, but a custom move operator could be created if
  // needed.
  Calendar(const Calendar&) = delete;
  Calendar& operator=(const Calendar&) = delete;

  /**
   * Create a Calendar.
   */
  static Result<UniquePtr<Calendar>, ICUError> TryCreate(
      const char* aLocale,
      Maybe<Span<const char16_t>> aTimeZoneOverride = Nothing{});

  /**
   * Get the BCP 47 keyword value string designating the calendar type. For
   * instance "gregory", "chinese", "islamic-civil", etc.
   */
  Result<Span<const char>, ICUError> GetBcp47Type();

  /**
   * Return the set of weekdays which are considered as part of the weekend.
   */
  Result<EnumSet<Weekday>, ICUError> GetWeekend();

  /**
   * Return the weekday which is considered the first day of the week.
   */
  Weekday GetFirstDayOfWeek();

  /**
   * Return the minimal number of days in the first week of a year.
   */
  int32_t GetMinimalDaysInFirstWeek();

  /**
   * Set the time for the calendar relative to the number of milliseconds since
   * 1 January 1970, UTC.
   */
  Result<Ok, ICUError> SetTimeInMs(double aUnixEpoch);

  /**
   * Return ICU legacy keywords, such as "gregorian", "islamic",
   * "islamic-civil", "hebrew", etc.
   */
  static Result<SpanEnumeration<char>, ICUError>
  GetLegacyKeywordValuesForLocale(const char* aLocale);

 private:
  /**
   * Internal function to convert a legacy calendar identifier to the newer
   * BCP 47 identifier.
   */
  static SpanResult<char> LegacyIdentifierToBcp47(const char* aIdentifier,
                                                  int32_t aLength);

 public:
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

  using Bcp47IdentifierEnumeration =
      Enumeration<char, SpanResult<char>, Calendar::LegacyIdentifierToBcp47>;

  /**
   * Return BCP 47 Unicode locale extension type keywords.
   */
  static Result<Bcp47IdentifierEnumeration, ICUError>
  GetBcp47KeywordValuesForLocale(const char* aLocale,
                                 CommonlyUsed aCommonlyUsed = CommonlyUsed::No);

  ~Calendar();

 private:
  friend class DateIntervalFormat;
  UCalendar* GetUCalendar() const { return mCalendar; }

  UCalendar* mCalendar = nullptr;
};

}  // namespace mozilla::intl

#endif
