/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleTable.h"

#include "Accessible2.h"
#include "AccessibleTable_i.c"
#include "AccessibleTable2_i.c"

#include "nsIAccessible.h"
#include "nsIAccessibleTable.h"
#include "nsIWinAccessNode.h"
#include "nsAccessNodeWrap.h"
#include "nsWinUtils.h"
#include "Statistics.h"

#include "nsCOMPtr.h"
#include "nsString.h"

using namespace mozilla::a11y;

#define CANT_QUERY_ASSERTION_MSG \
"Subclass of ia2AccessibleTable doesn't implement nsIAccessibleTable"\

// IUnknown

STDMETHODIMP
ia2AccessibleTable::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleTable == iid) {
    statistics::IAccessibleTableUsed();
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
ia2AccessibleTable::get_accessibleAt(long aRow, long aColumn,
                                   IUnknown **aAccessible)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_caption(IUnknown** aAccessible)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_childIndex(long aRowIndex, long aColumnIndex,
                                   long* aChildIndex)
{
  A11Y_TRYBLOCK_BEGIN

  *aChildIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  int32_t childIndex = 0;
  nsresult rv = tableAcc->GetCellIndexAt(aRowIndex, aColumnIndex, &childIndex);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aChildIndex = childIndex;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_columnDescription(long aColumn, BSTR* aDescription)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_columnExtentAt(long aRow, long aColumn,
                                      long* nColumnsSpanned)
{
  A11Y_TRYBLOCK_BEGIN

  *nColumnsSpanned = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  int32_t columnsSpanned = 0;
  nsresult rv = tableAcc->GetColumnExtentAt(aRow, aColumn, &columnsSpanned);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *nColumnsSpanned = columnsSpanned;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_columnHeader(IAccessibleTable** aAccessibleTable,
                                    long* aStartingRowIndex)
{
  A11Y_TRYBLOCK_BEGIN

  *aAccessibleTable = NULL;
  *aStartingRowIndex = -1;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_columnIndex(long aChildIndex, long* aColumnIndex)
{
  A11Y_TRYBLOCK_BEGIN

  *aColumnIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  int32_t columnIndex = 0;
  nsresult rv = tableAcc->GetColumnIndexAt(aChildIndex, &columnIndex);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aColumnIndex = columnIndex;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nColumns(long* aColumnCount)
{
  A11Y_TRYBLOCK_BEGIN

  *aColumnCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  int32_t columnCount = 0;
  nsresult rv = tableAcc->GetColumnCount(&columnCount);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aColumnCount = columnCount;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nRows(long* aRowCount)
{
  A11Y_TRYBLOCK_BEGIN

  *aRowCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  int32_t rowCount = 0;
  nsresult rv = tableAcc->GetRowCount(&rowCount);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aRowCount = rowCount;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedChildren(long* aChildCount)
{
  A11Y_TRYBLOCK_BEGIN

  *aChildCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  uint32_t count = 0;
  nsresult rv = tableAcc->GetSelectedCellCount(&count);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aChildCount = count;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedColumns(long* aColumnCount)
{
  A11Y_TRYBLOCK_BEGIN

  *aColumnCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  uint32_t count = 0;
  nsresult rv = tableAcc->GetSelectedColumnCount(&count);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aColumnCount = count;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedRows(long* aRowCount)
{
  A11Y_TRYBLOCK_BEGIN

  *aRowCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  uint32_t count = 0;
  nsresult rv = tableAcc->GetSelectedRowCount(&count);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aRowCount = count;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowDescription(long aRow, BSTR* aDescription)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowExtentAt(long aRow, long aColumn,
                                    long* aNRowsSpanned)
{
  A11Y_TRYBLOCK_BEGIN

  *aNRowsSpanned = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  int32_t rowsSpanned = 0;
  nsresult rv = tableAcc->GetRowExtentAt(aRow, aColumn, &rowsSpanned);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aNRowsSpanned = rowsSpanned;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowHeader(IAccessibleTable** aAccessibleTable,
                                  long* aStartingColumnIndex)
{
  A11Y_TRYBLOCK_BEGIN

  *aAccessibleTable = NULL;
  *aStartingColumnIndex = -1;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowIndex(long aChildIndex, long* aRowIndex)
{
  A11Y_TRYBLOCK_BEGIN

  *aRowIndex = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  int32_t rowIndex = 0;
  nsresult rv = tableAcc->GetRowIndexAt(aChildIndex, &rowIndex);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aRowIndex = rowIndex;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedChildren(long aMaxChildren, long** aChildren,
                                         long* aNChildren)
{
  A11Y_TRYBLOCK_BEGIN

  return GetSelectedItems(aChildren, aNChildren, ITEMSTYPE_CELLS);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedColumns(long aMaxColumns, long** aColumns,
                                        long* aNColumns)
{
  A11Y_TRYBLOCK_BEGIN

  return GetSelectedItems(aColumns, aNColumns, ITEMSTYPE_COLUMNS);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedRows(long aMaxRows, long** aRows, long* aNRows)
{
  A11Y_TRYBLOCK_BEGIN

  return GetSelectedItems(aRows, aNRows, ITEMSTYPE_ROWS);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_summary(IUnknown** aAccessible)
{
  A11Y_TRYBLOCK_BEGIN

  // Neither html:table nor xul:tree nor ARIA grid/tree have an ability to
  // link an accessible object to specify a summary. There is closes method
  // in nsIAccessibleTable::summary to get a summary as a string which is not
  // mapped directly to IAccessible2.

  *aAccessible = NULL;
  return S_FALSE;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_isColumnSelected(long aColumn, boolean* aIsSelected)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_isRowSelected(long aRow, boolean* aIsSelected)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_isSelected(long aRow, long aColumn,
                                   boolean* aIsSelected)
{
  A11Y_TRYBLOCK_BEGIN

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::selectRow(long aRow)
{
  A11Y_TRYBLOCK_BEGIN

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->SelectRow(aRow);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::selectColumn(long aColumn)
{
  A11Y_TRYBLOCK_BEGIN

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->SelectColumn(aColumn);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::unselectRow(long aRow)
{
  A11Y_TRYBLOCK_BEGIN

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->UnselectRow(aRow);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::unselectColumn(long aColumn)
{
  A11Y_TRYBLOCK_BEGIN

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsresult rv = tableAcc->UnselectColumn(aColumn);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowColumnExtentsAtIndex(long aIndex, long* aRow,
                                                long* aColumn,
                                                long* aRowExtents,
                                                long* aColumnExtents,
                                                boolean* aIsSelected)
{
  A11Y_TRYBLOCK_BEGIN

  *aRow = 0;
  *aColumn = 0;
  *aRowExtents = 0;
  *aColumnExtents = 0;
  *aIsSelected = false;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  int32_t rowIdx = -1, columnIdx = -1;
  nsresult rv = tableAcc->GetRowAndColumnIndicesAt(aIndex, &rowIdx, &columnIdx);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  int32_t rowExtents = 0;
  rv = tableAcc->GetRowExtentAt(rowIdx, columnIdx, &rowExtents);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  int32_t columnExtents = 0;
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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_modelChange(IA2TableModelChange* aModelChange)
{
  return E_NOTIMPL;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessibleTable2

STDMETHODIMP
ia2AccessibleTable::get_cellAt(long aRow, long aColumn, IUnknown** aCell)
{
  return get_accessibleAt(aRow, aColumn, aCell);
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedCells(long* aCellCount)
{
  return get_nSelectedChildren(aCellCount);
}

STDMETHODIMP
ia2AccessibleTable::get_selectedCells(IUnknown*** aCells, long* aNSelectedCells)
{
  A11Y_TRYBLOCK_BEGIN

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  nsCOMPtr<nsIArray> geckoCells;
  nsresult rv = tableAcc->GetSelectedCells(getter_AddRefs(geckoCells));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  return nsWinUtils::ConvertToIA2Array(geckoCells, aCells, aNSelectedCells);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedColumns(long** aColumns, long* aNColumns)
{
  A11Y_TRYBLOCK_BEGIN

  return GetSelectedItems(aColumns, aNColumns, ITEMSTYPE_COLUMNS);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedRows(long** aRows, long* aNRows)
{
  A11Y_TRYBLOCK_BEGIN

  return GetSelectedItems(aRows, aNRows, ITEMSTYPE_ROWS);

  A11Y_TRYBLOCK_END
}

////////////////////////////////////////////////////////////////////////////////
// ia2AccessibleTable public

HRESULT
ia2AccessibleTable::GetSelectedItems(long** aItems, long* aItemsCount,
                                     eItemsType aType)
{
  *aItemsCount = 0;

  nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryObject(this));
  NS_ASSERTION(tableAcc, CANT_QUERY_ASSERTION_MSG);
  if (!tableAcc)
    return E_FAIL;

  uint32_t size = 0;
  int32_t *items = nullptr;

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
  for (uint32_t index = 0; index < size; ++index)
    (*aItems)[index] = items[index];

  nsMemory::Free(items);
  return S_OK;
}

