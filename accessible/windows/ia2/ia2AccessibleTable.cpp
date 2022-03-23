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

#include "AccessibleWrap.h"
#include "IUnknownImpl.h"
#include "Statistics.h"
#include "TableAccessible.h"

#include "nsCOMPtr.h"
#include "nsString.h"

using namespace mozilla::a11y;

TableAccessible* ia2AccessibleTable::TableAcc() {
  AccessibleWrap* acc = LocalAcc();
  return acc ? acc->AsTable() : nullptr;
}

// IUnknown

STDMETHODIMP
ia2AccessibleTable::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

  *ppv = nullptr;

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

  return ia2AccessibleHypertext::QueryInterface(iid, ppv);
}

////////////////////////////////////////////////////////////////////////////////
// IAccessibleTable

STDMETHODIMP
ia2AccessibleTable::get_accessibleAt(long aRowIdx, long aColIdx,
                                     IUnknown** aAccessible) {
  return get_cellAt(aRowIdx, aColIdx, aAccessible);
}

STDMETHODIMP
ia2AccessibleTable::get_caption(IUnknown** aAccessible) {
  if (!aAccessible) return E_INVALIDARG;

  *aAccessible = nullptr;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  AccessibleWrap* caption = static_cast<AccessibleWrap*>(table->Caption());
  if (!caption) return S_FALSE;

  RefPtr<IAccessible> result;
  caption->GetNativeInterface(getter_AddRefs(result));
  result.forget(aAccessible);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_childIndex(long aRowIdx, long aColIdx,
                                   long* aChildIdx) {
  if (!aChildIdx) return E_INVALIDARG;

  *aChildIdx = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || aColIdx < 0 ||
      static_cast<uint32_t>(aRowIdx) >= table->RowCount() ||
      static_cast<uint32_t>(aColIdx) >= table->ColCount())
    return E_INVALIDARG;

  *aChildIdx = table->CellIndexAt(aRowIdx, aColIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_columnDescription(long aColIdx, BSTR* aDescription) {
  if (!aDescription) return E_INVALIDARG;

  *aDescription = nullptr;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= table->ColCount())
    return E_INVALIDARG;

  nsAutoString descr;
  table->ColDescription(aColIdx, descr);
  if (descr.IsEmpty()) return S_FALSE;

  *aDescription = ::SysAllocStringLen(descr.get(), descr.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
ia2AccessibleTable::get_columnExtentAt(long aRowIdx, long aColIdx,
                                       long* aSpan) {
  if (!aSpan) return E_INVALIDARG;

  *aSpan = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || aColIdx < 0 ||
      static_cast<uint32_t>(aRowIdx) >= table->RowCount() ||
      static_cast<uint32_t>(aColIdx) >= table->ColCount())
    return E_INVALIDARG;

  *aSpan = table->ColExtentAt(aRowIdx, aColIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_columnHeader(IAccessibleTable** aAccessibleTable,
                                     long* aStartingRowIndex) {
  if (!aAccessibleTable || !aStartingRowIndex) return E_INVALIDARG;

  *aAccessibleTable = nullptr;
  *aStartingRowIndex = -1;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2AccessibleTable::get_columnIndex(long aCellIdx, long* aColIdx) {
  if (!aColIdx) return E_INVALIDARG;

  *aColIdx = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aCellIdx < 0) {
    return E_INVALIDARG;
  }

  long colIdx = table->ColIndexAt(aCellIdx);
  if (colIdx == -1) {  // Indicates an error.
    return E_INVALIDARG;
  }

  *aColIdx = colIdx;
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_nColumns(long* aColCount) {
  if (!aColCount) return E_INVALIDARG;

  *aColCount = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  *aColCount = table->ColCount();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_nRows(long* aRowCount) {
  if (!aRowCount) return E_INVALIDARG;

  *aRowCount = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  *aRowCount = table->RowCount();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedChildren(long* aChildCount) {
  return get_nSelectedCells(aChildCount);
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedColumns(long* aColCount) {
  if (!aColCount) return E_INVALIDARG;

  *aColCount = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  *aColCount = table->SelectedColCount();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedRows(long* aRowCount) {
  if (!aRowCount) return E_INVALIDARG;

  *aRowCount = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  *aRowCount = table->SelectedRowCount();

  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_rowDescription(long aRowIdx, BSTR* aDescription) {
  if (!aDescription) return E_INVALIDARG;

  *aDescription = nullptr;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= table->RowCount())
    return E_INVALIDARG;

  nsAutoString descr;
  table->RowDescription(aRowIdx, descr);
  if (descr.IsEmpty()) return S_FALSE;

  *aDescription = ::SysAllocStringLen(descr.get(), descr.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
ia2AccessibleTable::get_rowExtentAt(long aRowIdx, long aColIdx, long* aSpan) {
  if (!aSpan) return E_INVALIDARG;

  *aSpan = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || aColIdx < 0 ||
      static_cast<uint32_t>(aRowIdx) >= table->RowCount() ||
      static_cast<uint32_t>(aColIdx) >= table->ColCount())
    return E_INVALIDARG;

  *aSpan = table->RowExtentAt(aRowIdx, aColIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_rowHeader(IAccessibleTable** aAccessibleTable,
                                  long* aStartingColumnIndex) {
  if (!aAccessibleTable || !aStartingColumnIndex) return E_INVALIDARG;

  *aAccessibleTable = nullptr;
  *aStartingColumnIndex = -1;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2AccessibleTable::get_rowIndex(long aCellIdx, long* aRowIdx) {
  if (!aRowIdx) return E_INVALIDARG;

  *aRowIdx = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aCellIdx < 0) {
    return E_INVALIDARG;
  }

  long rowIdx = table->RowIndexAt(aCellIdx);
  if (rowIdx == -1) {  // Indicates an error.
    return E_INVALIDARG;
  }

  *aRowIdx = rowIdx;
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_selectedChildren(long aMaxChildren, long** aChildren,
                                         long* aNChildren) {
  if (!aChildren || !aNChildren) return E_INVALIDARG;

  *aChildren = nullptr;
  *aNChildren = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  AutoTArray<uint32_t, 30> cellIndices;
  table->SelectedCellIndices(&cellIndices);

  uint32_t maxCells = cellIndices.Length();
  if (maxCells == 0) return S_FALSE;

  *aChildren = static_cast<LONG*>(moz_xmalloc(sizeof(LONG) * maxCells));
  *aNChildren = maxCells;
  for (uint32_t i = 0; i < maxCells; i++) (*aChildren)[i] = cellIndices[i];

  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_selectedColumns(long aMaxColumns, long** aColumns,
                                        long* aNColumns) {
  return get_selectedColumns(aColumns, aNColumns);
}

STDMETHODIMP
ia2AccessibleTable::get_selectedRows(long aMaxRows, long** aRows,
                                     long* aNRows) {
  return get_selectedRows(aRows, aNRows);
}

STDMETHODIMP
ia2AccessibleTable::get_summary(IUnknown** aAccessible) {
  if (!aAccessible) return E_INVALIDARG;

  // Neither html:table nor xul:tree nor ARIA grid/tree have an ability to
  // link an accessible object to specify a summary. There is closes method
  // in Table::summary to get a summary as a string which is not mapped
  // directly to IAccessible2.

  *aAccessible = nullptr;
  return S_FALSE;
}

STDMETHODIMP
ia2AccessibleTable::get_isColumnSelected(long aColIdx, boolean* aIsSelected) {
  if (!aIsSelected) return E_INVALIDARG;

  *aIsSelected = false;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= table->ColCount())
    return E_INVALIDARG;

  *aIsSelected = table->IsColSelected(aColIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_isRowSelected(long aRowIdx, boolean* aIsSelected) {
  if (!aIsSelected) return E_INVALIDARG;

  *aIsSelected = false;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= table->RowCount())
    return E_INVALIDARG;

  *aIsSelected = table->IsRowSelected(aRowIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_isSelected(long aRowIdx, long aColIdx,
                                   boolean* aIsSelected) {
  if (!aIsSelected) return E_INVALIDARG;

  *aIsSelected = false;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || aColIdx < 0 ||
      static_cast<uint32_t>(aColIdx) >= table->ColCount() ||
      static_cast<uint32_t>(aRowIdx) >= table->RowCount())
    return E_INVALIDARG;

  *aIsSelected = table->IsCellSelected(aRowIdx, aColIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::selectRow(long aRowIdx) {
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= table->RowCount())
    return E_INVALIDARG;

  table->SelectRow(aRowIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::selectColumn(long aColIdx) {
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= table->ColCount())
    return E_INVALIDARG;

  table->SelectCol(aColIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::unselectRow(long aRowIdx) {
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= table->RowCount())
    return E_INVALIDARG;

  table->UnselectRow(aRowIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::unselectColumn(long aColIdx) {
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= table->ColCount())
    return E_INVALIDARG;

  table->UnselectCol(aColIdx);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_rowColumnExtentsAtIndex(long aCellIdx, long* aRowIdx,
                                                long* aColIdx,
                                                long* aRowExtents,
                                                long* aColExtents,
                                                boolean* aIsSelected) {
  if (!aRowIdx || !aColIdx || !aRowExtents || !aColExtents || !aIsSelected)
    return E_INVALIDARG;

  *aRowIdx = 0;
  *aColIdx = 0;
  *aRowExtents = 0;
  *aColExtents = 0;
  *aIsSelected = false;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  if (aCellIdx < 0) {
    return E_INVALIDARG;
  }

  int32_t colIdx = 0, rowIdx = 0;
  table->RowAndColIndicesAt(aCellIdx, &rowIdx, &colIdx);
  if (rowIdx == -1 || colIdx == -1) {  // Indicates an error.
    return E_INVALIDARG;
  }

  *aRowIdx = rowIdx;
  *aColIdx = colIdx;
  *aRowExtents = table->RowExtentAt(rowIdx, colIdx);
  *aColExtents = table->ColExtentAt(rowIdx, colIdx);
  *aIsSelected = table->IsCellSelected(rowIdx, colIdx);

  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_modelChange(IA2TableModelChange* aModelChange) {
  return E_NOTIMPL;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessibleTable2

STDMETHODIMP
ia2AccessibleTable::get_cellAt(long aRowIdx, long aColIdx, IUnknown** aCell) {
  if (!aCell) return E_INVALIDARG;

  *aCell = nullptr;

  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  AccessibleWrap* cell =
      static_cast<AccessibleWrap*>(table->CellAt(aRowIdx, aColIdx));
  if (!cell) return E_INVALIDARG;

  RefPtr<IAccessible> result;
  cell->GetNativeInterface(getter_AddRefs(result));
  result.forget(aCell);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedCells(long* aCellCount) {
  if (!aCellCount) return E_INVALIDARG;

  *aCellCount = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  *aCellCount = table->SelectedCellCount();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_selectedCells(IUnknown*** aCells,
                                      long* aNSelectedCells) {
  if (!aCells || !aNSelectedCells) return E_INVALIDARG;

  *aCells = nullptr;
  *aNSelectedCells = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  AutoTArray<LocalAccessible*, 30> cells;
  table->SelectedCells(&cells);
  if (cells.IsEmpty()) return S_FALSE;

  *aCells = static_cast<IUnknown**>(
      ::CoTaskMemAlloc(sizeof(IUnknown*) * cells.Length()));
  if (!*aCells) return E_OUTOFMEMORY;

  for (uint32_t i = 0; i < cells.Length(); i++) {
    RefPtr<IAccessible> cell;
    cells[i]->GetNativeInterface(getter_AddRefs(cell));
    cell.forget(&(*aCells)[i]);
  }

  *aNSelectedCells = cells.Length();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_selectedColumns(long** aColumns, long* aNColumns) {
  if (!aColumns || !aNColumns) return E_INVALIDARG;

  *aColumns = nullptr;
  *aNColumns = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  AutoTArray<uint32_t, 30> colIndices;
  table->SelectedColIndices(&colIndices);

  uint32_t maxCols = colIndices.Length();
  if (maxCols == 0) return S_FALSE;

  *aColumns = static_cast<LONG*>(moz_xmalloc(sizeof(LONG) * maxCols));
  *aNColumns = maxCols;
  for (uint32_t i = 0; i < maxCols; i++) (*aColumns)[i] = colIndices[i];

  return S_OK;
}

STDMETHODIMP
ia2AccessibleTable::get_selectedRows(long** aRows, long* aNRows) {
  if (!aRows || !aNRows) return E_INVALIDARG;

  *aRows = nullptr;
  *aNRows = 0;
  TableAccessible* table = TableAcc();
  if (!table) return CO_E_OBJNOTCONNECTED;

  AutoTArray<uint32_t, 30> rowIndices;
  table->SelectedRowIndices(&rowIndices);

  uint32_t maxRows = rowIndices.Length();
  if (maxRows == 0) return S_FALSE;

  *aRows = static_cast<LONG*>(moz_xmalloc(sizeof(LONG) * maxRows));
  *aNRows = maxRows;
  for (uint32_t i = 0; i < maxRows; i++) (*aRows)[i] = rowIndices[i];

  return S_OK;
}
