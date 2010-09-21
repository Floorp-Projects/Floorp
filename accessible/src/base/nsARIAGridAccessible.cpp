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

#include "nsARIAGridAccessible.h"

#include "AccIterator.h"
#include "nsAccUtils.h"

#include "nsIMutableArray.h"
#include "nsComponentManagerUtils.h"

////////////////////////////////////////////////////////////////////////////////
// nsARIAGridAccessible
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Constructor

nsARIAGridAccessible::
  nsARIAGridAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(nsARIAGridAccessible,
                             nsAccessible,
                             nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleTable

NS_IMETHODIMP
nsARIAGridAccessible::GetCaption(nsIAccessible **aCaption)
{
  NS_ENSURE_ARG_POINTER(aCaption);
  *aCaption = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should be pointed by aria-labelledby on grid?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSummary(nsAString &aSummary)
{
  aSummary.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should be pointed by aria-describedby on grid?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetColumnCount(PRInt32 *acolumnCount)
{
  NS_ENSURE_ARG_POINTER(acolumnCount);
  *acolumnCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);
  nsAccessible *row = rowIter.GetNext();

  AccIterator cellIter(row, filters::GetCell);
  nsAccessible *cell = nsnull;

  while ((cell = cellIter.GetNext()))
    (*acolumnCount)++;

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetRowCount(PRInt32 *arowCount)
{
  NS_ENSURE_ARG_POINTER(arowCount);
  *arowCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);
  while (rowIter.GetNext())
    (*arowCount)++;

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetCellAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible *row = GetRowAt(aRowIndex);
  NS_ENSURE_ARG(row);

  nsAccessible *cell = GetCellInRowAt(row, aColumnIndex);
  NS_ENSURE_ARG(cell);

  NS_ADDREF(*aAccessible = cell);
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetCellIndexAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                     PRInt32 *aCellIndex)
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
nsARIAGridAccessible::GetColumnIndexAt(PRInt32 aCellIndex,
                                       PRInt32 *aColumnIndex)
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
nsARIAGridAccessible::GetRowIndexAt(PRInt32 aCellIndex, PRInt32 *aRowIndex)
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
nsARIAGridAccessible::GetRowAndColumnIndicesAt(PRInt32 aCellIndex,
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
nsARIAGridAccessible::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                        PRInt32 *aExtentCount)
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
nsARIAGridAccessible::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                      PRInt32 *aExtentCount)
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
nsARIAGridAccessible::GetColumnDescription(PRInt32 aColumn,
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
nsARIAGridAccessible::GetRowDescription(PRInt32 aRow, nsAString& aDescription)
{
  aDescription.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidRow(aRow));

  // XXX: not implemented
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::IsColumnSelected(PRInt32 aColumn, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidColumn(aColumn));

  AccIterator rowIter(this, filters::GetRow);
  nsAccessible *row = rowIter.GetNext();
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
  } while ((row = rowIter.GetNext()));

  *aIsSelected = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::IsRowSelected(PRInt32 aRow, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible *row = GetRowAt(aRow);
  NS_ENSURE_ARG(row);

  if (!nsAccUtils::IsARIASelected(row)) {
    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;
    while ((cell = cellIter.GetNext())) {
      if (!nsAccUtils::IsARIASelected(cell))
        return NS_OK;
    }
  }

  *aIsSelected = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::IsCellSelected(PRInt32 aRow, PRInt32 aColumn,
                                     PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

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

  *aIsSelected = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedCellCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  while ((row = rowIter.GetNext())) {
    if (nsAccUtils::IsARIASelected(row)) {
      (*aCount) += colCount;
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;

    while ((cell = cellIter.GetNext())) {
      if (nsAccUtils::IsARIASelected(cell))
        (*aCount)++;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedColumnCount(PRUint32* aCount)
{
  return GetSelectedColumnsArray(aCount);
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedRowCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  while ((row = rowIter.GetNext())) {
    if (nsAccUtils::IsARIASelected(row)) {
      (*aCount)++;
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = cellIter.GetNext();
    if (!cell)
      continue;

    PRBool isRowSelected = PR_TRUE;
    do {
      if (!nsAccUtils::IsARIASelected(cell)) {
        isRowSelected = PR_FALSE;
        break;
      }
    } while ((cell = cellIter.GetNext()));

    if (isRowSelected)
      (*aCount)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedCells(nsIArray **aCells)
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
  while ((row = rowIter.GetNext())) {
    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;

    if (nsAccUtils::IsARIASelected(row)) {
      while ((cell = cellIter.GetNext()))
        selCells->AppendElement(static_cast<nsIAccessible *>(cell), PR_FALSE);

      continue;
    }

    while ((cell = cellIter.GetNext())) {
      if (nsAccUtils::IsARIASelected(cell))
        selCells->AppendElement(static_cast<nsIAccessible *>(cell), PR_FALSE);
    }
  }

  NS_ADDREF(*aCells = selCells);
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedCellIndices(PRUint32 *aCellsCount,
                                             PRInt32 **aCells)
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
  for (PRInt32 rowIdx = 0; (row = rowIter.GetNext()); rowIdx++) {
    if (nsAccUtils::IsARIASelected(row)) {
      for (PRInt32 colIdx = 0; colIdx < colCount; colIdx++)
        selCells.AppendElement(rowIdx * colCount + colIdx);

      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;

    for (PRInt32 colIdx = 0; (cell = cellIter.GetNext()); colIdx++) {
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
nsARIAGridAccessible::GetSelectedColumnIndices(PRUint32 *acolumnCount,
                                               PRInt32 **aColumns)
{
  NS_ENSURE_ARG_POINTER(aColumns);

  return GetSelectedColumnsArray(acolumnCount, aColumns);
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedRowIndices(PRUint32 *arowCount,
                                            PRInt32 **aRows)
{
  NS_ENSURE_ARG_POINTER(arowCount);
  *arowCount = 0;
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
  for (PRInt32 rowIdx = 0; (row = rowIter.GetNext()); rowIdx++) {
    if (nsAccUtils::IsARIASelected(row)) {
      selRows.AppendElement(rowIdx);
      continue;
    }

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = cellIter.GetNext();
    if (!cell)
      continue;

    PRBool isRowSelected = PR_TRUE;
    do {
      if (!nsAccUtils::IsARIASelected(cell)) {
        isRowSelected = PR_FALSE;
        break;
      }
    } while ((cell = cellIter.GetNext()));

    if (isRowSelected)
      selRows.AppendElement(rowIdx);
  }

  PRUint32 selrowCount = selRows.Length();
  if (!selrowCount)
    return NS_OK;

  *aRows = static_cast<PRInt32*>(
    nsMemory::Clone(selRows.Elements(), selrowCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(*aRows, NS_ERROR_OUT_OF_MEMORY);

  *arowCount = selrowCount;
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::SelectRow(PRInt32 aRow)
{
  NS_ENSURE_ARG(IsValidRow(aRow));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  for (PRInt32 rowIdx = 0; (row = rowIter.GetNext()); rowIdx++) {
    nsresult rv = SetARIASelected(row, rowIdx == aRow);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::SelectColumn(PRInt32 aColumn)
{
  NS_ENSURE_ARG(IsValidColumn(aColumn));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  while ((row = rowIter.GetNext())) {
    // Unselect all cells in the row.
    nsresult rv = SetARIASelected(row, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Select cell at the column index.
    nsAccessible *cell = GetCellInRowAt(row, aColumn);
    if (cell) {
      rv = SetARIASelected(cell, PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::UnselectRow(PRInt32 aRow)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible *row = GetRowAt(aRow);
  NS_ENSURE_ARG(row);

  return SetARIASelected(row, PR_FALSE);
}

NS_IMETHODIMP
nsARIAGridAccessible::UnselectColumn(PRInt32 aColumn)
{
  NS_ENSURE_ARG(IsValidColumn(aColumn));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = nsnull;
  while ((row = rowIter.GetNext())) {
    nsAccessible *cell = GetCellInRowAt(row, aColumn);
    if (cell) {
      nsresult rv = SetARIASelected(cell, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::IsProbablyForLayout(PRBool *aIsProbablyForLayout)
{
  NS_ENSURE_ARG_POINTER(aIsProbablyForLayout);
  *aIsProbablyForLayout = PR_FALSE;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Protected

PRBool
nsARIAGridAccessible::IsValidRow(PRInt32 aRow)
{
  if (aRow < 0)
    return PR_FALSE;
  
  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);
  return aRow < rowCount;
}

PRBool
nsARIAGridAccessible::IsValidColumn(PRInt32 aColumn)
{
  if (aColumn < 0)
    return PR_FALSE;

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);
  return aColumn < colCount;
}

PRBool
nsARIAGridAccessible::IsValidRowNColumn(PRInt32 aRow, PRInt32 aColumn)
{
  if (aRow < 0 || aColumn < 0)
    return PR_FALSE;
  
  PRInt32 rowCount = 0;
  GetRowCount(&rowCount);
  if (aRow >= rowCount)
    return PR_FALSE;

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);
  return aColumn < colCount;
}

nsAccessible*
nsARIAGridAccessible::GetRowAt(PRInt32 aRow)
{
  PRInt32 rowIdx = aRow;

  AccIterator rowIter(this, filters::GetRow);

  nsAccessible *row = rowIter.GetNext();
  while (rowIdx != 0 && (row = rowIter.GetNext()))
    rowIdx--;

  return row;
}

nsAccessible*
nsARIAGridAccessible::GetCellInRowAt(nsAccessible *aRow, PRInt32 aColumn)
{
  PRInt32 colIdx = aColumn;

  AccIterator cellIter(aRow, filters::GetCell);
  nsAccessible *cell = cellIter.GetNext();
  while (colIdx != 0 && (cell = cellIter.GetNext()))
    colIdx--;

  return cell;
}

nsresult
nsARIAGridAccessible::SetARIASelected(nsAccessible *aAccessible,
                                      PRBool aIsSelected, PRBool aNotify)
{
  nsIContent *content = aAccessible->GetContent();
  NS_ENSURE_STATE(content);

  nsresult rv = NS_OK;
  if (aIsSelected)
    rv = content->SetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_selected,
                          NS_LITERAL_STRING("true"), aNotify);
  else
    rv = content->UnsetAttr(kNameSpaceID_None,
                            nsAccessibilityAtoms::aria_selected, aNotify);

  NS_ENSURE_SUCCESS(rv, rv);

  // No "smart" select/unselect for internal call.
  if (!aNotify)
    return NS_OK;

  // If row or cell accessible was selected then we're able to not bother about
  // selection of its cells or its row because our algorithm is row oriented,
  // i.e. we check selection on row firstly and then on cells.
  if (aIsSelected)
    return NS_OK;

  PRUint32 role = aAccessible->Role();

  // If the given accessible is row that was unselected then remove
  // aria-selected from cell accessible.
  if (role == nsIAccessibleRole::ROLE_ROW) {
    AccIterator cellIter(aAccessible, filters::GetCell);
    nsAccessible *cell = nsnull;

    while ((cell = cellIter.GetNext())) {
      rv = SetARIASelected(cell, PR_FALSE, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }

  // If the given accessible is cell that was unselected and its row is selected
  // then remove aria-selected from row and put aria-selected on
  // siblings cells.
  if (role == nsIAccessibleRole::ROLE_GRID_CELL ||
      role == nsIAccessibleRole::ROLE_ROWHEADER ||
      role == nsIAccessibleRole::ROLE_COLUMNHEADER) {
    nsAccessible *row = aAccessible->GetParent();

    if (row && row->Role() == nsIAccessibleRole::ROLE_ROW &&
        nsAccUtils::IsARIASelected(row)) {
      rv = SetARIASelected(row, PR_FALSE, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      AccIterator cellIter(row, filters::GetCell);
      nsAccessible *cell = nsnull;
      while ((cell = cellIter.GetNext())) {
        if (cell != aAccessible) {
          rv = SetARIASelected(cell, PR_TRUE, PR_FALSE);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsARIAGridAccessible::GetSelectedColumnsArray(PRUint32 *acolumnCount,
                                              PRInt32 **aColumns)
{
  NS_ENSURE_ARG_POINTER(acolumnCount);
  *acolumnCount = 0;
  if (aColumns)
    *aColumns = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccIterator rowIter(this, filters::GetRow);
  nsAccessible *row = rowIter.GetNext();
  if (!row)
    return NS_OK;

  PRInt32 colCount = 0;
  GetColumnCount(&colCount);
  if (!colCount)
    return NS_OK;

  PRInt32 selColCount = colCount;

  nsTArray<PRBool> isColSelArray(selColCount);
  isColSelArray.AppendElements(selColCount);
  for (PRInt32 i = 0; i < selColCount; i++)
    isColSelArray[i] = PR_TRUE;

  do {
    if (nsAccUtils::IsARIASelected(row))
      continue;

    PRInt32 colIdx = 0;

    AccIterator cellIter(row, filters::GetCell);
    nsAccessible *cell = nsnull;
    for (colIdx = 0; (cell = cellIter.GetNext()); colIdx++) {
      if (isColSelArray.SafeElementAt(colIdx, PR_FALSE) &&
          !nsAccUtils::IsARIASelected(cell)) {
        isColSelArray[colIdx] = PR_FALSE;
        selColCount--;
      }
    }
  } while ((row = rowIter.GetNext()));

  if (!selColCount)
    return NS_OK;

  if (!aColumns) {
    *acolumnCount = selColCount;
    return NS_OK;
  }

  *aColumns = static_cast<PRInt32*>(
    nsMemory::Alloc(selColCount * sizeof(PRInt32)));
  NS_ENSURE_TRUE(*aColumns, NS_ERROR_OUT_OF_MEMORY);

  *acolumnCount = selColCount;
  for (PRInt32 colIdx = 0, idx = 0; colIdx < colCount; colIdx++) {
    if (isColSelArray[colIdx])
      (*aColumns)[idx++] = colIdx;
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsARIAGridCellAccessible
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Constructor

nsARIAGridCellAccessible::
  nsARIAGridCellAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(nsARIAGridCellAccessible,
                             nsHyperTextAccessible,
                             nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleTableCell

NS_IMETHODIMP
nsARIAGridCellAccessible::GetTable(nsIAccessibleTable **aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  nsAccessible* thisRow = GetParent();
  if (!thisRow || thisRow->Role() != nsIAccessibleRole::ROLE_ROW)
    return NS_OK;

  nsAccessible* table = thisRow->GetParent();
  if (!table)
    return NS_OK;

  PRUint32 tableRole = table->Role();
  if (tableRole != nsIAccessibleRole::ROLE_TABLE &&
      tableRole != nsIAccessibleRole::ROLE_TREE_TABLE)
    return NS_OK;

  CallQueryInterface(table, aTable);
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridCellAccessible::GetColumnIndex(PRInt32 *aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible* row = GetParent();
  if (!row)
    return NS_OK;

  *aColumnIndex = 0;

  PRInt32 indexInRow = GetIndexInParent();
  for (PRInt32 idx = 0; idx < indexInRow; idx++) {
    nsAccessible* cell = row->GetChildAt(idx);
    PRUint32 role = cell->Role();
    if (role == nsIAccessibleRole::ROLE_GRID_CELL ||
        role == nsIAccessibleRole::ROLE_ROWHEADER ||
        role == nsIAccessibleRole::ROLE_COLUMNHEADER)
      (*aColumnIndex)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridCellAccessible::GetRowIndex(PRInt32 *aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible* row = GetParent();
  if (!row)
    return NS_OK;

  nsAccessible* table = row->GetParent();
  if (!table)
    return NS_OK;

  *aRowIndex = 0;

  PRInt32 indexInTable = row->GetIndexInParent();
  for (PRInt32 idx = 0; idx < indexInTable; idx++) {
    row = table->GetChildAt(idx);
    if (row->Role() == nsIAccessibleRole::ROLE_ROW)
      (*aRowIndex)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridCellAccessible::GetColumnExtent(PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridCellAccessible::GetRowExtent(PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aExtentCount = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridCellAccessible::GetColumnHeaderCells(nsIArray **aHeaderCells)
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
nsARIAGridCellAccessible::GetRowHeaderCells(nsIArray **aHeaderCells)
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
nsARIAGridCellAccessible::IsSelected(PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible *row = GetParent();
  if (!row || row->Role() != nsIAccessibleRole::ROLE_ROW)
    return NS_OK;

  if (!nsAccUtils::IsARIASelected(row) && !nsAccUtils::IsARIASelected(this))
    return NS_OK;

  *aIsSelected = PR_TRUE;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible

nsresult
nsARIAGridCellAccessible::GetARIAState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetARIAState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Return if the gridcell has aria-selected="true".
  if (*aState & nsIAccessibleStates::STATE_SELECTED)
    return NS_OK;

  // Check aria-selected="true" on the row.
  nsAccessible* row = GetParent();
  if (!row || row->Role() != nsIAccessibleRole::ROLE_ROW)
    return NS_OK;

  nsIContent *rowContent = row->GetContent();
  if (nsAccUtils::HasDefinedARIAToken(rowContent,
                                      nsAccessibilityAtoms::aria_selected) &&
      !rowContent->AttrValueIs(kNameSpaceID_None,
                               nsAccessibilityAtoms::aria_selected,
                               nsAccessibilityAtoms::_false, eCaseMatters)) {

    *aState |= nsIAccessibleStates::STATE_SELECTABLE |
      nsIAccessibleStates::STATE_SELECTED;
  }

  return NS_OK;
}

nsresult
nsARIAGridCellAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;
  
  nsresult rv = nsHyperTextAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  // Expose "table-cell-index" attribute.

  nsAccessible* thisRow = GetParent();
  if (!thisRow || thisRow->Role() != nsIAccessibleRole::ROLE_ROW)
    return NS_OK;

  PRInt32 colIdx = 0, colCount = 0;
  PRInt32 childCount = thisRow->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible *child = thisRow->GetChildAt(childIdx);
    if (child == this)
      colIdx = colCount;

    PRUint32 role = child->Role();
    if (role == nsIAccessibleRole::ROLE_GRID_CELL ||
        role == nsIAccessibleRole::ROLE_ROWHEADER ||
        role == nsIAccessibleRole::ROLE_COLUMNHEADER)
      colCount++;
  }

  nsAccessible* table = thisRow->GetParent();
  if (!table)
    return NS_OK;

  PRUint32 tableRole = table->Role();
  if (tableRole != nsIAccessibleRole::ROLE_TABLE &&
      tableRole != nsIAccessibleRole::ROLE_TREE_TABLE)
    return NS_OK;

  PRInt32 rowIdx = 0;
  childCount = table->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible *child = table->GetChildAt(childIdx);
    if (child == thisRow)
      break;

    if (child->Role() == nsIAccessibleRole::ROLE_ROW)
      rowIdx++;
  }

  PRInt32 idx = rowIdx * colCount + colIdx;

  nsAutoString stringIdx;
  stringIdx.AppendInt(idx);
  nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::tableCellIndex,
                         stringIdx);

  return NS_OK;
}
