/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleTable.h"
#include "mozilla/a11y/TableAccessible.h"
#include "nsIAccessiblePivot.h"
#include "Pivot.h"
#include "UiaGrid.h"

using namespace mozilla;
using namespace mozilla::a11y;

// Helpers

// Used to search for all row and column headers in a table. This could be slow,
// as it potentially walks all cells in the table. However, it's unclear if,
// when or how often clients will use this. If this proves to be a performance
// problem, we will need to add methods to TableAccessible to get all row and
// column headers in a faster way.
class HeaderRule : public PivotRule {
 public:
  explicit HeaderRule(role aRole) : mRole(aRole) {}

  virtual uint16_t Match(Accessible* aAcc) override {
    role accRole = aAcc->Role();
    if (accRole == mRole) {
      return nsIAccessibleTraversalRule::FILTER_MATCH |
             nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }
    if (accRole == roles::CAPTION || aAcc->IsTableCell()) {
      return nsIAccessibleTraversalRule::FILTER_IGNORE |
             nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }
    return nsIAccessibleTraversalRule::FILTER_IGNORE;
  }

 private:
  role mRole;
};

static SAFEARRAY* GetAllHeaders(Accessible* aTable, role aRole) {
  AutoTArray<Accessible*, 20> headers;
  Pivot pivot(aTable);
  HeaderRule rule(aRole);
  for (Accessible* header = pivot.Next(aTable, rule); header;
       header = pivot.Next(header, rule)) {
    headers.AppendElement(header);
  }
  return AccessibleArrayToUiaArray(headers);
}

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

// ITableProvider methods

STDMETHODIMP
UiaGrid::GetRowHeaders(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  *aRetVal = GetAllHeaders(acc, roles::ROWHEADER);
  return S_OK;
}

STDMETHODIMP
UiaGrid::GetColumnHeaders(__RPC__deref_out_opt SAFEARRAY** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  *aRetVal = GetAllHeaders(acc, roles::COLUMNHEADER);
  return S_OK;
}

STDMETHODIMP
UiaGrid::get_RowOrColumnMajor(__RPC__out enum RowOrColumnMajor* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  // HTML and ARIA tables are always in row major order.
  *aRetVal = RowOrColumnMajor_RowMajor;
  return S_OK;
}
