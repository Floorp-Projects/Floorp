/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageManager_h
#define mozilla_dom_StorageManager_h

#include "nsIDOMStorageManager.h"
#include "nsILocalStorageManager.h"
#include "StorageObserver.h"

#include "LocalStorage.h"
#include "LocalStorageCache.h"
#include "mozilla/dom/Storage.h"

#include "nsTHashtable.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsTHashMap.h"

namespace mozilla {

class OriginAttributesPattern;

namespace dom {

class LocalStorageManager final : public nsIDOMStorageManager,
                                  public nsILocalStorageManager,
                                  public StorageObserverSink {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEMANAGER
  NS_DECL_NSILOCALSTORAGEMANAGER

 public:
  LocalStorageManager();

  // Reads the preference for DOM storage quota
  static uint32_t GetOriginQuota();

  // Reads the preference for DOM storage site quota
  static uint32_t GetSiteQuota();

  // Gets (but not ensures) cache for the given scope
  LocalStorageCache* GetCache(const nsACString& aOriginSuffix,
                              const nsACString& aOriginNoSuffix);

  // Returns object keeping usage cache for the scope.
  already_AddRefed<StorageUsage> GetOriginUsage(
      const nsACString& aOriginNoSuffix, uint32_t aPrivateBrowsingId);

  static nsAutoCString CreateOrigin(const nsACString& aOriginSuffix,
                                    const nsACString& aOriginNoSuffix);

 private:
  ~LocalStorageManager();

  // StorageObserverSink, handler to various chrome clearing notification
  nsresult Observe(const char* aTopic,
                   const nsAString& aOriginAttributesPattern,
                   const nsACString& aOriginScope) override;

  // Since nsTHashtable doesn't like multiple inheritance, we have to aggregate
  // LocalStorageCache into the entry.
  class LocalStorageCacheHashKey : public nsCStringHashKey {
   public:
    explicit LocalStorageCacheHashKey(const nsACString* aKey)
        : nsCStringHashKey(aKey), mCache(new LocalStorageCache(aKey)) {}

    LocalStorageCacheHashKey(LocalStorageCacheHashKey&& aOther)
        : nsCStringHashKey(std::move(aOther)),
          mCache(std::move(aOther.mCache)),
          mCacheRef(std::move(aOther.mCacheRef)) {
      NS_ERROR("Shouldn't be called");
    }

    LocalStorageCache* cache() { return mCache; }
    // Keep the cache referenced forever, used for sessionStorage.
    void HardRef() { mCacheRef = mCache; }

   private:
    // weak ref only since cache references its manager.
    LocalStorageCache* mCache;
    // hard ref when this is sessionStorage to keep it alive forever.
    RefPtr<LocalStorageCache> mCacheRef;
  };

  // Ensures cache for a scope, when it doesn't exist it is created and
  // initalized, this also starts preload of persistent data.
  already_AddRefed<LocalStorageCache> PutCache(
      const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix,
      const nsACString& aQuotaKey, nsIPrincipal* aPrincipal);

  enum class CreateMode {
    // GetStorage: do not create if it's not already in memory.
    UseIfExistsNeverCreate,
    // CreateStorage: Create it if it's not already in memory.
    CreateAlways,
    // PrecacheStorage: Create only if the database says we ShouldPreloadOrigin.
    CreateIfShouldPreload
  };

  // Helper for creation of DOM storage objects
  nsresult GetStorageInternal(CreateMode aCreate, mozIDOMWindow* aWindow,
                              nsIPrincipal* aPrincipal,
                              nsIPrincipal* aStoragePrincipal,
                              const nsAString& aDocumentURI, bool aPrivate,
                              Storage** aRetval);

  // Suffix->origin->cache map
  using CacheOriginHashtable = nsTHashtable<LocalStorageCacheHashKey>;
  nsClassHashtable<nsCStringHashKey, CacheOriginHashtable> mCaches;

  void ClearCaches(uint32_t aUnloadFlags,
                   const OriginAttributesPattern& aPattern,
                   const nsACString& aKeyPrefix);

  // Global getter of localStorage manager service
  static LocalStorageManager* Self();

  // Like Self, but creates an instance if we're not yet initialized.
  static LocalStorageManager* Ensure();

 private:
  // Keeps usage cache objects for eTLD+1 scopes we have touched.
  nsTHashMap<nsCString, RefPtr<StorageUsage> > mUsages;

  friend class LocalStorageCache;
  friend class StorageDBChild;
  // Releases cache since it is no longer referrered by any Storage object.
  virtual void DropCache(LocalStorageCache* aCache);

  static LocalStorageManager* sSelf;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_StorageManager_h
