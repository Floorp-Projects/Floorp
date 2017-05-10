/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NumericInputTypes.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "nsNumberControlFrame.h"
#include "nsTextEditorState.h"

bool
NumberInputType::IsMutable() const
{
  return !mInputElement->IsDisabled() &&
         !mInputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly);
}

bool
NumericInputTypeBase::IsRangeOverflow() const
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
NumericInputTypeBase::IsRangeUnderflow() const
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
NumericInputTypeBase::HasStepMismatch(bool aUseZeroIfValueNaN) const
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
NumericInputTypeBase::ConvertStringToNumber(nsAString& aValue,
  mozilla::Decimal& aResultValue) const
{
  aResultValue = mozilla::dom::HTMLInputElement::StringToDecimal(aValue);
  if (!aResultValue.isFinite()) {
    return false;
  }
  return true;
}

bool
NumericInputTypeBase::ConvertNumberToString(mozilla::Decimal aValue,
                                            nsAString& aResultString) const
{
  MOZ_ASSERT(aValue.isFinite(), "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  char buf[32];
  bool ok = aValue.toString(buf, mozilla::ArrayLength(buf));
  aResultString.AssignASCII(buf);
  MOZ_ASSERT(ok, "buf not big enough");

  return ok;
}

/* input type=numer */

bool
NumberInputType::IsValueMissing() const
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
NumberInputType::HasBadInput() const
{
  nsAutoString value;
  GetNonFileValueInternal(value);
  if (!value.IsEmpty()) {
    // The input can't be bad, otherwise it would have been sanitized to the
    // empty string.
    NS_ASSERTION(!mInputElement->GetValueAsDecimal().isNaN(),
                 "Should have sanitized");
    return false;
  }
  nsNumberControlFrame* numberControlFrame =
    do_QueryFrame(GetPrimaryFrame());
  if (numberControlFrame &&
      !numberControlFrame->AnonTextControlIsEmpty()) {
    // The input the user entered failed to parse as a number.
    return true;
  }
  return false;
}

/* input type=range */
nsresult
RangeInputType::MinMaxStepAttrChanged()
{
  // The value may need to change when @min/max/step changes since the value may
  // have been invalid and can now change to a valid value, or vice versa. For
  // example, consider: <input type=range value=-1 max=1 step=3>. The valid
  // range is 0 to 1 while the nearest valid steps are -1 and 2 (the max value
  // having prevented there being a valid step in range). Changing @max to/from
  // 1 and a number greater than on equal to 3 should change whether we have a
  // step mismatch or not.
  // The value may also need to change between a value that results in a step
  // mismatch and a value that results in overflow. For example, if @max in the
  // example above were to change from 1 to -1.
  nsAutoString value;
  GetNonFileValueInternal(value);
  return SetValueInternal(value, nsTextEditorState::eSetValue_Internal);
}
