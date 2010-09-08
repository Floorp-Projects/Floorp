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

////////////////////////////////////////////////////////////////////////////////
// nsXFormsLabelAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsLabelAccessible::
  nsXFormsLabelAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsLabelAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LABEL;
}

nsresult
nsXFormsLabelAccessible::GetNameInternal(nsAString& aName)
{
  // XXX Correct name calculation for this, see bug 453594.
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelAccessible::GetDescription(nsAString& aDescription)
{
  nsAutoString description;
  nsresult rv = nsTextEquivUtils::
    GetTextEquivFromIDRefs(this, nsAccessibilityAtoms::aria_describedby,
                           description);
  aDescription = description;
  return rv;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsOutputAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsOutputAccessible::
  nsXFormsOutputAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsOutputAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_STATICTEXT;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsTriggerAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsTriggerAccessible::
  nsXFormsTriggerAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsTriggerAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_PUSHBUTTON;
}

NS_IMETHODIMP
nsXFormsTriggerAccessible::GetValue(nsAString& aValue)
{
  aValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsTriggerAccessible::GetNumActions(PRUint8 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);

  *aCount = 1;
  return NS_OK;
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
  nsXFormsInputAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsEditableAccessible(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsInputAccessible, nsAccessible, nsHyperTextAccessible, nsIAccessibleText, nsIAccessibleEditableText)

PRUint32
nsXFormsInputAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_ENTRY;
}

NS_IMETHODIMP
nsXFormsInputAccessible::GetNumActions(PRUint8* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);

  *aCount = 1;
  return NS_OK;
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
  nsXFormsInputBooleanAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsInputBooleanAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_CHECKBUTTON;
}

nsresult
nsXFormsInputBooleanAccessible::GetStateInternal(PRUint32 *aState,
                                                 PRUint32 *aExtraState)
{
  nsresult rv = nsXFormsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  rv = sXFormsService->GetValue(DOMNode, value);
  NS_ENSURE_SUCCESS(rv, rv);

  if (value.EqualsLiteral("true"))
    *aState |= nsIAccessibleStates::STATE_CHECKED;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputBooleanAccessible::GetNumActions(PRUint8 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);

  *aCount = 1;
  return NS_OK;
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
  nsXFormsInputDateAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsContainerAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsInputDateAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_DROPLIST;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSecretAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSecretAccessible::
  nsXFormsSecretAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsInputAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsSecretAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_PASSWORD_TEXT;
}

nsresult
nsXFormsSecretAccessible::GetStateInternal(PRUint32 *aState,
                                           PRUint32 *aExtraState)
{
  nsresult rv = nsXFormsInputAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_PROTECTED;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSecretAccessible::GetValue(nsAString& aValue)
{
  return NS_ERROR_FAILURE;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsRangeAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsRangeAccessible::
  nsXFormsRangeAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsRangeAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_SLIDER;
}

nsresult
nsXFormsRangeAccessible::GetStateInternal(PRUint32 *aState,
                                          PRUint32 *aExtraState)
{
  nsresult rv = nsXFormsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  PRUint32 isInRange = nsIXFormsUtilityService::STATE_NOT_A_RANGE;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  rv = sXFormsService->IsInRange(DOMNode, &isInRange);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isInRange == nsIXFormsUtilityService::STATE_OUT_OF_RANGE)
    *aState |= nsIAccessibleStates::STATE_INVALID;

  return NS_OK;
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
  *aMaximumValue = value.ToFloat(&error);
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
  *aMinimumValue = value.ToFloat(&error);
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
  *aMinimumIncrement = value.ToFloat(&error);
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
  *aCurrentValue = value.ToFloat(&error);
  return error;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsSelectAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsSelectAccessible::
  nsXFormsSelectAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsContainerAccessible(aContent, aShell)
{
}

nsresult
nsXFormsSelectAccessible::GetStateInternal(PRUint32 *aState,
                                           PRUint32 *aExtraState)
{
  nsresult rv = nsXFormsContainerAccessible::GetStateInternal(aState,
                                                              aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  PRUint32 isInRange = nsIXFormsUtilityService::STATE_NOT_A_RANGE;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  rv = sXFormsService->IsInRange(DOMNode, &isInRange);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isInRange == nsIXFormsUtilityService::STATE_OUT_OF_RANGE)
    *aState |= nsIAccessibleStates::STATE_INVALID;

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsChoicesAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsChoicesAccessible::
  nsXFormsChoicesAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsChoicesAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_GROUPING;
}

NS_IMETHODIMP
nsXFormsChoicesAccessible::GetValue(nsAString& aValue)
{
  aValue.Truncate();
  return NS_OK;
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
  nsXFormsSelectFullAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsSelectableAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsSelectFullAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_GROUPING;
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
  nsXFormsItemCheckgroupAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsSelectableItemAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsItemCheckgroupAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_CHECKBUTTON;
}

nsresult
nsXFormsItemCheckgroupAccessible::GetStateInternal(PRUint32 *aState,
                                                   PRUint32 *aExtraState)
{
  nsresult rv = nsXFormsSelectableItemAccessible::GetStateInternal(aState,
                                                                   aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  if (IsItemSelected())
    *aState |= nsIAccessibleStates::STATE_CHECKED;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemCheckgroupAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  if (IsItemSelected())
    aName.AssignLiteral("uncheck");
  else
    aName.AssignLiteral("check");

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsItemRadiogroupAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsItemRadiogroupAccessible::
  nsXFormsItemRadiogroupAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsSelectableItemAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsItemRadiogroupAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_RADIOBUTTON;
}

nsresult
nsXFormsItemRadiogroupAccessible::GetStateInternal(PRUint32 *aState,
                                                   PRUint32 *aExtraState)
{
  nsresult rv = nsXFormsSelectableItemAccessible::GetStateInternal(aState,
                                                                   aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  if (IsItemSelected())
    *aState |= nsIAccessibleStates::STATE_CHECKED;

  return NS_OK;
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
  nsXFormsSelectComboboxAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsSelectableAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsSelectComboboxAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_COMBOBOX;
}

nsresult
nsXFormsSelectComboboxAccessible::GetStateInternal(PRUint32 *aState,
                                                   PRUint32 *aExtraState)
{
  nsresult rv = nsXFormsSelectableAccessible::GetStateInternal(aState,
                                                               aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  PRBool isOpen = PR_FALSE;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  rv = sXFormsService->IsDropmarkerOpen(DOMNode, &isOpen);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isOpen)
    *aState = nsIAccessibleStates::STATE_EXPANDED;
  else
    *aState = nsIAccessibleStates::STATE_COLLAPSED;

  *aState |= nsIAccessibleStates::STATE_HASPOPUP |
             nsIAccessibleStates::STATE_FOCUSABLE;
  return NS_OK;
}

PRBool
nsXFormsSelectComboboxAccessible::GetAllowsAnonChildAccessibles()
{
  return PR_TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsItemComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsItemComboboxAccessible::
  nsXFormsItemComboboxAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsXFormsSelectableItemAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsItemComboboxAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LISTITEM;
}

nsresult
nsXFormsItemComboboxAccessible::GetStateInternal(PRUint32 *aState,
                                                 PRUint32 *aExtraState)
{
  nsresult rv = nsXFormsSelectableItemAccessible::GetStateInternal(aState,
                                                                   aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  if (*aState & nsIAccessibleStates::STATE_UNAVAILABLE)
    return NS_OK;

  *aState |= nsIAccessibleStates::STATE_SELECTABLE;
  if (IsItemSelected())
    *aState |= nsIAccessibleStates::STATE_SELECTED;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemComboboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("select");
  return NS_OK;
}

