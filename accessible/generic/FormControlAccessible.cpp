/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: alphabetically ordered

#include "FormControlAccessible.h"
#include "Role.h"

#include "mozilla/FloatingPoint.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULControlElement.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// ProgressMeterAccessible
////////////////////////////////////////////////////////////////////////////////

template class mozilla::a11y::ProgressMeterAccessible<1>;
template class mozilla::a11y::ProgressMeterAccessible<100>;

////////////////////////////////////////////////////////////////////////////////
// Accessible

template<int Max>
role
ProgressMeterAccessible<Max>::NativeRole()
{
  return roles::PROGRESSBAR;
}

template<int Max>
uint64_t
ProgressMeterAccessible<Max>::NativeState()
{
  uint64_t state = LeafAccessible::NativeState();

  // An undetermined progressbar (i.e. without a value) has a mixed state.
  nsAutoString attrValue;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, attrValue);

  if (attrValue.IsEmpty())
    state |= states::MIXED;

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// ProgressMeterAccessible<Max>: Widgets

template<int Max>
bool
ProgressMeterAccessible<Max>::IsWidget() const
{
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ProgressMeterAccessible<Max>: Value

template<int Max>
void
ProgressMeterAccessible<Max>::Value(nsString& aValue)
{
  LeafAccessible::Value(aValue);
  if (!aValue.IsEmpty())
    return;

  double maxValue = MaxValue();
  if (IsNaN(maxValue) || maxValue == 0)
    return;

  double curValue = CurValue();
  if (IsNaN(curValue))
    return;

  // Treat the current value bigger than maximum as 100%.
  double percentValue = (curValue < maxValue) ?
    (curValue / maxValue) * 100 : 100;

  aValue.AppendFloat(percentValue);
  aValue.Append('%');
}

template<int Max>
double
ProgressMeterAccessible<Max>::MaxValue() const
{
  double value = LeafAccessible::MaxValue();
  if (!IsNaN(value))
    return value;

  nsAutoString strValue;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::max, strValue)) {
    nsresult result = NS_OK;
    value = strValue.ToDouble(&result);
    if (NS_SUCCEEDED(result))
      return value;
  }

  return Max;
}

template<int Max>
double
ProgressMeterAccessible<Max>::MinValue() const
{
  double value = LeafAccessible::MinValue();
  return IsNaN(value) ? 0 : value;
}

template<int Max>
double
ProgressMeterAccessible<Max>::Step() const
{
  double value = LeafAccessible::Step();
  return IsNaN(value) ? 0 : value;
}

template<int Max>
double
ProgressMeterAccessible<Max>::CurValue() const
{
  double value = LeafAccessible::CurValue();
  if (!IsNaN(value))
    return value;

  nsAutoString attrValue;
  if (!mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, attrValue))
    return UnspecifiedNaN<double>();

  nsresult error = NS_OK;
  value = attrValue.ToDouble(&error);
  return NS_FAILED(error) ? UnspecifiedNaN<double>() : value;
}

template<int Max>
bool
ProgressMeterAccessible<Max>::SetCurValue(double aValue)
{
  return false; // progress meters are readonly.
}

////////////////////////////////////////////////////////////////////////////////
// RadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

RadioButtonAccessible::
  RadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

uint8_t
RadioButtonAccessible::ActionCount()
{
  return 1;
}

void
RadioButtonAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click)
    aName.AssignLiteral("select");
}

bool
RadioButtonAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click)
    return false;

  DoCommand();
  return true;
}

role
RadioButtonAccessible::NativeRole()
{
  return roles::RADIOBUTTON;
}

////////////////////////////////////////////////////////////////////////////////
// RadioButtonAccessible: Widgets

bool
RadioButtonAccessible::IsWidget() const
{
  return true;
}
