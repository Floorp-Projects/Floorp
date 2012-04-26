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
 * Mozilla Foundation.
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
  ARIAGridAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc), xpcAccessibleTable(this)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(ARIAGridAccessible,
                             nsAccessible,
                             nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
//nsAccessNode

void
ARIAGridAccessible::Shutdown()
{
  mTable = nsnull;
  nsAccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleTable

PRUint32
ARIAGridAccessible::ColCount()
{
  AccIterator rowIter(this, filters::GetRow);
  nsAccessible* row = rowIter.Next();
  if (!row)
    return 0;

  AccIterator cellIter(row, filters::GetCell);
  nsAccessible* cell = nsnull;

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

NS_IMETHODIMP
ARIAGridAccessible::GetCellAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                              nsIAccessible** aCell)
{
  NS_ENSURE_ARG_POINTER(aCell);
  *aCell = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible *row = GetRowAt(aRowIndex);
  NS_ENSURE_ARG(row);

  nsAccessible *cell = GetCellInRowAt(row, aColumnIndex);
  NS_ENSURE_ARG(cell);

  NS_ADDREF(*aCell = cell);
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetCellIndexAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                   PRInt32* aCellIndex)
{
  NS_ENSURE_ARG_POINTER(aCellIndex);
  *aCellIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aRowIndex >= 0 && aColumnIndex >= 0);

  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);
  NS_ENSURE_ARG(aRowIndex < rowCount);

  PRInt32 colsCount = 0;
  GetColumnCount(&colsCount);
  NS_ENSURE_ARG(aColumnIndex < colsCount);

  *aCellIndex = colsCount * aRowIndex + aColumnIndex;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetColumnIndexAt(PRInt32 aCellIndex,
                                     PRInt32* aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aCellIndex >= 0);

  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);

  PRInt32 colsCount = 0;
  GetColumnCount(&colsCount);

  NS_ENSURE_ARG(aCellIndex < rowCount * colsCount);

  *aColumnIndex = aCellIndex % colsCount;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetRowIndexAt(PRInt32 aCellIndex, PRInt32* aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aCellIndex >= 0);

  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);

  PRInt32 colsCount = 0;
  GetColumnCount(&colsCount);

  NS_ENSURE_ARG(aCellIndex < rowCount * colsCount);

  *aRowIndex = aCellIndex / colsCount;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetRowAndColumnIndicesAt(PRInt32 aCellIndex,
                                             PRInt32* aRowIndex,
                                             PRInt32* aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aCellIndex >= 0);

  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);

  PRInt32 colsCount = 0;
  GetColumnCount(&colsCount);

  NS_ENSURE_ARG(aCellIndex < rowCount * colsCount);

  *aColumnIndex = aCellIndex % colsCount;
  *aRowIndex = aCellIndex / colsCount;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                   PRInt32* aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidRowNColumn(aRow, aColumn));

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                      PRInt32* aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidRowNColumn(aRow, aColumn));

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetColumnDescription(PRInt32 aColumn,
                                         nsAString& aDescription)
{
  aDescription.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidColumn(aColumn));

  // XXX: not implemented
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ARIAGridAccessible::GetRowDescription(PRInt32 aRow, nsAString& aDescription)
{
  aDescription.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidRow(aRow));

  // XXX: not implemented
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ARIAGridAccessible::IsColumnSelected(PRInt32 aColumn, bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidColumn(aColumn));

  AccIterator rowIter(this, filters::GetRow);
  nsAccessible *row = rowIter.Next();
  if (!row)
    return NS_OK;

  do {
    if (!nsAccUtils::IsARIASelected(row)) {
      nsAccessible *cell = GetCellInRowAt(row, aColumn);
      if (!cell) // Do not fail due to wrong markup
        return NS_OK;
      
      if (!nsAccUtils::IsARIASelected(cell))
        return NS_OK;
    }
  } while ((row = rowIter.Next()));

  *aIsSelected = true;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::IsRowSelected(PRInt32 aRow, bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible *row = GetRowAt(aRow);
  NS_ENSURE_ARG(row);

  if (!nsAccUtils::IsARIASelected(row)) {
    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;
    while ((cell = cellIter.Next())) {
      if (!nsAccUtils::IsARIASelected(cell))
        return NS_OK;
    }
  }

  *aIsSelected = true;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::IsCellSelected(PRInt32 aRow, PRInt32 aColumn,
                                   bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible *row = GetRowAt(aRow);
  NS_ENSURE_ARG(row);

  if (!nsAccUtils::IsARIASelected(row)) {
    nsAccessible *cell = GetCellInRowAt(row, aColumn);
    NS_ENSURE_ARG(cell);

    if (!nsAccUtils::IsARIASelected(cell))
      return NS_OK;
  }

  *aIsSelected = true;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetSelectedCellCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  while ((row = rowIter.Next())) {
    if (nsAccUtils::IsARIASelected(row)) {
      (*aCount) += colCount;
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;

    while ((cell = cellIter.Next())) {
      if (nsAccUtils::IsARIASelected(cell))
        (*aCount)++;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetSelectedColumnCount(PRUint32* aCount)
{
  return GetSelectedColumnsArray(aCount);
}

NS_IMETHODIMP
ARIAGridAccessible::GetSelectedRowCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  while ((row = rowIter.Next())) {
    if (nsAccUtils::IsARIASelected(row)) {
      (*aCount)++;
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = cellIter.Next();
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
      (*aCount)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetSelectedCells(nsIArray** aCells)
{
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> selCells =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  while ((row = rowIter.Next())) {
    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;

    if (nsAccUtils::IsARIASelected(row)) {
      while ((cell = cellIter.Next()))
        selCells->AppendElement(static_cast<nsIAccessible *>(cell), false);

      continue;
    }

    while ((cell = cellIter.Next())) {
      if (nsAccUtils::IsARIASelected(cell))
        selCells->AppendElement(static_cast<nsIAccessible *>(cell), false);
    }
  }

  NS_ADDREF(*aCells = selCells);
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetSelectedCellIndices(PRUint32* aCellsCount,
                                           PRInt32** aCells)
{
  NS_ENSURE_ARG_POINTER(aCellsCount);
  *aCellsCount = 0;
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);

  nsTArray<PRInt32> selCells(rowCount * colCount);

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  for (PRInt32 rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    if (nsAccUtils::IsARIASelected(row)) {
      for (PRInt32 colIdx = 0; colIdx < colCount; colIdx++)
        selCells.AppendElement(rowIdx * colCount + colIdx);

      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;

    for (PRInt32 colIdx = 0; (cell = cellIter.Next()); colIdx++) {
      if (nsAccUtils::IsARIASelected(cell))
        selCells.AppendElement(rowIdx * colCount + colIdx);
    }
  }

  PRUint32 selCellsCount = selCells.Length();
  if (!selCellsCount)
    return NS_OK;

  *aCells = static_cast<PRInt32*>(
    nsMemory::Clone(selCells.Elements(), selCellsCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(*aCells, NS_ERROR_OUT_OF_MEMORY);

  *aCellsCount = selCellsCount;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::GetSelectedColumnIndices(PRUint32* aColumnCount,
                                             PRInt32** aColumns)
{
  NS_ENSURE_ARG_POINTER(aColumns);

  return GetSelectedColumnsArray(aColumnCount, aColumns);
}

NS_IMETHODIMP
ARIAGridAccessible::GetSelectedRowIndices(PRUint32* aRowCount,
                                          PRInt32** aRows)
{
  NS_ENSURE_ARG_POINTER(aRowCount);
  *aRowCount = 0;
  NS_ENSURE_ARG_POINTER(aRows);
  *aRows = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);
  if (!rowCount)
    return NS_OK;

  nsTArray<PRInt32> selRows(rowCount);

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  for (PRInt32 rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    if (nsAccUtils::IsARIASelected(row)) {
      selRows.AppendElement(rowIdx);
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = cellIter.Next();
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
      selRows.AppendElement(rowIdx);
  }

  PRUint32 selrowCount = selRows.Length();
  if (!selrowCount)
    return NS_OK;

  *aRows = static_cast<PRInt32*>(
    nsMemory::Clone(selRows.Elements(), selrowCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(*aRows, NS_ERROR_OUT_OF_MEMORY);

  *aRowCount = selrowCount;
  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::SelectRow(PRInt32 aRow)
{
  NS_ENSURE_ARG(IsValidRow(aRow));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  for (PRInt32 rowIdx = 0; (row = rowIter.Next()); rowIdx++) {
    nsresult rv = SetARIASelected(row, rowIdx == aRow);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
ARIAGridAccessible::SelectColumn(PRInt32 aColumn)
{
  NS_ENSURE_ARG(IsValidColumn(aColumn));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  while ((row = rowIter.Next())) {
    // Unselect all cells in the row.
    nsresult rv = SetARIASelected(row, false);
    NS_ENSURE_SUCCESS(rv, rv);

    // Select cell at the column index.
    nsAccessible *cell = GetCellInRowAt(row, aColumn);
    if (cell) {
      rv = SetARIASelected(cell, true);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

void
ARIAGridAccessible::UnselectRow(PRUint32 aRowIdx)
{
  nsAccessible* row = GetRowAt(aRowIdx);

  if (row)
    SetARIASelected(row, false);
}

void
ARIAGridAccessible::UnselectCol(PRUint32 aColIdx)
{
  AccIterator rowIter(this, filters::GetRow);

  nsAccessible* row = nsnull;
  while ((row = rowIter.Next())) {
    nsAccessible* cell = GetCellInRowAt(row, aColIdx);
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

bool
ARIAGridAccessible::IsValidRowNColumn(PRInt32 aRow, PRInt32 aColumn)
{
  if (aRow < 0 || aColumn < 0)
    return false;
  
  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);
  if (aRow >= rowCount)
    return false;

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);
  return aColumn < colCount;
}

nsAccessible*
ARIAGridAccessible::GetRowAt(PRInt32 aRow)
{
  PRInt32 rowIdx = aRow;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = rowIter.Next();
  while (rowIdx != 0 && (row = rowIter.Next()))
    rowIdx--;

  return row;
}

nsAccessible*
ARIAGridAccessible::GetCellInRowAt(nsAccessible* aRow, PRInt32 aColumn)
{
  PRInt32 colIdx = aColumn;

  AccIterator cellIter(aRow, filters::GetCell);
  nsAccessible *cell = cellIter.Next();
  while (colIdx != 0 && (cell = cellIter.Next()))
    colIdx--;

  return cell;
}

nsresult
ARIAGridAccessible::SetARIASelected(nsAccessible* aAccessible,
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
    nsAccessible *cell = nsnull;

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
    nsAccessible* row = aAccessible->Parent();

    if (row && row->Role() == roles::ROW &&
        nsAccUtils::IsARIASelected(row)) {
      rv = SetARIASelected(row, false, false);
      NS_ENSURE_SUCCESS(rv, rv);

      AccIterator cellIter(row, filters::GetCell);
      nsAccessible *cell = nsnull;
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

nsresult
ARIAGridAccessible::GetSelectedColumnsArray(PRUint32* aColumnCount,
                                            PRInt32** aColumns)
{
  NS_ENSURE_ARG_POINTER(aColumnCount);
  *aColumnCount = 0;
  if (aColumns)
    *aColumns = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);
  nsAccessible *row = rowIter.Next();
  if (!row)
    return NS_OK;

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);
  if (!colCount)
    return NS_OK;

  PRInt32 selColCount = colCount;

  nsTArray<bool> isColSelArray(selColCount);
  isColSelArray.AppendElements(selColCount);
  for (PRInt32 i = 0; i < selColCount; i++)
    isColSelArray[i] = true;

  do {
    if (nsAccUtils::IsARIASelected(row))
      continue;

    PRInt32 colIdx = 0;

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;
    for (colIdx = 0; (cell = cellIter.Next()); colIdx++) {
      if (isColSelArray.SafeElementAt(colIdx, false) &&
          !nsAccUtils::IsARIASelected(cell)) {
        isColSelArray[colIdx] = false;
        selColCount--;
      }
    }
  } while ((row = rowIter.Next()));

  if (!selColCount)
    return NS_OK;

  if (!aColumns) {
    *aColumnCount = selColCount;
    return NS_OK;
  }

  *aColumns = static_cast<PRInt32*>(
    nsMemory::Alloc(selColCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(*aColumns, NS_ERROR_OUT_OF_MEMORY);

  *aColumnCount = selColCount;
  for (PRInt32 colIdx = 0, idx = 0; colIdx < colCount; colIdx++) {
    if (isColSelArray[colIdx])
      (*aColumns)[idx++] = colIdx;
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// ARIAGridCellAccessible
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Constructor

ARIAGridCellAccessible::
  ARIAGridCellAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsHyperTextAccessibleWrap(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(ARIAGridCellAccessible,
                             nsHyperTextAccessible,
                             nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleTableCell

NS_IMETHODIMP
ARIAGridCellAccessible::GetTable(nsIAccessibleTable** aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  nsAccessible* thisRow = Parent();
  if (!thisRow || thisRow->Role() != roles::ROW)
    return NS_OK;

  nsAccessible* table = thisRow->Parent();
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

  nsAccessible* row = Parent();
  if (!row)
    return NS_OK;

  *aColumnIndex = 0;

  PRInt32 indexInRow = IndexInParent();
  for (PRInt32 idx = 0; idx < indexInRow; idx++) {
    nsAccessible* cell = row->GetChildAt(idx);
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

  nsAccessible* row = Parent();
  if (!row)
    return NS_OK;

  nsAccessible* table = row->Parent();
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

  nsAccessible* row = Parent();
  if (!row || row->Role() != roles::ROW)
    return NS_OK;

  if (!nsAccUtils::IsARIASelected(row) && !nsAccUtils::IsARIASelected(this))
    return NS_OK;

  *aIsSelected = true;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible

void
ARIAGridCellAccessible::ApplyARIAState(PRUint64* aState)
{
  nsHyperTextAccessibleWrap::ApplyARIAState(aState);

  // Return if the gridcell has aria-selected="true".
  if (*aState & states::SELECTED)
    return;

  // Check aria-selected="true" on the row.
  nsAccessible* row = Parent();
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
  
  nsresult rv = nsHyperTextAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  // Expose "table-cell-index" attribute.

  nsAccessible* thisRow = Parent();
  if (!thisRow || thisRow->Role() != roles::ROW)
    return NS_OK;

  PRInt32 colIdx = 0, colCount = 0;
  PRInt32 childCount = thisRow->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible *child = thisRow->GetChildAt(childIdx);
    if (child == this)
      colIdx = colCount;

    roles::Role role = child->Role();
    if (role == roles::GRID_CELL || role == roles::ROWHEADER ||
        role == roles::COLUMNHEADER)
      colCount++;
  }

  nsAccessible* table = thisRow->Parent();
  if (!table)
    return NS_OK;

  roles::Role tableRole = table->Role();
  if (tableRole != roles::TABLE && tableRole != roles::TREE_TABLE)
    return NS_OK;

  PRInt32 rowIdx = 0;
  childCount = table->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible *child = table->GetChildAt(childIdx);
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
