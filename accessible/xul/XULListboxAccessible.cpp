/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULListboxAccessible.h"

#include "LocalAccessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "mozilla/a11y/Role.h"
#include "States.h"

#include "nsComponentManagerUtils.h"
#include "nsIAutoCompletePopup.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsINodeList.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULColumAccessible
////////////////////////////////////////////////////////////////////////////////

XULColumAccessible::XULColumAccessible(nsIContent* aContent,
                                       DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {}

role XULColumAccessible::NativeRole() const { return roles::LIST; }

uint64_t XULColumAccessible::NativeState() const { return states::READONLY; }

////////////////////////////////////////////////////////////////////////////////
// XULColumnItemAccessible
////////////////////////////////////////////////////////////////////////////////

XULColumnItemAccessible::XULColumnItemAccessible(nsIContent* aContent,
                                                 DocAccessible* aDoc)
    : LeafAccessible(aContent, aDoc) {}

role XULColumnItemAccessible::NativeRole() const { return roles::COLUMNHEADER; }

uint64_t XULColumnItemAccessible::NativeState() const {
  return states::READONLY;
}

bool XULColumnItemAccessible::HasPrimaryAction() const { return true; }

void XULColumnItemAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == eAction_Click) aName.AssignLiteral("click");
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULListboxAccessible::XULListboxAccessible(nsIContent* aContent,
                                           DocAccessible* aDoc)
    : XULSelectControlAccessible(aContent, aDoc) {
  dom::Element* parentEl = mContent->GetParentElement();
  if (parentEl) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
        parentEl->AsAutoCompletePopup();
    if (autoCompletePopupElm) mGenericTypes |= eAutoCompletePopup;
  }

  if (IsMulticolumn()) mGenericTypes |= eTable;
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible: LocalAccessible

uint64_t XULListboxAccessible::NativeState() const {
  // As a XULListboxAccessible we can have the following states:
  //   FOCUSED, READONLY, FOCUSABLE

  // Get focus status from base class
  uint64_t states = LocalAccessible::NativeState();

  // see if we are multiple select if so set ourselves as such

  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::seltype,
                                         nsGkAtoms::multiple, eCaseMatters)) {
    states |= states::MULTISELECTABLE | states::EXTSELECTABLE;
  }

  return states;
}

role XULListboxAccessible::NativeRole() const {
  // A richlistbox is used with the new autocomplete URL bar, and has a parent
  // popup <panel>.
  if (mContent->GetParent() &&
      mContent->GetParent()->IsXULElement(nsGkAtoms::panel)) {
    return roles::COMBOBOX_LIST;
  }

  return IsMulticolumn() ? roles::TABLE : roles::LISTBOX;
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible: Table

uint32_t XULListboxAccessible::ColCount() const { return 0; }

uint32_t XULListboxAccessible::RowCount() {
  nsCOMPtr<nsIDOMXULSelectControlElement> element = Elm()->AsXULSelectControl();

  uint32_t itemCount = 0;
  if (element) element->GetItemCount(&itemCount);

  return itemCount;
}

LocalAccessible* XULListboxAccessible::CellAt(uint32_t aRowIndex,
                                              uint32_t aColumnIndex) {
  nsCOMPtr<nsIDOMXULSelectControlElement> control = Elm()->AsXULSelectControl();
  NS_ENSURE_TRUE(control, nullptr);

  RefPtr<dom::Element> element;
  control->GetItemAtIndex(aRowIndex, getter_AddRefs(element));
  if (!element) return nullptr;

  LocalAccessible* row = mDoc->GetAccessible(element);
  NS_ENSURE_TRUE(row, nullptr);

  return row->LocalChildAt(aColumnIndex);
}

bool XULListboxAccessible::IsColSelected(uint32_t aColIdx) {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
      Elm()->AsXULMultiSelectControl();
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  int32_t selectedrowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, false);

  return selectedrowCount == static_cast<int32_t>(RowCount());
}

bool XULListboxAccessible::IsRowSelected(uint32_t aRowIdx) {
  nsCOMPtr<nsIDOMXULSelectControlElement> control = Elm()->AsXULSelectControl();
  NS_ASSERTION(control, "Doesn't implement nsIDOMXULSelectControlElement.");

  RefPtr<dom::Element> element;
  nsresult rv = control->GetItemAtIndex(aRowIdx, getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, false);
  if (!element) {
    return false;
  }

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item =
      element->AsXULSelectControlItem();

  bool isSelected = false;
  item->GetSelected(&isSelected);
  return isSelected;
}

bool XULListboxAccessible::IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx) {
  return IsRowSelected(aRowIdx);
}

uint32_t XULListboxAccessible::SelectedCellCount() {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
      Elm()->AsXULMultiSelectControl();
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsINodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems) return 0;

  uint32_t selectedItemsCount = selectedItems->Length();

  return selectedItemsCount * ColCount();
}

uint32_t XULListboxAccessible::SelectedColCount() {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
      Elm()->AsXULMultiSelectControl();
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  int32_t selectedRowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedRowCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedRowCount > 0 &&
                 selectedRowCount == static_cast<int32_t>(RowCount())
             ? ColCount()
             : 0;
}

uint32_t XULListboxAccessible::SelectedRowCount() {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
      Elm()->AsXULMultiSelectControl();
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  int32_t selectedRowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedRowCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedRowCount >= 0 ? selectedRowCount : 0;
}

void XULListboxAccessible::SelectedCells(nsTArray<Accessible*>* aCells) {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
      Elm()->AsXULMultiSelectControl();
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsINodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems) return;

  uint32_t selectedItemsCount = selectedItems->Length();

  for (uint32_t index = 0; index < selectedItemsCount; index++) {
    nsIContent* itemContent = selectedItems->Item(index);
    LocalAccessible* item = mDoc->GetAccessible(itemContent);

    if (item) {
      uint32_t cellCount = item->ChildCount();
      for (uint32_t cellIdx = 0; cellIdx < cellCount; cellIdx++) {
        LocalAccessible* cell = mChildren[cellIdx];
        if (cell->Role() == roles::CELL) aCells->AppendElement(cell);
      }
    }
  }
}

void XULListboxAccessible::SelectedCellIndices(nsTArray<uint32_t>* aCells) {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
      Elm()->AsXULMultiSelectControl();
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsINodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems) return;

  uint32_t selectedItemsCount = selectedItems->Length();

  uint32_t colCount = ColCount();
  aCells->SetCapacity(selectedItemsCount * colCount);
  aCells->AppendElements(selectedItemsCount * colCount);

  for (uint32_t selItemsIdx = 0, cellsIdx = 0; selItemsIdx < selectedItemsCount;
       selItemsIdx++) {
    nsIContent* itemContent = selectedItems->Item(selItemsIdx);

    nsCOMPtr<nsIDOMXULSelectControlItemElement> item =
        itemContent->AsElement()->AsXULSelectControlItem();
    if (item) {
      int32_t itemIdx = -1;
      control->GetIndexOfItem(item, &itemIdx);
      if (itemIdx >= 0) {
        for (uint32_t colIdx = 0; colIdx < colCount; colIdx++, cellsIdx++) {
          aCells->ElementAt(cellsIdx) = itemIdx * colCount + colIdx;
        }
      }
    }
  }
}

void XULListboxAccessible::SelectedColIndices(nsTArray<uint32_t>* aCols) {
  uint32_t selColCount = SelectedColCount();
  aCols->SetCapacity(selColCount);

  for (uint32_t colIdx = 0; colIdx < selColCount; colIdx++) {
    aCols->AppendElement(colIdx);
  }
}

void XULListboxAccessible::SelectedRowIndices(nsTArray<uint32_t>* aRows) {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
      Elm()->AsXULMultiSelectControl();
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsINodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems) return;

  uint32_t rowCount = selectedItems->Length();

  if (!rowCount) return;

  aRows->SetCapacity(rowCount);
  aRows->AppendElements(rowCount);

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    nsIContent* itemContent = selectedItems->Item(rowIdx);
    nsCOMPtr<nsIDOMXULSelectControlItemElement> item =
        itemContent->AsElement()->AsXULSelectControlItem();

    if (item) {
      int32_t itemIdx = -1;
      control->GetIndexOfItem(item, &itemIdx);
      if (itemIdx >= 0) aRows->ElementAt(rowIdx) = itemIdx;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible: Widgets

bool XULListboxAccessible::IsWidget() const { return true; }

bool XULListboxAccessible::IsActiveWidget() const {
  if (IsAutoCompletePopup()) {
    nsIContent* parentContent = mContent->GetParent();
    if (parentContent) {
      nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
          parentContent->AsElement()->AsAutoCompletePopup();
      if (autoCompletePopupElm) {
        bool isOpen = false;
        autoCompletePopupElm->GetPopupOpen(&isOpen);
        return isOpen;
      }
    }
  }
  return FocusMgr()->HasDOMFocus(mContent);
}

bool XULListboxAccessible::AreItemsOperable() const {
  if (IsAutoCompletePopup()) {
    nsIContent* parentContent = mContent->GetParent();
    if (parentContent) {
      nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
          parentContent->AsElement()->AsAutoCompletePopup();
      if (autoCompletePopupElm) {
        bool isOpen = false;
        autoCompletePopupElm->GetPopupOpen(&isOpen);
        return isOpen;
      }
    }
  }
  return true;
}

LocalAccessible* XULListboxAccessible::ContainerWidget() const {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible
////////////////////////////////////////////////////////////////////////////////

XULListitemAccessible::XULListitemAccessible(nsIContent* aContent,
                                             DocAccessible* aDoc)
    : XULMenuitemAccessible(aContent, aDoc) {
  mIsCheckbox = mContent->AsElement()->AttrValueIs(
      kNameSpaceID_None, nsGkAtoms::type, nsGkAtoms::checkbox, eCaseMatters);
  mType = eXULListItemType;
}

XULListitemAccessible::~XULListitemAccessible() {}

LocalAccessible* XULListitemAccessible::GetListAccessible() const {
  if (IsDefunct()) return nullptr;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem =
      Elm()->AsXULSelectControlItem();
  if (!listItem) return nullptr;

  RefPtr<dom::Element> listElement;
  listItem->GetControl(getter_AddRefs(listElement));
  if (!listElement) return nullptr;

  return mDoc->GetAccessible(listElement);
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible LocalAccessible

void XULListitemAccessible::Description(nsString& aDesc) const {
  AccessibleWrap::Description(aDesc);
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible: LocalAccessible

/**
 * Get the name from GetXULName.
 */
ENameValueFlag XULListitemAccessible::NativeName(nsString& aName) const {
  return LocalAccessible::NativeName(aName);
}

role XULListitemAccessible::NativeRole() const {
  LocalAccessible* list = GetListAccessible();
  if (!list) {
    NS_ERROR("No list accessible for listitem accessible!");
    return roles::NOTHING;
  }

  if (list->Role() == roles::TABLE) return roles::ROW;

  if (mIsCheckbox) return roles::CHECK_RICH_OPTION;

  if (mParent && mParent->Role() == roles::COMBOBOX_LIST) {
    return roles::COMBOBOX_OPTION;
  }

  return roles::RICH_OPTION;
}

uint64_t XULListitemAccessible::NativeState() const {
  if (mIsCheckbox) return XULMenuitemAccessible::NativeState();

  uint64_t states = NativeInteractiveState();

  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem =
      Elm()->AsXULSelectControlItem();
  if (listItem) {
    bool isSelected;
    listItem->GetSelected(&isSelected);
    if (isSelected) states |= states::SELECTED;

    if (FocusMgr()->IsFocused(this)) states |= states::FOCUSED;
  }

  return states;
}

uint64_t XULListitemAccessible::NativeInteractiveState() const {
  return NativelyUnavailable() || (mParent && mParent->NativelyUnavailable())
             ? states::UNAVAILABLE
             : states::FOCUSABLE | states::SELECTABLE;
}

void XULListitemAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == eAction_Click && mIsCheckbox) {
    uint64_t states = NativeState();
    if (states & states::CHECKED) {
      aName.AssignLiteral("uncheck");
    } else {
      aName.AssignLiteral("check");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible: Widgets

LocalAccessible* XULListitemAccessible::ContainerWidget() const {
  return LocalParent();
}
