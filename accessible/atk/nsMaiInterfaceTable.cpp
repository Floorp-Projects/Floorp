/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "AccessibleWrap.h"
#include "mozilla/a11y/TableAccessible.h"
#include "nsAccessibilityService.h"
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
  Accessible* cell = acc->AsTable()->CellAt(aRowIdx, aColIdx);
  if (!cell) {
    return nullptr;
  }

  cellAtkObj = GetWrapperFor(cell);

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
  return static_cast<gint>(acc->AsTable()->CellIndexAt(aRowIdx, aColIdx));
}

static gint getColumnAtIndexCB(AtkTable* aTable, gint aIdx) {
  if (aIdx < 0) {
    return -1;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  return static_cast<gint>(acc->AsTable()->ColIndexAt(aIdx));
}

static gint getRowAtIndexCB(AtkTable* aTable, gint aIdx) {
  if (aIdx < 0) {
    return -1;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  return static_cast<gint>(acc->AsTable()->RowIndexAt(aIdx));
}

static gint getColumnCountCB(AtkTable* aTable) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  return static_cast<gint>(acc->AsTable()->ColCount());
}

static gint getRowCountCB(AtkTable* aTable) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  return static_cast<gint>(acc->AsTable()->RowCount());
}

static gint getColumnExtentAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx) {
  if (aRowIdx < 0 || aColIdx < 0) {
    return -1;
  }

  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  return static_cast<gint>(acc->AsTable()->ColExtentAt(aRowIdx, aColIdx));
}

static gint getRowExtentAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return -1;
  }
  return static_cast<gint>(acc->AsTable()->RowExtentAt(aRowIdx, aColIdx));
}

static AtkObject* getCaptionCB(AtkTable* aTable) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  Accessible* caption = acc->AsTable()->Caption();
  return caption ? GetWrapperFor(caption) : nullptr;
}

static const gchar* getColumnDescriptionCB(AtkTable* aTable, gint aColumn) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  nsAutoString autoStr;
  acc->AsTable()->ColDescription(aColumn, autoStr);
  return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject* getColumnHeaderCB(AtkTable* aTable, gint aColIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  Accessible* header = AccessibleWrap::GetColumnHeader(acc->AsTable(), aColIdx);
  return header ? GetWrapperFor(header) : nullptr;
}

static const gchar* getRowDescriptionCB(AtkTable* aTable, gint aRow) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  nsAutoString autoStr;
  acc->AsTable()->RowDescription(aRow, autoStr);
  return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject* getRowHeaderCB(AtkTable* aTable, gint aRowIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return nullptr;
  }
  Accessible* header = AccessibleWrap::GetRowHeader(acc->AsTable(), aRowIdx);
  return header ? GetWrapperFor(header) : nullptr;
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

  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return 0;
  }
  AutoTArray<uint32_t, 10> cols;
  acc->AsTable()->SelectedColIndices(&cols);

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
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return 0;
  }
  AutoTArray<uint32_t, 10> rows;
  acc->AsTable()->SelectedRowIndices(&rows);

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
  return static_cast<gboolean>(acc->AsTable()->IsColSelected(aColIdx));
}

static gboolean isRowSelectedCB(AtkTable* aTable, gint aRowIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return FALSE;
  }
  return static_cast<gboolean>(acc->AsTable()->IsRowSelected(aRowIdx));
}

static gboolean isCellSelectedCB(AtkTable* aTable, gint aRowIdx, gint aColIdx) {
  Accessible* acc = GetInternalObj(ATK_OBJECT(aTable));
  if (!acc) {
    return FALSE;
  }
  return static_cast<gboolean>(
      acc->AsTable()->IsCellSelected(aRowIdx, aColIdx));
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
