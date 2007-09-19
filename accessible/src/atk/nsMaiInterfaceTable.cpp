/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMaiInterfaceTable.h"

void
tableInterfaceInitCB(AtkTableIface *aIface)

{
    g_return_if_fail(aIface != NULL);

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

/* static */
AtkObject*
refAtCB(AtkTable *aTable, gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsCOMPtr<nsIAccessible> cell;
    nsresult rv = accTable->CellRefAt(aRow, aColumn,getter_AddRefs(cell));
    if (NS_FAILED(rv) || !cell)
        return nsnull;

    AtkObject *cellAtkObj = nsAccessibleWrap::GetAtkObject(cell);
    if (cellAtkObj) {
        g_object_ref(cellAtkObj);
    }
    return cellAtkObj;
}

gint
getIndexAtCB(AtkTable *aTable, gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 index;
    nsresult rv = accTable->GetIndexAt(aRow, aColumn, &index);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(index);
}

gint
getColumnAtIndexCB(AtkTable *aTable, gint aIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 col;
    nsresult rv = accTable->GetColumnAtIndex(aIndex, &col);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(col);
}

gint
getRowAtIndexCB(AtkTable *aTable, gint aIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 row;
    nsresult rv = accTable->GetRowAtIndex(aIndex, &row);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(row);
}

gint
getColumnCountCB(AtkTable *aTable)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 count;
    nsresult rv = accTable->GetColumns(&count);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(count);
}

gint
getRowCountCB(AtkTable *aTable)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return -1;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 count;
    nsresult rv = accTable->GetRows(&count);
    NS_ENSURE_SUCCESS(rv, -1);

    return static_cast<gint>(count);
}

gint
getColumnExtentAtCB(AtkTable *aTable,
                    gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
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

gint
getRowExtentAtCB(AtkTable *aTable,
                 gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
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

AtkObject*
getCaptionCB(AtkTable *aTable)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsCOMPtr<nsIAccessible> caption;
    nsresult rv = accTable->GetCaption(getter_AddRefs(caption));
    if (NS_FAILED(rv) || !caption)
        return nsnull;

    return nsAccessibleWrap::GetAtkObject(caption);
}

const gchar*
getColumnDescriptionCB(AtkTable *aTable, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsAutoString autoStr;
    nsresult rv = accTable->GetColumnDescription(aColumn, autoStr);
    NS_ENSURE_SUCCESS(rv, nsnull);

    return nsAccessibleWrap::ReturnString(autoStr);
}

AtkObject*
getColumnHeaderCB(AtkTable *aTable, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsCOMPtr<nsIAccessibleTable> header;
    nsresult rv = accTable->GetColumnHeader(getter_AddRefs(header));
    NS_ENSURE_SUCCESS(rv, nsnull);
    NS_ENSURE_TRUE(header, nsnull);

    // Note: "table column header" has different definition between atk and mai
    //
    // 1. "getColumnHeaderCB" defined in AtkTableIface should return object
    // whose role is "ATK_ROLE_TABLE_COLUMN_HEADER", which is implemented
    // by nsXULTreeColumnItemAccessible.
    //
    // 2. "GetColumnHeader" defined in nsIAccessibleTable returns
    // nsXULTreeColumnsAccessibleWrap, which exports nsIAccessibleTable and is
    // "ROLE_LIST".
    nsCOMPtr<nsIAccessible> accHeader;
    header->CellRefAt(0, aColumn, getter_AddRefs(accHeader));
    NS_ENSURE_TRUE(accHeader, nsnull);

    return nsAccessibleWrap::GetAtkObject(accHeader);
}

const gchar*
getRowDescriptionCB(AtkTable *aTable, gint aRow)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsAutoString autoStr;
    nsresult rv = accTable->GetRowDescription(aRow, autoStr);
    NS_ENSURE_SUCCESS(rv, nsnull);

    return nsAccessibleWrap::ReturnString(autoStr);
}

AtkObject*
getRowHeaderCB(AtkTable *aTable, gint aRow)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsCOMPtr<nsIAccessibleTable> header;
    nsresult rv = accTable->GetRowHeader(getter_AddRefs(header));
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsCOMPtr<nsIAccessible> accHeader(do_QueryInterface(header));
    NS_ENSURE_TRUE(accHeader, nsnull);

    return nsAccessibleWrap::GetAtkObject(accHeader);
}

AtkObject*
getSummaryCB(AtkTable *aTable)
{
    /* ??? in nsIAccessibleTable, it returns a nsAString */
    return nsnull;
}

gint
getSelectedColumnsCB(AtkTable *aTable, gint **aSelected)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return 0;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, 0);

    PRUint32 size = 0;
    PRInt32 *columns = NULL;
    nsresult rv = accTable->GetSelectedColumns(&size, &columns);
    if (NS_FAILED(rv) || (size == 0) || !columns) {
        *aSelected = nsnull;
        return 0;
    }

    gint *atkColumns = g_new(gint, size);
    if (!atkColumns) {
        NS_WARNING("OUT OF MEMORY");
        return nsnull;
    }

    //copy
    for (PRUint32 index = 0; index < size; ++index)
        atkColumns[index] = static_cast<gint>(columns[index]);
    nsMemory::Free(columns);

    *aSelected = atkColumns;
    return size;
}

gint
getSelectedRowsCB(AtkTable *aTable, gint **aSelected)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return 0;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, 0);

    PRUint32 size = 0;
    PRInt32 *rows = NULL;
    nsresult rv = accTable->GetSelectedRows(&size, &rows);
    if (NS_FAILED(rv) || (size == 0) || !rows) {
        *aSelected = nsnull;
        return 0;
    }

    gint *atkRows = g_new(gint, size);
    if (!atkRows) {
        NS_WARNING("OUT OF MEMORY");
        return nsnull;
    }

    //copy
    for (PRUint32 index = 0; index < size; ++index)
        atkRows[index] = static_cast<gint>(rows[index]);
    nsMemory::Free(rows);

    *aSelected = atkRows;
    return size;
}

gboolean
isColumnSelectedCB(AtkTable *aTable, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    PRBool outValue;
    nsresult rv = accTable->IsColumnSelected(aColumn, &outValue);
    return NS_FAILED(rv) ? FALSE : static_cast<gboolean>(outValue);
}

gboolean
isRowSelectedCB(AtkTable *aTable, gint aRow)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    PRBool outValue;
    nsresult rv = accTable->IsRowSelected(aRow, &outValue);
    return NS_FAILED(rv) ? FALSE : static_cast<gboolean>(outValue);
}

gboolean
isCellSelectedCB(AtkTable *aTable, gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    PRBool outValue;
    nsresult rv = accTable->IsCellSelected(aRow, aColumn, &outValue);
    return NS_FAILED(rv) ? FALSE : static_cast<gboolean>(outValue);
}
