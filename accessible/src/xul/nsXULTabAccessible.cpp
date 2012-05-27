/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTabAccessible.h"

#include "nsAccUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

// NOTE: alphabetically ordered
#include "nsIAccessibleRelation.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULRelatedElement.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXULTabAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTabAccessible::
  nsXULTabAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTabAccessible: nsIAccessible

PRUint8
nsXULTabAccessible::ActionCount()
{
  return 1;
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
    nsCOMPtr<nsIDOMXULElement> tab(do_QueryInterface(mContent));
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

role
nsXULTabAccessible::NativeRole()
{
  return roles::PAGETAB;
}

PRUint64
nsXULTabAccessible::NativeState()
{
  // Possible states: focused, focusable, unavailable(disabled), offscreen.

  // get focus and disable status from base class
  PRUint64 state = nsAccessibleWrap::NativeState();

  // In the past, tabs have been focusable in classic theme
  // They may be again in the future
  // Check style for -moz-user-focus: normal to see if it's focusable
  state &= ~states::FOCUSABLE;

  nsIFrame *frame = mContent->GetPrimaryFrame();
  if (frame) {
    const nsStyleUserInterface* ui = frame->GetStyleUserInterface();
    if (ui->mUserFocus == NS_STYLE_USER_FOCUS_NORMAL)
      state |= states::FOCUSABLE;
  }

  // Check whether the tab is selected
  state |= states::SELECTABLE;
  state &= ~states::SELECTED;
  nsCOMPtr<nsIDOMXULSelectControlItemElement> tab(do_QueryInterface(mContent));
  if (tab) {
    bool selected = false;
    if (NS_SUCCEEDED(tab->GetSelected(&selected)) && selected)
      state |= states::SELECTED;
  }
  return state;
}

// nsIAccessible
Relation
nsXULTabAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsAccessibleWrap::RelationByType(aType);
  if (aType != nsIAccessibleRelation::RELATION_LABEL_FOR)
    return rel;

  // Expose 'LABEL_FOR' relation on tab accessible for tabpanel accessible.
  nsCOMPtr<nsIDOMXULRelatedElement> tabsElm =
    do_QueryInterface(mContent->GetParent());
  if (!tabsElm)
    return rel;

  nsCOMPtr<nsIDOMNode> domNode(DOMNode());
  nsCOMPtr<nsIDOMNode> tabpanelNode;
  tabsElm->GetRelatedElement(domNode, getter_AddRefs(tabpanelNode));
  if (!tabpanelNode)
    return rel;

  nsCOMPtr<nsIContent> tabpanelContent(do_QueryInterface(tabpanelNode));
  rel.AppendTarget(mDoc, tabpanelContent);
  return rel;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULTabsAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTabsAccessible::
  nsXULTabsAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULSelectControlAccessible(aContent, aDoc)
{
}

role
nsXULTabsAccessible::NativeRole()
{
  return roles::PAGETABLIST;
}

PRUint8
nsXULTabsAccessible::ActionCount()
{
  return 0;
}

void
nsXULTabsAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
}

nsresult
nsXULTabsAccessible::GetNameInternal(nsAString& aName)
{
  // no name
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULTabpanelsAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTabpanelsAccessible::
  nsXULTabpanelsAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc)
{
}

role
nsXULTabpanelsAccessible::NativeRole()
{
  return roles::PANE;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULTabpanelAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTabpanelAccessible::
  nsXULTabpanelAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc)
{
}

role
nsXULTabpanelAccessible::NativeRole()
{
  return roles::PROPERTYPAGE;
}

Relation
nsXULTabpanelAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsAccessibleWrap::RelationByType(aType);
  if (aType != nsIAccessibleRelation::RELATION_LABELLED_BY)
    return rel;

  // Expose 'LABELLED_BY' relation on tabpanel accessible for tab accessible.
  nsCOMPtr<nsIDOMXULRelatedElement> tabpanelsElm =
    do_QueryInterface(mContent->GetParent());
  if (!tabpanelsElm)
    return rel;

  nsCOMPtr<nsIDOMNode> domNode(DOMNode());
  nsCOMPtr<nsIDOMNode> tabNode;
  tabpanelsElm->GetRelatedElement(domNode, getter_AddRefs(tabNode));
  if (!tabNode)
    return rel;

  nsCOMPtr<nsIContent> tabContent(do_QueryInterface(tabNode));
  rel.AppendTarget(mDoc, tabContent);
  return rel;
}
