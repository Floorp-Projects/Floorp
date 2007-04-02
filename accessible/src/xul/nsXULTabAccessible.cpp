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

// NOTE: alphabetically ordered
#include "nsXULTabAccessible.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"

/**
  * XUL Tab
  */

/** Constructor */
nsXULTabAccessible::nsXULTabAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsLeafAccessible(aNode, aShell)
{ 
}

/** Only one action available */
NS_IMETHODIMP nsXULTabAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/** Return the name of our only action  */
NS_IMETHODIMP nsXULTabAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Switch) {
    aName.AssignLiteral("switch"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/** Tell the tab to do it's action */
NS_IMETHODIMP nsXULTabAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Switch) {
    nsCOMPtr<nsIDOMXULElement> tab(do_QueryInterface(mDOMNode));
    if ( tab )
    {
      tab->Click();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

/** We are a tab */
NS_IMETHODIMP nsXULTabAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_PAGETAB;
  return NS_OK;
}

/**
  * Possible states: focused, focusable, unavailable(disabled), offscreen
  */
NS_IMETHODIMP
nsXULTabAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  // get focus and disable status from base class
  nsresult rv = nsLeafAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  // In the past, tabs have been focusable in classic theme
  // They may be again in the future
  // Check style for -moz-user-focus: normal to see if it's focusable
  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  if (presShell && content) {
    nsIFrame *frame = presShell->GetPrimaryFrameFor(content);
    if (frame) {
      const nsStyleUserInterface* ui = frame->GetStyleUserInterface();
      if (ui->mUserFocus == NS_STYLE_USER_FOCUS_NORMAL)
        *aState |= nsIAccessibleStates::STATE_FOCUSABLE;
    }
  }
  // Check whether the tab is selected
  *aState |= nsIAccessibleStates::STATE_SELECTABLE;
  *aState &= ~nsIAccessibleStates::STATE_SELECTED;
  nsCOMPtr<nsIDOMXULSelectControlItemElement> tab(do_QueryInterface(mDOMNode));
  if (tab) {
    PRBool selected = PR_FALSE;
    if (NS_SUCCEEDED(tab->GetSelected(&selected)) && selected)
      *aState |= nsIAccessibleStates::STATE_SELECTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULTabAccessible::GetAttributes(nsIPersistentProperties **aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  NS_ENSURE_TRUE(mDOMNode, NS_ERROR_FAILURE);

  nsresult rv = nsLeafAccessible::GetAttributes(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAccessibilityUtils::
    SetAccAttrsForXULSelectControlItem(mDOMNode, *aAttributes);

  return NS_OK;
}

/**
  * XUL TabBox
  *  to facilitate naming of the tabPanels object we will give this the name
  *   of the selected tab in the tabs object.
  */

/** Constructor */
nsXULTabBoxAccessible::nsXULTabBoxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessibleWrap(aNode, aShell)
{ 
}

/** We are a window*/
NS_IMETHODIMP nsXULTabBoxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_PANE;
  return NS_OK;
}

/** Possible states: normal */
NS_IMETHODIMP
nsXULTabBoxAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  return NS_OK;
}

#ifdef NEVER
/** 2 children, tabs, tabpanels */
NS_IMETHODIMP nsXULTabBoxAccessible::GetChildCount(PRInt32 *_retval)
{
  *_retval = 2;
  return NS_OK;
}
#endif

/**
  * XUL TabPanels
  *  XXX jgaunt -- this has to report the info for the selected child, reachable through
  *                the DOMNode. The TabPanels object has as its children the different
  *                vbox/hbox/whatevers that provide what you look at when you click on
  *                a tab.
  * Here is how this will work: when asked about an object the tabPanels object will find
  *  out the selected child and create the tabPanel object using the child. That should wrap
  *  any XUL/HTML content in the child, since it is a simple nsAccessible basically.
  * or maybe we just do that on creation. Not use the DOMnode we are given, but cache the selected
  *  DOMnode and then run from there.
  */

/** Constructor */
nsXULTabPanelsAccessible::nsXULTabPanelsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessibleWrap(aNode, aShell)
{ 
}

/** We are a Property Page */
NS_IMETHODIMP nsXULTabPanelsAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_PROPERTYPAGE;
  return NS_OK;
}

/** 
  * The name for the panel is the name from the tab associated with
  *  the panel. XXX not sure if the "panels" object should have the
  *  same name.
  */
NS_IMETHODIMP nsXULTabPanelsAccessible::GetName(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/**
  * XUL Tabs - the s really stands for strip. this is a collection of tab objects
  */

/** Constructor */
nsXULTabsAccessible::nsXULTabsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsXULSelectableAccessible(aNode, aShell)
{ 
}

/** We are a Page Tab List */
NS_IMETHODIMP nsXULTabsAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_PAGETABLIST;
  return NS_OK;
}

/** no actions */
NS_IMETHODIMP nsXULTabsAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

/** no state -- normal */
NS_IMETHODIMP
nsXULTabsAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  return nsAccessible::GetState(aState, aExtraState);
}

/** no value */
NS_IMETHODIMP nsXULTabsAccessible::GetValue(nsAString& _retval)
{
  return NS_OK;
}

/** no name*/
NS_IMETHODIMP nsXULTabsAccessible::GetName(nsAString& _retval)
{
  _retval.Truncate();
  return NS_OK;
}
