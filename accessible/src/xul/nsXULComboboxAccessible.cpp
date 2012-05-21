/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULComboboxAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsDocAccessible.h"
#include "nsCoreUtils.h"
#include "Role.h"
#include "States.h"

#include "nsIAutoCompleteInput.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXULComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULComboboxAccessible::
  nsXULComboboxAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc)
{
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::autocomplete, eIgnoreCase))
    mFlags |= eAutoCompleteAccessible;
  else
    mFlags |= eComboboxAccessible;
}

role
nsXULComboboxAccessible::NativeRole()
{
  return IsAutoComplete() ? roles::AUTOCOMPLETE : roles::COMBOBOX;
}

PRUint64
nsXULComboboxAccessible::NativeState()
{
  // As a nsComboboxAccessible we can have the following states:
  //     STATE_FOCUSED
  //     STATE_FOCUSABLE
  //     STATE_HASPOPUP
  //     STATE_EXPANDED
  //     STATE_COLLAPSED

  // Get focus status from base class
  PRUint64 states = nsAccessible::NativeState();

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mContent));
  if (menuList) {
    bool isOpen;
    menuList->GetOpen(&isOpen);
    if (isOpen) {
      states |= states::EXPANDED;
    }
    else {
      states |= states::COLLAPSED;
    }
  }

  states |= states::HASPOPUP | states::FOCUSABLE;

  return states;
}

void
nsXULComboboxAccessible::Description(nsString& aDescription)
{
  aDescription.Truncate();
  // Use description of currently focused option
  nsCOMPtr<nsIDOMXULMenuListElement> menuListElm(do_QueryInterface(mContent));
  if (!menuListElm)
    return;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> focusedOptionItem;
  menuListElm->GetSelectedItem(getter_AddRefs(focusedOptionItem));
  nsCOMPtr<nsIContent> focusedOptionContent =
    do_QueryInterface(focusedOptionItem);
  if (focusedOptionContent && mDoc) {
    nsAccessible* focusedOptionAcc = mDoc->GetAccessible(focusedOptionContent);
    if (focusedOptionAcc)
      focusedOptionAcc->Description(aDescription);
  }
}

void
nsXULComboboxAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  // The value is the option or text shown entered in the combobox.
  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mContent));
  if (menuList)
    menuList->GetLabel(aValue);
}

bool
nsXULComboboxAccessible::CanHaveAnonChildren()
{
  if (mContent->NodeInfo()->Equals(nsGkAtoms::textbox, kNameSpaceID_XUL) ||
      mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::editable,
                            nsGkAtoms::_true, eIgnoreCase)) {
    // Both the XUL <textbox type="autocomplete"> and <menulist editable="true"> widgets
    // use nsXULComboboxAccessible. We need to walk the anonymous children for these
    // so that the entry field is a child
    return true;
  }

  // Argument of false indicates we don't walk anonymous children for
  // menuitems
  return false;
}
PRUint8
nsXULComboboxAccessible::ActionCount()
{
  // Just one action (click).
  return 1;
}

NS_IMETHODIMP
nsXULComboboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != nsXULComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Programmaticaly toggle the combo box.
  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mContent));
  if (!menuList) {
    return NS_ERROR_FAILURE;
  }
  bool isDroppedDown;
  menuList->GetOpen(&isDroppedDown);
  return menuList->SetOpen(!isDroppedDown);
}

NS_IMETHODIMP
nsXULComboboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != nsXULComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Our action name is the reverse of our state:
  //     if we are close -> open is our name.
  //     if we are open -> close is our name.
  // Uses the frame to get the state, updated on every click.

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mContent));
  if (!menuList) {
    return NS_ERROR_FAILURE;
  }
  bool isDroppedDown;
  menuList->GetOpen(&isDroppedDown);
  if (isDroppedDown)
    aName.AssignLiteral("close"); 
  else
    aName.AssignLiteral("open"); 

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Widgets

bool
nsXULComboboxAccessible::IsActiveWidget() const
{
  if (IsAutoComplete() ||
     mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::editable,
                           nsGkAtoms::_true, eIgnoreCase)) {
    PRInt32 childCount = mChildren.Length();
    for (PRInt32 idx = 0; idx < childCount; idx++) {
      nsAccessible* child = mChildren[idx];
      if (child->Role() == roles::ENTRY)
        return FocusMgr()->HasDOMFocus(child->GetContent());
    }
    return false;
  }

  return FocusMgr()->HasDOMFocus(mContent);
}

bool
nsXULComboboxAccessible::AreItemsOperable() const
{
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

  nsCOMPtr<nsIDOMXULMenuListElement> menuListElm = do_QueryInterface(mContent);
  if (menuListElm) {
    bool isOpen = false;
    menuListElm->GetOpen(&isOpen);
    return isOpen;
  }

  return false;
}
