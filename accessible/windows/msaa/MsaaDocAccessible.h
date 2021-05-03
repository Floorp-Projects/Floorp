/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaDocAccessible_h__
#define mozilla_a11y_MsaaDocAccessible_h__

#include "ia2AccessibleHypertext.h"

namespace mozilla {

namespace a11y {
class Accessible;
class DocAccessible;

class MsaaDocAccessible : public ia2AccessibleHypertext {
 public:
  DocAccessible* DocAcc();

  // IUnknown
  DECL_IUNKNOWN_INHERITED
  IMPL_IUNKNOWN_REFCOUNTING_INHERITED(ia2AccessibleHypertext)

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

  static MsaaDocAccessible* GetFrom(DocAccessible* aDoc);

 protected:
  using ia2AccessibleHypertext::ia2AccessibleHypertext;

  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
  nsTHashMap<nsUint32HashKey, AccessibleWrap*> mIDToAccessibleMap;
};

}  // namespace a11y
}  // namespace mozilla

#endif
