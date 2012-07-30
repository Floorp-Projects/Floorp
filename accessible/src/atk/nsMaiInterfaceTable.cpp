/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "AccessibleWrap.h"
#include "nsAccUtils.h"
#include "nsIAccessibleTable.h"
#include "TableAccessible.h"
#include "nsMai.h"

#include "nsArrayUtils.h"

using namespace mozilla::a11y;

extern "C" {
static AtkObject*
refAtCB(AtkTable *aTable, gint aRow, gint aColumn)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nullptr);

    nsCOMPtr<nsIAccessible> cell;
    nsresult rv = accTable->GetCellAt(aRow, aColumn,getter_AddRefs(cell));
    if (NS_FAILED(rv) || !cell)
        return nullptr;

    AtkObject* cellAtkObj = AccessibleWrap::GetAtkObject(cell);
    if (cellAtkObj) {
        g_object_ref(cellAtkObj);
    }
    return cellAtkObj;
}

static gint
getIndexAtCB(AtkTable* aTable, gint aRow, gint aColumn)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

  TableAccessible* table = accWrap->AsTable();
  NS_ENSURE_TRUE(table, -1);

  return static_cast<gint>(table->CellIndexAt(aRow, aColumn));
}

static gint
getColumnAtIndexCB(AtkTable *aTable, gint aIndex)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 col;
    nsresult rv = accTable->GetColumnIndexAt(aIndex, &col);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(col);
}

static gint
getRowAtIndexCB(AtkTable *aTable, gint aIndex)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 row;
    nsresult rv = accTable->GetRowIndexAt(aIndex, &row);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(row);
}

static gint
getColumnCountCB(AtkTable *aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 count;
    nsresult rv = accTable->GetColumnCount(&count);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(count);
}

static gint
getRowCountCB(AtkTable *aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 count;
    nsresult rv = accTable->GetRowCount(&count);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(count);
}

static gint
getColumnExtentAtCB(AtkTable *aTable,
                    gint aRow, gint aColumn)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 extent;
    nsresult rv = accTable->GetColumnExtentAt(aRow, aColumn, &extent);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(extent);
}

static gint
getRowExtentAtCB(AtkTable *aTable,
                 gint aRow, gint aColumn)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 extent;
    nsresult rv = accTable->GetRowExtentAt(aRow, aColumn, &extent);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(extent);
}

static AtkObject*
getCaptionCB(AtkTable* aTable)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

  TableAccessible* table = accWrap->AsTable();
  NS_ENSURE_TRUE(table, nullptr);

  Accessible* caption = table->Caption();
  return caption ? AccessibleWrap::GetAtkObject(caption) : nullptr;
}

static const gchar*
getColumnDescriptionCB(AtkTable *aTable, gint aColumn)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nullptr);

    nsAutoString autoStr;
    nsresult rv = accTable->GetColumnDescription(aColumn, autoStr);
    NS_ENSURE_SUCCESS(rv, nullptr);

    return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject*
getColumnHeaderCB(AtkTable *aTable, gint aColumn)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nullptr);

    nsCOMPtr<nsIAccessible> accCell;
    accTable->GetCellAt(0, aColumn, getter_AddRefs(accCell));
    if (!accCell)
        return nullptr;

    // If the cell at the first row is column header then assume it is column
    // header for all rows,
    if (nsAccUtils::Role(accCell) == nsIAccessibleRole::ROLE_COLUMNHEADER)
        return AccessibleWrap::GetAtkObject(accCell);

    // otherwise get column header for the data cell at the first row.
    nsCOMPtr<nsIAccessibleTableCell> accTableCell =
        do_QueryInterface(accCell);

    if (accTableCell) {
        nsCOMPtr<nsIArray> headerCells;
        accTableCell->GetColumnHeaderCells(getter_AddRefs(headerCells));
        if (headerCells) {
            nsresult rv;
            nsCOMPtr<nsIAccessible> accHeaderCell =
                do_QueryElementAt(headerCells, 0, &rv);
            NS_ENSURE_SUCCESS(rv, nullptr);

            return AccessibleWrap::GetAtkObject(accHeaderCell);
        }
    }

    return nullptr;
}

static const gchar*
getRowDescriptionCB(AtkTable *aTable, gint aRow)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nullptr);

    nsAutoString autoStr;
    nsresult rv = accTable->GetRowDescription(aRow, autoStr);
    NS_ENSURE_SUCCESS(rv, nullptr);

    return AccessibleWrap::ReturnString(autoStr);
}

static AtkObject*
getRowHeaderCB(AtkTable *aTable, gint aRow)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return nullptr;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nullptr);

    nsCOMPtr<nsIAccessible> accCell;
    accTable->GetCellAt(aRow, 0, getter_AddRefs(accCell));
    if (!accCell)
      return nullptr;

    // If the cell at the first column is row header then assume it is row
    // header for all columns,
    if (nsAccUtils::Role(accCell) == nsIAccessibleRole::ROLE_ROWHEADER)
        return AccessibleWrap::GetAtkObject(accCell);

    // otherwise get row header for the data cell at the first column.
    nsCOMPtr<nsIAccessibleTableCell> accTableCell =
        do_QueryInterface(accCell);

    if (accTableCell) {
      nsCOMPtr<nsIArray> headerCells;
      accTableCell->GetRowHeaderCells(getter_AddRefs(headerCells));
      if (headerCells) {
        nsresult rv;
        nsCOMPtr<nsIAccessible> accHeaderCell =
            do_QueryElementAt(headerCells, 0, &rv);
        NS_ENSURE_SUCCESS(rv, nullptr);

        return AccessibleWrap::GetAtkObject(accHeaderCell);
      }
    }

    return nullptr;
}

static AtkObject*
getSummaryCB(AtkTable *aTable)
{
  // Neither html:table nor xul:tree nor ARIA grid/tree have an ability to
  // link an accessible object to specify a summary. There is closes method
  // in nsIAccessibleTable::summary to get a summary as a string which is not
  // mapped directly to ATK.
  return nullptr;
}

static gint
getSelectedColumnsCB(AtkTable *aTable, gint **aSelected)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return 0;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, 0);

    PRUint32 size = 0;
    PRInt32 *columns = NULL;
    nsresult rv = accTable->GetSelectedColumnIndices(&size, &columns);
    if (NS_FAILED(rv) || (size == 0) || !columns) {
        *aSelected = nullptr;
        return 0;
    }

    gint *atkColumns = g_new(gint, size);
    if (!atkColumns) {
        NS_WARNING("OUT OF MEMORY");
        return 0;
    }

    //copy
    for (PRUint32 index = 0; index < size; ++index)
        atkColumns[index] = static_cast<gint>(columns[index]);
    nsMemory::Free(columns);

    *aSelected = atkColumns;
    return size;
}

static gint
getSelectedRowsCB(AtkTable *aTable, gint **aSelected)
{
    AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return 0;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, 0);

    PRUint32 size = 0;
    PRInt32 *rows = NULL;
    nsresult rv = accTable->GetSelectedRowIndices(&size, &rows);
    if (NS_FAILED(rv) || (size == 0) || !rows) {
        *aSelected = nullptr;
        return 0;
    }

    gint *atkRows = g_new(gint, size);
    if (!atkRows) {
        NS_WARNING("OUT OF MEMORY");
        return 0;
    }

    //copy
    for (PRUint32 index = 0; index < size; ++index)
        atkRows[index] = static_cast<gint>(rows[index]);
    nsMemory::Free(rows);

    *aSelected = atkRows;
    return size;
}

static gboolean
isColumnSelectedCB(AtkTable *aTable, gint aColumn)
{
    AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    bool outValue;
    nsresult rv = accTable->IsColumnSelected(aColumn, &outValue);
    return NS_FAILED(rv) ? FALSE : static_cast<gboolean>(outValue);
}

static gboolean
isRowSelectedCB(AtkTable *aTable, gint aRow)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return FALSE;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    bool outValue;
    nsresult rv = accTable->IsRowSelected(aRow, &outValue);
    return NS_FAILED(rv) ? FALSE : static_cast<gboolean>(outValue);
}

static gboolean
isCellSelectedCB(AtkTable *aTable, gint aRow, gint aColumn)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
  if (!accWrap)
    return FALSE;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    bool outValue;
    nsresult rv = accTable->IsCellSelected(aRow, aColumn, &outValue);
    return NS_FAILED(rv) ? FALSE : static_cast<gboolean>(outValue);
}
}

void
tableInterfaceInitCB(AtkTableIface* aIface)
{
  NS_ASSERTION(aIface, "no interface!");
  if (NS_UNLIKELY(!aIface))
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
