/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULListboxAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"

#include "nsComponentManagerUtils.h"
#include "nsIAutoCompleteInput.h"
#include "nsIAutoCompletePopup.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULColumAccessible
////////////////////////////////////////////////////////////////////////////////

XULColumAccessible::
  XULColumAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULColumAccessible::NativeRole()
{
  return roles::LIST;
}

PRUint64
XULColumAccessible::NativeState()
{
  return states::READONLY;
}


////////////////////////////////////////////////////////////////////////////////
// XULColumnItemAccessible
////////////////////////////////////////////////////////////////////////////////

XULColumnItemAccessible::
  XULColumnItemAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

role
XULColumnItemAccessible::NativeRole()
{
  return roles::COLUMNHEADER;
}

PRUint64
XULColumnItemAccessible::NativeState()
{
  return states::READONLY;
}

PRUint8
XULColumnItemAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
XULColumnItemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("click");
  return NS_OK;
}

NS_IMETHODIMP
XULColumnItemAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULListboxAccessible::
  XULListboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULSelectControlAccessible(aContent, aDoc), xpcAccessibleTable(this)
{
  nsIContent* parentContent = mContent->GetParent();
  if (parentContent) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(parentContent);
    if (autoCompletePopupElm)
      mFlags |= eAutoCompletePopupAccessible;
  }
}

NS_IMPL_ADDREF_INHERITED(XULListboxAccessible, XULSelectControlAccessible)
NS_IMPL_RELEASE_INHERITED(XULListboxAccessible, XULSelectControlAccessible)

nsresult
XULListboxAccessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = XULSelectControlAccessible::QueryInterface(aIID, aInstancePtr);
  if (*aInstancePtr)
    return rv;

  if (aIID.Equals(NS_GET_IID(nsIAccessibleTable)) && IsMulticolumn()) {
    *aInstancePtr = static_cast<nsIAccessibleTable*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
//nsAccessNode

void
XULListboxAccessible::Shutdown()
{
  mTable = nsnull;
  XULSelectControlAccessible::Shutdown();
}

bool
XULListboxAccessible::IsMulticolumn()
{
  PRInt32 numColumns = 0;
  nsresult rv = GetColumnCount(&numColumns);
  if (NS_FAILED(rv))
    return false;

  return numColumns > 1;
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible. nsIAccessible

PRUint64
XULListboxAccessible::NativeState()
{
  // As a XULListboxAccessible we can have the following states:
  //   FOCUSED, READONLY, FOCUSABLE

  // Get focus status from base class
  PRUint64 states = Accessible::NativeState();

  // see if we are multiple select if so set ourselves as such

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::seltype,
                            nsGkAtoms::multiple, eCaseMatters)) {
      states |= states::MULTISELECTABLE | states::EXTSELECTABLE;
  }

  return states;
}

/**
  * Our value is the label of our ( first ) selected child.
  */
void
XULListboxAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  nsCOMPtr<nsIDOMXULSelectControlElement> select(do_QueryInterface(mContent));
  if (select) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
    select->GetSelectedItem(getter_AddRefs(selectedItem));
    if (selectedItem)
      selectedItem->GetLabel(aValue);
  }
}

role
XULListboxAccessible::NativeRole()
{
  // A richlistbox is used with the new autocomplete URL bar, and has a parent
  // popup <panel>.
  nsCOMPtr<nsIDOMXULPopupElement> xulPopup =
    do_QueryInterface(mContent->GetParent());
  if (xulPopup)
    return roles::COMBOBOX_LIST;

  return IsMulticolumn() ? roles::TABLE : roles::LISTBOX;
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible. nsIAccessibleTable

PRUint32
XULListboxAccessible::ColCount()
{
  nsIContent* headContent = nsnull;
  for (nsIContent* childContent = mContent->GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    if (childContent->NodeInfo()->Equals(nsGkAtoms::listcols,
                                         kNameSpaceID_XUL)) {
      headContent = childContent;
    }
  }
  if (!headContent)
    return 0;

  PRUint32 columnCount = 0;
  for (nsIContent* childContent = headContent->GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    if (childContent->NodeInfo()->Equals(nsGkAtoms::listcol,
                                         kNameSpaceID_XUL)) {
      columnCount++;
    }
  }

  return columnCount;
}

PRUint32
XULListboxAccessible::RowCount()
{
  nsCOMPtr<nsIDOMXULSelectControlElement> element(do_QueryInterface(mContent));

  PRUint32 itemCount = 0;
  if(element)
    element->GetItemCount(&itemCount);

  return itemCount;
}

Accessible*
XULListboxAccessible::CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex)
{ 
  nsCOMPtr<nsIDOMXULSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ENSURE_TRUE(control, nsnull);

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRowIndex, getter_AddRefs(item));
  if (!item)
    return nsnull;

  nsCOMPtr<nsIContent> itemContent(do_QueryInterface(item));
  if (!itemContent)
    return nsnull;

  Accessible* row = mDoc->GetAccessible(itemContent);
  NS_ENSURE_TRUE(row, nsnull);

  return row->GetChildAt(aColumnIndex);
}

NS_IMETHODIMP
XULListboxAccessible::GetColumnIndexAt(PRInt32 aIndex, PRInt32* aColumn)
{
  NS_ENSURE_ARG_POINTER(aColumn);
  *aColumn = -1;

  PRInt32 columnCount = 0;
  nsresult rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aColumn = aIndex % columnCount;
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetRowIndexAt(PRInt32 aIndex, PRInt32* aRow)
{
  NS_ENSURE_ARG_POINTER(aRow);
  *aRow = -1;

  PRInt32 columnCount = 0;
  nsresult rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aRow = aIndex / columnCount;
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetRowAndColumnIndicesAt(PRInt32 aCellIndex,
                                               PRInt32* aRowIndex,
                                               PRInt32* aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 columnCount = 0;
  nsresult rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aColumnIndex = aCellIndex % columnCount;
  *aRowIndex = aCellIndex / columnCount;
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetColumnDescription(PRInt32 aColumn,
                                           nsAString& aDescription)
{
  aDescription.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetRowDescription(PRInt32 aRow, nsAString& aDescription)
{
  aDescription.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::IsColumnSelected(PRInt32 aColumn, bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  PRInt32 selectedrowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount = 0;
  rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aIsSelected = (selectedrowCount == rowCount);
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::IsRowSelected(PRInt32 aRow, bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULSelectControlElement.");

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRow, getter_AddRefs(item));
  NS_ENSURE_TRUE(item, NS_ERROR_INVALID_ARG);

  return item->GetSelected(aIsSelected);
}

NS_IMETHODIMP
XULListboxAccessible::IsCellSelected(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                     bool* aIsSelected)
{
  return IsRowSelected(aRowIndex, aIsSelected);
}

NS_IMETHODIMP
XULListboxAccessible::GetSelectedCellCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return NS_OK;

  PRUint32 selectedItemsCount = 0;
  nsresult rv = selectedItems->GetLength(&selectedItemsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!selectedItemsCount)
    return NS_OK;

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedItemsCount * columnCount;
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetSelectedColumnCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  PRInt32 selectedrowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount = 0;
  rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (selectedrowCount != rowCount)
    return NS_OK;

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = columnCount;
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetSelectedRowCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  PRInt32 selectedrowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedrowCount;
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetSelectedCells(nsIArray** aCells)
{
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> selCells =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return NS_OK;

  PRUint32 selectedItemsCount = 0;
  rv = selectedItems->GetLength(&selectedItemsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(mDoc, NS_ERROR_FAILURE);
  PRUint32 index = 0;
  for (; index < selectedItemsCount; index++) {
    nsCOMPtr<nsIDOMNode> itemNode;
    selectedItems->Item(index, getter_AddRefs(itemNode));
    nsCOMPtr<nsIContent> itemContent(do_QueryInterface(itemNode));
    Accessible* item = mDoc->GetAccessible(itemContent);

    if (item) {
      PRUint32 cellCount = item->ChildCount();
      for (PRUint32 cellIdx = 0; cellIdx < cellCount; cellIdx++) {
        Accessible* cell = mChildren[cellIdx];
        if (cell->Role() == roles::CELL)
          selCells->AppendElement(static_cast<nsIAccessible*>(cell), false);
      }
    }
  }

  NS_ADDREF(*aCells = selCells);
  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetSelectedCellIndices(PRUint32* aNumCells,
                                             PRInt32** aCells)
{
  NS_ENSURE_ARG_POINTER(aNumCells);
  *aNumCells = 0;
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return NS_OK;

  PRUint32 selectedItemsCount = 0;
  nsresult rv = selectedItems->GetLength(&selectedItemsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 cellsCount = selectedItemsCount * columnCount;

  PRInt32 *cellsIdxArray =
    static_cast<PRInt32*>(nsMemory::Alloc((cellsCount) * sizeof(PRInt32)));
  NS_ENSURE_TRUE(cellsIdxArray, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 index = 0, cellsIdx = 0;
  for (; index < selectedItemsCount; index++) {
    nsCOMPtr<nsIDOMNode> itemNode;
    selectedItems->Item(index, getter_AddRefs(itemNode));
    nsCOMPtr<nsIDOMXULSelectControlItemElement> item =
      do_QueryInterface(itemNode);

    if (item) {
      PRInt32 itemIdx = -1;
      control->GetIndexOfItem(item, &itemIdx);
      if (itemIdx != -1) {
        PRInt32 colIdx = 0;
        for (; colIdx < columnCount; colIdx++)
          cellsIdxArray[cellsIdx++] = itemIdx * columnCount + colIdx;
      }
    }
  }

  *aNumCells = cellsCount;
  *aCells = cellsIdxArray;

  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetSelectedColumnIndices(PRUint32* aNumColumns,
                                               PRInt32** aColumns)
{
  NS_ENSURE_ARG_POINTER(aNumColumns);
  *aNumColumns = 0;
  NS_ENSURE_ARG_POINTER(aColumns);
  *aColumns = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 columnCount = 0;
  nsresult rv = GetSelectedColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!columnCount)
    return NS_OK;

  PRInt32 *colsIdxArray =
    static_cast<PRInt32*>(nsMemory::Alloc((columnCount) * sizeof(PRInt32)));
  NS_ENSURE_TRUE(colsIdxArray, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 colIdx = 0;
  for (; colIdx < columnCount; colIdx++)
    colsIdxArray[colIdx] = colIdx;

  *aNumColumns = columnCount;
  *aColumns = colsIdxArray;

  return NS_OK;
}

NS_IMETHODIMP
XULListboxAccessible::GetSelectedRowIndices(PRUint32* aNumRows,
                                            PRInt32** aRows)
{
  NS_ENSURE_ARG_POINTER(aNumRows);
  *aNumRows = 0;
  NS_ENSURE_ARG_POINTER(aRows);
  *aRows = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return NS_OK;

  PRUint32 selectedItemsCount = 0;
  nsresult rv = selectedItems->GetLength(&selectedItemsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!selectedItemsCount)
    return NS_OK;

  PRInt32 *rowsIdxArray =
    static_cast<PRInt32*>(nsMemory::Alloc((selectedItemsCount) * sizeof(PRInt32)));
  NS_ENSURE_TRUE(rowsIdxArray, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 index = 0;
  for (; index < selectedItemsCount; index++) {
    nsCOMPtr<nsIDOMNode> itemNode;
    selectedItems->Item(index, getter_AddRefs(itemNode));
    nsCOMPtr<nsIDOMXULSelectControlItemElement> item =
      do_QueryInterface(itemNode);

    if (item) {
      PRInt32 itemIdx = -1;
      control->GetIndexOfItem(item, &itemIdx);
      if (itemIdx != -1)
        rowsIdxArray[index] = itemIdx;
    }
  }

  *aNumRows = selectedItemsCount;
  *aRows = rowsIdxArray;

  return NS_OK;
}

void
XULListboxAccessible::SelectRow(PRUint32 aRowIdx)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRowIdx, getter_AddRefs(item));
  control->SelectItem(item);
}

void
XULListboxAccessible::UnselectRow(PRUint32 aRowIdx)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRowIdx, getter_AddRefs(item));
  control->RemoveItemFromSelection(item);
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible: Widgets

bool
XULListboxAccessible::IsWidget() const
{
  return true;
}

bool
XULListboxAccessible::IsActiveWidget() const
{
  if (IsAutoCompletePopup()) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(mContent->GetParent());

    if (autoCompletePopupElm) {
      bool isOpen = false;
      autoCompletePopupElm->GetPopupOpen(&isOpen);
      return isOpen;
    }
  }
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
XULListboxAccessible::AreItemsOperable() const
{
  if (IsAutoCompletePopup()) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(mContent->GetParent());

    if (autoCompletePopupElm) {
      bool isOpen = false;
      autoCompletePopupElm->GetPopupOpen(&isOpen);
      return isOpen;
    }
  }
  return true;
}

Accessible*
XULListboxAccessible::ContainerWidget() const
{
  if (IsAutoCompletePopup()) {
    // This works for XUL autocompletes. It doesn't work for HTML forms
    // autocomplete because of potential crossprocess calls (when autocomplete
    // lives in content process while popup lives in chrome process). If that's
    // a problem then rethink Widgets interface.
    nsCOMPtr<nsIDOMXULMenuListElement> menuListElm =
      do_QueryInterface(mContent->GetParent());
    if (menuListElm) {
      nsCOMPtr<nsIDOMNode> inputElm;
      menuListElm->GetInputField(getter_AddRefs(inputElm));
      if (inputElm) {
        nsCOMPtr<nsINode> inputNode = do_QueryInterface(inputElm);
        if (inputNode) {
          Accessible* input = 
            mDoc->GetAccessible(inputNode);
          return input ? input->ContainerWidget() : nsnull;
        }
      }
    }
  }
  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible
////////////////////////////////////////////////////////////////////////////////

XULListitemAccessible::
  XULListitemAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULMenuitemAccessible(aContent, aDoc)
{
  mIsCheckbox = mContent->AttrValueIs(kNameSpaceID_None,
                                      nsGkAtoms::type,
                                      nsGkAtoms::checkbox,
                                      eCaseMatters);
}

NS_IMPL_ISUPPORTS_INHERITED0(XULListitemAccessible, Accessible)

Accessible* 
XULListitemAccessible::GetListAccessible()
{
  if (IsDefunct())
    return nsnull;
  
  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem =
    do_QueryInterface(mContent);
  if (!listItem)
    return nsnull;

  nsCOMPtr<nsIDOMXULSelectControlElement> list;
  listItem->GetControl(getter_AddRefs(list));

  nsCOMPtr<nsIContent> listContent(do_QueryInterface(list));
  if (!listContent)
    return nsnull;

  return mDoc->GetAccessible(listContent);
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible Accessible

void
XULListitemAccessible::Description(nsString& aDesc)
{
  AccessibleWrap::Description(aDesc);
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible. nsIAccessible

/**
  * If there is a Listcell as a child ( not anonymous ) use it, otherwise
  *   default to getting the name from GetXULName
  */
nsresult
XULListitemAccessible::GetNameInternal(nsAString& aName)
{
  nsIContent* childContent = mContent->GetFirstChild();
  if (childContent) {
    if (childContent->NodeInfo()->Equals(nsGkAtoms::listcell,
                                         kNameSpaceID_XUL)) {
      childContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
      return NS_OK;
    }
  }
  return GetXULName(aName);
}

role
XULListitemAccessible::NativeRole()
{
  Accessible* list = GetListAccessible();
  if (!list) {
    NS_ERROR("No list accessible for listitem accessible!");
    return roles::NOTHING;
  }

  if (list->Role() == roles::TABLE)
    return roles::ROW;

  if (mIsCheckbox)
    return roles::CHECK_RICH_OPTION;

  if (mParent && mParent->Role() == roles::COMBOBOX_LIST)
    return roles::COMBOBOX_OPTION;

  return roles::RICH_OPTION;
}

PRUint64
XULListitemAccessible::NativeState()
{
  if (mIsCheckbox)
    return XULMenuitemAccessible::NativeState();

  PRUint64 states = NativeInteractiveState();

  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem =
    do_QueryInterface(mContent);

  if (listItem) {
    bool isSelected;
    listItem->GetSelected(&isSelected);
    if (isSelected)
      states |= states::SELECTED;

    if (FocusMgr()->IsFocused(this))
      states |= states::FOCUSED;
  }

  return states;
}

PRUint64
XULListitemAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() || (mParent && mParent->NativelyUnavailable()) ?
    states::UNAVAILABLE : states::FOCUSABLE | states::SELECTABLE;
}

NS_IMETHODIMP
XULListitemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click && mIsCheckbox) {
    // check or uncheck
    PRUint64 states = NativeState();

    if (states & states::CHECKED)
      aName.AssignLiteral("uncheck");
    else
      aName.AssignLiteral("check");

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

bool
XULListitemAccessible::CanHaveAnonChildren()
{
  // That indicates we should walk anonymous children for listitems
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible: Widgets

Accessible*
XULListitemAccessible::ContainerWidget() const
{
  return Parent();
}


////////////////////////////////////////////////////////////////////////////////
// XULListCellAccessible
////////////////////////////////////////////////////////////////////////////////

XULListCellAccessible::
  XULListCellAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(XULListCellAccessible,
                             HyperTextAccessible,
                             nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// XULListCellAccessible: nsIAccessibleTableCell implementation

NS_IMETHODIMP
XULListCellAccessible::GetTable(nsIAccessibleTable** aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* thisRow = Parent();
  if (!thisRow || thisRow->Role() != roles::ROW)
    return NS_OK;

  Accessible* table = thisRow->Parent();
  if (!table || table->Role() != roles::TABLE)
    return NS_OK;

  CallQueryInterface(table, aTable);
  return NS_OK;
}

NS_IMETHODIMP
XULListCellAccessible::GetColumnIndex(PRInt32* aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* row = Parent();
  if (!row)
    return NS_OK;

  *aColumnIndex = 0;

  PRInt32 indexInRow = IndexInParent();
  for (PRInt32 idx = 0; idx < indexInRow; idx++) {
    Accessible* cell = row->GetChildAt(idx);
    roles::Role role = cell->Role();
    if (role == roles::CELL || role == roles::GRID_CELL ||
        role == roles::ROWHEADER || role == roles::COLUMNHEADER)
      (*aColumnIndex)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
XULListCellAccessible::GetRowIndex(PRInt32* aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* row = Parent();
  if (!row)
    return NS_OK;

  Accessible* table = row->Parent();
  if (!table)
    return NS_OK;

  *aRowIndex = 0;

  PRInt32 indexInTable = row->IndexInParent();
  for (PRInt32 idx = 0; idx < indexInTable; idx++) {
    row = table->GetChildAt(idx);
    if (row->Role() == roles::ROW)
      (*aRowIndex)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
XULListCellAccessible::GetColumnExtent(PRInt32* aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
XULListCellAccessible::GetRowExtent(PRInt32* aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
XULListCellAccessible::GetColumnHeaderCells(nsIArray** aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleTable> table;
  GetTable(getter_AddRefs(table));
  NS_ENSURE_STATE(table); // we expect to be in a listbox (table)

  // Get column header cell from XUL listhead.
  Accessible* list = nsnull;

  nsRefPtr<Accessible> tableAcc(do_QueryObject(table));
  PRUint32 tableChildCount = tableAcc->ChildCount();
  for (PRUint32 childIdx = 0; childIdx < tableChildCount; childIdx++) {
    Accessible* child = tableAcc->GetChildAt(childIdx);
    if (child->Role() == roles::LIST) {
      list = child;
      break;
    }
  }

  if (list) {
    PRInt32 colIdx = -1;
    GetColumnIndex(&colIdx);

    nsIAccessible *headerCell = list->GetChildAt(colIdx);
    if (headerCell) {
      nsresult rv = NS_OK;
      nsCOMPtr<nsIMutableArray> headerCells =
        do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      headerCells->AppendElement(headerCell, false);
      NS_ADDREF(*aHeaderCells = headerCells);
      return NS_OK;
    }
  }

  // No column header cell from XUL markup, try to get it from ARIA markup.
  return nsAccUtils::GetHeaderCellsFor(table, this,
                                       nsAccUtils::eColumnHeaderCells,
                                       aHeaderCells);
}

NS_IMETHODIMP
XULListCellAccessible::GetRowHeaderCells(nsIArray** aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleTable> table;
  GetTable(getter_AddRefs(table));
  NS_ENSURE_STATE(table); // we expect to be in a listbox (table)

  // Calculate row header cells from ARIA markup.
  return nsAccUtils::GetHeaderCellsFor(table, this,
                                       nsAccUtils::eRowHeaderCells,
                                       aHeaderCells);
}

NS_IMETHODIMP
XULListCellAccessible::IsSelected(bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleTable> table;
  GetTable(getter_AddRefs(table));
  NS_ENSURE_STATE(table); // we expect to be in a listbox (table)

  PRInt32 rowIdx = -1;
  GetRowIndex(&rowIdx);

  return table->IsRowSelected(rowIdx, aIsSelected);
}

////////////////////////////////////////////////////////////////////////////////
// XULListCellAccessible. Accessible implementation

role
XULListCellAccessible::NativeRole()
{
  return roles::CELL;
}

nsresult
XULListCellAccessible::GetAttributesInternal(nsIPersistentProperties* aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // "table-cell-index" attribute
  nsCOMPtr<nsIAccessibleTable> table;
  GetTable(getter_AddRefs(table));
  NS_ENSURE_STATE(table); // we expect to be in a listbox (table)

  PRInt32 rowIdx = -1;
  GetRowIndex(&rowIdx);
  PRInt32 colIdx = -1;
  GetColumnIndex(&colIdx);

  PRInt32 cellIdx = -1;
  table->GetCellIndexAt(rowIdx, colIdx, &cellIdx);

  nsAutoString stringIdx;
  stringIdx.AppendInt(cellIdx);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::tableCellIndex,
                         stringIdx);

  return NS_OK;
}
