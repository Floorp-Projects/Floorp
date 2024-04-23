/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_UiaGridItem_h__
#define mozilla_a11y_UiaGridItem_h__

#include "objbase.h"
#include "uiautomation.h"

namespace mozilla::a11y {
class TableCellAccessible;

/**
 * IGridItemProvider and ITableItemProvider implementations.
 */
class UiaGridItem : public IGridItemProvider, public ITableItemProvider {
 public:
  // IGridItemProvider
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Row(
      /* [retval][out] */ __RPC__out int* aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_Column(
      /* [retval][out] */ __RPC__out int* aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_RowSpan(
      /* [retval][out] */ __RPC__out int* aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ColumnSpan(
      /* [retval][out] */ __RPC__out int* aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ContainingGrid(
      /* [retval][out] */ __RPC__deref_out_opt IRawElementProviderSimple**
          aRetVal);

  // ITableItemProvider
  virtual HRESULT STDMETHODCALLTYPE GetRowHeaderItems(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetColumnHeaderItems(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

 private:
  TableCellAccessible* CellAcc();
};

}  // namespace mozilla::a11y

#endif
