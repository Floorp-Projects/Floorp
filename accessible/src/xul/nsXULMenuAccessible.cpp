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
 *   Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsXULMenuAccessible.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMKeyEvent.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"

// ------------------------ Menu Item -----------------------------

nsXULMenuitemAccessible::nsXULMenuitemAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell): 
nsAccessibleWrap(aDOMNode, aShell)
{ 
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetState(PRUint32 *_retval)
{
  nsAccessible::GetState(_retval);

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
  if (tagName.EqualsLiteral("menu"))
    *_retval |= STATE_HASPOPUP;

  nsAutoString menuItemType;
  element->GetAttribute(NS_LITERAL_STRING("type"), menuItemType); 

  if (!menuItemType.IsEmpty()) {
    // Selectable?
    if (menuItemType.EqualsLiteral("radio"))
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
  GetParent(getter_AddRefs(parentAccessible));
  parentAccessible->GetState(&parentState);
  *_retval &= ~STATE_OFFSCREEN;  // clear the old OFFSCREEN bit
  *_retval |= (parentState & STATE_OFFSCREEN);  // or it with the parent's offscreen bit

  return NS_OK;
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No DOM element for menu  node!");
  element->GetAttribute(NS_LITERAL_STRING("label"), _retval); 

  return NS_OK;
}

//return menu accesskey: N or Alt+F
NS_IMETHODIMP nsXULMenuitemAccessible::GetKeyboardShortcut(nsAString& _retval)
{
  static PRInt32 gMenuAccesskeyModifier = -1;  // magic value of -1 indicates unitialized state

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) {
    nsAutoString accesskey;
    elt->GetAttribute(NS_LITERAL_STRING("accesskey"), accesskey);
    if (accesskey.IsEmpty())
      return NS_OK;

    nsCOMPtr<nsIAccessible> parentAccessible;
    GetParent(getter_AddRefs(parentAccessible));
    if (parentAccessible) {
      PRUint32 role;
      parentAccessible->GetRole(&role);
      if (role == ROLE_MENUBAR) {
        // If top level menu item, add Alt+ or whatever modifier text to string
        // No need to cache pref service, this happens rarely
        if (gMenuAccesskeyModifier == -1) {
          // Need to initialize cached global accesskey pref
          gMenuAccesskeyModifier = 0;
          nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
          if (prefBranch)
            prefBranch->GetIntPref("ui.key.menuAccessKey", &gMenuAccesskeyModifier);
        }
        nsAutoString propertyKey;
        switch (gMenuAccesskeyModifier) {
          case nsIDOMKeyEvent::DOM_VK_CONTROL: propertyKey.AssignLiteral("VK_CONTROL"); break;
          case nsIDOMKeyEvent::DOM_VK_ALT: propertyKey.AssignLiteral("VK_ALT"); break;
          case nsIDOMKeyEvent::DOM_VK_META: propertyKey.AssignLiteral("VK_META"); break;
        }
        if (!propertyKey.IsEmpty())
          nsAccessible::GetFullKeyName(propertyKey, accesskey, _retval);
      }
    }
    if (_retval.IsEmpty())
      _retval = accesskey;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//return menu shortcut: Ctrl+F or Ctrl+Shift+L
NS_IMETHODIMP nsXULMenuitemAccessible::GetKeyBinding(nsAString& _retval)
{
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) {
    nsAutoString accelText;
    elt->GetAttribute(NS_LITERAL_STRING("acceltext"), accelText);
    if (accelText.IsEmpty())
      return NS_OK;

    _retval = accelText;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_MENUITEM;
  return NS_OK;
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetChildCount(PRInt32 *aAccChildCount)
{
  // Set menugenerated="true" on the menupopup node to generate the
  // sub-menu items if they have not been generated
  PRUint32 childIndex, numChildren = 0;
  nsCOMPtr<nsIDOMNode> childNode;
  nsCOMPtr<nsIDOMNodeList> nodeList;
  mDOMNode->GetChildNodes(getter_AddRefs(nodeList));
  if (nodeList && NS_OK == nodeList->GetLength(&numChildren)) {
    for (childIndex = 0; childIndex < numChildren; childIndex++) {
      nodeList->Item(childIndex, getter_AddRefs(childNode));
      nsAutoString nodeName;
      childNode->GetNodeName(nodeName);
      if (nodeName.EqualsLiteral("menupopup")) {
        break;
      }
    }

    if (childIndex < numChildren) {
      nsCOMPtr<nsIDOMElement> element(do_QueryInterface(childNode));
      if (element) {
        nsAutoString attr;
        element->GetAttribute(NS_LITERAL_STRING("menugenerated"), attr);
        if (!attr.EqualsLiteral("true")) {
          element->SetAttribute(NS_LITERAL_STRING("menugenerated"), NS_LITERAL_STRING("true"));
        }
      }
    }
  }

  // Argument of PR_FALSE indicates we don't walk anonymous children for menuitems
  CacheChildren(PR_FALSE);
  *aAccChildCount = mAccChildCount;
  return NS_OK;  
}

NS_IMETHODIMP nsXULMenuitemAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Select) {   // default action
    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(mDOMNode));
    if (xulElement)
      xulElement->Click();

    nsCOMPtr<nsIAccessible> parentAccessible;
    GetParent(getter_AddRefs(parentAccessible));
    if (parentAccessible) {
      PRUint32 role;
      parentAccessible->GetRole(&role);
      if (role == ROLE_LIST) {
        nsCOMPtr<nsIAccessible> buttonAccessible;
        parentAccessible->GetPreviousSibling(getter_AddRefs(buttonAccessible));
        PRUint32 state;
        buttonAccessible->GetState(&state);
        if (state & STATE_PRESSED)
          buttonAccessible->DoAction(eAction_Click);
      }
    }
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

/** select us! close combo box if necessary*/
NS_IMETHODIMP nsXULMenuitemAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Select) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("select"), _retval); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULMenuitemAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}


// ------------------------ Menu Separator ----------------------------

nsXULMenuSeparatorAccessible::nsXULMenuSeparatorAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell): 
nsXULMenuitemAccessible(aDOMNode, aShell)
{ 
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetState(PRUint32 *_retval)
{
  // Isn't focusable, but can be offscreen
  nsXULMenuitemAccessible::GetState(_retval);
  *_retval &= STATE_OFFSCREEN;

  return NS_OK;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetName(nsAString& _retval)
{
  _retval.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_SEPARATOR;
  return NS_OK;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::DoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULMenuSeparatorAccessible::GetNumActions(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
// ------------------------ Menu Popup -----------------------------

nsXULMenupopupAccessible::nsXULMenupopupAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell): 
  nsAccessibleWrap(aDOMNode, aShell)
{ 
}

NS_IMETHODIMP nsXULMenupopupAccessible::GetState(PRUint32 *_retval)
{
  // We are onscreen if our parent is active
  *_retval = 0;
  PRBool isActive = PR_FALSE;

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  element->HasAttribute(NS_LITERAL_STRING("menuactive"), &isActive);
  if (!isActive) {
    nsCOMPtr<nsIAccessible> parentAccessible;
    nsCOMPtr<nsIDOMNode> parentNode;
    GetParent(getter_AddRefs(parentAccessible));
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(parentAccessible));
    if (accessNode) 
      accessNode->GetDOMNode(getter_AddRefs(parentNode));
    element = do_QueryInterface(parentNode);
    if (element)
      element->HasAttribute(NS_LITERAL_STRING("open"), &isActive);
  }

  if (!isActive)
    *_retval |= STATE_OFFSCREEN;

  return NS_OK;
}

NS_IMETHODIMP nsXULMenupopupAccessible::GetName(nsAString& _retval)
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

NS_IMETHODIMP nsXULMenupopupAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_MENUPOPUP;
  return NS_OK;
}

// ------------------------ Menu Bar -----------------------------

nsXULMenubarAccessible::nsXULMenubarAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell): 
  nsAccessibleWrap(aDOMNode, aShell)
{ 
}

NS_IMETHODIMP nsXULMenubarAccessible::GetState(PRUint32 *_retval)
{
  nsresult rv = nsAccessible::GetState(_retval);
  *_retval &= ~STATE_FOCUSABLE; // Menu bar iteself is not actually focusable
  return rv;
}


NS_IMETHODIMP nsXULMenubarAccessible::GetName(nsAString& _retval)
{
  _retval.AssignLiteral("Application");

  return NS_OK;
}

NS_IMETHODIMP nsXULMenubarAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_MENUBAR;
  return NS_OK;
}

