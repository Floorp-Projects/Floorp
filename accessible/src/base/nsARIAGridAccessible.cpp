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


////////////////////////////////////////////////////////////////////////////////
// nsARIAGridAccessible
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Constructor

nsARIAGridAccessible::nsARIAGridAccessible(nsIDOMNode* aDomNode,
                                           nsIWeakReference* aShell) :
  nsAccessibleWrap(aDomNode, aShell)
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
nsARIAGridAccessible::GetColumns(PRInt32 *aColumns)
{
  NS_ENSURE_ARG_POINTER(aColumns);
  *aColumns = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> row, nextChild;
  GetFirstChild(getter_AddRefs(row));
  while (row && nsAccUtils::Role(row) != nsIAccessibleRole::ROLE_ROW) {
    row->GetNextSibling(getter_AddRefs(nextChild));
    row.swap(nextChild);
  }

  if (!row)
    return NS_OK;

  nsCOMPtr<nsIAccessible> cell;
  row->GetFirstChild(getter_AddRefs(cell));
  while (cell) {
    PRUint32 role = nsAccUtils::Role(cell);
    if (role == nsIAccessibleRole::ROLE_GRID_CELL ||
        role == nsIAccessibleRole::ROLE_ROWHEADER ||
        role == nsIAccessibleRole::ROLE_COLUMNHEADER)
      (*aColumns)++;

    cell->GetNextSibling(getter_AddRefs(nextChild));
    cell.swap(nextChild);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  NS_ENSURE_ARG_POINTER(aColumnHeader);
  *aColumnHeader = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: what should we return here?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetRows(PRInt32 *aRows)
{
  NS_ENSURE_ARG_POINTER(aRows);
  *aRows = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> row, nextRow;
  GetFirstChild(getter_AddRefs(row));
  while (row) {
    if (nsAccUtils::Role(row) == nsIAccessibleRole::ROLE_ROW)
      (*aRows)++;

    row->GetNextSibling(getter_AddRefs(nextRow));
    row.swap(nextRow);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetRowHeader(nsIAccessibleTable **aRowHeader)
{
  NS_ENSURE_ARG_POINTER(aRowHeader);
  *aRowHeader = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: what should we return here?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::CellRefAt(PRInt32 aRow, PRInt32 aColumn,
                                nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 rowIdx = aRow + 1;
  nsCOMPtr<nsIAccessible> row, nextChild;
  GetFirstChild(getter_AddRefs(row));
  while (row) {
    if (nsAccUtils::Role(row) == nsIAccessibleRole::ROLE_ROW)
      rowIdx--;

    if (rowIdx == 0)
      break;

    row->GetNextSibling(getter_AddRefs(nextChild));
    row.swap(nextChild);
  }

  NS_ENSURE_ARG(row && rowIdx == 0);

  PRInt32 colIdx = aColumn + 1;
  nsCOMPtr<nsIAccessible> cell;
  row->GetFirstChild(getter_AddRefs(cell));
  while (cell) {
    PRUint32 role = nsAccUtils::Role(cell);
    if (role == nsIAccessibleRole::ROLE_GRID_CELL ||
        role == nsIAccessibleRole::ROLE_ROWHEADER ||
        role == nsIAccessibleRole::ROLE_COLUMNHEADER)
      colIdx--;

    if (colIdx == 0)
      break;

    cell->GetNextSibling(getter_AddRefs(nextChild));
    cell.swap(nextChild);
  }

  NS_ENSURE_ARG(cell && colIdx == 0);

  NS_ADDREF(*aAccessible = cell);
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetIndexAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);
  *aIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aRow >= 0 && aColumn >= 0);

  PRInt32 rowCount = 0;
  GetRows(&rowCount);
  NS_ENSURE_ARG(aRow < rowCount);

  PRInt32 colCount = 0;
  GetColumns(&colCount);
  NS_ENSURE_ARG(aColumn < colCount);

  *aIndex = colCount * aRow + aColumn;
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *aColumn)
{
  NS_ENSURE_ARG_POINTER(aColumn);
  *aColumn = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aIndex >= 0);

  PRInt32 rowCount = 0;
  GetRows(&rowCount);
  
  PRInt32 colCount = 0;
  GetColumns(&colCount);

  NS_ENSURE_ARG(aIndex < rowCount * colCount);

  *aColumn = aIndex % colCount;
  return NS_OK;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetRowAtIndex(PRInt32 aIndex, PRInt32 *aRow)
{
  NS_ENSURE_ARG_POINTER(aRow);
  *aRow = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aIndex >= 0);

  PRInt32 rowCount = 0;
  GetRows(&rowCount);

  PRInt32 colCount = 0;
  GetColumns(&colCount);

  NS_ENSURE_ARG(aIndex < rowCount * colCount);

  *aRow = aIndex / colCount;
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

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::IsRowSelected(PRInt32 aRow, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidRow(aRow));

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::IsCellSelected(PRInt32 aRow, PRInt32 aColumn,
                                     PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(IsValidRowNColumn(aRow, aColumn));

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedCellsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedColumnsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedRowsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedCells(PRUint32 *aCellsCount, PRInt32 **aCells)
{
  NS_ENSURE_ARG_POINTER(aCellsCount);
  *aCellsCount = 0;
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedColumns(PRUint32 *aColumnsCount,
                                         PRInt32 **aColumns)
{
  NS_ENSURE_ARG_POINTER(aColumnsCount);
  *aColumnsCount = 0;
  NS_ENSURE_ARG_POINTER(aColumns);
  *aColumns = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::GetSelectedRows(PRUint32 *aRowsCount, PRInt32 **aRows)
{
  NS_ENSURE_ARG_POINTER(aRowsCount);
  *aRowsCount = 0;
  NS_ENSURE_ARG_POINTER(aRows);
  *aRows = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::SelectRow(PRInt32 aRow)
{
  NS_ENSURE_ARG(IsValidRow(aRow));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::SelectColumn(PRInt32 aColumn)
{
  NS_ENSURE_ARG(IsValidColumn(aColumn));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::UnselectRow(PRInt32 aRow)
{
  NS_ENSURE_ARG(IsValidRow(aRow));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsARIAGridAccessible::UnselectColumn(PRInt32 aColumn)
{
  NS_ENSURE_ARG(IsValidColumn(aColumn));

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // XXX: should we rely on aria-selected or DOM selection?
  return NS_ERROR_NOT_IMPLEMENTED;
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
  GetRows(&rowCount);
  return aRow < rowCount;
}

PRBool
nsARIAGridAccessible::IsValidColumn(PRInt32 aColumn)
{
  if (aColumn < 0)
    return PR_FALSE;

  PRInt32 colCount = 0;
  GetColumns(&colCount);
  return aColumn < colCount;
}

PRBool
nsARIAGridAccessible::IsValidRowNColumn(PRInt32 aRow, PRInt32 aColumn)
{
  if (aRow < 0 || aColumn < 0)
    return PR_FALSE;
  
  PRInt32 rowCount = 0;
  GetRows(&rowCount);
  if (aRow >= rowCount)
    return PR_FALSE;

  PRInt32 colCount = 0;
  GetColumns(&colCount);
  return aColumn < colCount;
}


////////////////////////////////////////////////////////////////////////////////
// nsARIAGridCellAccessible
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Constructor

nsARIAGridCellAccessible::nsARIAGridCellAccessible(nsIDOMNode* aDomNode,
                                                   nsIWeakReference* aShell) :
  nsHyperTextAccessibleWrap(aDomNode, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(nsARIAGridCellAccessible,
                             nsHyperTextAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsAccessible

nsresult
nsARIAGridCellAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;
  
  nsresult rv = nsHyperTextAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  // Expose "table-cell-index" attribute.

  nsCOMPtr<nsIAccessible> thisRow;
  GetParent(getter_AddRefs(thisRow));
  if (nsAccUtils::Role(thisRow) != nsIAccessibleRole::ROLE_ROW)
    return NS_OK;

  PRInt32 colIdx = 0, colCount = 0;
  nsCOMPtr<nsIAccessible> child, nextChild;
  thisRow->GetFirstChild(getter_AddRefs(child));
  while (child) {
    if (child == this)
      colIdx = colCount;

    PRUint32 role = nsAccUtils::Role(child);
    if (role == nsIAccessibleRole::ROLE_GRID_CELL ||
        role == nsIAccessibleRole::ROLE_ROWHEADER ||
        role == nsIAccessibleRole::ROLE_COLUMNHEADER)
      colCount++;

    child->GetNextSibling(getter_AddRefs(nextChild));
    child.swap(nextChild);
  }

  nsCOMPtr<nsIAccessible> table;
  thisRow->GetParent(getter_AddRefs(table));
  if (nsAccUtils::Role(table) != nsIAccessibleRole::ROLE_TABLE &&
      nsAccUtils::Role(table) != nsIAccessibleRole::ROLE_TREE_TABLE)
    return NS_OK;

  PRInt32 rowIdx = 0;
  table->GetFirstChild(getter_AddRefs(child));
  while (child && child != thisRow) {
    if (nsAccUtils::Role(child) == nsIAccessibleRole::ROLE_ROW)
      rowIdx++;

    child->GetNextSibling(getter_AddRefs(nextChild));
    child.swap(nextChild);
  }

  PRInt32 idx = rowIdx * colCount + colIdx;

  nsAutoString stringIdx;
  stringIdx.AppendInt(idx);
  nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::tableCellIndex,
                         stringIdx);

  return NS_OK;
}
