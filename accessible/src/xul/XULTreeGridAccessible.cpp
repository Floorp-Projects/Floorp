/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULTreeGridAccessibleWrap.h"

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
// XULTreeGridAccessible
////////////////////////////////////////////////////////////////////////////////

XULTreeGridAccessible::
  XULTreeGridAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULTreeAccessible(aContent, aDoc), xpcAccessibleTable(this)
{
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED1(XULTreeGridAccessible,
                             XULTreeAccessible,
                             nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: nsIAccessibleTable implementation

PRUint32
XULTreeGridAccessible::ColCount()
{
  return nsCoreUtils::GetSensibleColumnCount(mTree);
}

PRUint32
XULTreeGridAccessible::RowCount()
{
  if (!mTreeView)
    return 0;

  PRInt32 rowCount = 0;
  mTreeView->GetRowCount(&rowCount);
  return rowCount >= 0 ? rowCount : 0;
}

PRUint32
XULTreeGridAccessible::SelectedCellCount()
{
  return SelectedRowCount() * ColCount();
}

PRUint32
XULTreeGridAccessible::SelectedColCount()
{
  // If all the row has been selected, then all the columns are selected,
  // because we can't select a column alone.

  PRInt32 selectedRowCount = 0;
  nsresult rv = GetSelectionCount(&selectedRowCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedRowCount > 0 && selectedRowCount == RowCount() ? ColCount() : 0;
}

PRUint32
XULTreeGridAccessible::SelectedRowCount()
{
  PRInt32 selectedRowCount = 0;
  nsresult rv = GetSelectionCount(&selectedRowCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedRowCount >= 0 ? selectedRowCount : 0;
}

NS_IMETHODIMP
XULTreeGridAccessible::GetSelectedCells(nsIArray** aCells)
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

void
XULTreeGridAccessible::SelectedCellIndices(nsTArray<PRUint32>* aCells)
{
  PRUint32 colCount = ColCount(), rowCount = RowCount();

  for (PRUint32 rowIdx = 0; rowIdx < rowCount; rowIdx++)
    if (IsRowSelected(rowIdx))
      for (PRUint32 colIdx = 0; colIdx < colCount; colIdx++)
        aCells->AppendElement(rowIdx * colCount + colIdx);
}

void
XULTreeGridAccessible::SelectedColIndices(nsTArray<PRUint32>* aCols)
{
  if (RowCount() != SelectedRowCount())
    return;

  PRUint32 colCount = ColCount();
  aCols->SetCapacity(colCount);
  for (PRUint32 colIdx = 0; colIdx < colCount; colIdx++)
    aCols->AppendElement(colIdx);
}

void
XULTreeGridAccessible::SelectedRowIndices(nsTArray<PRUint32>* aRows)
{
  PRUint32 rowCount = RowCount();
  for (PRUint32 rowIdx = 0; rowIdx < rowCount; rowIdx++)
    if (IsRowSelected(rowIdx))
      aRows->AppendElement(rowIdx);
}

Accessible*
XULTreeGridAccessible::CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex)
{ 
  Accessible* row = GetTreeItemAccessible(aRowIndex);
  if (!row)
    return nsnull;

  nsCOMPtr<nsITreeColumn> column =
    nsCoreUtils::GetSensibleColumnAt(mTree, aColumnIndex);
  if (!column)
    return nsnull;

  nsRefPtr<XULTreeItemAccessibleBase> rowAcc = do_QueryObject(row);
  if (!rowAcc)
    return nsnull;

  return rowAcc->GetCellAccessible(column);
}

void
XULTreeGridAccessible::ColDescription(PRUint32 aColIdx, nsString& aDescription)
{
  aDescription.Truncate();

  nsCOMPtr<nsIAccessible> treeColumns;
  Accessible::GetFirstChild(getter_AddRefs(treeColumns));
  if (treeColumns) {
    nsCOMPtr<nsIAccessible> treeColumnItem;
    treeColumns->GetChildAt(aColIdx, getter_AddRefs(treeColumnItem));
    if (treeColumnItem)
      treeColumnItem->GetName(aDescription);
  }
}

bool
XULTreeGridAccessible::IsColSelected(PRUint32 aColIdx)
{
  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.

  PRInt32 selectedrowCount = 0;
  nsresult rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, false);

  return selectedrowCount == RowCount();
}

bool
XULTreeGridAccessible::IsRowSelected(PRUint32 aRowIdx)
{
  if (!mTreeView)
    return false;

  nsCOMPtr<nsITreeSelection> selection;
  nsresult rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, false);

  bool isSelected = false;
  selection->IsSelected(aRowIdx, &isSelected);
  return isSelected;
}

bool
XULTreeGridAccessible::IsCellSelected(PRUint32 aRowIdx, PRUint32 aColIdx)
{
  return IsRowSelected(aRowIdx);
}

void
XULTreeGridAccessible::SelectRow(PRUint32 aRowIdx)
{
  if (!mTreeView)
    return;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ASSERTION(selection, "GetSelection() Shouldn't fail!");

  selection->Select(aRowIdx);
}

void
XULTreeGridAccessible::UnselectRow(PRUint32 aRowIdx)
{
  if (!mTreeView)
    return;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));

  if (selection)
    selection->ClearRange(aRowIdx, aRowIdx);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: nsAccessNode implementation

void
XULTreeGridAccessible::Shutdown()
{
  mTable = nsnull;
  XULTreeAccessible::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: Accessible implementation

role
XULTreeGridAccessible::NativeRole()
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
// XULTreeGridAccessible: XULTreeAccessible implementation

already_AddRefed<Accessible>
XULTreeGridAccessible::CreateTreeItemAccessible(PRInt32 aRow)
{
  nsRefPtr<Accessible> accessible =
    new XULTreeGridRowAccessible(mContent, mDoc, this, mTree, mTreeView, aRow);

  return accessible.forget();
}


////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible
////////////////////////////////////////////////////////////////////////////////

XULTreeGridRowAccessible::
  XULTreeGridRowAccessible(nsIContent* aContent, DocAccessible* aDoc,
                           Accessible* aTreeAcc, nsITreeBoxObject* aTree,
                           nsITreeView* aTreeView, PRInt32 aRow) :
  XULTreeItemAccessibleBase(aContent, aDoc, aTreeAcc, aTree, aTreeView, aRow)
{
  mAccessibleCache.Init(kDefaultTreeCacheSize);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: nsISupports and cycle collection implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(XULTreeGridRowAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XULTreeGridRowAccessible,
                                                  XULTreeItemAccessibleBase)
CycleCollectorTraverseCache(tmp->mAccessibleCache, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XULTreeGridRowAccessible,
                                                XULTreeItemAccessibleBase)
ClearCache(tmp->mAccessibleCache);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(XULTreeGridRowAccessible)
NS_INTERFACE_MAP_STATIC_AMBIGUOUS(XULTreeGridRowAccessible)
NS_INTERFACE_MAP_END_INHERITING(XULTreeItemAccessibleBase)

NS_IMPL_ADDREF_INHERITED(XULTreeGridRowAccessible,
                         XULTreeItemAccessibleBase)
NS_IMPL_RELEASE_INHERITED(XULTreeGridRowAccessible,
                          XULTreeItemAccessibleBase)

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: nsAccessNode implementation

void
XULTreeGridRowAccessible::Shutdown()
{
  ClearCache(mAccessibleCache);
  XULTreeItemAccessibleBase::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: Accessible implementation

role
XULTreeGridRowAccessible::NativeRole()
{
  return roles::ROW;
}

ENameValueFlag
XULTreeGridRowAccessible::Name(nsString& aName)
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

Accessible*
XULTreeGridRowAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
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

Accessible*
XULTreeGridRowAccessible::GetChildAt(PRUint32 aIndex)
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
XULTreeGridRowAccessible::ChildCount() const
{
  return nsCoreUtils::GetSensibleColumnCount(mTree);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: XULTreeItemAccessibleBase implementation

Accessible*
XULTreeGridRowAccessible::GetCellAccessible(nsITreeColumn* aColumn)
{
  NS_PRECONDITION(aColumn, "No tree column!");

  void* key = static_cast<void*>(aColumn);
  Accessible* cachedCell = mAccessibleCache.GetWeak(key);
  if (cachedCell)
    return cachedCell;

  nsRefPtr<Accessible> cell =
    new XULTreeGridCellAccessibleWrap(mContent, mDoc, this, mTree,
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
XULTreeGridRowAccessible::RowInvalidated(PRInt32 aStartColIdx,
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
      Accessible* cellAccessible = GetCellAccessible(column);
      if (cellAccessible) {
        nsRefPtr<XULTreeGridCellAccessible> cellAcc = do_QueryObject(cellAccessible);

        cellAcc->CellInvalidated();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: Accessible protected implementation

void
XULTreeGridRowAccessible::CacheChildren()
{
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible
////////////////////////////////////////////////////////////////////////////////

XULTreeGridCellAccessible::
  XULTreeGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc,
                            XULTreeGridRowAccessible* aRowAcc,
                            nsITreeBoxObject* aTree, nsITreeView* aTreeView,
                            PRInt32 aRow, nsITreeColumn* aColumn) :
  LeafAccessible(aContent, aDoc), mTree(aTree),
  mTreeView(aTreeView), mRow(aRow), mColumn(aColumn)
{
  mParent = aRowAcc;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: nsISupports implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(XULTreeGridCellAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XULTreeGridCellAccessible,
                                                  LeafAccessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTree)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mColumn)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XULTreeGridCellAccessible,
                                                LeafAccessible)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTree)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mColumn)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(XULTreeGridCellAccessible)
  NS_INTERFACE_TABLE_INHERITED2(XULTreeGridCellAccessible,
                                nsIAccessibleTableCell,
                                XULTreeGridCellAccessible)
NS_INTERFACE_TABLE_TAIL_INHERITING(LeafAccessible)
NS_IMPL_ADDREF_INHERITED(XULTreeGridCellAccessible, LeafAccessible)
NS_IMPL_RELEASE_INHERITED(XULTreeGridCellAccessible, LeafAccessible)

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: nsIAccessible implementation

Accessible*
XULTreeGridCellAccessible::FocusedChild()
{
  return nsnull;
}

ENameValueFlag
XULTreeGridCellAccessible::Name(nsString& aName)
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
XULTreeGridCellAccessible::GetBounds(PRInt32* aX, PRInt32* aY,
                                     PRInt32* aWidth, PRInt32* aHeight)
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
XULTreeGridCellAccessible::ActionCount()
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
XULTreeGridCellAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
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
XULTreeGridCellAccessible::DoAction(PRUint8 aIndex)
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
// XULTreeGridCellAccessible: nsIAccessibleTableCell implementation

NS_IMETHODIMP
XULTreeGridCellAccessible::GetTable(nsIAccessibleTable** aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  if (IsDefunct())
    return NS_OK;

  Accessible* grandParent = mParent->Parent();
  if (grandParent)
    CallQueryInterface(grandParent, aTable);

  return NS_OK;
}

NS_IMETHODIMP
XULTreeGridCellAccessible::GetColumnIndex(PRInt32* aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aColumnIndex = GetColumnIndex();
  return NS_OK;
}

NS_IMETHODIMP
XULTreeGridCellAccessible::GetRowIndex(PRInt32* aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aRowIndex = mRow;
  return NS_OK;
}

NS_IMETHODIMP
XULTreeGridCellAccessible::GetColumnExtent(PRInt32* aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 1;

  return NS_OK;
}

NS_IMETHODIMP
XULTreeGridCellAccessible::GetRowExtent(PRInt32* aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 1;

  return NS_OK;
}

NS_IMETHODIMP
XULTreeGridCellAccessible::GetColumnHeaderCells(nsIArray** aHeaderCells)
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
  Accessible* headerCell = mDoc->GetAccessible(columnContent);

  if (headerCell)
    headerCells->AppendElement(static_cast<nsIAccessible*>(headerCell),
                               false);

  NS_ADDREF(*aHeaderCells = headerCells);
  return NS_OK;
}

NS_IMETHODIMP
XULTreeGridCellAccessible::GetRowHeaderCells(nsIArray** aHeaderCells)
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
XULTreeGridCellAccessible::IsSelected(bool* aIsSelected)
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
// XULTreeGridCellAccessible: nsAccessNode implementation

bool
XULTreeGridCellAccessible::Init()
{
  if (!LeafAccessible::Init() || !mTreeView)
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
XULTreeGridCellAccessible::IsPrimaryForNode() const
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: Accessible public implementation

nsresult
XULTreeGridCellAccessible::GetAttributesInternal(nsIPersistentProperties* aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // "table-cell-index" attribute
  Accessible* grandParent = mParent->Parent();
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
XULTreeGridCellAccessible::NativeRole()
{
  return roles::GRID_CELL;
}

PRUint64
XULTreeGridCellAccessible::NativeState()
{
  if (!mTreeView)
    return states::DEFUNCT;

  // selectable/selected state
  PRUint64 states = states::SELECTABLE; // keep in sync with NativeInteractiveState

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

PRUint64
XULTreeGridCellAccessible::NativeInteractiveState() const
{
  return states::SELECTABLE;
}

PRInt32
XULTreeGridCellAccessible::IndexInParent() const
{
  return GetColumnIndex();
}

Relation
XULTreeGridCellAccessible::RelationByType(PRUint32 aType)
{
  return Relation();
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: public implementation

PRInt32
XULTreeGridCellAccessible::GetColumnIndex() const
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
XULTreeGridCellAccessible::CellInvalidated()
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
// XULTreeGridCellAccessible: Accessible protected implementation

Accessible*
XULTreeGridCellAccessible::GetSiblingAtOffset(PRInt32 aOffset,
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

  nsRefPtr<XULTreeItemAccessibleBase> rowAcc = do_QueryObject(Parent());
  return rowAcc->GetCellAccessible(columnAtOffset);
}

void
XULTreeGridCellAccessible::DispatchClickEvent(nsIContent* aContent,
                                              PRUint32 aActionIndex)
{
  if (IsDefunct())
    return;

  nsCoreUtils::DispatchClickEvent(mTree, mRow, mColumn);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: protected implementation

bool
XULTreeGridCellAccessible::IsEditable() const
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
