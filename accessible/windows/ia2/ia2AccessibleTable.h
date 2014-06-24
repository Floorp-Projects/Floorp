/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_TABLE_H
#define _ACCESSIBLE_TABLE_H

#include "AccessibleTable.h"
#include "AccessibleTable2.h"

namespace mozilla {
namespace a11y {

class TableAccessible;

class ia2AccessibleTable : public IAccessibleTable,
                           public IAccessibleTable2
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


  // IAccessibleTable2

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_cellAt(
      /* [in] */ long row,
      /* [in] */ long column,
      /* [out, retval] */ IUnknown **cell);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nSelectedCells(
      /* [out, retval] */ long *cellCount);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedCells(
      /* [out, size_is(,*nSelectedCells,)] */ IUnknown ***cells,
      /* [out, retval] */ long *nSelectedCells);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedColumns(
      /* [out, size_is(,*nColumns)] */ long **selectedColumns,
      /* [out, retval] */ long *nColumns);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedRows(
      /* [out, size_is(,*nRows)] */ long **selectedRows, 
      /* [out, retval] */ long *nRows);

protected:
  ia2AccessibleTable(TableAccessible* aTable) : mTable(aTable) {}

  TableAccessible* mTable;
};

} // namespace a11y
} // namespace mozilla

#endif
