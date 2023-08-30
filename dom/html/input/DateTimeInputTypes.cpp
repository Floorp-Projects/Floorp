/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DateTimeInputTypes.h"

#include "js/Date.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsDOMTokenList.h"

namespace mozilla::dom {

const double DateTimeInputTypeBase::kMinimumYear = 1;
const double DateTimeInputTypeBase::kMaximumYear = 275760;
const double DateTimeInputTypeBase::kMaximumMonthInMaximumYear = 9;
const double DateTimeInputTypeBase::kMaximumWeekInMaximumYear = 37;
const double DateTimeInputTypeBase::kMsPerDay = 24 * 60 * 60 * 1000;

bool DateTimeInputTypeBase::IsMutable() const {
  return !mInputElement->IsDisabledOrReadOnly();
}

bool DateTimeInputTypeBase::IsValueMissing() const {
  if (!mInputElement->IsRequired()) {
    return false;
  }

  if (!IsMutable()) {
    return false;
  }

  return IsValueEmpty();
}

bool DateTimeInputTypeBase::IsRangeOverflow() const {
  Decimal maximum = mInputElement->GetMaximum();
  if (maximum.isNaN()) {
    return false;
  }

  Decimal value = mInputElement->GetValueAsDecimal();
  if (value.isNaN()) {
    return false;
  }

  return value > maximum;
}

bool DateTimeInputTypeBase::IsRangeUnderflow() const {
  Decimal minimum = mInputElement->GetMinimum();
  if (minimum.isNaN()) {
    return false;
  }

  Decimal value = mInputElement->GetValueAsDecimal();
  if (value.isNaN()) {
    return false;
  }

  return value < minimum;
}

bool DateTimeInputTypeBase::HasStepMismatch() const {
  Decimal value = mInputElement->GetValueAsDecimal();
  return mInputElement->ValueIsStepMismatch(value);
}

bool DateTimeInputTypeBase::HasBadInput() const {
  ShadowRoot* shadow = mInputElement->GetShadowRoot();
  if (!shadow) {
    return false;
  }

  Element* editWrapperElement = shadow->GetElementById(u"edit-wrapper"_ns);
  if (!editWrapperElement) {
    return false;
  }

  bool allEmpty = true;
  // Empty field does not imply bad input, but incomplete field does.
  for (Element* child = editWrapperElement->GetFirstElementChild(); child;
       child = child->GetNextElementSibling()) {
    if (!child->ClassList()->Contains(u"datetime-edit-field"_ns)) {
      continue;
    }
    nsAutoString value;
    child->GetAttr(nsGkAtoms::value, value);
    if (!value.IsEmpty()) {
      allEmpty = false;
      break;
    }
  }

  // If some fields are available but input element's value is empty implies it
  // has been sanitized.
  return !allEmpty && IsValueEmpty();
}

nsresult DateTimeInputTypeBase::GetRangeOverflowMessage(nsAString& aMessage) {
  nsAutoString maxStr;
  mInputElement->GetAttr(nsGkAtoms::max, maxStr);

  return nsContentUtils::FormatMaybeLocalizedString(
      aMessage, nsContentUtils::eDOM_PROPERTIES,
      "FormValidationDateTimeRangeOverflow", mInputElement->OwnerDoc(), maxStr);
}

nsresult DateTimeInputTypeBase::GetRangeUnderflowMessage(nsAString& aMessage) {
  nsAutoString minStr;
  mInputElement->GetAttr(nsGkAtoms::min, minStr);

  return nsContentUtils::FormatMaybeLocalizedString(
      aMessage, nsContentUtils::eDOM_PROPERTIES,
      "FormValidationDateTimeRangeUnderflow", mInputElement->OwnerDoc(),
      minStr);
}

void DateTimeInputTypeBase::MinMaxStepAttrChanged() {
  if (Element* dateTimeBoxElement = mInputElement->GetDateTimeBoxElement()) {
    AsyncEventDispatcher::RunDOMEventWhenSafe(
        *dateTimeBoxElement, u"MozNotifyMinMaxStepAttrChanged"_ns,
        CanBubble::eNo, ChromeOnlyDispatch::eNo);
  }
}

bool DateTimeInputTypeBase::GetTimeFromMs(double aValue, uint16_t* aHours,
                                          uint16_t* aMinutes,
                                          uint16_t* aSeconds,
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

nsresult DateInputType::GetBadInputMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationInvalidDate",
      mInputElement->OwnerDoc(), aMessage);
}

auto DateInputType::ConvertStringToNumber(const nsAString& aValue) const
    -> StringToNumberResult {
  uint32_t year, month, day;
  if (!ParseDate(aValue, &year, &month, &day)) {
    return {};
  }
  JS::ClippedTime time = JS::TimeClip(JS::MakeDate(year, month - 1, day));
  if (!time.isValid()) {
    return {};
  }
  return {Decimal::fromDouble(time.toDouble())};
}

bool DateInputType::ConvertNumberToString(Decimal aValue,
                                          nsAString& aResultString) const {
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  // The specs (and our JS APIs) require |aValue| to be truncated.
  aValue = aValue.floor();

  double year = JS::YearFromTime(aValue.toDouble());
  double month = JS::MonthFromTime(aValue.toDouble());
  double day = JS::DayFromTime(aValue.toDouble());

  if (std::isnan(year) || std::isnan(month) || std::isnan(day)) {
    return false;
  }

  aResultString.AppendPrintf("%04.0f-%02.0f-%02.0f", year, month + 1, day);
  return true;
}

// input type=time

nsresult TimeInputType::GetBadInputMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationInvalidTime",
      mInputElement->OwnerDoc(), aMessage);
}

auto TimeInputType::ConvertStringToNumber(const nsAString& aValue) const
    -> StringToNumberResult {
  uint32_t milliseconds;
  if (!ParseTime(aValue, &milliseconds)) {
    return {};
  }
  return {Decimal(int32_t(milliseconds))};
}

bool TimeInputType::ConvertNumberToString(Decimal aValue,
                                          nsAString& aResultString) const {
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  aValue = aValue.floor();
  // Per spec, we need to truncate |aValue| and we should only represent
  // times inside a day [00:00, 24:00[, which means that we should do a
  // modulo on |aValue| using the number of milliseconds in a day (86400000).
  uint32_t value =
      NS_floorModulo(aValue, Decimal::fromDouble(kMsPerDay)).toDouble();

  uint16_t milliseconds, seconds, minutes, hours;
  if (!GetTimeFromMs(value, &hours, &minutes, &seconds, &milliseconds)) {
    return false;
  }

  if (milliseconds != 0) {
    aResultString.AppendPrintf("%02d:%02d:%02d.%03d", hours, minutes, seconds,
                               milliseconds);
  } else if (seconds != 0) {
    aResultString.AppendPrintf("%02d:%02d:%02d", hours, minutes, seconds);
  } else {
    aResultString.AppendPrintf("%02d:%02d", hours, minutes);
  }

  return true;
}

bool TimeInputType::HasReversedRange() const {
  mozilla::Decimal maximum = mInputElement->GetMaximum();
  if (maximum.isNaN()) {
    return false;
  }

  mozilla::Decimal minimum = mInputElement->GetMinimum();
  if (minimum.isNaN()) {
    return false;
  }

  return maximum < minimum;
}

bool TimeInputType::IsReversedRangeUnderflowAndOverflow() const {
  mozilla::Decimal maximum = mInputElement->GetMaximum();
  mozilla::Decimal minimum = mInputElement->GetMinimum();
  mozilla::Decimal value = mInputElement->GetValueAsDecimal();

  MOZ_ASSERT(HasReversedRange(), "Must have reserved range.");

  if (value.isNaN()) {
    return false;
  }

  // When an element has a reversed range, and the value is more than the
  // maximum and less than the minimum the element is simultaneously suffering
  // from an underflow and suffering from an overflow.
  return value > maximum && value < minimum;
}

bool TimeInputType::IsRangeOverflow() const {
  return HasReversedRange() ? IsReversedRangeUnderflowAndOverflow()
                            : DateTimeInputTypeBase::IsRangeOverflow();
}

bool TimeInputType::IsRangeUnderflow() const {
  return HasReversedRange() ? IsReversedRangeUnderflowAndOverflow()
                            : DateTimeInputTypeBase::IsRangeUnderflow();
}

nsresult TimeInputType::GetReversedRangeUnderflowAndOverflowMessage(
    nsAString& aMessage) {
  nsAutoString maxStr;
  mInputElement->GetAttr(nsGkAtoms::max, maxStr);

  nsAutoString minStr;
  mInputElement->GetAttr(nsGkAtoms::min, minStr);

  return nsContentUtils::FormatMaybeLocalizedString(
      aMessage, nsContentUtils::eDOM_PROPERTIES,
      "FormValidationTimeReversedRangeUnderflowAndOverflow",
      mInputElement->OwnerDoc(), minStr, maxStr);
}

nsresult TimeInputType::GetRangeOverflowMessage(nsAString& aMessage) {
  return HasReversedRange()
             ? GetReversedRangeUnderflowAndOverflowMessage(aMessage)
             : DateTimeInputTypeBase::GetRangeOverflowMessage(aMessage);
}

nsresult TimeInputType::GetRangeUnderflowMessage(nsAString& aMessage) {
  return HasReversedRange()
             ? GetReversedRangeUnderflowAndOverflowMessage(aMessage)
             : DateTimeInputTypeBase::GetRangeUnderflowMessage(aMessage);
}

// input type=week

nsresult WeekInputType::GetBadInputMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationInvalidWeek",
      mInputElement->OwnerDoc(), aMessage);
}

auto WeekInputType::ConvertStringToNumber(const nsAString& aValue) const
    -> StringToNumberResult {
  uint32_t year, week;
  if (!ParseWeek(aValue, &year, &week)) {
    return {};
  }
  if (year < kMinimumYear || year > kMaximumYear) {
    return {};
  }
  // Maximum week is 275760-W37, the week of 275760-09-13.
  if (year == kMaximumYear && week > kMaximumWeekInMaximumYear) {
    return {};
  }
  double days = DaysSinceEpochFromWeek(year, week);
  return {Decimal::fromDouble(days * kMsPerDay)};
}

bool WeekInputType::ConvertNumberToString(Decimal aValue,
                                          nsAString& aResultString) const {
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  aValue = aValue.floor();

  // Based on ISO 8601 date.
  double year = JS::YearFromTime(aValue.toDouble());
  double month = JS::MonthFromTime(aValue.toDouble());
  double day = JS::DayFromTime(aValue.toDouble());
  // Adding 1 since day starts from 0.
  double dayInYear = JS::DayWithinYear(aValue.toDouble(), year) + 1;

  // Return if aValue is outside the valid JS date-time range.
  if (std::isnan(year) || std::isnan(month) || std::isnan(day) ||
      std::isnan(dayInYear)) {
    return false;
  }

  // DayOfWeek requires the year to be non-negative.
  if (year < 0) {
    return false;
  }

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

nsresult MonthInputType::GetBadInputMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationInvalidMonth",
      mInputElement->OwnerDoc(), aMessage);
}

auto MonthInputType::ConvertStringToNumber(const nsAString& aValue) const
    -> StringToNumberResult {
  uint32_t year, month;
  if (!ParseMonth(aValue, &year, &month)) {
    return {};
  }

  if (year < kMinimumYear || year > kMaximumYear) {
    return {};
  }

  // Maximum valid month is 275760-09.
  if (year == kMaximumYear && month > kMaximumMonthInMaximumYear) {
    return {};
  }

  int32_t months = MonthsSinceJan1970(year, month);
  return {Decimal(int32_t(months))};
}

bool MonthInputType::ConvertNumberToString(Decimal aValue,
                                           nsAString& aResultString) const {
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  aValue = aValue.floor();

  double month = NS_floorModulo(aValue, Decimal(12)).toDouble();
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

nsresult DateTimeLocalInputType::GetBadInputMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationInvalidDateTime",
      mInputElement->OwnerDoc(), aMessage);
}

auto DateTimeLocalInputType::ConvertStringToNumber(
    const nsAString& aValue) const -> StringToNumberResult {
  uint32_t year, month, day, timeInMs;
  if (!ParseDateTimeLocal(aValue, &year, &month, &day, &timeInMs)) {
    return {};
  }
  JS::ClippedTime time =
      JS::TimeClip(JS::MakeDate(year, month - 1, day, timeInMs));
  if (!time.isValid()) {
    return {};
  }
  return {Decimal::fromDouble(time.toDouble())};
}

bool DateTimeLocalInputType::ConvertNumberToString(
    Decimal aValue, nsAString& aResultString) const {
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  aValue = aValue.floor();

  uint32_t timeValue =
      NS_floorModulo(aValue, Decimal::fromDouble(kMsPerDay)).toDouble();

  uint16_t milliseconds, seconds, minutes, hours;
  if (!GetTimeFromMs(timeValue, &hours, &minutes, &seconds, &milliseconds)) {
    return false;
  }

  double year = JS::YearFromTime(aValue.toDouble());
  double month = JS::MonthFromTime(aValue.toDouble());
  double day = JS::DayFromTime(aValue.toDouble());

  if (std::isnan(year) || std::isnan(month) || std::isnan(day)) {
    return false;
  }

  if (milliseconds != 0) {
    aResultString.AppendPrintf("%04.0f-%02.0f-%02.0fT%02d:%02d:%02d.%03d", year,
                               month + 1, day, hours, minutes, seconds,
                               milliseconds);
  } else if (seconds != 0) {
    aResultString.AppendPrintf("%04.0f-%02.0f-%02.0fT%02d:%02d:%02d", year,
                               month + 1, day, hours, minutes, seconds);
  } else {
    aResultString.AppendPrintf("%04.0f-%02.0f-%02.0fT%02d:%02d", year,
                               month + 1, day, hours, minutes);
  }

  return true;
}

}  // namespace mozilla::dom
