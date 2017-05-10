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
