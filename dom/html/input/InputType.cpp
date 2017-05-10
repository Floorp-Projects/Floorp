/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputType.h"

#include "nsIFormControl.h"
#include "ButtonInputTypes.h"
#include "CheckableInputTypes.h"
#include "ColorInputType.h"
#include "DateTimeInputTypes.h"
#include "FileInputType.h"
#include "HiddenInputType.h"
#include "NumericInputTypes.h"
#include "SingleLineTextInputTypes.h"

const mozilla::Decimal InputType::kStepAny = mozilla::Decimal(0);

/* static */ mozilla::UniquePtr<InputType, DoNotDelete>
InputType::Create(mozilla::dom::HTMLInputElement* aInputElement, uint8_t aType,
                  void* aMemory)
{
  mozilla::UniquePtr<InputType, DoNotDelete> inputType;
  switch(aType) {
    // Single line text
    case NS_FORM_INPUT_TEXT:
      inputType.reset(TextInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_TEL:
      inputType.reset(TelInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_EMAIL:
      inputType.reset(EmailInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_SEARCH:
      inputType.reset(SearchInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_PASSWORD:
      inputType.reset(PasswordInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_URL:
      inputType.reset(URLInputType::Create(aInputElement, aMemory));
      break;
    // Button
    case NS_FORM_INPUT_BUTTON:
      inputType.reset(ButtonInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_SUBMIT:
      inputType.reset(SubmitInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_IMAGE:
      inputType.reset(ImageInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_RESET:
      inputType.reset(ResetInputType::Create(aInputElement, aMemory));
      break;
    // Checkable
    case NS_FORM_INPUT_CHECKBOX:
      inputType.reset(CheckboxInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_RADIO:
      inputType.reset(RadioInputType::Create(aInputElement, aMemory));
      break;
    // Numeric
    case NS_FORM_INPUT_NUMBER:
      inputType.reset(NumberInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_RANGE:
      inputType.reset(RangeInputType::Create(aInputElement, aMemory));
      break;
    // DateTime
    case NS_FORM_INPUT_DATE:
      inputType.reset(DateInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_TIME:
      inputType.reset(TimeInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_MONTH:
      inputType.reset(MonthInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_WEEK:
      inputType.reset(WeekInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_DATETIME_LOCAL:
      inputType.reset(DateTimeLocalInputType::Create(aInputElement, aMemory));
      break;
    // Others
    case NS_FORM_INPUT_COLOR:
      inputType.reset(ColorInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_FILE:
      inputType.reset(FileInputType::Create(aInputElement, aMemory));
      break;
    case NS_FORM_INPUT_HIDDEN:
      inputType.reset(HiddenInputType::Create(aInputElement, aMemory));
      break;
    default:
      inputType.reset(TextInputType::Create(aInputElement, aMemory));
  }

  return inputType;
}

bool
InputType::IsMutable() const
{
  return !mInputElement->IsDisabled();
}

bool
InputType::IsValueEmpty() const
{
  return mInputElement->IsValueEmpty();
}

void
InputType::GetNonFileValueInternal(nsAString& aValue) const
{
  return mInputElement->GetNonFileValueInternal(aValue);
}

nsresult
InputType::SetValueInternal(const nsAString& aValue, uint32_t aFlags)
{
  return mInputElement->SetValueInternal(aValue, aFlags);
}

mozilla::Decimal
InputType::GetStepBase() const
{
  return mInputElement->GetStepBase();
}

nsIFrame*
InputType::GetPrimaryFrame() const
{
  return mInputElement->GetPrimaryFrame();
}

void
InputType::DropReference()
{
  // Drop our (non ref-counted) reference.
  mInputElement = nullptr;
}

bool
InputType::IsTooLong() const
{
  return false;
}

bool
InputType::IsTooShort() const
{
  return false;
}

bool
InputType::IsValueMissing() const
{
  return false;
}

bool
InputType::HasTypeMismatch() const
{
  return false;
}

bool
InputType::HasPatternMismatch() const
{
  return false;
}

bool
InputType::IsRangeOverflow() const
{
  return false;
}

bool
InputType::IsRangeUnderflow() const
{
  return false;
}

bool
InputType::HasStepMismatch(bool aUseZeroIfValueNaN) const
{
  return false;
}

bool
InputType::HasBadInput() const
{
  return false;
}

nsresult
InputType::MinMaxStepAttrChanged()
{
  return NS_OK;
}

bool
InputType::ConvertStringToNumber(nsAString& aValue,
                                 mozilla::Decimal& aResultValue) const
{
  NS_WARNING("InputType::ConvertStringToNumber called");

  return false;
}

bool
InputType::ConvertNumberToString(mozilla::Decimal aValue,
                                 nsAString& aResultString) const
{
  NS_WARNING("InputType::ConvertNumberToString called");

  return false;
}

bool
InputType::ParseDate(const nsAString& aValue, uint32_t* aYear, uint32_t* aMonth,
                     uint32_t* aDay) const
{
  // TODO: move this function and implementation to DateTimeInpuTypeBase when
  // refactoring is completed. Now we can only call HTMLInputElement::ParseDate
  // from here, since the method is protected and only InputType is a friend
  // class.
  return mInputElement->ParseDate(aValue, aYear, aMonth, aDay);
}

bool
InputType::ParseTime(const nsAString& aValue, uint32_t* aResult) const
{
  // see comment in InputType::ParseDate().
  return mInputElement->ParseTime(aValue, aResult);
}

bool
InputType::ParseMonth(const nsAString& aValue, uint32_t* aYear,
                      uint32_t* aMonth) const
{
  // see comment in InputType::ParseDate().
  return mInputElement->ParseMonth(aValue, aYear, aMonth);
}

bool
InputType::ParseWeek(const nsAString& aValue, uint32_t* aYear,
                     uint32_t* aWeek) const
{
  // see comment in InputType::ParseDate().
  return mInputElement->ParseWeek(aValue, aYear, aWeek);
}

bool
InputType::ParseDateTimeLocal(const nsAString& aValue, uint32_t* aYear,
                              uint32_t* aMonth, uint32_t* aDay, uint32_t* aTime)
                              const
{
  // see comment in InputType::ParseDate().
  return mInputElement->ParseDateTimeLocal(aValue, aYear, aMonth, aDay, aTime);
}

int32_t
InputType::MonthsSinceJan1970(uint32_t aYear, uint32_t aMonth) const
{
  // see comment in InputType::ParseDate().
  return mInputElement->MonthsSinceJan1970(aYear, aMonth);
}

double
InputType::DaysSinceEpochFromWeek(uint32_t aYear, uint32_t aWeek) const
{
  // see comment in InputType::ParseDate().
  return mInputElement->DaysSinceEpochFromWeek(aYear, aWeek);
}

uint32_t
InputType::DayOfWeek(uint32_t aYear, uint32_t aMonth, uint32_t aDay,
                     bool isoWeek) const
{
  // see comment in InputType::ParseDate().
  return mInputElement->DayOfWeek(aYear, aMonth, aDay, isoWeek);
}

uint32_t
InputType::MaximumWeekInYear(uint32_t aYear) const
{
  // see comment in InputType::ParseDate().
  return mInputElement->MaximumWeekInYear(aYear);
}
