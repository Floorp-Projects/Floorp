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

#include "nsIBoxObject.h"
#include "nsIMutableArray.h"
#include "nsIPersistentProperties2.h"
#include "nsITreeSelection.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED1(XULTreeGridAccessible,
                             XULTreeAccessible,
                             nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: nsIAccessibleTable implementation

uint32_t
XULTreeGridAccessible::ColCount()
{
  return nsCoreUtils::GetSensibleColumnCount(mTree);
}

uint32_t
XULTreeGridAccessible::RowCount()
{
  if (!mTreeView)
    return 0;

  int32_t rowCount = 0;
  mTreeView->GetRowCount(&rowCount);
  return rowCount >= 0 ? rowCount : 0;
}

uint32_t
XULTreeGridAccessible::SelectedCellCount()
{
  return SelectedRowCount() * ColCount();
}

uint32_t
XULTreeGridAccessible::SelectedColCount()
{
  // If all the row has been selected, then all the columns are selected,
  // because we can't select a column alone.

  int32_t selectedRowCount = 0;
  nsresult rv = GetSelectionCount(&selectedRowCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedRowCount > 0 && selectedRowCount == RowCount() ? ColCount() : 0;
}

uint32_t
XULTreeGridAccessible::SelectedRowCount()
{
  int32_t selectedRowCount = 0;
  nsresult rv = GetSelectionCount(&selectedRowCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedRowCount >= 0 ? selectedRowCount : 0;
}

void
XULTreeGridAccessible::SelectedCells(nsTArray<Accessible*>* aCells)
{
  uint32_t colCount = ColCount(), rowCount = RowCount();

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    if (IsRowSelected(rowIdx)) {
      for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
        Accessible* cell = CellAt(rowIdx, colIdx);
        aCells->AppendElement(cell);
      }
    }
  }
}

void
XULTreeGridAccessible::SelectedCellIndices(nsTArray<uint32_t>* aCells)
{
  uint32_t colCount = ColCount(), rowCount = RowCount();

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++)
    if (IsRowSelected(rowIdx))
      for (uint32_t colIdx = 0; colIdx < colCount; colIdx++)
        aCells->AppendElement(rowIdx * colCount + colIdx);
}

void
XULTreeGridAccessible::SelectedColIndices(nsTArray<uint32_t>* aCols)
{
  if (RowCount() != SelectedRowCount())
    return;

  uint32_t colCount = ColCount();
  aCols->SetCapacity(colCount);
  for (uint32_t colIdx = 0; colIdx < colCount; colIdx++)
    aCols->AppendElement(colIdx);
}

void
XULTreeGridAccessible::SelectedRowIndices(nsTArray<uint32_t>* aRows)
{
  uint32_t rowCount = RowCount();
  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++)
    if (IsRowSelected(rowIdx))
      aRows->AppendElement(rowIdx);
}

Accessible*
XULTreeGridAccessible::CellAt(uint32_t aRowIndex, uint32_t aColumnIndex)
{ 
  Accessible* row = GetTreeItemAccessible(aRowIndex);
  if (!row)
    return nullptr;

  nsCOMPtr<nsITreeColumn> column =
    nsCoreUtils::GetSensibleColumnAt(mTree, aColumnIndex);
  if (!column)
    return nullptr;

  nsRefPtr<XULTreeItemAccessibleBase> rowAcc = do_QueryObject(row);
  if (!rowAcc)
    return nullptr;

  return rowAcc->GetCellAccessible(column);
}

void
XULTreeGridAccessible::ColDescription(uint32_t aColIdx, nsString& aDescription)
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
XULTreeGridAccessible::IsColSelected(uint32_t aColIdx)
{
  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.

  int32_t selectedrowCount = 0;
  nsresult rv = GetSelectionCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, false);

  return selectedrowCount == RowCount();
}

bool
XULTreeGridAccessible::IsRowSelected(uint32_t aRowIdx)
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
XULTreeGridAccessible::IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx)
{
  return IsRowSelected(aRowIdx);
}

void
XULTreeGridAccessible::SelectRow(uint32_t aRowIdx)
{
  if (!mTreeView)
    return;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ASSERTION(selection, "GetSelection() Shouldn't fail!");

  selection->Select(aRowIdx);
}

void
XULTreeGridAccessible::UnselectRow(uint32_t aRowIdx)
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
  mTable = nullptr;
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
XULTreeGridAccessible::CreateTreeItemAccessible(int32_t aRow)
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
                           nsITreeView* aTreeView, int32_t aRow) :
  XULTreeItemAccessibleBase(aContent, aDoc, aTreeAcc, aTree, aTreeView, aRow),
  mAccessibleCache(kDefaultTreeCacheSize)
{
  mGenericTypes |= eTableRow;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: nsISupports and cycle collection implementation

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(XULTreeGridRowAccessible,
                                     XULTreeItemAccessibleBase,
                                     mAccessibleCache)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(XULTreeGridRowAccessible)
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
XULTreeGridRowAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                                       EWhichChildAtPoint aWhichChild)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return nullptr;

  nsPresContext *presContext = frame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();

  nsIFrame *rootFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, nullptr);

  nsIntRect rootRect = rootFrame->GetScreenRect();

  int32_t clientX = presContext->DevPixelsToIntCSSPixels(aX) - rootRect.x;
  int32_t clientY = presContext->DevPixelsToIntCSSPixels(aY) - rootRect.y;

  int32_t row = -1;
  nsCOMPtr<nsITreeColumn> column;
  nsAutoCString childEltUnused;
  mTree->GetCellAt(clientX, clientY, &row, getter_AddRefs(column),
                   childEltUnused);

  // Return if we failed to find tree cell in the row for the given point.
  if (row != mRow || !column)
    return nullptr;

  return GetCellAccessible(column);
}

Accessible*
XULTreeGridRowAccessible::GetChildAt(uint32_t aIndex)
{
  if (IsDefunct())
    return nullptr;

  nsCOMPtr<nsITreeColumn> column =
    nsCoreUtils::GetSensibleColumnAt(mTree, aIndex);
  if (!column)
    return nullptr;

  return GetCellAccessible(column);
}

uint32_t
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
    if (Document()->BindToDocument(cell, nullptr))
      return cell;

    mAccessibleCache.Remove(key);
  }

  return nullptr;
}

void
XULTreeGridRowAccessible::RowInvalidated(int32_t aStartColIdx,
                                         int32_t aEndColIdx)
{
  nsCOMPtr<nsITreeColumns> treeColumns;
  mTree->GetColumns(getter_AddRefs(treeColumns));
  if (!treeColumns)
    return;

  for (int32_t colIdx = aStartColIdx; colIdx <= aEndColIdx; ++colIdx) {
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
                            int32_t aRow, nsITreeColumn* aColumn) :
  LeafAccessible(aContent, aDoc), xpcAccessibleTableCell(this), mTree(aTree),
  mTreeView(aTreeView), mRow(aRow), mColumn(aColumn)
{
  mParent = aRowAcc;
  mStateFlags |= eSharedNode;
  mGenericTypes |= eTableCell;

  NS_ASSERTION(mTreeView, "mTreeView is null");

  int16_t type = -1;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX)
    mTreeView->GetCellValue(mRow, mColumn, mCachedTextEquiv);
  else
    mTreeView->GetCellText(mRow, mColumn, mCachedTextEquiv);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: nsISupports implementation

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(XULTreeGridCellAccessible, LeafAccessible,
                                     mTree, mColumn)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(XULTreeGridCellAccessible)
  NS_INTERFACE_TABLE_INHERITED2(XULTreeGridCellAccessible,
                                nsIAccessibleTableCell,
                                XULTreeGridCellAccessible)
NS_INTERFACE_TABLE_TAIL_INHERITING(LeafAccessible)
NS_IMPL_ADDREF_INHERITED(XULTreeGridCellAccessible, LeafAccessible)
NS_IMPL_RELEASE_INHERITED(XULTreeGridCellAccessible, LeafAccessible)

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: nsIAccessible implementation

void
XULTreeGridCellAccessible::Shutdown()
{
  mTableCell = nullptr;
  LeafAccessible::Shutdown();
}

Accessible*
XULTreeGridCellAccessible::FocusedChild()
{
  return nullptr;
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
XULTreeGridCellAccessible::GetBounds(int32_t* aX, int32_t* aY,
                                     int32_t* aWidth, int32_t* aHeight)
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

  int32_t x = 0, y = 0, width = 0, height = 0;
  nsresult rv = mTree->GetCoordsForCellItem(mRow, mColumn,
                                            NS_LITERAL_CSTRING("cell"),
                                            &x, &y, &width, &height);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t tcX = 0, tcY = 0;
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

uint8_t
XULTreeGridCellAccessible::ActionCount()
{
  bool isCycler = false;
  mColumn->GetCycler(&isCycler);
  if (isCycler)
    return 1;

  int16_t type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX && IsEditable())
    return 1;

  return 0;
}

NS_IMETHODIMP
XULTreeGridCellAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
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

  int16_t type;
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
XULTreeGridCellAccessible::DoAction(uint8_t aIndex)
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

  int16_t type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX && IsEditable()) {
    DoCommand();
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: nsIAccessibleTableCell implementation

TableAccessible*
XULTreeGridCellAccessible::Table() const
{
  Accessible* grandParent = mParent->Parent();
  if (grandParent)
    return grandParent->AsTable();

  return nullptr;
}

uint32_t
XULTreeGridCellAccessible::ColIdx() const
{
  uint32_t colIdx = 0;
  nsCOMPtr<nsITreeColumn> column = mColumn;
  while ((column = nsCoreUtils::GetPreviousSensibleColumn(column)))
    colIdx++;

  return colIdx;
}

uint32_t
XULTreeGridCellAccessible::RowIdx() const
{
  return mRow;
}

void
XULTreeGridCellAccessible::ColHeaderCells(nsTArray<Accessible*>* aHeaderCells)
{
  nsCOMPtr<nsIDOMElement> columnElm;
  mColumn->GetElement(getter_AddRefs(columnElm));

  nsCOMPtr<nsIContent> columnContent(do_QueryInterface(columnElm));
  Accessible* headerCell = mDoc->GetAccessible(columnContent);
  if (headerCell)
    aHeaderCells->AppendElement(headerCell);
}

bool
XULTreeGridCellAccessible::Selected()
{
  nsCOMPtr<nsITreeSelection> selection;
  nsresult rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, false);

  bool selected = false;
  selection->IsSelected(mRow, &selected);
  return selected;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: Accessible public implementation

already_AddRefed<nsIPersistentProperties>
XULTreeGridCellAccessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);

  // "table-cell-index" attribute
  TableAccessible* table = Table();
  if (!table)
    return attributes.forget();

  nsAutoString stringIdx;
  stringIdx.AppendInt(table->CellIndexAt(mRow, ColIdx()));
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::tableCellIndex, stringIdx);

  // "cycles" attribute
  bool isCycler = false;
  nsresult rv = mColumn->GetCycler(&isCycler);
  if (NS_SUCCEEDED(rv) && isCycler)
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::cycles,
                           NS_LITERAL_STRING("true"));

  return attributes.forget();
}

role
XULTreeGridCellAccessible::NativeRole()
{
  return roles::GRID_CELL;
}

uint64_t
XULTreeGridCellAccessible::NativeState()
{
  if (!mTreeView)
    return states::DEFUNCT;

  // selectable/selected state
  uint64_t states = states::SELECTABLE; // keep in sync with NativeInteractiveState

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool isSelected = false;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected)
      states |= states::SELECTED;
  }

  // checked state
  int16_t type;
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

uint64_t
XULTreeGridCellAccessible::NativeInteractiveState() const
{
  return states::SELECTABLE;
}

int32_t
XULTreeGridCellAccessible::IndexInParent() const
{
  return ColIdx();
}

Relation
XULTreeGridCellAccessible::RelationByType(uint32_t aType)
{
  return Relation();
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: public implementation

void
XULTreeGridCellAccessible::CellInvalidated()
{

  nsAutoString textEquiv;

  int16_t type;
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
XULTreeGridCellAccessible::GetSiblingAtOffset(int32_t aOffset,
                                              nsresult* aError) const
{
  if (aError)
    *aError =  NS_OK; // fail peacefully

  nsCOMPtr<nsITreeColumn> columnAtOffset(mColumn), column;
  if (aOffset < 0) {
    for (int32_t index = aOffset; index < 0 && columnAtOffset; index++) {
      column = nsCoreUtils::GetPreviousSensibleColumn(columnAtOffset);
      column.swap(columnAtOffset);
    }
  } else {
    for (int32_t index = aOffset; index > 0 && columnAtOffset; index--) {
      column = nsCoreUtils::GetNextSensibleColumn(columnAtOffset);
      column.swap(columnAtOffset);
    }
  }

  if (!columnAtOffset)
    return nullptr;

  nsRefPtr<XULTreeItemAccessibleBase> rowAcc = do_QueryObject(Parent());
  return rowAcc->GetCellAccessible(columnAtOffset);
}

void
XULTreeGridCellAccessible::DispatchClickEvent(nsIContent* aContent,
                                              uint32_t aActionIndex)
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
