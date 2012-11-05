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

#include "nsArrayUtils.h"

#include "mozilla/Likely.h"

using namespace mozilla::a11y;

extern "C" {
static AtkObject*
refAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap || aRowIdx < 0 || aColIdx < 0)
    return nullptr;

  Accessible* cell = accWrap->AsTable()->CellAt(aRowIdx, aColIdx);
  if (!cell)
    return nullptr;

  AtkObject* cellAtkObj = AccessibleWrap::GetAtkObject(cell);
  if (cellAtkObj)
    g_object_ref(cellAtkObj);

  return cellAtkObj;
}

static gint
getIndexAtCB(AtkTable* aTable, gint aRowIdx, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap || aRowIdx < 0 || aColIdx < 0)
    return -1;

  return static_cast<gint>(accWrap->AsTable()->CellIndexAt(aRowIdx, aColIdx));
}

static gint
getColumnAtIndexCB(AtkTable *aTable, gint aIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap || aIdx < 0)
    return -1;

    return static_cast<gint>(accWrap->AsTable()->ColIndexAt(aIdx));
}

static gint
getRowAtIndexCB(AtkTable *aTable, gint aIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap || aIdx < 0)
    return -1;

    return static_cast<gint>(accWrap->AsTable()->RowIndexAt(aIdx));
}

static gint
getColumnCountCB(AtkTable *aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

    return static_cast<gint>(accWrap->AsTable()->ColCount());
}

static gint
getRowCountCB(AtkTable *aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

    return static_cast<gint>(accWrap->AsTable()->RowCount());
}

static gint
getColumnExtentAtCB(AtkTable *aTable, gint aRowIdx, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap || aRowIdx < 0 || aColIdx < 0)
    return -1;

    return static_cast<gint>(accWrap->AsTable()->ColExtentAt(aRowIdx, aColIdx));
}

static gint
getRowExtentAtCB(AtkTable *aTable, gint aRowIdx, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

  return static_cast<gint>(accWrap->AsTable()->RowExtentAt(aRowIdx, aColIdx));
}

static AtkObject*
getCaptionCB(AtkTable* aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

  Accessible* caption = accWrap->AsTable()->Caption();
  return caption ? AccessibleWrap::GetAtkObject(caption) : nullptr;
}

static const gchar*
getColumnDescriptionCB(AtkTable *aTable, gint aColumn)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

  nsAutoString autoStr;
  accWrap->AsTable()->ColDescription(aColumn, autoStr);

  return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject*
getColumnHeaderCB(AtkTable *aTable, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

  Accessible* cell = accWrap->AsTable()->CellAt(0, aColIdx);
  if (!cell)
    return nullptr;

  // If the cell at the first row is column header then assume it is column
  // header for all rows,
  if (cell->Role() == roles::COLUMNHEADER)
    return AccessibleWrap::GetAtkObject(cell);

  // otherwise get column header for the data cell at the first row.
  TableCellAccessible* tableCell = cell->AsTableCell();
  if (!tableCell)
    return nullptr;

  nsAutoTArray<Accessible*, 10> headerCells;
  tableCell->ColHeaderCells(&headerCells);
  if (headerCells.IsEmpty())
    return nullptr;

  return AccessibleWrap::GetAtkObject(headerCells[0]);
}

static const gchar*
getRowDescriptionCB(AtkTable *aTable, gint aRow)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

  nsAutoString autoStr;
  accWrap->AsTable()->RowDescription(aRow, autoStr);

  return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject*
getRowHeaderCB(AtkTable *aTable, gint aRowIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

  Accessible* cell = accWrap->AsTable()->CellAt(aRowIdx, 0);
  if (!cell)
    return nullptr;

  // If the cell at the first column is row header then assume it is row
  // header for all columns,
  if (cell->Role() == roles::ROWHEADER)
    return AccessibleWrap::GetAtkObject(cell);

  // otherwise get row header for the data cell at the first column.
  TableCellAccessible* tableCell = cell->AsTableCell();
  if (!tableCell)
    return nullptr;

  nsAutoTArray<Accessible*, 10> headerCells;
  tableCell->RowHeaderCells(&headerCells);
  if (headerCells.IsEmpty())
    return nullptr;

  return AccessibleWrap::GetAtkObject(headerCells[0]);
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

  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return 0;

  nsAutoTArray<uint32_t, 10> cols;
  accWrap->AsTable()->SelectedColIndices(&cols);
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
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return 0;

  nsAutoTArray<uint32_t, 10> rows;
  accWrap->AsTable()->SelectedRowIndices(&rows);

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
  if (!accWrap)
    return FALSE;

  return static_cast<gboolean>(accWrap->AsTable()->IsColSelected(aColIdx));
}

static gboolean
isRowSelectedCB(AtkTable *aTable, gint aRowIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return FALSE;

  return static_cast<gboolean>(accWrap->AsTable()->IsRowSelected(aRowIdx));
}

static gboolean
isCellSelectedCB(AtkTable *aTable, gint aRowIdx, gint aColIdx)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return FALSE;

    return static_cast<gboolean>(accWrap->AsTable()->
      IsCellSelected(aRowIdx, aColIdx));
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
