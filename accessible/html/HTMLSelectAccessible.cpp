/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLSelectAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "nsEventShell.h"
#include "nsTextEquivUtils.h"
#include "Role.h"
#include "States.h"

#include "nsCOMPtr.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "nsIComboboxControlFrame.h"
#include "nsContainerFrame.h"
#include "nsIListControlFrame.h"

using namespace mozilla::a11y;
using namespace mozilla::dom;

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLSelectListAccessible::
  HTMLSelectListAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  mGenericTypes |= eListControl | eSelect;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible: Accessible public

uint64_t
HTMLSelectListAccessible::NativeState()
{
  uint64_t state = AccessibleWrap::NativeState();
  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple))
    state |= states::MULTISELECTABLE | states::EXTSELECTABLE;

  return state;
}

role
HTMLSelectListAccessible::NativeRole() const
{
  return roles::LISTBOX;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible: SelectAccessible

bool
HTMLSelectListAccessible::SelectAll()
{
  return mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple) ?
    AccessibleWrap::SelectAll() : false;
}

bool
HTMLSelectListAccessible::UnselectAll()
{
  return mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple) ?
    AccessibleWrap::UnselectAll() : false;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible: Widgets

bool
HTMLSelectListAccessible::IsWidget() const
{
  return true;
}

bool
HTMLSelectListAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
HTMLSelectListAccessible::AreItemsOperable() const
{
  return true;
}

Accessible*
HTMLSelectListAccessible::CurrentItem()
{
  nsIListControlFrame* listControlFrame = do_QueryFrame(GetFrame());
  if (listControlFrame) {
    nsCOMPtr<nsIContent> activeOptionNode = listControlFrame->GetCurrentOption();
    if (activeOptionNode) {
      DocAccessible* document = Document();
      if (document)
        return document->GetAccessible(activeOptionNode);
    }
  }
  return nullptr;
}

void
HTMLSelectListAccessible::SetCurrentItem(Accessible* aItem)
{
  if (!aItem->GetContent()->IsElement())
    return;

  aItem->GetContent()->AsElement()->SetAttr(kNameSpaceID_None,
                                            nsGkAtoms::selected,
                                            NS_LITERAL_STRING("true"),
                                            true);
}

bool
HTMLSelectListAccessible::IsAcceptableChild(nsIContent* aEl) const
{
  return aEl->IsAnyOfHTMLElements(nsGkAtoms::option, nsGkAtoms::optgroup);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectOptionAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLSelectOptionAccessible::
  HTMLSelectOptionAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectOptionAccessible: Accessible public

role
HTMLSelectOptionAccessible::NativeRole() const
{
  if (GetCombobox())
    return roles::COMBOBOX_OPTION;

  return roles::OPTION;
}

ENameValueFlag
HTMLSelectOptionAccessible::NativeName(nsString& aName)
{
  // CASE #1 -- great majority of the cases
  // find the label attribute - this is what the W3C says we should use
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
  if (!aName.IsEmpty())
    return eNameOK;

  // CASE #2 -- no label parameter, get the first child,
  // use it if it is a text node
  nsIContent* text = mContent->GetFirstChild();
  if (text && text->IsText()) {
    nsTextEquivUtils::AppendTextEquivFromTextContent(text, &aName);
    aName.CompressWhitespace();
    return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
  }

  return eNameOK;
}

uint64_t
HTMLSelectOptionAccessible::NativeState()
{
  // As a HTMLSelectOptionAccessible we can have the following states:
  // SELECTABLE, SELECTED, FOCUSED, FOCUSABLE, OFFSCREEN
  // Upcall to Accessible, but skip HyperTextAccessible impl
  // because we don't want EDITABLE or SELECTABLE_TEXT
  uint64_t state = Accessible::NativeState();

  Accessible* select = GetSelect();
  if (!select)
    return state;

  uint64_t selectState = select->State();
  if (selectState & states::INVISIBLE)
    return state;

  // Are we selected?
  HTMLOptionElement* option = HTMLOptionElement::FromNode(mContent);
  bool selected = option && option->Selected();
  if (selected)
    state |= states::SELECTED;

  if (selectState & states::OFFSCREEN) {
    state |= states::OFFSCREEN;
  } else if (selectState & states::COLLAPSED) {
    // <select> is COLLAPSED: add OFFSCREEN, if not the currently
    // visible option
    if (!selected) {
      state |= states::OFFSCREEN;
      // Ensure the invisible state is removed. Otherwise, group info will skip
      // this option. Furthermore, this gets cached and this doesn't get
      // invalidated even once the select is expanded.
      state &= ~states::INVISIBLE;
    } else {
      // Clear offscreen and invisible for currently showing option
      state &= ~(states::OFFSCREEN | states::INVISIBLE);
      state |= selectState & states::OPAQUE1;
    }
  } else {
    // XXX list frames are weird, don't rely on Accessible's general
    // visibility implementation unless they get reimplemented in layout
    state &= ~states::OFFSCREEN;
    // <select> is not collapsed: compare bounds to calculate OFFSCREEN
    Accessible* listAcc = Parent();
    if (listAcc) {
      nsIntRect optionRect = Bounds();
      nsIntRect listRect = listAcc->Bounds();
      if (optionRect.Y() < listRect.Y() ||
          optionRect.YMost() > listRect.YMost()) {
        state |= states::OFFSCREEN;
      }
    }
  }

  return state;
}

uint64_t
HTMLSelectOptionAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ?
    states::UNAVAILABLE : states::FOCUSABLE | states::SELECTABLE;
}

int32_t
HTMLSelectOptionAccessible::GetLevelInternal()
{
  nsIContent* parentContent = mContent->GetParent();

  int32_t level =
    parentContent->NodeInfo()->Equals(nsGkAtoms::optgroup) ? 2 : 1;

  if (level == 1 && Role() != roles::HEADING)
    level = 0; // In a single level list, the level is irrelevant

  return level;
}

nsRect
HTMLSelectOptionAccessible::RelativeBounds(nsIFrame** aBoundingFrame) const
{
  Accessible* combobox = GetCombobox();
  if (combobox && (combobox->State() & states::COLLAPSED))
    return combobox->RelativeBounds(aBoundingFrame);

  return HyperTextAccessibleWrap::RelativeBounds(aBoundingFrame);
}

void
HTMLSelectOptionAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Select)
    aName.AssignLiteral("select");
}

uint8_t
HTMLSelectOptionAccessible::ActionCount()
{
  return 1;
}

bool
HTMLSelectOptionAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Select)
    return false;

  DoCommand();
  return true;
}

void
HTMLSelectOptionAccessible::SetSelected(bool aSelect)
{
  HTMLOptionElement* option = HTMLOptionElement::FromNode(mContent);
  if (option)
    option->SetSelected(aSelect);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectOptionAccessible: Widgets

Accessible*
HTMLSelectOptionAccessible::ContainerWidget() const
{
  Accessible* parent = Parent();
  if (parent && parent->IsHTMLOptGroup())
    parent = parent->Parent();

  return parent && parent->IsListControl() ? parent : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectOptGroupAccessible
////////////////////////////////////////////////////////////////////////////////

role
HTMLSelectOptGroupAccessible::NativeRole() const
{
  return roles::GROUPING;
}

uint64_t
HTMLSelectOptGroupAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ? states::UNAVAILABLE : 0;
}

bool
HTMLSelectOptGroupAccessible::IsAcceptableChild(nsIContent* aEl) const
{
  return aEl->IsCharacterData() || aEl->IsHTMLElement(nsGkAtoms::option);
}

uint8_t
HTMLSelectOptGroupAccessible::ActionCount()
{
  return 0;
}

void
HTMLSelectOptGroupAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();
}

bool
HTMLSelectOptGroupAccessible::DoAction(uint8_t aIndex)
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLComboboxAccessible::
  HTMLComboboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  mType = eHTMLComboboxType;
  mGenericTypes |= eCombobox;
  mStateFlags |= eNoKidsFromDOM;

  nsIComboboxControlFrame* comboFrame = do_QueryFrame(GetFrame());
  if (comboFrame) {
    nsIFrame* listFrame = comboFrame->GetDropDown();
    if (listFrame) {
      mListAccessible = new HTMLComboboxListAccessible(mParent, mContent, mDoc);
      Document()->BindToDocument(mListAccessible, nullptr);
      AppendChild(mListAccessible);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: Accessible

role
HTMLComboboxAccessible::NativeRole() const
{
  return roles::COMBOBOX;
}

bool
HTMLComboboxAccessible::RemoveChild(Accessible* aChild)
{
  MOZ_ASSERT(aChild == mListAccessible);
  if (AccessibleWrap::RemoveChild(aChild)) {
    mListAccessible = nullptr;
    return true;
  }
  return false;
}

void
HTMLComboboxAccessible::Shutdown()
{
  MOZ_ASSERT(mDoc->IsDefunct() || !mListAccessible);
  if (mListAccessible) {
    mListAccessible->Shutdown();
    mListAccessible = nullptr;
  }

  AccessibleWrap::Shutdown();
}

uint64_t
HTMLComboboxAccessible::NativeState()
{
  // As a HTMLComboboxAccessible we can have the following states:
  // FOCUSED, FOCUSABLE, HASPOPUP, EXPANDED, COLLAPSED
  // Get focus status from base class
  uint64_t state = Accessible::NativeState();

  nsIComboboxControlFrame* comboFrame = do_QueryFrame(GetFrame());
  if (comboFrame && comboFrame->IsDroppedDown())
    state |= states::EXPANDED;
  else
    state |= states::COLLAPSED;

  state |= states::HASPOPUP;
  return state;
}

void
HTMLComboboxAccessible::Description(nsString& aDescription)
{
  aDescription.Truncate();
  // First check to see if combo box itself has a description, perhaps through
  // tooltip (title attribute) or via aria-describedby
  Accessible::Description(aDescription);
  if (!aDescription.IsEmpty())
    return;

  // Otherwise use description of selected option.
  Accessible* option = SelectedOption();
  if (option)
    option->Description(aDescription);
}

void
HTMLComboboxAccessible::Value(nsString& aValue)
{
  // Use accessible name of selected option.
  Accessible* option = SelectedOption();
  if (option)
    option->Name(aValue);
}

uint8_t
HTMLComboboxAccessible::ActionCount()
{
  return 1;
}

bool
HTMLComboboxAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click)
    return false;

  DoCommand();
  return true;
}

void
HTMLComboboxAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != HTMLComboboxAccessible::eAction_Click)
    return;

  nsIComboboxControlFrame* comboFrame = do_QueryFrame(GetFrame());
  if (!comboFrame)
    return;

  if (comboFrame->IsDroppedDown())
    aName.AssignLiteral("close");
  else
    aName.AssignLiteral("open");
}

bool
HTMLComboboxAccessible::IsAcceptableChild(nsIContent* aEl) const
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: Widgets

bool
HTMLComboboxAccessible::IsWidget() const
{
  return true;
}

bool
HTMLComboboxAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
HTMLComboboxAccessible::AreItemsOperable() const
{
  nsIComboboxControlFrame* comboboxFrame = do_QueryFrame(GetFrame());
  return comboboxFrame && comboboxFrame->IsDroppedDown();
}

Accessible*
HTMLComboboxAccessible::CurrentItem()
{
  return AreItemsOperable() ? mListAccessible->CurrentItem() : nullptr;
}

void
HTMLComboboxAccessible::SetCurrentItem(Accessible* aItem)
{
  if (AreItemsOperable())
    mListAccessible->SetCurrentItem(aItem);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: protected

Accessible*
HTMLComboboxAccessible::SelectedOption() const
{
  HTMLSelectElement* select = HTMLSelectElement::FromNode(mContent);
  int32_t selectedIndex = select->SelectedIndex();

  if (selectedIndex >= 0) {
    HTMLOptionElement* option = select->Item(selectedIndex);
    if (option) {
      DocAccessible* document = Document();
      if (document)
        return document->GetAccessible(option);
    }
  }

  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxListAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLComboboxListAccessible::
  HTMLComboboxListAccessible(Accessible* aParent, nsIContent* aContent,
                             DocAccessible* aDoc) :
  HTMLSelectListAccessible(aContent, aDoc)
{
  mStateFlags |= eSharedNode;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: Accessible

nsIFrame*
HTMLComboboxListAccessible::GetFrame() const
{
  nsIFrame* frame = HTMLSelectListAccessible::GetFrame();
  nsIComboboxControlFrame* comboBox = do_QueryFrame(frame);
  if (comboBox) {
    return comboBox->GetDropDown();
  }

  return nullptr;
}

role
HTMLComboboxListAccessible::NativeRole() const
{
  return roles::COMBOBOX_LIST;
}

uint64_t
HTMLComboboxListAccessible::NativeState()
{
  // As a HTMLComboboxListAccessible we can have the following states:
  // FOCUSED, FOCUSABLE, FLOATING, INVISIBLE
  // Get focus status from base class
  uint64_t state = Accessible::NativeState();

  nsIComboboxControlFrame* comboFrame = do_QueryFrame(mParent->GetFrame());
  if (comboFrame && comboFrame->IsDroppedDown())
    state |= states::FLOATING;
  else
    state |= states::INVISIBLE;

  return state;
}

nsRect
HTMLComboboxListAccessible::RelativeBounds(nsIFrame** aBoundingFrame) const
{
  *aBoundingFrame = nullptr;

  Accessible* comboAcc = Parent();
  if (!comboAcc)
    return nsRect();

  if (0 == (comboAcc->State() & states::COLLAPSED)) {
    return HTMLSelectListAccessible::RelativeBounds(aBoundingFrame);
  }

  // Get the first option.
  nsIContent* content = mContent->GetFirstChild();
  if (!content)
    return nsRect();

  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    *aBoundingFrame = nullptr;
    return nsRect();
  }

  *aBoundingFrame = frame->GetParent();
  return (*aBoundingFrame)->GetRect();
}

bool
HTMLComboboxListAccessible::IsAcceptableChild(nsIContent* aEl) const
{
  return aEl->IsAnyOfHTMLElements(nsGkAtoms::option, nsGkAtoms::optgroup);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxListAccessible: Widgets

bool
HTMLComboboxListAccessible::IsActiveWidget() const
{
  return mParent && mParent->IsActiveWidget();
}

bool
HTMLComboboxListAccessible::AreItemsOperable() const
{
  return mParent && mParent->AreItemsOperable();
}
