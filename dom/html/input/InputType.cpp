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
