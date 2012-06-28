/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULTabAccessible.h"

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
// XULTabAccessible
////////////////////////////////////////////////////////////////////////////////

XULTabAccessible::
  XULTabAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// XULTabAccessible: nsIAccessible

PRUint8
XULTabAccessible::ActionCount()
{
  return 1;
}

/** Return the name of our only action  */
NS_IMETHODIMP
XULTabAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Switch) {
    aName.AssignLiteral("switch"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/** Tell the tab to do its action */
NS_IMETHODIMP
XULTabAccessible::DoAction(PRUint8 index)
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
// XULTabAccessible: Accessible

role
XULTabAccessible::NativeRole()
{
  return roles::PAGETAB;
}

PRUint64
XULTabAccessible::NativeState()
{
  // Possible states: focused, focusable, unavailable(disabled), offscreen.

  // get focus and disable status from base class
  PRUint64 state = AccessibleWrap::NativeState();

  // Check whether the tab is selected
  nsCOMPtr<nsIDOMXULSelectControlItemElement> tab(do_QueryInterface(mContent));
  if (tab) {
    bool selected = false;
    if (NS_SUCCEEDED(tab->GetSelected(&selected)) && selected)
      state |= states::SELECTED;
  }
  return state;
}

PRUint64
XULTabAccessible::NativeInteractiveState() const
{
  PRUint64 state = Accessible::NativeInteractiveState();
  return (state & states::UNAVAILABLE) ? state : state | states::SELECTABLE;
}

// nsIAccessible
Relation
XULTabAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
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
// XULTabsAccessible
////////////////////////////////////////////////////////////////////////////////

XULTabsAccessible::
  XULTabsAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULSelectControlAccessible(aContent, aDoc)
{
}

role
XULTabsAccessible::NativeRole()
{
  return roles::PAGETABLIST;
}

PRUint8
XULTabsAccessible::ActionCount()
{
  return 0;
}

void
XULTabsAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
}

nsresult
XULTabsAccessible::GetNameInternal(nsAString& aName)
{
  // no name
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// XULDeckAccessible
////////////////////////////////////////////////////////////////////////////////

role
XULDeckAccessible::NativeRole()
{
  return roles::PANE;
}

////////////////////////////////////////////////////////////////////////////////
// XULTabpanelAccessible
////////////////////////////////////////////////////////////////////////////////

XULTabpanelAccessible::
  XULTabpanelAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULTabpanelAccessible::NativeRole()
{
  return roles::PROPERTYPAGE;
}

Relation
XULTabpanelAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
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
