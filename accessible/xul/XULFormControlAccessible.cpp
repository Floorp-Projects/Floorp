/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULFormControlAccessible.h"

#include "Accessible-inl.h"
#include "HTMLFormControlAccessible.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "nsIAccessibleRelation.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "TreeWalker.h"
#include "XULMenuAccessible.h"

#include "nsIDOMNSEditableElement.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIEditor.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsNameSpaceManager.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULButtonAccessible::
  XULButtonAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  if (ContainsMenu()) {
    mGenericTypes |= eMenuButton;
  } else {
    mGenericTypes |= eButton;
  }
}

XULButtonAccessible::~XULButtonAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: nsISupports

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: nsIAccessible

uint8_t
XULButtonAccessible::ActionCount() const
{
  return 1;
}

void
XULButtonAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click)
    aName.AssignLiteral("press");
}

bool
XULButtonAccessible::DoAction(uint8_t aIndex) const
{
  if (aIndex != 0)
    return false;

  DoCommand();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: Accessible

role
XULButtonAccessible::NativeRole() const
{
  return roles::PUSHBUTTON;
}

uint64_t
XULButtonAccessible::NativeState() const
{
  // Possible states: focused, focusable, unavailable(disabled).

  // get focus and disable status from base class
  uint64_t state = Accessible::NativeState();

  // Buttons can be checked -- they simply appear pressed in rather than checked
  nsCOMPtr<nsIDOMXULButtonElement> xulButtonElement(do_QueryInterface(mContent));
  if (xulButtonElement) {
    nsAutoString type;
    xulButtonElement->GetType(type);
    if (type.EqualsLiteral("checkbox") || type.EqualsLiteral("radio")) {
      state |= states::CHECKABLE;
      bool checked = false;
      xulButtonElement->GetChecked(&checked);
      if (checked) {
        state |= states::PRESSED;
      }
    }
  }

  if (ContainsMenu())
    state |= states::HASPOPUP;

  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::_default))
    state |= states::DEFAULT;

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: Widgets

bool
XULButtonAccessible::IsWidget() const
{
  return true;
}

bool
XULButtonAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
XULButtonAccessible::AreItemsOperable() const
{
  if (IsMenuButton()) {
    Accessible* menuPopup = mChildren.SafeElementAt(0, nullptr);
    if (menuPopup) {
      nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(menuPopup->GetFrame());
      return menuPopupFrame->IsOpen();
    }
  }
  return false; // no items
}

Accessible*
XULButtonAccessible::ContainerWidget() const
{
  if (IsMenuButton() && mParent && mParent->IsAutoComplete())
    return mParent;
  return nullptr;
}

bool
XULButtonAccessible::IsAcceptableChild(nsIContent* aEl) const
{
  // In general XUL button has not accessible children. Nevertheless menu
  // buttons can have button (@type="menu-button") and popup accessibles
  // (@type="menu-button", @type="menu" or columnpicker.

  // Get an accessible for menupopup or popup elements.
  if (aEl->IsXULElement(nsGkAtoms::menupopup) ||
      aEl->IsXULElement(nsGkAtoms::popup)) {
    return true;
  }

  // Button and toolbarbutton are real buttons. Get an accessible
  // for it. Ignore dropmarker button which is placed as a last child.
  if ((!aEl->IsXULElement(nsGkAtoms::button) &&
       !aEl->IsXULElement(nsGkAtoms::toolbarbutton)) ||
      aEl->IsXULElement(nsGkAtoms::dropMarker)) {
    return false;
  }

  return mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                            nsGkAtoms::menuButton, eCaseMatters);
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible protected

bool
XULButtonAccessible::ContainsMenu() const
{
  static Element::AttrValuesArray strings[] =
    {&nsGkAtoms::menu, &nsGkAtoms::menuButton, nullptr};

  return mContent->AsElement()->FindAttrValueIn(kNameSpaceID_None,
                                                nsGkAtoms::type,
                                                strings, eCaseMatters) >= 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULDropmarkerAccessible
////////////////////////////////////////////////////////////////////////////////

XULDropmarkerAccessible::
  XULDropmarkerAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

uint8_t
XULDropmarkerAccessible::ActionCount() const
{
  return 1;
}

bool
XULDropmarkerAccessible::DropmarkerOpen(bool aToggleOpen) const
{
  bool isOpen = false;

  nsIContent* parent = mContent->GetFlattenedTreeParent();

  while (parent) {
    nsCOMPtr<nsIDOMXULButtonElement> parentButtonElement =
      do_QueryInterface(parent);
    if (parentButtonElement) {
      parentButtonElement->GetOpen(&isOpen);
      if (aToggleOpen)
        parentButtonElement->SetOpen(!isOpen);
      return isOpen;
    }

    nsCOMPtr<nsIDOMXULMenuListElement> parentMenuListElement =
      do_QueryInterface(parent);
    if (parentMenuListElement) {
      parentMenuListElement->GetOpen(&isOpen);
      if (aToggleOpen)
        parentMenuListElement->SetOpen(!isOpen);
      return isOpen;
    }
    parent = parent->GetFlattenedTreeParent();
  }

  return isOpen;
}

void
XULDropmarkerAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();
  if (aIndex == eAction_Click) {
    if (DropmarkerOpen(false))
      aName.AssignLiteral("close");
    else
      aName.AssignLiteral("open");
  }
}

bool
XULDropmarkerAccessible::DoAction(uint8_t index) const
{
  if (index == eAction_Click) {
    DropmarkerOpen(true); // Reverse the open attribute
    return true;
  }
  return false;
}

role
XULDropmarkerAccessible::NativeRole() const
{
  return roles::PUSHBUTTON;
}

uint64_t
XULDropmarkerAccessible::NativeState() const
{
  return DropmarkerOpen(false) ? states::PRESSED : 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULCheckboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULCheckboxAccessible::
  XULCheckboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

role
XULCheckboxAccessible::NativeRole() const
{
  return roles::CHECKBUTTON;
}

uint8_t
XULCheckboxAccessible::ActionCount() const
{
  return 1;
}

void
XULCheckboxAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    if (NativeState() & states::CHECKED)
      aName.AssignLiteral("uncheck");
    else
      aName.AssignLiteral("check");
  }
}

bool
XULCheckboxAccessible::DoAction(uint8_t aIndex) const
{
  if (aIndex != eAction_Click)
    return false;

  DoCommand();
  return true;
}

uint64_t
XULCheckboxAccessible::NativeState() const
{
  // Possible states: focused, focusable, unavailable(disabled), checked
  // Get focus and disable status from base class
  uint64_t state = LeafAccessible::NativeState();

  state |= states::CHECKABLE;

  // Determine Checked state
  nsCOMPtr<nsIDOMXULCheckboxElement> xulCheckboxElement =
    do_QueryInterface(mContent);
  if (xulCheckboxElement) {
    bool checked = false;
    xulCheckboxElement->GetChecked(&checked);
    if (checked) {
      state |= states::CHECKED;
    }
  }

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// XULGroupboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULGroupboxAccessible::
  XULGroupboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULGroupboxAccessible::NativeRole() const
{
  return roles::GROUPING;
}

ENameValueFlag
XULGroupboxAccessible::NativeName(nsString& aName) const
{
  // XXX: we use the first related accessible only.
  Accessible* label =
    RelationByType(RelationType::LABELLED_BY).Next();
  if (label)
    return label->Name(aName);

  return eNameOK;
}

Relation
XULGroupboxAccessible::RelationByType(RelationType aType) const
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType != RelationType::LABELLED_BY)
    return rel;

  // The label for xul:groupbox is generated from xul:label that is
  // inside the anonymous content of the xul:caption.
  // The xul:label has an accessible object but the xul:caption does not
  uint32_t childCount = ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* childAcc = GetChildAt(childIdx);
    if (childAcc->Role() == roles::LABEL) {
      // Ensure that it's our label
      Relation reverseRel = childAcc->RelationByType(RelationType::LABEL_FOR);
      Accessible* testGroupbox = nullptr;
      while ((testGroupbox = reverseRel.Next()))
        if (testGroupbox == this) {
          // The <label> points back to this groupbox
          rel.AppendTarget(childAcc);
        }
    }
  }

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULRadioButtonAccessible::
  XULRadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  RadioButtonAccessible(aContent, aDoc)
{
}

uint64_t
XULRadioButtonAccessible::NativeState() const
{
  uint64_t state = LeafAccessible::NativeState();
  state |= states::CHECKABLE;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> radioButton =
    do_QueryInterface(mContent);
  if (radioButton) {
    bool selected = false;   // Radio buttons can be selected
    radioButton->GetSelected(&selected);
    if (selected) {
      state |= states::CHECKED;
    }
  }

  return state;
}

uint64_t
XULRadioButtonAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ? states::UNAVAILABLE : states::FOCUSABLE;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioButtonAccessible: Widgets

Accessible*
XULRadioButtonAccessible::ContainerWidget() const
{
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

XULRadioGroupAccessible::
  XULRadioGroupAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULSelectControlAccessible(aContent, aDoc)
{
}

role
XULRadioGroupAccessible::NativeRole() const
{
  return roles::RADIO_GROUP;
}

uint64_t
XULRadioGroupAccessible::NativeInteractiveState() const
{
  // The radio group is not focusable. Sometimes the focus controller will
  // report that it is focused. That means that the actual selected radio button
  // should be considered focused.
  return NativelyUnavailable() ? states::UNAVAILABLE : 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioGroupAccessible: Widgets

bool
XULRadioGroupAccessible::IsWidget() const
{
  return true;
}

bool
XULRadioGroupAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
XULRadioGroupAccessible::AreItemsOperable() const
{
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// XULStatusBarAccessible
////////////////////////////////////////////////////////////////////////////////

XULStatusBarAccessible::
  XULStatusBarAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULStatusBarAccessible::NativeRole() const
{
  return roles::STATUSBAR;
}


////////////////////////////////////////////////////////////////////////////////
// XULToolbarButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarButtonAccessible::
  XULToolbarButtonAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULButtonAccessible(aContent, aDoc)
{
}

void
XULToolbarButtonAccessible::GetPositionAndSizeInternal(int32_t* aPosInSet,
                                                       int32_t* aSetSize)
{
  int32_t setSize = 0;
  int32_t posInSet = 0;

  Accessible* parent = Parent();
  if (!parent)
    return;

  uint32_t childCount = parent->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = parent->GetChildAt(childIdx);
    if (IsSeparator(child)) { // end of a group of buttons
      if (posInSet)
        break; // we've found our group, so we're done

      setSize = 0; // not our group, so start a new group

    } else {
      setSize++; // another button in the group

      if (child == this)
        posInSet = setSize; // we've found our button
    }
  }

  *aPosInSet = posInSet;
  *aSetSize = setSize;
}

bool
XULToolbarButtonAccessible::IsSeparator(Accessible* aAccessible)
{
  nsIContent* content = aAccessible->GetContent();
  return content && content->IsAnyOfXULElements(nsGkAtoms::toolbarseparator,
                                                nsGkAtoms::toolbarspacer,
                                                nsGkAtoms::toolbarspring);
}


////////////////////////////////////////////////////////////////////////////////
// XULToolbarAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarAccessible::
  XULToolbarAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULToolbarAccessible::NativeRole() const
{
  return roles::TOOLBAR;
}

ENameValueFlag
XULToolbarAccessible::NativeName(nsString& aName) const
{
  if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::toolbarname, aName))
    aName.CompressWhitespace();

  return eNameOK;
}


////////////////////////////////////////////////////////////////////////////////
// XULToolbarAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarSeparatorAccessible::
  XULToolbarSeparatorAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

role
XULToolbarSeparatorAccessible::NativeRole() const
{
  return roles::SEPARATOR;
}

uint64_t
XULToolbarSeparatorAccessible::NativeState() const
{
  return 0;
}
