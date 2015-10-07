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
static AtkObject*
refAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx)
{
  if (aRowIdx < 0 || aColIdx < 0) {
    return nullptr;
  }

  AtkObject* cellAtkObj = nullptr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    Accessible* cell = accWrap->AsTable()->CellAt(aRowIdx, aColIdx);
    if (!cell) {
      return nullptr;
    }

    cellAtkObj = AccessibleWrap::GetAtkObject(cell);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    ProxyAccessible* cell = proxy->TableCellAt(aRowIdx, aColIdx);
    if (!cell) {
      return nullptr;
    }

    cellAtkObj = GetWrapperFor(cell);
  }

  if (cellAtkObj) {
    g_object_ref(cellAtkObj);
  }

  return cellAtkObj;
}

static gint
getIndexAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx)
{
  if (aRowIdx < 0 || aColIdx < 0) {
    return -1;
  }

  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gint>(accWrap->AsTable()->CellIndexAt(aRowIdx, aColIdx));
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gint>(proxy->TableCellIndexAt(aRowIdx, aColIdx));
  }

  return -1;
}

static gint
getColumnAtIndexCB(AtkTable *aTable, gint aIdx)
{
  if (aIdx < 0) {
    return -1;
  }

  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gint>(accWrap->AsTable()->ColIndexAt(aIdx));
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gint>(proxy->TableColumnIndexAt(aIdx));
  }

  return -1;
}

static gint
getRowAtIndexCB(AtkTable *aTable, gint aIdx)
{
  if (aIdx < 0) {
    return -1;
  }

  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gint>(accWrap->AsTable()->RowIndexAt(aIdx));
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gint>(proxy->TableRowIndexAt(aIdx));
  }

  return -1;
}

static gint
getColumnCountCB(AtkTable *aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gint>(accWrap->AsTable()->ColCount());
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gint>(proxy->TableColumnCount());
  }

  return -1;
}

static gint
getRowCountCB(AtkTable *aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gint>(accWrap->AsTable()->RowCount());
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gint>(proxy->TableRowCount());
  }

  return -1;
}

static gint
getColumnExtentAtCB(AtkTable *aTable, gint aRowIdx, gint aColIdx)
{
  if (aRowIdx < 0 || aColIdx < 0) {
    return -1;
  }

  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gint>(accWrap->AsTable()->ColExtentAt(aRowIdx, aColIdx));
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gint>(proxy->TableColumnExtentAt(aRowIdx, aColIdx));
  }

  return -1;
}

static gint
getRowExtentAtCB(AtkTable *aTable, gint aRowIdx, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gint>(accWrap->AsTable()->RowExtentAt(aRowIdx, aColIdx));
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gint>(proxy->TableRowExtentAt(aRowIdx, aColIdx));
  }

  return -1;
}

static AtkObject*
getCaptionCB(AtkTable* aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    Accessible* caption = accWrap->AsTable()->Caption();
    return caption ? AccessibleWrap::GetAtkObject(caption) : nullptr;
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    ProxyAccessible* caption = proxy->TableCaption();
    return caption ? GetWrapperFor(caption) : nullptr;
  }

  return nullptr;
}

static const gchar*
getColumnDescriptionCB(AtkTable *aTable, gint aColumn)
{
  nsAutoString autoStr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    accWrap->AsTable()->ColDescription(aColumn, autoStr);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    proxy->TableColumnDescription(aColumn, autoStr);
  } else {
    return nullptr;
  }

  return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject*
getColumnHeaderCB(AtkTable *aTable, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    Accessible* header =
      AccessibleWrap::GetColumnHeader(accWrap->AsTable(), aColIdx);
    return header ? AccessibleWrap::GetAtkObject(header) : nullptr;
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    ProxyAccessible* header = proxy->AtkTableColumnHeader(aColIdx);
    return header ? GetWrapperFor(header) : nullptr;
  }

  return nullptr;
}

static const gchar*
getRowDescriptionCB(AtkTable *aTable, gint aRow)
{
  nsAutoString autoStr;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    accWrap->AsTable()->RowDescription(aRow, autoStr);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    proxy->TableRowDescription(aRow, autoStr);
  } else {
    return nullptr;
  }

  return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject*
getRowHeaderCB(AtkTable *aTable, gint aRowIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    Accessible* header =
      AccessibleWrap::GetRowHeader(accWrap->AsTable(), aRowIdx);
    return header ? AccessibleWrap::GetAtkObject(header) : nullptr;
  }

  if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    ProxyAccessible* header = proxy->AtkTableRowHeader(aRowIdx);
    return header ? GetWrapperFor(header) : nullptr;
  }

  return nullptr;
}

static AtkObject*
getSummaryCB(AtkTable *aTable)
{
  // Neither html:table nor xul:tree nor ARIA grid/tree have an ability to
  // link an accessible object to specify a summary. There is closes method
  // in TableAccessible::summary to get a summary as a string which is not
  // mapped directly to ATK.
  return nullptr;
}

static gint
getSelectedColumnsCB(AtkTable *aTable, gint** aSelected)
{
  *aSelected = nullptr;

  nsAutoTArray<uint32_t, 10> cols;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    accWrap->AsTable()->SelectedColIndices(&cols);
   } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    proxy->TableSelectedColumnIndices(&cols);
  } else {
    return 0;
  }

  if (cols.IsEmpty())
    return 0;

  gint* atkColumns = g_new(gint, cols.Length());
  if (!atkColumns) {
    NS_WARNING("OUT OF MEMORY");
    return 0;
  }

  memcpy(atkColumns, cols.Elements(), cols.Length() * sizeof(uint32_t));
  *aSelected = atkColumns;
  return cols.Length();
}

static gint
getSelectedRowsCB(AtkTable *aTable, gint **aSelected)
{
  nsAutoTArray<uint32_t, 10> rows;
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    accWrap->AsTable()->SelectedRowIndices(&rows);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    proxy->TableSelectedRowIndices(&rows);
  } else {
    return 0;
  }

  gint* atkRows = g_new(gint, rows.Length());
  if (!atkRows) {
    NS_WARNING("OUT OF MEMORY");
    return 0;
  }

  memcpy(atkRows, rows.Elements(), rows.Length() * sizeof(uint32_t));
  *aSelected = atkRows;
  return rows.Length();
}

static gboolean
isColumnSelectedCB(AtkTable *aTable, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gboolean>(accWrap->AsTable()->IsColSelected(aColIdx));
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gboolean>(proxy->TableColumnSelected(aColIdx));
  }

  return FALSE;
}

static gboolean
isRowSelectedCB(AtkTable *aTable, gint aRowIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gboolean>(accWrap->AsTable()->IsRowSelected(aRowIdx));
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gboolean>(proxy->TableRowSelected(aRowIdx));
  }

  return FALSE;
}

static gboolean
isCellSelectedCB(AtkTable *aTable, gint aRowIdx, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (accWrap) {
    return static_cast<gboolean>(accWrap->AsTable()->
      IsCellSelected(aRowIdx, aColIdx));
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aTable))) {
    return static_cast<gboolean>(proxy->TableCellSelected(aRowIdx, aColIdx));
  }

  return FALSE;
}
}

void
tableInterfaceInitCB(AtkTableIface* aIface)
{
  NS_ASSERTION(aIface, "no interface!");
  if (MOZ_UNLIKELY(!aIface))
    return;

  aIface->ref_at = refAtCB;
  aIface->get_index_at = getIndexAtCB;
  aIface->get_column_at_index = getColumnAtIndexCB;
  aIface->get_row_at_index = getRowAtIndexCB;
  aIface->get_n_columns = getColumnCountCB;
  aIface->get_n_rows = getRowCountCB;
  aIface->get_column_extent_at = getColumnExtentAtCB;
  aIface->get_row_extent_at = getRowExtentAtCB;
  aIface->get_caption = getCaptionCB;
  aIface->get_column_description = getColumnDescriptionCB;
  aIface->get_column_header = getColumnHeaderCB;
  aIface->get_row_description = getRowDescriptionCB;
  aIface->get_row_header = getRowHeaderCB;
  aIface->get_summary = getSummaryCB;
  aIface->get_selected_columns = getSelectedColumnsCB;
  aIface->get_selected_rows = getSelectedRowsCB;
  aIface->is_column_selected = isColumnSelectedCB;
  aIface->is_row_selected = isRowSelectedCB;
  aIface->is_selected = isCellSelectedCB;
}
