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

/** Tell the tab to do its action */
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
nsresult
nsXULTabAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  // get focus and disable status from base class
  nsresult rv = nsLeafAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

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
nsXULTabAccessible::GetAccessibleRelated(PRUint32 aRelationType,
                                         nsIAccessible **aRelatedAccessible)
{
  NS_ENSURE_ARG_POINTER(aRelatedAccessible);
  *aRelatedAccessible = nsnull;

  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsresult rv = nsLeafAccessible::GetAccessibleRelated(aRelationType,
                                                       aRelatedAccessible);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*aRelatedAccessible ||
      aRelationType != nsIAccessibleRelation::RELATION_LABEL_FOR)
    return NS_OK;

  // Expose 'LABEL_FOR' relation on tab accessible for tabpanel accessible.
  // XXX: It makes sense to require the interface from xul:tab to get linked
  // tabpanel element.
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  // Check whether tab and tabpanel are related by 'linkedPanel' attribute on
  // xul:tab element.
  nsAutoString linkedPanelID;
  content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::linkedPanel,
                   linkedPanelID);

  if (!linkedPanelID.IsEmpty()) {
    nsCOMPtr<nsIDOMDocument> document;
    mDOMNode->GetOwnerDocument(getter_AddRefs(document));
    NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMElement> linkedPanel;
    document->GetElementById(linkedPanelID, getter_AddRefs(linkedPanel));
    if (linkedPanel) {
      nsCOMPtr<nsIDOMNode> linkedPanelNode(do_QueryInterface(linkedPanel));
      GetAccService()->GetAccessibleInWeakShell(linkedPanelNode, mWeakShell,
                                                aRelatedAccessible);
      return NS_OK;
    }
  }

  // If there is no 'linkedPanel' attribute on xul:tab element then we
  // assume tab and tabpanels are related 1 to 1. We follow algorithm from
  // the setter 'selectedIndex' of tabbox.xml#tabs binding.

  nsCOMPtr<nsIAccessible> tabsAcc = GetParent();
  NS_ENSURE_TRUE(nsAccUtils::Role(tabsAcc) == nsIAccessibleRole::ROLE_PAGETABLIST,
                 NS_ERROR_FAILURE);

  PRInt32 tabIndex = -1;

  nsCOMPtr<nsIAccessible> childAcc;
  tabsAcc->GetFirstChild(getter_AddRefs(childAcc));
  while (childAcc) {
    if (nsAccUtils::Role(childAcc) == nsIAccessibleRole::ROLE_PAGETAB)
      tabIndex++;

    if (childAcc == this)
      break;

    nsCOMPtr<nsIAccessible> acc;
    childAcc->GetNextSibling(getter_AddRefs(acc));
    childAcc.swap(acc);
  }

  nsCOMPtr<nsIAccessible> tabBoxAcc;
  tabsAcc->GetParent(getter_AddRefs(tabBoxAcc));
  NS_ENSURE_TRUE(nsAccUtils::Role(tabBoxAcc) == nsIAccessibleRole::ROLE_PANE,
                 NS_ERROR_FAILURE);

  tabBoxAcc->GetFirstChild(getter_AddRefs(childAcc));
  while (childAcc) {
    if (nsAccUtils::Role(childAcc) == nsIAccessibleRole::ROLE_PROPERTYPAGE) {
      if (tabIndex == 0) {
        NS_ADDREF(*aRelatedAccessible = childAcc);
        return NS_OK;
      }

      tabIndex--;
    }

    nsCOMPtr<nsIAccessible> acc;
    childAcc->GetNextSibling(getter_AddRefs(acc));
    childAcc.swap(acc);
  }

  return NS_OK;
}

nsresult
nsXULTabAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  NS_ENSURE_TRUE(mDOMNode, NS_ERROR_FAILURE);

  nsresult rv = nsLeafAccessible::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAccUtils::SetAccAttrsForXULSelectControlItem(mDOMNode, aAttributes);

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

#ifdef NEVER
/** 2 children, tabs, tabpanels */
NS_IMETHODIMP nsXULTabBoxAccessible::GetChildCount(PRInt32 *_retval)
{
  *_retval = 2;
  return NS_OK;
}
#endif

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

/** no value */
NS_IMETHODIMP nsXULTabsAccessible::GetValue(nsAString& _retval)
{
  return NS_OK;
}

nsresult
nsXULTabsAccessible::GetNameInternal(nsAString& aName)
{
  // no name
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTabpanelAccessible

nsXULTabpanelAccessible::
  nsXULTabpanelAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
  nsAccessibleWrap(aNode, aShell)
{
}

NS_IMETHODIMP
nsXULTabpanelAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);

  *aRole = nsIAccessibleRole::ROLE_PROPERTYPAGE;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTabpanelAccessible::GetAccessibleRelated(PRUint32 aRelationType,
                                              nsIAccessible **aRelatedAccessible)
{
  NS_ENSURE_ARG_POINTER(aRelatedAccessible);
  *aRelatedAccessible = nsnull;

  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsresult rv = nsAccessibleWrap::GetAccessibleRelated(aRelationType,
                                                       aRelatedAccessible);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*aRelatedAccessible ||
      aRelationType != nsIAccessibleRelation::RELATION_LABELLED_BY)
    return NS_OK;

  // Expose 'LABELLED_BY' relation on tabpanel accessible for tab accessible.
  nsCOMPtr<nsIAccessible> tabBoxAcc;
  GetParent(getter_AddRefs(tabBoxAcc));
  NS_ENSURE_TRUE(nsAccUtils::Role(tabBoxAcc) == nsIAccessibleRole::ROLE_PANE,
                 NS_ERROR_FAILURE);

  PRInt32 tabpanelIndex = -1;
  nsCOMPtr<nsIAccessible> tabsAcc;

  PRBool isTabpanelFound = PR_FALSE;
  nsCOMPtr<nsIAccessible> childAcc;
  tabBoxAcc->GetFirstChild(getter_AddRefs(childAcc));
  while (childAcc && (!tabsAcc || !isTabpanelFound)) {
    if (nsAccUtils::Role(childAcc) == nsIAccessibleRole::ROLE_PAGETABLIST)
      tabsAcc = childAcc;

    if (!isTabpanelFound &&
        nsAccUtils::Role(childAcc) == nsIAccessibleRole::ROLE_PROPERTYPAGE)
      tabpanelIndex++;

    if (childAcc == this)
      isTabpanelFound = PR_TRUE;

    nsCOMPtr<nsIAccessible> acc;
    childAcc->GetNextSibling(getter_AddRefs(acc));
    childAcc.swap(acc);
  }

  if (!tabsAcc || tabpanelIndex == -1)
    return NS_OK;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsIAtom *atomID = content->GetID();

  nsCOMPtr<nsIAccessible> foundTabAcc;
  tabsAcc->GetFirstChild(getter_AddRefs(childAcc));
  while (childAcc) {
    if (nsAccUtils::Role(childAcc) == nsIAccessibleRole::ROLE_PAGETAB) {
      if (atomID) {
        nsCOMPtr<nsIAccessNode> tabAccNode(do_QueryInterface(childAcc));
        nsCOMPtr<nsIDOMNode> tabNode;
        tabAccNode->GetDOMNode(getter_AddRefs(tabNode));
        nsCOMPtr<nsIContent> tabContent(do_QueryInterface(tabNode));
        NS_ENSURE_TRUE(tabContent, NS_ERROR_FAILURE);

        if (tabContent->AttrValueIs(kNameSpaceID_None,
                                    nsAccessibilityAtoms::linkedPanel, atomID,
                                    eCaseMatters)) {
          NS_ADDREF(*aRelatedAccessible = childAcc);
          return NS_OK;
        }
      }

      if (tabpanelIndex == 0) {
        foundTabAcc = childAcc;
        if (!atomID)
          break;
      }

      tabpanelIndex--;
    }

    nsCOMPtr<nsIAccessible> acc;
    childAcc->GetNextSibling(getter_AddRefs(acc));
    childAcc.swap(acc);
  }

  NS_IF_ADDREF(*aRelatedAccessible = foundTabAcc);
  return NS_OK;
}

