/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InputType.h"

#include "nsIFormControl.h"
#include "mozilla/dom/ButtonInputTypes.h"
#include "mozilla/dom/CheckableInputTypes.h"
#include "mozilla/dom/ColorInputType.h"
#include "mozilla/dom/DateTimeInputTypes.h"
#include "mozilla/dom/FileInputType.h"
#include "mozilla/dom/HiddenInputType.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/NumericInputTypes.h"
#include "mozilla/dom/SingleLineTextInputTypes.h"

#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

const Decimal InputType::kStepAny = Decimal(0);

/* static */ UniquePtr<InputType, InputType::DoNotDelete> InputType::Create(
    HTMLInputElement* aInputElement, uint8_t aType, void* aMemory) {
  UniquePtr<InputType, InputType::DoNotDelete> inputType;
  switch (aType) {
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

bool InputType::IsMutable() const { return !mInputElement->IsDisabled(); }

bool InputType::IsValueEmpty() const { return mInputElement->IsValueEmpty(); }

void InputType::GetNonFileValueInternal(nsAString& aValue) const {
  return mInputElement->GetNonFileValueInternal(aValue);
}

nsresult InputType::SetValueInternal(const nsAString& aValue, uint32_t aFlags) {
  RefPtr<HTMLInputElement> inputElement(mInputElement);
  return inputElement->SetValueInternal(aValue, aFlags);
}

Decimal InputType::GetStepBase() const { return mInputElement->GetStepBase(); }

nsIFrame* InputType::GetPrimaryFrame() const {
  return mInputElement->GetPrimaryFrame();
}

void InputType::DropReference() {
  // Drop our (non ref-counted) reference.
  mInputElement = nullptr;
}

bool InputType::IsTooLong() const { return false; }

bool InputType::IsTooShort() const { return false; }

bool InputType::IsValueMissing() const { return false; }

bool InputType::HasTypeMismatch() const { return false; }

Maybe<bool> InputType::HasPatternMismatch() const { return Some(false); }

bool InputType::IsRangeOverflow() const { return false; }

bool InputType::IsRangeUnderflow() const { return false; }

bool InputType::HasStepMismatch(bool aUseZeroIfValueNaN) const { return false; }

bool InputType::HasBadInput() const { return false; }

nsresult InputType::GetValidationMessage(
    nsAString& aValidationMessage,
    nsIConstraintValidation::ValidityStateType aType) {
  nsresult rv = NS_OK;

  switch (aType) {
    case nsIConstraintValidation::VALIDITY_STATE_TOO_LONG: {
      nsAutoString message;
      int32_t maxLength = mInputElement->MaxLength();
      int32_t textLength = mInputElement->InputTextLength(CallerType::System);
      nsAutoString strMaxLength;
      nsAutoString strTextLength;

      strMaxLength.AppendInt(maxLength);
      strTextLength.AppendInt(textLength);

      rv = nsContentUtils::FormatMaybeLocalizedString(
          message, nsContentUtils::eDOM_PROPERTIES, "FormValidationTextTooLong",
          mInputElement->OwnerDoc(), strMaxLength, strTextLength);
      aValidationMessage = message;
      break;
    }
    case nsIConstraintValidation::VALIDITY_STATE_TOO_SHORT: {
      nsAutoString message;
      int32_t minLength = mInputElement->MinLength();
      int32_t textLength = mInputElement->InputTextLength(CallerType::System);
      nsAutoString strMinLength;
      nsAutoString strTextLength;

      strMinLength.AppendInt(minLength);
      strTextLength.AppendInt(textLength);

      rv = nsContentUtils::FormatMaybeLocalizedString(
          message, nsContentUtils::eDOM_PROPERTIES,
          "FormValidationTextTooShort", mInputElement->OwnerDoc(), strMinLength,
          strTextLength);

      aValidationMessage = message;
      break;
    }
    case nsIConstraintValidation::VALIDITY_STATE_VALUE_MISSING: {
      nsAutoString message;
      rv = GetValueMissingMessage(message);
      if (NS_FAILED(rv)) {
        return rv;
      }

      aValidationMessage = message;
      break;
    }
    case nsIConstraintValidation::VALIDITY_STATE_TYPE_MISMATCH: {
      nsAutoString message;
      rv = GetTypeMismatchMessage(message);
      if (NS_FAILED(rv)) {
        return rv;
      }

      aValidationMessage = message;
      break;
    }
    case nsIConstraintValidation::VALIDITY_STATE_PATTERN_MISMATCH: {
      nsAutoString message;
      nsAutoString title;
      mInputElement->GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
      if (title.IsEmpty()) {
        rv = nsContentUtils::GetMaybeLocalizedString(
            nsContentUtils::eDOM_PROPERTIES, "FormValidationPatternMismatch",
            mInputElement->OwnerDoc(), message);
      } else {
        if (title.Length() >
            nsIConstraintValidation::sContentSpecifiedMaxLengthMessage) {
          title.Truncate(
              nsIConstraintValidation::sContentSpecifiedMaxLengthMessage);
        }
        rv = nsContentUtils::FormatMaybeLocalizedString(
            message, nsContentUtils::eDOM_PROPERTIES,
            "FormValidationPatternMismatchWithTitle", mInputElement->OwnerDoc(),
            title);
      }
      aValidationMessage = message;
      break;
    }
    case nsIConstraintValidation::VALIDITY_STATE_RANGE_OVERFLOW: {
      nsAutoString message;
      rv = GetRangeOverflowMessage(message);
      if (NS_FAILED(rv)) {
        return rv;
      }

      aValidationMessage = message;
      break;
    }
    case nsIConstraintValidation::VALIDITY_STATE_RANGE_UNDERFLOW: {
      nsAutoString message;
      rv = GetRangeUnderflowMessage(message);
      if (NS_FAILED(rv)) {
        return rv;
      }

      aValidationMessage = message;
      break;
    }
    case nsIConstraintValidation::VALIDITY_STATE_STEP_MISMATCH: {
      nsAutoString message;

      Decimal value = mInputElement->GetValueAsDecimal();
      MOZ_ASSERT(!value.isNaN());

      Decimal step = mInputElement->GetStep();
      MOZ_ASSERT(step != kStepAny && step > Decimal(0));

      Decimal stepBase = mInputElement->GetStepBase();

      Decimal valueLow = value - NS_floorModulo(value - stepBase, step);
      Decimal valueHigh = value + step - NS_floorModulo(value - stepBase, step);

      Decimal maximum = mInputElement->GetMaximum();

      if (maximum.isNaN() || valueHigh <= maximum) {
        nsAutoString valueLowStr, valueHighStr;
        ConvertNumberToString(valueLow, valueLowStr);
        ConvertNumberToString(valueHigh, valueHighStr);

        if (valueLowStr.Equals(valueHighStr)) {
          rv = nsContentUtils::FormatMaybeLocalizedString(
              message, nsContentUtils::eDOM_PROPERTIES,
              "FormValidationStepMismatchOneValue", mInputElement->OwnerDoc(),
              valueLowStr);
        } else {
          rv = nsContentUtils::FormatMaybeLocalizedString(
              message, nsContentUtils::eDOM_PROPERTIES,
              "FormValidationStepMismatch", mInputElement->OwnerDoc(),
              valueLowStr, valueHighStr);
        }
      } else {
        nsAutoString valueLowStr;
        ConvertNumberToString(valueLow, valueLowStr);

        rv = nsContentUtils::FormatMaybeLocalizedString(
            message, nsContentUtils::eDOM_PROPERTIES,
            "FormValidationStepMismatchOneValue", mInputElement->OwnerDoc(),
            valueLowStr);
      }

      aValidationMessage = message;
      break;
    }
    case nsIConstraintValidation::VALIDITY_STATE_BAD_INPUT: {
      nsAutoString message;
      rv = GetBadInputMessage(message);
      if (NS_FAILED(rv)) {
        return rv;
      }

      aValidationMessage = message;
      break;
    }
    default:
      return NS_ERROR_UNEXPECTED;
  }

  return rv;
}

nsresult InputType::GetValueMissingMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationValueMissing",
      mInputElement->OwnerDoc(), aMessage);
}

nsresult InputType::GetTypeMismatchMessage(nsAString& aMessage) {
  return NS_ERROR_UNEXPECTED;
}

nsresult InputType::GetRangeOverflowMessage(nsAString& aMessage) {
  return NS_ERROR_UNEXPECTED;
}

nsresult InputType::GetRangeUnderflowMessage(nsAString& aMessage) {
  return NS_ERROR_UNEXPECTED;
}

nsresult InputType::GetBadInputMessage(nsAString& aMessage) {
  return NS_ERROR_UNEXPECTED;
}

nsresult InputType::MinMaxStepAttrChanged() { return NS_OK; }

bool InputType::ConvertStringToNumber(nsAString& aValue,
                                      Decimal& aResultValue) const {
  NS_WARNING("InputType::ConvertStringToNumber called");

  return false;
}

bool InputType::ConvertNumberToString(Decimal aValue,
                                      nsAString& aResultString) const {
  NS_WARNING("InputType::ConvertNumberToString called");

  return false;
}

bool InputType::ParseDate(const nsAString& aValue, uint32_t* aYear,
                          uint32_t* aMonth, uint32_t* aDay) const {
  // TODO: move this function and implementation to DateTimeInpuTypeBase when
  // refactoring is completed. Now we can only call HTMLInputElement::ParseDate
  // from here, since the method is protected and only InputType is a friend
  // class.
  return mInputElement->ParseDate(aValue, aYear, aMonth, aDay);
}

bool InputType::ParseTime(const nsAString& aValue, uint32_t* aResult) const {
  // see comment in InputType::ParseDate().
  return HTMLInputElement::ParseTime(aValue, aResult);
}

bool InputType::ParseMonth(const nsAString& aValue, uint32_t* aYear,
                           uint32_t* aMonth) const {
  // see comment in InputType::ParseDate().
  return mInputElement->ParseMonth(aValue, aYear, aMonth);
}

bool InputType::ParseWeek(const nsAString& aValue, uint32_t* aYear,
                          uint32_t* aWeek) const {
  // see comment in InputType::ParseDate().
  return mInputElement->ParseWeek(aValue, aYear, aWeek);
}

bool InputType::ParseDateTimeLocal(const nsAString& aValue, uint32_t* aYear,
                                   uint32_t* aMonth, uint32_t* aDay,
                                   uint32_t* aTime) const {
  // see comment in InputType::ParseDate().
  return mInputElement->ParseDateTimeLocal(aValue, aYear, aMonth, aDay, aTime);
}

int32_t InputType::MonthsSinceJan1970(uint32_t aYear, uint32_t aMonth) const {
  // see comment in InputType::ParseDate().
  return mInputElement->MonthsSinceJan1970(aYear, aMonth);
}

double InputType::DaysSinceEpochFromWeek(uint32_t aYear, uint32_t aWeek) const {
  // see comment in InputType::ParseDate().
  return mInputElement->DaysSinceEpochFromWeek(aYear, aWeek);
}

uint32_t InputType::DayOfWeek(uint32_t aYear, uint32_t aMonth, uint32_t aDay,
                              bool isoWeek) const {
  // see comment in InputType::ParseDate().
  return mInputElement->DayOfWeek(aYear, aMonth, aDay, isoWeek);
}

uint32_t InputType::MaximumWeekInYear(uint32_t aYear) const {
  // see comment in InputType::ParseDate().
  return mInputElement->MaximumWeekInYear(aYear);
}
