/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLSelectAccessible.h"

#include "LocalAccessible-inl.h"
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
#include "nsComboboxControlFrame.h"
#include "nsContainerFrame.h"
#include "nsListControlFrame.h"

using namespace mozilla::a11y;
using namespace mozilla::dom;

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLSelectListAccessible::HTMLSelectListAccessible(nsIContent* aContent,
                                                   DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {
  mGenericTypes |= eListControl | eSelect;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible: LocalAccessible public

uint64_t HTMLSelectListAccessible::NativeState() const {
  uint64_t state = AccessibleWrap::NativeState();
  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
    state |= states::MULTISELECTABLE | states::EXTSELECTABLE;
  }

  return state;
}

role HTMLSelectListAccessible::NativeRole() const { return roles::LISTBOX; }

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible: SelectAccessible

bool HTMLSelectListAccessible::SelectAll() {
  return mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)
             ? AccessibleWrap::SelectAll()
             : false;
}

bool HTMLSelectListAccessible::UnselectAll() {
  return mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)
             ? AccessibleWrap::UnselectAll()
             : false;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectListAccessible: Widgets

bool HTMLSelectListAccessible::IsWidget() const { return true; }

bool HTMLSelectListAccessible::IsActiveWidget() const {
  return FocusMgr()->HasDOMFocus(mContent);
}

bool HTMLSelectListAccessible::AreItemsOperable() const { return true; }

LocalAccessible* HTMLSelectListAccessible::CurrentItem() const {
  nsListControlFrame* listControlFrame = do_QueryFrame(GetFrame());
  if (listControlFrame) {
    nsCOMPtr<nsIContent> activeOptionNode =
        listControlFrame->GetCurrentOption();
    if (activeOptionNode) {
      DocAccessible* document = Document();
      if (document) return document->GetAccessible(activeOptionNode);
    }
  }
  return nullptr;
}

void HTMLSelectListAccessible::SetCurrentItem(const LocalAccessible* aItem) {
  if (!aItem->GetContent()->IsElement()) return;

  aItem->GetContent()->AsElement()->SetAttr(
      kNameSpaceID_None, nsGkAtoms::selected, u"true"_ns, true);
}

bool HTMLSelectListAccessible::IsAcceptableChild(nsIContent* aEl) const {
  return aEl->IsAnyOfHTMLElements(nsGkAtoms::option, nsGkAtoms::optgroup);
}

bool HTMLSelectListAccessible::AttributeChangesState(nsAtom* aAttribute) {
  return aAttribute == nsGkAtoms::multiple ||
         LocalAccessible::AttributeChangesState(aAttribute);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectOptionAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLSelectOptionAccessible::HTMLSelectOptionAccessible(nsIContent* aContent,
                                                       DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectOptionAccessible: LocalAccessible public

role HTMLSelectOptionAccessible::NativeRole() const {
  if (GetCombobox()) return roles::COMBOBOX_OPTION;

  return roles::OPTION;
}

ENameValueFlag HTMLSelectOptionAccessible::NativeName(nsString& aName) const {
  // CASE #1 -- great majority of the cases
  // find the label attribute - this is what the W3C says we should use
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
  if (!aName.IsEmpty()) return eNameOK;

  // CASE #2 -- no label parameter, get the first child,
  // use it if it is a text node
  LocalAccessible* firstChild = LocalFirstChild();
  nsIContent* text = firstChild ? firstChild->GetContent() : nullptr;
  if (text && text->IsText()) {
    nsTextEquivUtils::AppendTextEquivFromTextContent(text, &aName);
    aName.CompressWhitespace();
    return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
  }

  return eNameOK;
}

void HTMLSelectOptionAccessible::DOMAttributeChanged(
    int32_t aNameSpaceID, nsAtom* aAttribute, int32_t aModType,
    const nsAttrValue* aOldValue, uint64_t aOldState) {
  HyperTextAccessibleWrap::DOMAttributeChanged(aNameSpaceID, aAttribute,
                                               aModType, aOldValue, aOldState);

  if (aAttribute == nsGkAtoms::label) {
    dom::Element* elm = Elm();
    if (!elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_labelledby) &&
        !elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_label)) {
      mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
    }
  }
}

uint64_t HTMLSelectOptionAccessible::NativeState() const {
  // As a HTMLSelectOptionAccessible we can have the following states:
  // SELECTABLE, SELECTED, FOCUSED, FOCUSABLE, OFFSCREEN
  // Upcall to LocalAccessible, but skip HyperTextAccessible impl
  // because we don't want EDITABLE or SELECTABLE_TEXT
  uint64_t state = LocalAccessible::NativeState();

  LocalAccessible* select = GetSelect();
  if (!select) return state;

  uint64_t selectState = select->State();
  if (selectState & states::INVISIBLE) return state;

  // Are we selected?
  HTMLOptionElement* option = HTMLOptionElement::FromNode(mContent);
  bool selected = option && option->Selected();
  if (selected) state |= states::SELECTED;

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
    // XXX list frames are weird, don't rely on LocalAccessible's general
    // visibility implementation unless they get reimplemented in layout
    state &= ~states::OFFSCREEN;
    // <select> is not collapsed: compare bounds to calculate OFFSCREEN
    LocalAccessible* listAcc = LocalParent();
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

uint64_t HTMLSelectOptionAccessible::NativeInteractiveState() const {
  return NativelyUnavailable() ? states::UNAVAILABLE
                               : states::FOCUSABLE | states::SELECTABLE;
}

int32_t HTMLSelectOptionAccessible::GetLevelInternal() {
  nsIContent* parentContent = mContent->GetParent();

  int32_t level =
      parentContent->NodeInfo()->Equals(nsGkAtoms::optgroup) ? 2 : 1;

  if (level == 1 && Role() != roles::HEADING) {
    level = 0;  // In a single level list, the level is irrelevant
  }

  return level;
}

nsRect HTMLSelectOptionAccessible::RelativeBounds(
    nsIFrame** aBoundingFrame) const {
  LocalAccessible* combobox = GetCombobox();
  if (combobox && (combobox->State() & states::COLLAPSED)) {
    return combobox->RelativeBounds(aBoundingFrame);
  }

  return HyperTextAccessibleWrap::RelativeBounds(aBoundingFrame);
}

nsresult HTMLSelectOptionAccessible::HandleAccEvent(AccEvent* aEvent) {
  nsresult rv = HyperTextAccessibleWrap::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  AccStateChangeEvent* event = downcast_accEvent(aEvent);
  if (event && (event->GetState() == states::SELECTED)) {
    LocalAccessible* widget = ContainerWidget();
    if (widget && !widget->AreItemsOperable()) {
      // Collapsed options' ACTIVE state reflects their SELECT state.
      nsEventShell::FireEvent(this, states::ACTIVE, event->IsStateEnabled(),
                              true);
    }
  }

  return NS_OK;
}

void HTMLSelectOptionAccessible::ActionNameAt(uint8_t aIndex,
                                              nsAString& aName) {
  if (aIndex == eAction_Select) aName.AssignLiteral("select");
}

uint8_t HTMLSelectOptionAccessible::ActionCount() const { return 1; }

bool HTMLSelectOptionAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != eAction_Select) return false;

  DoCommand();
  return true;
}

void HTMLSelectOptionAccessible::SetSelected(bool aSelect) {
  HTMLOptionElement* option = HTMLOptionElement::FromNode(mContent);
  if (option) option->SetSelected(aSelect);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectOptionAccessible: Widgets

LocalAccessible* HTMLSelectOptionAccessible::ContainerWidget() const {
  LocalAccessible* parent = LocalParent();
  if (parent && parent->IsHTMLOptGroup()) {
    parent = parent->LocalParent();
  }

  return parent && parent->IsListControl() ? parent : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSelectOptGroupAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLSelectOptGroupAccessible::NativeRole() const {
  return roles::GROUPING;
}

uint64_t HTMLSelectOptGroupAccessible::NativeInteractiveState() const {
  return NativelyUnavailable() ? states::UNAVAILABLE : 0;
}

bool HTMLSelectOptGroupAccessible::IsAcceptableChild(nsIContent* aEl) const {
  return aEl->IsCharacterData() || aEl->IsHTMLElement(nsGkAtoms::option);
}

uint8_t HTMLSelectOptGroupAccessible::ActionCount() const { return 0; }

void HTMLSelectOptGroupAccessible::ActionNameAt(uint8_t aIndex,
                                                nsAString& aName) {
  aName.Truncate();
}

bool HTMLSelectOptGroupAccessible::DoAction(uint8_t aIndex) const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLComboboxAccessible::HTMLComboboxAccessible(nsIContent* aContent,
                                               DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {
  mType = eHTMLComboboxType;
  mGenericTypes |= eCombobox;
  mStateFlags |= eNoKidsFromDOM;

  nsComboboxControlFrame* comboFrame = do_QueryFrame(GetFrame());
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
// HTMLComboboxAccessible: LocalAccessible

role HTMLComboboxAccessible::NativeRole() const { return roles::COMBOBOX; }

bool HTMLComboboxAccessible::RemoveChild(LocalAccessible* aChild) {
  MOZ_ASSERT(aChild == mListAccessible);
  if (AccessibleWrap::RemoveChild(aChild)) {
    mListAccessible = nullptr;
    return true;
  }
  return false;
}

void HTMLComboboxAccessible::Shutdown() {
  MOZ_ASSERT(!mDoc || mDoc->IsDefunct() || !mListAccessible);
  if (mListAccessible) {
    mListAccessible->Shutdown();
    mListAccessible = nullptr;
  }

  AccessibleWrap::Shutdown();
}

uint64_t HTMLComboboxAccessible::NativeState() const {
  // As a HTMLComboboxAccessible we can have the following states:
  // FOCUSED, FOCUSABLE, HASPOPUP, EXPANDED, COLLAPSED
  // Get focus status from base class
  uint64_t state = LocalAccessible::NativeState();

  nsComboboxControlFrame* comboFrame = do_QueryFrame(GetFrame());
  if (comboFrame && comboFrame->IsDroppedDown()) {
    state |= states::EXPANDED;
  } else {
    state |= states::COLLAPSED;
  }

  state |= states::HASPOPUP;
  return state;
}

void HTMLComboboxAccessible::Description(nsString& aDescription) const {
  aDescription.Truncate();
  // First check to see if combo box itself has a description, perhaps through
  // tooltip (title attribute) or via aria-describedby
  LocalAccessible::Description(aDescription);
  if (!aDescription.IsEmpty()) return;

  // Otherwise use description of selected option.
  LocalAccessible* option = SelectedOption();
  if (option) option->Description(aDescription);
}

void HTMLComboboxAccessible::Value(nsString& aValue) const {
  // Use accessible name of selected option.
  LocalAccessible* option = SelectedOption();
  if (option) option->Name(aValue);
}

uint8_t HTMLComboboxAccessible::ActionCount() const { return 1; }

bool HTMLComboboxAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != eAction_Click) return false;

  DoCommand();
  return true;
}

void HTMLComboboxAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex != HTMLComboboxAccessible::eAction_Click) return;

  nsComboboxControlFrame* comboFrame = do_QueryFrame(GetFrame());
  if (!comboFrame) return;

  if (comboFrame->IsDroppedDown()) {
    aName.AssignLiteral("close");
  } else {
    aName.AssignLiteral("open");
  }
}

bool HTMLComboboxAccessible::IsAcceptableChild(nsIContent* aEl) const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: Widgets

bool HTMLComboboxAccessible::IsWidget() const { return true; }

bool HTMLComboboxAccessible::IsActiveWidget() const {
  return FocusMgr()->HasDOMFocus(mContent);
}

bool HTMLComboboxAccessible::AreItemsOperable() const {
  nsComboboxControlFrame* comboboxFrame = do_QueryFrame(GetFrame());
  return comboboxFrame && comboboxFrame->IsDroppedDown();
}

LocalAccessible* HTMLComboboxAccessible::CurrentItem() const {
  return AreItemsOperable() ? mListAccessible->CurrentItem() : nullptr;
}

void HTMLComboboxAccessible::SetCurrentItem(const LocalAccessible* aItem) {
  if (AreItemsOperable()) mListAccessible->SetCurrentItem(aItem);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: protected

LocalAccessible* HTMLComboboxAccessible::SelectedOption() const {
  HTMLSelectElement* select = HTMLSelectElement::FromNode(mContent);
  int32_t selectedIndex = select->SelectedIndex();

  if (selectedIndex >= 0) {
    HTMLOptionElement* option = select->Item(selectedIndex);
    if (option) {
      DocAccessible* document = Document();
      if (document) return document->GetAccessible(option);
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxListAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLComboboxListAccessible::HTMLComboboxListAccessible(LocalAccessible* aParent,
                                                       nsIContent* aContent,
                                                       DocAccessible* aDoc)
    : HTMLSelectListAccessible(aContent, aDoc) {
  mStateFlags |= eSharedNode;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxAccessible: LocalAccessible

nsIFrame* HTMLComboboxListAccessible::GetFrame() const {
  nsIFrame* frame = HTMLSelectListAccessible::GetFrame();
  nsComboboxControlFrame* comboBox = do_QueryFrame(frame);
  if (comboBox) {
    return comboBox->GetDropDown();
  }

  return nullptr;
}

role HTMLComboboxListAccessible::NativeRole() const {
  return roles::COMBOBOX_LIST;
}

uint64_t HTMLComboboxListAccessible::NativeState() const {
  // As a HTMLComboboxListAccessible we can have the following states:
  // FOCUSED, FOCUSABLE, FLOATING, INVISIBLE
  // Get focus status from base class
  uint64_t state = LocalAccessible::NativeState();

  nsComboboxControlFrame* comboFrame = do_QueryFrame(mParent->GetFrame());
  if (comboFrame && comboFrame->IsDroppedDown()) {
    state |= states::FLOATING;
  } else {
    state |= states::INVISIBLE;
  }

  return state;
}

nsRect HTMLComboboxListAccessible::RelativeBounds(
    nsIFrame** aBoundingFrame) const {
  *aBoundingFrame = nullptr;

  LocalAccessible* comboAcc = LocalParent();
  if (!comboAcc) return nsRect();

  if (0 == (comboAcc->State() & states::COLLAPSED)) {
    return HTMLSelectListAccessible::RelativeBounds(aBoundingFrame);
  }

  // Get the first option.
  nsIContent* content = mContent->GetFirstChild();
  if (!content) return nsRect();

  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    *aBoundingFrame = nullptr;
    return nsRect();
  }

  *aBoundingFrame = frame->GetParent();
  return (*aBoundingFrame)->GetRect();
}

bool HTMLComboboxListAccessible::IsAcceptableChild(nsIContent* aEl) const {
  return aEl->IsAnyOfHTMLElements(nsGkAtoms::option, nsGkAtoms::optgroup);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLComboboxListAccessible: Widgets

bool HTMLComboboxListAccessible::IsActiveWidget() const {
  return mParent && mParent->IsActiveWidget();
}

bool HTMLComboboxListAccessible::AreItemsOperable() const {
  return mParent && mParent->AreItemsOperable();
}
