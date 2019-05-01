/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_TABLECELL_H
#define _ACCESSIBLE_TABLECELL_H

#include "AccessibleTableCell.h"

namespace mozilla {
namespace a11y {
class TableCellAccessible;

class ia2AccessibleTableCell : public IAccessibleTableCell {
 public:
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleTableCell

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_table(
      /* [out, retval] */ IUnknown** table);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnExtent(
      /* [out, retval] */ long* nColumnsSpanned);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnHeaderCells(
      /* [out, size_is(,*nColumnHeaderCells,)] */ IUnknown*** cellAccessibles,
      /* [out, retval] */ long* nColumnHeaderCells);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnIndex(
      /* [out, retval] */ long* columnIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowExtent(
      /* [out, retval] */ long* nRowsSpanned);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowHeaderCells(
      /* [out, size_is(,*nRowHeaderCells,)] */ IUnknown*** cellAccessibles,
      /* [out, retval] */ long* nRowHeaderCells);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowIndex(
      /* [out, retval] */ long* rowIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowColumnExtents(
      /* [out] */ long* row,
      /* [out] */ long* column,
      /* [out] */ long* rowExtents,
      /* [out] */ long* columnExtents,
      /* [out, retval] */ boolean* isSelected);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isSelected(
      /* [out, retval] */ boolean* isSelected);

 protected:
  ia2AccessibleTableCell(TableCellAccessible* aTableCell)
      : mTableCell(aTableCell) {}

  TableCellAccessible* mTableCell;
};

}  // namespace a11y
}  // namespace mozilla

#endif
