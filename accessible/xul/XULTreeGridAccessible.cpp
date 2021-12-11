/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULTreeGridAccessibleWrap.h"

#include "AccAttributes.h"
#include "LocalAccessible-inl.h"
#include "nsAccCache.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "nsEventShell.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "nsQueryObject.h"
#include "nsTreeColumns.h"

#include "nsITreeSelection.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TreeColumnBinding.h"
#include "mozilla/dom/XULTreeElementBinding.h"

using namespace mozilla::a11y;
using namespace mozilla;

XULTreeGridAccessible::~XULTreeGridAccessible() {}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: Table

uint32_t XULTreeGridAccessible::ColCount() const {
  return nsCoreUtils::GetSensibleColumnCount(mTree);
}

uint32_t XULTreeGridAccessible::RowCount() {
  if (!mTreeView) return 0;

  int32_t rowCount = 0;
  mTreeView->GetRowCount(&rowCount);
  return rowCount >= 0 ? rowCount : 0;
}

uint32_t XULTreeGridAccessible::SelectedCellCount() {
  return SelectedRowCount() * ColCount();
}

uint32_t XULTreeGridAccessible::SelectedColCount() {
  // If all the row has been selected, then all the columns are selected,
  // because we can't select a column alone.

  uint32_t selectedRowCount = SelectedItemCount();
  return selectedRowCount > 0 && selectedRowCount == RowCount() ? ColCount()
                                                                : 0;
}

uint32_t XULTreeGridAccessible::SelectedRowCount() {
  return SelectedItemCount();
}

void XULTreeGridAccessible::SelectedCells(nsTArray<LocalAccessible*>* aCells) {
  uint32_t colCount = ColCount(), rowCount = RowCount();

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    if (IsRowSelected(rowIdx)) {
      for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
        LocalAccessible* cell = CellAt(rowIdx, colIdx);
        aCells->AppendElement(cell);
      }
    }
  }
}

void XULTreeGridAccessible::SelectedCellIndices(nsTArray<uint32_t>* aCells) {
  uint32_t colCount = ColCount(), rowCount = RowCount();

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    if (IsRowSelected(rowIdx)) {
      for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
        aCells->AppendElement(rowIdx * colCount + colIdx);
      }
    }
  }
}

void XULTreeGridAccessible::SelectedColIndices(nsTArray<uint32_t>* aCols) {
  if (RowCount() != SelectedRowCount()) return;

  uint32_t colCount = ColCount();
  aCols->SetCapacity(colCount);
  for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
    aCols->AppendElement(colIdx);
  }
}

void XULTreeGridAccessible::SelectedRowIndices(nsTArray<uint32_t>* aRows) {
  uint32_t rowCount = RowCount();
  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    if (IsRowSelected(rowIdx)) aRows->AppendElement(rowIdx);
  }
}

LocalAccessible* XULTreeGridAccessible::CellAt(uint32_t aRowIndex,
                                               uint32_t aColumnIndex) {
  LocalAccessible* row = GetTreeItemAccessible(aRowIndex);
  if (!row) return nullptr;

  RefPtr<nsTreeColumn> column =
      nsCoreUtils::GetSensibleColumnAt(mTree, aColumnIndex);
  if (!column) return nullptr;

  RefPtr<XULTreeItemAccessibleBase> rowAcc = do_QueryObject(row);
  if (!rowAcc) return nullptr;

  return rowAcc->GetCellAccessible(column);
}

void XULTreeGridAccessible::ColDescription(uint32_t aColIdx,
                                           nsString& aDescription) {
  aDescription.Truncate();

  LocalAccessible* treeColumns = LocalAccessible::LocalChildAt(0);
  if (treeColumns) {
    LocalAccessible* treeColumnItem = treeColumns->LocalChildAt(aColIdx);
    if (treeColumnItem) treeColumnItem->Name(aDescription);
  }
}

bool XULTreeGridAccessible::IsColSelected(uint32_t aColIdx) {
  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.
  return SelectedItemCount() == RowCount();
}

bool XULTreeGridAccessible::IsRowSelected(uint32_t aRowIdx) {
  if (!mTreeView) return false;

  nsCOMPtr<nsITreeSelection> selection;
  nsresult rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, false);

  bool isSelected = false;
  selection->IsSelected(aRowIdx, &isSelected);
  return isSelected;
}

bool XULTreeGridAccessible::IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx) {
  return IsRowSelected(aRowIdx);
}

void XULTreeGridAccessible::SelectRow(uint32_t aRowIdx) {
  if (!mTreeView) return;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ASSERTION(selection, "GetSelection() Shouldn't fail!");

  selection->Select(aRowIdx);
}

void XULTreeGridAccessible::UnselectRow(uint32_t aRowIdx) {
  if (!mTreeView) return;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));

  if (selection) selection->ClearRange(aRowIdx, aRowIdx);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: LocalAccessible implementation

role XULTreeGridAccessible::NativeRole() const {
  RefPtr<nsTreeColumns> treeColumns = mTree->GetColumns(FlushType::None);
  if (!treeColumns) {
    NS_ERROR("No treecolumns object for tree!");
    return roles::NOTHING;
  }

  nsTreeColumn* primaryColumn = treeColumns->GetPrimaryColumn();

  return primaryColumn ? roles::TREE_TABLE : roles::TABLE;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessible: XULTreeAccessible implementation

already_AddRefed<LocalAccessible>
XULTreeGridAccessible::CreateTreeItemAccessible(int32_t aRow) const {
  RefPtr<LocalAccessible> accessible = new XULTreeGridRowAccessible(
      mContent, mDoc, const_cast<XULTreeGridAccessible*>(this), mTree,
      mTreeView, aRow);

  return accessible.forget();
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible
////////////////////////////////////////////////////////////////////////////////

XULTreeGridRowAccessible::XULTreeGridRowAccessible(
    nsIContent* aContent, DocAccessible* aDoc, LocalAccessible* aTreeAcc,
    dom::XULTreeElement* aTree, nsITreeView* aTreeView, int32_t aRow)
    : XULTreeItemAccessibleBase(aContent, aDoc, aTreeAcc, aTree, aTreeView,
                                aRow),
      mAccessibleCache(kDefaultTreeCacheLength) {
  mGenericTypes |= eTableRow;
  mStateFlags |= eNoKidsFromDOM;
}

XULTreeGridRowAccessible::~XULTreeGridRowAccessible() {}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: nsISupports and cycle collection implementation

NS_IMPL_CYCLE_COLLECTION_INHERITED(XULTreeGridRowAccessible,
                                   XULTreeItemAccessibleBase, mAccessibleCache)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XULTreeGridRowAccessible)
NS_INTERFACE_MAP_END_INHERITING(XULTreeItemAccessibleBase)

NS_IMPL_ADDREF_INHERITED(XULTreeGridRowAccessible, XULTreeItemAccessibleBase)
NS_IMPL_RELEASE_INHERITED(XULTreeGridRowAccessible, XULTreeItemAccessibleBase)

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: LocalAccessible implementation

void XULTreeGridRowAccessible::Shutdown() {
  if (mDoc && !mDoc->IsDefunct()) {
    UnbindCacheEntriesFromDocument(mAccessibleCache);
  }

  XULTreeItemAccessibleBase::Shutdown();
}

role XULTreeGridRowAccessible::NativeRole() const { return roles::ROW; }

ENameValueFlag XULTreeGridRowAccessible::Name(nsString& aName) const {
  aName.Truncate();

  // XXX: the row name sholdn't be a concatenation of cell names (bug 664384).
  RefPtr<nsTreeColumn> column = nsCoreUtils::GetFirstSensibleColumn(mTree);
  while (column) {
    if (!aName.IsEmpty()) aName.Append(' ');

    nsAutoString cellName;
    GetCellName(column, cellName);
    aName.Append(cellName);

    column = nsCoreUtils::GetNextSensibleColumn(column);
  }

  return eNameOK;
}

LocalAccessible* XULTreeGridRowAccessible::LocalChildAtPoint(
    int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) {
  nsIFrame* frame = GetFrame();
  if (!frame) return nullptr;

  nsPresContext* presContext = frame->PresContext();
  PresShell* presShell = presContext->PresShell();

  nsIFrame* rootFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, nullptr);

  CSSIntRect rootRect = rootFrame->GetScreenRect();

  int32_t clientX = presContext->DevPixelsToIntCSSPixels(aX) - rootRect.X();
  int32_t clientY = presContext->DevPixelsToIntCSSPixels(aY) - rootRect.Y();

  ErrorResult rv;
  dom::TreeCellInfo cellInfo;
  mTree->GetCellAt(clientX, clientY, cellInfo, rv);

  // Return if we failed to find tree cell in the row for the given point.
  if (cellInfo.mRow != mRow || !cellInfo.mCol) return nullptr;

  return GetCellAccessible(cellInfo.mCol);
}

LocalAccessible* XULTreeGridRowAccessible::LocalChildAt(uint32_t aIndex) const {
  if (IsDefunct()) return nullptr;

  RefPtr<nsTreeColumn> column = nsCoreUtils::GetSensibleColumnAt(mTree, aIndex);
  if (!column) return nullptr;

  return GetCellAccessible(column);
}

uint32_t XULTreeGridRowAccessible::ChildCount() const {
  return nsCoreUtils::GetSensibleColumnCount(mTree);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridRowAccessible: XULTreeItemAccessibleBase implementation

XULTreeGridCellAccessible* XULTreeGridRowAccessible::GetCellAccessible(
    nsTreeColumn* aColumn) const {
  MOZ_ASSERT(aColumn, "No tree column!");

  void* key = static_cast<void*>(aColumn);
  XULTreeGridCellAccessible* cachedCell = mAccessibleCache.GetWeak(key);
  if (cachedCell) return cachedCell;

  RefPtr<XULTreeGridCellAccessible> cell = new XULTreeGridCellAccessibleWrap(
      mContent, mDoc, const_cast<XULTreeGridRowAccessible*>(this), mTree,
      mTreeView, mRow, aColumn);
  mAccessibleCache.InsertOrUpdate(key, RefPtr{cell});
  Document()->BindToDocument(cell, nullptr);
  return cell;
}

void XULTreeGridRowAccessible::RowInvalidated(int32_t aStartColIdx,
                                              int32_t aEndColIdx) {
  RefPtr<nsTreeColumns> treeColumns = mTree->GetColumns(FlushType::None);
  if (!treeColumns) return;

  bool nameChanged = false;
  for (int32_t colIdx = aStartColIdx; colIdx <= aEndColIdx; ++colIdx) {
    nsTreeColumn* column = treeColumns->GetColumnAt(colIdx);
    if (column && !nsCoreUtils::IsColumnHidden(column)) {
      XULTreeGridCellAccessible* cell = GetCellAccessible(column);
      if (cell) nameChanged |= cell->CellInvalidated();
    }
  }

  if (nameChanged) {
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible
////////////////////////////////////////////////////////////////////////////////

XULTreeGridCellAccessible::XULTreeGridCellAccessible(
    nsIContent* aContent, DocAccessible* aDoc,
    XULTreeGridRowAccessible* aRowAcc, dom::XULTreeElement* aTree,
    nsITreeView* aTreeView, int32_t aRow, nsTreeColumn* aColumn)
    : LeafAccessible(aContent, aDoc),
      mTree(aTree),
      mTreeView(aTreeView),
      mRow(aRow),
      mColumn(aColumn) {
  mParent = aRowAcc;
  mStateFlags |= eSharedNode;
  mGenericTypes |= eTableCell;

  NS_ASSERTION(mTreeView, "mTreeView is null");

  if (mColumn->Type() == dom::TreeColumn_Binding::TYPE_CHECKBOX) {
    mTreeView->GetCellValue(mRow, mColumn, mCachedTextEquiv);
  } else {
    mTreeView->GetCellText(mRow, mColumn, mCachedTextEquiv);
  }
}

XULTreeGridCellAccessible::~XULTreeGridCellAccessible() {}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: nsISupports implementation

NS_IMPL_CYCLE_COLLECTION_INHERITED(XULTreeGridCellAccessible, LeafAccessible,
                                   mTree, mColumn)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XULTreeGridCellAccessible)
NS_INTERFACE_MAP_END_INHERITING(LeafAccessible)
NS_IMPL_ADDREF_INHERITED(XULTreeGridCellAccessible, LeafAccessible)
NS_IMPL_RELEASE_INHERITED(XULTreeGridCellAccessible, LeafAccessible)

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: LocalAccessible

void XULTreeGridCellAccessible::Shutdown() {
  mTree = nullptr;
  mTreeView = nullptr;
  mRow = -1;
  mColumn = nullptr;
  mParent = nullptr;  // null-out to prevent base class's shutdown ops

  LeafAccessible::Shutdown();
}

LocalAccessible* XULTreeGridCellAccessible::FocusedChild() { return nullptr; }

ENameValueFlag XULTreeGridCellAccessible::Name(nsString& aName) const {
  aName.Truncate();

  if (!mTreeView) return eNameOK;

  mTreeView->GetCellText(mRow, mColumn, aName);

  // If there is still no name try the cell value:
  // This is for graphical cells. We need tree/table view implementors to
  // implement FooView::GetCellValue to return a meaningful string for cases
  // where there is something shown in the cell (non-text) such as a star icon;
  // in which case GetCellValue for that cell would return "starred" or
  // "flagged" for example.
  if (aName.IsEmpty()) mTreeView->GetCellValue(mRow, mColumn, aName);

  return eNameOK;
}

nsIntRect XULTreeGridCellAccessible::BoundsInCSSPixels() const {
  // Get bounds for tree cell and add x and y of treechildren element to
  // x and y of the cell.
  nsresult rv;
  nsIntRect rect = mTree->GetCoordsForCellItem(mRow, mColumn, u"cell"_ns, rv);
  if (NS_FAILED(rv)) {
    return nsIntRect();
  }

  RefPtr<dom::Element> bodyElement = mTree->GetTreeBody();
  if (!bodyElement || !bodyElement->IsXULElement()) {
    return nsIntRect();
  }

  nsIFrame* bodyFrame = bodyElement->GetPrimaryFrame();
  if (!bodyFrame) {
    return nsIntRect();
  }

  CSSIntRect screenRect = bodyFrame->GetScreenRect();
  rect.x += screenRect.x;
  rect.y += screenRect.y;
  return rect;
}

nsRect XULTreeGridCellAccessible::BoundsInAppUnits() const {
  nsIntRect bounds = BoundsInCSSPixels();
  nsPresContext* presContext = mDoc->PresContext();
  return nsRect(presContext->CSSPixelsToAppUnits(bounds.X()),
                presContext->CSSPixelsToAppUnits(bounds.Y()),
                presContext->CSSPixelsToAppUnits(bounds.Width()),
                presContext->CSSPixelsToAppUnits(bounds.Height()));
}

uint8_t XULTreeGridCellAccessible::ActionCount() const {
  if (mColumn->Cycler()) return 1;

  if (mColumn->Type() == dom::TreeColumn_Binding::TYPE_CHECKBOX &&
      IsEditable()) {
    return 1;
  }

  return 0;
}

void XULTreeGridCellAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();

  if (aIndex != eAction_Click || !mTreeView) return;

  if (mColumn->Cycler()) {
    aName.AssignLiteral("cycle");
    return;
  }

  if (mColumn->Type() == dom::TreeColumn_Binding::TYPE_CHECKBOX &&
      IsEditable()) {
    nsAutoString value;
    mTreeView->GetCellValue(mRow, mColumn, value);
    if (value.EqualsLiteral("true")) {
      aName.AssignLiteral("uncheck");
    } else {
      aName.AssignLiteral("check");
    }
  }
}

bool XULTreeGridCellAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != eAction_Click) return false;

  if (mColumn->Cycler()) {
    DoCommand();
    return true;
  }

  if (mColumn->Type() == dom::TreeColumn_Binding::TYPE_CHECKBOX &&
      IsEditable()) {
    DoCommand();
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: TableCell

TableAccessible* XULTreeGridCellAccessible::Table() const {
  LocalAccessible* grandParent = mParent->LocalParent();
  if (grandParent) return grandParent->AsTable();

  return nullptr;
}

uint32_t XULTreeGridCellAccessible::ColIdx() const {
  uint32_t colIdx = 0;
  RefPtr<nsTreeColumn> column = mColumn;
  while ((column = nsCoreUtils::GetPreviousSensibleColumn(column))) colIdx++;

  return colIdx;
}

uint32_t XULTreeGridCellAccessible::RowIdx() const { return mRow; }

void XULTreeGridCellAccessible::ColHeaderCells(
    nsTArray<LocalAccessible*>* aHeaderCells) {
  dom::Element* columnElm = mColumn->Element();

  LocalAccessible* headerCell = mDoc->GetAccessible(columnElm);
  if (headerCell) aHeaderCells->AppendElement(headerCell);
}

bool XULTreeGridCellAccessible::Selected() {
  nsCOMPtr<nsITreeSelection> selection;
  nsresult rv = mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, false);

  bool selected = false;
  selection->IsSelected(mRow, &selected);
  return selected;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: LocalAccessible public implementation

already_AddRefed<AccAttributes> XULTreeGridCellAccessible::NativeAttributes() {
  RefPtr<AccAttributes> attributes = new AccAttributes();

  // "table-cell-index" attribute
  TableAccessible* table = Table();
  if (!table) return attributes.forget();

  attributes->SetAttribute(nsGkAtoms::tableCellIndex,
                           table->CellIndexAt(mRow, ColIdx()));

  // "cycles" attribute
  if (mColumn->Cycler()) {
    attributes->SetAttribute(nsGkAtoms::cycles, true);
  }

  return attributes.forget();
}

role XULTreeGridCellAccessible::NativeRole() const { return roles::GRID_CELL; }

uint64_t XULTreeGridCellAccessible::NativeState() const {
  if (!mTreeView) return states::DEFUNCT;

  // selectable/selected state
  uint64_t states =
      states::SELECTABLE;  // keep in sync with NativeInteractiveState

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool isSelected = false;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected) states |= states::SELECTED;
  }

  // checked state
  if (mColumn->Type() == dom::TreeColumn_Binding::TYPE_CHECKBOX) {
    states |= states::CHECKABLE;
    nsAutoString checked;
    mTreeView->GetCellValue(mRow, mColumn, checked);
    if (checked.EqualsIgnoreCase("true")) states |= states::CHECKED;
  }

  return states;
}

uint64_t XULTreeGridCellAccessible::NativeInteractiveState() const {
  return states::SELECTABLE;
}

int32_t XULTreeGridCellAccessible::IndexInParent() const { return ColIdx(); }

Relation XULTreeGridCellAccessible::RelationByType(RelationType aType) const {
  return Relation();
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: public implementation

bool XULTreeGridCellAccessible::CellInvalidated() {
  nsAutoString textEquiv;

  if (mColumn->Type() == dom::TreeColumn_Binding::TYPE_CHECKBOX) {
    mTreeView->GetCellValue(mRow, mColumn, textEquiv);
    if (mCachedTextEquiv != textEquiv) {
      bool isEnabled = textEquiv.EqualsLiteral("true");
      RefPtr<AccEvent> accEvent =
          new AccStateChangeEvent(this, states::CHECKED, isEnabled);
      nsEventShell::FireEvent(accEvent);

      mCachedTextEquiv = textEquiv;
      return true;
    }

    return false;
  }

  mTreeView->GetCellText(mRow, mColumn, textEquiv);
  if (mCachedTextEquiv != textEquiv) {
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
    mCachedTextEquiv = textEquiv;
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: LocalAccessible protected implementation

LocalAccessible* XULTreeGridCellAccessible::GetSiblingAtOffset(
    int32_t aOffset, nsresult* aError) const {
  if (aError) *aError = NS_OK;  // fail peacefully

  RefPtr<nsTreeColumn> columnAtOffset(mColumn), column;
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

  if (!columnAtOffset) return nullptr;

  RefPtr<XULTreeItemAccessibleBase> rowAcc = do_QueryObject(LocalParent());
  return rowAcc->GetCellAccessible(columnAtOffset);
}

void XULTreeGridCellAccessible::DispatchClickEvent(
    nsIContent* aContent, uint32_t aActionIndex) const {
  if (IsDefunct()) return;

  RefPtr<dom::XULTreeElement> tree = mTree;
  RefPtr<nsTreeColumn> column = mColumn;
  nsCoreUtils::DispatchClickEvent(tree, mRow, column);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessible: protected implementation

bool XULTreeGridCellAccessible::IsEditable() const {
  // XXX: logic corresponds to tree.xml, it's preferable to have interface
  // method to check it.
  bool isEditable = false;
  nsresult rv = mTreeView->IsEditable(mRow, mColumn, &isEditable);
  if (NS_FAILED(rv) || !isEditable) return false;

  dom::Element* columnElm = mColumn->Element();

  if (!columnElm->AttrValueIs(kNameSpaceID_None, nsGkAtoms::editable,
                              nsGkAtoms::_true, eCaseMatters)) {
    return false;
  }

  return mContent->AsElement()->AttrValueIs(
      kNameSpaceID_None, nsGkAtoms::editable, nsGkAtoms::_true, eCaseMatters);
}
