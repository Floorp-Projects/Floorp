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
#include "nsIAccessibleEvent.h"
#include "nsTextEquivUtils.h"
#include "Role.h"
#include "States.h"

#include "nsCOMPtr.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "nsIComboboxControlFrame.h"
#include "nsIFrame.h"
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
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple))
    state |= states::MULTISELECTABLE | states::EXTSELECTABLE;

  return state;
}

role
HTMLSelectListAccessible::NativeRole()
{
  return roles::LISTBOX;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible: SelectAccessible

bool
HTMLSelectListAccessible::SelectAll()
{
  return mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple) ?
    AccessibleWrap::SelectAll() : false;
}

bool
HTMLSelectListAccessible::UnselectAll()
{
  return mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple) ?
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
  aItem->GetContent()->SetAttr(kNameSpaceID_None,
                               nsGkAtoms::selected, NS_LITERAL_STRING("true"),
                               true);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible: Accessible protected

void
HTMLSelectListAccessible::CacheChildren()
{
  // Cache accessibles for <optgroup> and <option> DOM decendents as children,
  // as well as the accessibles for them. Avoid whitespace text nodes. We want
  // to count all the <optgroup>s and <option>s as children because we want
  // a flat tree under the Select List.
  for (nsIContent* childContent = mContent->GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    if (!childContent->IsHTML()) {
      continue;
    }

    nsIAtom* tag = childContent->Tag();
    if (tag == nsGkAtoms::option ||
        tag == nsGkAtoms::optgroup) {

      // Get an accessible for option or optgroup and cache it.
      nsRefPtr<Accessible> accessible =
        GetAccService()->GetOrCreateAccessible(childContent, this);
      if (accessible)
        AppendChild(accessible);
    }
  }
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
HTMLSelectOptionAccessible::NativeRole()
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
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
  if (!aName.IsEmpty())
    return eNameOK;

  // CASE #2 -- no label parameter, get the first child, 
  // use it if it is a text node
  nsIContent* text = mContent->GetFirstChild();
  if (text && text->IsNodeOfType(nsINode::eTEXT)) {
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
  HTMLOptionElement* option = HTMLOptionElement::FromContent(mContent);
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
      int32_t optionX, optionY, optionWidth, optionHeight;
      int32_t listX, listY, listWidth, listHeight;
      GetBounds(&optionX, &optionY, &optionWidth, &optionHeight);
      listAcc->GetBounds(&listX, &listY, &listWidth, &listHeight);
      if (optionY < listY || optionY + optionHeight > listY + listHeight) {
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

void
HTMLSelectOptionAccessible::GetBoundsRect(nsRect& aTotalBounds,
                                          nsIFrame** aBoundingFrame)
{
  Accessible* combobox = GetCombobox();
  if (combobox && (combobox->State() & states::COLLAPSED))
    combobox->GetBoundsRect(aTotalBounds, aBoundingFrame);
  else
    HyperTextAccessibleWrap::GetBoundsRect(aTotalBounds, aBoundingFrame);
}

NS_IMETHODIMP
HTMLSelectOptionAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Select) {
    aName.AssignLiteral("select"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

uint8_t
HTMLSelectOptionAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
HTMLSelectOptionAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Select)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  DoCommand();
  return NS_OK;
}

NS_IMETHODIMP
HTMLSelectOptionAccessible::SetSelected(bool aSelect)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  HTMLOptionElement* option = HTMLOptionElement::FromContent(mContent);
  return option ? option->SetSelected(aSelect) : NS_ERROR_FAILURE;
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
HTMLSelectOptGroupAccessible::NativeRole()
{
  return roles::GROUPING;
}

uint64_t
HTMLSelectOptGroupAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ? states::UNAVAILABLE : 0;
}

NS_IMETHODIMP
HTMLSelectOptGroupAccessible::DoAction(uint8_t index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HTMLSelectOptGroupAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

uint8_t
HTMLSelectOptGroupAccessible::ActionCount()
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLComboboxAccessible::
  HTMLComboboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  mGenericTypes |= eCombobox;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: Accessible

role
HTMLComboboxAccessible::NativeRole()
{
  return roles::COMBOBOX;
}

void
HTMLComboboxAccessible::InvalidateChildren()
{
  AccessibleWrap::InvalidateChildren();

  if (mListAccessible)
    mListAccessible->InvalidateChildren();
}

void
HTMLComboboxAccessible::CacheChildren()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return;

  nsIComboboxControlFrame* comboFrame = do_QueryFrame(frame);
  if (!comboFrame)
    return;

  nsIFrame* listFrame = comboFrame->GetDropDown();
  if (!listFrame)
    return;

  if (!mListAccessible) {
    mListAccessible = 
      new HTMLComboboxListAccessible(mParent, mContent, mDoc);

    // Initialize and put into cache.
    if (!Document()->BindToDocument(mListAccessible, nullptr))
      return;
  }

  if (AppendChild(mListAccessible)) {
    // Cache combobox option accessibles so that we build complete accessible
    // tree for combobox.
    mListAccessible->EnsureChildren();
  }
}

void
HTMLComboboxAccessible::Shutdown()
{
  AccessibleWrap::Shutdown();

  if (mListAccessible) {
    mListAccessible->Shutdown();
    mListAccessible = nullptr;
  }
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

NS_IMETHODIMP
HTMLComboboxAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  DoCommand();
  return NS_OK;
}

/**
  * Our action name is the reverse of our state: 
  *     if we are closed -> open is our name.
  *     if we are open -> closed is our name.
  * Uses the frame to get the state, updated on every click
  */
NS_IMETHODIMP
HTMLComboboxAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != HTMLComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }
  nsIFrame* frame = GetFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }
  nsIComboboxControlFrame* comboFrame = do_QueryFrame(frame);
  if (!comboFrame) {
    return NS_ERROR_FAILURE;
  }
  if (comboFrame->IsDroppedDown())
    aName.AssignLiteral("close"); 
  else
    aName.AssignLiteral("open"); 

  return NS_OK;
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
  nsIFrame* frame = GetFrame();
  nsIComboboxControlFrame* comboboxFrame = do_QueryFrame(frame);
  if (!comboboxFrame)
    return nullptr;

  nsIListControlFrame* listControlFrame =
    do_QueryFrame(comboboxFrame->GetDropDown());
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


////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxListAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLComboboxListAccessible::
  HTMLComboboxListAccessible(nsIAccessible* aParent, nsIContent* aContent,
                             DocAccessible* aDoc) :
  HTMLSelectListAccessible(aContent, aDoc)
{
  mStateFlags |= eSharedNode;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: nsAccessNode

nsIFrame*
HTMLComboboxListAccessible::GetFrame() const
{
  nsIFrame* frame = HTMLSelectListAccessible::GetFrame();

  if (frame) {
    nsIComboboxControlFrame* comboBox = do_QueryFrame(frame);
    if (comboBox) {
      return comboBox->GetDropDown();
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: Accessible

role
HTMLComboboxListAccessible::NativeRole()
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

/**
  * Gets the bounds for the areaFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void
HTMLComboboxListAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  *aBoundingFrame = nullptr;

  Accessible* comboAcc = Parent();
  if (!comboAcc)
    return;

  if (0 == (comboAcc->State() & states::COLLAPSED)) {
    HTMLSelectListAccessible::GetBoundsRect(aBounds, aBoundingFrame);
    return;
  }

  // Get the first option.
  nsIContent* content = mContent->GetFirstChild();
  if (!content) {
    return;
  }
  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    *aBoundingFrame = nullptr;
    return;
  }

  *aBoundingFrame = frame->GetParent();
  aBounds = (*aBoundingFrame)->GetRect();
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

