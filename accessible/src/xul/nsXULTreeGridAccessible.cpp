/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTreeGridAccessibleWrap.h"

#include "nsAccCache.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "nsEventShell.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

#include "nsITreeSelection.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTreeGridAccessible::
  nsXULTreeGridAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsXULTreeAccessible(aContent, aDoc), xpcAccessibleTable(this)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeGridAccessible,
                             nsXULTreeAccessible,
                             nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsIAccessibleTable implementation

PRUint32
nsXULTreeGridAccessible::ColCount()
{
  return nsCoreUtils::GetSensibleColumnCount(mTree);
}

PRUint32
nsXULTreeGridAccessible::RowCount()
{
  if (!mTreeView)
    return 0;

  PRInt32 rowCount = 0;
  mTreeView->GetRowCount(&rowCount);
  return rowCount >= 0 ? rowCount : 0;
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

  if (!mTreeView)
    return NS_OK;

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

  bool isSelected;
  for (PRInt32 rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    selection->IsSelected(rowIdx, &isSelected);
    if (isSelected) {
      for (PRInt32 colIdx = 0; colIdx < columnCount; colIdx++) {
        nsCOMPtr<nsIAccessible> cell;
        GetCellAt(rowIdx, colIdx, getter_AddRefs(cell));
        selCells->AppendElement(cell, false);
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

  if (!mTreeView)
    return NS_OK;

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

  bool isSelected;
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

  if (!mTreeView)
    return NS_OK;

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

  bool isSelected;
  for (PRInt32 rowIdx = 0, arrayIdx = 0; rowIdx < rowCount; rowIdx++) {
    selection->IsSelected(rowIdx, &isSelected);
    if (isSelected)
      outArray[arrayIdx++] = rowIdx;
  }

  *arowCount = selectedrowCount;
  *aRows = outArray;
  return NS_OK;
}

nsAccessible*
nsXULTreeGridAccessible::CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex)
{ 
  nsAccessible* row = GetTreeItemAccessible(aRowIndex);
  if (!row)
    return nsnull;

  nsCOMPtr<nsITreeColumn> column =
    nsCoreUtils::GetSensibleColumnAt(mTree, aColumnIndex);
  if (!column)
    return nsnull;

  nsRefPtr<nsXULTreeItemAccessibleBase> rowAcc = do_QueryObject(row);
  if (!rowAcc)
    return nsnull;

  return rowAcc->GetCellAccessible(column);
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
nsXULTreeGridAccessible::GetRowAndColumnIndicesAt(PRInt32 aCellIndex,
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
                                          bool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

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
nsXULTreeGridAccessible::IsRowSelected(PRInt32 aRowIndex, bool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (!mTreeView)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsITreeSelection> selection;
  nsresult rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);
  
  return selection->IsSelected(aRowIndex, aIsSelected);
}

NS_IMETHODIMP
nsXULTreeGridAccessible::IsCellSelected(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                        bool *aIsSelected)
{
  return IsRowSelected(aRowIndex, aIsSelected);
}

NS_IMETHODIMP
nsXULTreeGridAccessible::SelectRow(PRInt32 aRowIndex)
{
  if (!mTreeView)
    return NS_ERROR_INVALID_ARG;

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

void
nsXULTreeGridAccessible::UnselectRow(PRUint32 aRowIdx)
{
  if (!mTreeView)
    return;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  
  if (selection)
    selection->ClearRange(aRowIdx, aRowIdx);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsAccessNode implementation

void
nsXULTreeGridAccessible::Shutdown()
{
  mTable = nsnull;
  nsXULTreeAccessible::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsAccessible implementation

role
nsXULTreeGridAccessible::NativeRole()
{
  nsCOMPtr<nsITreeColumns> treeColumns;
  mTree->GetColumns(getter_AddRefs(treeColumns));
  if (!treeColumns) {
    NS_ERROR("No treecolumns object for tree!");
    return roles::NOTHING;
  }

  nsCOMPtr<nsITreeColumn> primaryColumn;
  treeColumns->GetPrimaryColumn(getter_AddRefs(primaryColumn));

  return primaryColumn ? roles::TREE_TABLE : roles::TABLE;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessible: nsXULTreeAccessible implementation

already_AddRefed<nsAccessible>
nsXULTreeGridAccessible::CreateTreeItemAccessible(PRInt32 aRow)
{
  nsRefPtr<nsAccessible> accessible =
    new nsXULTreeGridRowAccessible(mContent, mDoc, this, mTree,
                                   mTreeView, aRow);

  return accessible.forget();
}


////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTreeGridRowAccessible::
  nsXULTreeGridRowAccessible(nsIContent* aContent, DocAccessible* aDoc,
                             nsAccessible* aTreeAcc, nsITreeBoxObject* aTree,
                             nsITreeView* aTreeView, PRInt32 aRow) :
  nsXULTreeItemAccessibleBase(aContent, aDoc, aTreeAcc, aTree, aTreeView, aRow)
{
  mAccessibleCache.Init(kDefaultTreeCacheSize);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsISupports and cycle collection implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULTreeGridRowAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXULTreeGridRowAccessible,
                                                  nsXULTreeItemAccessibleBase)
CycleCollectorTraverseCache(tmp->mAccessibleCache, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXULTreeGridRowAccessible,
                                                nsXULTreeItemAccessibleBase)
ClearCache(tmp->mAccessibleCache);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXULTreeGridRowAccessible)
NS_INTERFACE_MAP_STATIC_AMBIGUOUS(nsXULTreeGridRowAccessible)
NS_INTERFACE_MAP_END_INHERITING(nsXULTreeItemAccessibleBase)

NS_IMPL_ADDREF_INHERITED(nsXULTreeGridRowAccessible,
                         nsXULTreeItemAccessibleBase)
NS_IMPL_RELEASE_INHERITED(nsXULTreeGridRowAccessible,
                          nsXULTreeItemAccessibleBase)

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsAccessNode implementation

void
nsXULTreeGridRowAccessible::Shutdown()
{
  ClearCache(mAccessibleCache);
  nsXULTreeItemAccessibleBase::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsAccessible implementation

role
nsXULTreeGridRowAccessible::NativeRole()
{
  return roles::ROW;
}

ENameValueFlag
nsXULTreeGridRowAccessible::Name(nsString& aName)
{
  aName.Truncate();

  // XXX: the row name sholdn't be a concatenation of cell names (bug 664384).
  nsCOMPtr<nsITreeColumn> column = nsCoreUtils::GetFirstSensibleColumn(mTree);
  while (column) {
    if (!aName.IsEmpty())
      aName.AppendLiteral(" ");

    nsAutoString cellName;
    GetCellName(column, cellName);
    aName.Append(cellName);

    column = nsCoreUtils::GetNextSensibleColumn(column);
  }

  return eNameOK;
}

nsAccessible*
nsXULTreeGridRowAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                         EWhichChildAtPoint aWhichChild)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return nsnull;

  nsPresContext *presContext = frame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();

  nsIFrame *rootFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, nsnull);

  nsIntRect rootRect = rootFrame->GetScreenRect();

  PRInt32 clientX = presContext->DevPixelsToIntCSSPixels(aX) - rootRect.x;
  PRInt32 clientY = presContext->DevPixelsToIntCSSPixels(aY) - rootRect.y;

  PRInt32 row = -1;
  nsCOMPtr<nsITreeColumn> column;
  nsCAutoString childEltUnused;
  mTree->GetCellAt(clientX, clientY, &row, getter_AddRefs(column),
                   childEltUnused);

  // Return if we failed to find tree cell in the row for the given point.
  if (row != mRow || !column)
    return nsnull;

  return GetCellAccessible(column);
}

nsAccessible*
nsXULTreeGridRowAccessible::GetChildAt(PRUint32 aIndex)
{
  if (IsDefunct())
    return nsnull;

  nsCOMPtr<nsITreeColumn> column =
    nsCoreUtils::GetSensibleColumnAt(mTree, aIndex);
  if (!column)
    return nsnull;

  return GetCellAccessible(column);
}

PRUint32
nsXULTreeGridRowAccessible::ChildCount() const
{
  return nsCoreUtils::GetSensibleColumnCount(mTree);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsXULTreeItemAccessibleBase implementation

nsAccessible*
nsXULTreeGridRowAccessible::GetCellAccessible(nsITreeColumn* aColumn)
{
  NS_PRECONDITION(aColumn, "No tree column!");

  void* key = static_cast<void*>(aColumn);
  nsAccessible* cachedCell = mAccessibleCache.GetWeak(key);
  if (cachedCell)
    return cachedCell;

  nsRefPtr<nsAccessible> cell =
    new nsXULTreeGridCellAccessibleWrap(mContent, mDoc, this, mTree,
                                        mTreeView, mRow, aColumn);
  if (cell) {
    mAccessibleCache.Put(key, cell);
    if (Document()->BindToDocument(cell, nsnull))
      return cell;

    mAccessibleCache.Remove(key);
  }

  return nsnull;
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
      nsAccessible *cellAccessible = GetCellAccessible(column);
      if (cellAccessible) {
        nsRefPtr<nsXULTreeGridCellAccessible> cellAcc = do_QueryObject(cellAccessible);

        cellAcc->CellInvalidated();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridRowAccessible: nsAccessible protected implementation

void
nsXULTreeGridRowAccessible::CacheChildren()
{
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible
////////////////////////////////////////////////////////////////////////////////

nsXULTreeGridCellAccessible::
  nsXULTreeGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc,
                              nsXULTreeGridRowAccessible* aRowAcc,
                              nsITreeBoxObject* aTree, nsITreeView* aTreeView,
                              PRInt32 aRow, nsITreeColumn* aColumn) :
  nsLeafAccessible(aContent, aDoc), mTree(aTree),
  mTreeView(aTreeView), mRow(aRow), mColumn(aColumn)
{
  mParent = aRowAcc;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsISupports implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULTreeGridCellAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXULTreeGridCellAccessible,
                                                  nsLeafAccessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTree)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mColumn)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXULTreeGridCellAccessible,
                                                nsLeafAccessible)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTree)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mColumn)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsXULTreeGridCellAccessible)
  NS_INTERFACE_TABLE_INHERITED2(nsXULTreeGridCellAccessible,
                                nsIAccessibleTableCell,
                                nsXULTreeGridCellAccessible)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsLeafAccessible)
NS_IMPL_ADDREF_INHERITED(nsXULTreeGridCellAccessible, nsLeafAccessible)
NS_IMPL_RELEASE_INHERITED(nsXULTreeGridCellAccessible, nsLeafAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsIAccessible implementation

nsAccessible*
nsXULTreeGridCellAccessible::FocusedChild()
{
  return nsnull;
}

ENameValueFlag
nsXULTreeGridCellAccessible::Name(nsString& aName)
{
  aName.Truncate();

  if (!mTreeView)
    return eNameOK;

  mTreeView->GetCellText(mRow, mColumn, aName);

  // If there is still no name try the cell value:
  // This is for graphical cells. We need tree/table view implementors to implement
  // FooView::GetCellValue to return a meaningful string for cases where there is
  // something shown in the cell (non-text) such as a star icon; in which case
  // GetCellValue for that cell would return "starred" or "flagged" for example.
  if (aName.IsEmpty())
    mTreeView->GetCellValue(mRow, mColumn, aName);

  return eNameOK;
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

  nsPresContext* presContext = mDoc->PresContext();
  *aX = presContext->CSSPixelsToDevPixels(x);
  *aY = presContext->CSSPixelsToDevPixels(y);
  *aWidth = presContext->CSSPixelsToDevPixels(width);
  *aHeight = presContext->CSSPixelsToDevPixels(height);

  return NS_OK;
}

PRUint8
nsXULTreeGridCellAccessible::ActionCount()
{
  bool isCycler = false;
  mColumn->GetCycler(&isCycler);
  if (isCycler)
    return 1;

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX && IsEditable())
    return 1;

  return 0;
}

NS_IMETHODIMP
nsXULTreeGridCellAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct() || !mTreeView)
    return NS_ERROR_FAILURE;

  bool isCycler = false;
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

  bool isCycler = false;
  mColumn->GetCycler(&isCycler);
  if (isCycler) {
    DoCommand();
    return NS_OK;
  }

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX && IsEditable()) {
    DoCommand();
    return NS_OK;
  }

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

  nsAccessible* grandParent = mParent->Parent();
  if (grandParent)
    CallQueryInterface(grandParent, aTable);

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

  if (IsDefunct() || !mDoc)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> headerCells =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> columnElm;
  mColumn->GetElement(getter_AddRefs(columnElm));

  nsCOMPtr<nsIContent> columnContent(do_QueryInterface(columnElm));
  nsAccessible *headerCell = mDoc->GetAccessible(columnContent);

  if (headerCell)
    headerCells->AppendElement(static_cast<nsIAccessible*>(headerCell),
                               false);

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
nsXULTreeGridCellAccessible::IsSelected(bool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct() || !mTreeView)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeSelection> selection;
  nsresult rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  return selection->IsSelected(mRow, aIsSelected);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsAccessNode implementation

bool
nsXULTreeGridCellAccessible::Init()
{
  if (!nsLeafAccessible::Init() || !mTreeView)
    return false;

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX)
    mTreeView->GetCellValue(mRow, mColumn, mCachedTextEquiv);
  else
    mTreeView->GetCellText(mRow, mColumn, mCachedTextEquiv);

  return true;
}

bool
nsXULTreeGridCellAccessible::IsPrimaryForNode() const
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsAccessible public implementation

nsresult
nsXULTreeGridCellAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // "table-cell-index" attribute
  nsAccessible* grandParent = mParent->Parent();
  if (!grandParent)
    return NS_OK;

  nsCOMPtr<nsIAccessibleTable> tableAccessible = do_QueryObject(grandParent);

  // XXX - temp fix for crash bug 516047
  if (!tableAccessible)
    return NS_ERROR_FAILURE;
    
  PRInt32 colIdx = GetColumnIndex();

  PRInt32 cellIdx = -1;
  tableAccessible->GetCellIndexAt(mRow, colIdx, &cellIdx);

  nsAutoString stringIdx;
  stringIdx.AppendInt(cellIdx);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::tableCellIndex,
                         stringIdx);

  // "cycles" attribute
  bool isCycler = false;
  nsresult rv = mColumn->GetCycler(&isCycler);
  if (NS_SUCCEEDED(rv) && isCycler)
    nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::cycles,
                           NS_LITERAL_STRING("true"));

  return NS_OK;
}

role
nsXULTreeGridCellAccessible::NativeRole()
{
  return roles::GRID_CELL;
}

PRUint64
nsXULTreeGridCellAccessible::NativeState()
{
  if (!mTreeView)
    return states::DEFUNCT;

  // selectable/selected state
  PRUint64 states = states::SELECTABLE;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool isSelected = false;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected)
      states |= states::SELECTED;
  }

  // checked state
  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX) {
    states |= states::CHECKABLE;
    nsAutoString checked;
    mTreeView->GetCellValue(mRow, mColumn, checked);
    if (checked.EqualsIgnoreCase("true"))
      states |= states::CHECKED;
  }

  return states;
}

PRInt32
nsXULTreeGridCellAccessible::IndexInParent() const
{
  return GetColumnIndex();
}

Relation
nsXULTreeGridCellAccessible::RelationByType(PRUint32 aType)
{
  return Relation();
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: public implementation

PRInt32
nsXULTreeGridCellAccessible::GetColumnIndex() const
{
  PRInt32 index = 0;
  nsCOMPtr<nsITreeColumn> column = mColumn;
  while (true) {
    column = nsCoreUtils::GetPreviousSensibleColumn(column);
    if (!column)
      break;
    index++;
  }

  return index;
}

void
nsXULTreeGridCellAccessible::CellInvalidated()
{
  if (!mTreeView)
    return;

  nsAutoString textEquiv;

  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX) {
    mTreeView->GetCellValue(mRow, mColumn, textEquiv);
    if (mCachedTextEquiv != textEquiv) {
      bool isEnabled = textEquiv.EqualsLiteral("true");
      nsRefPtr<AccEvent> accEvent =
        new AccStateChangeEvent(this, states::CHECKED, isEnabled);
      nsEventShell::FireEvent(accEvent);

      mCachedTextEquiv = textEquiv;
    }

    return;
  }

  mTreeView->GetCellText(mRow, mColumn, textEquiv);
  if (mCachedTextEquiv != textEquiv) {
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
    mCachedTextEquiv = textEquiv;
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: nsAccessible protected implementation

nsAccessible*
nsXULTreeGridCellAccessible::GetSiblingAtOffset(PRInt32 aOffset,
                                                nsresult* aError) const
{
  if (aError)
    *aError =  NS_OK; // fail peacefully

  nsCOMPtr<nsITreeColumn> columnAtOffset(mColumn), column;
  if (aOffset < 0) {
    for (PRInt32 index = aOffset; index < 0 && columnAtOffset; index++) {
      column = nsCoreUtils::GetPreviousSensibleColumn(columnAtOffset);
      column.swap(columnAtOffset);
    }
  } else {
    for (PRInt32 index = aOffset; index > 0 && columnAtOffset; index--) {
      column = nsCoreUtils::GetNextSensibleColumn(columnAtOffset);
      column.swap(columnAtOffset);
    }
  }

  if (!columnAtOffset)
    return nsnull;

  nsRefPtr<nsXULTreeItemAccessibleBase> rowAcc = do_QueryObject(Parent());
  return rowAcc->GetCellAccessible(columnAtOffset);
}

void
nsXULTreeGridCellAccessible::DispatchClickEvent(nsIContent *aContent,
                                                PRUint32 aActionIndex)
{
  if (IsDefunct())
    return;

  nsCoreUtils::DispatchClickEvent(mTree, mRow, mColumn);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessible: protected implementation

bool
nsXULTreeGridCellAccessible::IsEditable() const
{
  if (!mTreeView)
    return false;

  // XXX: logic corresponds to tree.xml, it's preferable to have interface
  // method to check it.
  bool isEditable = false;
  nsresult rv = mTreeView->IsEditable(mRow, mColumn, &isEditable);
  if (NS_FAILED(rv) || !isEditable)
    return false;

  nsCOMPtr<nsIDOMElement> columnElm;
  mColumn->GetElement(getter_AddRefs(columnElm));
  if (!columnElm)
    return false;

  nsCOMPtr<nsIContent> columnContent(do_QueryInterface(columnElm));
  if (!columnContent->AttrValueIs(kNameSpaceID_None,
                                  nsGkAtoms::editable,
                                  nsGkAtoms::_true,
                                  eCaseMatters))
    return false;

  return mContent->AttrValueIs(kNameSpaceID_None,
                               nsGkAtoms::editable,
                               nsGkAtoms::_true, eCaseMatters);
}
