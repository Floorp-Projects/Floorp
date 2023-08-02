/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULTabAccessible.h"

#include "ARIAMap.h"
#include "nsAccUtils.h"
#include "Relation.h"
#include "mozilla/a11y/Role.h"
#include "States.h"

// NOTE: alphabetically ordered
#include "mozilla/dom/Document.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULRelatedElement.h"
#include "nsXULElement.h"

#include "mozilla/dom/BindingDeclarations.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULTabAccessible
////////////////////////////////////////////////////////////////////////////////

XULTabAccessible::XULTabAccessible(nsIContent* aContent, DocAccessible* aDoc)
    : HyperTextAccessible(aContent, aDoc) {}

////////////////////////////////////////////////////////////////////////////////
// XULTabAccessible: LocalAccessible

bool XULTabAccessible::HasPrimaryAction() const { return true; }

void XULTabAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == eAction_Switch) aName.AssignLiteral("switch");
}

bool XULTabAccessible::DoAction(uint8_t index) const {
  if (index == eAction_Switch) {
    // XXXbz Could this just FromContent?
    RefPtr<nsXULElement> tab = nsXULElement::FromNodeOrNull(mContent);
    if (tab) {
      tab->Click(mozilla::dom::CallerType::System);
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULTabAccessible: LocalAccessible

role XULTabAccessible::NativeRole() const { return roles::PAGETAB; }

uint64_t XULTabAccessible::NativeState() const {
  // Possible states: focused, focusable, unavailable(disabled), offscreen.

  // get focus and disable status from base class
  uint64_t state = AccessibleWrap::NativeState();

  // Check whether the tab is selected and/or pinned
  nsCOMPtr<nsIDOMXULSelectControlItemElement> tab =
      Elm()->AsXULSelectControlItem();
  if (tab) {
    bool selected = false;
    if (NS_SUCCEEDED(tab->GetSelected(&selected)) && selected) {
      state |= states::SELECTED;
    }

    if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::pinned,
                                           nsGkAtoms::_true, eCaseMatters)) {
      state |= states::PINNED;
    }
  }

  return state;
}

uint64_t XULTabAccessible::NativeInteractiveState() const {
  uint64_t state = LocalAccessible::NativeInteractiveState();
  return (state & states::UNAVAILABLE) ? state : state | states::SELECTABLE;
}

Relation XULTabAccessible::RelationByType(RelationType aType) const {
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType != RelationType::LABEL_FOR) return rel;

  // Expose 'LABEL_FOR' relation on tab accessible for tabpanel accessible.
  ErrorResult rv;
  nsIContent* parent = mContent->AsElement()->Closest("tabs"_ns, rv);
  if (!parent) return rel;

  nsCOMPtr<nsIDOMXULRelatedElement> tabsElm =
      parent->AsElement()->AsXULRelated();
  if (!tabsElm) return rel;

  RefPtr<mozilla::dom::Element> tabpanelElement;
  tabsElm->GetRelatedElement(GetNode(), getter_AddRefs(tabpanelElement));
  if (!tabpanelElement) return rel;

  rel.AppendTarget(mDoc, tabpanelElement);
  return rel;
}

void XULTabAccessible::ApplyARIAState(uint64_t* aState) const {
  HyperTextAccessible::ApplyARIAState(aState);
  // XUL tab has an implicit ARIA role of tab, so support aria-selected.
  // Don't use aria::MapToState because that will set the SELECTABLE state
  // even if the tab is disabled.
  if (nsAccUtils::IsARIASelected(this)) {
    *aState |= states::SELECTED;
  }
}

////////////////////////////////////////////////////////////////////////////////
// XULTabsAccessible
////////////////////////////////////////////////////////////////////////////////

XULTabsAccessible::XULTabsAccessible(nsIContent* aContent, DocAccessible* aDoc)
    : XULSelectControlAccessible(aContent, aDoc) {}

role XULTabsAccessible::NativeRole() const { return roles::PAGETABLIST; }

bool XULTabsAccessible::HasPrimaryAction() const { return false; }

void XULTabsAccessible::Value(nsString& aValue) const { aValue.Truncate(); }

ENameValueFlag XULTabsAccessible::NativeName(nsString& aName) const {
  // no name
  return eNameOK;
}

void XULTabsAccessible::ApplyARIAState(uint64_t* aState) const {
  XULSelectControlAccessible::ApplyARIAState(aState);
  // XUL tabs has an implicit ARIA role of tablist, so support
  // aria-multiselectable.
  MOZ_ASSERT(Elm());
  aria::MapToState(aria::eARIAMultiSelectable, Elm(), aState);
}

// XUL tabs is a single selection control and doesn't allow ARIA selection.
// However, if aria-multiselectable is used, it becomes a multiselectable
// control, where both native and ARIA markup are used to set selection.
// Therefore, if aria-multiselectable is set, use the base implementation of
// the selection retrieval methods in order to support ARIA selection.
// We don't bother overriding the selection setting methods because
// current front-end code using XUL tabs doesn't support setting of
//  aria-selected by the a11y engine and we still want to be able to set the
// primary selected item according to XUL.

void XULTabsAccessible::SelectedItems(nsTArray<Accessible*>* aItems) {
  if (nsAccUtils::IsARIAMultiSelectable(this)) {
    AccessibleWrap::SelectedItems(aItems);
  } else {
    XULSelectControlAccessible::SelectedItems(aItems);
  }
}

Accessible* XULTabsAccessible::GetSelectedItem(uint32_t aIndex) {
  if (nsAccUtils::IsARIAMultiSelectable(this)) {
    return AccessibleWrap::GetSelectedItem(aIndex);
  }

  return XULSelectControlAccessible::GetSelectedItem(aIndex);
}

uint32_t XULTabsAccessible::SelectedItemCount() {
  if (nsAccUtils::IsARIAMultiSelectable(this)) {
    return AccessibleWrap::SelectedItemCount();
  }

  return XULSelectControlAccessible::SelectedItemCount();
}

bool XULTabsAccessible::IsItemSelected(uint32_t aIndex) {
  if (nsAccUtils::IsARIAMultiSelectable(this)) {
    return AccessibleWrap::IsItemSelected(aIndex);
  }

  return XULSelectControlAccessible::IsItemSelected(aIndex);
}

////////////////////////////////////////////////////////////////////////////////
// XULTabpanelsAccessible
////////////////////////////////////////////////////////////////////////////////

role XULTabpanelsAccessible::NativeRole() const { return roles::PANE; }

////////////////////////////////////////////////////////////////////////////////
// XULTabpanelAccessible
////////////////////////////////////////////////////////////////////////////////

XULTabpanelAccessible::XULTabpanelAccessible(nsIContent* aContent,
                                             DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {}

role XULTabpanelAccessible::NativeRole() const { return roles::PROPERTYPAGE; }

Relation XULTabpanelAccessible::RelationByType(RelationType aType) const {
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType != RelationType::LABELLED_BY) return rel;

  // Expose 'LABELLED_BY' relation on tabpanel accessible for tab accessible.
  if (!mContent->GetParent()) return rel;

  nsCOMPtr<nsIDOMXULRelatedElement> tabpanelsElm =
      mContent->GetParent()->AsElement()->AsXULRelated();
  if (!tabpanelsElm) return rel;

  RefPtr<mozilla::dom::Element> tabElement;
  tabpanelsElm->GetRelatedElement(GetNode(), getter_AddRefs(tabElement));
  if (!tabElement) return rel;

  rel.AppendTarget(mDoc, tabElement);
  return rel;
}
