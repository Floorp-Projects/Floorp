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
#include "mozilla/FloatingPoint.h"

namespace mozilla {
namespace a11y {

////////////////////////////////////////////////////////////////////////////////
// XULSliderAccessible
////////////////////////////////////////////////////////////////////////////////

XULSliderAccessible::
  XULSliderAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  mStateFlags |= eHasNumericValue | eNoXBLKids;
}

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

  dom::Element* sliderElm = GetSliderElement();
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

void
XULSliderAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();
  if (aIndex == 0)
    aName.AssignLiteral("activate");
}

bool
XULSliderAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != 0)
    return false;

  dom::Element* sliderElm = GetSliderElement();
  if (sliderElm)
    DoCommand(sliderElm);

  return true;
}

double
XULSliderAccessible::MaxValue() const
{
  double value = AccessibleWrap::MaxValue();
  return IsNaN(value) ? GetSliderAttr(nsGkAtoms::maxpos) : value;
}

double
XULSliderAccessible::MinValue() const
{
  double value = AccessibleWrap::MinValue();
  return IsNaN(value) ? GetSliderAttr(nsGkAtoms::minpos) : value;
}

double
XULSliderAccessible::Step() const
{
  double value = AccessibleWrap::Step();
  return IsNaN(value) ? GetSliderAttr(nsGkAtoms::increment) : value;
}

double
XULSliderAccessible::CurValue() const
{
  double value = AccessibleWrap::CurValue();
  return IsNaN(value) ? GetSliderAttr(nsGkAtoms::curpos) : value;
}

bool
XULSliderAccessible::SetCurValue(double aValue)
{
  if (AccessibleWrap::SetCurValue(aValue))
    return true;

  return SetSliderAttr(nsGkAtoms::curpos, aValue);
}

// Utils

dom::Element*
XULSliderAccessible::GetSliderElement() const
{
  if (!mSliderElement) {
    // XXX: we depend on anonymous content.
    mSliderElement = mContent->OwnerDoc()->
      GetAnonymousElementByAttribute(mContent, nsGkAtoms::anonid,
                                     NS_LITERAL_STRING("slider"));
  }

  return mSliderElement;
}

nsresult
XULSliderAccessible::GetSliderAttr(nsAtom* aName, nsAString& aValue) const
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
XULSliderAccessible::SetSliderAttr(nsAtom* aName, const nsAString& aValue)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (dom::Element* sliderElm = GetSliderElement())
    sliderElm->SetAttr(kNameSpaceID_None, aName, aValue, true);

  return NS_OK;
}

double
XULSliderAccessible::GetSliderAttr(nsAtom* aName) const
{
  nsAutoString attrValue;
  nsresult rv = GetSliderAttr(aName, attrValue);
  if (NS_FAILED(rv))
    return UnspecifiedNaN<double>();

  nsresult error = NS_OK;
  double value = attrValue.ToDouble(&error);
  return NS_FAILED(error) ? UnspecifiedNaN<double>() : value;
}

bool
XULSliderAccessible::SetSliderAttr(nsAtom* aName, double aValue)
{
  nsAutoString value;
  value.AppendFloat(aValue);

  return NS_SUCCEEDED(SetSliderAttr(aName, value));
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

}
}
