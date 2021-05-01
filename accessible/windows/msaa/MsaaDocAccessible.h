/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaDocAccessible_h__
#define mozilla_a11y_MsaaDocAccessible_h__

#include "DocAccessible.h"
#include "MsaaAccessible.h"

namespace mozilla {

class PresShell;

namespace a11y {
class DocAccessible;

// XXX This should inherit from MsaaAccessible. Inheriting from DocAccessible
// is a necessary hack until we remove the inheritance of DocAccessibleWrap.
class MsaaDocAccessible : public DocAccessible {
 public:
  MsaaDocAccessible(dom::Document* aDocument, PresShell* aPresShell)
      : DocAccessible(aDocument, aPresShell) {}

  // IAccessible

  // Override get_accParent for e10s
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accParent(
      /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispParent)
      override;

  // Override get_accValue to provide URL when no other value is available
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accValue(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszValue) override;

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
  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsTHashMap<nsUint32HashKey, AccessibleWrap*> mIDToAccessibleMap;

 private:
  DocAccessible* DocAcc();
};

}  // namespace a11y
}  // namespace mozilla

#endif
