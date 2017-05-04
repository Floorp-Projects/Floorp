/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateTimeInputTypes.h"

#include "mozilla/dom/HTMLInputElement.h"

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
