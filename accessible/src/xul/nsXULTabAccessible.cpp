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

#include "nsXULTabAccessible.h"

#include "nsAccUtils.h"
#include "nsRelUtils.h"

// NOTE: alphabetically ordered
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"

////////////////////////////////////////////////////////////////////////////////
// nsXULTabAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTabAccessible::
  nsXULTabAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell) :
  nsAccessibleWrap(aNode, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTabAccessible: nsIAccessible

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

////////////////////////////////////////////////////////////////////////////////
// nsXULTabAccessible: nsAccessible

nsresult
nsXULTabAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_PAGETAB;
  return NS_OK;
}

nsresult
nsXULTabAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Possible states: focused, focusable, unavailable(disabled), offscreen.

  // get focus and disable status from base class
  nsresult rv = nsAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  // In the past, tabs have been focusable in classic theme
  // They may be again in the future
  // Check style for -moz-user-focus: normal to see if it's focusable
  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (content) {
    nsIFrame *frame = content->GetPrimaryFrame();
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

// nsIAccessible
NS_IMETHODIMP
nsXULTabAccessible::GetRelationByType(PRUint32 aRelationType,
                                      nsIAccessibleRelation **aRelation)
{
  nsresult rv = nsAccessibleWrap::GetRelationByType(aRelationType,
                                                    aRelation);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aRelationType != nsIAccessibleRelation::RELATION_LABEL_FOR)
    return NS_OK;

  // Expose 'LABEL_FOR' relation on tab accessible for tabpanel accessible.
  // XXX: It makes sense to require the interface from xul:tab to get linked
  // tabpanel element.
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  // Check whether tab and tabpanel are related by 'linkedPanel' attribute on
  // xul:tab element.
  rv = nsRelUtils::AddTargetFromIDRefAttr(aRelationType, aRelation, content,
                                          nsAccessibilityAtoms::linkedPanel,
                                          PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rv != NS_OK_NO_RELATION_TARGET)
    return NS_OK;

  // If there is no 'linkedPanel' attribute on xul:tab element then we
  // assume tab and tabpanels are related 1 to 1. We follow algorithm from
  // the setter 'selectedIndex' of tabbox.xml#tabs binding.

  nsAccessible* tabsAcc = GetParent();
  NS_ENSURE_TRUE(nsAccUtils::Role(tabsAcc) == nsIAccessibleRole::ROLE_PAGETABLIST,
                 NS_ERROR_FAILURE);

  PRInt32 tabIndex = -1;

  PRInt32 childCount = tabsAcc->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible* childAcc = tabsAcc->GetChildAt(childIdx);
    if (nsAccUtils::Role(childAcc) == nsIAccessibleRole::ROLE_PAGETAB)
      tabIndex++;

    if (childAcc == this)
      break;
  }

  nsAccessible* tabBoxAcc = tabsAcc->GetParent();
  NS_ENSURE_TRUE(nsAccUtils::Role(tabBoxAcc) == nsIAccessibleRole::ROLE_PANE,
                 NS_ERROR_FAILURE);

  childCount = tabBoxAcc->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible* childAcc = tabBoxAcc->GetChildAt(childIdx);
    if (nsAccUtils::Role(childAcc) == nsIAccessibleRole::ROLE_PROPERTYPAGE) {
      if (tabIndex == 0)
        return nsRelUtils::AddTarget(aRelationType, aRelation, childAcc);

      tabIndex--;
    }
  }

  return NS_OK;
}

void
nsXULTabAccessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                               PRInt32 *aSetSize)
{
  nsAccUtils::GetPositionAndSizeForXULSelectControlItem(mDOMNode, aPosInSet,
                                                        aSetSize);
}


////////////////////////////////////////////////////////////////////////////////
// nsXULTabBoxAccessible
////////////////////////////////////////////////////////////////////////////////

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
nsresult
nsXULTabBoxAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_PANE;
  return NS_OK;
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
nsresult
nsXULTabsAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_PAGETABLIST;
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

nsresult
nsXULTabpanelAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_PROPERTYPAGE;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTabpanelAccessible::GetRelationByType(PRUint32 aRelationType,
                                           nsIAccessibleRelation **aRelation)
{
  nsresult rv = nsAccessibleWrap::GetRelationByType(aRelationType, aRelation);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aRelationType != nsIAccessibleRelation::RELATION_LABELLED_BY)
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
          return nsRelUtils::AddTarget(aRelationType, aRelation, childAcc);
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

  return nsRelUtils::AddTarget(aRelationType, aRelation, foundTabAcc);
}

