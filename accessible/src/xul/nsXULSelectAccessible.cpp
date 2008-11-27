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
 *   Original Author: Eric Vaughan (evaughan@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsXULSelectAccessible.h"
#include "nsAccessibilityService.h"
#include "nsIContent.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULTextboxElement.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsCaseTreatment.h"

////////////////////////////////////////////////////////////////////////////////
// nsXULColumnsAccessible

nsXULColumnsAccessible::
  nsXULColumnsAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell) :
  nsAccessibleWrap(aDOMNode, aShell)
{
}

NS_IMETHODIMP
nsXULColumnsAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);

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

nsXULColumnItemAccessible::
  nsXULColumnItemAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell) :
  nsLeafAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP
nsXULColumnItemAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);

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

nsresult
nsXULColumnItemAccessible::GetNameInternal(nsAString& aName)
{
  return GetXULName(aName);
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

  if (aIID.Equals(NS_GET_IID(nsIAccessibleTable)) && IsTree()) {
    *aInstancePtr = static_cast<nsIAccessibleTable*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

PRBool
nsXULListboxAccessible::IsTree()
{
  PRInt32 numColumns = 0;
  nsresult rv = GetColumns(&numColumns);
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

NS_IMETHODIMP nsXULListboxAccessible::GetRole(PRUint32 *aRole)
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

  if (IsTree())
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
nsXULListboxAccessible::GetColumns(PRInt32 *aNumColumns)
{
  NS_ENSURE_ARG_POINTER(aNumColumns);
  *aNumColumns = 0;

  if (!mDOMNode)
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

  *aNumColumns = columnCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  NS_ENSURE_ARG_POINTER(aColumnHeader);
  *aColumnHeader = nsnull;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetRows(PRInt32 *aNumRows)
{
  NS_ENSURE_ARG_POINTER(aNumRows);
  *aNumRows = 0;

  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULSelectControlElement> element(do_QueryInterface(mDOMNode));
  NS_ENSURE_STATE(element);

  PRUint32 itemCount = 0;
  nsresult rv = element->GetItemCount(&itemCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aNumRows = itemCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetRowHeader(nsIAccessibleTable **aRowHeader)
{
  NS_ENSURE_ARG_POINTER(aRowHeader);
  *aRowHeader = nsnull;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULListboxAccessible::CellRefAt(PRInt32 aRow, PRInt32 aColumn,
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

  nsCOMPtr<nsIAccessible> accessibleRow;
  GetAccService()->GetAccessibleInWeakShell(item, mWeakShell,
                                            getter_AddRefs(accessibleRow));
  NS_ENSURE_STATE(accessibleRow);

  nsresult rv = accessibleRow->GetChildAt(aColumn, aAccessibleCell);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetIndexAt(PRInt32 aRow, PRInt32 aColumn,
                                   PRInt32 *aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);
  *aIndex = -1;

  PRInt32 rowCount = 0;
  nsresult rv = GetRows(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(0 <= aRow && aRow <= rowCount, NS_ERROR_INVALID_ARG);

  PRInt32 columnCount = 0;
  rv = GetColumns(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(0 <= aColumn && aColumn <= columnCount, NS_ERROR_INVALID_ARG);

  *aIndex = aRow * columnCount + aColumn;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *aColumn)
{
  NS_ENSURE_ARG_POINTER(aColumn);
  *aColumn = -1;

  PRInt32 columnCount = 0;
  nsresult rv = GetColumns(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aColumn = aIndex % columnCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetRowAtIndex(PRInt32 aIndex, PRInt32 *aRow)
{
  NS_ENSURE_ARG_POINTER(aRow);
  *aRow = -1;

  PRInt32 columnCount = 0;
  nsresult rv = GetColumns(&columnCount);
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

  PRInt32 selectedRowsCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowsCount = 0;
  rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aIsSelected = (selectedRowsCount == rowsCount);
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
nsXULListboxAccessible::IsCellSelected(PRInt32 aRow, PRInt32 aColumn,
                                       PRBool *aIsSelected)
{
  return IsRowSelected(aRow, aIsSelected);
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedCellsCount(PRUint32* aCount)
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

  PRInt32 columnsCount = 0;
  rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedItemsCount * columnsCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedColumnsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
  do_QueryInterface(mDOMNode);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  PRInt32 selectedRowsCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowsCount = 0;
  rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (selectedRowsCount != rowsCount)
    return NS_OK;

  PRInt32 columnsCount = 0;
  rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = columnsCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedRowsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mDOMNode);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  PRInt32 selectedRowsCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedRowsCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedCells(PRUint32 *aNumCells, PRInt32 **aCells)
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

  PRInt32 columnsCount = 0;
  rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 cellsCount = selectedItemsCount * columnsCount;

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
        for (; colIdx < columnsCount; colIdx++)
          cellsIdxArray[cellsIdx++] = itemIdx * columnsCount + colIdx;
      }
    }
  }

  *aNumCells = cellsCount;
  *aCells = cellsIdxArray;

  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedColumns(PRUint32 *aNumColumns,
                                           PRInt32 **aColumns)
{
  NS_ENSURE_ARG_POINTER(aNumColumns);
  *aNumColumns = 0;
  NS_ENSURE_ARG_POINTER(aColumns);
  *aColumns = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 columnsCount = 0;
  nsresult rv = GetSelectedColumnsCount(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!columnsCount)
    return NS_OK;

  PRInt32 *colsIdxArray =
    static_cast<PRInt32*>(nsMemory::Alloc((columnsCount) * sizeof(PRInt32)));
  NS_ENSURE_TRUE(colsIdxArray, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 colIdx = 0;
  for (; colIdx < columnsCount; colIdx++)
    colsIdxArray[colIdx] = colIdx;

  *aNumColumns = columnsCount;
  *aColumns = colsIdxArray;

  return NS_OK;
}

NS_IMETHODIMP
nsXULListboxAccessible::GetSelectedRows(PRUint32 *aNumRows, PRInt32 **aRows)
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

nsXULListitemAccessible::
  nsXULListitemAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell) :
  nsXULMenuitemAccessible(aDOMNode, aShell)
{
  mIsCheckbox = PR_FALSE;
  nsCOMPtr<nsIDOMElement> listItem (do_QueryInterface(mDOMNode));
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

/**
  *
  */
NS_IMETHODIMP nsXULListitemAccessible::GetRole(PRUint32 *aRole)
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
  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem (do_QueryInterface(mDOMNode));
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

NS_IMETHODIMP
nsXULListitemAccessible::GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren)
{
  // That indicates we should walk anonymous children for listitems
  *aAllowsAnonChildren = PR_TRUE;
  return NS_OK;
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
nsXULListCellAccessible::
  nsXULListCellAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
  nsHyperTextAccessibleWrap(aDOMNode, aShell)
{
}

NS_IMETHODIMP
nsXULListCellAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);

  *aRole = nsIAccessibleRole::ROLE_CELL;
  return NS_OK;
}

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/** ----- nsXULComboboxAccessible ----- */

/** Constructor */
nsXULComboboxAccessible::nsXULComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsAccessibleWrap(aDOMNode, aShell)
{
}

nsresult
nsXULComboboxAccessible::Init()
{
  nsresult rv = nsAccessibleWrap::Init();
  nsXULMenupopupAccessible::GenerateMenu(mDOMNode);
  return rv;
}

/** We are a combobox */
NS_IMETHODIMP nsXULComboboxAccessible::GetRole(PRUint32 *aRole)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
  if (!content) {
    return NS_ERROR_FAILURE;
  }
  if (content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                           NS_LITERAL_STRING("autocomplete"), eIgnoreCase)) {
    *aRole = nsIAccessibleRole::ROLE_AUTOCOMPLETE;
  } else {
    *aRole = nsIAccessibleRole::ROLE_COMBOBOX;
  }
  return NS_OK;
}

/**
  * As a nsComboboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_HASPOPUP
  *     STATE_EXPANDED
  *     STATE_COLLAPSED
  */
nsresult
nsXULComboboxAccessible::GetStateInternal(PRUint32 *aState,
                                          PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (menuList) {
    PRBool isOpen;
    menuList->GetOpen(&isOpen);
    if (isOpen) {
      *aState |= nsIAccessibleStates::STATE_EXPANDED;
    }
    else {
      *aState |= nsIAccessibleStates::STATE_COLLAPSED;
    }
  }

  *aState |= nsIAccessibleStates::STATE_HASPOPUP |
             nsIAccessibleStates::STATE_FOCUSABLE;

  return NS_OK;
}

NS_IMETHODIMP nsXULComboboxAccessible::GetValue(nsAString& _retval)
{
  _retval.Truncate();

  // The MSAA/ATK value is the option or text shown entered in the combobox
  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (menuList) {
    return menuList->GetLabel(_retval);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULComboboxAccessible::GetDescription(nsAString& aDescription)
{
  // Use description of currently focused option
  aDescription.Truncate();
  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (!menuList) {
    return NS_ERROR_FAILURE;  // Shut down
  }
  nsCOMPtr<nsIDOMXULSelectControlItemElement> focusedOption;
  menuList->GetSelectedItem(getter_AddRefs(focusedOption));
  nsCOMPtr<nsIDOMNode> focusedOptionNode(do_QueryInterface(focusedOption));
  if (focusedOptionNode) {
    nsCOMPtr<nsIAccessibilityService> accService = 
      do_GetService("@mozilla.org/accessibilityService;1");
    NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);
    nsCOMPtr<nsIAccessible> focusedOptionAccessible;
    accService->GetAccessibleInWeakShell(focusedOptionNode, mWeakShell, 
                                        getter_AddRefs(focusedOptionAccessible));
    NS_ENSURE_TRUE(focusedOptionAccessible, NS_ERROR_FAILURE);
    return focusedOptionAccessible->GetDescription(aDescription);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULComboboxAccessible::GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren)
{
  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);

  if (content->NodeInfo()->Equals(nsAccessibilityAtoms::textbox, kNameSpaceID_XUL) ||
      content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::editable,
                           nsAccessibilityAtoms::_true, eIgnoreCase)) {
    // Both the XUL <textbox type="autocomplete"> and <menulist editable="true"> widgets
    // use nsXULComboboxAccessible. We need to walk the anonymous children for these
    // so that the entry field is a child
    *aAllowsAnonChildren = PR_TRUE;
  } else {
    // Argument of PR_FALSE indicates we don't walk anonymous children for
    // menuitems
    *aAllowsAnonChildren = PR_FALSE;
  }
  return NS_OK;
}

/** Just one action ( click ). */
NS_IMETHODIMP nsXULComboboxAccessible::GetNumActions(PRUint8 *aNumActions)
{
  *aNumActions = 1;
  return NS_OK;
}

/**
  * Programmaticaly toggle the combo box
  */
NS_IMETHODIMP nsXULComboboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != nsXULComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (!menuList) {
    return NS_ERROR_FAILURE;
  }
  PRBool isDroppedDown;
  menuList->GetOpen(&isDroppedDown);
  return menuList->SetOpen(!isDroppedDown);
}

/**
  * Our action name is the reverse of our state: 
  *     if we are closed -> open is our name.
  *     if we are open -> closed is our name.
  * Uses the frame to get the state, updated on every click
  */
NS_IMETHODIMP nsXULComboboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != nsXULComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mDOMNode));
  if (!menuList) {
    return NS_ERROR_FAILURE;
  }
  PRBool isDroppedDown;
  menuList->GetOpen(&isDroppedDown);
  if (isDroppedDown)
    aName.AssignLiteral("close"); 
  else
    aName.AssignLiteral("open"); 

  return NS_OK;
}

