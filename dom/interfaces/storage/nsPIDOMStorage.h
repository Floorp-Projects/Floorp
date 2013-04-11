/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPIDOMStorage_h_
#define __nsPIDOMStorage_h_

#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class DOMStorageCache;
class DOMStorageManager;

} // ::dom
} // ::mozilla

// {09198A51-5D27-4992-97E4-38A9CEA2A65D}
#define NS_PIDOMSTORAGE_IID \
  { 0x9198a51, 0x5d27, 0x4992, \
    { 0x97, 0xe4, 0x38, 0xa9, 0xce, 0xa2, 0xa6, 0x5d } }

class nsPIDOMStorage : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMSTORAGE_IID)

  enum StorageType {
    LocalStorage = 1,
    SessionStorage = 2
  };

  virtual StorageType GetType() const = 0;
  virtual mozilla::dom::DOMStorageManager* GetManager() const = 0;
  virtual const mozilla::dom::DOMStorageCache* GetCache() const = 0;

  virtual nsTArray<nsString>* GetKeys() = 0;

  virtual nsIPrincipal* GetPrincipal() = 0;
  virtual bool PrincipalEquals(nsIPrincipal* principal) = 0;
  virtual bool CanAccess(nsIPrincipal *aPrincipal) = 0;
  virtual bool IsPrivate() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIDOMStorage, NS_PIDOMSTORAGE_IID)

#endif // __nsPIDOMStorage_h_
