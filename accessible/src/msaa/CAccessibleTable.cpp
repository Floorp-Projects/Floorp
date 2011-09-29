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
#include "AccessibleTable2_i.c"

#include "nsIAccessible.h"
#include "nsIAccessibleTable.h"
#include "nsIWinAccessNode.h"
#include "nsAccessNodeWrap.h"
#include "nsWinUtils.h"

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

  if (IID_IAccessibleTable2 == iid) {
    *ppv = static_cast<IAccessibleTable2*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessibleTable

STDMETHODIMP
CAccessibleTable::get_accessibleAt(long aRow, long aColumn,
                                   IUnknown **aAccessible)
{
__try {
  *aAccessible = NULL;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsCOMPtr<nsIAccessible> cell;
  nsresult rv = tableAcc->GetCellAt(aRow, aColumn, getter_AddRefs(cell));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(cell));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  rv = winAccessNode->QueryNativeInterface(IID_IAccessible2, &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aAccessible = static_cast<IUnknown*>(instancePtr);
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_caption(IUnknown **aAccessible)
{
__try {
  *aAccessible = NULL;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsCOMPtr<nsIAccessible> caption;
  nsresult rv = tableAcc->GetCaption(getter_AddRefs(caption));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (!caption)
    return S_FALSE;

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(caption));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  rv = winAccessNode->QueryNativeInterface(IID_IAccessible2, &instancePtr);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aAccessible = static_cast<IUnknown*>(instancePtr);
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_childIndex(long aRowIndex, long aColumnIndex,
                                 long *aChildIndex)
{
__try {
  *aChildIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 childIndex = 0;
  nsresult rv = tableAcc->GetCellIndexAt(aRowIndex, aColumnIndex, &childIndex);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aChildIndex = childIndex;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_columnDescription(long aColumn, BSTR *aDescription)
{
__try {
  *aDescription = NULL;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsAutoString descr;
  nsresult rv = tableAcc->GetColumnDescription (aColumn, descr);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (descr.IsEmpty())
    return S_FALSE;

  *aDescription = ::SysAllocStringLen(descr.get(), descr.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_columnExtentAt(long aRow, long aColumn,
                                     long *nColumnsSpanned)
{
__try {
  *nColumnsSpanned = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 columnsSpanned = 0;
  nsresult rv = tableAcc->GetColumnExtentAt(aRow, aColumn, &columnsSpanned);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *nColumnsSpanned = columnsSpanned;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_columnHeader(IAccessibleTable **aAccessibleTable,
                                   long *aStartingRowIndex)
{
__try {
  *aAccessibleTable = NULL;
  *aStartingRowIndex = -1;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_NOTIMPL;
}

STDMETHODIMP
CAccessibleTable::get_columnIndex(long aChildIndex, long *aColumnIndex)
{
__try {
  *aColumnIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 columnIndex = 0;
  nsresult rv = tableAcc->GetColumnIndexAt(aChildIndex, &columnIndex);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aColumnIndex = columnIndex;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_nColumns(long *aColumnCount)
{
__try {
  *aColumnCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 columnCount = 0;
  nsresult rv = tableAcc->GetColumnCount(&columnCount);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aColumnCount = columnCount;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_nRows(long *aRowCount)
{
__try {
  *aRowCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 rowCount = 0;
  nsresult rv = tableAcc->GetRowCount(&rowCount);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aRowCount = rowCount;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_nSelectedChildren(long *aChildCount)
{
__try {
  *aChildCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRUint32 count = 0;
  nsresult rv = tableAcc->GetSelectedCellCount(&count);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aChildCount = count;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_nSelectedColumns(long *aColumnCount)
{
__try {
  *aColumnCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRUint32 count = 0;
  nsresult rv = tableAcc->GetSelectedColumnCount(&count);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aColumnCount = count;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_nSelectedRows(long *aRowCount)
{
__try {
  *aRowCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRUint32 count = 0;
  nsresult rv = tableAcc->GetSelectedRowCount(&count);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aRowCount = count;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_rowDescription(long aRow, BSTR *aDescription)
{
__try {
  *aDescription = NULL;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsAutoString descr;
  nsresult rv = tableAcc->GetRowDescription (aRow, descr);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (descr.IsEmpty())
    return S_FALSE;

  *aDescription = ::SysAllocStringLen(descr.get(), descr.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_rowExtentAt(long aRow, long aColumn, long *aNRowsSpanned)
{
__try {
  *aNRowsSpanned = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 rowsSpanned = 0;
  nsresult rv = tableAcc->GetRowExtentAt(aRow, aColumn, &rowsSpanned);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aNRowsSpanned = rowsSpanned;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_rowHeader(IAccessibleTable **aAccessibleTable,
                                long *aStartingColumnIndex)
{
__try {
  *aAccessibleTable = NULL;
  *aStartingColumnIndex = -1;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_NOTIMPL;
}

STDMETHODIMP
CAccessibleTable::get_rowIndex(long aChildIndex, long *aRowIndex)
{
__try {
  *aRowIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 rowIndex = 0;
  nsresult rv = tableAcc->GetRowIndexAt(aChildIndex, &rowIndex);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aRowIndex = rowIndex;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_selectedChildren(long aMaxChildren, long **aChildren,
                                       long *aNChildren)
{
__try {
  return GetSelectedItems(aChildren, aNChildren, ITEMSTYPE_CELLS);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_selectedColumns(long aMaxColumns, long **aColumns,
                                      long *aNColumns)
{
__try {
  return GetSelectedItems(aColumns, aNColumns, ITEMSTYPE_COLUMNS);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_selectedRows(long aMaxRows, long **aRows, long *aNRows)
{
__try {
  return GetSelectedItems(aRows, aNRows, ITEMSTYPE_ROWS);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_summary(IUnknown **aAccessible)
{
__try {
  *aAccessible = NULL;

  // Neither html:table nor xul:tree nor ARIA grid/tree have an ability to
  // link an accessible object to specify a summary. There is closes method
  // in nsIAccessibleTable::summary to get a summary as a string which is not
  // mapped directly to IAccessible2.

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_FALSE;
}

STDMETHODIMP
CAccessibleTable::get_isColumnSelected(long aColumn, boolean *aIsSelected)
{
__try {
  *aIsSelected = false;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  bool isSelected = false;
  nsresult rv = tableAcc->IsColumnSelected(aColumn, &isSelected);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aIsSelected = isSelected;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_isRowSelected(long aRow, boolean *aIsSelected)
{
__try {
  *aIsSelected = false;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  bool isSelected = false;
  nsresult rv = tableAcc->IsRowSelected(aRow, &isSelected);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aIsSelected = isSelected;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_isSelected(long aRow, long aColumn, boolean *aIsSelected)
{
__try {
  *aIsSelected = false;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  bool isSelected = false;
  nsresult rv = tableAcc->IsCellSelected(aRow, aColumn, &isSelected);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aIsSelected = isSelected;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::selectRow(long aRow)
{
__try {
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->SelectRow(aRow);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::selectColumn(long aColumn)
{
__try {
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->SelectColumn(aColumn);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::unselectRow(long aRow)
{
__try {
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->UnselectRow(aRow);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::unselectColumn(long aColumn)
{
__try {
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->UnselectColumn(aColumn);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_rowColumnExtentsAtIndex(long aIndex, long *aRow,
                                              long *aColumn,
                                              long *aRowExtents,
                                              long *aColumnExtents,
                                              boolean *aIsSelected)
{
__try {
  *aRow = 0;
  *aColumn = 0;
  *aRowExtents = 0;
  *aColumnExtents = 0;
  *aIsSelected = false;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRInt32 rowIdx = -1, columnIdx = -1;
  nsresult rv = tableAcc->GetRowAndColumnIndicesAt(aIndex, &rowIdx, &columnIdx);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  PRInt32 rowExtents = 0;
  rv = tableAcc->GetRowExtentAt(rowIdx, columnIdx, &rowExtents);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  PRInt32 columnExtents = 0;
  rv = tableAcc->GetColumnExtentAt(rowIdx, columnIdx, &columnExtents);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  bool isSelected = false;
  rv = tableAcc->IsCellSelected(rowIdx, columnIdx, &isSelected);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aRow = rowIdx;
  *aColumn = columnIdx;
  *aRowExtents = rowExtents;
  *aColumnExtents = columnExtents;
  *aIsSelected = isSelected;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_modelChange(IA2TableModelChange *aModelChange)
{
__try {

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_NOTIMPL;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessibleTable2

STDMETHODIMP
CAccessibleTable::get_cellAt(long row, long column, IUnknown **cell)
{
  return get_accessibleAt(row, column, cell);
}

STDMETHODIMP
CAccessibleTable::get_nSelectedCells(long *cellCount)
{
  return get_nSelectedChildren(cellCount);
}

STDMETHODIMP
CAccessibleTable::get_selectedCells(IUnknown ***cells, long *nSelectedCells)
{
__try {
  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsCOMPtr<nsIArray> geckoCells;
  nsresult rv = tableAcc->GetSelectedCells(getter_AddRefs(geckoCells));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  return nsWinUtils::ConvertToIA2Array(geckoCells, cells, nSelectedCells);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_selectedColumns(long **aColumns, long *aNColumns)
{
__try {
  return GetSelectedItems(aColumns, aNColumns, ITEMSTYPE_COLUMNS);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTable::get_selectedRows(long **aRows, long *aNRows)
{
__try {
  return GetSelectedItems(aRows, aNRows, ITEMSTYPE_ROWS);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////
// CAccessibleTable public

HRESULT
CAccessibleTable::GetSelectedItems(long **aItems, long *aItemsCount,
                                   eItemsType aType)
{
  *aItemsCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  PRUint32 size = 0;
  PRInt32 *items = nsnull;

  nsresult rv = NS_OK;
  switch (aType) {
    case ITEMSTYPE_CELLS:
      rv = tableAcc->GetSelectedCellIndices(&size, &items);
      break;
    case ITEMSTYPE_COLUMNS:
      rv = tableAcc->GetSelectedColumnIndices(&size, &items);
      break;
    case ITEMSTYPE_ROWS:
      rv = tableAcc->GetSelectedRowIndices(&size, &items);
      break;
    default:
      return E_FAIL;
  }

  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (size == 0 || !items)
    return S_FALSE;

  *aItems = static_cast<long*>(nsMemory::Alloc((size) * sizeof(long)));
  if (!*aItems)
    return E_OUTOFMEMORY;

  *aItemsCount = size;
  for (PRUint32 index = 0; index < size; ++index)
    (*aItems)[index] = items[index];

  nsMemory::Free(items);
  return S_OK;
}

