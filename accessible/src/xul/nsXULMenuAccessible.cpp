/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsXULMenuAccessible.h"
#include "nsAccessible.h"
#include "nsIAccessible.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULPopupElement.h"

// ------------------------ Menu Item -----------------------------

nsXULMenuitemAccessible::nsXULMenuitemAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell): 
nsAccessible(aDOMNode, aShell)
{ 
}


NS_IMETHODIMP nsXULMenuitemAccessible::GetAccState(PRUint32 *_retval)
{
  nsAccessible::GetAccState(_retval);

  // Focused?
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No DOM element for menu  node!");
  PRBool isFocused = PR_FALSE;
  element->HasAttribute(NS_LITERAL_STRING("_moz-menuactive"), &isFocused); 
  if (isFocused)
    *_retval |= STATE_FOCUSED;

  // Has Popup?
  nsAutoString tagName;
  element->GetLocalName(tagName);
  if (tagName.Equals(NS_LITERAL_STRING("menu")))
    *_retval |= STATE_HASPOPUP;

  nsAutoString menuItemType;
  element->GetAttribute(NS_LITERAL_STRING("type"), menuItemType); 

  if (!menuItemType.IsEmpty()) {
    // Selectable?
    if (menuItemType.Equals(NS_LITERAL_STRING("radio")))
      *_retval |= STATE_SELECTABLE;

    // Checked?
    PRBool isChecked = PR_FALSE;
    element->HasAttribute(NS_LITERAL_STRING("checked"), &isChecked); 
    if (isChecked) {
      if (*_retval & STATE_SELECTABLE)
        *_retval |= STATE_SELECTED;  // Use STATE_SELECTED for radio buttons
      else *_retval |= STATE_CHECKED;
    }
  }

  // Offscreen?
  // If parent or grandparent menuitem is offscreen, then we're offscreen too
  // We get it by replacing the current offscreen bit with the parent's
  PRUint32 parentState = 0;
  nsCOMPtr<nsIAccessible> parentAccessible;
  GetAccParent(getter_AddRefs(parentAccessible));
  parentAccessible->GetAccState(&parentState);
  *_retval &= ~STATE_OFFSCREEN;  // clear the old OFFSCREEN bit
  *_retval |= (parentState & STATE_OFFSCREEN);  // or it with the parent's offscreen bit

  return NS_OK;
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetAccName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No DOM element for menu  node!");
  element->GetAttribute(NS_LITERAL_STRING("label"), _retval); 

  return NS_OK;
}


NS_IMETHODIMP nsXULMenuitemAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_MENUITEM;
  return NS_OK;
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetAccFirstChild(nsIAccessible **aAccFirstChild)
{
  *aAccFirstChild = nsnull;

  // Last argument of PR_FALSE indicates we don't walk anonymous children for menuitems
  nsAccessibleTreeWalker walker(mPresShell, mDOMNode, mSiblingIndex, mSiblingList, PR_FALSE);
  if (NS_SUCCEEDED(walker.GetFirstChild())) {
    *aAccFirstChild = walker.mState.accessible;
    NS_ADDREF(*aAccFirstChild);
  }

  return NS_OK;  
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetAccLastChild(nsIAccessible **aAccLastChild)
{
  *aAccLastChild = nsnull;

  // Last argument of PR_FALSE indicates we don't walk anonymous children for menuitems
  nsAccessibleTreeWalker walker(mPresShell, mDOMNode, mSiblingIndex, mSiblingList, PR_FALSE);
  if (NS_SUCCEEDED(walker.GetLastChild())) {
    *aAccLastChild = walker.mState.accessible;
    NS_ADDREF(*aAccLastChild);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetAccChildCount(PRInt32 *aAccChildCount)
{
  // Last argument of PR_FALSE indicates we don't walk anonymous children for menuitems
  nsAccessibleTreeWalker walker(mPresShell, mDOMNode, mSiblingIndex, mSiblingList, PR_FALSE);
  *aAccChildCount = walker.GetChildCount();

  return NS_OK;  
}

// ------------------------ Menu Separator ----------------------------

nsXULMenuSeparatorAccessible::nsXULMenuSeparatorAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell): 
nsXULMenuitemAccessible(aDOMNode, aShell)
{ 
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetAccState(PRUint32 *_retval)
{
  // Isn't focusable, but can be offscreen
  nsXULMenuitemAccessible::GetAccState(_retval);
  *_retval &= STATE_OFFSCREEN;

  return NS_OK;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetAccName(nsAString& _retval)
{
  _retval.Assign(NS_LITERAL_STRING(""));
  return NS_OK;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_SEPARATOR;
  return NS_OK;
}

// ------------------------ Menu Popup -----------------------------

nsXULMenupopupAccessible::nsXULMenupopupAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell): nsAccessible(aDOMNode, aShell)
{ 
}

NS_IMETHODIMP nsXULMenupopupAccessible::GetAccState(PRUint32 *_retval)
{
  // We are onscreen if our parent is active
  *_retval = 0;
  PRBool isActive = PR_FALSE;

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  element->HasAttribute(NS_LITERAL_STRING("menuactive"), &isActive);
  if (!isActive) {
    nsCOMPtr<nsIAccessible> parentAccessible;
    nsCOMPtr<nsIDOMNode> parentNode;
    GetAccParent(getter_AddRefs(parentAccessible));
    if (parentAccessible)
      parentAccessible->AccGetDOMNode(getter_AddRefs(parentNode));
    element = do_QueryInterface(parentNode);
    if (element)
      element->HasAttribute(NS_LITERAL_STRING("open"), &isActive);
  }

  if (!isActive)
    *_retval |= STATE_OFFSCREEN;

  return NS_OK;
}

NS_IMETHODIMP nsXULMenupopupAccessible::GetAccName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No element for popup node!");

  while (element) {
    element->GetAttribute(NS_LITERAL_STRING("label"), _retval);
    if (!_retval.IsEmpty())
      return NS_OK;
    nsCOMPtr<nsIDOMNode> parentNode, node(do_QueryInterface(element));
    if (!node)
      return NS_ERROR_FAILURE;
    node->GetParentNode(getter_AddRefs(parentNode));
    element = do_QueryInterface(parentNode);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULMenupopupAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_MENUPOPUP;
  return NS_OK;
}

// ------------------------ Menu Bar -----------------------------

nsXULMenubarAccessible::nsXULMenubarAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell): nsAccessible(aDOMNode, aShell)
{ 
}

NS_IMETHODIMP nsXULMenubarAccessible::GetAccState(PRUint32 *_retval)
{
  return nsAccessible::GetAccState(_retval);
}


NS_IMETHODIMP nsXULMenubarAccessible::GetAccName(nsAString& _retval)
{
  _retval = NS_LITERAL_STRING("menubar");

  return NS_OK;
}

NS_IMETHODIMP nsXULMenubarAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_MENUBAR;
  return NS_OK;
}

