/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULSliderAccessible.h"

#include "nsAccessibilityService.h"
#include "Role.h"
#include "States.h"

#include "nsIFrame.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULSliderAccessible
////////////////////////////////////////////////////////////////////////////////

XULSliderAccessible::
  XULSliderAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  mStateFlags |= eHasNumericValue;
}

// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(XULSliderAccessible,
                             AccessibleWrap,
                             nsIAccessibleValue)

// Accessible

role
XULSliderAccessible::NativeRole()
{
  return roles::SLIDER;
}

uint64_t
XULSliderAccessible::NativeInteractiveState() const
 {
  if (NativelyUnavailable())
    return states::UNAVAILABLE;

  nsIContent* sliderElm = GetSliderElement();
  if (sliderElm) {
    nsIFrame* frame = sliderElm->GetPrimaryFrame();
    if (frame && frame->IsFocusable())
      return states::FOCUSABLE;
  }

  return 0;
}

bool
XULSliderAccessible::NativelyUnavailable() const
{
  return mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                               nsGkAtoms::_true, eCaseMatters);
}

// nsIAccessible

void
XULSliderAccessible::Value(nsString& aValue)
{
  GetSliderAttr(nsGkAtoms::curpos, aValue);
}

uint8_t
XULSliderAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
XULSliderAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();

  NS_ENSURE_ARG(aIndex == 0);

  aName.AssignLiteral("activate"); 
  return NS_OK;
}

NS_IMETHODIMP
XULSliderAccessible::DoAction(uint8_t aIndex)
{
  NS_ENSURE_ARG(aIndex == 0);

  nsIContent* sliderElm = GetSliderElement();
  if (sliderElm)
    DoCommand(sliderElm);

  return NS_OK;
}

// nsIAccessibleValue

NS_IMETHODIMP
XULSliderAccessible::GetMaximumValue(double* aValue)
{
  nsresult rv = AccessibleWrap::GetMaximumValue(aValue);

  // ARIA redefined maximum value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return GetSliderAttr(nsGkAtoms::maxpos, aValue);
}

NS_IMETHODIMP
XULSliderAccessible::GetMinimumValue(double* aValue)
{
  nsresult rv = AccessibleWrap::GetMinimumValue(aValue);

  // ARIA redefined minmum value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return GetSliderAttr(nsGkAtoms::minpos, aValue);
}

NS_IMETHODIMP
XULSliderAccessible::GetMinimumIncrement(double* aValue)
{
  nsresult rv = AccessibleWrap::GetMinimumIncrement(aValue);

  // ARIA redefined minimum increment value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return GetSliderAttr(nsGkAtoms::increment, aValue);
}

NS_IMETHODIMP
XULSliderAccessible::GetCurrentValue(double* aValue)
{
  nsresult rv = AccessibleWrap::GetCurrentValue(aValue);

  // ARIA redefined current value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return GetSliderAttr(nsGkAtoms::curpos, aValue);
}

NS_IMETHODIMP
XULSliderAccessible::SetCurrentValue(double aValue)
{
  nsresult rv = AccessibleWrap::SetCurrentValue(aValue);

  // ARIA redefined current value.
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  return SetSliderAttr(nsGkAtoms::curpos, aValue);
}

bool
XULSliderAccessible::CanHaveAnonChildren()
{
  // Do not allow anonymous xul:slider be accessible.
  return false;
}

// Utils

nsIContent*
XULSliderAccessible::GetSliderElement() const
{
  if (!mSliderNode) {
    // XXX: we depend on anonymous content.
    mSliderNode = mContent->OwnerDoc()->
      GetAnonymousElementByAttribute(mContent, nsGkAtoms::anonid,
                                     NS_LITERAL_STRING("slider"));
  }

  return mSliderNode;
}

nsresult
XULSliderAccessible::GetSliderAttr(nsIAtom* aName, nsAString& aValue)
{
  aValue.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsIContent* sliderElm = GetSliderElement();
  if (sliderElm)
    sliderElm->GetAttr(kNameSpaceID_None, aName, aValue);

  return NS_OK;
}

nsresult
XULSliderAccessible::SetSliderAttr(nsIAtom* aName, const nsAString& aValue)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsIContent* sliderElm = GetSliderElement();
  if (sliderElm)
    sliderElm->SetAttr(kNameSpaceID_None, aName, aValue, true);

  return NS_OK;
}

nsresult
XULSliderAccessible::GetSliderAttr(nsIAtom* aName, double* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  nsAutoString attrValue;
  nsresult rv = GetSliderAttr(aName, attrValue);
  NS_ENSURE_SUCCESS(rv, rv);

  // Return zero value if there is no attribute or its value is empty.
  if (attrValue.IsEmpty())
    return NS_OK;

  nsresult error = NS_OK;
  double value = attrValue.ToDouble(&error);
  if (NS_SUCCEEDED(error))
    *aValue = value;

  return NS_OK;
}

nsresult
XULSliderAccessible::SetSliderAttr(nsIAtom* aName, double aValue)
{
  nsAutoString value;
  value.AppendFloat(aValue);

  return SetSliderAttr(aName, value);
}


////////////////////////////////////////////////////////////////////////////////
// XULThumbAccessible
////////////////////////////////////////////////////////////////////////////////

XULThumbAccessible::
  XULThumbAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// XULThumbAccessible: Accessible

role
XULThumbAccessible::NativeRole()
{
  return roles::INDICATOR;
}

