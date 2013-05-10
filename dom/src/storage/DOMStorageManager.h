/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStorageManager_h__
#define nsDOMStorageManager_h__

#include "nsIDOMStorageManager.h"
#include "DOMStorageObserver.h"

#include "nsPIDOMStorage.h"
#include "DOMStorageCache.h"

#include "nsTHashtable.h"

namespace mozilla {
namespace dom {

const nsPIDOMStorage::StorageType SessionStorage = nsPIDOMStorage::SessionStorage;
const nsPIDOMStorage::StorageType LocalStorage = nsPIDOMStorage::LocalStorage;

class DOMStorage;

class DOMStorageManager : public nsIDOMStorageManager
                        , public DOMStorageObserverSink
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEMANAGER

public:
  virtual nsPIDOMStorage::StorageType Type() { return mType; }

  // Reads the preference for DOM storage quota
  static uint32_t GetQuota();
  // Gets (but not ensures) cache for the given scope
  DOMStorageCache* GetCache(const nsACString& aScope) const;

protected:
  DOMStorageManager(nsPIDOMStorage::StorageType aType);
  virtual ~DOMStorageManager();

private:
  // DOMStorageObserverSink, handler to various chrome clearing notification
  virtual nsresult Observe(const char* aTopic, const nsACString& aScopePrefix);

  // Since nsTHashtable doesn't like multiple inheritance, we have to aggregate
  // DOMStorageCache into the entry.
  class DOMStorageCacheHashKey : public nsCStringHashKey
  {
  public:
    DOMStorageCacheHashKey(const nsACString* aKey)
      : nsCStringHashKey(aKey)
      , mCache(new DOMStorageCache(aKey))
    {}

    DOMStorageCacheHashKey(const DOMStorageCacheHashKey& aOther)
      : nsCStringHashKey(aOther)
    {
      NS_ERROR("Shouldn't be called");
    }

    DOMStorageCache* cache() { return mCache; }
    // Keep the cache referenced forever, used for sessionStorage.
    void HardRef() { mCacheRef = mCache; }

  private:
    // weak ref only since cache references its manager.
    DOMStorageCache* mCache;
    // hard ref when this is sessionStorage to keep it alive forever.
    nsRefPtr<DOMStorageCache> mCacheRef;
  };

  // Ensures cache for a scope, when it doesn't exist it is created and initalized,
  // this also starts preload of persistent data.
  already_AddRefed<DOMStorageCache> PutCache(const nsACString& aScope,
                                             nsIPrincipal* aPrincipal);

  // Helper for creation of DOM storage objects
  nsresult GetStorageInternal(bool aCreate,
                              nsIPrincipal* aPrincipal,
                              const nsAString& aDocumentURI,
                              bool aPrivate,
                              nsIDOMStorage** aRetval);

  // Scope->cache map
  nsTHashtable<DOMStorageCacheHashKey> mCaches;
  const nsPIDOMStorage::StorageType mType;

  static PLDHashOperator ClearCacheEnumerator(DOMStorageCacheHashKey* aCache,
                                              void* aClosure);

protected:
  friend class DOMStorageCache;
  // Releases cache since it is no longer referrered by any DOMStorage object.
  virtual void DropCache(DOMStorageCache* aCache);
};

// Derived classes to allow two different contract ids, one for localStorage and
// one for sessionStorage management.  localStorage manager is used as service
// scoped to the application while sessionStorage managers are instantiated by each
// top doc shell in the application since sessionStorages are isolated per top level
// browsing context.  The code may easily by shared by both.

class DOMLocalStorageManager MOZ_FINAL : public DOMStorageManager
{
public:
  DOMLocalStorageManager();
  virtual ~DOMLocalStorageManager();

  // Global getter of localStorage manager service
  static DOMLocalStorageManager* Self() { return sSelf; }

private:
  static DOMLocalStorageManager* sSelf;
};

class DOMSessionStorageManager MOZ_FINAL : public DOMStorageManager
{
public:
  DOMSessionStorageManager();
};

} // ::dom
} // ::mozilla

#endif /* nsDOMStorageManager_h__ */
