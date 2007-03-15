/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Original Author: Eric Vaughan (evaughan@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsXULSelectAccessible.h"
#include "nsAccessibilityService.h"
#include "nsIContent.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULTextboxElement.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsCaseTreatment.h"

/**
  * Selects, Listboxes and Comboboxes, are made up of a number of different
  *  widgets, some of which are shared between the two. This file contains
  *  all of the widgets for both of the Selects, for XUL only.
  *  (except nsXULRadioGroupAccessible which inherits
  *   nsXULSelectableAccessible so that it supports nsIAccessibleSelectable)
  *
  *  Listbox:
  *     - nsXULListboxAccessible              <richlistbox/>
  *         - nsXULListitemAccessible         <richlistitem/>
  *
  *  Comboboxes:
  *     - nsXULComboboxAccessible             <menulist/>
  *        - nsXULMenuAccessible              <menupopup/>
  *            - nsXULMenuitemAccessible      <menuitem/>
  */

/** ----- nsXULListboxAccessible ----- */

/** Constructor */
nsXULListboxAccessible::nsXULListboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsXULSelectableAccessible(aDOMNode, aShell)
{
}

/**
  * As a nsXULListboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_READONLY
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsXULListboxAccessible::GetState(PRUint32 *aState)
{
  // Get focus status from base class
  nsAccessible::GetState(aState);

// see if we are multiple select if so set ourselves as such
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(mDOMNode));
  if (element) {
    nsAutoString selType;
    element->GetAttribute(NS_LITERAL_STRING("seltype"), selType);
    if (!selType.IsEmpty() && selType.EqualsLiteral("multiple"))
      *aState |= STATE_MULTISELECTABLE | STATE_EXTSELECTABLE;
  }

  return NS_OK;
}

/**
  * Our value is the label of our ( first ) selected child.
  */
NS_IMETHODIMP nsXULListboxAccessible::GetValue(nsAString& _retval)
{
  _retval.Truncate();
  nsCOMPtr<nsIDOMXULSelectControlElement> select(do_QueryInterface(mDOMNode));
  if (select) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
    select->GetSelectedItem(getter_AddRefs(selectedItem));
    if (selectedItem)
      return selectedItem->GetLabel(_retval);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULListboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_LIST;
  return NS_OK;
}

/** ----- nsXULListitemAccessible ----- */

/** Constructor */
nsXULListitemAccessible::nsXULListitemAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsXULMenuitemAccessible(aDOMNode, aShell)
{
  mIsCheckbox = PR_FALSE;
  nsCOMPtr<nsIDOMElement> listItem (do_QueryInterface(mDOMNode));
  if (listItem) {
    nsAutoString typeString;
    nsresult res = listItem->GetAttribute(NS_LITERAL_STRING("type"), typeString);
    if (NS_SUCCEEDED(res) && typeString.Equals(NS_LITERAL_STRING("checkbox")))
      mIsCheckbox = PR_TRUE;
  }
}

/** Inherit the ISupports impl from nsAccessible, we handle nsIAccessibleSelectable */
NS_IMPL_ISUPPORTS_INHERITED0(nsXULListitemAccessible, nsAccessible)

/**
  * If there is a Listcell as a child ( not anonymous ) use it, otherwise
  *   default to getting the name from GetXULName
  */
NS_IMETHODIMP nsXULListitemAccessible::GetName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMNode> child;
  if (NS_SUCCEEDED(mDOMNode->GetFirstChild(getter_AddRefs(child)))) {
    nsCOMPtr<nsIDOMElement> childElement (do_QueryInterface(child));
    if (childElement) {
      nsAutoString tagName;
      childElement->GetLocalName(tagName);
      if (tagName.EqualsLiteral("listcell")) {
        childElement->GetAttribute(NS_LITERAL_STRING("label"), _retval);
        return NS_OK;
      }
    }
  }
  return GetXULName(_retval);
}

/**
  *
  */
NS_IMETHODIMP nsXULListitemAccessible::GetRole(PRUint32 *aRole)
{
  if (mIsCheckbox)
    *aRole = nsIAccessibleRole::ROLE_CHECKBUTTON;
  else
    *aRole = nsIAccessibleRole::ROLE_LISTITEM;
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsXULListitemAccessible::GetState(PRUint32 *aState)
{
  if (mIsCheckbox) {
    nsXULMenuitemAccessible::GetState(aState);
    return NS_OK;
  }

  *aState = STATE_FOCUSABLE | STATE_SELECTABLE;
  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem (do_QueryInterface(mDOMNode));
  if (listItem) {
    PRBool isSelected;
    listItem->GetSelected(&isSelected);
    if (isSelected)
      *aState |= STATE_SELECTED;

    if (gLastFocusedNode == mDOMNode) {
      *aState |= STATE_FOCUSED;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULListitemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click && mIsCheckbox) {
    // check or uncheck
    PRUint32 state;
    GetState(&state);

    if (state & STATE_CHECKED)
      aName.AssignLiteral("uncheck");
    else
      aName.AssignLiteral("check");

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/** ----- nsXULComboboxAccessible ----- */

/** Constructor */
nsXULComboboxAccessible::nsXULComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsAccessibleWrap(aDOMNode, aShell)
{
}

NS_IMETHODIMP nsXULComboboxAccessible::Init()
{
  nsresult rv = nsAccessibleWrap::Init();
  nsXULMenupopupAccessible::GenerateMenu(mDOMNode);
  return rv;
}

/** We are a combobox */
NS_IMETHODIMP nsXULComboboxAccessible::GetRole(PRUint32 *aRole)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
  if (!content) {
    return NS_ERROR_FAILURE;
  }
  *aRole = content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                                NS_LITERAL_STRING("autocomplete"), eIgnoreCase) ?
                                nsIAccessibleRole::ROLE_AUTOCOMPLETE :
                                nsIAccessibleRole::ROLE_COMBOBOX;
  return NS_OK;
}

/**
  * As a nsComboboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_READONLY
  *     STATE_FOCUSABLE
  *     STATE_HASPOPUP
  *     STATE_EXPANDED
  *     STATE_COLLAPSED
  */
NS_IMETHODIMP nsXULComboboxAccessible::GetState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetState(_retval);

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (menuList) {
    PRBool isOpen;
    menuList->GetOpen(&isOpen);
    if (isOpen) {
      *_retval |= STATE_EXPANDED;
    }
    else {
      *_retval |= STATE_COLLAPSED;
    }
    PRBool isEditable;
    menuList->GetEditable(&isEditable);
    if (!isEditable) {
      *_retval |= STATE_READONLY;
    }
  }

  *_retval |= STATE_HASPOPUP | STATE_FOCUSABLE;

  return NS_OK;
}

NS_IMETHODIMP nsXULComboboxAccessible::GetValue(nsAString& _retval)
{
  _retval.Truncate();

  // The MSAA/ATK value is the option or text shown entered in the combobox
  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (menuList) {
    return menuList->GetLabel(_retval);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULComboboxAccessible::GetDescription(nsAString& aDescription)
{
  // Use description of currently focused option
  aDescription.Truncate();
  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (!menuList) {
    return NS_ERROR_FAILURE;  // Shut down
  }
  nsCOMPtr<nsIDOMXULSelectControlItemElement> focusedOption;
  menuList->GetSelectedItem(getter_AddRefs(focusedOption));
  nsCOMPtr<nsIDOMNode> focusedOptionNode(do_QueryInterface(focusedOption));
  if (focusedOptionNode) {
    nsCOMPtr<nsIAccessibilityService> accService = 
      do_GetService("@mozilla.org/accessibilityService;1");
    NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);
    nsCOMPtr<nsIAccessible> focusedOptionAccessible;
    accService->GetAccessibleInWeakShell(focusedOptionNode, mWeakShell, 
                                        getter_AddRefs(focusedOptionAccessible));
    NS_ENSURE_TRUE(focusedOptionAccessible, NS_ERROR_FAILURE);
    return focusedOptionAccessible->GetDescription(aDescription);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULComboboxAccessible::GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);

  if (content->NodeInfo()->Equals(nsAccessibilityAtoms::textbox, kNameSpaceID_XUL) ||
      content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::editable,
                           nsAccessibilityAtoms::_true, eIgnoreCase)) {
    // Both the XUL <textbox type="autocomplete"> and <menulist editable="true"> widgets
    // use nsXULComboboxAccessible. We need to walk the anonymous children for these
    // so that the entry field is a child
    *aAllowsAnonChildren = PR_TRUE;
  } else {
    // Argument of PR_FALSE indicates we don't walk anonymous children for
    // menuitems
    *aAllowsAnonChildren = PR_FALSE;
  }
  return NS_OK;
}

/** Just one action ( click ). */
NS_IMETHODIMP nsXULComboboxAccessible::GetNumActions(PRUint8 *aNumActions)
{
  *aNumActions = 1;
  return NS_OK;
}

/**
  * Programmaticaly toggle the combo box
  */
NS_IMETHODIMP nsXULComboboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != nsXULComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (!menuList) {
    return NS_ERROR_FAILURE;
  }
  PRBool isDroppedDown;
  menuList->GetOpen(&isDroppedDown);
  return menuList->SetOpen(!isDroppedDown);
}

/**
  * Our action name is the reverse of our state: 
  *     if we are closed -> open is our name.
  *     if we are open -> closed is our name.
  * Uses the frame to get the state, updated on every click
  */
NS_IMETHODIMP nsXULComboboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != nsXULComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (!menuList) {
    return NS_ERROR_FAILURE;
  }
  PRBool isDroppedDown;
  menuList->GetOpen(&isDroppedDown);
  if (isDroppedDown)
    aName.AssignLiteral("close"); 
  else
    aName.AssignLiteral("open"); 

  return NS_OK;
}

