/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DateTimeInputTypes_h__
#define DateTimeInputTypes_h__

#include "InputType.h"

class DateTimeInputTypeBase : public ::InputType
{
public:
  ~DateTimeInputTypeBase() override {}

  bool IsValueMissing() const override;
  bool IsRangeOverflow() const override;
  bool IsRangeUnderflow() const override;
  bool HasStepMismatch(bool aUseZeroIfValueNaN) const override;
  bool HasBadInput() const override;

  nsresult GetRangeOverflowMessage(nsXPIDLString& aMessage) override;
  nsresult GetRangeUnderflowMessage(nsXPIDLString& aMessage) override;

  nsresult MinMaxStepAttrChanged() override;

protected:
  explicit DateTimeInputTypeBase(mozilla::dom::HTMLInputElement* aInputElement)
    : InputType(aInputElement)
  {}

  /**
   * Checks preference "dom.forms.datetime" to determine if input date and time
   * should be supported.
   */
  static bool IsInputDateTimeEnabled();

  bool IsMutable() const override;

  /**
   * This method converts aValue (milliseconds within a day) to hours, minutes,
   * seconds and milliseconds.
   */
  bool GetTimeFromMs(double aValue, uint16_t* aHours, uint16_t* aMinutes,
                     uint16_t* aSeconds, uint16_t* aMilliseconds) const;

  // Minimum year limited by HTML standard, year >= 1.
  static const double kMinimumYear;
  // Maximum year limited by ECMAScript date object range, year <= 275760.
  static const double kMaximumYear;
  // Maximum valid month is 275760-09.
  static const double kMaximumMonthInMaximumYear;
  // Maximum valid week is 275760-W37.
  static const double kMaximumWeekInMaximumYear;
  // Milliseconds in a day.
  static const double kMsPerDay;
};

// input type=date
class DateInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) DateInputType(aInputElement);
  }

  // Currently, for input date and time, only date can have an invalid value, as
  // we forbid or autocorrect values that are not in the valid range for time.
  // For example, in 12hr format, if user enters '2' in the hour field, it will
  // be treated as '02' and automatically advance to the next field.
  nsresult GetBadInputMessage(nsXPIDLString& aMessage) override;

  bool ConvertStringToNumber(nsAString& aValue,
                             mozilla::Decimal& aResultValue) const override;
  bool ConvertNumberToString(mozilla::Decimal aValue,
                             nsAString& aResultString) const override;

private:
  explicit DateInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};

// input type=time
class TimeInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) TimeInputType(aInputElement);
  }

  bool ConvertStringToNumber(nsAString& aValue,
                             mozilla::Decimal& aResultValue) const override;
  bool ConvertNumberToString(mozilla::Decimal aValue,
                             nsAString& aResultString) const override;

private:
  explicit TimeInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};

// input type=week
class WeekInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) WeekInputType(aInputElement);
  }

  bool ConvertStringToNumber(nsAString& aValue,
                             mozilla::Decimal& aResultValue) const override;
  bool ConvertNumberToString(mozilla::Decimal aValue,
                             nsAString& aResultString) const override;

private:
  explicit WeekInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};

// input type=month
class MonthInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) MonthInputType(aInputElement);
  }

  bool ConvertStringToNumber(nsAString& aValue,
                             mozilla::Decimal& aResultValue) const override;
  bool ConvertNumberToString(mozilla::Decimal aValue,
                             nsAString& aResultString) const override;

private:
  explicit MonthInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};

// input type=datetime-local
class DateTimeLocalInputType : public DateTimeInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) DateTimeLocalInputType(aInputElement);
  }

  bool ConvertStringToNumber(nsAString& aValue,
                             mozilla::Decimal& aResultValue) const override;
  bool ConvertNumberToString(mozilla::Decimal aValue,
                             nsAString& aResultString) const override;

private:
  explicit DateTimeLocalInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : DateTimeInputTypeBase(aInputElement)
  {}
};


#endif /* DateTimeInputTypes_h__ */
