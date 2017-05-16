/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateTimeInputTypes.h"

#include "js/Date.h"
#include "mozilla/dom/HTMLInputElement.h"

const double DateTimeInputTypeBase::kMinimumYear = 1;
const double DateTimeInputTypeBase::kMaximumYear = 275760;
const double DateTimeInputTypeBase::kMaximumMonthInMaximumYear = 9;
const double DateTimeInputTypeBase::kMaximumWeekInMaximumYear = 37;
const double DateTimeInputTypeBase::kMsPerDay = 24 * 60 * 60 * 1000;

bool
DateTimeInputTypeBase::IsMutable() const
{
  return !mInputElement->IsDisabled() &&
         !mInputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly);
}

bool
DateTimeInputTypeBase::IsValueMissing() const
{
  if (!mInputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
    return false;
  }

  if (!IsMutable()) {
    return false;
  }

  return IsValueEmpty();
}

bool
DateTimeInputTypeBase::IsRangeOverflow() const
{
  mozilla::Decimal maximum = mInputElement->GetMaximum();
  if (maximum.isNaN()) {
    return false;
  }

  mozilla::Decimal value = mInputElement->GetValueAsDecimal();
  if (value.isNaN()) {
    return false;
  }

  return value > maximum;
}

bool
DateTimeInputTypeBase::IsRangeUnderflow() const
{
  mozilla::Decimal minimum = mInputElement->GetMinimum();
  if (minimum.isNaN()) {
    return false;
  }

  mozilla::Decimal value = mInputElement->GetValueAsDecimal();
  if (value.isNaN()) {
    return false;
  }

  return value < minimum;
}

bool
DateTimeInputTypeBase::HasStepMismatch(bool aUseZeroIfValueNaN) const
{
  mozilla::Decimal value = mInputElement->GetValueAsDecimal();
  if (value.isNaN()) {
    if (aUseZeroIfValueNaN) {
      value = mozilla::Decimal(0);
    } else {
      // The element can't suffer from step mismatch if it's value isn't a number.
      return false;
    }
  }

  mozilla::Decimal step = mInputElement->GetStep();
  if (step == kStepAny) {
    return false;
  }

  // Value has to be an integral multiple of step.
  return NS_floorModulo(value - GetStepBase(), step) != mozilla::Decimal(0);
}

bool
DateTimeInputTypeBase::GetTimeFromMs(double aValue, uint16_t* aHours,
                                     uint16_t* aMinutes, uint16_t* aSeconds,
                                     uint16_t* aMilliseconds) const {
  MOZ_ASSERT(aValue >= 0 && aValue < kMsPerDay,
             "aValue must be milliseconds within a day!");

  uint32_t value = floor(aValue);

  *aMilliseconds = value % 1000;
  value /= 1000;

  *aSeconds = value % 60;
  value /= 60;

  *aMinutes = value % 60;
  value /= 60;

  *aHours = value;

  return true;
}

// input type=date

bool
DateInputType::ConvertStringToNumber(nsAString& aValue,
                                     mozilla::Decimal& aResultValue) const
{
  uint32_t year, month, day;
  if (!ParseDate(aValue, &year, &month, &day)) {
    return false;
  }

  JS::ClippedTime time = JS::TimeClip(JS::MakeDate(year, month - 1, day));
  if (!time.isValid()) {
    return false;
  }

  aResultValue = mozilla::Decimal::fromDouble(time.toDouble());
  return true;
}

bool
DateInputType::ConvertNumberToString(mozilla::Decimal aValue,
                                     nsAString& aResultString) const
{
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  // The specs (and our JS APIs) require |aValue| to be truncated.
  aValue = aValue.floor();

  double year = JS::YearFromTime(aValue.toDouble());
  double month = JS::MonthFromTime(aValue.toDouble());
  double day = JS::DayFromTime(aValue.toDouble());

  if (mozilla::IsNaN(year) || mozilla::IsNaN(month) || mozilla::IsNaN(day)) {
    return false;
  }

  aResultString.AppendPrintf("%04.0f-%02.0f-%02.0f", year, month + 1, day);
  return true;
}

// input type=time

bool
TimeInputType::ConvertStringToNumber(nsAString& aValue,
                                     mozilla::Decimal& aResultValue) const
{
  uint32_t milliseconds;
  if (!ParseTime(aValue, &milliseconds)) {
    return false;
  }

  aResultValue = mozilla::Decimal(int32_t(milliseconds));
  return true;
}

bool
TimeInputType::ConvertNumberToString(mozilla::Decimal aValue,
                                     nsAString& aResultString) const
{
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  aValue = aValue.floor();
  // Per spec, we need to truncate |aValue| and we should only represent
  // times inside a day [00:00, 24:00[, which means that we should do a
  // modulo on |aValue| using the number of milliseconds in a day (86400000).
  uint32_t value =
    NS_floorModulo(aValue, mozilla::Decimal::fromDouble(kMsPerDay)).toDouble();

  uint16_t milliseconds, seconds, minutes, hours;
  if (!GetTimeFromMs(value, &hours, &minutes, &seconds, &milliseconds)) {
    return false;
  }

  if (milliseconds != 0) {
    aResultString.AppendPrintf("%02d:%02d:%02d.%03d",
                               hours, minutes, seconds, milliseconds);
  } else if (seconds != 0) {
    aResultString.AppendPrintf("%02d:%02d:%02d",
                               hours, minutes, seconds);
  } else {
    aResultString.AppendPrintf("%02d:%02d", hours, minutes);
  }

  return true;
}

// input type=week

bool
WeekInputType::ConvertStringToNumber(nsAString& aValue,
                                     mozilla::Decimal& aResultValue) const
{
  uint32_t year, week;
  if (!ParseWeek(aValue, &year, &week)) {
    return false;
  }

  if (year < kMinimumYear || year > kMaximumYear) {
    return false;
  }

  // Maximum week is 275760-W37, the week of 275760-09-13.
  if (year == kMaximumYear && week > kMaximumWeekInMaximumYear) {
    return false;
  }

  double days = DaysSinceEpochFromWeek(year, week);
  aResultValue = mozilla::Decimal::fromDouble(days * kMsPerDay);
  return true;
}

bool
WeekInputType::ConvertNumberToString(mozilla::Decimal aValue,
                                     nsAString& aResultString) const
{
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  aValue = aValue.floor();

  // Based on ISO 8601 date.
  double year = JS::YearFromTime(aValue.toDouble());
  double month = JS::MonthFromTime(aValue.toDouble());
  double day = JS::DayFromTime(aValue.toDouble());
  // Adding 1 since day starts from 0.
  double dayInYear = JS::DayWithinYear(aValue.toDouble(), year) + 1;

  // Adding 1 since month starts from 0.
  uint32_t isoWeekday = DayOfWeek(year, month + 1, day, true);
  // Target on Wednesday since ISO 8601 states that week 1 is the week
  // with the first Thursday of that year.
  uint32_t week = (dayInYear - isoWeekday + 10) / 7;

  if (week < 1) {
    year--;
    if (year < 1) {
      return false;
    }
    week = MaximumWeekInYear(year);
  } else if (week > MaximumWeekInYear(year)) {
    year++;
    if (year > kMaximumYear ||
        (year == kMaximumYear && week > kMaximumWeekInMaximumYear)) {
      return false;
    }
    week = 1;
  }

  aResultString.AppendPrintf("%04.0f-W%02d", year, week);
  return true;
}

// input type=month

bool
MonthInputType::ConvertStringToNumber(nsAString& aValue,
                                      mozilla::Decimal& aResultValue) const
{
  uint32_t year, month;
  if (!ParseMonth(aValue, &year, &month)) {
    return false;
  }

  if (year < kMinimumYear || year > kMaximumYear) {
    return false;
  }

  // Maximum valid month is 275760-09.
  if (year == kMaximumYear && month > kMaximumMonthInMaximumYear) {
    return false;
  }

  int32_t months = MonthsSinceJan1970(year, month);
  aResultValue = mozilla::Decimal(int32_t(months));
  return true;
}

bool
MonthInputType::ConvertNumberToString(mozilla::Decimal aValue,
                                      nsAString& aResultString) const
{
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  aValue = aValue.floor();

  double month = NS_floorModulo(aValue, mozilla::Decimal(12)).toDouble();
  month = (month < 0 ? month + 12 : month);

  double year = 1970 + (aValue.toDouble() - month) / 12;

  // Maximum valid month is 275760-09.
  if (year < kMinimumYear || year > kMaximumYear) {
    return false;
  }

  if (year == kMaximumYear && month > 8) {
    return false;
  }

  aResultString.AppendPrintf("%04.0f-%02.0f", year, month + 1);
  return true;

}

// input type=datetime-local

bool
DateTimeLocalInputType::ConvertStringToNumber(
  nsAString& aValue, mozilla::Decimal& aResultValue) const
{
  uint32_t year, month, day, timeInMs;
  if (!ParseDateTimeLocal(aValue, &year, &month, &day, &timeInMs)) {
    return false;
  }

  JS::ClippedTime time = JS::TimeClip(JS::MakeDate(year, month - 1, day,
                                                   timeInMs));
  if (!time.isValid()) {
    return false;
  }

  aResultValue = mozilla::Decimal::fromDouble(time.toDouble());
  return true;
}

bool
DateTimeLocalInputType::ConvertNumberToString(mozilla::Decimal aValue,
                                              nsAString& aResultString) const
{
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  aValue = aValue.floor();

  uint32_t timeValue =
    NS_floorModulo(aValue, mozilla::Decimal::fromDouble(kMsPerDay)).toDouble();

  uint16_t milliseconds, seconds, minutes, hours;
  if (!GetTimeFromMs(timeValue, &hours, &minutes, &seconds, &milliseconds)) {
    return false;
  }

  double year = JS::YearFromTime(aValue.toDouble());
  double month = JS::MonthFromTime(aValue.toDouble());
  double day = JS::DayFromTime(aValue.toDouble());

  if (mozilla::IsNaN(year) || mozilla::IsNaN(month) || mozilla::IsNaN(day)) {
    return false;
  }

  if (milliseconds != 0) {
    aResultString.AppendPrintf("%04.0f-%02.0f-%02.0fT%02d:%02d:%02d.%03d",
                               year, month + 1, day, hours, minutes,
                               seconds, milliseconds);
  } else if (seconds != 0) {
    aResultString.AppendPrintf("%04.0f-%02.0f-%02.0fT%02d:%02d:%02d",
                               year, month + 1, day, hours, minutes,
                               seconds);
  } else {
    aResultString.AppendPrintf("%04.0f-%02.0f-%02.0fT%02d:%02d",
                               year, month + 1, day, hours, minutes);
  }

  return true;
}
