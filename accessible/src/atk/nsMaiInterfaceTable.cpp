/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMaiInterfaceTable.h"

G_BEGIN_DECLS

/* table interface callbacks */
static void interfaceInitCB(AtkTableIface *aIface);
static AtkObject* refAtCB(AtkTable *aTable, gint aRow, gint aColumn);
static gint getIndexAtCB(AtkTable *aTable, gint aRow, gint aColumn);
static gint getColumnAtIndexCB(AtkTable *aTable, gint aIndex);
static gint getRowAtIndexCB(AtkTable *aTable, gint aIndex);
static gint getColumnCountCB(AtkTable *aTable);
static gint getRowCountCB(AtkTable *aTable);
static gint getColumnExtentAtCB(AtkTable *aTable, gint aRow, gint aColumn);
static gint getRowExtentAtCB(AtkTable *aTable, gint aRow, gint aColumn);
static AtkObject* getCaptionCB(AtkTable *aTable);
static const gchar* getColumnDescriptionCB(AtkTable *aTable, gint aColumn);
static AtkObject* getColumnHeaderCB(AtkTable *aTable, gint aColumn);
static const gchar* getRowDescriptionCB(AtkTable *aTable, gint aRow);
static AtkObject* getRowHeaderCB(AtkTable *aTable, gint aRow);
static AtkObject* getSummaryCB(AtkTable *aTable);
static gint getSelectedColumnsCB(AtkTable *aTable, gint **aSelected);
static gint getSelectedRowsCB(AtkTable *aTable, gint **aSelected);
static gboolean isColumnSelectedCB(AtkTable *aTable, gint aColumn);
static gboolean isRowSelectedCB(AtkTable *aTable, gint aRow);
static gboolean isCellSelectedCB(AtkTable *aTable, gint aRow, gint aColumn);

/* what are missing now for atk table */

/* ==================================================
   void              (* set_caption)              (AtkTable      *aTable,
   AtkObject     *caption);
   void              (* set_column_description)   (AtkTable      *aTable,
   gint          aColumn,
   const gchar   *description);
   void              (* set_column_header)        (AtkTable      *aTable,
   gint          aColumn,
   AtkObject     *header);
   void              (* set_row_description)      (AtkTable      *aTable,
   gint          aRow,
   const gchar   *description);
   void              (* set_row_header)           (AtkTable      *aTable,
   gint          aRow,
   AtkObject     *header);
   void              (* set_summary)              (AtkTable      *aTable,
   AtkObject     *accessible);
   gboolean          (* add_row_selection)        (AtkTable      *aTable,
   gint          aRow);
   gboolean          (* remove_row_selection)     (AtkTable      *aTable,
   gint          aRow);
   gboolean          (* add_column_selection)     (AtkTable      *aTable,
   gint          aColumn);
   gboolean          (* remove_column_selection)  (AtkTable      *aTable,
   gint          aColumn);

   ////////////////////////////////////////
   // signal handlers
   //
   void              (* row_inserted)           (AtkTable      *aTable,
   gint          aRow,
   gint          num_inserted);
   void              (* column_inserted)        (AtkTable      *aTable,
   gint          aColumn,
   gint          num_inserted);
   void              (* row_deleted)              (AtkTable      *aTable,
   gint          aRow,
   gint          num_deleted);
   void              (* column_deleted)           (AtkTable      *aTable,
   gint          aColumn,
   gint          num_deleted);
   void              (* row_reordered)            (AtkTable      *aTable);
   void              (* column_reordered)         (AtkTable      *aTable);
   void              (* model_changed)            (AtkTable      *aTable);

   * ==================================================
   */
G_END_DECLS

MaiInterfaceTable::MaiInterfaceTable(nsAccessibleWrap* aAccWrap):
    MaiInterface(aAccWrap)
{
}

MaiInterfaceTable::~MaiInterfaceTable()
{
}

MaiInterfaceType
MaiInterfaceTable::GetType()
{
    return MAI_INTERFACE_TABLE;
}

const GInterfaceInfo *
MaiInterfaceTable::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_table_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_table_info;
}

/* static functions */

void
interfaceInitCB(AtkTableIface *aIface)

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
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsCOMPtr<nsIAccessible> cell;
    nsresult rv = accTable->CellRefAt(aRow, aColumn,getter_AddRefs(cell));
    if (NS_FAILED(rv) || !cell)
        return nsnull;

    nsIAccessible *tmpAcc = cell;
    nsAccessibleWrap *cellAccWrap = NS_STATIC_CAST(nsAccessibleWrap *, tmpAcc);

    AtkObject *atkObj = cellAccWrap->GetAtkObject();
    if (atkObj)
        g_object_ref(atkObj);
    return atkObj;
}

gint
getIndexAtCB(AtkTable *aTable, gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 index;
    nsresult rv = accTable->GetIndexAt(aRow, aColumn, &index);
    NS_ENSURE_SUCCESS(rv, -1);

    return NS_STATIC_CAST(gint, index);
}

gint
getColumnAtIndexCB(AtkTable *aTable, gint aIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 col;
    nsresult rv = accTable->GetColumnAtIndex(aIndex, &col);
    NS_ENSURE_SUCCESS(rv, -1);

    return NS_STATIC_CAST(gint, col);
}

gint
getRowAtIndexCB(AtkTable *aTable, gint aIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 row;
    nsresult rv = accTable->GetRowAtIndex(aIndex, &row);
    NS_ENSURE_SUCCESS(rv, -1);

    return NS_STATIC_CAST(gint, row);
}

gint
getColumnCountCB(AtkTable *aTable)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 count;
    nsresult rv = accTable->GetColumns(&count);
    NS_ENSURE_SUCCESS(rv, -1);

    return NS_STATIC_CAST(gint, count);
}

gint
getRowCountCB(AtkTable *aTable)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 count;
    nsresult rv = accTable->GetRows(&count);
    NS_ENSURE_SUCCESS(rv, -1);

    return NS_STATIC_CAST(gint, count);
}

gint
getColumnExtentAtCB(AtkTable *aTable,
                    gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 extent;
    nsresult rv = accTable->GetColumnExtentAt(aRow, aColumn, &extent);
    NS_ENSURE_SUCCESS(rv, -1);

    return NS_STATIC_CAST(gint, extent);
}

gint
getRowExtentAtCB(AtkTable *aTable,
                 gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, -1);

    PRInt32 extent;
    nsresult rv = accTable->GetRowExtentAt(aRow, aColumn, &extent);
    NS_ENSURE_SUCCESS(rv, -1);

    return NS_STATIC_CAST(gint, extent);
}

AtkObject*
getCaptionCB(AtkTable *aTable)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsCOMPtr<nsIAccessible> caption;
    nsresult rv = accTable->GetCaption(getter_AddRefs(caption));
    if (NS_FAILED(rv) || !caption)
        return nsnull;

    nsIAccessible *tmpAcc = caption;
    nsAccessibleWrap *captionAccWrap =
        NS_STATIC_CAST(nsAccessibleWrap *, tmpAcc);

    return captionAccWrap->GetAtkObject();
}

const gchar*
getColumnDescriptionCB(AtkTable *aTable, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    MaiInterfaceTable *maiTable =
        NS_STATIC_CAST(MaiInterfaceTable *,
                       accWrap->GetMaiInterface(MAI_INTERFACE_TABLE));
    NS_ENSURE_TRUE(maiTable, nsnull);

    const char *description = maiTable->GetColumnDescription();
    if (!description) {
        nsAutoString autoStr;
        nsresult rv = accTable->GetColumnDescription(aColumn, autoStr);
        NS_ENSURE_SUCCESS(rv, nsnull);

        maiTable->SetColumnDescription(autoStr);
        description = maiTable->GetColumnDescription();
    }
    return description;
}

AtkObject*
getColumnHeaderCB(AtkTable *aTable, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsCOMPtr<nsIAccessibleTable> header;
    nsresult rv = accTable->GetColumnHeader(getter_AddRefs(header));
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsCOMPtr<nsIAccessible> accHeader(do_QueryInterface(header));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsIAccessible *tmpAcc = accHeader;
    nsAccessibleWrap *headerAccWrap =
        NS_STATIC_CAST(nsAccessibleWrap *, tmpAcc);

    return headerAccWrap->GetAtkObject();
}

const gchar*
getRowDescriptionCB(AtkTable *aTable, gint aRow)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    MaiInterfaceTable *maiTable =
        NS_STATIC_CAST(MaiInterfaceTable *,
                       accWrap->GetMaiInterface(MAI_INTERFACE_TABLE));
    NS_ENSURE_TRUE(maiTable, nsnull);

    const char *description = maiTable->GetRowDescription();
    if (!description) {
        nsAutoString autoStr;
        nsresult rv = accTable->GetRowDescription(aRow, autoStr);
        NS_ENSURE_SUCCESS(rv, nsnull);

        maiTable->SetRowDescription(autoStr);
        description = maiTable->GetRowDescription();
    }
    return description;
}

AtkObject*
getRowHeaderCB(AtkTable *aTable, gint aRow)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsCOMPtr<nsIAccessibleTable> header;
    nsresult rv = accTable->GetRowHeader(getter_AddRefs(header));
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsCOMPtr<nsIAccessible> accHeader(do_QueryInterface(header));
    NS_ENSURE_TRUE(accTable, nsnull);

    nsIAccessible *tmpAcc = accHeader;
    nsAccessibleWrap *headerAccWrap =
        NS_STATIC_CAST(nsAccessibleWrap *, tmpAcc);

    return headerAccWrap->GetAtkObject();
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
    NS_ENSURE_TRUE(accWrap, 0);

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
        atkColumns[index] = NS_STATIC_CAST(gint, columns[index]);
    nsMemory::Free(columns);

    *aSelected = atkColumns;
    return size;
}

gint
getSelectedRowsCB(AtkTable *aTable, gint **aSelected)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, 0);

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
        atkRows[index] = NS_STATIC_CAST(gint, rows[index]);
    nsMemory::Free(rows);

    *aSelected = atkRows;
    return size;
}

gboolean
isColumnSelectedCB(AtkTable *aTable, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    PRBool outValue;
    nsresult rv = accTable->IsColumnSelected(aColumn, &outValue);
    return NS_FAILED(rv) ? FALSE : NS_STATIC_CAST(gboolean, outValue);
}

gboolean
isRowSelectedCB(AtkTable *aTable, gint aRow)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    PRBool outValue;
    nsresult rv = accTable->IsRowSelected(aRow, &outValue);
    return NS_FAILED(rv) ? FALSE : NS_STATIC_CAST(gboolean, outValue);
}

gboolean
isCellSelectedCB(AtkTable *aTable, gint aRow, gint aColumn)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aTable));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsCOMPtr<nsIAccessibleTable> accTable;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleTable),
                            getter_AddRefs(accTable));
    NS_ENSURE_TRUE(accTable, FALSE);

    PRBool outValue;
    nsresult rv = accTable->IsCellSelected(aRow, aColumn, &outValue);
    return NS_FAILED(rv) ? FALSE : NS_STATIC_CAST(gboolean, outValue);
}
