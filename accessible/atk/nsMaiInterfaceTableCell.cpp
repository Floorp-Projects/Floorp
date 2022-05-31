/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "mozilla/a11y/TableAccessibleBase.h"
#include "mozilla/a11y/TableCellAccessibleBase.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsMai.h"
#include "RemoteAccessible.h"
#include "nsTArray.h"

#include "mozilla/Likely.h"

using namespace mozilla;
using namespace mozilla::a11y;

extern "C" {
static gint GetColumnSpanCB(AtkTableCell* aCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return 0;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableCellBase()->ColExtent());
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return proxy->ColExtent();
  }

  return 0;
}

static gint GetRowSpanCB(AtkTableCell* aCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return 0;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableCellBase()->RowExtent());
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return proxy->RowExtent();
  }

  return 0;
}

static gboolean GetPositionCB(AtkTableCell* aCell, gint* aRow, gint* aCol) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return false;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    TableCellAccessibleBase* cell = acc->AsTableCellBase();
    if (!cell) {
      return false;
    }
    *aRow = cell->RowIdx();
    *aCol = cell->ColIdx();
    return true;
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    uint32_t rowIdx = 0, colIdx = 0;
    proxy->GetPosition(&rowIdx, &colIdx);
    *aCol = colIdx;
    *aRow = rowIdx;
    return true;
  }

  return false;
}

static gboolean GetColumnRowSpanCB(AtkTableCell* aCell, gint* aCol, gint* aRow,
                                   gint* aColExtent, gint* aRowExtent) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return false;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    TableCellAccessibleBase* cellAcc = acc->AsTableCellBase();
    if (!cellAcc) {
      return false;
    }
    *aCol = cellAcc->ColIdx();
    *aRow = cellAcc->RowIdx();
    *aColExtent = cellAcc->ColExtent();
    *aRowExtent = cellAcc->ColExtent();
    return true;
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    uint32_t colIdx = 0, rowIdx = 0, colExtent = 0, rowExtent = 0;
    proxy->GetColRowExtents(&colIdx, &rowIdx, &colExtent, &rowExtent);
    *aCol = colIdx;
    *aRow = rowIdx;
    *aColExtent = colExtent;
    *aRowExtent = rowExtent;
    return true;
  }

  return false;
}

static AtkObject* GetTableCB(AtkTableCell* aTableCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTableCell));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    TableAccessibleBase* table = acc->AsTableCellBase()->Table();
    if (!table) {
      return nullptr;
    }

    Accessible* tableAcc = table->AsAccessible();
    return tableAcc ? GetWrapperFor(tableAcc) : nullptr;
  }

  if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aTableCell))) {
    RemoteAccessible* table = proxy->TableOfACell();
    return table ? GetWrapperFor(table) : nullptr;
  }

  return nullptr;
}

static GPtrArray* GetColumnHeaderCellsCB(AtkTableCell* aCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    AutoTArray<Accessible*, 10> headers;
    acc->AsTableCellBase()->ColHeaderCells(&headers);
    if (headers.IsEmpty()) {
      return nullptr;
    }

    GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
    for (Accessible* header : headers) {
      AtkObject* atkHeader = AccessibleWrap::GetAtkObject(header->AsLocal());
      g_object_ref(atkHeader);
      g_ptr_array_add(atkHeaders, atkHeader);
    }

    return atkHeaders;
  }

  if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aCell))) {
    AutoTArray<RemoteAccessible*, 10> headers;
    proxy->ColHeaderCells(&headers);
    if (headers.IsEmpty()) {
      return nullptr;
    }

    GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
    for (RemoteAccessible* header : headers) {
      AtkObject* atkHeader = GetWrapperFor(header);
      g_object_ref(atkHeader);
      g_ptr_array_add(atkHeaders, atkHeader);
    }

    return atkHeaders;
  }

  return nullptr;
}

static GPtrArray* GetRowHeaderCellsCB(AtkTableCell* aCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    AutoTArray<Accessible*, 10> headers;
    acc->AsTableCellBase()->RowHeaderCells(&headers);
    if (headers.IsEmpty()) {
      return nullptr;
    }

    GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
    for (Accessible* header : headers) {
      AtkObject* atkHeader = AccessibleWrap::GetAtkObject(header->AsLocal());
      g_object_ref(atkHeader);
      g_ptr_array_add(atkHeaders, atkHeader);
    }

    return atkHeaders;
  }

  if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aCell))) {
    AutoTArray<RemoteAccessible*, 10> headers;
    proxy->RowHeaderCells(&headers);
    if (headers.IsEmpty()) {
      return nullptr;
    }

    GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
    for (RemoteAccessible* header : headers) {
      AtkObject* atkHeader = GetWrapperFor(header);
      g_object_ref(atkHeader);
      g_ptr_array_add(atkHeaders, atkHeader);
    }

    return atkHeaders;
  }

  return nullptr;
}
}

void tableCellInterfaceInitCB(AtkTableCellIface* aIface) {
  NS_ASSERTION(aIface, "no interface!");
  if (MOZ_UNLIKELY(!aIface)) return;

  aIface->get_column_span = GetColumnSpanCB;
  aIface->get_column_header_cells = GetColumnHeaderCellsCB;
  aIface->get_position = GetPositionCB;
  aIface->get_row_span = GetRowSpanCB;
  aIface->get_row_header_cells = GetRowHeaderCellsCB;
  aIface->get_row_column_span = GetColumnRowSpanCB;
  aIface->get_table = GetTableCB;
}
