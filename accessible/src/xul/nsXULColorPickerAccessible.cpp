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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: John Gaunt (jgaunt@netscape.com)
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

#include "nsXULColorPickerAccessible.h"

#include "nsAccessibleTreeWalker.h"

#include "nsIDOMElement.h"


////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerTileAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULColorPickerTileAccessible::
  nsXULColorPickerTileAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell) :
  nsAccessibleWrap(aNode, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerTileAccessible: nsIAccessible

NS_IMETHODIMP
nsXULColorPickerTileAccessible::GetValue(nsAString& aValue)
{
  aValue.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::color, aValue);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerTileAccessible: nsAccessible

nsresult
nsXULColorPickerTileAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_PUSHBUTTON;
  return NS_OK;
}

nsresult
nsXULColorPickerTileAccessible::GetStateInternal(PRUint32 *aState,
                                                 PRUint32 *aExtraState)
{
  // Possible states: focused, focusable, selected.

  // get focus and disable status from base class
  nsresult rv = nsAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_FOCUSABLE;

  // Focused?
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No XUL Element for colorpicker");
  PRBool isFocused = PR_FALSE;
  element->HasAttribute(NS_LITERAL_STRING("hover"), &isFocused);
  if (isFocused)
    *aState |= nsIAccessibleStates::STATE_FOCUSED;

  PRBool isSelected = PR_FALSE;
  element->HasAttribute(NS_LITERAL_STRING("selected"), &isSelected);
  if (isFocused)
    *aState |= nsIAccessibleStates::STATE_SELECTED;

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible
////////////////////////////////////////////////////////////////////////////////

/**
  * Default Constructor
  */
nsXULColorPickerAccessible::nsXULColorPickerAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsXULColorPickerTileAccessible(aNode, aShell)
{ 
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible: nsAccessNode

nsresult
nsXULColorPickerAccessible::Init()
{
  nsresult rv = nsXULColorPickerTileAccessible::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCoreUtils::GeneratePopupTree(mDOMNode, PR_TRUE);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible: nsAccessible

nsresult
nsXULColorPickerAccessible::GetStateInternal(PRUint32 *aState,
                                             PRUint32 *aExtraState)
{
  // Possible states: focused, focusable, unavailable(disabled).

  // get focus and disable status from base class
  nsresult rv = nsAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_FOCUSABLE |
             nsIAccessibleStates::STATE_HASPOPUP;

  return NS_OK;
}

nsresult
nsXULColorPickerAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_BUTTONDROPDOWNGRID;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULColorPickerAccessible: protected nsAccessible

void
nsXULColorPickerAccessible::CacheChildren()
{
  if (IsDefunct()) {
    mAccChildCount = eChildCountUninitialized;
    return;   // This outer doc node has been shut down
  }

  if (mAccChildCount != eChildCountUninitialized)
    return;

  mAccChildCount = 0;  // Avoid reentry

  nsCOMPtr<nsIAccessible> menupopupAccessible;
  nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, PR_TRUE);
  walker.GetFirstChild();

  while (walker.mState.accessible) {
    PRUint32 role = nsAccUtils::Role(walker.mState.accessible);

    if (role == nsIAccessibleRole::ROLE_ALERT) {
      // Get an accessbile for menupopup or panel elements.
      menupopupAccessible = walker.mState.accessible;
      break;
    }

    walker.GetNextSibling();
  }

  if (!menupopupAccessible)
    return;

  SetFirstChild(menupopupAccessible);

  nsRefPtr<nsAccessible> menupopupAcc =
    nsAccUtils::QueryObject<nsAccessible>(menupopupAccessible);
  menupopupAcc->SetParent(this);

  mAccChildCount++;
}
