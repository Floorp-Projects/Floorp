/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageManager.h"
#include "LocalStorage.h"
#include "StorageDBThread.h"
#include "StorageUtils.h"

#include "nsIScriptSecurityManager.h"
#include "nsIEffectiveTLDService.h"

#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"

// Only allow relatively small amounts of data since performance of
// the synchronous IO is very bad.
// We are enforcing simple per-origin quota only.
#define DEFAULT_QUOTA_LIMIT (5 * 1024)

namespace mozilla {
namespace dom {

using namespace StorageUtils;

namespace {

int32_t gQuotaLimit = DEFAULT_QUOTA_LIMIT;

} // namespace

LocalStorageManager* LocalStorageManager::sSelf = nullptr;

// static
uint32_t
LocalStorageManager::GetQuota()
{
  static bool preferencesInitialized = false;
  if (!preferencesInitialized) {
    mozilla::Preferences::AddIntVarCache(&gQuotaLimit,
                                         "dom.storage.default_quota",
                                         DEFAULT_QUOTA_LIMIT);
    preferencesInitialized = true;
  }

  return gQuotaLimit * 1024; // pref is in kBs
}

NS_IMPL_ISUPPORTS(LocalStorageManager,
                  nsIDOMStorageManager)

LocalStorageManager::LocalStorageManager()
  : mCaches(8)
  , mLowDiskSpace(false)
{
  StorageObserver* observer = StorageObserver::Self();
  NS_ASSERTION(observer, "No StorageObserver, cannot observe private data delete notifications!");

  if (observer) {
    observer->AddSink(this);
  }

  NS_ASSERTION(!sSelf, "Somebody is trying to do_CreateInstance(\"@mozilla/dom/localStorage-manager;1\"");
  sSelf = this;

  if (!XRE_IsParentProcess()) {
    // Do this only on the child process.  The thread IPC bridge
    // is also used to communicate chrome observer notifications.
    // Note: must be called after we set sSelf
    StorageDBChild::GetOrCreate();
  }
}

LocalStorageManager::~LocalStorageManager()
{
  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->RemoveSink(this);
  }

  sSelf = nullptr;
}

namespace {

nsresult
CreateQuotaDBKey(nsIPrincipal* aPrincipal,
                 nsACString& aKey)
{
  nsresult rv;

  nsCOMPtr<nsIEffectiveTLDService> eTLDService(do_GetService(
    NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsAutoCString eTLDplusOne;
  rv = eTLDService->GetBaseDomain(uri, 0, eTLDplusOne);
  if (NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS == rv) {
    // XXX bug 357323 - what to do for localhost/file exactly?
    rv = uri->GetAsciiHost(eTLDplusOne);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  aKey.Truncate();
  aPrincipal->OriginAttributesRef().CreateSuffix(aKey);

  nsAutoCString subdomainsDBKey;
  CreateReversedDomain(eTLDplusOne, subdomainsDBKey);

  aKey.Append(':');
  aKey.Append(subdomainsDBKey);

  return NS_OK;
}

} // namespace

// static
nsCString
LocalStorageManager::CreateOrigin(const nsACString& aOriginSuffix,
                                  const nsACString& aOriginNoSuffix)
{
  // Note: some hard-coded sqlite statements are dependent on the format this
  // method returns.  Changing this without updating those sqlite statements
  // will cause malfunction.

  nsAutoCString scope;
  scope.Append(aOriginSuffix);
  scope.Append(':');
  scope.Append(aOriginNoSuffix);
  return scope;
}

LocalStorageCache*
LocalStorageManager::GetCache(const nsACString& aOriginSuffix,
                              const nsACString& aOriginNoSuffix)
{
  CacheOriginHashtable* table = mCaches.LookupOrAdd(aOriginSuffix);
  LocalStorageCacheHashKey* entry = table->GetEntry(aOriginNoSuffix);
  if (!entry) {
    return nullptr;
  }

  return entry->cache();
}

already_AddRefed<StorageUsage>
LocalStorageManager::GetOriginUsage(const nsACString& aOriginNoSuffix)
{
  RefPtr<StorageUsage> usage;
  if (mUsages.Get(aOriginNoSuffix, &usage)) {
    return usage.forget();
  }

  usage = new StorageUsage(aOriginNoSuffix);

  StorageDBChild* storageChild = StorageDBChild::GetOrCreate();
  if (storageChild) {
    storageChild->AsyncGetUsage(usage);
  }

  mUsages.Put(aOriginNoSuffix, usage);

  return usage.forget();
}

already_AddRefed<LocalStorageCache>
LocalStorageManager::PutCache(const nsACString& aOriginSuffix,
                              const nsACString& aOriginNoSuffix,
                              nsIPrincipal* aPrincipal)
{
  CacheOriginHashtable* table = mCaches.LookupOrAdd(aOriginSuffix);
  LocalStorageCacheHashKey* entry = table->PutEntry(aOriginNoSuffix);
  RefPtr<LocalStorageCache> cache = entry->cache();

  nsAutoCString quotaOrigin;
  CreateQuotaDBKey(aPrincipal, quotaOrigin);

  // Lifetime handled by the cache, do persist
  cache->Init(this, true, aPrincipal, quotaOrigin);
  return cache.forget();
}

void
LocalStorageManager::DropCache(LocalStorageCache* aCache)
{
  if (!NS_IsMainThread()) {
    NS_WARNING("StorageManager::DropCache called on a non-main thread, shutting down?");
  }

  CacheOriginHashtable* table = mCaches.LookupOrAdd(aCache->OriginSuffix());
  table->RemoveEntry(aCache->OriginNoSuffix());
}

nsresult
LocalStorageManager::GetStorageInternal(CreateMode aCreateMode,
                                        mozIDOMWindow* aWindow,
                                        nsIPrincipal* aPrincipal,
                                        const nsAString& aDocumentURI,
                                        bool aPrivate,
                                        nsIDOMStorage** aRetval)
{
  nsAutoCString originAttrSuffix;
  nsAutoCString originKey;

  nsresult rv = GenerateOriginKey(aPrincipal, originAttrSuffix, originKey);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<LocalStorageCache> cache = GetCache(originAttrSuffix, originKey);

  // Get or create a cache for the given scope
  if (!cache) {
    if (aCreateMode == CreateMode::UseIfExistsNeverCreate) {
      *aRetval = nullptr;
      return NS_OK;
    }

    if (aCreateMode == CreateMode::CreateIfShouldPreload) {
      // This is a demand to just preload the cache, if the scope has
      // no data stored, bypass creation and preload of the cache.
      StorageDBChild* db = StorageDBChild::Get();
      if (db) {
        if (!db->ShouldPreloadOrigin(LocalStorageManager::CreateOrigin(originAttrSuffix, originKey))) {
          return NS_OK;
        }
      } else {
        if (originKey.EqualsLiteral("knalb.:about")) {
          return NS_OK;
        }
      }
    }

    PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      return NS_ERROR_FAILURE;
    }

    PrincipalInfo principalInfo;
    rv = mozilla::ipc::PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    uint32_t privateBrowsingId;
    rv = aPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // There is always a single instance of a cache per scope
    // in a single instance of a DOM storage manager.
    cache = PutCache(originAttrSuffix, originKey, aPrincipal);

    LocalStorageCacheChild* actor = new LocalStorageCacheChild(cache);

    MOZ_ALWAYS_TRUE(
      backgroundActor->SendPBackgroundLocalStorageCacheConstructor(
                                                            actor,
                                                            principalInfo,
                                                            originKey,
                                                            privateBrowsingId));

    cache->SetActor(actor);
  }

  if (aRetval) {
    nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

    nsCOMPtr<nsIDOMStorage> storage = new LocalStorage(
      inner, this, cache, aDocumentURI, aPrincipal, aPrivate);
    storage.forget(aRetval);
  }

  return NS_OK;
}

NS_IMETHODIMP
LocalStorageManager::PrecacheStorage(nsIPrincipal* aPrincipal,
                                     nsIDOMStorage** aRetval)
{
  return GetStorageInternal(CreateMode::CreateIfShouldPreload, nullptr,
                            aPrincipal, EmptyString(), false, aRetval);
}

NS_IMETHODIMP
LocalStorageManager::CreateStorage(mozIDOMWindow* aWindow,
                                   nsIPrincipal* aPrincipal,
                                   const nsAString& aDocumentURI,
                                   bool aPrivate,
                                   nsIDOMStorage** aRetval)
{
  return GetStorageInternal(CreateMode::CreateAlways, aWindow, aPrincipal,
                            aDocumentURI, aPrivate, aRetval);
}

NS_IMETHODIMP
LocalStorageManager::GetStorage(mozIDOMWindow* aWindow,
                                nsIPrincipal* aPrincipal,
                                bool aPrivate,
                                nsIDOMStorage** aRetval)
{
  return GetStorageInternal(CreateMode::UseIfExistsNeverCreate, aWindow,
                            aPrincipal, EmptyString(), aPrivate, aRetval);
}

NS_IMETHODIMP
LocalStorageManager::CloneStorage(nsIDOMStorage* aStorage)
{
  // Cloning is supported only for sessionStorage
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager::CheckStorage(nsIPrincipal* aPrincipal,
                                  nsIDOMStorage* aStorage,
                                  bool* aRetval)
{
  nsresult rv;

  RefPtr<LocalStorage> storage = static_cast<LocalStorage*>(aStorage);
  if (!storage) {
    return NS_ERROR_UNEXPECTED;
  }

  *aRetval = false;

  if (!aPrincipal) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString suffix;
  nsAutoCString origin;
  rv = GenerateOriginKey(aPrincipal, suffix, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LocalStorageCache* cache = GetCache(suffix, origin);
  if (cache != storage->GetCache()) {
    return NS_OK;
  }

  if (!storage->PrincipalEquals(aPrincipal)) {
    return NS_OK;
  }

  *aRetval = true;
  return NS_OK;
}

void
LocalStorageManager::ClearCaches(uint32_t aUnloadFlags,
                                 const OriginAttributesPattern& aPattern,
                                 const nsACString& aOriginScope)
{
  for (auto iter1 = mCaches.Iter(); !iter1.Done(); iter1.Next()) {
    OriginAttributes oa;
    DebugOnly<bool> rv = oa.PopulateFromSuffix(iter1.Key());
    MOZ_ASSERT(rv);
    if (!aPattern.Matches(oa)) {
      // This table doesn't match the given origin attributes pattern
      continue;
    }

    CacheOriginHashtable* table = iter1.Data();

    for (auto iter2 = table->Iter(); !iter2.Done(); iter2.Next()) {
      LocalStorageCache* cache = iter2.Get()->cache();

      if (aOriginScope.IsEmpty() ||
          StringBeginsWith(cache->OriginNoSuffix(), aOriginScope)) {
        cache->UnloadItems(aUnloadFlags);
      }
    }
  }
}

nsresult
LocalStorageManager::Observe(const char* aTopic,
                             const nsAString& aOriginAttributesPattern,
                             const nsACString& aOriginScope)
{
  OriginAttributesPattern pattern;
  if (!pattern.Init(aOriginAttributesPattern)) {
    NS_ERROR("Cannot parse origin attributes pattern");
    return NS_ERROR_FAILURE;
  }

  // Clear everything, caches + database
  if (!strcmp(aTopic, "cookie-cleared")) {
    ClearCaches(LocalStorageCache::kUnloadComplete, pattern, EmptyCString());
    return NS_OK;
  }

  // Clear everything, caches + database
  if (!strcmp(aTopic, "extension:purge-localStorage-caches")) {
    ClearCaches(LocalStorageCache::kUnloadComplete, pattern, aOriginScope);
    return NS_OK;
  }

  // Clear from caches everything that has been stored
  // while in session-only mode
  if (!strcmp(aTopic, "session-only-cleared")) {
    ClearCaches(LocalStorageCache::kUnloadSession, pattern, aOriginScope);
    return NS_OK;
  }

  // Clear everything (including so and pb data) from caches and database
  // for the gived domain and subdomains.
  if (!strcmp(aTopic, "domain-data-cleared")) {
    ClearCaches(LocalStorageCache::kUnloadComplete, pattern, aOriginScope);
    return NS_OK;
  }

  // Clear all private-browsing caches
  if (!strcmp(aTopic, "private-browsing-data-cleared")) {
    ClearCaches(LocalStorageCache::kUnloadPrivate, pattern, EmptyCString());
    return NS_OK;
  }

  // Clear localStorage data beloging to an origin pattern
  if (!strcmp(aTopic, "origin-attr-pattern-cleared")) {
    ClearCaches(LocalStorageCache::kUnloadComplete, pattern, EmptyCString());
    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-change")) {
    // For case caches are still referenced - clear them completely
    ClearCaches(LocalStorageCache::kUnloadComplete, pattern, EmptyCString());
    mCaches.Clear();
    return NS_OK;
  }

  if (!strcmp(aTopic, "low-disk-space")) {
    mLowDiskSpace = true;
    return NS_OK;
  }

  if (!strcmp(aTopic, "no-low-disk-space")) {
    mLowDiskSpace = false;
    return NS_OK;
  }

#ifdef DOM_STORAGE_TESTS
  if (!strcmp(aTopic, "test-reload")) {
    // This immediately completely reloads all caches from the database.
    ClearCaches(LocalStorageCache::kTestReload, pattern, EmptyCString());
    return NS_OK;
  }

  if (!strcmp(aTopic, "test-flushed")) {
    if (!XRE_IsParentProcess()) {
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      if (obs) {
        obs->NotifyObservers(nullptr, "domstorage-test-flushed", nullptr);
      }
    }

    return NS_OK;
  }
#endif

  NS_ERROR("Unexpected topic");
  return NS_ERROR_UNEXPECTED;
}

LocalStorageManager*
LocalStorageManager::Ensure()
{
  if (sSelf) {
    return sSelf;
  }

  // Cause sSelf to be populated.
  nsCOMPtr<nsIDOMStorageManager> initializer =
    do_GetService("@mozilla.org/dom/localStorage-manager;1");
  MOZ_ASSERT(sSelf, "Didn't initialize?");

  return sSelf;
}

} // namespace dom
} // namespace mozilla
