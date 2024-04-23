/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_UiaGrid_h__
#define mozilla_a11y_UiaGrid_h__

#include "objbase.h"
#include "uiautomation.h"

namespace mozilla::a11y {
class Accessible;
class TableAccessible;

/**
 * IGridProvider and ITableProvider implementations.
 */
class UiaGrid : public IGridProvider, public ITableProvider {
 public:
  // IGridProvider
  virtual HRESULT STDMETHODCALLTYPE GetItem(
      /* [in] */ int aRow,
      /* [in] */ int aColumn,
      /* [retval][out] */
      __RPC__deref_out_opt IRawElementProviderSimple** aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_RowCount(
      /* [retval][out] */ __RPC__out int* aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ColumnCount(
      /* [retval][out] */ __RPC__out int* aRetVal);

  // ITableProvider
  virtual HRESULT STDMETHODCALLTYPE GetRowHeaders(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetColumnHeaders(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_RowOrColumnMajor(
      /* [retval][out] */ __RPC__out enum RowOrColumnMajor* aRetVal);

 private:
  Accessible* Acc();
  TableAccessible* TableAcc();
};

}  // namespace mozilla::a11y

#endif
