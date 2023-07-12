/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DateTimeInputTypes_h__
#define mozilla_dom_DateTimeInputTypes_h__

#include "mozilla/dom/InputType.h"

namespace mozilla::dom {

class DateTimeInputTypeBase : public InputType {
 public:
  ~DateTimeInputTypeBase() override = default;

  bool IsValueMissing() const override;
  bool IsRangeOverflow() const override;
  bool IsRangeUnderflow() const override;
  bool HasStepMismatch() const override;
  bool HasBadInput() const override;

  nsresult GetRangeOverflowMessage(nsAString& aMessage) override;
  nsresult GetRangeUnderflowMessage(nsAString& aMessage) override;

  void MinMaxStepAttrChanged() override;

 protected:
  explicit DateTimeInputTypeBase(HTMLInputElement* aInputElement)
      : InputType(aInputElement) {}

  bool IsMutable() const override;

  nsresult GetBadInputMessage(nsAString& aMessage) override = 0;

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
class DateInputType : public DateTimeInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) DateInputType(aInputElement);
  }

  nsresult GetBadInputMessage(nsAString& aMessage) override;

  StringToNumberResult ConvertStringToNumber(const nsAString&) const override;
  bool ConvertNumberToString(Decimal aValue,
                             nsAString& aResultString) const override;

 private:
  explicit DateInputType(HTMLInputElement* aInputElement)
      : DateTimeInputTypeBase(aInputElement) {}
};

// input type=time
class TimeInputType : public DateTimeInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) TimeInputType(aInputElement);
  }

  nsresult GetBadInputMessage(nsAString& aMessage) override;

  StringToNumberResult ConvertStringToNumber(const nsAString&) const override;
  bool ConvertNumberToString(Decimal aValue,
                             nsAString& aResultString) const override;
  bool IsRangeOverflow() const override;
  bool IsRangeUnderflow() const override;
  nsresult GetRangeOverflowMessage(nsAString& aMessage) override;
  nsresult GetRangeUnderflowMessage(nsAString& aMessage) override;

 private:
  explicit TimeInputType(HTMLInputElement* aInputElement)
      : DateTimeInputTypeBase(aInputElement) {}

  // https://html.spec.whatwg.org/multipage/input.html#has-a-reversed-range
  bool HasReversedRange() const;
  bool IsReversedRangeUnderflowAndOverflow() const;
  nsresult GetReversedRangeUnderflowAndOverflowMessage(nsAString& aMessage);
};

// input type=week
class WeekInputType : public DateTimeInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) WeekInputType(aInputElement);
  }

  nsresult GetBadInputMessage(nsAString& aMessage) override;
  StringToNumberResult ConvertStringToNumber(const nsAString&) const override;
  bool ConvertNumberToString(Decimal aValue,
                             nsAString& aResultString) const override;

 private:
  explicit WeekInputType(HTMLInputElement* aInputElement)
      : DateTimeInputTypeBase(aInputElement) {}
};

// input type=month
class MonthInputType : public DateTimeInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) MonthInputType(aInputElement);
  }

  nsresult GetBadInputMessage(nsAString& aMessage) override;
  StringToNumberResult ConvertStringToNumber(const nsAString&) const override;
  bool ConvertNumberToString(Decimal aValue,
                             nsAString& aResultString) const override;

 private:
  explicit MonthInputType(HTMLInputElement* aInputElement)
      : DateTimeInputTypeBase(aInputElement) {}
};

// input type=datetime-local
class DateTimeLocalInputType : public DateTimeInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) DateTimeLocalInputType(aInputElement);
  }

  nsresult GetBadInputMessage(nsAString& aMessage) override;
  StringToNumberResult ConvertStringToNumber(const nsAString&) const override;
  bool ConvertNumberToString(Decimal aValue,
                             nsAString& aResultString) const override;

 private:
  explicit DateTimeLocalInputType(HTMLInputElement* aInputElement)
      : DateTimeInputTypeBase(aInputElement) {}
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_DateTimeInputTypes_h__ */
