/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleWrap_h__
#define mozilla_a11y_DocAccessibleWrap_h__

#include "DocAccessible.h"

namespace mozilla {

class PresShell;

namespace a11y {

class DocAccessibleWrap : public DocAccessible {
 public:
  DocAccessibleWrap(dom::Document* aDocument, PresShell* aPresShell);
  virtual ~DocAccessibleWrap();

  DECL_IUNKNOWN_INHERITED

  // IAccessible

  // Override get_accParent for e10s
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accParent(
      /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispParent)
      override;

  // Override get_accValue to provide URL when no other value is available
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszValue) override;

  // LocalAccessible
  virtual void Shutdown();

  // DocAccessible
  virtual void* GetNativeWindow() const;

  /**
   * Manage the mapping from id to Accessible.
   */
  void AddID(uint32_t aID, AccessibleWrap* aAcc) {
    mIDToAccessibleMap.InsertOrUpdate(aID, aAcc);
  }
  void RemoveID(uint32_t aID) { mIDToAccessibleMap.Remove(aID); }
  AccessibleWrap* GetAccessibleByID(uint32_t aID) const {
    return mIDToAccessibleMap.Get(aID);
  }

 protected:
  // DocAccessible
  virtual void DoInitialUpdate();

 protected:
  void* mHWND;

  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsTHashMap<nsUint32HashKey, AccessibleWrap*> mIDToAccessibleMap;
};

}  // namespace a11y
}  // namespace mozilla

#endif
