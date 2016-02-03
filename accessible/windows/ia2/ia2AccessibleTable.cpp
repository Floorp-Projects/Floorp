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

// IUnknown

STDMETHODIMP
ia2AccessibleTable::QueryInterface(REFIID iid, void** ppv)
{
  if (!ppv)
    return E_INVALIDARG;

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

  return E_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessibleTable

STDMETHODIMP
ia2AccessibleTable::get_accessibleAt(long aRowIdx, long aColIdx,
                                     IUnknown** aAccessible)
{
  return get_cellAt(aRowIdx, aColIdx, aAccessible);
}

STDMETHODIMP
ia2AccessibleTable::get_caption(IUnknown** aAccessible)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAccessible)
    return E_INVALIDARG;

  *aAccessible = nullptr;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  AccessibleWrap* caption = static_cast<AccessibleWrap*>(mTable->Caption());
  if (!caption)
    return S_FALSE;

  (*aAccessible = static_cast<IAccessible*>(caption))->AddRef();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_childIndex(long aRowIdx, long aColIdx,
                                   long* aChildIdx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aChildIdx)
    return E_INVALIDARG;

  *aChildIdx = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || aColIdx < 0 ||
      static_cast<uint32_t>(aRowIdx) >= mTable->RowCount() ||
      static_cast<uint32_t>(aColIdx) >= mTable->ColCount())
    return E_INVALIDARG;

  *aChildIdx = mTable->CellIndexAt(aRowIdx, aColIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_columnDescription(long aColIdx, BSTR* aDescription)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aDescription)
    return E_INVALIDARG;

  *aDescription = nullptr;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= mTable->ColCount())
    return E_INVALIDARG;

  nsAutoString descr;
  mTable->ColDescription(aColIdx, descr);
  if (descr.IsEmpty())
    return S_FALSE;

  *aDescription = ::SysAllocStringLen(descr.get(), descr.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_columnExtentAt(long aRowIdx, long aColIdx,
                                      long* aSpan)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aSpan)
    return E_INVALIDARG;

  *aSpan = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || aColIdx < 0 ||
      static_cast<uint32_t>(aRowIdx) >= mTable->RowCount() ||
      static_cast<uint32_t>(aColIdx) >= mTable->ColCount())
    return E_INVALIDARG;

  *aSpan = mTable->ColExtentAt(aRowIdx, aColIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_columnHeader(IAccessibleTable** aAccessibleTable,
                                    long* aStartingRowIndex)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAccessibleTable || !aStartingRowIndex)
    return E_INVALIDARG;

  *aAccessibleTable = nullptr;
  *aStartingRowIndex = -1;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_columnIndex(long aCellIdx, long* aColIdx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aColIdx)
    return E_INVALIDARG;

  *aColIdx = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aCellIdx < 0 ||
      static_cast<uint32_t>(aCellIdx) >= mTable->ColCount() * mTable->RowCount())
    return E_INVALIDARG;

  *aColIdx = mTable->ColIndexAt(aCellIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nColumns(long* aColCount)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aColCount)
    return E_INVALIDARG;

  *aColCount = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  *aColCount = mTable->ColCount();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nRows(long* aRowCount)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRowCount)
    return E_INVALIDARG;

  *aRowCount = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  *aRowCount = mTable->RowCount();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedChildren(long* aChildCount)
{
  return get_nSelectedCells(aChildCount);
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedColumns(long* aColCount)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aColCount)
    return E_INVALIDARG;

  *aColCount = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  *aColCount = mTable->SelectedColCount();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedRows(long* aRowCount)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRowCount)
    return E_INVALIDARG;

  *aRowCount = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  *aRowCount = mTable->SelectedRowCount();

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowDescription(long aRowIdx, BSTR* aDescription)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aDescription)
    return E_INVALIDARG;

  *aDescription = nullptr;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= mTable->RowCount())
    return E_INVALIDARG;

  nsAutoString descr;
  mTable->RowDescription(aRowIdx, descr);
  if (descr.IsEmpty())
    return S_FALSE;

  *aDescription = ::SysAllocStringLen(descr.get(), descr.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowExtentAt(long aRowIdx, long aColIdx, long* aSpan)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aSpan)
    return E_INVALIDARG;

  *aSpan = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || aColIdx < 0 ||
      static_cast<uint32_t>(aRowIdx) >= mTable->RowCount() ||
      static_cast<uint32_t>(aColIdx) >= mTable->ColCount())
    return E_INVALIDARG;

  *aSpan = mTable->RowExtentAt(aRowIdx, aColIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowHeader(IAccessibleTable** aAccessibleTable,
                                  long* aStartingColumnIndex)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAccessibleTable || !aStartingColumnIndex)
    return E_INVALIDARG;

  *aAccessibleTable = nullptr;
  *aStartingColumnIndex = -1;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowIndex(long aCellIdx, long* aRowIdx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRowIdx)
    return E_INVALIDARG;

  *aRowIdx = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aCellIdx < 0 ||
      static_cast<uint32_t>(aCellIdx) >= mTable->ColCount() * mTable->RowCount())
    return E_INVALIDARG;

  *aRowIdx = mTable->RowIndexAt(aCellIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedChildren(long aMaxChildren, long** aChildren,
                                         long* aNChildren)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aChildren || !aNChildren)
    return E_INVALIDARG;

  *aChildren = nullptr;
  *aNChildren = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  AutoTArray<uint32_t, 30> cellIndices;
  mTable->SelectedCellIndices(&cellIndices);

  uint32_t maxCells = cellIndices.Length();
  if (maxCells == 0)
    return S_FALSE;

  *aChildren = static_cast<LONG*>(moz_xmalloc(sizeof(LONG) * maxCells));
  *aNChildren = maxCells;
  for (uint32_t i = 0; i < maxCells; i++)
    (*aChildren)[i] = cellIndices[i];

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedColumns(long aMaxColumns, long** aColumns,
                                        long* aNColumns)
{
  A11Y_TRYBLOCK_BEGIN

  return get_selectedColumns(aColumns, aNColumns);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedRows(long aMaxRows, long** aRows, long* aNRows)
{
  A11Y_TRYBLOCK_BEGIN

  return get_selectedRows(aRows, aNRows);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_summary(IUnknown** aAccessible)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAccessible)
    return E_INVALIDARG;

  // Neither html:table nor xul:tree nor ARIA grid/tree have an ability to
  // link an accessible object to specify a summary. There is closes method
  // in Table::summary to get a summary as a string which is not mapped
  // directly to IAccessible2.

  *aAccessible = nullptr;
  return S_FALSE;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_isColumnSelected(long aColIdx, boolean* aIsSelected)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aIsSelected)
    return E_INVALIDARG;

  *aIsSelected = false;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= mTable->ColCount())
    return E_INVALIDARG;

  *aIsSelected = mTable->IsColSelected(aColIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_isRowSelected(long aRowIdx, boolean* aIsSelected)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aIsSelected)
    return E_INVALIDARG;

  *aIsSelected = false;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= mTable->RowCount())
    return E_INVALIDARG;

  *aIsSelected = mTable->IsRowSelected(aRowIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_isSelected(long aRowIdx, long aColIdx,
                                   boolean* aIsSelected)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aIsSelected)
    return E_INVALIDARG;

  *aIsSelected = false;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || aColIdx < 0 ||
      static_cast<uint32_t>(aColIdx) >= mTable->ColCount() ||
      static_cast<uint32_t>(aRowIdx) >= mTable->RowCount())
    return E_INVALIDARG;

  *aIsSelected = mTable->IsCellSelected(aRowIdx, aColIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::selectRow(long aRowIdx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= mTable->RowCount())
    return E_INVALIDARG;

  mTable->SelectRow(aRowIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::selectColumn(long aColIdx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= mTable->ColCount())
    return E_INVALIDARG;

  mTable->SelectCol(aColIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::unselectRow(long aRowIdx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= mTable->RowCount())
    return E_INVALIDARG;

  mTable->UnselectRow(aRowIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::unselectColumn(long aColIdx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= mTable->ColCount())
    return E_INVALIDARG;

  mTable->UnselectCol(aColIdx);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_rowColumnExtentsAtIndex(long aCellIdx, long* aRowIdx,
                                                long* aColIdx,
                                                long* aRowExtents,
                                                long* aColExtents,
                                                boolean* aIsSelected)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRowIdx || !aColIdx || !aRowExtents || !aColExtents || !aIsSelected)
    return E_INVALIDARG;

  *aRowIdx = 0;
  *aColIdx = 0;
  *aRowExtents = 0;
  *aColExtents = 0;
  *aIsSelected = false;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  if (aCellIdx < 0 ||
      static_cast<uint32_t>(aCellIdx) >= mTable->ColCount() * mTable->RowCount())
    return E_INVALIDARG;

  int32_t colIdx = 0, rowIdx = 0;
  mTable->RowAndColIndicesAt(aCellIdx, &rowIdx, &colIdx);
  *aRowIdx = rowIdx;
  *aColIdx = colIdx;
  *aRowExtents = mTable->RowExtentAt(rowIdx, colIdx);
  *aColExtents = mTable->ColExtentAt(rowIdx, colIdx);
  *aIsSelected = mTable->IsCellSelected(rowIdx, colIdx);

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
ia2AccessibleTable::get_cellAt(long aRowIdx, long aColIdx, IUnknown** aCell)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aCell)
    return E_INVALIDARG;

  *aCell = nullptr;

  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  AccessibleWrap* cell =
    static_cast<AccessibleWrap*>(mTable->CellAt(aRowIdx, aColIdx));
  if (!cell)
    return E_INVALIDARG;

  (*aCell = static_cast<IAccessible*>(cell))->AddRef();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_nSelectedCells(long* aCellCount)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aCellCount)
    return E_INVALIDARG;

  *aCellCount = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  *aCellCount = mTable->SelectedCellCount();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedCells(IUnknown*** aCells, long* aNSelectedCells)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aCells || !aNSelectedCells)
    return E_INVALIDARG;

  *aCells = nullptr;
  *aNSelectedCells = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  AutoTArray<Accessible*, 30> cells;
  mTable->SelectedCells(&cells);
  if (cells.IsEmpty())
    return S_FALSE;

  *aCells =
    static_cast<IUnknown**>(::CoTaskMemAlloc(sizeof(IUnknown*) *
                                             cells.Length()));
  if (!*aCells)
    return E_OUTOFMEMORY;

  for (uint32_t i = 0; i < cells.Length(); i++) {
    (*aCells)[i] =
      static_cast<IAccessible*>(static_cast<AccessibleWrap*>(cells[i]));
    ((*aCells)[i])->AddRef();
  }

  *aNSelectedCells = cells.Length();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedColumns(long** aColumns, long* aNColumns)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aColumns || !aNColumns)
    return E_INVALIDARG;

  *aColumns = nullptr;
  *aNColumns = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  AutoTArray<uint32_t, 30> colIndices;
  mTable->SelectedColIndices(&colIndices);

  uint32_t maxCols = colIndices.Length();
  if (maxCols == 0)
    return S_FALSE;

  *aColumns = static_cast<LONG*>(moz_xmalloc(sizeof(LONG) * maxCols));
  *aNColumns = maxCols;
  for (uint32_t i = 0; i < maxCols; i++)
    (*aColumns)[i] = colIndices[i];

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleTable::get_selectedRows(long** aRows, long* aNRows)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRows || !aNRows)
    return E_INVALIDARG;

  *aRows = nullptr;
  *aNRows = 0;
  if (!mTable)
    return CO_E_OBJNOTCONNECTED;

  AutoTArray<uint32_t, 30> rowIndices;
  mTable->SelectedRowIndices(&rowIndices);

  uint32_t maxRows = rowIndices.Length();
  if (maxRows == 0)
    return S_FALSE;

  *aRows = static_cast<LONG*>(moz_xmalloc(sizeof(LONG) * maxRows));
  *aNRows = maxRows;
  for (uint32_t i = 0; i < maxRows; i++)
    (*aRows)[i] = rowIndices[i];

  return S_OK;

  A11Y_TRYBLOCK_END
}
