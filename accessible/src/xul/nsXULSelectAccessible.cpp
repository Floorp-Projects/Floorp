/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsXULSelectAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIServiceManager.h"
#include "nsLayoutAtoms.h"

/**
  * Selects, Listboxes and Comboboxes, are made up of a number of different
  *  widgets, some of which are shared between the two. This file contains 
  *  all of the widgets for both of the Selects, for XUL only. Some of them
  *  extend classes from nsSelectAccessible.cpp, which contains base classes 
  *  that are also extended by the XUL Select Accessibility support.
  *
  *  Listbox:
  *     - nsXULListboxAccessible
  *        - nsXULSelectListAccessible
  *           - nsXULSelectOptionAccessible
  *
  *  Comboboxes:
  *     - nsXULComboboxAccessible
  *        - nsHTMLTextFieldAccessible (editable) or nsTextAccessible (readonly)
  *        - nsXULComboboxButtonAccessible
  *         - nsXULSelectListAccessible
  *            - nsXULSelectOptionAccessible
  */

/** ------------------------------------------------------ */
/**  First, the common widgets                             */
/** ------------------------------------------------------ */

/** ----- nsXULSelectListAccessible ----- */

/** Default Constructor */
nsXULSelectListAccessible::nsXULSelectListAccessible(nsIDOMNode* aDOMNode, 
                                                     nsIWeakReference* aShell)
:nsAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP nsXULSelectListAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LIST;
  return NS_OK;
}

/**
  * As a nsXULSelectListAccessible we can have the following states:
  *     STATE_MULTISELECTABLE
  *     STATE_EXTSELECTABLE
  */
NS_IMETHODIMP nsXULSelectListAccessible::GetAccState(PRUint32 *_retval)
{ 
  *_retval = 0;
  nsAutoString selectionTypeString;
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No nsIDOMElement for caption node!");
  element->GetAttribute(NS_LITERAL_STRING("seltype"), selectionTypeString) ;
  if (selectionTypeString.EqualsIgnoreCase("multiple"))
    *_retval |= STATE_MULTISELECTABLE | STATE_EXTSELECTABLE;

  return NS_OK;
}

/** ----- nsXULSelectOptionAccessible ----- */

/** Default Constructor */
nsXULSelectOptionAccessible::nsXULSelectOptionAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsXULMenuitemAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP nsXULSelectOptionAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LISTITEM;
  return NS_OK;
}

/**
  * As a nsXULSelectOptionAccessible we can have the following states:
  *     STATE_SELECTABLE
  *     STATE_SELECTED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsXULSelectOptionAccessible::GetAccState(PRUint32 *_retval)
{
  nsXULMenuitemAccessible::GetAccState(_retval);

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item(do_QueryInterface(mDOMNode));
  PRBool isSelected = PR_FALSE;
  item->GetSelected(&isSelected);
  if (isSelected)
    *_retval |= STATE_SELECTED;

  return NS_OK;
}

/** ------------------------------------------------------ */
/**  Secondly, the Listbox widget                          */
/** ------------------------------------------------------ */

/** ----- nsXULListboxAccessible ----- */

/** Constructor */
nsXULListboxAccessible::nsXULListboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsListboxAccessible(aDOMNode, aShell)
{
}

/** Inherit the ISupports impl from nsAccessible, we handle nsIAccessibleSelectable */
NS_IMPL_ISUPPORTS_INHERITED1(nsXULListboxAccessible, nsListboxAccessible, nsIAccessibleSelectable)

/**
  * Our value is the value of our ( first ) selected child. nsIDOMXULSelectElement
  *     returns this by default with GetValue().
  */
NS_IMETHODIMP nsXULListboxAccessible::GetAccValue(nsAWritableString& _retval)
{
  nsCOMPtr<nsIDOMXULSelectControlElement> select(do_QueryInterface(mDOMNode));
  if (select) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
    select->GetSelectedItem(getter_AddRefs(selectedItem));
    return selectedItem->GetValue(_retval);
  }
  return NS_ERROR_FAILURE;
}

/**
  * nsIAccessibleSelectable method. 
  *   - gets from the Select DOMNode the list of all Select Options
  *   - iterates through all of the options looking for selected Options
  *   - creates IAccessible objects for selected Options
  *   - Returns the IAccessibles for selected Options in the nsISupportsArray
  *
  * retval will be nsnull if:
  *   - there are no Options in the Select Element
  *   - there are Options but none are selected
  *   - the DOMNode is not a nsIDOMXULSelectControlElement ( shouldn't happen )
  */
NS_IMETHODIMP nsXULListboxAccessible::GetSelectedChildren(nsISupportsArray **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  nsCOMPtr<nsISupportsArray> selectedAccessibles;
  NS_NewISupportsArray(getter_AddRefs(selectedAccessibles));
  if (!selectedAccessibles || !accService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> optionNode, nextOptionNode;
  mDOMNode->GetFirstChild(getter_AddRefs(optionNode));

  while (PR_TRUE) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> option(do_QueryInterface(optionNode));
    if (!option)
      break;
    PRBool isSelected = PR_FALSE;
    option->GetSelected(&isSelected);
    if (isSelected) {
      nsCOMPtr<nsIAccessible> tempAccessible;
      accService->CreateXULSelectOptionAccessible(optionNode, getter_AddRefs(tempAccessible));
      if (tempAccessible)
        selectedAccessibles->AppendElement(tempAccessible);
    }

    optionNode->GetNextSibling(getter_AddRefs(nextOptionNode));
    optionNode = nextOptionNode;
  }

  PRUint32 length = 0;
  selectedAccessibles->Count(&length); // reusing length
  if ( length != 0 ) { // length of nsISupportsArray containing selected options
    *_retval = selectedAccessibles;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
  }

  // no options, not a select or none of the options are selected
  return NS_OK;
}

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/** ----- nsXULComboboxAccessible ----- */

/** Constructor */
nsXULComboboxAccessible::nsXULComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsComboboxAccessible(aDOMNode, aShell)
{
}

/**
  * Our value is the value of our ( first ) selected child. nsIDOMXULSelectElement
  *     returns this by default with GetValue().
  */
NS_IMETHODIMP nsXULComboboxAccessible::GetAccValue(nsAWritableString& _retval)
{
  nsCOMPtr<nsIDOMXULSelectControlElement> select(do_QueryInterface(mDOMNode));
  if (select) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
    select->GetSelectedItem(getter_AddRefs(selectedItem));
    return selectedItem->GetValue(_retval);
  }
  return NS_ERROR_FAILURE;
}


