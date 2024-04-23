/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleTable.h"
#include "mozilla/a11y/TableAccessible.h"
#include "UiaGrid.h"

using namespace mozilla;
using namespace mozilla::a11y;

// UiaGrid

Accessible* UiaGrid::Acc() {
  auto* ia2t = static_cast<ia2AccessibleTable*>(this);
  return ia2t->MsaaAccessible::Acc();
}

TableAccessible* UiaGrid::TableAcc() {
  Accessible* acc = Acc();
  return acc ? acc->AsTable() : nullptr;
}

// IGridProvider methods

STDMETHODIMP
UiaGrid::GetItem(int aRow, int aColumn,
                 __RPC__deref_out_opt IRawElementProviderSimple** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  TableAccessible* table = TableAcc();
  if (!table) {
    return CO_E_OBJNOTCONNECTED;
  }
  Accessible* cell = table->CellAt(aRow, aColumn);
  if (!cell) {
    return E_INVALIDARG;
  }
  RefPtr<IRawElementProviderSimple> uia = MsaaAccessible::GetFrom(cell);
  uia.forget(aRetVal);
  return S_OK;
}

STDMETHODIMP
UiaGrid::get_RowCount(__RPC__out int* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TableAccessible* table = TableAcc();
  if (!table) {
    return CO_E_OBJNOTCONNECTED;
  }
  *aRetVal = table->RowCount();
  return S_OK;
}

STDMETHODIMP
UiaGrid::get_ColumnCount(__RPC__out int* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  TableAccessible* table = TableAcc();
  if (!table) {
    return CO_E_OBJNOTCONNECTED;
  }
  *aRetVal = table->ColCount();
  return S_OK;
}
