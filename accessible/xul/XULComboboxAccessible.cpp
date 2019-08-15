/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULComboboxAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "DocAccessible.h"
#include "nsCoreUtils.h"
#include "Role.h"
#include "States.h"

#include "nsIAutoCompleteInput.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULComboboxAccessible::XULComboboxAccessible(nsIContent* aContent,
                                             DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                         nsGkAtoms::autocomplete, eIgnoreCase))
    mGenericTypes |= eAutoComplete;
  else
    mGenericTypes |= eCombobox;
}

role XULComboboxAccessible::NativeRole() const {
  return IsAutoComplete() ? roles::AUTOCOMPLETE : roles::COMBOBOX;
}

uint64_t XULComboboxAccessible::NativeState() const {
  // As a nsComboboxAccessible we can have the following states:
  //     STATE_FOCUSED
  //     STATE_FOCUSABLE
  //     STATE_HASPOPUP
  //     STATE_EXPANDED
  //     STATE_COLLAPSED

  // Get focus status from base class
  uint64_t state = Accessible::NativeState();

  nsCOMPtr<nsIDOMXULMenuListElement> menuList = Elm()->AsXULMenuList();
  if (menuList) {
    bool isOpen = false;
    menuList->GetOpen(&isOpen);
    if (isOpen)
      state |= states::EXPANDED;
    else
      state |= states::COLLAPSED;
  }

  return state | states::HASPOPUP;
}

void XULComboboxAccessible::Description(nsString& aDescription) {
  aDescription.Truncate();
  // Use description of currently focused option
  nsCOMPtr<nsIDOMXULMenuListElement> menuListElm = Elm()->AsXULMenuList();
  if (!menuListElm) return;

  nsCOMPtr<Element> focusedOptionItem;
  menuListElm->GetSelectedItem(getter_AddRefs(focusedOptionItem));
  if (focusedOptionItem && mDoc) {
    Accessible* focusedOptionAcc = mDoc->GetAccessible(focusedOptionItem);
    if (focusedOptionAcc) focusedOptionAcc->Description(aDescription);
  }
}

void XULComboboxAccessible::Value(nsString& aValue) const {
  aValue.Truncate();

  // The value is the option or text shown entered in the combobox.
  nsCOMPtr<nsIDOMXULMenuListElement> menuList = Elm()->AsXULMenuList();
  if (menuList) menuList->GetLabel(aValue);
}

uint8_t XULComboboxAccessible::ActionCount() const {
  // Just one action (click).
  return 1;
}

bool XULComboboxAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != XULComboboxAccessible::eAction_Click) return false;

  // Programmaticaly toggle the combo box.
  nsCOMPtr<nsIDOMXULMenuListElement> menuList = Elm()->AsXULMenuList();
  if (!menuList) return false;

  bool isDroppedDown = false;
  menuList->GetOpen(&isDroppedDown);
  menuList->SetOpen(!isDroppedDown);
  return true;
}

void XULComboboxAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();
  if (aIndex != XULComboboxAccessible::eAction_Click) return;

  nsCOMPtr<nsIDOMXULMenuListElement> menuList = Elm()->AsXULMenuList();
  if (!menuList) return;

  bool isDroppedDown = false;
  menuList->GetOpen(&isDroppedDown);
  if (isDroppedDown)
    aName.AssignLiteral("close");
  else
    aName.AssignLiteral("open");
}

////////////////////////////////////////////////////////////////////////////////
// Widgets

bool XULComboboxAccessible::IsActiveWidget() const {
  if (IsAutoComplete() ||
      mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::editable,
                                         nsGkAtoms::_true, eIgnoreCase)) {
    int32_t childCount = mChildren.Length();
    for (int32_t idx = 0; idx < childCount; idx++) {
      Accessible* child = mChildren[idx];
      if (child->Role() == roles::ENTRY)
        return FocusMgr()->HasDOMFocus(child->GetContent());
    }
    return false;
  }

  return FocusMgr()->HasDOMFocus(mContent);
}

bool XULComboboxAccessible::AreItemsOperable() const {
  if (IsAutoComplete()) {
    nsCOMPtr<nsIAutoCompleteInput> autoCompleteInputElm =
        do_QueryInterface(mContent);
    if (autoCompleteInputElm) {
      bool isOpen = false;
      autoCompleteInputElm->GetPopupOpen(&isOpen);
      return isOpen;
    }
    return false;
  }

  nsCOMPtr<nsIDOMXULMenuListElement> menuListElm = Elm()->AsXULMenuList();
  if (menuListElm) {
    bool isOpen = false;
    menuListElm->GetOpen(&isOpen);
    return isOpen;
  }

  return false;
}
