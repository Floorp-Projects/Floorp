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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsXULTreeGridAccessible.h"

#include "nsITreeSelection.h"

////////////////////////////////////////////////////////////////////////////////
// Internal static functions
////////////////////////////////////////////////////////////////////////////////

static PLDHashOperator
ElementTraverser(const void *aKey, nsIAccessNode *aAccessNode,
                 void *aUserArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aUserArg);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb,
                                     "mAccessNodeCache of XUL tree row entry");
  cb->NoteXPCOMChild(aAccessNode);
  return PL_DHASH_NEXT;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTreeGridAccessible::
  nsXULTreeGridAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell) :
  nsXULTreeAccessible(aDOMNode, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeGridAccessible,
                             nsXULTreeAccessible,
                             nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsIAccessibleTable implementation

NS_IMETHODIMP
nsXULTreeGridAccessible::GetCaption(nsIAccessible **aCaption)
{
  NS_ENSURE_ARG_POINTER(aCaption);
  *aCaption = nsnull;

  return IsDefunct() ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSummary(nsAString &aSummary)
{
  aSummary.Truncate();
  return IsDefunct() ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetColumns(PRInt32 *aColumnsCount)
{
  NS_ENSURE_ARG_POINTER(aColumnsCount);
  *aColumnsCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aColumnsCount = nsCoreUtils::GetSensibleColumnsCount(mTree);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  NS_ENSURE_ARG_POINTER(aColumnHeader);
  *aColumnHeader = nsnull;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetRows(PRInt32 *aRowsCount)
{
  NS_ENSURE_ARG_POINTER(aRowsCount);
  *aRowsCount = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  return mTreeView->GetRowCount(aRowsCount);
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetRowHeader(nsIAccessibleTable **aRowHeader)
{
  NS_ENSURE_ARG_POINTER(aRowHeader);
  *aRowHeader = nsnull;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedCellsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  PRUint32 selectedRowsCount = 0;
  nsresult rv = GetSelectedRowsCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnsCount = 0;
  rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedRowsCount * columnsCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedColumnsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // If all the row has been selected, then all the columns are selected,
  // because we can't select a column alone.

  PRInt32 rowsCount = 0;
  nsresult rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedRowsCount = 0;
  rv = GetSelectionCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rowsCount == selectedRowsCount) {
    PRInt32 columnsCount = 0;
    rv = GetColumns(&columnsCount);
    NS_ENSURE_SUCCESS(rv, rv);

    *aCount = columnsCount;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedRowsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 selectedRowsCount = 0;
  nsresult rv = GetSelectionCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedRowsCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedCells(PRUint32 *aCellsCount,
                                          PRInt32 **aCells)
{
  NS_ENSURE_ARG_POINTER(aCellsCount);
  *aCellsCount = 0;
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  PRInt32 selectedRowsCount = 0;
  nsresult rv = GetSelectionCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnsCount = 0;
  rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedCellsCount = selectedRowsCount * columnsCount;
  PRInt32* outArray = static_cast<PRInt32*>(
    nsMemory::Alloc(selectedCellsCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsITreeSelection> selection;
  rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowsCount = 0;
  rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelected;
  for (PRInt32 rowIdx = 0, arrayIdx = 0; rowIdx < rowsCount; rowIdx++) {
    selection->IsSelected(rowIdx, &isSelected);
    if (isSelected) {
      for (PRInt32 colIdx = 0; colIdx < columnsCount; colIdx++)
        outArray[arrayIdx++] = rowIdx * columnsCount + colIdx;
    }
  }

  *aCellsCount = selectedCellsCount;
  *aCells = outArray;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedColumns(PRUint32 *aColumnsCount,
                                            PRInt32 **aColumns)
{
  NS_ENSURE_ARG_POINTER(aColumnsCount);
  *aColumnsCount = 0;
  NS_ENSURE_ARG_POINTER(aColumns);
  *aColumns = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.

  PRInt32 rowsCount = 0;
  nsresult rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedRowsCount = 0;
  rv = GetSelectionCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rowsCount != selectedRowsCount)
    return NS_OK;

  PRInt32 columnsCount = 0;
  rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32* outArray = static_cast<PRInt32*>(
    nsMemory::Alloc(columnsCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  for (PRInt32 colIdx = 0; colIdx < columnsCount; colIdx++)
    outArray[colIdx] = colIdx;

  *aColumnsCount = columnsCount;
  *aColumns = outArray;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedRows(PRUint32 *aRowsCount, PRInt32 **aRows)
{
  NS_ENSURE_ARG_POINTER(aRowsCount);
  *aRowsCount = 0;
  NS_ENSURE_ARG_POINTER(aRows);
  *aRows = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 selectedRowsCount = 0;
  nsresult rv = GetSelectionCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32* outArray = static_cast<PRInt32*>(
    nsMemory::Alloc(selectedRowsCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsITreeSelection> selection;
  rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowsCount = 0;
  rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelected;
  for (PRInt32 rowIdx = 0, arrayIdx = 0; rowIdx < rowsCount; rowIdx++) {
    selection->IsSelected(rowIdx, &isSelected);
    if (isSelected)
      outArray[arrayIdx++] = rowIdx;
  }

  *aRowsCount = selectedRowsCount;
  *aRows = outArray;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::CellRefAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                   nsIAccessible **aCell)
{
  NS_ENSURE_ARG_POINTER(aCell);
  *aCell = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> rowAccessible;
  GetTreeItemAccessible(aRowIndex, getter_AddRefs(rowAccessible));
  if (!rowAccessible)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsITreeColumn> column =
  nsCoreUtils::GetSensibleColumnAt(mTree, aColumnIndex);
  if (!column)
    return NS_ERROR_INVALID_ARG;

  nsRefPtr<nsXULTreeItemAccessibleBase> rowAcc =
    nsAccUtils::QueryObject<nsXULTreeItemAccessibleBase>(rowAccessible);

  rowAcc->GetCellAccessible(column, aCell);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetIndexAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                    PRInt32 *aCellIndex)
{
  NS_ENSURE_ARG_POINTER(aCellIndex);
  *aCellIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 columnsCount = 0;
  nsresult rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCellIndex = aRowIndex * columnsCount + aColumnIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetColumnAtIndex(PRInt32 aCellIndex,
                                          PRInt32 *aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  PRInt32 columnsCount = 0;
  nsresult rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aColumnIndex = aCellIndex % columnsCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetRowAtIndex(PRInt32 aCellIndex, PRInt32 *aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  PRInt32 columnsCount;
  nsresult rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aRowIndex = aCellIndex / columnsCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetColumnExtentAt(PRInt32 aRowIndex,
                                           PRInt32 aColumnIndex,
                                           PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 1;

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetRowExtentAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                        PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 1;

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetColumnDescription(PRInt32 aColumnIndex,
                                              nsAString& aDescription)
{
  aDescription.Truncate();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetRowDescription(PRInt32 aRowIndex,
                                           nsAString& aDescription)
{
  aDescription.Truncate();

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::IsColumnSelected(PRInt32 aColumnIndex,
                                          PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.
  
  PRInt32 rowsCount = 0;
  nsresult rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedRowsCount = 0;
  rv = GetSelectionCount(&selectedRowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aIsSelected = rowsCount == selectedRowsCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::IsRowSelected(PRInt32 aRowIndex, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeSelection> selection;
  nsresult rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);
  
  return selection->IsSelected(aRowIndex, aIsSelected);
}

NS_IMETHODIMP
nsXULTreeGridAccessible::IsCellSelected(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                        PRBool *aIsSelected)
{
  return IsRowSelected(aRowIndex, aIsSelected);
}

NS_IMETHODIMP
nsXULTreeGridAccessible::SelectRow(PRInt32 aRowIndex)
{
  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_STATE(selection);

  return selection->Select(aRowIndex);
}

NS_IMETHODIMP
nsXULTreeGridAccessible::SelectColumn(PRInt32 aColumnIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::UnselectRow(PRInt32 aRowIndex)
{
  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_STATE(selection);

  return selection->ClearRange(aRowIndex, aRowIndex);
}

NS_IMETHODIMP
nsXULTreeGridAccessible::UnselectColumn(PRInt32 aColumnIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::IsProbablyForLayout(PRBool *aIsProbablyForLayout)
{
  NS_ENSURE_ARG_POINTER(aIsProbablyForLayout);
  *aIsProbablyForLayout = PR_FALSE;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsAccessible implementation

nsresult
nsXULTreeGridAccessible::GetRoleInternal(PRUint32 *aRole)
{
  nsCOMPtr<nsITreeColumns> treeColumns;
  mTree->GetColumns(getter_AddRefs(treeColumns));
  NS_ENSURE_STATE(treeColumns);

  nsCOMPtr<nsITreeColumn> primaryColumn;
  treeColumns->GetPrimaryColumn(getter_AddRefs(primaryColumn));

  *aRole = primaryColumn ?
    nsIAccessibleRole::ROLE_TREE_TABLE :
    nsIAccessibleRole::ROLE_TABLE;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsXULTreeAccessible implementation

void
nsXULTreeGridAccessible::CreateTreeItemAccessible(PRInt32 aRow,
                                                  nsAccessNode** aAccessNode)
{
  *aAccessNode = new nsXULTreeGridRowAccessible(mDOMNode, mWeakShell, this,
                                                mTree, mTreeView, aRow);
  NS_IF_ADDREF(*aAccessNode);
}


////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTreeGridRowAccessible::
  nsXULTreeGridRowAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell,
                             nsIAccessible *aTreeAcc, nsITreeBoxObject* aTree,
                             nsITreeView *aTreeView, PRInt32 aRow) :
  nsXULTreeItemAccessibleBase(aDOMNode, aShell, aTreeAcc, aTree, aTreeView, aRow)
{
  mAccessNodeCache.Init(kDefaultTreeCacheSize);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsISupports and cycle collection implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULTreeGridRowAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXULTreeGridRowAccessible,
                                                  nsAccessible)
tmp->mAccessNodeCache.EnumerateRead(ElementTraverser, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXULTreeGridRowAccessible,
                                                nsAccessible)
tmp->ClearCache(tmp->mAccessNodeCache);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXULTreeGridRowAccessible)
NS_INTERFACE_MAP_STATIC_AMBIGUOUS(nsXULTreeGridRowAccessible)
NS_INTERFACE_MAP_END_INHERITING(nsXULTreeItemAccessibleBase)

NS_IMPL_ADDREF_INHERITED(nsXULTreeGridRowAccessible,
                         nsXULTreeItemAccessibleBase)
NS_IMPL_RELEASE_INHERITED(nsXULTreeGridRowAccessible,
                          nsXULTreeItemAccessibleBase)

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsIAccessible implementation

NS_IMETHODIMP
nsXULTreeGridRowAccessible::GetFirstChild(nsIAccessible **aFirstChild)
{
  NS_ENSURE_ARG_POINTER(aFirstChild);
  *aFirstChild = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeColumn> column = nsCoreUtils::GetFirstSensibleColumn(mTree);
  NS_ASSERTION(column, "No column for table grid!");
  if (!column)
    return NS_ERROR_FAILURE;

  GetCellAccessible(column, aFirstChild);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridRowAccessible::GetLastChild(nsIAccessible **aLastChild)
{
  NS_ENSURE_ARG_POINTER(aLastChild);
  *aLastChild = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeColumn> column = nsCoreUtils::GetLastSensibleColumn(mTree);
  NS_ASSERTION(column, "No column for table grid!");
  if (!column)
    return NS_ERROR_FAILURE;

  GetCellAccessible(column, aLastChild);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridRowAccessible::GetChildCount(PRInt32 *aChildCount)
{
  NS_ENSURE_ARG_POINTER(aChildCount);
  *aChildCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aChildCount = nsCoreUtils::GetSensibleColumnsCount(mTree);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridRowAccessible::GetChildAt(PRInt32 aChildIndex,
                                       nsIAccessible **aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeColumn> column =
    nsCoreUtils::GetSensibleColumnAt(mTree, aChildIndex);
  if (!column)
    return NS_ERROR_INVALID_ARG;

  GetCellAccessible(column, aChild);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsAccessNode implementation

nsresult
nsXULTreeGridRowAccessible::Shutdown()
{
  ClearCache(mAccessNodeCache);
  return nsXULTreeItemAccessibleBase::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsAccessible implementation

nsresult
nsXULTreeGridRowAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_ROW;
  return NS_OK;
}

nsresult
nsXULTreeGridRowAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                            PRBool aDeepestChild,
                                            nsIAccessible **aChild)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return NS_ERROR_FAILURE;

  nsPresContext *presContext = frame->PresContext();
  nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();

  nsIFrame *rootFrame = presShell->GetRootFrame();
  NS_ENSURE_STATE(rootFrame);

  nsIntRect rootRect = rootFrame->GetScreenRectExternal();

  PRInt32 clientX = presContext->DevPixelsToIntCSSPixels(aX - rootRect.x);
  PRInt32 clientY = presContext->DevPixelsToIntCSSPixels(aY - rootRect.y);

  PRInt32 row = -1;
  nsCOMPtr<nsITreeColumn> column;
  nsCAutoString childEltUnused;
  mTree->GetCellAt(clientX, clientY, &row, getter_AddRefs(column),
                   childEltUnused);

  // Return if we failed to find tree cell in the row for the given point.
  if (row != mRow || !column)
    return NS_OK;

  GetCellAccessible(column, aChild);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsXULTreeItemAccessibleBase implementation

void
nsXULTreeGridRowAccessible::GetCellAccessible(nsITreeColumn* aColumn,
                                              nsIAccessible** aAccessible)
{
  NS_PRECONDITION(aColumn, "No tree column!");
  *aAccessible = nsnull;

  void* key = static_cast<void*>(aColumn);

  nsCOMPtr<nsIAccessNode> accessNode;
  GetCacheEntry(mAccessNodeCache, key, getter_AddRefs(accessNode));

  if (!accessNode) {
    nsRefPtr<nsAccessNode> cellAcc =
      new nsXULTreeGridCellAccessible(mDOMNode, mWeakShell, this, mTree,
                                      mTreeView, mRow, aColumn);
    if (!cellAcc)
      return;

    nsresult rv = cellAcc->Init();
    if (NS_FAILED(rv))
      return;

    accessNode = cellAcc;
    PutCacheEntry(mAccessNodeCache, key, accessNode);
  }

  CallQueryInterface(accessNode, aAccessible);
}

void
nsXULTreeGridRowAccessible::RowInvalidated(PRInt32 aStartColIdx,
                                           PRInt32 aEndColIdx)
{
  nsCOMPtr<nsITreeColumns> treeColumns;
  mTree->GetColumns(getter_AddRefs(treeColumns));
  if (!treeColumns)
    return;

  for (PRInt32 colIdx = aStartColIdx; colIdx <= aEndColIdx; ++colIdx) {
    nsCOMPtr<nsITreeColumn> column;
    treeColumns->GetColumnAt(colIdx, getter_AddRefs(column));
    if (column && !nsCoreUtils::IsColumnHidden(column)) {
      nsCOMPtr<nsIAccessible> cellAccessible;
      GetCellAccessible(column, getter_AddRefs(cellAccessible));
      if (cellAccessible) {
        nsRefPtr<nsXULTreeGridCellAccessible> cellAcc =
          nsAccUtils::QueryObject<nsXULTreeGridCellAccessible>(cellAccessible);

        cellAcc->CellInvalidated();
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTreeGridCellAccessible::
nsXULTreeGridCellAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell,
                            nsXULTreeGridRowAccessible *aRowAcc,
                            nsITreeBoxObject *aTree, nsITreeView *aTreeView,
                            PRInt32 aRow, nsITreeColumn* aColumn) :
  nsLeafAccessible(aDOMNode, aShell), mTree(aTree),
  mTreeView(aTreeView), mRow(aRow), mColumn(aColumn)
{
  mParent = aRowAcc;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeGridCellAccessible,
                             nsLeafAccessible,
                             nsXULTreeGridCellAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsIAccessNode implementation

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetUniqueID(void **aUniqueID)
{
  NS_ENSURE_ARG_POINTER(aUniqueID);
  *aUniqueID = static_cast<void*>(this);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsIAccessible implementation

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetParent(nsIAccessible **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (mParent) {
    *aParent = mParent;
    NS_ADDREF(*aParent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeColumn> nextColumn =
    nsCoreUtils::GetNextSensibleColumn(mColumn);
  if (!nextColumn)
    return NS_OK;

  nsRefPtr<nsXULTreeItemAccessibleBase> rowAcc =
    nsAccUtils::QueryObject<nsXULTreeItemAccessibleBase>(mParent);
  rowAcc->GetCellAccessible(nextColumn, aNextSibling);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetPreviousSibling(nsIAccessible **aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);
  *aPreviousSibling = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeColumn> nextColumn =
    nsCoreUtils::GetPreviousSensibleColumn(mColumn);
  if (!nextColumn)
    return NS_OK;

  nsRefPtr<nsXULTreeItemAccessibleBase> rowAcc =
    nsAccUtils::QueryObject<nsXULTreeItemAccessibleBase>(mParent);
  rowAcc->GetCellAccessible(nextColumn, aPreviousSibling);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetFocusedChild(nsIAccessible **aFocusedChild) 
{
  NS_ENSURE_ARG_POINTER(aFocusedChild);
  *aFocusedChild = nsnull;

  return IsDefunct() ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetName(nsAString& aName)
{
  aName.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  mTreeView->GetCellText(mRow, mColumn, aName);

  // If there is still no name try the cell value:
  // This is for graphical cells. We need tree/table view implementors to implement
  // FooView::GetCellValue to return a meaningful string for cases where there is
  // something shown in the cell (non-text) such as a star icon; in which case
  // GetCellValue for that cell would return "starred" or "flagged" for example.
  if (aName.IsEmpty())
    mTreeView->GetCellValue(mRow, mColumn, aName);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetBounds(PRInt32 *aX, PRInt32 *aY,
                                       PRInt32 *aWidth, PRInt32 *aHeight)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Get bounds for tree cell and add x and y of treechildren element to
  // x and y of the cell.
  nsCOMPtr<nsIBoxObject> boxObj = nsCoreUtils::GetTreeBodyBoxObject(mTree);
  NS_ENSURE_STATE(boxObj);

  PRInt32 x = 0, y = 0, width = 0, height = 0;
  nsresult rv = mTree->GetCoordsForCellItem(mRow, mColumn,
                                            NS_LITERAL_CSTRING("cell"),
                                            &x, &y, &width, &height);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 tcX = 0, tcY = 0;
  boxObj->GetScreenX(&tcX);
  boxObj->GetScreenY(&tcY);
  x += tcX;
  y += tcY;

  nsPresContext *presContext = GetPresContext();
  *aX = presContext->CSSPixelsToDevPixels(x);
  *aY = presContext->CSSPixelsToDevPixels(y);
  *aWidth = presContext->CSSPixelsToDevPixels(width);
  *aHeight = presContext->CSSPixelsToDevPixels(height);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetNumActions(PRUint8 *aActionsCount)
{
  NS_ENSURE_ARG_POINTER(aActionsCount);
  *aActionsCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRBool isCycler = PR_FALSE;
  mColumn->GetCycler(&isCycler);
  if (isCycler) {
    *aActionsCount = 1;
    return NS_OK;
  }

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX && IsEditable())
    *aActionsCount = 1;

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRBool isCycler = PR_FALSE;
  mColumn->GetCycler(&isCycler);
  if (isCycler) {
    aName.AssignLiteral("cycle");
    return NS_OK;
  }

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX && IsEditable()) {
    nsAutoString value;
    mTreeView->GetCellValue(mRow, mColumn, value);
    if (value.EqualsLiteral("true"))
      aName.AssignLiteral("uncheck");
    else
      aName.AssignLiteral("check");
    
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRBool isCycler = PR_FALSE;
  mColumn->GetCycler(&isCycler);
  if (isCycler)
    return DoCommand();

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX && IsEditable())
    return DoCommand();

  return NS_ERROR_INVALID_ARG;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsAccessNode implementation

PRBool
nsXULTreeGridCellAccessible::IsDefunct()
{
  return nsLeafAccessible::IsDefunct() || !mParent || !mTree || !mTreeView ||
    !mColumn;
}

nsresult
nsXULTreeGridCellAccessible::Init()
{
  nsresult rv = nsLeafAccessible::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX)
    mTreeView->GetCellValue(mRow, mColumn, mCachedTextEquiv);
  else
    mTreeView->GetCellText(mRow, mColumn, mCachedTextEquiv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsAccessible implementation

nsresult
nsXULTreeGridCellAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // "table-cell-index" attribute
  nsCOMPtr<nsIAccessible> accessible;
  mParent->GetParent(getter_AddRefs(accessible));
  nsCOMPtr<nsIAccessibleTable> tableAccessible = do_QueryInterface(accessible);

  PRInt32 colIdx = GetColumnIndex();

  PRInt32 cellIdx = -1;
  tableAccessible->GetIndexAt(mRow, colIdx, &cellIdx);

  nsAutoString stringIdx;
  stringIdx.AppendInt(cellIdx);
  nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::tableCellIndex,
                         stringIdx);

  // "cycles" attribute
  PRBool isCycler = PR_FALSE;
  nsresult rv = mColumn->GetCycler(&isCycler);
  if (NS_SUCCEEDED(rv) && isCycler)
    nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::cycles,
                           NS_LITERAL_STRING("true"));

  return NS_OK;
}

nsresult
nsXULTreeGridCellAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_GRID_CELL;
  return NS_OK;
}

nsresult
nsXULTreeGridCellAccessible::GetStateInternal(PRUint32 *aStates,
                                              PRUint32 *aExtraStates)
{
  NS_ENSURE_ARG_POINTER(aStates);

  *aStates = 0;
  if (aExtraStates)
    *aExtraStates = 0;

  if (IsDefunct()) {
    if (aExtraStates)
      *aExtraStates = nsIAccessibleStates::EXT_STATE_DEFUNCT;
    return NS_OK_DEFUNCT_OBJECT;
  }

  // selectable/selected state
  *aStates |= nsIAccessibleStates::STATE_SELECTABLE;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    PRBool isSelected = PR_FALSE;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected)
      *aStates |= nsIAccessibleStates::STATE_SELECTED;
  }

  // checked state
  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX) {
    *aStates |= nsIAccessibleStates::STATE_CHECKABLE;
    nsAutoString checked;
    mTreeView->GetCellValue(mRow, mColumn, checked);
    if (checked.EqualsIgnoreCase("true"))
      *aStates |= nsIAccessibleStates::STATE_CHECKED;
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: public implementation

PRInt32
nsXULTreeGridCellAccessible::GetColumnIndex() const
{
  PRInt32 index = 0;
  nsCOMPtr<nsITreeColumn> column = mColumn;
  while (column = nsCoreUtils::GetPreviousSensibleColumn(column))
    index++;

  return index;
}

void
nsXULTreeGridCellAccessible::CellInvalidated()
{
  nsAutoString textEquiv;

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX) {
    mTreeView->GetCellValue(mRow, mColumn, textEquiv);
    if (mCachedTextEquiv != textEquiv) {
      PRBool isEnabled = textEquiv.EqualsLiteral("true");
      nsCOMPtr<nsIAccessibleStateChangeEvent> accEvent =
        new nsAccStateChangeEvent(this, nsIAccessibleStates::STATE_CHECKED,
                                  PR_FALSE, isEnabled);
      if (accEvent)
        FireAccessibleEvent(accEvent);

      mCachedTextEquiv = textEquiv;
    }

    return;
  }

  mTreeView->GetCellText(mRow, mColumn, textEquiv);
  if (mCachedTextEquiv != textEquiv) {
    nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
    mCachedTextEquiv = textEquiv;
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: protected implementation

void
nsXULTreeGridCellAccessible::DispatchClickEvent(nsIContent *aContent,
                                                PRUint32 aActionIndex)
{
  if (IsDefunct())
    return;

  nsCoreUtils::DispatchClickEvent(mTree, mRow, mColumn);
}

PRBool
nsXULTreeGridCellAccessible::IsEditable() const
{
  // XXX: logic corresponds to tree.xml, it's preferable to have interface
  // method to check it.
  PRBool isEditable = PR_FALSE;
  nsresult rv = mTreeView->IsEditable(mRow, mColumn, &isEditable);
  if (NS_FAILED(rv) || !isEditable)
    return PR_FALSE;

  nsCOMPtr<nsIDOMElement> columnElm;
  mColumn->GetElement(getter_AddRefs(columnElm));
  if (!columnElm)
    return PR_FALSE;

  nsCOMPtr<nsIContent> columnContent(do_QueryInterface(columnElm));
  if (!columnContent->AttrValueIs(kNameSpaceID_None,
                                  nsAccessibilityAtoms::editable,
                                  nsAccessibilityAtoms::_true,
                                  eCaseMatters))
    return PR_FALSE;

  nsCOMPtr<nsIContent> treeContent(do_QueryInterface(mDOMNode));
  return treeContent->AttrValueIs(kNameSpaceID_None,
                                  nsAccessibilityAtoms::editable,
                                  nsAccessibilityAtoms::_true,
                                  eCaseMatters);
}
