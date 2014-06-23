/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStorage_h___
#define nsDOMStorage_h___

#include "mozilla/Attributes.h"
#include "nsIDOMStorage.h"
#include "nsPIDOMStorage.h"
#include "nsWeakReference.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

class DOMStorageManager;
class DOMStorageCache;

class DOMStorage MOZ_FINAL : public nsIDOMStorage
                           , public nsPIDOMStorage
                           , public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGE

  // nsPIDOMStorage
  virtual StorageType GetType() const MOZ_OVERRIDE;
  virtual DOMStorageManager* GetManager() const MOZ_OVERRIDE { return mManager; }
  virtual const DOMStorageCache* GetCache() const MOZ_OVERRIDE { return mCache; }

  virtual nsTArray<nsString>* GetKeys() MOZ_OVERRIDE;
  virtual nsIPrincipal* GetPrincipal() MOZ_OVERRIDE;
  virtual bool PrincipalEquals(nsIPrincipal* aPrincipal) MOZ_OVERRIDE;
  virtual bool CanAccess(nsIPrincipal* aPrincipal) MOZ_OVERRIDE;
  virtual bool IsPrivate() MOZ_OVERRIDE { return mIsPrivate; }

  DOMStorage(DOMStorageManager* aManager,
             DOMStorageCache* aCache,
             const nsAString& aDocumentURI,
             nsIPrincipal* aPrincipal,
             bool aIsPrivate);

  // The method checks whether the caller can use a storage.
  // CanUseStorage is called before any DOM initiated operation
  // on a storage is about to happen and ensures that the storage's
  // session-only flag is properly set according the current settings.
  // It is an optimization since the privileges check and session only
  // state determination are complex and share the code (comes hand in
  // hand together).
  static bool CanUseStorage(DOMStorage* aStorage = nullptr);

  bool IsPrivate() const { return mIsPrivate; }
  bool IsSessionOnly() const { return mIsSessionOnly; }

private:
  ~DOMStorage();

  friend class DOMStorageManager;
  friend class DOMStorageCache;

  nsRefPtr<DOMStorageManager> mManager;
  nsRefPtr<DOMStorageCache> mCache;
  nsString mDocumentURI;

  // Principal this DOMStorage (i.e. localStorage or sessionStorage) has
  // been created for
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // Whether this storage is running in private-browsing window.
  bool mIsPrivate : 1;

  // Whether storage is set to persist data only per session, may change
  // dynamically and is set by CanUseStorage function that is called
  // before any operation on the storage.
  bool mIsSessionOnly : 1;

  void BroadcastChangeNotification(const nsSubstring& aKey,
                                   const nsSubstring& aOldValue,
                                   const nsSubstring& aNewValue);
};

} // ::dom
} // ::mozilla

#endif /* nsDOMStorage_h___ */
