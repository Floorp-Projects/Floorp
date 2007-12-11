/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pete Zha (pete.zha@sun.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIDOMElement.h"
#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsXULTreeAccessibleWrap.h"

// --------------------------------------------------------
// nsXULTreeAccessibleWrap Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeAccessibleWrap, nsXULTreeAccessible, nsIAccessibleTable)

nsXULTreeAccessibleWrap::nsXULTreeAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULTreeAccessible(aDOMNode, aShell)
{
}

// tree's children count is row count * col count + treecols count
// override "children count = row count + treecols count" defined in
// nsXULTreeAccessible
NS_IMETHODIMP nsXULTreeAccessibleWrap::GetChildCount(PRInt32 *aAccChildCount)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  // get treecols count, which is cached by nsAccessibleTreeWalker
  // by going through DOM structure of XUL tree
  nsAccessible::GetChildCount(aAccChildCount);

  if (*aAccChildCount != 0 && *aAccChildCount != eChildCountUninitialized) {
    // add the count of table cell (or tree item) accessibles, which are
    // created and appended by XUL tree accessible implementation
    PRInt32 rowCount, colCount = 1;
    mTreeView->GetRowCount(&rowCount);
    mFirstChild->GetChildCount(&colCount);

    *aAccChildCount += rowCount * colCount;
  }
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetCaption(nsIAccessible **aCaption)
{
  *aCaption = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetSummary(nsAString &aSummary)
{
  aSummary.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumns(PRInt32 *aColumns)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIAccessible> acc;
  rv = nsAccessible::GetFirstChild(getter_AddRefs(acc));
  NS_ENSURE_TRUE(acc, NS_ERROR_FAILURE);

  rv = acc->GetChildCount(aColumns);

  // The last child could be column picker. In that case, we need to minus the
  // number of columns by 1
  nsCOMPtr<nsIAccessible> lastChildAccessible;
  acc->GetLastChild(getter_AddRefs(lastChildAccessible));
  nsCOMPtr<nsIAccessNode> accessNode = do_QueryInterface(lastChildAccessible);
  NS_ENSURE_TRUE(accessNode, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMNode> domNode;
  accessNode->GetDOMNode(getter_AddRefs(domNode));
  nsCOMPtr<nsIContent> content = do_QueryInterface(domNode);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  // it's menupopup inside column picker
  if (content->NodeInfo()->Equals(nsAccessibilityAtoms::menupopup,
                                  kNameSpaceID_XUL)) {
    (*aColumns)--;
  }

  return *aColumns > 0 ? rv : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIAccessible> acc;
  nsAccessible::GetFirstChild(getter_AddRefs(acc));
  NS_ENSURE_TRUE(acc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessibleTable> accTable(do_QueryInterface(acc, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  *aColumnHeader = accTable;
  NS_IF_ADDREF(*aColumnHeader);

  return rv;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRows(PRInt32 *aRows)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  return mTreeView->GetRowCount(aRows);
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRowHeader(nsIAccessibleTable **aRowHeader)
{
  // Row header not supported
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeAccessibleWrap::GetSelectedCellsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeAccessibleWrap::GetSelectedColumnsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeAccessibleWrap::GetSelectedRowsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeAccessibleWrap::GetSelectedCells(PRUint32 *aNumCells,
                                          PRInt32 **aCells)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetSelectedColumns(PRUint32 *aNumColumns, PRInt32 **aColumns)
{
  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aNumColumns);

  nsresult rv = NS_OK;

  PRInt32 rows;
  rv = GetRows(&rows);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedRows;
  rv = GetSelectionCount(&selectedRows);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rows == selectedRows) {
    PRInt32 columns;
    rv = GetColumns(&columns);
    NS_ENSURE_SUCCESS(rv, rv);

    *aNumColumns = columns;
  } else {
    *aNumColumns = 0;
    return rv;
  }

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumColumns) * sizeof(PRInt32));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 index = 0; index < *aNumColumns; index++) {
    outArray[index] = index;
  }

  *aColumns = outArray;
  return rv;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetSelectedRows(PRUint32 *aNumRows, PRInt32 **aRows)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aNumRows);

  nsresult rv = NS_OK;

  rv = GetSelectionCount((PRInt32 *)aNumRows);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumRows) * sizeof(PRInt32));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsITreeView> view;
  rv = mTree->GetView(getter_AddRefs(view));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITreeSelection> selection;
  rv = view->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount;
  rv = GetRows(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelected;
  PRInt32 index, curr = 0;
  for (index = 0; index < rowCount; index++) {
    selection->IsSelected(index, &isSelected);
    if (isSelected) {
      outArray[curr++] = index;
    }
  }

  *aRows = outArray;
  return rv;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::CellRefAt(PRInt32 aRow, PRInt32 aColumn, nsIAccessible **aAccessibleCell)
{
  NS_ENSURE_TRUE(mDOMNode && mTree, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;

  PRInt32 index;
  rv = GetIndexAt(aRow, aColumn, &index);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetChildAt(index, aAccessibleCell);
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetIndexAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *aIndex)
{
  NS_ENSURE_TRUE(mDOMNode, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aIndex);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 treeCols;
  nsAccessible::GetChildCount(&treeCols);
  *aIndex = aRow * columns + aColumn + treeCols;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 treeCols;
  nsAccessible::GetChildCount(&treeCols);

  *_retval = (aIndex - treeCols) % columns;
  
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRowAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 treeCols;
  nsAccessible::GetChildCount(&treeCols);

  *_retval = (aIndex - treeCols) / columns;

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumnDescription(PRInt32 aColumn, nsAString & _retval)
{
  nsCOMPtr<nsIAccessibleTable> columnHeader;
  nsresult rv = GetColumnHeader(getter_AddRefs(columnHeader));
  if (NS_SUCCEEDED(rv) && columnHeader) {
    return columnHeader->GetColumnDescription(aColumn, _retval);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRowDescription(PRInt32 aRow, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::IsColumnSelected(PRInt32 aColumn, PRBool *_retval)
{
  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 rows;
  rv = GetRows(&rows);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedRows;
  rv = GetSelectionCount(&selectedRows);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = rows == selectedRows;
  return rv;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::IsRowSelected(PRInt32 aRow, PRBool *_retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;

  nsCOMPtr<nsITreeView> view;
  rv = mTree->GetView(getter_AddRefs(view));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITreeSelection> selection;
  rv = view->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  return selection->IsSelected(aRow, _retval);
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::IsCellSelected(PRInt32 aRow, PRInt32 aColumn, PRBool *_retval)
{
  return IsRowSelected(aRow, _retval);
}

NS_IMETHODIMP
nsXULTreeAccessibleWrap::SelectRow(PRInt32 aRow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeAccessibleWrap::SelectColumn(PRInt32 aColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeAccessibleWrap::UnselectRow(PRInt32 aRow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeAccessibleWrap::UnselectColumn(PRInt32 aColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  PRInt32 rowIndex;
  nsresult rv = GetRowAtIndex(aIndex, &rowIndex);

  nsCOMPtr<nsITreeSelection> selection;
  rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ASSERTION(selection, "Can't get selection from mTreeView");

  if (selection) {
    selection->IsSelected(rowIndex, aSelState);
    // XXX: Can move to nsXULTreeAccessible if this can be applied to cross-platform
    if ((!(*aSelState) && eSelection_Add == aMethod)) {
      nsresult rv = selection->Select(rowIndex);
      mTree->EnsureRowIsVisible(aIndex);
      return rv;
    }
    // XXX: Will eSelection_Remove happen for XULTree? Leave the original implementation here
    if ((*aSelState) && eSelection_Remove == aMethod) {
      return selection->ToggleSelect(rowIndex);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::IsProbablyForLayout(PRBool *aIsProbablyForLayout)
{
  *aIsProbablyForLayout = PR_FALSE;
  return NS_OK;
}

// --------------------------------------------------------
// nsXULTreeColumnsAccessibleWrap Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeColumnsAccessibleWrap, nsXULTreeColumnsAccessible, nsIAccessibleTable)

nsXULTreeColumnsAccessibleWrap::nsXULTreeColumnsAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULTreeColumnsAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetCaption(nsIAccessible **aCaption)
{
  *aCaption = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetSummary(nsAString &aSummary)
{
  aSummary.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumns(PRInt32 *aColumns)
{
  nsresult rv = GetChildCount(aColumns);
  return *aColumns > 0 ? rv : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumnHeader(nsIAccessibleTable * *aColumnHeader)
{
  // Column header not supported.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRows(PRInt32 *aRows)
{
  NS_ENSURE_ARG_POINTER(aRows);

  *aRows = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRowHeader(nsIAccessibleTable * *aRowHeader)
{
  // Row header not supported.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::GetSelectedCellsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::GetSelectedColumnsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::GetSelectedRowsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::GetSelectedCells(PRUint32 *aNumCells,
                                                 PRInt32 **aCells)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetSelectedColumns(PRUint32 *columnsSize, PRInt32 **columns)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetSelectedRows(PRUint32 *rowsSize, PRInt32 **rows)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::CellRefAt(PRInt32 aRow, PRInt32 aColumn, nsIAccessible **_retval)
{
  nsCOMPtr<nsIAccessible> next, temp;
  GetFirstChild(getter_AddRefs(next));
  NS_ENSURE_TRUE(next, NS_ERROR_FAILURE);

  for (PRInt32 col = 0; col < aColumn; col++) {
    next->GetNextSibling(getter_AddRefs(temp));
    NS_ENSURE_TRUE(temp, NS_ERROR_FAILURE);

    next = temp;
  }

  *_retval = next;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetIndexAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = aColumn;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = aIndex;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRowAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumnDescription(PRInt32 aColumn, nsAString & _retval)
{
  nsCOMPtr<nsIAccessible> column;  
  nsresult rv = CellRefAt(0, aColumn, getter_AddRefs(column));
  if (NS_SUCCEEDED(rv) && column) {
    return column->GetName(_retval);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRowDescription(PRInt32 aRow, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::IsColumnSelected(PRInt32 aColumn, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::IsRowSelected(PRInt32 aRow, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::IsCellSelected(PRInt32 aRow, PRInt32 aColumn, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::SelectRow(PRInt32 aRow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::SelectColumn(PRInt32 aColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::UnselectRow(PRInt32 aRow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::UnselectColumn(PRInt32 aColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::IsProbablyForLayout(PRBool *aIsProbablyForLayout)
{
  *aIsProbablyForLayout = PR_FALSE;
  return NS_OK;
}
