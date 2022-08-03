/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULFormControlAccessible.h"

#include "LocalAccessible-inl.h"
#include "HTMLFormControlAccessible.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "TreeWalker.h"
#include "XULMenuAccessible.h"

#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULRadioGroupElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsNameSpaceManager.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULButtonAccessible::XULButtonAccessible(nsIContent* aContent,
                                         DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {
  if (ContainsMenu()) {
    mGenericTypes |= eMenuButton;
  } else {
    mGenericTypes |= eButton;
  }
}

XULButtonAccessible::~XULButtonAccessible() {}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: nsISupports

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: nsIAccessible

bool XULButtonAccessible::HasPrimaryAction() const { return true; }

void XULButtonAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == eAction_Click) aName.AssignLiteral("press");
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: LocalAccessible

role XULButtonAccessible::NativeRole() const {
  // Buttons can be checked; they simply appear pressed in rather than checked.
  // In this case, we must expose them as toggle buttons.
  nsCOMPtr<nsIDOMXULButtonElement> xulButtonElement = Elm()->AsXULButton();
  if (xulButtonElement) {
    nsAutoString type;
    xulButtonElement->GetType(type);
    if (type.EqualsLiteral("checkbox") || type.EqualsLiteral("radio")) {
      return roles::TOGGLE_BUTTON;
    }
  }
  return roles::PUSHBUTTON;
}

uint64_t XULButtonAccessible::NativeState() const {
  // Possible states: focused, focusable, unavailable(disabled).

  // get focus and disable status from base class
  uint64_t state = LocalAccessible::NativeState();

  nsCOMPtr<nsIDOMXULButtonElement> xulButtonElement = Elm()->AsXULButton();
  if (xulButtonElement) {
    // Some buttons can have their checked state set without being of type
    // checkbox or radio. Expose the pressed state unconditionally.
    bool checked = false;
    xulButtonElement->GetChecked(&checked);
    if (checked) {
      state |= states::PRESSED;
    }
  }

  if (ContainsMenu()) state |= states::HASPOPUP;

  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::_default)) {
    state |= states::DEFAULT;
  }

  return state;
}

bool XULButtonAccessible::AttributeChangesState(nsAtom* aAttribute) {
  if (aAttribute == nsGkAtoms::checked) {
    return true;
  }
  return AccessibleWrap::AttributeChangesState(aAttribute);
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: Widgets

bool XULButtonAccessible::IsWidget() const { return true; }

bool XULButtonAccessible::IsActiveWidget() const {
  return FocusMgr()->HasDOMFocus(mContent);
}

bool XULButtonAccessible::AreItemsOperable() const {
  if (IsMenuButton()) {
    LocalAccessible* menuPopup = mChildren.SafeElementAt(0, nullptr);
    if (menuPopup) {
      nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(menuPopup->GetFrame());
      return menuPopupFrame->IsOpen();
    }
  }
  return false;  // no items
}

bool XULButtonAccessible::IsAcceptableChild(nsIContent* aEl) const {
  // In general XUL buttons should not have accessible children. However:
  return
      //   menu buttons can have popup accessibles (@type="menu" or
      //   columnpicker).
      aEl->IsXULElement(nsGkAtoms::menupopup) ||
      aEl->IsXULElement(nsGkAtoms::popup) ||
      // A XUL button can be labelled by a child text node, so we need to allow
      // that as a child so it will be picked up when computing name from
      // subtree.
      aEl->IsText();
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible protected

bool XULButtonAccessible::ContainsMenu() const {
  return mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                            nsGkAtoms::menu, eCaseMatters);
}

////////////////////////////////////////////////////////////////////////////////
// XULDropmarkerAccessible
////////////////////////////////////////////////////////////////////////////////

XULDropmarkerAccessible::XULDropmarkerAccessible(nsIContent* aContent,
                                                 DocAccessible* aDoc)
    : LeafAccessible(aContent, aDoc) {}

bool XULDropmarkerAccessible::HasPrimaryAction() const { return true; }

bool XULDropmarkerAccessible::DropmarkerOpen(bool aToggleOpen) const {
  bool isOpen = false;

  nsIContent* parent = mContent->GetFlattenedTreeParent();

  while (parent) {
    nsCOMPtr<nsIDOMXULButtonElement> parentButtonElement =
        parent->AsElement()->AsXULButton();
    if (parentButtonElement) {
      parentButtonElement->GetOpen(&isOpen);
      if (aToggleOpen) parentButtonElement->SetOpen(!isOpen);
      return isOpen;
    }

    nsCOMPtr<nsIDOMXULMenuListElement> parentMenuListElement =
        parent->AsElement()->AsXULMenuList();
    if (parentMenuListElement) {
      parentMenuListElement->GetOpen(&isOpen);
      if (aToggleOpen) parentMenuListElement->SetOpen(!isOpen);
      return isOpen;
    }
    parent = parent->GetFlattenedTreeParent();
  }

  return isOpen;
}

void XULDropmarkerAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();
  if (aIndex == eAction_Click) {
    if (DropmarkerOpen(false)) {
      aName.AssignLiteral("close");
    } else {
      aName.AssignLiteral("open");
    }
  }
}

bool XULDropmarkerAccessible::DoAction(uint8_t index) const {
  if (index == eAction_Click) {
    DropmarkerOpen(true);  // Reverse the open attribute
    return true;
  }
  return false;
}

role XULDropmarkerAccessible::NativeRole() const { return roles::PUSHBUTTON; }

uint64_t XULDropmarkerAccessible::NativeState() const {
  return DropmarkerOpen(false) ? states::PRESSED : 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULGroupboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULGroupboxAccessible::XULGroupboxAccessible(nsIContent* aContent,
                                             DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {}

role XULGroupboxAccessible::NativeRole() const { return roles::GROUPING; }

ENameValueFlag XULGroupboxAccessible::NativeName(nsString& aName) const {
  // XXX: we use the first related accessible only.
  LocalAccessible* label =
      RelationByType(RelationType::LABELLED_BY).LocalNext();
  if (label) return label->Name(aName);

  return eNameOK;
}

Relation XULGroupboxAccessible::RelationByType(RelationType aType) const {
  Relation rel = AccessibleWrap::RelationByType(aType);

  // The label for xul:groupbox is generated from the first xul:label
  if (aType == RelationType::LABELLED_BY && ChildCount() > 0) {
    LocalAccessible* childAcc = LocalChildAt(0);
    if (childAcc->Role() == roles::LABEL &&
        childAcc->GetContent()->IsXULElement(nsGkAtoms::label)) {
      rel.AppendTarget(childAcc);
    }
  }

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULRadioButtonAccessible::XULRadioButtonAccessible(nsIContent* aContent,
                                                   DocAccessible* aDoc)
    : RadioButtonAccessible(aContent, aDoc) {}

uint64_t XULRadioButtonAccessible::NativeState() const {
  uint64_t state = LeafAccessible::NativeState();
  state |= states::CHECKABLE;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> radioButton =
      Elm()->AsXULSelectControlItem();
  if (radioButton) {
    bool selected = false;  // Radio buttons can be selected
    radioButton->GetSelected(&selected);
    if (selected) {
      state |= states::CHECKED;
    }
  }

  return state;
}

uint64_t XULRadioButtonAccessible::NativeInteractiveState() const {
  return NativelyUnavailable() ? states::UNAVAILABLE : states::FOCUSABLE;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioButtonAccessible: Widgets

LocalAccessible* XULRadioButtonAccessible::ContainerWidget() const {
  return mParent;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioGroupAccessible
////////////////////////////////////////////////////////////////////////////////

/**
 * XUL Radio Group
 *   The Radio Group proxies for the Radio Buttons themselves. The Group gets
 *   focus whereas the Buttons do not. So we only have an accessible object for
 *   this for the purpose of getting the proper RadioButton. Need this here to
 *   avoid circular reference problems when navigating the accessible tree and
 *   for getting to the radiobuttons.
 */

XULRadioGroupAccessible::XULRadioGroupAccessible(nsIContent* aContent,
                                                 DocAccessible* aDoc)
    : XULSelectControlAccessible(aContent, aDoc) {}

role XULRadioGroupAccessible::NativeRole() const { return roles::RADIO_GROUP; }

uint64_t XULRadioGroupAccessible::NativeInteractiveState() const {
  // The radio group is not focusable. Sometimes the focus controller will
  // report that it is focused. That means that the actual selected radio button
  // should be considered focused.
  return NativelyUnavailable() ? states::UNAVAILABLE : 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioGroupAccessible: Widgets

bool XULRadioGroupAccessible::IsWidget() const { return true; }

bool XULRadioGroupAccessible::IsActiveWidget() const {
  return FocusMgr()->HasDOMFocus(mContent);
}

bool XULRadioGroupAccessible::AreItemsOperable() const { return true; }

LocalAccessible* XULRadioGroupAccessible::CurrentItem() const {
  if (!mSelectControl) {
    return nullptr;
  }

  RefPtr<dom::Element> currentItemElm;
  nsCOMPtr<nsIDOMXULRadioGroupElement> group =
      mSelectControl->AsXULRadioGroup();
  if (group) {
    group->GetFocusedItem(getter_AddRefs(currentItemElm));
  }

  if (currentItemElm) {
    DocAccessible* document = Document();
    if (document) {
      return document->GetAccessible(currentItemElm);
    }
  }

  return nullptr;
}

void XULRadioGroupAccessible::SetCurrentItem(const LocalAccessible* aItem) {
  if (!mSelectControl) {
    return;
  }

  nsCOMPtr<dom::Element> itemElm = aItem->Elm();
  nsCOMPtr<nsIDOMXULRadioGroupElement> group =
      mSelectControl->AsXULRadioGroup();
  if (group) {
    group->SetFocusedItem(itemElm);
  }
}

////////////////////////////////////////////////////////////////////////////////
// XULStatusBarAccessible
////////////////////////////////////////////////////////////////////////////////

XULStatusBarAccessible::XULStatusBarAccessible(nsIContent* aContent,
                                               DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {}

role XULStatusBarAccessible::NativeRole() const { return roles::STATUSBAR; }

////////////////////////////////////////////////////////////////////////////////
// XULToolbarButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarButtonAccessible::XULToolbarButtonAccessible(nsIContent* aContent,
                                                       DocAccessible* aDoc)
    : XULButtonAccessible(aContent, aDoc) {}

void XULToolbarButtonAccessible::GetPositionAndSetSize(int32_t* aPosInSet,
                                                       int32_t* aSetSize) {
  int32_t setSize = 0;
  int32_t posInSet = 0;

  LocalAccessible* parent = LocalParent();
  if (!parent) return;

  uint32_t childCount = parent->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    LocalAccessible* child = parent->LocalChildAt(childIdx);
    if (IsSeparator(child)) {  // end of a group of buttons
      if (posInSet) break;     // we've found our group, so we're done

      setSize = 0;  // not our group, so start a new group

    } else {
      setSize++;  // another button in the group

      if (child == this) posInSet = setSize;  // we've found our button
    }
  }

  *aPosInSet = posInSet;
  *aSetSize = setSize;
}

bool XULToolbarButtonAccessible::IsSeparator(LocalAccessible* aAccessible) {
  nsIContent* content = aAccessible->GetContent();
  return content && content->IsAnyOfXULElements(nsGkAtoms::toolbarseparator,
                                                nsGkAtoms::toolbarspacer,
                                                nsGkAtoms::toolbarspring);
}

////////////////////////////////////////////////////////////////////////////////
// XULToolbarButtonAccessible: Widgets

bool XULToolbarButtonAccessible::IsAcceptableChild(nsIContent* aEl) const {
  // In general XUL button has not accessible children. Nevertheless menu
  // buttons can have popup accessibles (@type="menu" or columnpicker).
  // Also: Toolbar buttons can have labels as children.
  // But only if the label attribute is not present.
  return aEl->IsXULElement(nsGkAtoms::menupopup) ||
         aEl->IsXULElement(nsGkAtoms::popup) ||
         (aEl->IsXULElement(nsGkAtoms::label) &&
          !mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::label));
}

////////////////////////////////////////////////////////////////////////////////
// XULToolbarAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarAccessible::XULToolbarAccessible(nsIContent* aContent,
                                           DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {}

role XULToolbarAccessible::NativeRole() const { return roles::TOOLBAR; }

ENameValueFlag XULToolbarAccessible::NativeName(nsString& aName) const {
  if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::toolbarname,
                                     aName)) {
    aName.CompressWhitespace();
  }

  return eNameOK;
}

////////////////////////////////////////////////////////////////////////////////
// XULToolbarAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarSeparatorAccessible::XULToolbarSeparatorAccessible(
    nsIContent* aContent, DocAccessible* aDoc)
    : LeafAccessible(aContent, aDoc) {}

role XULToolbarSeparatorAccessible::NativeRole() const {
  return roles::SEPARATOR;
}

uint64_t XULToolbarSeparatorAccessible::NativeState() const { return 0; }
