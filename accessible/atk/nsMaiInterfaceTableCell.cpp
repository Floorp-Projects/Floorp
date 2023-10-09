/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "mozilla/a11y/TableAccessible.h"
#include "mozilla/a11y/TableCellAccessible.h"
#include "nsAccessibilityService.h"
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
  return static_cast<gint>(acc->AsTableCell()->ColExtent());
}

static gint GetRowSpanCB(AtkTableCell* aCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return 0;
  }
  return static_cast<gint>(acc->AsTableCell()->RowExtent());
}

static gboolean GetPositionCB(AtkTableCell* aCell, gint* aRow, gint* aCol) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return false;
  }
  TableCellAccessible* cell = acc->AsTableCell();
  if (!cell) {
    return false;
  }
  *aRow = static_cast<gint>(cell->RowIdx());
  *aCol = static_cast<gint>(cell->ColIdx());
  return true;
}

static gboolean GetColumnRowSpanCB(AtkTableCell* aCell, gint* aCol, gint* aRow,
                                   gint* aColExtent, gint* aRowExtent) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return false;
  }
  TableCellAccessible* cellAcc = acc->AsTableCell();
  if (!cellAcc) {
    return false;
  }
  *aCol = static_cast<gint>(cellAcc->ColIdx());
  *aRow = static_cast<gint>(cellAcc->RowIdx());
  *aColExtent = static_cast<gint>(cellAcc->ColExtent());
  *aRowExtent = static_cast<gint>(cellAcc->ColExtent());
  return true;
}

static AtkObject* GetTableCB(AtkTableCell* aTableCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTableCell));
  if (!acc) {
    return nullptr;
  }
  TableCellAccessible* cell = acc->AsTableCell();
  if (!cell) {
    return nullptr;
  }
  TableAccessible* table = cell->Table();
  if (!table) {
    return nullptr;
  }
  Accessible* tableAcc = table->AsAccessible();
  return tableAcc ? GetWrapperFor(tableAcc) : nullptr;
}

static GPtrArray* GetColumnHeaderCellsCB(AtkTableCell* aCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return nullptr;
  }
  TableCellAccessible* cell = acc->AsTableCell();
  if (!cell) {
    return nullptr;
  }
  AutoTArray<Accessible*, 10> headers;
  cell->ColHeaderCells(&headers);
  if (headers.IsEmpty()) {
    return nullptr;
  }

  GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
  for (Accessible* header : headers) {
    AtkObject* atkHeader = GetWrapperFor(header);
    g_object_ref(atkHeader);
    g_ptr_array_add(atkHeaders, atkHeader);
  }

  return atkHeaders;
}

static GPtrArray* GetRowHeaderCellsCB(AtkTableCell* aCell) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aCell));
  if (!acc) {
    return nullptr;
  }
  TableCellAccessible* cell = acc->AsTableCell();
  if (!cell) {
    return nullptr;
  }
  AutoTArray<Accessible*, 10> headers;
  cell->RowHeaderCells(&headers);
  if (headers.IsEmpty()) {
    return nullptr;
  }

  GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
  for (Accessible* header : headers) {
    AtkObject* atkHeader = GetWrapperFor(header);
    g_object_ref(atkHeader);
    g_ptr_array_add(atkHeaders, atkHeader);
  }

  return atkHeaders;
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
