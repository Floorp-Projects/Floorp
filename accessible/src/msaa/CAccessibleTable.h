/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef _ACCESSIBLE_TABLE_H
#define _ACCESSIBLE_TABLE_H

#include "nsISupports.h"

#include "AccessibleTable.h"

class CAccessibleTable: public nsISupports,
                        public IAccessibleTable
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleTable
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_accessibleAt(
      /* [in] */ long row,
      /* [in] */ long column,
      /* [retval][out] */ IUnknown **accessible);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_caption(
      /* [retval][out] */ IUnknown **accessible);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_childIndex(
      /* [in] */ long rowIndex,
      /* [in] */ long columnIndex,
      /* [retval][out] */ long *childIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnDescription(
      /* [in] */ long column,
      /* [retval][out] */ BSTR *description);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnExtentAt( 
      /* [in] */ long row,
      /* [in] */ long column,
      /* [retval][out] */ long *nColumnsSpanned);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnHeader(
      /* [out] */ IAccessibleTable **accessibleTable,
      /* [retval][out] */ long *startingRowIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnIndex(
      /* [in] */ long childIndex,
      /* [retval][out] */ long *columnIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nColumns(
      /* [retval][out] */ long *columnCount);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nRows(
      /* [retval][out] */ long *rowCount);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nSelectedChildren(
      /* [retval][out] */ long *childCount);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nSelectedColumns(
      /* [retval][out] */ long *columnCount);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nSelectedRows(
      /* [retval][out] */ long *rowCount);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowDescription(
      /* [in] */ long row,
      /* [retval][out] */ BSTR *description);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowExtentAt(
      /* [in] */ long row,
      /* [in] */ long column,
      /* [retval][out] */ long *nRowsSpanned);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowHeader(
      /* [out] */ IAccessibleTable **accessibleTable,
      /* [retval][out] */ long *startingColumnIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowIndex(
      /* [in] */ long childIndex,
      /* [retval][out] */ long *rowIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedChildren(
      /* [in] */ long maxChildren,
      /* [length_is][length_is][size_is][size_is][out] */ long **children,
      /* [retval][out] */ long *nChildren);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedColumns(
      /* [in] */ long maxColumns,
      /* [length_is][length_is][size_is][size_is][out] */ long **columns,
      /* [retval][out] */ long *nColumns);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedRows(
      /* [in] */ long maxRows,
      /* [length_is][length_is][size_is][size_is][out] */ long **rows,
      /* [retval][out] */ long *nRows);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_summary(
      /* [retval][out] */ IUnknown **accessible);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isColumnSelected(
      /* [in] */ long column,
      /* [retval][out] */ boolean *isSelected);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isRowSelected(
      /* [in] */ long row,
      /* [retval][out] */ boolean *isSelected);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isSelected(
      /* [in] */ long row,
      /* [in] */ long column,
      /* [retval][out] */ boolean *isSelected);

  virtual HRESULT STDMETHODCALLTYPE selectRow(
      /* [in] */ long row);

  virtual HRESULT STDMETHODCALLTYPE selectColumn(
      /* [in] */ long column);

  virtual HRESULT STDMETHODCALLTYPE unselectRow(
      /* [in] */ long row);

  virtual HRESULT STDMETHODCALLTYPE unselectColumn(
      /* [in] */ long column);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowColumnExtentsAtIndex(
      /* [in] */ long index,
      /* [out] */ long *row,
      /* [out] */ long *column,
      /* [out] */ long *rowExtents,
      /* [out] */ long *columnExtents,
      /* [retval][out] */ boolean *isSelected);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_modelChange(
      /* [retval][out] */ IA2TableModelChange *modelChange);

private:
  enum eItemsType {
    ITEMSTYPE_CELLS,
    ITEMSTYPE_COLUMNS,
    ITEMSTYPE_ROWS
  };

  HRESULT GetSelectedItems(long aMaxItems, long **aItems, long *aItemsCount,
                           eItemsType aType);
};

#endif

