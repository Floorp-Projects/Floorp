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
#include "nsIDocument.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULRelatedElement.h"
#include "nsXULElement.h"

#include "mozilla/dom/BindingDeclarations.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULTabAccessible
////////////////////////////////////////////////////////////////////////////////

XULTabAccessible::
  XULTabAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// XULTabAccessible: Accessible

uint8_t
XULTabAccessible::ActionCount()
{
  return 1;
}

void
XULTabAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Switch)
    aName.AssignLiteral("switch");
}

bool
XULTabAccessible::DoAction(uint8_t index)
{
  if (index == eAction_Switch) {
    // XXXbz Could this just FromContent?
    RefPtr<nsXULElement> tab = nsXULElement::FromContentOrNull(mContent);
    if (tab) {
      tab->Click(mozilla::dom::CallerType::System);
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULTabAccessible: Accessible

role
XULTabAccessible::NativeRole()
{
  return roles::PAGETAB;
}

uint64_t
XULTabAccessible::NativeState()
{
  // Possible states: focused, focusable, unavailable(disabled), offscreen.

  // get focus and disable status from base class
  uint64_t state = AccessibleWrap::NativeState();

  // Check whether the tab is selected and/or pinned
  nsCOMPtr<nsIDOMXULSelectControlItemElement> tab(do_QueryInterface(mContent));
  if (tab) {
    bool selected = false;
    if (NS_SUCCEEDED(tab->GetSelected(&selected)) && selected)
      state |= states::SELECTED;

    if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::pinned,
                                           nsGkAtoms::_true, eCaseMatters))
      state |= states::PINNED;

  }

  return state;
}

uint64_t
XULTabAccessible::NativeInteractiveState() const
{
  uint64_t state = Accessible::NativeInteractiveState();
  return (state & states::UNAVAILABLE) ? state : state | states::SELECTABLE;
}

Relation
XULTabAccessible::RelationByType(RelationType aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType != RelationType::LABEL_FOR)
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

uint8_t
XULTabsAccessible::ActionCount()
{
  return 0;
}

void
XULTabsAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
}

ENameValueFlag
XULTabsAccessible::NativeName(nsString& aName)
{
  // no name
  return eNameOK;
}


////////////////////////////////////////////////////////////////////////////////
// XULTabpanelsAccessible
////////////////////////////////////////////////////////////////////////////////

role
XULTabpanelsAccessible::NativeRole()
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
XULTabpanelAccessible::RelationByType(RelationType aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType != RelationType::LABELLED_BY)
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
