/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleTableCell.h"
#include "mozilla/a11y/TableAccessible.h"
#include "mozilla/a11y/TableCellAccessible.h"
#include "UiaGridItem.h"

using namespace mozilla;
using namespace mozilla::a11y;

// UiaGridItem

TableCellAccessible* UiaGridItem::CellAcc() {
  auto* derived = static_cast<ia2AccessibleTableCell*>(this);
  Accessible* acc = derived->Acc();
  return acc ? acc->AsTableCell() : nullptr;
}

// IGridItemProvider methods

STDMETHODIMP
UiaGridItem::get_Row(__RPC__out int* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TableCellAccessible* cell = CellAcc();
  if (!cell) {
    return CO_E_OBJNOTCONNECTED;
  }
  *aRetVal = cell->RowIdx();
  return S_OK;
}

STDMETHODIMP
UiaGridItem::get_Column(__RPC__out int* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TableCellAccessible* cell = CellAcc();
  if (!cell) {
    return CO_E_OBJNOTCONNECTED;
  }
  *aRetVal = cell->ColIdx();
  return S_OK;
}

STDMETHODIMP
UiaGridItem::get_RowSpan(__RPC__out int* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TableCellAccessible* cell = CellAcc();
  if (!cell) {
    return CO_E_OBJNOTCONNECTED;
  }
  *aRetVal = cell->RowExtent();
  return S_OK;
}

STDMETHODIMP
UiaGridItem::get_ColumnSpan(__RPC__out int* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TableCellAccessible* cell = CellAcc();
  if (!cell) {
    return CO_E_OBJNOTCONNECTED;
  }
  *aRetVal = cell->ColExtent();
  return S_OK;
}

STDMETHODIMP
UiaGridItem::get_ContainingGrid(
    __RPC__deref_out_opt IRawElementProviderSimple** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  TableCellAccessible* cell = CellAcc();
  if (!cell) {
    return CO_E_OBJNOTCONNECTED;
  }
  TableAccessible* table = cell->Table();
  if (!table) {
    return E_FAIL;
  }
  Accessible* tableAcc = table->AsAccessible();
  RefPtr<IRawElementProviderSimple> uia = MsaaAccessible::GetFrom(tableAcc);
  uia.forget(aRetVal);
  return S_OK;
}

// ITableItemProvider methods

STDMETHODIMP
UiaGridItem::GetRowHeaderItems(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  TableCellAccessible* cell = CellAcc();
  if (!cell) {
    return CO_E_OBJNOTCONNECTED;
  }
  AutoTArray<Accessible*, 10> cells;
  cell->RowHeaderCells(&cells);
  *aRetVal = AccessibleArrayToUiaArray(cells);
  return S_OK;
}

STDMETHODIMP
UiaGridItem::GetColumnHeaderItems(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  TableCellAccessible* cell = CellAcc();
  if (!cell) {
    return CO_E_OBJNOTCONNECTED;
  }
  AutoTArray<Accessible*, 10> cells;
  cell->ColHeaderCells(&cells);
  *aRetVal = AccessibleArrayToUiaArray(cells);
  return S_OK;
}
