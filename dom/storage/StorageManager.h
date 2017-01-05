/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageManager_h
#define mozilla_dom_StorageManager_h

#include "nsIDOMStorageManager.h"
#include "StorageObserver.h"

#include "StorageCache.h"
#include "mozilla/dom/Storage.h"

#include "nsTHashtable.h"
#include "nsDataHashtable.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"

namespace mozilla {

class OriginAttributesPattern;

namespace dom {

const Storage::StorageType SessionStorage = Storage::SessionStorage;
const Storage::StorageType LocalStorage = Storage::LocalStorage;

class StorageManagerBase : public nsIDOMStorageManager
                         , public StorageObserverSink
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEMANAGER

public:
  virtual Storage::StorageType Type() { return mType; }

  // Reads the preference for DOM storage quota
  static uint32_t GetQuota();
  // Gets (but not ensures) cache for the given scope
  StorageCache* GetCache(const nsACString& aOriginSuffix,
                         const nsACString& aOriginNoSuffix);
  // Returns object keeping usage cache for the scope.
  already_AddRefed<StorageUsage> GetOriginUsage(const nsACString& aOriginNoSuffix);

  static nsCString CreateOrigin(const nsACString& aOriginSuffix,
                                const nsACString& aOriginNoSuffix);

protected:
  explicit StorageManagerBase(Storage::StorageType aType);
  virtual ~StorageManagerBase();

private:
  // StorageObserverSink, handler to various chrome clearing notification
  virtual nsresult Observe(const char* aTopic,
                           const nsAString& aOriginAttributesPattern,
                           const nsACString& aOriginScope) override;

  // Since nsTHashtable doesn't like multiple inheritance, we have to aggregate
  // StorageCache into the entry.
  class StorageCacheHashKey : public nsCStringHashKey
  {
  public:
    explicit StorageCacheHashKey(const nsACString* aKey)
      : nsCStringHashKey(aKey)
      , mCache(new StorageCache(aKey))
    {}

    StorageCacheHashKey(const StorageCacheHashKey& aOther)
      : nsCStringHashKey(aOther)
    {
      NS_ERROR("Shouldn't be called");
    }

    StorageCache* cache() { return mCache; }
    // Keep the cache referenced forever, used for sessionStorage.
    void HardRef() { mCacheRef = mCache; }

  private:
    // weak ref only since cache references its manager.
    StorageCache* mCache;
    // hard ref when this is sessionStorage to keep it alive forever.
    RefPtr<StorageCache> mCacheRef;
  };

  // Ensures cache for a scope, when it doesn't exist it is created and
  // initalized, this also starts preload of persistent data.
  already_AddRefed<StorageCache> PutCache(const nsACString& aOriginSuffix,
                                          const nsACString& aOriginNoSuffix,
                                          nsIPrincipal* aPrincipal);

  // Helper for creation of DOM storage objects
  nsresult GetStorageInternal(bool aCreate,
                              mozIDOMWindow* aWindow,
                              nsIPrincipal* aPrincipal,
                              const nsAString& aDocumentURI,
                              nsIDOMStorage** aRetval);

  // Suffix->origin->cache map
  typedef nsTHashtable<StorageCacheHashKey> CacheOriginHashtable;
  nsClassHashtable<nsCStringHashKey, CacheOriginHashtable> mCaches;

  const Storage::StorageType mType;

  // If mLowDiskSpace is true it indicates a low device storage situation and
  // so no localStorage writes are allowed. sessionStorage writes are still
  // allowed.
  bool mLowDiskSpace;
  bool IsLowDiskSpace() const { return mLowDiskSpace; };

  void ClearCaches(uint32_t aUnloadFlags,
                   const OriginAttributesPattern& aPattern,
                   const nsACString& aKeyPrefix);

protected:
  // Keeps usage cache objects for eTLD+1 scopes we have touched.
  nsDataHashtable<nsCStringHashKey, RefPtr<StorageUsage> > mUsages;

  friend class StorageCache;
  // Releases cache since it is no longer referrered by any Storage object.
  virtual void DropCache(StorageCache* aCache);
};

// Derived classes to allow two different contract ids, one for localStorage and
// one for sessionStorage management.  localStorage manager is used as service
// scoped to the application while sessionStorage managers are instantiated by
// each top doc shell in the application since sessionStorages are isolated per
// top level browsing context.  The code may easily by shared by both.

class DOMLocalStorageManager final : public StorageManagerBase
{
public:
  DOMLocalStorageManager();
  virtual ~DOMLocalStorageManager();

  // Global getter of localStorage manager service
  static DOMLocalStorageManager* Self() { return sSelf; }

  // Like Self, but creates an instance if we're not yet initialized.
  static DOMLocalStorageManager* Ensure();

private:
  static DOMLocalStorageManager* sSelf;
};

class DOMSessionStorageManager final : public StorageManagerBase
{
public:
  DOMSessionStorageManager();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StorageManager_h
