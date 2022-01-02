/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleTableCell.h"

#include "AccessibleTable2_i.c"
#include "AccessibleTableCell_i.c"

#include "AccessibleWrap.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "IUnknownImpl.h"

#include "nsCOMPtr.h"
#include "nsString.h"

using namespace mozilla::a11y;

TableCellAccessible* ia2AccessibleTableCell::CellAcc() {
  AccessibleWrap* acc = LocalAcc();
  return acc ? acc->AsTableCell() : nullptr;
}

// IUnknown
IMPL_IUNKNOWN_QUERY_HEAD(ia2AccessibleTableCell)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessibleTableCell)
IMPL_IUNKNOWN_QUERY_TAIL_INHERITED(ia2AccessibleHypertext)

////////////////////////////////////////////////////////////////////////////////
// IAccessibleTableCell

STDMETHODIMP
ia2AccessibleTableCell::get_table(IUnknown** aTable) {
  if (!aTable) return E_INVALIDARG;

  *aTable = nullptr;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  TableAccessible* table = tableCell->Table();
  if (!table) return E_FAIL;

  AccessibleWrap* wrap = static_cast<AccessibleWrap*>(table->AsAccessible());
  RefPtr<IAccessibleTable> result;
  wrap->GetNativeInterface(getter_AddRefs(result));
  result.forget(aTable);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTableCell::get_columnExtent(long* aSpan) {
  if (!aSpan) return E_INVALIDARG;

  *aSpan = 0;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  *aSpan = tableCell->ColExtent();

  return S_OK;
}

STDMETHODIMP
ia2AccessibleTableCell::get_columnHeaderCells(IUnknown*** aCellAccessibles,
                                              long* aNColumnHeaderCells) {
  if (!aCellAccessibles || !aNColumnHeaderCells) return E_INVALIDARG;

  *aCellAccessibles = nullptr;
  *aNColumnHeaderCells = 0;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  AutoTArray<LocalAccessible*, 10> cells;
  tableCell->ColHeaderCells(&cells);

  *aNColumnHeaderCells = cells.Length();
  *aCellAccessibles = static_cast<IUnknown**>(
      ::CoTaskMemAlloc(sizeof(IUnknown*) * cells.Length()));

  if (!*aCellAccessibles) return E_OUTOFMEMORY;

  for (uint32_t i = 0; i < cells.Length(); i++) {
    AccessibleWrap* cell = static_cast<AccessibleWrap*>(cells[i]);
    RefPtr<IAccessible> iaCell;
    cell->GetNativeInterface(getter_AddRefs(iaCell));
    iaCell.forget(&(*aCellAccessibles)[i]);
  }

  return S_OK;
}

STDMETHODIMP
ia2AccessibleTableCell::get_columnIndex(long* aColIdx) {
  if (!aColIdx) return E_INVALIDARG;

  *aColIdx = -1;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  *aColIdx = tableCell->ColIdx();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTableCell::get_rowExtent(long* aSpan) {
  if (!aSpan) return E_INVALIDARG;

  *aSpan = 0;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  *aSpan = tableCell->RowExtent();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTableCell::get_rowHeaderCells(IUnknown*** aCellAccessibles,
                                           long* aNRowHeaderCells) {
  if (!aCellAccessibles || !aNRowHeaderCells) return E_INVALIDARG;

  *aCellAccessibles = nullptr;
  *aNRowHeaderCells = 0;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  AutoTArray<LocalAccessible*, 10> cells;
  tableCell->RowHeaderCells(&cells);

  *aNRowHeaderCells = cells.Length();
  *aCellAccessibles = static_cast<IUnknown**>(
      ::CoTaskMemAlloc(sizeof(IUnknown*) * cells.Length()));
  if (!*aCellAccessibles) return E_OUTOFMEMORY;

  for (uint32_t i = 0; i < cells.Length(); i++) {
    AccessibleWrap* cell = static_cast<AccessibleWrap*>(cells[i]);
    RefPtr<IAccessible> iaCell;
    cell->GetNativeInterface(getter_AddRefs(iaCell));
    iaCell.forget(&(*aCellAccessibles)[i]);
  }

  return S_OK;
}

STDMETHODIMP
ia2AccessibleTableCell::get_rowIndex(long* aRowIdx) {
  if (!aRowIdx) return E_INVALIDARG;

  *aRowIdx = -1;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  *aRowIdx = tableCell->RowIdx();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleTableCell::get_rowColumnExtents(long* aRowIdx, long* aColIdx,
                                             long* aRowExtents,
                                             long* aColExtents,
                                             boolean* aIsSelected) {
  if (!aRowIdx || !aColIdx || !aRowExtents || !aColExtents || !aIsSelected)
    return E_INVALIDARG;

  *aRowIdx = *aColIdx = *aRowExtents = *aColExtents = 0;
  *aIsSelected = false;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  *aRowIdx = tableCell->RowIdx();
  *aColIdx = tableCell->ColIdx();
  *aRowExtents = tableCell->RowExtent();
  *aColExtents = tableCell->ColExtent();
  *aIsSelected = tableCell->Selected();

  return S_OK;
}

STDMETHODIMP
ia2AccessibleTableCell::get_isSelected(boolean* aIsSelected) {
  if (!aIsSelected) return E_INVALIDARG;

  *aIsSelected = false;
  TableCellAccessible* tableCell = CellAcc();
  if (!tableCell) return CO_E_OBJNOTCONNECTED;

  *aIsSelected = tableCell->Selected();
  return S_OK;
}
