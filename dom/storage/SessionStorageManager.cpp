/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageManager.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "SessionStorage.h"
#include "SessionStorageCache.h"
#include "SessionStorageObserver.h"
#include "SessionStorageService.h"
#include "StorageUtils.h"

namespace mozilla {
namespace dom {

using namespace StorageUtils;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStorageManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSessionStorageManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(SessionStorageManager, mBrowsingContext)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SessionStorageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SessionStorageManager)

SessionStorageManager::SessionStorageManager(
    RefPtr<BrowsingContext> aBrowsingContext)
    : mBrowsingContext(std::move(aBrowsingContext)) {
  if (const auto service = SessionStorageService::Get()) {
    service->RegisterSessionStorageManager(this);
  }

  StorageObserver* observer = StorageObserver::Self();
  NS_ASSERTION(
      observer,
      "No StorageObserver, cannot observe private data delete notifications!");

  if (observer) {
    observer->AddSink(this);
  }

  if (!XRE_IsParentProcess() && NextGenLocalStorageEnabled()) {
    // When LSNG is enabled the thread IPC bridge doesn't exist, so we have to
    // create own protocol to distribute chrome observer notifications to
    // content processes.
    mObserver = SessionStorageObserver::Get();

    if (!mObserver) {
      ContentChild* contentActor = ContentChild::GetSingleton();
      MOZ_ASSERT(contentActor);

      RefPtr<SessionStorageObserver> observer = new SessionStorageObserver();

      SessionStorageObserverChild* actor =
          new SessionStorageObserverChild(observer);

      MOZ_ALWAYS_TRUE(
          contentActor->SendPSessionStorageObserverConstructor(actor));

      observer->SetActor(actor);

      mObserver = std::move(observer);
    }
  }
}

SessionStorageManager::~SessionStorageManager() {
  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->RemoveSink(this);
  }

  if (const auto service = SessionStorageService::Get()) {
    service->UnregisterSessionStorageManager(this);
  }
}

NS_IMETHODIMP
SessionStorageManager::PrecacheStorage(nsIPrincipal* aPrincipal,
                                       nsIPrincipal* aStoragePrincipal,
                                       Storage** aRetval) {
  // Nothing to preload.
  return NS_OK;
}

NS_IMETHODIMP
SessionStorageManager::GetSessionStorageCache(
    nsIPrincipal* aPrincipal, nsIPrincipal* aStoragePrincipal,
    RefPtr<SessionStorageCache>* aRetVal) {
  return GetSessionStorageCacheHelper(aStoragePrincipal, true, nullptr,
                                      aRetVal);
}

nsresult SessionStorageManager::GetSessionStorageCacheHelper(
    nsIPrincipal* aPrincipal, bool aMakeIfNeeded,
    SessionStorageCache* aCloneFrom, RefPtr<SessionStorageCache>* aRetVal) {
  nsAutoCString originKey;
  nsAutoCString originAttributes;
  nsresult rv = aPrincipal->GetStorageOriginKey(originKey);
  aPrincipal->OriginAttributesRef().CreateSuffix(originAttributes);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return GetSessionStorageCacheHelper(originAttributes, originKey,
                                      aMakeIfNeeded, aCloneFrom, aRetVal);
}

nsresult SessionStorageManager::GetSessionStorageCacheHelper(
    const nsACString& aOriginAttrs, const nsACString& aOriginKey,
    bool aMakeIfNeeded, SessionStorageCache* aCloneFrom,
    RefPtr<SessionStorageCache>* aRetVal) {
  if (OriginRecord* const originRecord = GetOriginRecord(
          aOriginAttrs, aOriginKey, aMakeIfNeeded, aCloneFrom)) {
    *aRetVal = originRecord->mCache;
  } else {
    *aRetVal = nullptr;
  }
  return NS_OK;
}

SessionStorageManager::OriginRecord* SessionStorageManager::GetOriginRecord(
    const nsACString& aOriginAttrs, const nsACString& aOriginKey,
    const bool aMakeIfNeeded, SessionStorageCache* const aCloneFrom) {
  OriginKeyHashTable* table;
  if (!mOATable.Get(aOriginAttrs, &table)) {
    if (aMakeIfNeeded) {
      table = new OriginKeyHashTable();
      mOATable.Put(aOriginAttrs, table);
    } else {
      return nullptr;
    }
  }

  OriginRecord* originRecord;
  if (!table->Get(aOriginKey, &originRecord)) {
    if (aMakeIfNeeded) {
      originRecord = new OriginRecord();
      if (aCloneFrom) {
        originRecord->mCache = aCloneFrom->Clone();
      } else {
        originRecord->mCache = new SessionStorageCache();
      }
      table->Put(aOriginKey, originRecord);
    } else {
      return nullptr;
    }
  }

  return originRecord;
}

NS_IMETHODIMP
SessionStorageManager::CreateStorage(mozIDOMWindow* aWindow,
                                     nsIPrincipal* aPrincipal,
                                     nsIPrincipal* aStoragePrincipal,
                                     const nsAString& aDocumentURI,
                                     bool aPrivate, Storage** aRetval) {
  RefPtr<SessionStorageCache> cache;
  nsresult rv = GetSessionStorageCache(aPrincipal, aStoragePrincipal, &cache);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

  RefPtr<SessionStorage> storage =
      new SessionStorage(inner, aPrincipal, aStoragePrincipal, cache, this,
                         aDocumentURI, aPrivate);

  storage.forget(aRetval);
  return NS_OK;
}

NS_IMETHODIMP
SessionStorageManager::GetStorage(mozIDOMWindow* aWindow,
                                  nsIPrincipal* aPrincipal,
                                  nsIPrincipal* aStoragePrincipal,
                                  bool aPrivate, Storage** aRetval) {
  *aRetval = nullptr;

  RefPtr<SessionStorageCache> cache;
  nsresult rv =
      GetSessionStorageCacheHelper(aStoragePrincipal, false, nullptr, &cache);
  if (NS_FAILED(rv) || !cache) {
    return rv;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

  RefPtr<SessionStorage> storage =
      new SessionStorage(inner, aPrincipal, aStoragePrincipal, cache, this,
                         EmptyString(), aPrivate);

  storage.forget(aRetval);
  return NS_OK;
}

NS_IMETHODIMP
SessionStorageManager::CloneStorage(Storage* aStorage) {
  if (NS_WARN_IF(!aStorage)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aStorage->Type() != Storage::eSessionStorage) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<SessionStorageCache> cache;
  return GetSessionStorageCacheHelper(
      aStorage->StoragePrincipal(), true,
      static_cast<SessionStorage*>(aStorage)->Cache(), &cache);
}

NS_IMETHODIMP
SessionStorageManager::CheckStorage(nsIPrincipal* aPrincipal, Storage* aStorage,
                                    bool* aRetval) {
  if (NS_WARN_IF(!aStorage)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!aPrincipal) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aRetval = false;

  RefPtr<SessionStorageCache> cache;
  nsresult rv =
      GetSessionStorageCacheHelper(aPrincipal, false, nullptr, &cache);
  if (NS_FAILED(rv) || !cache) {
    return rv;
  }

  if (aStorage->Type() != Storage::eSessionStorage) {
    return NS_OK;
  }

  RefPtr<SessionStorage> sessionStorage =
      static_cast<SessionStorage*>(aStorage);
  if (sessionStorage->Cache() != cache) {
    return NS_OK;
  }

  if (!StorageUtils::PrincipalsEqual(aStorage->StoragePrincipal(),
                                     aPrincipal)) {
    return NS_OK;
  }

  *aRetval = true;
  return NS_OK;
}

void SessionStorageManager::ClearStorages(
    ClearStorageType aType, const OriginAttributesPattern& aPattern,
    const nsACString& aOriginScope) {
  for (auto iter1 = mOATable.Iter(); !iter1.Done(); iter1.Next()) {
    OriginAttributes oa;
    DebugOnly<bool> ok = oa.PopulateFromSuffix(iter1.Key());
    MOZ_ASSERT(ok);
    if (!aPattern.Matches(oa)) {
      // This table doesn't match the given origin attributes pattern
      continue;
    }

    OriginKeyHashTable* table = iter1.UserData();
    for (auto iter2 = table->Iter(); !iter2.Done(); iter2.Next()) {
      if (aOriginScope.IsEmpty() ||
          StringBeginsWith(iter2.Key(), aOriginScope)) {
        const auto cache = iter2.Data()->mCache;
        if (aType == eAll) {
          cache->Clear(SessionStorageCache::eDefaultSetType, false);
          cache->Clear(SessionStorageCache::eSessionSetType, false);
        } else {
          MOZ_ASSERT(aType == eSessionOnly);
          cache->Clear(SessionStorageCache::eSessionSetType, false);
        }
      }
    }
  }
}

void SessionStorageManager::SendSessionStorageDataToParentProcess() {
  if (!mBrowsingContext || mBrowsingContext->IsDiscarded()) {
    return;
  }

  for (auto oaIter = mOATable.Iter(); !oaIter.Done(); oaIter.Next()) {
    for (auto originIter = oaIter.Data()->Iter(); !originIter.Done();
         originIter.Next()) {
      SendSessionStorageCache(ContentChild::GetSingleton(), oaIter.Key(),
                              originIter.Key(), originIter.Data()->mCache);
    }
  }
}

void SessionStorageManager::SendSessionStorageDataToContentProcess(
    ContentParent* const aActor, nsIPrincipal* const aPrincipal) {
  nsAutoCString originAttrs;
  nsAutoCString originKey;
  nsresult rv = aPrincipal->GetStorageOriginKey(originKey);
  aPrincipal->OriginAttributesRef().CreateSuffix(originAttrs);
  if (NS_FAILED(rv)) {
    return;
  }

  const auto originRecord =
      GetOriginRecord(originAttrs, originKey, false, nullptr);
  if (!originRecord) {
    return;
  }

  const auto id = aActor->ChildID();
  if (!originRecord->mKnownTo.Contains(id)) {
    originRecord->mKnownTo.PutEntry(id);
    SendSessionStorageCache(aActor, originAttrs, originKey,
                            originRecord->mCache);
  }
}

template <typename Actor>
void SessionStorageManager::SendSessionStorageCache(
    Actor* const aActor, const nsACString& aOriginAttrs,
    const nsACString& aOriginKey, SessionStorageCache* const aCache) {
  nsTArray<KeyValuePair> defaultData =
      aCache->SerializeData(SessionStorageCache::eDefaultSetType);
  nsTArray<KeyValuePair> sessionData =
      aCache->SerializeData(SessionStorageCache::eSessionSetType);
  Unused << aActor->SendSessionStorageData(
      mBrowsingContext->Id(), nsCString{aOriginAttrs}, nsCString{aOriginKey},
      defaultData, sessionData);
}

void SessionStorageManager::LoadSessionStorageData(
    ContentParent* const aSource, const nsACString& aOriginAttrs,
    const nsACString& aOriginKey, const nsTArray<KeyValuePair>& aDefaultData,
    const nsTArray<KeyValuePair>& aSessionData) {
  const auto originRecord =
      GetOriginRecord(aOriginAttrs, aOriginKey, true, nullptr);
  MOZ_ASSERT(originRecord);

  if (aSource) {
    originRecord->mKnownTo.RemoveEntry(aSource->ChildID());
  }

  originRecord->mCache->DeserializeData(SessionStorageCache::eDefaultSetType,
                                        aDefaultData);
  originRecord->mCache->DeserializeData(SessionStorageCache::eSessionSetType,
                                        aSessionData);
}

nsresult SessionStorageManager::Observe(
    const char* aTopic, const nsAString& aOriginAttributesPattern,
    const nsACString& aOriginScope) {
  OriginAttributesPattern pattern;
  if (!pattern.Init(aOriginAttributesPattern)) {
    NS_ERROR("Cannot parse origin attributes pattern");
    return NS_ERROR_FAILURE;
  }

  // Clear everything, caches + database
  if (!strcmp(aTopic, "cookie-cleared")) {
    ClearStorages(eAll, pattern, EmptyCString());
    return NS_OK;
  }

  // Clear from caches everything that has been stored
  // while in session-only mode
  if (!strcmp(aTopic, "session-only-cleared")) {
    ClearStorages(eSessionOnly, pattern, aOriginScope);
    return NS_OK;
  }

  // Clear everything (including so and pb data) from caches and database
  // for the given domain and subdomains.
  if (!strcmp(aTopic, "browser:purge-sessionStorage")) {
    ClearStorages(eAll, pattern, aOriginScope);
    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-change")) {
    // For case caches are still referenced - clear them completely
    ClearStorages(eAll, pattern, EmptyCString());
    mOATable.Clear();
    return NS_OK;
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
