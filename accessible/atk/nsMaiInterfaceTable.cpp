/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "AccessibleWrap.h"
#include "mozilla/a11y/TableAccessibleBase.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsMai.h"
#include "RemoteAccessible.h"
#include "nsTArray.h"

#include "mozilla/Likely.h"

using namespace mozilla;
using namespace mozilla::a11y;

extern "C" {
static AtkObject* refAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx) {
  if (aRowIdx < 0 || aColIdx < 0) {
    return nullptr;
  }

  AtkObject* cellAtkObj = nullptr;
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    Accessible* cell = acc->AsTableBase()->CellAt(aRowIdx, aColIdx);
    if (!cell) {
      return nullptr;
    }

    cellAtkObj = GetWrapperFor(cell);
  } else if (RemoteAccessible* proxy = acc->AsRemote()) {
    RemoteAccessible* cell = proxy->TableCellAt(aRowIdx, aColIdx);
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

static gint getIndexAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx) {
  if (aRowIdx < 0 || aColIdx < 0) {
    return -1;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableBase()->CellIndexAt(aRowIdx, aColIdx));
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gint>(proxy->TableCellIndexAt(aRowIdx, aColIdx));
  }

  return -1;
}

static gint getColumnAtIndexCB(AtkTable* aTable, gint aIdx) {
  if (aIdx < 0) {
    return -1;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableBase()->ColIndexAt(aIdx));
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gint>(proxy->TableColumnIndexAt(aIdx));
  }

  return -1;
}

static gint getRowAtIndexCB(AtkTable* aTable, gint aIdx) {
  if (aIdx < 0) {
    return -1;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableBase()->RowIndexAt(aIdx));
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gint>(proxy->TableRowIndexAt(aIdx));
  }

  return -1;
}

static gint getColumnCountCB(AtkTable* aTable) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableBase()->ColCount());
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gint>(proxy->TableColumnCount());
  }

  return -1;
}

static gint getRowCountCB(AtkTable* aTable) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableBase()->RowCount());
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gint>(proxy->TableRowCount());
  }

  return -1;
}

static gint getColumnExtentAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx) {
  if (aRowIdx < 0 || aColIdx < 0) {
    return -1;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableBase()->ColExtentAt(aRowIdx, aColIdx));
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gint>(proxy->TableColumnExtentAt(aRowIdx, aColIdx));
  }

  return -1;
}

static gint getRowExtentAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gint>(acc->AsTableBase()->RowExtentAt(aRowIdx, aColIdx));
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gint>(proxy->TableRowExtentAt(aRowIdx, aColIdx));
  }

  return -1;
}

static AtkObject* getCaptionCB(AtkTable* aTable) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    Accessible* caption = acc->AsTableBase()->Caption();
    return caption ? GetWrapperFor(caption) : nullptr;
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    RemoteAccessible* caption = proxy->TableCaption();
    return caption ? GetWrapperFor(caption) : nullptr;
  }

  return nullptr;
}

static const gchar* getColumnDescriptionCB(AtkTable* aTable, gint aColumn) {
  nsAutoString autoStr;
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    acc->AsTableBase()->ColDescription(aColumn, autoStr);
  } else if (RemoteAccessible* proxy = acc->AsRemote()) {
    proxy->TableColumnDescription(aColumn, autoStr);
  }

  return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject* getColumnHeaderCB(AtkTable* aTable, gint aColIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    Accessible* header =
        AccessibleWrap::GetColumnHeader(acc->AsTableBase(), aColIdx);
    return header ? GetWrapperFor(header) : nullptr;
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    RemoteAccessible* header = proxy->AtkTableColumnHeader(aColIdx);
    return header ? GetWrapperFor(header) : nullptr;
  }

  return nullptr;
}

static const gchar* getRowDescriptionCB(AtkTable* aTable, gint aRow) {
  nsAutoString autoStr;
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    acc->AsTableBase()->RowDescription(aRow, autoStr);
  } else if (RemoteAccessible* proxy = acc->AsRemote()) {
    proxy->TableRowDescription(aRow, autoStr);
  }

  return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject* getRowHeaderCB(AtkTable* aTable, gint aRowIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    Accessible* header =
        AccessibleWrap::GetRowHeader(acc->AsTableBase(), aRowIdx);
    return header ? GetWrapperFor(header) : nullptr;
  }

  if (RemoteAccessible* proxy = acc->AsRemote()) {
    RemoteAccessible* header = proxy->AtkTableRowHeader(aRowIdx);
    return header ? GetWrapperFor(header) : nullptr;
  }

  return nullptr;
}

static AtkObject* getSummaryCB(AtkTable* aTable) {
  // Neither html:table nor xul:tree nor ARIA grid/tree have an ability to
  // link an accessible object to specify a summary. There is closes method
  // in TableAccessible::summary to get a summary as a string which is not
  // mapped directly to ATK.
  return nullptr;
}

static gint getSelectedColumnsCB(AtkTable* aTable, gint** aSelected) {
  *aSelected = nullptr;

  AutoTArray<uint32_t, 10> cols;
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return 0;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    acc->AsTableBase()->SelectedColIndices(&cols);
  } else if (RemoteAccessible* proxy = acc->AsRemote()) {
    proxy->TableSelectedColumnIndices(&cols);
  }

  if (cols.IsEmpty()) return 0;

  gint* atkColumns = g_new(gint, cols.Length());
  if (!atkColumns) {
    NS_WARNING("OUT OF MEMORY");
    return 0;
  }

  memcpy(atkColumns, cols.Elements(), cols.Length() * sizeof(uint32_t));
  *aSelected = atkColumns;
  return cols.Length();
}

static gint getSelectedRowsCB(AtkTable* aTable, gint** aSelected) {
  AutoTArray<uint32_t, 10> rows;
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return 0;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    acc->AsTableBase()->SelectedRowIndices(&rows);
  } else if (RemoteAccessible* proxy = acc->AsRemote()) {
    proxy->TableSelectedRowIndices(&rows);
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

static gboolean isColumnSelectedCB(AtkTable* aTable, gint aColIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return FALSE;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gboolean>(acc->AsTableBase()->IsColSelected(aColIdx));
  }
  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gboolean>(proxy->TableColumnSelected(aColIdx));
  }

  return FALSE;
}

static gboolean isRowSelectedCB(AtkTable* aTable, gint aRowIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return FALSE;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gboolean>(acc->AsTableBase()->IsRowSelected(aRowIdx));
  }
  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gboolean>(proxy->TableRowSelected(aRowIdx));
  }

  return FALSE;
}

static gboolean isCellSelectedCB(AtkTable* aTable, gint aRowIdx, gint aColIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return FALSE;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() || acc->IsLocal()) {
    return static_cast<gboolean>(
        acc->AsTableBase()->IsCellSelected(aRowIdx, aColIdx));
  }
  if (RemoteAccessible* proxy = acc->AsRemote()) {
    return static_cast<gboolean>(proxy->TableCellSelected(aRowIdx, aColIdx));
  }

  return FALSE;
}
}

void tableInterfaceInitCB(AtkTableIface* aIface) {
  NS_ASSERTION(aIface, "no interface!");
  if (MOZ_UNLIKELY(!aIface)) return;

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
