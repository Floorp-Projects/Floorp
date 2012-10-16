/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXFormsFormControlsAccessible.h"

#include "nsTextEquivUtils.h"
#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXFormsLabelAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsLabelAccessible::
  nsXFormsLabelAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsLabelAccessible::NativeRole()
{
  return roles::LABEL;
}

ENameValueFlag
nsXFormsLabelAccessible::NativeName(nsString& aName)
{
  // XXX Correct name calculation for this, see bug 453594.
  return eNameOK;
}

void
nsXFormsLabelAccessible::Description(nsString& aDescription)
{
  nsTextEquivUtils::
    GetTextEquivFromIDRefs(this, nsGkAtoms::aria_describedby,
                           aDescription);
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsOutputAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsOutputAccessible::
  nsXFormsOutputAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsOutputAccessible::NativeRole()
{
  return roles::STATICTEXT;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsTriggerAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsTriggerAccessible::
  nsXFormsTriggerAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsTriggerAccessible::NativeRole()
{
  return roles::PUSHBUTTON;
}

void
nsXFormsTriggerAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
}

uint8_t
nsXFormsTriggerAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXFormsTriggerAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("press");
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsXFormsTriggerAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsInputAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsInputAccessible::
  nsXFormsInputAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsEditableAccessible(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsInputAccessible,
                             Accessible,
                             nsIAccessibleText,
                             nsIAccessibleEditableText)

role
nsXFormsInputAccessible::NativeRole()
{
  return roles::ENTRY;
}

uint8_t
nsXFormsInputAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXFormsInputAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("activate");
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  return sXFormsService->Focus(DOMNode);
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsInputBooleanAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsInputBooleanAccessible::
  nsXFormsInputBooleanAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsInputBooleanAccessible::NativeRole()
{
  return roles::CHECKBUTTON;
}

uint64_t
nsXFormsInputBooleanAccessible::NativeState()
{
  uint64_t state = nsXFormsAccessible::NativeState();

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->GetValue(DOMNode, value);
  NS_ENSURE_SUCCESS(rv, state);

  if (value.EqualsLiteral("true"))
    state |= states::CHECKED;

  return state;
}

uint8_t
nsXFormsInputBooleanAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXFormsInputBooleanAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->GetValue(DOMNode, value);
  NS_ENSURE_SUCCESS(rv, rv);

  if (value.EqualsLiteral("true"))
    aName.AssignLiteral("uncheck");
  else
    aName.AssignLiteral("check");

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputBooleanAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsInputDateAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsInputDateAccessible::
  nsXFormsInputDateAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsContainerAccessible(aContent, aDoc)
{
}

role
nsXFormsInputDateAccessible::NativeRole()
{
  return roles::DROPLIST;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSecretAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSecretAccessible::
  nsXFormsSecretAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsInputAccessible(aContent, aDoc)
{
}

role
nsXFormsSecretAccessible::NativeRole()
{
  return roles::PASSWORD_TEXT;
}

uint64_t
nsXFormsSecretAccessible::NativeState()
{
  return nsXFormsInputAccessible::NativeState() | states::PROTECTED;
}

void
nsXFormsSecretAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsRangeAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsRangeAccessible::
  nsXFormsRangeAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsRangeAccessible::NativeRole()
{
  return roles::SLIDER;
}

uint64_t
nsXFormsRangeAccessible::NativeState()
{
  uint64_t state = nsXFormsAccessible::NativeState();

  uint32_t isInRange = nsIXFormsUtilityService::STATE_NOT_A_RANGE;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->IsInRange(DOMNode, &isInRange);
  NS_ENSURE_SUCCESS(rv, state);

  if (isInRange == nsIXFormsUtilityService::STATE_OUT_OF_RANGE)
    state |= states::INVALID;

  return state;
}

NS_IMETHODIMP
nsXFormsRangeAccessible::GetMaximumValue(double *aMaximumValue)
{
  NS_ENSURE_ARG_POINTER(aMaximumValue);

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->GetRangeEnd(DOMNode, value);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult error = NS_OK;
  *aMaximumValue = value.ToDouble(&error);
  return error;
}

NS_IMETHODIMP
nsXFormsRangeAccessible::GetMinimumValue(double *aMinimumValue)
{
  NS_ENSURE_ARG_POINTER(aMinimumValue);

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->GetRangeStart(DOMNode, value);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult error = NS_OK;
  *aMinimumValue = value.ToDouble(&error);
  return error;
}

NS_IMETHODIMP
nsXFormsRangeAccessible::GetMinimumIncrement(double *aMinimumIncrement)
{
  NS_ENSURE_ARG_POINTER(aMinimumIncrement);

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->GetRangeStep(DOMNode, value);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult error = NS_OK;
  *aMinimumIncrement = value.ToDouble(&error);
  return error;
}

NS_IMETHODIMP
nsXFormsRangeAccessible::GetCurrentValue(double *aCurrentValue)
{
  NS_ENSURE_ARG_POINTER(aCurrentValue);

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->GetValue(DOMNode, value);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult error = NS_OK;
  *aCurrentValue = value.ToDouble(&error);
  return error;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectAccessible::
  nsXFormsSelectAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsContainerAccessible(aContent, aDoc)
{
}

uint64_t
nsXFormsSelectAccessible::NativeState()
{
  uint64_t state = nsXFormsContainerAccessible::NativeState();

  uint32_t isInRange = nsIXFormsUtilityService::STATE_NOT_A_RANGE;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->IsInRange(DOMNode, &isInRange);
  NS_ENSURE_SUCCESS(rv, state);

  if (isInRange == nsIXFormsUtilityService::STATE_OUT_OF_RANGE)
    state |= states::INVALID;

  return state;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsChoicesAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsChoicesAccessible::
  nsXFormsChoicesAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsChoicesAccessible::NativeRole()
{
  return roles::GROUPING;
}

void
nsXFormsChoicesAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
}

void
nsXFormsChoicesAccessible::CacheChildren()
{
  CacheSelectChildren();
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectFullAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectFullAccessible::
  nsXFormsSelectFullAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsSelectableAccessible(aContent, aDoc)
{
}

role
nsXFormsSelectFullAccessible::NativeRole()
{
  return roles::GROUPING;
}

void
nsXFormsSelectFullAccessible::CacheChildren()
{
  CacheSelectChildren();
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsItemCheckgroupAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsItemCheckgroupAccessible::
  nsXFormsItemCheckgroupAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsSelectableItemAccessible(aContent, aDoc)
{
}

role
nsXFormsItemCheckgroupAccessible::NativeRole()
{
  return roles::CHECKBUTTON;
}

uint64_t
nsXFormsItemCheckgroupAccessible::NativeState()
{
  uint64_t state = nsXFormsSelectableItemAccessible::NativeState();

  if (IsSelected())
    state |= states::CHECKED;

  return state;
}

NS_IMETHODIMP
nsXFormsItemCheckgroupAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  if (IsSelected())
    aName.AssignLiteral("uncheck");
  else
    aName.AssignLiteral("check");

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsItemRadiogroupAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsItemRadiogroupAccessible::
  nsXFormsItemRadiogroupAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsSelectableItemAccessible(aContent, aDoc)
{
}

role
nsXFormsItemRadiogroupAccessible::NativeRole()
{
  return roles::RADIOBUTTON;
}

uint64_t
nsXFormsItemRadiogroupAccessible::NativeState()
{
  uint64_t state = nsXFormsSelectableItemAccessible::NativeState();

  if (IsSelected())
    state |= states::CHECKED;

  return state;
}

NS_IMETHODIMP
nsXFormsItemRadiogroupAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("select");
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectComboboxAccessible::
  nsXFormsSelectComboboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsSelectableAccessible(aContent, aDoc)
{
}

role
nsXFormsSelectComboboxAccessible::NativeRole()
{
  return roles::COMBOBOX;
}

uint64_t
nsXFormsSelectComboboxAccessible::NativeState()
{
  uint64_t state = nsXFormsSelectableAccessible::NativeState();

  bool isOpen = false;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->IsDropmarkerOpen(DOMNode, &isOpen);
  NS_ENSURE_SUCCESS(rv, state);

  if (isOpen)
    state |= states::EXPANDED;
  else
    state |= states::COLLAPSED;

  return state | states::HASPOPUP;
}

uint64_t
nsXFormsSelectComboboxAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ? states::UNAVAILABLE : states::FOCUSABLE;
}

bool
nsXFormsSelectComboboxAccessible::CanHaveAnonChildren()
{
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsItemComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsItemComboboxAccessible::
  nsXFormsItemComboboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXFormsSelectableItemAccessible(aContent, aDoc)
{
}

role
nsXFormsItemComboboxAccessible::NativeRole()
{
  return roles::LISTITEM;
}

uint64_t
nsXFormsItemComboboxAccessible::NativeState()
{
  uint64_t state = nsXFormsSelectableItemAccessible::NativeState();
  if (IsSelected())
    state |= states::SELECTED;

  return state;
}

uint64_t
nsXFormsItemComboboxAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ?
    states::UNAVAILABLE : states::FOCUSABLE | states::SELECTABLE;
}

NS_IMETHODIMP
nsXFormsItemComboboxAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("select");
  return NS_OK;
}

