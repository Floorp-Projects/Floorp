/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAGridAccessible-inl.h"

#include "LocalAccessible-inl.h"
#include "AccAttributes.h"
#include "AccIterator.h"
#include "nsAccUtils.h"
#include "Role.h"
#include "States.h"

#include "mozilla/dom/Element.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// ARIAGridAccessible
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Constructor

ARIAGridAccessible::ARIAGridAccessible(nsIContent* aContent,
                                       DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {
  mGenericTypes |= eTable;
}

role ARIAGridAccessible::NativeRole() const {
  a11y::role r = GetAccService()->MarkupRole(mContent);
  return r != roles::NOTHING ? r : roles::TABLE;
}

already_AddRefed<AccAttributes> ARIAGridAccessible::NativeAttributes() {
  RefPtr<AccAttributes> attributes = AccessibleWrap::NativeAttributes();

  if (IsProbablyLayoutTable()) {
    attributes->SetAttribute(nsGkAtoms::layout_guess, true);
  }

  return attributes.forget();
}

////////////////////////////////////////////////////////////////////////////////
// Table

uint32_t ARIAGridAccessible::ColCount() const {
  AccIterator rowIter(this, filters::GetRow);
  LocalAccessible* row = rowIter.Next();
  if (!row) return 0;

  AccIterator cellIter(row, filters::GetCell);
  LocalAccessible* cell = nullptr;

  uint32_t colCount = 0;
  while ((cell = cellIter.Next())) {
    MOZ_ASSERT(cell->IsTableCell(), "No table or grid cell!");
    colCount += cell->AsTableCell()->ColExtent();
  }

  return colCount;
}

uint32_t ARIAGridAccessible::RowCount() {
  uint32_t rowCount = 0;
  AccIterator rowIter(this, filters::GetRow);
  while (rowIter.Next()) rowCount++;

  return rowCount;
}

LocalAccessible* ARIAGridAccessible::CellAt(uint32_t aRowIndex,
                                            uint32_t aColumnIndex) {
  LocalAccessible* row = RowAt(aRowIndex);
  if (!row) return nullptr;

  return CellInRowAt(row, aColumnIndex);
}

bool ARIAGridAccessible::IsColSelected(uint32_t aColIdx) {
  if (IsARIARole(nsGkAtoms::table)) return false;

  AccIterator rowIter(this, filters::GetRow);
  LocalAccessible* row = rowIter.Next();
  if (!row) return false;

  do {
    if (!nsAccUtils::IsARIASelected(row)) {
      LocalAccessible* cell = CellInRowAt(row, aColIdx);
      if (!cell || !nsAccUtils::IsARIASelected(cell)) return false;
    }
  } while ((row = rowIter.Next()));

  return true;
}

bool ARIAGridAccessible::IsRowSelected(uint32_t aRowIdx) {
  if (IsARIARole(nsGkAtoms::table)) return false;

  LocalAccessible* row = RowAt(aRowIdx);
  if (!row) return false;

  if (!nsAccUtils::IsARIASelected(row)) {
    AccIterator cellIter(row, filters::GetCell);
    LocalAccessible* cell = nullptr;
    while ((cell = cellIter.Next())) {
      if (!nsAccUtils::IsARIASelected(cell)) return false;
    }
  }

  return true;
}

bool ARIAGridAccessible::IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx) {
  if (IsARIARole(nsGkAtoms::table)) return false;

  LocalAccessible* row = RowAt(aRowIdx);
  if (!row) return false;

  if (!nsAccUtils::IsARIASelected(row)) {
    LocalAccessible* cell = CellInRowAt(row, aColIdx);
    if (!cell || !nsAccUtils::IsARIASelected(cell)) return false;
  }

  return true;
}

uint32_t ARIAGridAccessible::SelectedCellCount() {
  if (IsARIARole(nsGkAtoms::table)) return 0;

  uint32_t count = 0, colCount = ColCount();

  AccIterator rowIter(this, filters::GetRow);
  LocalAccessible* row = nullptr;

  while ((row = rowIter.Next())) {
    if (nsAccUtils::IsARIASelected(row)) {
      count += colCount;
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    LocalAccessible* cell = nullptr;

    while ((cell = cellIter.Next())) {
      if (nsAccUtils::IsARIASelected(cell)) count++;
    }
  }

  return count;
}

uint32_t ARIAGridAccessible::SelectedColCount() {
  if (IsARIARole(nsGkAtoms::table)) return 0;

  uint32_t colCount = ColCount();
  if (!colCount) return 0;

  AccIterator rowIter(this, filters::GetRow);
  LocalAccessible* row = rowIter.Next();
  if (!row) return 0;

  nsTArray<bool> isColSelArray(colCount);
  isColSelArray.AppendElements(colCount);
  memset(isColSelArray.Elements(), true, colCount * sizeof(bool));

  uint32_t selColCount = colCount;
  do {
    if (nsAccUtils::IsARIASelected(row)) continue;

    AccIterator cellIter(row, filters::GetCell);
    LocalAccessible* cell = nullptr;
    for (uint32_t colIdx = 0; (cell = cellIter.Next()) && colIdx < colCount;
         colIdx++) {
      if (isColSelArray[colIdx] && !nsAccUtils::IsARIASelected(cell)) {
        isColSelArray[colIdx] = false;
        selColCount--;
      }
    }
  } while ((row = rowIter.Next()));

  return selColCount;
}

uint32_t ARIAGridAccessible::SelectedRowCount() {
  if (IsARIARole(nsGkAtoms::table)) return 0;

  uint32_t count = 0;

  AccIterator rowIter(this, filters::GetRow);
  LocalAccessible* row = nullptr;

  while ((row = rowIter.Next())) {
    if (nsAccUtils::IsARIASelected(row)) {
      count++;
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    LocalAccessible* cell = cellIter.Next();
    if (!cell) continue;

    bool isRowSelected = true;
    do {
      if (!nsAccUtils::IsARIASelected(cell)) {
        isRowSelected = false;
        break;
      }
    } while ((cell = cellIter.Next()));

    if (isRowSelected) count++;
  }

  return count;
}

void ARIAGridAccessible::SelectedCells(nsTArray<LocalAccessible*>* aCells) {
  if (IsARIARole(nsGkAtoms::table)) return;

  AccIterator rowIter(this, filters::GetRow);

  LocalAccessible* row = nullptr;
  while ((row = rowIter.Next())) {
    AccIterator cellIter(row, filters::GetCell);
    LocalAccessible* cell = nullptr;

    if (nsAccUtils::IsARIASelected(row)) {
      while ((cell = cellIter.Next())) aCells->AppendElement(cell);

      continue;
    }

    while ((cell = cellIter.Next())) {
      if (nsAccUtils::IsARIASelected(cell)) aCells->AppendElement(cell);
    }
  }
}

void ARIAGridAccessible::SelectedCellIndices(nsTArray<uint32_t>* aCells) {
  if (IsARIARole(nsGkAtoms::table)) return;

  uint32_t colCount = ColCount();

  AccIterator rowIter(this, filters::GetRow);
  LocalAccessible* row = nullptr;
  for (uint32_t rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    if (nsAccUtils::IsARIASelected(row)) {
      for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
        aCells->AppendElement(rowIdx * colCount + colIdx);
      }

      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    LocalAccessible* cell = nullptr;
    for (uint32_t colIdx = 0; (cell = cellIter.Next()); colIdx++) {
      if (nsAccUtils::IsARIASelected(cell)) {
        aCells->AppendElement(rowIdx * colCount + colIdx);
      }
    }
  }
}

void ARIAGridAccessible::SelectedColIndices(nsTArray<uint32_t>* aCols) {
  if (IsARIARole(nsGkAtoms::table)) return;

  uint32_t colCount = ColCount();
  if (!colCount) return;

  AccIterator rowIter(this, filters::GetRow);
  LocalAccessible* row = rowIter.Next();
  if (!row) return;

  nsTArray<bool> isColSelArray(colCount);
  isColSelArray.AppendElements(colCount);
  memset(isColSelArray.Elements(), true, colCount * sizeof(bool));

  do {
    if (nsAccUtils::IsARIASelected(row)) continue;

    AccIterator cellIter(row, filters::GetCell);
    LocalAccessible* cell = nullptr;
    for (uint32_t colIdx = 0; (cell = cellIter.Next()) && colIdx < colCount;
         colIdx++) {
      if (isColSelArray[colIdx] && !nsAccUtils::IsARIASelected(cell)) {
        isColSelArray[colIdx] = false;
      }
    }
  } while ((row = rowIter.Next()));

  for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
    if (isColSelArray[colIdx]) aCols->AppendElement(colIdx);
  }
}

void ARIAGridAccessible::SelectedRowIndices(nsTArray<uint32_t>* aRows) {
  if (IsARIARole(nsGkAtoms::table)) return;

  AccIterator rowIter(this, filters::GetRow);
  LocalAccessible* row = nullptr;
  for (uint32_t rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    if (nsAccUtils::IsARIASelected(row)) {
      aRows->AppendElement(rowIdx);
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    LocalAccessible* cell = cellIter.Next();
    if (!cell) continue;

    bool isRowSelected = true;
    do {
      if (!nsAccUtils::IsARIASelected(cell)) {
        isRowSelected = false;
        break;
      }
    } while ((cell = cellIter.Next()));

    if (isRowSelected) aRows->AppendElement(rowIdx);
  }
}

void ARIAGridAccessible::SelectRow(uint32_t aRowIdx) {
  if (IsARIARole(nsGkAtoms::table)) return;

  AccIterator rowIter(this, filters::GetRow);

  LocalAccessible* row = nullptr;
  for (uint32_t rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    DebugOnly<nsresult> rv = SetARIASelected(row, rowIdx == aRowIdx);
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetARIASelected() Shouldn't fail!");
  }
}

void ARIAGridAccessible::SelectCol(uint32_t aColIdx) {
  if (IsARIARole(nsGkAtoms::table)) return;

  AccIterator rowIter(this, filters::GetRow);

  LocalAccessible* row = nullptr;
  while ((row = rowIter.Next())) {
    // Unselect all cells in the row.
    DebugOnly<nsresult> rv = SetARIASelected(row, false);
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetARIASelected() Shouldn't fail!");

    // Select cell at the column index.
    LocalAccessible* cell = CellInRowAt(row, aColIdx);
    if (cell) SetARIASelected(cell, true);
  }
}

void ARIAGridAccessible::UnselectRow(uint32_t aRowIdx) {
  if (IsARIARole(nsGkAtoms::table)) return;

  LocalAccessible* row = RowAt(aRowIdx);
  if (row) SetARIASelected(row, false);
}

void ARIAGridAccessible::UnselectCol(uint32_t aColIdx) {
  if (IsARIARole(nsGkAtoms::table)) return;

  AccIterator rowIter(this, filters::GetRow);

  LocalAccessible* row = nullptr;
  while ((row = rowIter.Next())) {
    LocalAccessible* cell = CellInRowAt(row, aColIdx);
    if (cell) SetARIASelected(cell, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Protected

nsresult ARIAGridAccessible::SetARIASelected(LocalAccessible* aAccessible,
                                             bool aIsSelected, bool aNotify) {
  if (IsARIARole(nsGkAtoms::table)) return NS_OK;

  nsIContent* content = aAccessible->GetContent();
  NS_ENSURE_STATE(content);

  nsresult rv = NS_OK;
  if (content->IsElement()) {
    if (aIsSelected) {
      rv = content->AsElement()->SetAttr(
          kNameSpaceID_None, nsGkAtoms::aria_selected, u"true"_ns, aNotify);
    } else {
      rv = content->AsElement()->SetAttr(
          kNameSpaceID_None, nsGkAtoms::aria_selected, u"false"_ns, aNotify);
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // No "smart" select/unselect for internal call.
  if (!aNotify) return NS_OK;

  // If row or cell accessible was selected then we're able to not bother about
  // selection of its cells or its row because our algorithm is row oriented,
  // i.e. we check selection on row firstly and then on cells.
  if (aIsSelected) return NS_OK;

  roles::Role role = aAccessible->Role();

  // If the given accessible is row that was unselected then remove
  // aria-selected from cell accessible.
  if (role == roles::ROW) {
    AccIterator cellIter(aAccessible, filters::GetCell);
    LocalAccessible* cell = nullptr;

    while ((cell = cellIter.Next())) {
      rv = SetARIASelected(cell, false, false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }

  // If the given accessible is cell that was unselected and its row is selected
  // then remove aria-selected from row and put aria-selected on
  // siblings cells.
  if (role == roles::GRID_CELL || role == roles::ROWHEADER ||
      role == roles::COLUMNHEADER) {
    LocalAccessible* row = aAccessible->LocalParent();

    if (row && row->Role() == roles::ROW && nsAccUtils::IsARIASelected(row)) {
      rv = SetARIASelected(row, false, false);
      NS_ENSURE_SUCCESS(rv, rv);

      AccIterator cellIter(row, filters::GetCell);
      LocalAccessible* cell = nullptr;
      while ((cell = cellIter.Next())) {
        if (cell != aAccessible) {
          rv = SetARIASelected(cell, true, false);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// ARIARowAccessible
////////////////////////////////////////////////////////////////////////////////

ARIARowAccessible::ARIARowAccessible(nsIContent* aContent, DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {
  mGenericTypes |= eTableRow;
}

role ARIARowAccessible::NativeRole() const {
  a11y::role r = GetAccService()->MarkupRole(mContent);
  return r != roles::NOTHING ? r : roles::ROW;
}

GroupPos ARIARowAccessible::GroupPosition() {
  int32_t count = 0, index = 0;
  LocalAccessible* table = nsAccUtils::TableFor(this);
  if (table) {
    if (nsCoreUtils::GetUIntAttr(table->GetContent(), nsGkAtoms::aria_rowcount,
                                 &count) &&
        nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_rowindex, &index)) {
      return GroupPos(0, index, count);
    }

    // Deal with the special case here that tables and grids can have rows
    // which are wrapped in generic text container elements. Exclude tree grids
    // because these are dealt with elsewhere.
    if (table->Role() == roles::TABLE) {
      LocalAccessible* row = nullptr;
      AccIterator rowIter(table, filters::GetRow);
      while ((row = rowIter.Next())) {
        index++;
        if (row == this) {
          break;
        }
      }

      if (row) {
        count = table->AsTable()->RowCount();
        return GroupPos(0, index, count);
      }
    }
  }

  return AccessibleWrap::GroupPosition();
}

// LocalAccessible protected
ENameValueFlag ARIARowAccessible::NativeName(nsString& aName) const {
  // We want to calculate the name from content only if an ARIA role is
  // present. ARIARowAccessible might also be used by tables with
  // display:block; styling, in which case we do not want the name from
  // content.
  if (HasStrongARIARole()) {
    return AccessibleWrap::NativeName(aName);
  }

  return eNameOK;
}

////////////////////////////////////////////////////////////////////////////////
// ARIAGridCellAccessible
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Constructor

ARIAGridCellAccessible::ARIAGridCellAccessible(nsIContent* aContent,
                                               DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {
  mGenericTypes |= eTableCell;
}

role ARIAGridCellAccessible::NativeRole() const {
  a11y::role r = GetAccService()->MarkupRole(mContent);
  return r != roles::NOTHING ? r : roles::CELL;
}

////////////////////////////////////////////////////////////////////////////////
// TableCell

TableAccessible* ARIAGridCellAccessible::Table() const {
  LocalAccessible* table = nsAccUtils::TableFor(Row());
  return table ? table->AsTable() : nullptr;
}

uint32_t ARIAGridCellAccessible::ColIdx() const {
  LocalAccessible* row = Row();
  if (!row) return 0;

  int32_t indexInRow = IndexInParent();
  uint32_t colIdx = 0;
  for (int32_t idx = 0; idx < indexInRow; idx++) {
    LocalAccessible* cell = row->LocalChildAt(idx);
    if (cell->IsTableCell()) {
      colIdx += cell->AsTableCell()->ColExtent();
    }
  }

  return colIdx;
}

uint32_t ARIAGridCellAccessible::RowIdx() const { return RowIndexFor(Row()); }

bool ARIAGridCellAccessible::Selected() {
  LocalAccessible* row = Row();
  if (!row) return false;

  return nsAccUtils::IsARIASelected(row) || nsAccUtils::IsARIASelected(this);
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible

void ARIAGridCellAccessible::ApplyARIAState(uint64_t* aState) const {
  HyperTextAccessibleWrap::ApplyARIAState(aState);

  // Return if the gridcell has aria-selected="true".
  if (*aState & states::SELECTED) return;

  // Check aria-selected="true" on the row.
  LocalAccessible* row = LocalParent();
  if (!row || row->Role() != roles::ROW) return;

  nsIContent* rowContent = row->GetContent();
  if (nsAccUtils::HasDefinedARIAToken(rowContent, nsGkAtoms::aria_selected) &&
      !rowContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                            nsGkAtoms::aria_selected,
                                            nsGkAtoms::_false, eCaseMatters)) {
    *aState |= states::SELECTABLE | states::SELECTED;
  }
}

already_AddRefed<AccAttributes> ARIAGridCellAccessible::NativeAttributes() {
  RefPtr<AccAttributes> attributes =
      HyperTextAccessibleWrap::NativeAttributes();

  // Expose "table-cell-index" attribute.
  LocalAccessible* thisRow = Row();
  if (!thisRow) return attributes.forget();

  int32_t rowIdx = RowIndexFor(thisRow);
  if (rowIdx == -1) {  // error
    return attributes.forget();
  }

  int32_t colIdx = 0, colCount = 0;
  uint32_t childCount = thisRow->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    LocalAccessible* child = thisRow->LocalChildAt(childIdx);
    if (child == this) colIdx = colCount;

    roles::Role role = child->Role();
    if (role == roles::CELL || role == roles::GRID_CELL ||
        role == roles::ROWHEADER || role == roles::COLUMNHEADER) {
      colCount++;
    }
  }

  attributes->SetAttribute(nsGkAtoms::tableCellIndex,
                           rowIdx * colCount + colIdx);

#ifdef DEBUG
  RefPtr<nsAtom> cppClass = NS_Atomize(u"cppclass"_ns);
  attributes->SetAttributeStringCopy(cppClass, u"ARIAGridCellAccessible"_ns);
#endif

  return attributes.forget();
}

GroupPos ARIAGridCellAccessible::GroupPosition() {
  int32_t count = 0, index = 0;
  TableAccessible* table = Table();
  if (table &&
      nsCoreUtils::GetUIntAttr(table->AsAccessible()->GetContent(),
                               nsGkAtoms::aria_colcount, &count) &&
      nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_colindex, &index)) {
    return GroupPos(0, index, count);
  }

  return GroupPos();
}
