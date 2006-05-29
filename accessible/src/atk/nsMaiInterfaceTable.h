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

#ifndef __MAI_INTERFACE_TABLE_H__
#define __MAI_INTERFACE_TABLE_H__

#include "nsMai.h"
#include "nsIAccessibleTable.h"

G_BEGIN_DECLS

/* table interface callbacks */
void tableInterfaceInitCB(AtkTableIface *aIface);
AtkObject* refAtCB(AtkTable *aTable, gint aRow, gint aColumn);
gint getIndexAtCB(AtkTable *aTable, gint aRow, gint aColumn);
gint getColumnAtIndexCB(AtkTable *aTable, gint aIndex);
gint getRowAtIndexCB(AtkTable *aTable, gint aIndex);
gint getColumnCountCB(AtkTable *aTable);
gint getRowCountCB(AtkTable *aTable);
gint getColumnExtentAtCB(AtkTable *aTable, gint aRow, gint aColumn);
gint getRowExtentAtCB(AtkTable *aTable, gint aRow, gint aColumn);
AtkObject* getCaptionCB(AtkTable *aTable);
const gchar* getColumnDescriptionCB(AtkTable *aTable, gint aColumn);
AtkObject* getColumnHeaderCB(AtkTable *aTable, gint aColumn);
const gchar* getRowDescriptionCB(AtkTable *aTable, gint aRow);
AtkObject* getRowHeaderCB(AtkTable *aTable, gint aRow);
AtkObject* getSummaryCB(AtkTable *aTable);
gint getSelectedColumnsCB(AtkTable *aTable, gint **aSelected);
gint getSelectedRowsCB(AtkTable *aTable, gint **aSelected);
gboolean isColumnSelectedCB(AtkTable *aTable, gint aColumn);
gboolean isRowSelectedCB(AtkTable *aTable, gint aRow);
gboolean isCellSelectedCB(AtkTable *aTable, gint aRow, gint aColumn);

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

#endif /* __MAI_INTERFACE_TABLE_H__ */
