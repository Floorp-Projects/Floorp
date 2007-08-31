/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "CAccessibleTable.h"

#include "Accessible2.h"
#include "AccessibleTable_i.c"

#include "nsIAccessible.h"
#include "nsIAccessibleTable.h"
#include "nsIWinAccessNode.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#define CANT_QUERY_ASSERTION_MSG \
"Subclass of CAccessibleTable doesn't implement nsIAccessibleTable"\

// IUnknown

STDMETHODIMP
CAccessibleTable::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleTable == iid) {
    *ppv = static_cast<IAccessibleTable*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleTable

STDMETHODIMP
CAccessibleTable::get_accessibleAt(long aRow, long aColumn,
                                   IUnknown **aAccessible)
{
  *aAccessible = NULL;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsCOMPtr<nsIAccessible> cell;
  tableAcc->CellRefAt(aRow, aColumn, getter_AddRefs(cell));

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(cell));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  nsresult rv = winAccessNode->QueryNativeInterface(IID_IAccessible2,
                                                    &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aAccessible = static_cast<IUnknown*>(instancePtr);
  return S_OK;
}

STDMETHODIMP
CAccessibleTable::get_caption(IUnknown **aAccessible)
{
  *aAccessible = NULL;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsCOMPtr<nsIAccessible> caption;
  tableAcc->GetCaption(getter_AddRefs(caption));

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(caption));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  nsresult rv = winAccessNode->QueryNativeInterface(IID_IAccessible2,
                                                    &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aAccessible = static_cast<IUnknown*>(instancePtr);
  return S_OK;
}

STDMETHODIMP
CAccessibleTable::get_childIndex(long aRowIndex, long aColumnIndex,
                                 long *aChildIndex)
{
  *aChildIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 childIndex = 0;
  nsresult rv = tableAcc->GetIndexAt(aRowIndex, aColumnIndex, &childIndex);
  *aChildIndex = childIndex;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_columnDescription(long aColumn, BSTR *aDescription)
{
  *aDescription = NULL;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsAutoString descr;
  nsresult rv = tableAcc->GetColumnDescription (aColumn, descr);
  if (NS_FAILED(rv))
    return E_FAIL;

  INT result = ::SysReAllocStringLen(aDescription, descr.get(), descr.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
CAccessibleTable::get_columnExtentAt(long aRow, long aColumn,
                                     long *nColumnsSpanned)
{
  *nColumnsSpanned = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 columnsSpanned = 0;
  nsresult rv = tableAcc->GetColumnExtentAt(aRow, aColumn, &columnsSpanned);
  *nColumnsSpanned = columnsSpanned;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_columnHeader(IAccessibleTable **aAccessibleTable,
                                   long *aStartingRowIndex)
{
  *aAccessibleTable = NULL;

  // XXX: starting row index is always 0.
  *aStartingRowIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsCOMPtr<nsIAccessibleTable> header;
  nsresult rv = tableAcc->GetColumnHeader(getter_AddRefs(header));
  if (NS_FAILED(rv))
    return E_FAIL;

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(header));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  rv = winAccessNode->QueryNativeInterface(IID_IAccessibleTable, &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aAccessibleTable = static_cast<IAccessibleTable*>(instancePtr);
  return S_OK;
}

STDMETHODIMP
CAccessibleTable::get_columnIndex(long aChildIndex, long *aColumnIndex)
{
  *aColumnIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 columnIndex = 0;
  nsresult rv = tableAcc->GetColumnAtIndex(aChildIndex, &columnIndex);
  *aColumnIndex = columnIndex;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_nColumns(long *aColumnCount)
{
  *aColumnCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 columnCount = 0;
  nsresult rv = tableAcc->GetColumns(&columnCount);
  *aColumnCount = columnCount;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_nRows(long *aRowCount)
{
  *aRowCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 rowCount = 0;
  nsresult rv = tableAcc->GetRows(&rowCount);
  *aRowCount = rowCount;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_nSelectedChildren(long *aChildCount)
{
  *aChildCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRUint32 count = 0;
  nsresult rv = tableAcc->GetSelectedCellsCount(&count);
  *aChildCount = count;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_nSelectedColumns(long *aColumnCount)
{
  *aColumnCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRUint32 count = 0;
  nsresult rv = tableAcc->GetSelectedColumnsCount(&count);
  *aColumnCount = count;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_nSelectedRows(long *aRowCount)
{
  *aRowCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRUint32 count = 0;
  nsresult rv = tableAcc->GetSelectedRowsCount(&count);
  *aRowCount = count;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_rowDescription(long aRow, BSTR *aDescription)
{
  *aDescription = NULL;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsAutoString descr;
  nsresult rv = tableAcc->GetRowDescription (aRow, descr);
  if (NS_FAILED(rv))
    return E_FAIL;

  INT result = ::SysReAllocStringLen(aDescription, descr.get(), descr.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
CAccessibleTable::get_rowExtentAt(long aRow, long aColumn, long *aNRowsSpanned)
{
  *aNRowsSpanned = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 rowsSpanned = 0;
  nsresult rv = tableAcc->GetRowExtentAt(aRow, aColumn, &rowsSpanned);
  *aNRowsSpanned = rowsSpanned;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_rowHeader(IAccessibleTable **aAccessibleTable,
                                long *aStartingColumnIndex)
{
  *aAccessibleTable = NULL;

  // XXX: starting column index is always 0.
  *aStartingColumnIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsCOMPtr<nsIAccessibleTable> header;
  nsresult rv = tableAcc->GetRowHeader(getter_AddRefs(header));
  if (NS_FAILED(rv))
    return E_FAIL;

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(header));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  rv = winAccessNode->QueryNativeInterface(IID_IAccessibleTable,
                                           &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aAccessibleTable = static_cast<IAccessibleTable*>(instancePtr);
  return S_OK;
}

STDMETHODIMP
CAccessibleTable::get_rowIndex(long aChildIndex, long *aRowIndex)
{
  *aRowIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 rowIndex = 0;
  nsresult rv = tableAcc->GetRowAtIndex(aChildIndex, &rowIndex);
  *aRowIndex = rowIndex;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_selectedChildren(long aMaxChildren, long **aChildren,
                                       long *aNChildren)
{
  return GetSelectedItems(aMaxChildren, aChildren, aNChildren, ITEMSTYPE_CELLS);
}

STDMETHODIMP
CAccessibleTable::get_selectedColumns(long aMaxColumns, long **aColumns,
                                      long *aNColumns)
{
  return GetSelectedItems(aMaxColumns, aColumns, aNColumns, ITEMSTYPE_COLUMNS);
}

STDMETHODIMP
CAccessibleTable::get_selectedRows(long aMaxRows, long **aRows, long *aNRows)
{
  return GetSelectedItems(aMaxRows, aRows, aNRows, ITEMSTYPE_ROWS);
}

STDMETHODIMP
CAccessibleTable::get_summary(IUnknown **aAccessible)
{
  *aAccessible = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP
CAccessibleTable::get_isColumnSelected(long aColumn, boolean *aIsSelected)
{
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRBool isSelected = PR_FALSE;
  nsresult rv = tableAcc->IsColumnSelected(aColumn, &isSelected);
  *aIsSelected = isSelected;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_isRowSelected(long aRow, boolean *aIsSelected)
{
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRBool isSelected = PR_FALSE;
  nsresult rv = tableAcc->IsRowSelected(aRow, &isSelected);
  *aIsSelected = isSelected;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_isSelected(long aRow, long aColumn, boolean *aIsSelected)
{
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRBool isSelected = PR_FALSE;
  nsresult rv = tableAcc->IsCellSelected(aRow, aColumn, &isSelected);
  *aIsSelected = isSelected;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::selectRow(long aRow)
{
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->SelectRow(aRow);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::selectColumn(long aColumn)
{
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->SelectColumn(aColumn);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::unselectRow(long aRow)
{
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->UnselectRow(aRow);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::unselectColumn(long aColumn)
{
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->UnselectColumn(aColumn);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleTable::get_rowColumnExtentsAtIndex(long aIndex, long *aRow,
                                              long *aColumn,
                                              long *aRowExtents,
                                              long *aColumnExtents,
                                              boolean *aIsSelected)
{
  *aRow = 0;
  *aColumn = 0;
  *aRowExtents = 0;
  *aColumnExtents = 0;
  *aIsSelected = false;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 row = -1;
  nsresult rv = tableAcc->GetRowAtIndex(aIndex, &row);
  if (NS_FAILED(rv))
    return E_FAIL;

  PRInt32 column = -1;
  rv = tableAcc->GetColumnAtIndex(aIndex, &column);
  if (NS_FAILED(rv))
    return E_FAIL;

  PRInt32 rowExtents = 0;
  rv = tableAcc->GetRowExtentAt(row, column, &rowExtents);
  if (NS_FAILED(rv))
    return E_FAIL;

  PRInt32 columnExtents = 0;
  rv = tableAcc->GetColumnExtentAt(row, column, &columnExtents);
  if (NS_FAILED(rv))
    return E_FAIL;

  PRBool isSelected = PR_FALSE;
  rv = tableAcc->IsCellSelected(row, column, &isSelected);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aRow = row;
  *aColumn = column;
  *aRowExtents = rowExtents;
  *aColumnExtents = columnExtents;
  *aIsSelected = isSelected;

  return S_OK;
}

STDMETHODIMP
CAccessibleTable::get_modelChange(IA2TableModelChange *aModelChange)
{
  aModelChange = NULL;
  return E_NOTIMPL;
}

// CAccessibleTable

HRESULT
CAccessibleTable::GetSelectedItems(long aMaxItems, long **aItems,
                                   long *aItemsCount, eItemsType aType)
{
  *aItemsCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRUint32 size = 0;
  PRInt32 *items = NULL;

  nsresult rv = NS_OK;
  switch (aType) {
    case ITEMSTYPE_CELLS:
      rv = tableAcc->GetSelectedCells(&size, &items);
      break;
    case ITEMSTYPE_COLUMNS:
      rv = tableAcc->GetSelectedColumns(&size, &items);
      break;
    case ITEMSTYPE_ROWS:
      rv = tableAcc->GetSelectedRows(&size, &items);
      break;
    default:
      return E_FAIL;
  }

  if (NS_FAILED(rv))
    return E_FAIL;

  if (size == 0 || !items)
    return S_OK;

  PRUint32 maxSize = size < (PRUint32)aMaxItems ? size : aMaxItems;
  *aItemsCount = maxSize;

  for (PRUint32 index = 0; index < maxSize; ++index)
    (*aItems)[index] = items[index];

  nsMemory::Free(items);
  return S_OK;
}

