/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAGridAccessible.h"

#include "Accessible-inl.h"
#include "AccIterator.h"
#include "nsAccUtils.h"
#include "Role.h"
#include "States.h"

#include "nsIMutableArray.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// ARIAGridAccessible
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Constructor

ARIAGridAccessible::
  ARIAGridAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc), xpcAccessibleTable(this)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(ARIAGridAccessible,
                             Accessible,
                             nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
//nsAccessNode

void
ARIAGridAccessible::Shutdown()
{
  mTable = nsnull;
  AccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleTable

PRUint32
ARIAGridAccessible::ColCount()
{
  AccIterator rowIter(this, filters::GetRow);
  Accessible* row = rowIter.Next();
  if (!row)
    return 0;

  AccIterator cellIter(row, filters::GetCell);
  Accessible* cell = nsnull;

  PRUint32 colCount = 0;
  while ((cell = cellIter.Next()))
    colCount++;

  return colCount;
}

PRUint32
ARIAGridAccessible::RowCount()
{
  PRUint32 rowCount = 0;
  AccIterator rowIter(this, filters::GetRow);
  while (rowIter.Next())
    rowCount++;

  return rowCount;
}

Accessible*
ARIAGridAccessible::CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex)
{ 
  Accessible* row = GetRowAt(aRowIndex);
  if (!row)
    return nsnull;

  return GetCellInRowAt(row, aColumnIndex);
}

bool
ARIAGridAccessible::IsColSelected(PRUint32 aColIdx)
{
  AccIterator rowIter(this, filters::GetRow);
  Accessible* row = rowIter.Next();
  if (!row)
    return false;

  do {
    if (!nsAccUtils::IsARIASelected(row)) {
      Accessible* cell = GetCellInRowAt(row, aColIdx);
      if (!cell || !nsAccUtils::IsARIASelected(cell))
        return false;
    }
  } while ((row = rowIter.Next()));

  return true;
}

bool
ARIAGridAccessible::IsRowSelected(PRUint32 aRowIdx)
{
  Accessible* row = GetRowAt(aRowIdx);
  if(!row)
    return false;

  if (!nsAccUtils::IsARIASelected(row)) {
    AccIterator cellIter(row, filters::GetCell);
    Accessible* cell = nsnull;
    while ((cell = cellIter.Next())) {
      if (!nsAccUtils::IsARIASelected(cell))
        return false;
    }
  }

  return true;
}

bool
ARIAGridAccessible::IsCellSelected(PRUint32 aRowIdx, PRUint32 aColIdx)
{
  Accessible* row = GetRowAt(aRowIdx);
  if(!row)
    return false;

  if (!nsAccUtils::IsARIASelected(row)) {
    Accessible* cell = GetCellInRowAt(row, aColIdx);
    if (!cell || !nsAccUtils::IsARIASelected(cell))
      return false;
  }

  return true;
}

PRUint32
ARIAGridAccessible::SelectedCellCount()
{
  PRUint32 count = 0, colCount = ColCount();

  AccIterator rowIter(this, filters::GetRow);
  Accessible* row = nsnull;

  while ((row = rowIter.Next())) {
    if (nsAccUtils::IsARIASelected(row)) {
      count += colCount;
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    Accessible* cell = nsnull;

    while ((cell = cellIter.Next())) {
      if (nsAccUtils::IsARIASelected(cell))
        count++;
    }
  }

  return count;
}

PRUint32
ARIAGridAccessible::SelectedColCount()
{
  PRUint32 colCount = ColCount();
  if (!colCount)
    return 0;

  AccIterator rowIter(this, filters::GetRow);
  Accessible* row = rowIter.Next();
  if (!row)
    return 0;

  nsTArray<bool> isColSelArray(colCount);
  isColSelArray.AppendElements(colCount);
  memset(isColSelArray.Elements(), true, colCount * sizeof(bool));

  PRUint32 selColCount = colCount;
  do {
    if (nsAccUtils::IsARIASelected(row))
      continue;

    AccIterator cellIter(row, filters::GetCell);
    Accessible* cell = nsnull;
    for (PRUint32 colIdx = 0;
         (cell = cellIter.Next()) && colIdx < colCount; colIdx++)
      if (isColSelArray[colIdx] && !nsAccUtils::IsARIASelected(cell)) {
        isColSelArray[colIdx] = false;
        selColCount--;
      }
  } while ((row = rowIter.Next()));

  return selColCount;
}

PRUint32
ARIAGridAccessible::SelectedRowCount()
{
  PRUint32 count = 0;

  AccIterator rowIter(this, filters::GetRow);
  Accessible* row = nsnull;

  while ((row = rowIter.Next())) {
    if (nsAccUtils::IsARIASelected(row)) {
      count++;
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    Accessible* cell = cellIter.Next();
    if (!cell)
      continue;

    bool isRowSelected = true;
    do {
      if (!nsAccUtils::IsARIASelected(cell)) {
        isRowSelected = false;
        break;
      }
    } while ((cell = cellIter.Next()));

    if (isRowSelected)
      count++;
  }

  return count;
}

void
ARIAGridAccessible::SelectedCells(nsTArray<Accessible*>* aCells)
{
  AccIterator rowIter(this, filters::GetRow);

  Accessible* row = nsnull;
  while ((row = rowIter.Next())) {
    AccIterator cellIter(row, filters::GetCell);
    Accessible* cell = nsnull;

    if (nsAccUtils::IsARIASelected(row)) {
      while ((cell = cellIter.Next()))
        aCells->AppendElement(cell);

      continue;
    }

    while ((cell = cellIter.Next())) {
      if (nsAccUtils::IsARIASelected(cell))
        aCells->AppendElement(cell);
    }
  }
}

void
ARIAGridAccessible::SelectedCellIndices(nsTArray<PRUint32>* aCells)
{
  PRUint32 rowCount = RowCount(), colCount = ColCount();

  AccIterator rowIter(this, filters::GetRow);
  Accessible* row = nsnull;
  for (PRUint32 rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    if (nsAccUtils::IsARIASelected(row)) {
      for (PRUint32 colIdx = 0; colIdx < colCount; colIdx++)
        aCells->AppendElement(rowIdx * colCount + colIdx);

      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    Accessible* cell = nsnull;
    for (PRUint32 colIdx = 0; (cell = cellIter.Next()); colIdx++) {
      if (nsAccUtils::IsARIASelected(cell))
        aCells->AppendElement(rowIdx * colCount + colIdx);
    }
  }
}

void
ARIAGridAccessible::SelectedColIndices(nsTArray<PRUint32>* aCols)
{
  PRUint32 colCount = ColCount();
  if (!colCount)
    return;

  AccIterator rowIter(this, filters::GetRow);
  Accessible* row = rowIter.Next();
  if (!row)
    return;

  nsTArray<bool> isColSelArray(colCount);
  isColSelArray.AppendElements(colCount);
  memset(isColSelArray.Elements(), true, colCount * sizeof(bool));

  do {
    if (nsAccUtils::IsARIASelected(row))
      continue;

    AccIterator cellIter(row, filters::GetCell);
    Accessible* cell = nsnull;
    for (PRUint32 colIdx = 0;
         (cell = cellIter.Next()) && colIdx < colCount; colIdx++)
      if (isColSelArray[colIdx] && !nsAccUtils::IsARIASelected(cell)) {
        isColSelArray[colIdx] = false;
      }
  } while ((row = rowIter.Next()));

  for (PRUint32 colIdx = 0; colIdx < colCount; colIdx++)
    if (isColSelArray[colIdx])
      aCols->AppendElement(colIdx);
}

void
ARIAGridAccessible::SelectedRowIndices(nsTArray<PRUint32>* aRows)
{
  AccIterator rowIter(this, filters::GetRow);
  Accessible* row = nsnull;
  for (PRUint32 rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    if (nsAccUtils::IsARIASelected(row)) {
      aRows->AppendElement(rowIdx);
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    Accessible* cell = cellIter.Next();
    if (!cell)
      continue;

    bool isRowSelected = true;
    do {
      if (!nsAccUtils::IsARIASelected(cell)) {
        isRowSelected = false;
        break;
      }
    } while ((cell = cellIter.Next()));

    if (isRowSelected)
      aRows->AppendElement(rowIdx);
  }
}

void
ARIAGridAccessible::SelectRow(PRUint32 aRowIdx)
{
  AccIterator rowIter(this, filters::GetRow);

  Accessible* row = nsnull;
  for (PRInt32 rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    nsresult rv = SetARIASelected(row, rowIdx == aRowIdx);
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetARIASelected() Shouldn't fail!");
  }
}

void
ARIAGridAccessible::SelectCol(PRUint32 aColIdx)
{
  AccIterator rowIter(this, filters::GetRow);

  Accessible* row = nsnull;
  while ((row = rowIter.Next())) {
    // Unselect all cells in the row.
    nsresult rv = SetARIASelected(row, false);
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetARIASelected() Shouldn't fail!");

    // Select cell at the column index.
    Accessible* cell = GetCellInRowAt(row, aColIdx);
    if (cell)
      SetARIASelected(cell, true);
  }
}

void
ARIAGridAccessible::UnselectRow(PRUint32 aRowIdx)
{
  Accessible* row = GetRowAt(aRowIdx);

  if (row)
    SetARIASelected(row, false);
}

void
ARIAGridAccessible::UnselectCol(PRUint32 aColIdx)
{
  AccIterator rowIter(this, filters::GetRow);

  Accessible* row = nsnull;
  while ((row = rowIter.Next())) {
    Accessible* cell = GetCellInRowAt(row, aColIdx);
    if (cell)
      SetARIASelected(cell, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Protected

bool
ARIAGridAccessible::IsValidRow(PRInt32 aRow)
{
  if (aRow < 0)
    return false;
  
  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);
  return aRow < rowCount;
}

bool
ARIAGridAccessible::IsValidColumn(PRInt32 aColumn)
{
  if (aColumn < 0)
    return false;

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);
  return aColumn < colCount;
}

Accessible*
ARIAGridAccessible::GetRowAt(PRInt32 aRow)
{
  PRInt32 rowIdx = aRow;

  AccIterator rowIter(this, filters::GetRow);

  Accessible* row = rowIter.Next();
  while (rowIdx != 0 && (row = rowIter.Next()))
    rowIdx--;

  return row;
}

Accessible*
ARIAGridAccessible::GetCellInRowAt(Accessible* aRow, PRInt32 aColumn)
{
  PRInt32 colIdx = aColumn;

  AccIterator cellIter(aRow, filters::GetCell);
  Accessible* cell = cellIter.Next();
  while (colIdx != 0 && (cell = cellIter.Next()))
    colIdx--;

  return cell;
}

nsresult
ARIAGridAccessible::SetARIASelected(Accessible* aAccessible,
                                    bool aIsSelected, bool aNotify)
{
  nsIContent *content = aAccessible->GetContent();
  NS_ENSURE_STATE(content);

  nsresult rv = NS_OK;
  if (aIsSelected)
    rv = content->SetAttr(kNameSpaceID_None, nsGkAtoms::aria_selected,
                          NS_LITERAL_STRING("true"), aNotify);
  else
    rv = content->UnsetAttr(kNameSpaceID_None,
                            nsGkAtoms::aria_selected, aNotify);

  NS_ENSURE_SUCCESS(rv, rv);

  // No "smart" select/unselect for internal call.
  if (!aNotify)
    return NS_OK;

  // If row or cell accessible was selected then we're able to not bother about
  // selection of its cells or its row because our algorithm is row oriented,
  // i.e. we check selection on row firstly and then on cells.
  if (aIsSelected)
    return NS_OK;

  roles::Role role = aAccessible->Role();

  // If the given accessible is row that was unselected then remove
  // aria-selected from cell accessible.
  if (role == roles::ROW) {
    AccIterator cellIter(aAccessible, filters::GetCell);
    Accessible* cell = nsnull;

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
    Accessible* row = aAccessible->Parent();

    if (row && row->Role() == roles::ROW &&
        nsAccUtils::IsARIASelected(row)) {
      rv = SetARIASelected(row, false, false);
      NS_ENSURE_SUCCESS(rv, rv);

      AccIterator cellIter(row, filters::GetCell);
      Accessible* cell = nsnull;
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
// ARIAGridCellAccessible
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Constructor

ARIAGridCellAccessible::
  ARIAGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(ARIAGridCellAccessible,
                             HyperTextAccessible,
                             nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleTableCell

NS_IMETHODIMP
ARIAGridCellAccessible::GetTable(nsIAccessibleTable** aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  Accessible* thisRow = Parent();
  if (!thisRow || thisRow->Role() != roles::ROW)
    return NS_OK;

  Accessible* table = thisRow->Parent();
  if (!table)
    return NS_OK;

  roles::Role tableRole = table->Role();
  if (tableRole != roles::TABLE && tableRole != roles::TREE_TABLE)
    return NS_OK;

  CallQueryInterface(table, aTable);
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridCellAccessible::GetColumnIndex(PRInt32* aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* row = Parent();
  if (!row)
    return NS_OK;

  *aColumnIndex = 0;

  PRInt32 indexInRow = IndexInParent();
  for (PRInt32 idx = 0; idx < indexInRow; idx++) {
    Accessible* cell = row->GetChildAt(idx);
    roles::Role role = cell->Role();
    if (role == roles::GRID_CELL || role == roles::ROWHEADER ||
        role == roles::COLUMNHEADER)
      (*aColumnIndex)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
ARIAGridCellAccessible::GetRowIndex(PRInt32* aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* row = Parent();
  if (!row)
    return NS_OK;

  Accessible* table = row->Parent();
  if (!table)
    return NS_OK;

  *aRowIndex = 0;

  PRInt32 indexInTable = row->IndexInParent();
  for (PRInt32 idx = 0; idx < indexInTable; idx++) {
    row = table->GetChildAt(idx);
    if (row->Role() == roles::ROW)
      (*aRowIndex)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
ARIAGridCellAccessible::GetColumnExtent(PRInt32* aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridCellAccessible::GetRowExtent(PRInt32* aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridCellAccessible::GetColumnHeaderCells(nsIArray** aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleTable> table;
  GetTable(getter_AddRefs(table));
  if (!table)
    return NS_OK;

  return nsAccUtils::GetHeaderCellsFor(table, this,
                                       nsAccUtils::eColumnHeaderCells,
                                       aHeaderCells);
}

NS_IMETHODIMP
ARIAGridCellAccessible::GetRowHeaderCells(nsIArray** aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleTable> table;
  GetTable(getter_AddRefs(table));
  if (!table)
    return NS_OK;

  return nsAccUtils::GetHeaderCellsFor(table, this,
                                       nsAccUtils::eRowHeaderCells,
                                       aHeaderCells);
}

NS_IMETHODIMP
ARIAGridCellAccessible::IsSelected(bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* row = Parent();
  if (!row || row->Role() != roles::ROW)
    return NS_OK;

  if (!nsAccUtils::IsARIASelected(row) && !nsAccUtils::IsARIASelected(this))
    return NS_OK;

  *aIsSelected = true;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

void
ARIAGridCellAccessible::ApplyARIAState(PRUint64* aState) const
{
  HyperTextAccessibleWrap::ApplyARIAState(aState);

  // Return if the gridcell has aria-selected="true".
  if (*aState & states::SELECTED)
    return;

  // Check aria-selected="true" on the row.
  Accessible* row = Parent();
  if (!row || row->Role() != roles::ROW)
    return;

  nsIContent *rowContent = row->GetContent();
  if (nsAccUtils::HasDefinedARIAToken(rowContent,
                                      nsGkAtoms::aria_selected) &&
      !rowContent->AttrValueIs(kNameSpaceID_None,
                               nsGkAtoms::aria_selected,
                               nsGkAtoms::_false, eCaseMatters))
    *aState |= states::SELECTABLE | states::SELECTED;
}

nsresult
ARIAGridCellAccessible::GetAttributesInternal(nsIPersistentProperties* aAttributes)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;
  
  nsresult rv = HyperTextAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  // Expose "table-cell-index" attribute.

  Accessible* thisRow = Parent();
  if (!thisRow || thisRow->Role() != roles::ROW)
    return NS_OK;

  PRInt32 colIdx = 0, colCount = 0;
  PRUint32 childCount = thisRow->ChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = thisRow->GetChildAt(childIdx);
    if (child == this)
      colIdx = colCount;

    roles::Role role = child->Role();
    if (role == roles::GRID_CELL || role == roles::ROWHEADER ||
        role == roles::COLUMNHEADER)
      colCount++;
  }

  Accessible* table = thisRow->Parent();
  if (!table)
    return NS_OK;

  roles::Role tableRole = table->Role();
  if (tableRole != roles::TABLE && tableRole != roles::TREE_TABLE)
    return NS_OK;

  PRInt32 rowIdx = 0;
  childCount = table->ChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = table->GetChildAt(childIdx);
    if (child == thisRow)
      break;

    if (child->Role() == roles::ROW)
      rowIdx++;
  }

  PRInt32 idx = rowIdx * colCount + colIdx;

  nsAutoString stringIdx;
  stringIdx.AppendInt(idx);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::tableCellIndex,
                         stringIdx);

  return NS_OK;
}
