/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMStorageManager.h"
#include "DOMStorage.h"
#include "DOMStorageDBThread.h"

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

namespace {

int32_t gQuotaLimit = DEFAULT_QUOTA_LIMIT;

} // namespace

DOMLocalStorageManager*
DOMLocalStorageManager::sSelf = nullptr;

// static
uint32_t
DOMStorageManager::GetQuota()
{
  static bool preferencesInitialized = false;
  if (!preferencesInitialized) {
    mozilla::Preferences::AddIntVarCache(&gQuotaLimit, "dom.storage.default_quota",
                                         DEFAULT_QUOTA_LIMIT);
    preferencesInitialized = true;
  }

  return gQuotaLimit * 1024; // pref is in kBs
}

void
ReverseString(const nsCSubstring& aSource, nsCSubstring& aResult)
{
  nsACString::const_iterator sourceBegin, sourceEnd;
  aSource.BeginReading(sourceBegin);
  aSource.EndReading(sourceEnd);

  aResult.SetLength(aSource.Length());
  nsACString::iterator destEnd;
  aResult.EndWriting(destEnd);

  while (sourceBegin != sourceEnd) {
    *(--destEnd) = *sourceBegin;
    ++sourceBegin;
  }
}

nsresult
CreateReversedDomain(const nsACString& aAsciiDomain,
                     nsACString& aKey)
{
  if (aAsciiDomain.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ReverseString(aAsciiDomain, aKey);

  aKey.Append('.');
  return NS_OK;
}

bool
PrincipalsEqual(nsIPrincipal* aObjectPrincipal, nsIPrincipal* aSubjectPrincipal)
{
  if (!aSubjectPrincipal) {
    return true;
  }

  if (!aObjectPrincipal) {
    return false;
  }

  return aSubjectPrincipal->Equals(aObjectPrincipal);
}

NS_IMPL_ISUPPORTS(DOMStorageManager,
                  nsIDOMStorageManager)

DOMStorageManager::DOMStorageManager(DOMStorage::StorageType aType)
  : mCaches(8)
  , mType(aType)
  , mLowDiskSpace(false)
{
  DOMStorageObserver* observer = DOMStorageObserver::Self();
  NS_ASSERTION(observer, "No DOMStorageObserver, cannot observe private data delete notifications!");

  if (observer) {
    observer->AddSink(this);
  }
}

DOMStorageManager::~DOMStorageManager()
{
  DOMStorageObserver* observer = DOMStorageObserver::Self();
  if (observer) {
    observer->RemoveSink(this);
  }
}

namespace {

nsresult
AppendOriginNoSuffix(nsIPrincipal* aPrincipal,
                nsACString& aKey)
{
  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!uri) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString domainOrigin;
  rv = uri->GetAsciiHost(domainOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  if (domainOrigin.IsEmpty()) {
    // For the file:/// protocol use the exact directory as domain.
    bool isScheme = false;
    if (NS_SUCCEEDED(uri->SchemeIs("file", &isScheme)) && isScheme) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = url->GetDirectory(domainOrigin);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Append reversed domain
  nsAutoCString reverseDomain;
  rv = CreateReversedDomain(domainOrigin, reverseDomain);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aKey.Append(reverseDomain);

  // Append scheme
  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  aKey.Append(':');
  aKey.Append(scheme);

  // Append port if any
  int32_t port = NS_GetRealPort(uri);
  if (port != -1) {
    aKey.Append(nsPrintfCString(":%d", port));
  }

  return NS_OK;
}

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
DOMStorageManager::CreateOrigin(const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix)
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

DOMStorageCache*
DOMStorageManager::GetCache(const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix)
{
  CacheOriginHashtable* table = mCaches.LookupOrAdd(aOriginSuffix);
  DOMStorageCacheHashKey* entry = table->GetEntry(aOriginNoSuffix);
  if (!entry) {
    return nullptr;
  }

  return entry->cache();
}

already_AddRefed<DOMStorageUsage>
DOMStorageManager::GetOriginUsage(const nsACString& aOriginNoSuffix)
{
  RefPtr<DOMStorageUsage> usage;
  if (mUsages.Get(aOriginNoSuffix, &usage)) {
    return usage.forget();
  }

  usage = new DOMStorageUsage(aOriginNoSuffix);

  if (mType == LocalStorage) {
    DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
    if (db) {
      db->AsyncGetUsage(usage);
    }
  }

  mUsages.Put(aOriginNoSuffix, usage);

  return usage.forget();
}

already_AddRefed<DOMStorageCache>
DOMStorageManager::PutCache(const nsACString& aOriginSuffix,
                            const nsACString& aOriginNoSuffix,
                            nsIPrincipal* aPrincipal)
{
  CacheOriginHashtable* table = mCaches.LookupOrAdd(aOriginSuffix);
  DOMStorageCacheHashKey* entry = table->PutEntry(aOriginNoSuffix);
  RefPtr<DOMStorageCache> cache = entry->cache();

  nsAutoCString quotaOrigin;
  CreateQuotaDBKey(aPrincipal, quotaOrigin);

  switch (mType) {
  case SessionStorage:
    // Lifetime handled by the manager, don't persist
    entry->HardRef();
    cache->Init(this, false, aPrincipal, quotaOrigin);
    break;

  case LocalStorage:
    // Lifetime handled by the cache, do persist
    cache->Init(this, true, aPrincipal, quotaOrigin);
    break;

  default:
    MOZ_ASSERT(false);
  }

  return cache.forget();
}

void
DOMStorageManager::DropCache(DOMStorageCache* aCache)
{
  if (!NS_IsMainThread()) {
    NS_WARNING("DOMStorageManager::DropCache called on a non-main thread, shutting down?");
  }

  CacheOriginHashtable* table = mCaches.LookupOrAdd(aCache->OriginSuffix());
  table->RemoveEntry(aCache->OriginNoSuffix());
}

nsresult
DOMStorageManager::GetStorageInternal(bool aCreate,
                                      mozIDOMWindow* aWindow,
                                      nsIPrincipal* aPrincipal,
                                      const nsAString& aDocumentURI,
                                      nsIDOMStorage** aRetval)
{
  nsresult rv;

  nsAutoCString originAttrSuffix;
  aPrincipal->OriginAttributesRef().CreateSuffix(originAttrSuffix);

  nsAutoCString originKey;
  rv = AppendOriginNoSuffix(aPrincipal, originKey);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<DOMStorageCache> cache = GetCache(originAttrSuffix, originKey);

  // Get or create a cache for the given scope
  if (!cache) {
    if (!aCreate) {
      *aRetval = nullptr;
      return NS_OK;
    }

    if (!aRetval) {
      // This is a demand to just preload the cache, if the scope has
      // no data stored, bypass creation and preload of the cache.
      DOMStorageDBBridge* db = DOMStorageCache::GetDatabase();
      if (db) {
        if (!db->ShouldPreloadOrigin(DOMStorageManager::CreateOrigin(originAttrSuffix, originKey))) {
          return NS_OK;
        }
      } else {
        if (originKey.EqualsLiteral("knalb.:about")) {
          return NS_OK;
        }
      }
    }

    // There is always a single instance of a cache per scope
    // in a single instance of a DOM storage manager.
    cache = PutCache(originAttrSuffix, originKey, aPrincipal);
  } else if (mType == SessionStorage) {
    if (!cache->CheckPrincipal(aPrincipal)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  if (aRetval) {
    nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

    nsCOMPtr<nsIDOMStorage> storage = new DOMStorage(
      inner, this, cache, aDocumentURI, aPrincipal);
    storage.forget(aRetval);
  }

  return NS_OK;
}

NS_IMETHODIMP
DOMStorageManager::PrecacheStorage(nsIPrincipal* aPrincipal)
{
  return GetStorageInternal(true, nullptr, aPrincipal, EmptyString(), nullptr);
}

NS_IMETHODIMP
DOMStorageManager::CreateStorage(mozIDOMWindow* aWindow,
                                 nsIPrincipal* aPrincipal,
                                 const nsAString& aDocumentURI,
                                 nsIDOMStorage** aRetval)
{
  return GetStorageInternal(true, aWindow, aPrincipal, aDocumentURI, aRetval);
}

NS_IMETHODIMP
DOMStorageManager::GetStorage(mozIDOMWindow* aWindow,
                              nsIPrincipal* aPrincipal,
                              nsIDOMStorage** aRetval)
{
  return GetStorageInternal(false, aWindow, aPrincipal, EmptyString(), aRetval);
}

NS_IMETHODIMP
DOMStorageManager::CloneStorage(nsIDOMStorage* aStorage)
{
  if (mType != SessionStorage) {
    // Cloning is supported only for sessionStorage
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<DOMStorage> storage = static_cast<DOMStorage*>(aStorage);
  if (!storage) {
    return NS_ERROR_UNEXPECTED;
  }

  const DOMStorageCache* origCache = storage->GetCache();

  DOMStorageCache* existingCache = GetCache(origCache->OriginSuffix(),
                                            origCache->OriginNoSuffix());
  if (existingCache) {
    // Do not replace an existing sessionStorage.
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Since this manager is sessionStorage manager, PutCache hard references
  // the cache in our hashtable.
  RefPtr<DOMStorageCache> newCache = PutCache(origCache->OriginSuffix(),
                                              origCache->OriginNoSuffix(),
                                              origCache->Principal());

  newCache->CloneFrom(origCache);
  return NS_OK;
}

NS_IMETHODIMP
DOMStorageManager::CheckStorage(nsIPrincipal* aPrincipal,
                                nsIDOMStorage* aStorage,
                                bool* aRetval)
{
  nsresult rv;

  RefPtr<DOMStorage> storage = static_cast<DOMStorage*>(aStorage);
  if (!storage) {
    return NS_ERROR_UNEXPECTED;
  }

  *aRetval = false;

  if (!aPrincipal) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString suffix;
  aPrincipal->OriginAttributesRef().CreateSuffix(suffix);

  nsAutoCString origin;
  rv = AppendOriginNoSuffix(aPrincipal, origin);
  if (NS_FAILED(rv)) {
    return rv;
  }

  DOMStorageCache* cache = GetCache(suffix, origin);
  if (cache != storage->GetCache()) {
    return NS_OK;
  }

  if (!storage->PrincipalEquals(aPrincipal)) {
    return NS_OK;
  }

  *aRetval = true;
  return NS_OK;
}

// Obsolete nsIDOMStorageManager methods

NS_IMETHODIMP
DOMStorageManager::GetLocalStorageForPrincipal(nsIPrincipal* aPrincipal,
                                               const nsAString& aDocumentURI,
                                               nsIDOMStorage** aRetval)
{
  if (mType != LocalStorage) {
    return NS_ERROR_UNEXPECTED;
  }

  return CreateStorage(nullptr, aPrincipal, aDocumentURI, aRetval);
}

void
DOMStorageManager::ClearCaches(uint32_t aUnloadFlags,
                               const OriginAttributesPattern& aPattern,
                               const nsACString& aOriginScope)
{
  for (auto iter1 = mCaches.Iter(); !iter1.Done(); iter1.Next()) {
    PrincipalOriginAttributes oa;
    DebugOnly<bool> rv = oa.PopulateFromSuffix(iter1.Key());
    MOZ_ASSERT(rv);
    if (!aPattern.Matches(oa)) {
      // This table doesn't match the given origin attributes pattern
      continue;
    }

    CacheOriginHashtable* table = iter1.Data();

    for (auto iter2 = table->Iter(); !iter2.Done(); iter2.Next()) {
      DOMStorageCache* cache = iter2.Get()->cache();

      if (aOriginScope.IsEmpty() ||
          StringBeginsWith(cache->OriginNoSuffix(), aOriginScope)) {
        cache->UnloadItems(aUnloadFlags);
      }
    }
  }
}

nsresult
DOMStorageManager::Observe(const char* aTopic,
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
    ClearCaches(DOMStorageCache::kUnloadComplete, pattern, EmptyCString());
    return NS_OK;
  }

  // Clear from caches everything that has been stored
  // while in session-only mode
  if (!strcmp(aTopic, "session-only-cleared")) {
    ClearCaches(DOMStorageCache::kUnloadSession, pattern, aOriginScope);
    return NS_OK;
  }

  // Clear everything (including so and pb data) from caches and database
  // for the gived domain and subdomains.
  if (!strcmp(aTopic, "domain-data-cleared")) {
    ClearCaches(DOMStorageCache::kUnloadComplete, pattern, aOriginScope);
    return NS_OK;
  }

  // Clear all private-browsing caches
  if (!strcmp(aTopic, "private-browsing-data-cleared")) {
    ClearCaches(DOMStorageCache::kUnloadPrivate, pattern, EmptyCString());
    return NS_OK;
  }

  // Clear localStorage data beloging to an origin pattern
  if (!strcmp(aTopic, "origin-attr-pattern-cleared")) {
    // sessionStorage is expected to stay
    if (mType == SessionStorage) {
      return NS_OK;
    }

    ClearCaches(DOMStorageCache::kUnloadComplete, pattern, EmptyCString());
    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-change")) {
    // For case caches are still referenced - clear them completely
    ClearCaches(DOMStorageCache::kUnloadComplete, pattern, EmptyCString());
    mCaches.Clear();
    return NS_OK;
  }

  if (!strcmp(aTopic, "low-disk-space")) {
    if (mType == LocalStorage) {
      mLowDiskSpace = true;
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, "no-low-disk-space")) {
    if (mType == LocalStorage) {
      mLowDiskSpace = false;
    }

    return NS_OK;
  }

#ifdef DOM_STORAGE_TESTS
  if (!strcmp(aTopic, "test-reload")) {
    if (mType != LocalStorage) {
      return NS_OK;
    }

    // This immediately completely reloads all caches from the database.
    ClearCaches(DOMStorageCache::kTestReload, pattern, EmptyCString());
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

// DOMLocalStorageManager

DOMLocalStorageManager::DOMLocalStorageManager()
  : DOMStorageManager(LocalStorage)
{
  NS_ASSERTION(!sSelf, "Somebody is trying to do_CreateInstance(\"@mozilla/dom/localStorage-manager;1\"");
  sSelf = this;

  if (!XRE_IsParentProcess()) {
    // Do this only on the child process.  The thread IPC bridge
    // is also used to communicate chrome observer notifications.
    // Note: must be called after we set sSelf
    DOMStorageCache::StartDatabase();
  }
}

DOMLocalStorageManager::~DOMLocalStorageManager()
{
  sSelf = nullptr;
}

DOMLocalStorageManager*
DOMLocalStorageManager::Ensure()
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

// DOMSessionStorageManager

DOMSessionStorageManager::DOMSessionStorageManager()
  : DOMStorageManager(SessionStorage)
{
  if (!XRE_IsParentProcess()) {
    // Do this only on the child process.  The thread IPC bridge
    // is also used to communicate chrome observer notifications.
    DOMStorageCache::StartDatabase();
  }
}

} // namespace dom
} // namespace mozilla
