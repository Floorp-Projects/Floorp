/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "Accessible-inl.h"
#include "AccessibleWrap.h"
#include "nsAccUtils.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "nsMai.h"
#include "ProxyAccessible.h"
#include "nsArrayUtils.h"

#include "mozilla/Likely.h"

using namespace mozilla::a11y;

extern "C" {
static gint
GetColumnSpanCB(AtkTableCell* aCell)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aCell));
  if (accWrap) {
    return accWrap->AsTableCell()->ColExtent();
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aCell))) {
    return proxy->ColExtent();
  }

  return 0;
}

static gboolean
GetRowSpanCB(AtkTableCell* aCell)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aCell));
  if (accWrap) {
    return accWrap->AsTableCell()->RowExtent();
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aCell))) {
    return proxy->RowExtent();
  }

  return 0;
}

static gboolean
GetPositionCB(AtkTableCell* aCell, gint* aRow, gint* aCol)
{
  if (AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aCell))) {
    TableCellAccessible* cell = accWrap->AsTableCell();
    if (!cell) {
      return false;
    }
    *aRow = cell->RowIdx();
    *aCol = cell->ColIdx();
    return true;
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aCell))) {
    uint32_t rowIdx = 0, colIdx = 0;
    proxy->GetPosition(&rowIdx, &colIdx);
    *aCol = colIdx;
    *aRow = rowIdx;
    return true;
  }

  return false;
}

static gboolean
GetColumnRowSpanCB(AtkTableCell* aCell, gint* aCol, gint* aRow,
                   gint* aColExtent, gint* aRowExtent) {
  if (AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aCell))) {
    TableCellAccessible* cellAcc = accWrap->AsTableCell();
    if (!cellAcc) {
      return false;
    }
    *aCol = cellAcc->ColIdx();
    *aRow = cellAcc->RowIdx();
    *aColExtent = cellAcc->ColExtent();
    *aRowExtent = cellAcc->ColExtent();
    return true;
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aCell))) {
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

static AtkObject*
GetTableCB(AtkTableCell* aTableCell)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTableCell));
  if (accWrap) {
    TableAccessible* table = accWrap->AsTableCell()->Table();
    if (!table) {
      return nullptr;
    }

    Accessible* tableAcc = table->AsAccessible();
    return tableAcc ? AccessibleWrap::GetAtkObject(tableAcc) : nullptr;
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTableCell))) {
    ProxyAccessible* table = proxy->TableOfACell();
    return table ? GetWrapperFor(table) : nullptr;
  }

  return nullptr;
}

static GPtrArray*
GetColumnHeaderCellsCB(AtkTableCell* aCell)
{
  if (AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aCell))) {
    AutoTArray<Accessible*, 10> headers;
    accWrap->AsTableCell()->ColHeaderCells(&headers);
    if (headers.IsEmpty()) {
      return nullptr;
    }

    GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
    for (Accessible* header: headers) {
      AtkObject* atkHeader = AccessibleWrap::GetAtkObject(header);
      g_object_ref(atkHeader);
      g_ptr_array_add(atkHeaders, atkHeader);
    }

    return atkHeaders;
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aCell))) {
    AutoTArray<ProxyAccessible*, 10> headers;
    proxy->ColHeaderCells(&headers);
    if (headers.IsEmpty()) {
      return nullptr;
    }

    GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
    for (ProxyAccessible* header: headers) {
      AtkObject* atkHeader = GetWrapperFor(header);
      g_object_ref(atkHeader);
      g_ptr_array_add(atkHeaders, atkHeader);
    }

    return atkHeaders;
  }

  return nullptr;
}

static GPtrArray*
GetRowHeaderCellsCB(AtkTableCell* aCell)
{
  if (AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aCell))) {
    AutoTArray<Accessible*, 10> headers;
    accWrap->AsTableCell()->RowHeaderCells(&headers);
    if (headers.IsEmpty()) {
      return nullptr;
    }

    GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
    for (Accessible* header: headers) {
      AtkObject* atkHeader = AccessibleWrap::GetAtkObject(header);
      g_object_ref(atkHeader);
      g_ptr_array_add(atkHeaders, atkHeader);
    }

    return atkHeaders;
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aCell))) {
    AutoTArray<ProxyAccessible*, 10> headers;
    proxy->RowHeaderCells(&headers);
    if (headers.IsEmpty()) {
      return nullptr;
    }

    GPtrArray* atkHeaders = g_ptr_array_sized_new(headers.Length());
    for (ProxyAccessible* header: headers) {
      AtkObject* atkHeader = GetWrapperFor(header);
      g_object_ref(atkHeader);
      g_ptr_array_add(atkHeaders, atkHeader);
    }

    return atkHeaders;
  }

  return nullptr;
}
}

void
tableCellInterfaceInitCB(AtkTableCellIface* aIface)
{
  NS_ASSERTION(aIface, "no interface!");
  if (MOZ_UNLIKELY(!aIface))
    return;

  aIface->get_column_span = GetColumnSpanCB;
  aIface->get_column_header_cells = GetColumnHeaderCellsCB;
  aIface->get_position = GetPositionCB;
  aIface->get_row_span = GetRowSpanCB;
  aIface->get_row_header_cells = GetRowHeaderCellsCB;
  aIface->get_row_column_span = GetColumnRowSpanCB;
  aIface->get_table = GetTableCB;
}
