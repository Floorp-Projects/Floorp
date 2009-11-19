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

#include "nsXULTreeGridAccessibleWrap.h"

#include "nsITreeSelection.h"
#include "nsServiceManagerUtils.h"

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
nsXULTreeGridAccessible::GetColumnCount(PRInt32 *acolumnCount)
{
  NS_ENSURE_ARG_POINTER(acolumnCount);
  *acolumnCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *acolumnCount = nsCoreUtils::GetSensiblecolumnCount(mTree);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetRowCount(PRInt32 *arowCount)
{
  NS_ENSURE_ARG_POINTER(arowCount);
  *arowCount = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  return mTreeView->GetRowCount(arowCount);
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedCellCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  PRUint32 selectedrowCount = 0;
  nsresult rv = GetSelectedRowCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedrowCount * columnCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedColumnCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // If all the row has been selected, then all the columns are selected,
  // because we can't select a column alone.

  PRInt32 rowCount = 0;
  nsresult rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedrowCount = 0;
  rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rowCount == selectedrowCount) {
    PRInt32 columnCount = 0;
    rv = GetColumnCount(&columnCount);
    NS_ENSURE_SUCCESS(rv, rv);

    *aCount = columnCount;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedRowCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 selectedrowCount = 0;
  nsresult rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = selectedrowCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedCells(nsIArray **aCells)
{
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  nsCOMPtr<nsIMutableArray> selCells = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(selCells, NS_ERROR_FAILURE);

  PRInt32 selectedrowCount = 0;
  nsresult rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITreeSelection> selection;
  rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount = 0;
  rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelected;
  for (PRInt32 rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    selection->IsSelected(rowIdx, &isSelected);
    if (isSelected) {
      for (PRInt32 colIdx = 0; colIdx < columnCount; colIdx++) {
        nsCOMPtr<nsIAccessible> cell;
        GetCellAt(rowIdx, colIdx, getter_AddRefs(cell));
        selCells->AppendElement(cell, PR_FALSE);
      }
    }
  }

  NS_ADDREF(*aCells = selCells);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedCellIndices(PRUint32 *aCellsCount,
                                                PRInt32 **aCells)
{
  NS_ENSURE_ARG_POINTER(aCellsCount);
  *aCellsCount = 0;
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  PRInt32 selectedrowCount = 0;
  nsresult rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedCellCount = selectedrowCount * columnCount;
  PRInt32* outArray = static_cast<PRInt32*>(
    nsMemory::Alloc(selectedCellCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsITreeSelection> selection;
  rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount = 0;
  rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelected;
  for (PRInt32 rowIdx = 0, arrayIdx = 0; rowIdx < rowCount; rowIdx++) {
    selection->IsSelected(rowIdx, &isSelected);
    if (isSelected) {
      for (PRInt32 colIdx = 0; colIdx < columnCount; colIdx++)
        outArray[arrayIdx++] = rowIdx * columnCount + colIdx;
    }
  }

  *aCellsCount = selectedCellCount;
  *aCells = outArray;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedColumnIndices(PRUint32 *acolumnCount,
                                                  PRInt32 **aColumns)
{
  NS_ENSURE_ARG_POINTER(acolumnCount);
  *acolumnCount = 0;
  NS_ENSURE_ARG_POINTER(aColumns);
  *aColumns = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.

  PRInt32 rowCount = 0;
  nsresult rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedrowCount = 0;
  rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rowCount != selectedrowCount)
    return NS_OK;

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32* outArray = static_cast<PRInt32*>(
    nsMemory::Alloc(columnCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  for (PRInt32 colIdx = 0; colIdx < columnCount; colIdx++)
    outArray[colIdx] = colIdx;

  *acolumnCount = columnCount;
  *aColumns = outArray;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetSelectedRowIndices(PRUint32 *arowCount,
                                               PRInt32 **aRows)
{
  NS_ENSURE_ARG_POINTER(arowCount);
  *arowCount = 0;
  NS_ENSURE_ARG_POINTER(aRows);
  *aRows = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 selectedrowCount = 0;
  nsresult rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32* outArray = static_cast<PRInt32*>(
    nsMemory::Alloc(selectedrowCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsITreeSelection> selection;
  rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount = 0;
  rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelected;
  for (PRInt32 rowIdx = 0, arrayIdx = 0; rowIdx < rowCount; rowIdx++) {
    selection->IsSelected(rowIdx, &isSelected);
    if (isSelected)
      outArray[arrayIdx++] = rowIdx;
  }

  *arowCount = selectedrowCount;
  *aRows = outArray;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetCellAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
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
nsXULTreeGridAccessible::GetCellIndexAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                        PRInt32 *aCellIndex)
{
  NS_ENSURE_ARG_POINTER(aCellIndex);
  *aCellIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 columnCount = 0;
  nsresult rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCellIndex = aRowIndex * columnCount + aColumnIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetColumnIndexAt(PRInt32 aCellIndex,
                                          PRInt32 *aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  PRInt32 columnCount = 0;
  nsresult rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aColumnIndex = aCellIndex % columnCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetRowIndexAt(PRInt32 aCellIndex, PRInt32 *aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  PRInt32 columnCount;
  nsresult rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aRowIndex = aCellIndex / columnCount;
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

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> treeColumns;
  nsAccessible::GetFirstChild(getter_AddRefs(treeColumns));
  if (treeColumns) {
    nsCOMPtr<nsIAccessible> treeColumnItem;
    treeColumns->GetChildAt(aColumnIndex, getter_AddRefs(treeColumnItem));
    if (treeColumnItem)
      return treeColumnItem->GetName(aDescription);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridAccessible::GetRowDescription(PRInt32 aRowIndex,
                                           nsAString& aDescription)
{
  aDescription.Truncate();
  return NS_OK;
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
  
  PRInt32 rowCount = 0;
  nsresult rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedrowCount = 0;
  rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aIsSelected = rowCount == selectedrowCount;
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

  *aChildCount = nsCoreUtils::GetSensiblecolumnCount(mTree);
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
      new nsXULTreeGridCellAccessibleWrap(mDOMNode, mWeakShell, this, mTree,
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

NS_IMPL_ISUPPORTS_INHERITED2(nsXULTreeGridCellAccessible,
                             nsLeafAccessible,
                             nsIAccessibleTableCell,
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
// nsXULTreeGridCellAccessible: nsIAccessibleTableCell implementation

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetTable(nsIAccessibleTable **aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  if (IsDefunct())
    return NS_OK;

  nsCOMPtr<nsIAccessible> accessible;
  mParent->GetParent(getter_AddRefs(accessible));
  CallQueryInterface(accessible, aTable);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetColumnIndex(PRInt32 *aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aColumnIndex = GetColumnIndex();
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetRowIndex(PRInt32 *aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aRowIndex = mRow;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetColumnExtent(PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 1;

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetRowExtent(PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 1;

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetColumnHeaderCells(nsIArray **aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> headerCells =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> columnElm;
  mColumn->GetElement(getter_AddRefs(columnElm));

  nsCOMPtr<nsIAccessible> headerCell;
  GetAccService()->GetAccessibleInWeakShell(columnElm, mWeakShell,
                                            getter_AddRefs(headerCell));

  if (headerCell)
    headerCells->AppendElement(headerCell, PR_FALSE);

  NS_ADDREF(*aHeaderCells = headerCells);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetRowHeaderCells(nsIArray **aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> headerCells =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aHeaderCells = headerCells);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::IsSelected(PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeSelection> selection;
  nsresult rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  return selection->IsSelected(mRow, aIsSelected);
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

  // XXX - temp fix for crash bug 516047
  if (!tableAccessible)
    return NS_ERROR_FAILURE;
    
  PRInt32 colIdx = GetColumnIndex();

  PRInt32 cellIdx = -1;
  tableAccessible->GetCellIndexAt(mRow, colIdx, &cellIdx);

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
      nsCOMPtr<nsIAccessibleEvent> accEvent =
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
