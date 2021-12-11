/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include "DateTimeFormatUtils.h"

namespace mozilla::intl {

DateTimePartType ConvertUFormatFieldToPartType(UDateFormatField fieldName) {
  // See intl/icu/source/i18n/unicode/udat.h for a detailed field list.  This
  // switch is deliberately exhaustive: cases might have to be added/removed
  // if this code is compiled with a different ICU with more
  // UDateFormatField enum initializers.  Please guard such cases with
  // appropriate ICU version-testing #ifdefs, should cross-version divergence
  // occur.
  switch (fieldName) {
    case UDAT_ERA_FIELD:
      return DateTimePartType::Era;

    case UDAT_YEAR_FIELD:
    case UDAT_YEAR_WOY_FIELD:
    case UDAT_EXTENDED_YEAR_FIELD:
      return DateTimePartType::Year;

    case UDAT_YEAR_NAME_FIELD:
      return DateTimePartType::YearName;

    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
      return DateTimePartType::Month;

    case UDAT_DATE_FIELD:
    case UDAT_JULIAN_DAY_FIELD:
      return DateTimePartType::Day;

    case UDAT_HOUR_OF_DAY1_FIELD:
    case UDAT_HOUR_OF_DAY0_FIELD:
    case UDAT_HOUR1_FIELD:
    case UDAT_HOUR0_FIELD:
      return DateTimePartType::Hour;

    case UDAT_MINUTE_FIELD:
      return DateTimePartType::Minute;

    case UDAT_SECOND_FIELD:
      return DateTimePartType::Second;

    case UDAT_DAY_OF_WEEK_FIELD:
    case UDAT_STANDALONE_DAY_FIELD:
    case UDAT_DOW_LOCAL_FIELD:
    case UDAT_DAY_OF_WEEK_IN_MONTH_FIELD:
      return DateTimePartType::Weekday;

    case UDAT_AM_PM_FIELD:
    case UDAT_FLEXIBLE_DAY_PERIOD_FIELD:
      return DateTimePartType::DayPeriod;

    case UDAT_TIMEZONE_FIELD:
    case UDAT_TIMEZONE_GENERIC_FIELD:
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
      return DateTimePartType::TimeZoneName;

    case UDAT_FRACTIONAL_SECOND_FIELD:
      return DateTimePartType::FractionalSecondDigits;

#ifndef U_HIDE_INTERNAL_API
    case UDAT_RELATED_YEAR_FIELD:
      return DateTimePartType::RelatedYear;
#endif

    case UDAT_DAY_OF_YEAR_FIELD:
    case UDAT_WEEK_OF_YEAR_FIELD:
    case UDAT_WEEK_OF_MONTH_FIELD:
    case UDAT_MILLISECONDS_IN_DAY_FIELD:
    case UDAT_TIMEZONE_RFC_FIELD:
    case UDAT_QUARTER_FIELD:
    case UDAT_STANDALONE_QUARTER_FIELD:
    case UDAT_TIMEZONE_SPECIAL_FIELD:
    case UDAT_TIMEZONE_ISO_FIELD:
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
    case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
#ifndef U_HIDE_INTERNAL_API
    case UDAT_TIME_SEPARATOR_FIELD:
#endif
      // These fields are all unsupported.
      return DateTimePartType::Unknown;

#ifndef U_HIDE_DEPRECATED_API
    case UDAT_FIELD_COUNT:
      MOZ_ASSERT_UNREACHABLE(
          "format field sentinel value returned by "
          "iterator!");
#endif
  }

  MOZ_ASSERT_UNREACHABLE(
      "unenumerated, undocumented format field returned "
      "by iterator");
  return DateTimePartType::Unknown;
}

}  // namespace mozilla::intl
