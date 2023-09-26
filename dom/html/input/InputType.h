/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputType_h__
#define mozilla_dom_InputType_h__

#include <stdint.h>
#include "mozilla/Decimal.h"
#include "mozilla/Maybe.h"
#include "mozilla/TextControlState.h"
#include "mozilla/UniquePtr.h"
#include "nsIConstraintValidation.h"
#include "nsString.h"
#include "nsError.h"

// This must come outside of any namespace, or else it won't overload with the
// double based version in nsMathUtils.h
inline mozilla::Decimal NS_floorModulo(mozilla::Decimal x, mozilla::Decimal y) {
  return (x - y * (x / y).floor());
}

class nsIFrame;

namespace mozilla::dom {
class HTMLInputElement;

/**
 * A common superclass for different types of a HTMLInputElement.
 */
class InputType {
 public:
  using ValueSetterOption = TextControlState::ValueSetterOption;
  using ValueSetterOptions = TextControlState::ValueSetterOptions;

  // Custom deleter for UniquePtr<InputType> to avoid freeing memory
  // pre-allocated for InputType, but we still need to call the destructor
  // explictly.
  struct DoNotDelete {
    void operator()(InputType* p) { p->~InputType(); }
  };

  static UniquePtr<InputType, DoNotDelete> Create(
      HTMLInputElement* aInputElement, FormControlType, void* aMemory);

  virtual ~InputType() = default;

  // Float value returned by GetStep() when the step attribute is set to 'any'.
  static constexpr Decimal kStepAny = Decimal(0_d);

  /**
   * Drop the reference to the input element.
   */
  void DropReference();

  virtual bool MinAndMaxLengthApply() const { return false; }
  virtual bool IsTooLong() const;
  virtual bool IsTooShort() const;
  virtual bool IsValueMissing() const;
  virtual bool HasTypeMismatch() const;
  // May return Nothing() if the JS engine failed to evaluate the regex.
  virtual Maybe<bool> HasPatternMismatch() const;
  virtual bool IsRangeOverflow() const;
  virtual bool IsRangeUnderflow() const;
  virtual bool HasStepMismatch() const;
  virtual bool HasBadInput() const;

  nsresult GetValidationMessage(
      nsAString& aValidationMessage,
      nsIConstraintValidation::ValidityStateType aType);
  virtual nsresult GetValueMissingMessage(nsAString& aMessage);
  virtual nsresult GetTypeMismatchMessage(nsAString& aMessage);
  virtual nsresult GetRangeOverflowMessage(nsAString& aMessage);
  virtual nsresult GetRangeUnderflowMessage(nsAString& aMessage);
  virtual nsresult GetBadInputMessage(nsAString& aMessage);

  MOZ_CAN_RUN_SCRIPT virtual void MinMaxStepAttrChanged() {}

  /**
   * Convert a string to a Decimal number in a type specific way,
   * http://www.whatwg.org/specs/web-apps/current-work/multipage/the-input-element.html#concept-input-value-string-number
   * ie parse a date string to a timestamp if type=date,
   * or parse a number string to its value if type=number.
   * @param aValue the string to be parsed.
   */
  struct StringToNumberResult {
    // The result decimal. Successfully parsed if it's finite.
    Decimal mResult = Decimal::nan();
    // Whether the result required reading locale-dependent data (for input
    // type=number), or the value parses using the regular HTML rules.
    bool mLocalized = false;
  };
  virtual StringToNumberResult ConvertStringToNumber(
      const nsAString& aValue) const;

  /**
   * Convert a Decimal to a string in a type specific way, ie convert a
   * timestamp to a date string if type=date or append the number string
   * representing the value if type=number.
   *
   * @param aValue the Decimal to be converted
   * @param aResultString [out] the string representing the Decimal
   * @return whether the function succeeded, it will fail if the current input's
   *         type is not supported or the number can't be converted to a string
   *         as expected by the type.
   */
  virtual bool ConvertNumberToString(Decimal aValue,
                                     nsAString& aResultString) const;

 protected:
  explicit InputType(HTMLInputElement* aInputElement)
      : mInputElement(aInputElement) {}

  /**
   * Get the mutable state of the element.
   * When the element isn't mutable (immutable), the value or checkedness
   * should not be changed by the user.
   *
   * See:
   * https://html.spec.whatwg.org/multipage/forms.html#the-input-element:concept-fe-mutable
   */
  virtual bool IsMutable() const;

  /**
   * Returns whether the input element's current value is the empty string.
   * This only makes sense for some input types; does NOT make sense for file
   * inputs.
   *
   * @return whether the input element's current value is the empty string.
   */
  bool IsValueEmpty() const;

  // A getter for callers that know we're not dealing with a file input, so they
  // don't have to think about the caller type.
  void GetNonFileValueInternal(nsAString& aValue) const;

  /**
   * Setting the input element's value.
   *
   * @param aValue      String to set.
   * @param aOptions    See TextControlState::ValueSetterOption.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  SetValueInternal(const nsAString& aValue, const ValueSetterOptions& aOptions);

  /**
   * Get the primary frame for the input element.
   */
  nsIFrame* GetPrimaryFrame() const;

  /**
   * Parse a date string of the form yyyy-mm-dd
   *
   * @param aValue the string to be parsed.
   * @return the date in aYear, aMonth, aDay.
   * @return whether the parsing was successful.
   */
  bool ParseDate(const nsAString& aValue, uint32_t* aYear, uint32_t* aMonth,
                 uint32_t* aDay) const;

  /**
   * Returns the time expressed in milliseconds of |aValue| being parsed as a
   * time following the HTML specifications:
   * https://html.spec.whatwg.org/multipage/infrastructure.html#parse-a-time-string
   *
   * Note: |aResult| can be null.
   *
   * @param aValue the string to be parsed.
   * @param aResult the time expressed in milliseconds representing the time
   * [out]
   * @return whether the parsing was successful.
   */
  bool ParseTime(const nsAString& aValue, uint32_t* aResult) const;

  /**
   * Parse a month string of the form yyyy-mm
   *
   * @param the string to be parsed.
   * @return the year and month in aYear and aMonth.
   * @return whether the parsing was successful.
   */
  bool ParseMonth(const nsAString& aValue, uint32_t* aYear,
                  uint32_t* aMonth) const;

  /**
   * Parse a week string of the form yyyy-Www
   *
   * @param the string to be parsed.
   * @return the year and week in aYear and aWeek.
   * @return whether the parsing was successful.
   */
  bool ParseWeek(const nsAString& aValue, uint32_t* aYear,
                 uint32_t* aWeek) const;

  /**
   * Parse a datetime-local string of the form yyyy-mm-ddThh:mm[:ss.s] or
   * yyyy-mm-dd hh:mm[:ss.s], where fractions of seconds can be 1 to 3 digits.
   *
   * @param the string to be parsed.
   * @return the date in aYear, aMonth, aDay and time expressed in milliseconds
   *         in aTime.
   * @return whether the parsing was successful.
   */
  bool ParseDateTimeLocal(const nsAString& aValue, uint32_t* aYear,
                          uint32_t* aMonth, uint32_t* aDay,
                          uint32_t* aTime) const;

  /**
   * This methods returns the number of months between January 1970 and the
   * given year and month.
   */
  int32_t MonthsSinceJan1970(uint32_t aYear, uint32_t aMonth) const;

  /**
   * This methods returns the number of days since epoch for a given year and
   * week.
   */
  double DaysSinceEpochFromWeek(uint32_t aYear, uint32_t aWeek) const;

  /**
   * This methods returns the day of the week given a date. If @isoWeek is true,
   * 7=Sunday, otherwise, 0=Sunday.
   */
  uint32_t DayOfWeek(uint32_t aYear, uint32_t aMonth, uint32_t aDay,
                     bool isoWeek) const;

  /**
   * This methods returns the maximum number of week in a given year, the
   * result is either 52 or 53.
   */
  uint32_t MaximumWeekInYear(uint32_t aYear) const;

  HTMLInputElement* mInputElement;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_InputType_h__ */
