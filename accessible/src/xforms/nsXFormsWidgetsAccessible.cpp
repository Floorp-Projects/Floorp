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

#include "nsXFormsWidgetsAccessible.h"

#include "States.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXFormsDropmarkerWidgetAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsDropmarkerWidgetAccessible::
  nsXFormsDropmarkerWidgetAccessible(nsIContent *aContent,
                                     nsIWeakReference *aShell) :
  nsLeafAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsDropmarkerWidgetAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_PUSHBUTTON;
}

PRUint64
nsXFormsDropmarkerWidgetAccessible::NativeState()
{
  bool isOpen = false;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->IsDropmarkerOpen(DOMNode, &isOpen);
  NS_ENSURE_SUCCESS(rv, 0);

  return isOpen ? states::PRESSED: 0;
}

PRUint8
nsXFormsDropmarkerWidgetAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsXFormsDropmarkerWidgetAccessible::GetActionName(PRUint8 aIndex,
                                                  nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  bool isOpen = false;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->IsDropmarkerOpen(DOMNode, &isOpen);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isOpen)
    aName.AssignLiteral("close");
  else
    aName.AssignLiteral("open");

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsDropmarkerWidgetAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  return sXFormsService->ToggleDropmarkerState(DOMNode);
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsCalendarWidgetAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsCalendarWidgetAccessible::
nsXFormsCalendarWidgetAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

PRUint32
nsXFormsCalendarWidgetAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_CALENDAR;
}


////////////////////////////////////////////////////////////////////////////////
// nsXFormsComboboxPopupWidgetAccessible
////////////////////////////////////////////////////////////////////////////////

nsXFormsComboboxPopupWidgetAccessible::
  nsXFormsComboboxPopupWidgetAccessible(nsIContent *aContent,
                                        nsIWeakReference *aShell) :
  nsXFormsAccessible(aContent, aShell)
{
}

PRUint32
nsXFormsComboboxPopupWidgetAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LIST;
}

PRUint64
nsXFormsComboboxPopupWidgetAccessible::NativeState()
{
  PRUint64 state = nsXFormsAccessible::NativeState();

  bool isOpen = false;
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv = sXFormsService->IsDropmarkerOpen(DOMNode, &isOpen);
  NS_ENSURE_SUCCESS(rv, state);

  state |= states::FOCUSABLE;

  if (isOpen)
    state = states::FLOATING;
  else
    state = states::INVISIBLE;

  return state;
}

NS_IMETHODIMP
nsXFormsComboboxPopupWidgetAccessible::GetValue(nsAString& aValue)
{
  aValue.Truncate();
  return NS_OK;
}

nsresult
nsXFormsComboboxPopupWidgetAccessible::GetNameInternal(nsAString& aName)
{
  // Override nsXFormsAccessible::GetName() to prevent name calculation by
  // XForms rules.
  return NS_OK;
}

void
nsXFormsComboboxPopupWidgetAccessible::Description(nsString& aDescription)
{
  aDescription.Truncate();
}

void
nsXFormsComboboxPopupWidgetAccessible::CacheChildren()
{
  nsCOMPtr<nsIDOMNode> parent = do_QueryInterface(mContent->GetNodeParent());
  // Parent node must be an xforms:select1 element.
  CacheSelectChildren(parent);
}

