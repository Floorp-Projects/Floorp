/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXFormsFormControlsAccessible.h"

#include "nsTextEquivUtils.h"
#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXFormsLabelAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsLabelAccessible::
  nsXFormsLabelAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsLabelAccessible::NativeRole()
{
  return roles::LABEL;
}

nsresult
nsXFormsLabelAccessible::GetNameInternal(nsAString& aName)
{
  // XXX Correct name calculation for this, see bug 453594.
  return NS_OK;
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
  nsXFormsOutputAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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
  nsXFormsTriggerAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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

PRUint8
nsXFormsTriggerAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXFormsTriggerAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("press");
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsXFormsTriggerAccessible::DoAction(PRUint8 aIndex)
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
  nsXFormsInputAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsEditableAccessible(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsInputAccessible, nsAccessible, nsHyperTextAccessible, nsIAccessibleText, nsIAccessibleEditableText)

role
nsXFormsInputAccessible::NativeRole()
{
  return roles::ENTRY;
}

PRUint8
nsXFormsInputAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXFormsInputAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("activate");
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputAccessible::DoAction(PRUint8 aIndex)
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
  nsXFormsInputBooleanAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsInputBooleanAccessible::NativeRole()
{
  return roles::CHECKBUTTON;
}

PRUint64
nsXFormsInputBooleanAccessible::NativeState()
{
  PRUint64 state = nsXFormsAccessible::NativeState();

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->GetValue(DOMNode, value);
  NS_ENSURE_SUCCESS(rv, state);

  if (value.EqualsLiteral("true"))
    state |= states::CHECKED;

  return state;
}

PRUint8
nsXFormsInputBooleanAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXFormsInputBooleanAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
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
nsXFormsInputBooleanAccessible::DoAction(PRUint8 aIndex)
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
  nsXFormsInputDateAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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
  nsXFormsSecretAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsInputAccessible(aContent, aDoc)
{
}

role
nsXFormsSecretAccessible::NativeRole()
{
  return roles::PASSWORD_TEXT;
}

PRUint64
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
  nsXFormsRangeAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsAccessible(aContent, aDoc)
{
}

role
nsXFormsRangeAccessible::NativeRole()
{
  return roles::SLIDER;
}

PRUint64
nsXFormsRangeAccessible::NativeState()
{
  PRUint64 state = nsXFormsAccessible::NativeState();

  PRUint32 isInRange = nsIXFormsUtilityService::STATE_NOT_A_RANGE;
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

  PRInt32 error = NS_OK;
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

  PRInt32 error = NS_OK;
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

  PRInt32 error = NS_OK;
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

  PRInt32 error = NS_OK;
  *aCurrentValue = value.ToDouble(&error);
  return error;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectAccessible::
  nsXFormsSelectAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsContainerAccessible(aContent, aDoc)
{
}

PRUint64
nsXFormsSelectAccessible::NativeState()
{
  PRUint64 state = nsXFormsContainerAccessible::NativeState();

  PRUint32 isInRange = nsIXFormsUtilityService::STATE_NOT_A_RANGE;
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
  nsXFormsChoicesAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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
  nsXFormsSelectFullAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
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
  nsXFormsItemCheckgroupAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsSelectableItemAccessible(aContent, aDoc)
{
}

role
nsXFormsItemCheckgroupAccessible::NativeRole()
{
  return roles::CHECKBUTTON;
}

PRUint64
nsXFormsItemCheckgroupAccessible::NativeState()
{
  PRUint64 state = nsXFormsSelectableItemAccessible::NativeState();

  if (IsSelected())
    state |= states::CHECKED;

  return state;
}

NS_IMETHODIMP
nsXFormsItemCheckgroupAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
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
  nsXFormsItemRadiogroupAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsSelectableItemAccessible(aContent, aDoc)
{
}

role
nsXFormsItemRadiogroupAccessible::NativeRole()
{
  return roles::RADIOBUTTON;
}

PRUint64
nsXFormsItemRadiogroupAccessible::NativeState()
{
  PRUint64 state = nsXFormsSelectableItemAccessible::NativeState();

  if (IsSelected())
    state |= states::CHECKED;

  return state;
}

NS_IMETHODIMP
nsXFormsItemRadiogroupAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
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
  nsXFormsSelectComboboxAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsSelectableAccessible(aContent, aDoc)
{
}

role
nsXFormsSelectComboboxAccessible::NativeRole()
{
  return roles::COMBOBOX;
}

PRUint64
nsXFormsSelectComboboxAccessible::NativeState()
{
  PRUint64 state = nsXFormsSelectableAccessible::NativeState();

  bool isOpen = false;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->IsDropmarkerOpen(DOMNode, &isOpen);
  NS_ENSURE_SUCCESS(rv, state);

  if (isOpen)
    state |= states::EXPANDED;
  else
    state |= states::COLLAPSED;

  return state | states::HASPOPUP | states::FOCUSABLE;
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
  nsXFormsItemComboboxAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXFormsSelectableItemAccessible(aContent, aDoc)
{
}

role
nsXFormsItemComboboxAccessible::NativeRole()
{
  return roles::LISTITEM;
}

PRUint64
nsXFormsItemComboboxAccessible::NativeState()
{
  PRUint64 state = nsXFormsSelectableItemAccessible::NativeState();

  if (state & states::UNAVAILABLE)
    return state;

  state |= states::SELECTABLE;
  if (IsSelected())
    state |= states::SELECTED;

  return state;
}

NS_IMETHODIMP
nsXFormsItemComboboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("select");
  return NS_OK;
}

