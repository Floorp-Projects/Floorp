/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal <aaronl@netscape.com> (original author)
 *   Kyle Yuan <kyle.yuan@sun.com>
 *   Alexander Surkov <surkov.alexander@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXULListboxAccessible.h"

#include "nsIDOMXULPopupElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsServiceManagerUtils.h"

////////////////////////////////////////////////////////////////////////////////
// nsXULColumnsAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULColumnsAccessible::
  nsXULColumnsAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell) :
  nsAccessibleWrap(aDOMNode, aShell)
{
}

nsresult
nsXULColumnsAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_LIST;
  return NS_OK;
}

nsresult
nsXULColumnsAccessible::GetStateInternal(PRUint32 *aState,
                                         PRUint32 *aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = 0;

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  *aState = nsIAccessibleStates::STATE_READONLY;

  if (aExtraState)
    *aExtraState = 0;

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULColumnItemAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULColumnItemAccessible::
  nsXULColumnItemAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell) :
  nsLeafAccessible(aDOMNode, aShell)
{
}

nsresult
nsXULColumnItemAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_COLUMNHEADER;
  return NS_OK;
}

nsresult
nsXULColumnItemAccessible::GetStateInternal(PRUint32 *aState,
                                            PRUint32 *aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  *aState = nsIAccessibleStates::STATE_READONLY;
  if (aExtraState)
    *aExtraState = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsXULColumnItemAccessible::GetNumActions(PRUint8 *aNumActions)
{
  NS_ENSURE_ARG_POINTER(aNumActions);

  *aNumActions = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsXULColumnItemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("click");
  return NS_OK;
}

NS_IMETHODIMP
nsXULColumnItemAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  return DoCommand();
}

////////////////////////////////////////////////////////////////////////////////
// nsXULListboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULListboxAccessible::
  nsXULListboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell) :
  nsXULSelectableAccessible(aDOMNode, aShell)
{
}

NS_IMPL_ADDREF_INHERITED(nsXULListboxAccessible, nsXULSelectableAccessible)
NS_IMPL_RELEASE_INHERITED(nsXULListboxAccessible, nsXULSelectableAccessible)

nsresult
nsXULListboxAccessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = nsXULSelectableAccessible::QueryInterface(aIID, aInstancePtr);
  if (*aInstancePtr)
    return rv;

  if (aIID.Equals(NS_GET_IID(nsIAccessibleTable)) && IsMulticolumn()) {
    *aInstancePtr = static_cast<nsIAccessibleTable*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

PRBool
nsXULListboxAccessible::IsMulticolumn()
{
  PRInt32 numColumns = 0;
  nsresult rv = GetColumnCount(&numColumns);
  if (NS_FAILED(rv))
    return PR_FALSE;

  return numColumns > 1;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULListboxAccessible. nsIAccessible

nsresult
nsXULListboxAccessible::GetStateInternal(PRUint32 *aState,
                                         PRUint32 *aExtraState)
{
  // As a nsXULListboxAccessible we can have the following states:
  //   STATE_FOCUSED
  //   STATE_READONLY
  //   STATE_FOCUSABLE

  // Get focus status from base class
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

// see if we are multiple select if so set ourselves as such
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(mDOMNode));
  if (element) {
    nsAutoString selType;
    element->GetAttribute(NS_LITERAL_STRING("seltype"), selType);
    if (!selType.IsEmpty() && selType.EqualsLiteral("multiple"))
      *aState |= nsIAccessibleStates::STATE_MULTISELECTABLE |
                 nsIAccessibleStates::STATE_EXTSELECTABLE;
  }

  return NS_OK;
}

/**
  * Our value is the label of our ( first ) selected child.
  */
NS_IMETHODIMP nsXULListboxAccessible::GetValue(nsAString& _retval)
{
  _retval.Truncate();
  nsCOMPtr<nsIDOMXULSelectControlElement> select(do_QueryInterface(mDOMNode));
  if (select) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
    select->GetSelectedItem(getter_AddRefs(selectedItem));
    if (selectedItem)
      return selectedItem->GetLabel(_retval);
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsXULListboxAccessible::GetRoleInternal(PRUint32 *aRole)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
  if (content) {
    // A richlistbox is used with the new autocomplete URL bar,
    // and has a parent popup <panel>
    nsCOMPtr<nsIDOMXULPopupElement> xulPopup =
      do_QueryInterface(content->GetParent());
    if (xulPopup) {
      *aRole = nsIAccessibleRole::ROLE_COMBOBOX_LIST;
      return NS_OK;
    }
  }

  if (IsMulticolumn())
    *aRole = nsIAccessibleRole::ROLE_TABLE;
  else
    *aRole = nsIAccessibleRole::ROLE_LISTBOX;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULListboxAccessible. nsIAccessibleTable

NS_IMETHODIMP
nsXULListboxAccessible::GetCaption(nsIAccessible **aCaption)
{
  NS_ENSURE_ARG_POINTER(aCaption);
  *aCaption = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSummary(nsAString &aSummary)
{
  aSummary.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetColumnCount(PRInt32 *aColumnsCout)
{
  NS_ENSURE_ARG_POINTER(aColumnsCout);
  *aColumnsCout = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  nsCOMPtr<nsIContent> headContent;
  PRUint32 count = content->GetChildCount();

  for (PRUint32 index = 0; index < count; ++index) {
    nsCOMPtr<nsIContent> childContent(content->GetChildAt(index));
    NS_ENSURE_STATE(childContent);

    if (childContent->NodeInfo()->Equals(nsAccessibilityAtoms::listcols,
                                         kNameSpaceID_XUL)) {
      headContent = childContent;
    }
  }

  if (!headContent)
    return NS_OK;

  PRUint32 columnCount = 0;
  count = headContent->GetChildCount();

  for (PRUint32 index = 0; index < count; ++index) {
    nsCOMPtr<nsIContent> childContent(headContent->GetChildAt(index));
    NS_ENSURE_STATE(childContent);

    if (childContent->NodeInfo()->Equals(nsAccessibilityAtoms::listcol,
                                         kNameSpaceID_XUL)) {
      columnCount++;
    }
  }

  *aColumnsCout = columnCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetRowCount(PRInt32 *arowCount)
{
  NS_ENSURE_ARG_POINTER(arowCount);
  *arowCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULSelectControlElement> element(do_QueryInterface(mDOMNode));
  NS_ENSURE_STATE(element);

  PRUint32 itemCount = 0;
  nsresult rv = element->GetItemCount(&itemCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *arowCount = itemCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetCellAt(PRInt32 aRow, PRInt32 aColumn,
                                  nsIAccessible **aAccessibleCell)
{
  NS_ENSURE_ARG_POINTER(aAccessibleCell);
  *aAccessibleCell = nsnull;

  if (IsDefunct())
    return NS_OK;

  nsCOMPtr<nsIDOMXULSelectControlElement> control =
    do_QueryInterface(mDOMNode);

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRow, getter_AddRefs(item));
  NS_ENSURE_TRUE(item, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMNode> itemNode(do_QueryInterface(item));

  nsCOMPtr<nsIAccessible> accessibleRow;
  GetAccService()->GetAccessibleInWeakShell(itemNode, mWeakShell,
                                            getter_AddRefs(accessibleRow));
  NS_ENSURE_STATE(accessibleRow);

  nsresult rv = accessibleRow->GetChildAt(aColumn, aAccessibleCell);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetCellIndexAt(PRInt32 aRow, PRInt32 aColumn,
                                       PRInt32 *aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);
  *aIndex = -1;

  PRInt32 rowCount = 0;
  nsresult rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(0 <= aRow && aRow <= rowCount, NS_ERROR_INVALID_ARG);

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(0 <= aColumn && aColumn <= columnCount, NS_ERROR_INVALID_ARG);

  *aIndex = aRow * columnCount + aColumn;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetColumnIndexAt(PRInt32 aIndex, PRInt32 *aColumn)
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
nsXULListboxAccessible::GetRowIndexAt(PRInt32 aIndex, PRInt32 *aRow)
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
nsXULListboxAccessible::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                          PRInt32 *aCellSpans)
{
  NS_ENSURE_ARG_POINTER(aCellSpans);
  *aCellSpans = 1;

  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                       PRInt32 *aCellSpans)
{
  NS_ENSURE_ARG_POINTER(aCellSpans);
  *aCellSpans = 1;

  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetColumnDescription(PRInt32 aColumn,
                                             nsAString& aDescription)
{
  aDescription.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetRowDescription(PRInt32 aRow, nsAString& aDescription)
{
  aDescription.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::IsColumnSelected(PRInt32 aColumn, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
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
nsXULListboxAccessible::IsRowSelected(PRInt32 aRow, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULSelectControlElement> control =
    do_QueryInterface(mDOMNode);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULSelectControlElement.");
  
  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRow, getter_AddRefs(item));
  NS_ENSURE_TRUE(item, NS_ERROR_INVALID_ARG);

  return item->GetSelected(aIsSelected);
}

NS_IMETHODIMP
nsXULListboxAccessible::IsCellSelected(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                       PRBool *aIsSelected)
{
  return IsRowSelected(aRowIndex, aIsSelected);
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedCellCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
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
nsXULListboxAccessible::GetSelectedColumnCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
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
nsXULListboxAccessible::GetSelectedRowCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  PRInt32 selectedrowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedrowCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedCells(nsIArray **aCells)
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
    do_QueryInterface(mDOMNode);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return NS_OK;

  PRUint32 selectedItemsCount = 0;
  rv = selectedItems->GetLength(&selectedItemsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 index = 0;
  for (; index < selectedItemsCount; index++) {
    nsCOMPtr<nsIDOMNode> itemNode;
    selectedItems->Item(index, getter_AddRefs(itemNode));
    nsCOMPtr<nsIAccessible> item;
    GetAccService()->GetAccessibleInWeakShell(itemNode, mWeakShell,
                                              getter_AddRefs(item));

    if (item) {
      nsCOMPtr<nsIAccessible> cell, nextCell;
      item->GetFirstChild(getter_AddRefs(cell));
      while (cell) {
        if (nsAccUtils::Role(cell) == nsIAccessibleRole::ROLE_CELL)
          selCells->AppendElement(cell, PR_FALSE);

        cell->GetNextSibling(getter_AddRefs(nextCell));
        nextCell.swap(cell);
      }
    }
  }

  NS_ADDREF(*aCells = selCells);
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedCellIndices(PRUint32 *aNumCells,
                                               PRInt32 **aCells)
{
  NS_ENSURE_ARG_POINTER(aNumCells);
  *aNumCells = 0;
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
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
nsXULListboxAccessible::GetSelectedColumnIndices(PRUint32 *aNumColumns,
                                                 PRInt32 **aColumns)
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
nsXULListboxAccessible::GetSelectedRowIndices(PRUint32 *aNumRows,
                                              PRInt32 **aRows)
{
  NS_ENSURE_ARG_POINTER(aNumRows);
  *aNumRows = 0;
  NS_ENSURE_ARG_POINTER(aRows);
  *aRows = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
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

NS_IMETHODIMP
nsXULListboxAccessible::SelectRow(PRInt32 aRow)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRow, getter_AddRefs(item));
  NS_ENSURE_TRUE(item, NS_ERROR_INVALID_ARG);

  return control->SelectItem(item);
}

NS_IMETHODIMP
nsXULListboxAccessible::SelectColumn(PRInt32 aColumn)
{
  // xul:listbox and xul:richlistbox support row selection only.
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::UnselectRow(PRInt32 aRow)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRow, getter_AddRefs(item));
  NS_ENSURE_TRUE(item, NS_ERROR_INVALID_ARG);

  return control->RemoveItemFromSelection(item);
}

NS_IMETHODIMP
nsXULListboxAccessible::UnselectColumn(PRInt32 aColumn)
{
  // xul:listbox and xul:richlistbox support row selection only.
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::IsProbablyForLayout(PRBool *aIsProbablyForLayout)
{
  NS_ENSURE_ARG_POINTER(aIsProbablyForLayout);
  *aIsProbablyForLayout = PR_FALSE;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULListitemAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULListitemAccessible::
  nsXULListitemAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell) :
  nsXULMenuitemAccessible(aDOMNode, aShell)
{
  mIsCheckbox = PR_FALSE;
  nsCOMPtr<nsIDOMElement> listItem(do_QueryInterface(mDOMNode));
  if (listItem) {
    nsAutoString typeString;
    nsresult res = listItem->GetAttribute(NS_LITERAL_STRING("type"), typeString);
    if (NS_SUCCEEDED(res) && typeString.Equals(NS_LITERAL_STRING("checkbox")))
      mIsCheckbox = PR_TRUE;
  }
}

/** Inherit the ISupports impl from nsAccessible, we handle nsIAccessibleSelectable */
NS_IMPL_ISUPPORTS_INHERITED0(nsXULListitemAccessible, nsAccessible)

already_AddRefed<nsIAccessible>
nsXULListitemAccessible::GetListAccessible()
{
  if (IsDefunct())
    return nsnull;
  
  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem =
    do_QueryInterface(mDOMNode);
  if (!listItem)
    return nsnull;

  nsCOMPtr<nsIDOMXULSelectControlElement> list;
  listItem->GetControl(getter_AddRefs(list));

  nsCOMPtr<nsIDOMNode> listNode(do_QueryInterface(list));
  if (!listNode)
    return nsnull;

  nsIAccessible *listAcc = nsnull;
  GetAccService()->GetAccessibleInWeakShell(listNode, mWeakShell, &listAcc);
  return listAcc;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULListitemAccessible. nsIAccessible

/**
  * If there is a Listcell as a child ( not anonymous ) use it, otherwise
  *   default to getting the name from GetXULName
  */
nsresult
nsXULListitemAccessible::GetNameInternal(nsAString& aName)
{
  nsCOMPtr<nsIDOMNode> child;
  if (NS_SUCCEEDED(mDOMNode->GetFirstChild(getter_AddRefs(child)))) {
    nsCOMPtr<nsIDOMElement> childElement (do_QueryInterface(child));
    if (childElement) {
      nsAutoString tagName;
      childElement->GetLocalName(tagName);
      if (tagName.EqualsLiteral("listcell")) {
        childElement->GetAttribute(NS_LITERAL_STRING("label"), aName);
        return NS_OK;
      }
    }
  }
  return GetXULName(aName);
}

nsresult
nsXULListitemAccessible::GetRoleInternal(PRUint32 *aRole)
{
  nsCOMPtr<nsIAccessible> listAcc = GetListAccessible();
  NS_ENSURE_STATE(listAcc);

  if (nsAccUtils::Role(listAcc) == nsIAccessibleRole::ROLE_TABLE) {
    *aRole = nsIAccessibleRole::ROLE_ROW;
    return NS_OK;
  }

  if (mIsCheckbox)
    *aRole = nsIAccessibleRole::ROLE_CHECKBUTTON;
  else if (nsAccUtils::Role(mParent) == nsIAccessibleRole::ROLE_COMBOBOX_LIST)
    *aRole = nsIAccessibleRole::ROLE_COMBOBOX_OPTION;
  else
    *aRole = nsIAccessibleRole::ROLE_RICH_OPTION;
  return NS_OK;
}

nsresult
nsXULListitemAccessible::GetStateInternal(PRUint32 *aState,
                                          PRUint32 *aExtraState)
{
  if (mIsCheckbox) {
    return nsXULMenuitemAccessible::GetStateInternal(aState, aExtraState);
  }

  *aState = 0;

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  if (aExtraState)
    *aExtraState = 0;

  *aState = nsIAccessibleStates::STATE_FOCUSABLE |
            nsIAccessibleStates::STATE_SELECTABLE;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem =
    do_QueryInterface(mDOMNode);

  if (listItem) {
    PRBool isSelected;
    listItem->GetSelected(&isSelected);
    if (isSelected)
      *aState |= nsIAccessibleStates::STATE_SELECTED;

    if (gLastFocusedNode == mDOMNode) {
      *aState |= nsIAccessibleStates::STATE_FOCUSED;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULListitemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click && mIsCheckbox) {
    // check or uncheck
    PRUint32 state;
    nsresult rv = GetStateInternal(&state, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    if (state & nsIAccessibleStates::STATE_CHECKED)
      aName.AssignLiteral("uncheck");
    else
      aName.AssignLiteral("check");

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

PRBool
nsXULListitemAccessible::GetAllowsAnonChildAccessibles()
{
  // That indicates we should walk anonymous children for listitems
  return PR_TRUE;
}

nsresult
nsXULListitemAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  // Call base class instead of nsXULMenuAccessible because menu accessible
  // has own implementation of group attributes setting which interferes with
  // this one.
  nsresult rv = nsAccessible::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAccUtils::SetAccAttrsForXULSelectControlItem(mDOMNode, aAttributes);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsXULListCellAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULListCellAccessible::
  nsXULListCellAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
  nsHyperTextAccessibleWrap(aDOMNode, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(nsXULListCellAccessible,
                             nsHyperTextAccessible,
                             nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// nsXULListCellAccessible: nsIAccessibleTableCell implementation

NS_IMETHODIMP
nsXULListCellAccessible::GetTable(nsIAccessibleTable **aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> thisRow;
  GetParent(getter_AddRefs(thisRow));
  if (nsAccUtils::Role(thisRow) != nsIAccessibleRole::ROLE_ROW)
    return NS_OK;

  nsCOMPtr<nsIAccessible> table;
  thisRow->GetParent(getter_AddRefs(table));
  if (nsAccUtils::Role(table) != nsIAccessibleRole::ROLE_TABLE)
    return NS_OK;

  CallQueryInterface(table, aTable);
  return NS_OK;
}

NS_IMETHODIMP
nsXULListCellAccessible::GetColumnIndex(PRInt32 *aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aColumnIndex = 0;

  nsCOMPtr<nsIAccessible> prevCell, tmpAcc;
  GetPreviousSibling(getter_AddRefs(prevCell));

  while (prevCell) {
    PRUint32 role = nsAccUtils::Role(prevCell);
    if (role == nsIAccessibleRole::ROLE_CELL ||
        role == nsIAccessibleRole::ROLE_GRID_CELL ||
        role == nsIAccessibleRole::ROLE_ROWHEADER ||
        role == nsIAccessibleRole::ROLE_COLUMNHEADER)
      (*aColumnIndex)++;

    prevCell->GetPreviousSibling(getter_AddRefs(tmpAcc));
    tmpAcc.swap(prevCell);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULListCellAccessible::GetRowIndex(PRInt32 *aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> row, prevRow;
  GetParent(getter_AddRefs(row));

  while (row) {
    if (nsAccUtils::Role(row) == nsIAccessibleRole::ROLE_ROW)
      (*aRowIndex)++;

    row->GetPreviousSibling(getter_AddRefs(prevRow));
    row.swap(prevRow);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULListCellAccessible::GetColumnExtent(PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListCellAccessible::GetRowExtent(PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListCellAccessible::GetColumnHeaderCells(nsIArray **aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleTable> table;
  GetTable(getter_AddRefs(table));
  NS_ENSURE_STATE(table); // we expect to be in a listbox (table)

  // Get column header cell from XUL listhead.
  nsCOMPtr<nsIAccessible> tableAcc(do_QueryInterface(table));

  nsCOMPtr<nsIAccessible> list, nextChild;
  tableAcc->GetFirstChild(getter_AddRefs(list));
  while (list) {
    if (nsAccUtils::Role(list) == nsIAccessibleRole::ROLE_LIST)
      break;

    list->GetNextSibling(getter_AddRefs(nextChild));
    nextChild.swap(list);
  }

  if (list) {
    PRInt32 colIdx = -1;
    GetColumnIndex(&colIdx);

    nsCOMPtr<nsIAccessible> headerCell;
    list->GetChildAt(colIdx, getter_AddRefs(headerCell));

    if (headerCell) {
      nsresult rv = NS_OK;
      nsCOMPtr<nsIMutableArray> headerCells =
        do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      headerCells->AppendElement(headerCell, PR_FALSE);
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
nsXULListCellAccessible::GetRowHeaderCells(nsIArray **aHeaderCells)
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
nsXULListCellAccessible::IsSelected(PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

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
// nsXULListCellAccessible. nsAccessible implementation

nsresult
nsXULListCellAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_CELL;
  return NS_OK;
}

nsresult
nsXULListCellAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
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
  nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::tableCellIndex,
                         stringIdx);

  return NS_OK;
}
