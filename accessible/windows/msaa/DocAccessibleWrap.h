/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleWrap_h__
#define mozilla_a11y_DocAccessibleWrap_h__

#include "DocAccessible.h"

namespace mozilla {
namespace a11y {

class DocAccessibleWrap : public DocAccessible
{
public:
  DocAccessibleWrap(nsIDocument* aDocument, nsIPresShell* aPresShell);
  virtual ~DocAccessibleWrap();

  DECL_IUNKNOWN_INHERITED

  // IAccessible

    // Override get_accValue to provide URL when no other value is available
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accValue( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszValue);

  // Accessible
  virtual void Shutdown();

  // DocAccessible
  virtual void* GetNativeWindow() const;

  /**
   * Manage the mapping from id to Accessible.
   */
#ifdef _WIN64
  void AddID(uint32_t aID, AccessibleWrap* aAcc)
    { mIDToAccessibleMap.Put(aID, aAcc); }
  void RemoveID(uint32_t aID) { mIDToAccessibleMap.Remove(aID); }
  AccessibleWrap* GetAccessibleByID(uint32_t aID) const
    { return mIDToAccessibleMap.Get(aID); }
#endif

protected:
  // DocAccessible
  virtual void DoInitialUpdate();

protected:
  void* mHWND;

  /*
   * This provides a mapping from 32 bit id to accessible objects.
   */
#ifdef _WIN64
  nsDataHashtable<nsUint32HashKey, AccessibleWrap*> mIDToAccessibleMap;
#endif
};

} // namespace a11y
} // namespace mozilla

#endif
