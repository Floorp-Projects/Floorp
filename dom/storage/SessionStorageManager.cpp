/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageManager.h"

#include "StorageIPC.h"
#include "SessionStorage.h"
#include "SessionStorageCache.h"
#include "SessionStorageObserver.h"
#include "StorageUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/PBackgroundSessionStorageCache.h"
#include "mozilla/dom/PBackgroundSessionStorageManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsTHashMap.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

using namespace StorageUtils;

// Parent process, background thread hashmap that stores top context id and
// manager pair.
static StaticAutoPtr<
    nsRefPtrHashtable<nsUint64HashKey, BackgroundSessionStorageManager>>
    sManagers;

bool RecvShutdownBackgroundSessionStorageManagers() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  sManagers = nullptr;
  return true;
}

void RecvPropagateBackgroundSessionStorageManager(
    uint64_t aCurrentTopContextId, uint64_t aTargetTopContextId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (sManagers) {
    if (RefPtr<BackgroundSessionStorageManager> mgr =
            sManagers->Get(aCurrentTopContextId)) {
      // Because of bfcache, we may re-register aTargetTopContextId in
      // CanonicalBrowsingContext::ReplacedBy.
      // XXXBFCache do we want to tweak this behavior and ensure this is
      // called only once?
      sManagers->InsertOrUpdate(aTargetTopContextId, std::move(mgr));
    }
  }
}

bool RecvRemoveBackgroundSessionStorageManager(uint64_t aTopContextId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (sManagers) {
    sManagers->Remove(aTopContextId);
  }

  return true;
}

SessionStorageManagerBase::OriginRecord*
SessionStorageManagerBase::GetOriginRecord(
    const nsACString& aOriginAttrs, const nsACString& aOriginKey,
    const bool aMakeIfNeeded, SessionStorageCache* const aCloneFrom) {
  // XXX It seems aMakeIfNeeded is always known at compile-time, so this could
  // be split into two functions.

  if (aMakeIfNeeded) {
    return mOATable.GetOrInsertNew(aOriginAttrs)
        ->LookupOrInsertWith(
            aOriginKey,
            [&] {
              auto newOriginRecord = MakeUnique<OriginRecord>();
              if (aCloneFrom) {
                newOriginRecord->mCache = aCloneFrom->Clone();
              } else {
                newOriginRecord->mCache = new SessionStorageCache();
              }
              return newOriginRecord;
            })
        .get();
  }

  auto* const table = mOATable.Get(aOriginAttrs);
  if (!table) return nullptr;

  return table->Get(aOriginKey);
}

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
    : mBrowsingContext(std::move(aBrowsingContext)), mActor(nullptr) {
  AssertIsOnMainThread();

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

  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }
}

bool SessionStorageManager::CanLoadData() {
  AssertIsOnMainThread();

  return mBrowsingContext && !mBrowsingContext->IsDiscarded();
}

void SessionStorageManager::SetActor(SessionStorageManagerChild* aActor) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

bool SessionStorageManager::ActorExists() const {
  AssertIsOnMainThread();

  return mActor;
}

void SessionStorageManager::ClearActor() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mActor);

  mActor = nullptr;
}

nsresult SessionStorageManager::EnsureManager() {
  AssertIsOnMainThread();
  MOZ_ASSERT(CanLoadData());

  if (ActorExists()) {
    return NS_OK;
  }

  ::mozilla::ipc::PBackgroundChild* backgroundActor =
      ::mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SessionStorageManagerChild> actor =
      new SessionStorageManagerChild(this);
  MOZ_ASSERT(actor);

  MOZ_ALWAYS_TRUE(
      backgroundActor->SendPBackgroundSessionStorageManagerConstructor(
          actor, mBrowsingContext->Top()->Id()));

  SetActor(actor);

  return NS_OK;
}

SessionStorageCacheChild* SessionStorageManager::EnsureCache(
    const nsCString& aOriginAttrs, const nsCString& aOriginKey,
    SessionStorageCache& aCache) {
  AssertIsOnMainThread();
  MOZ_ASSERT(CanLoadData());
  MOZ_ASSERT(ActorExists());

  if (aCache.Actor()) {
    return aCache.Actor();
  }

  RefPtr<SessionStorageCacheChild> actor =
      new SessionStorageCacheChild(&aCache);
  MOZ_ALWAYS_TRUE(mActor->SendPBackgroundSessionStorageCacheConstructor(
      actor, aOriginAttrs, aOriginKey));

  aCache.SetActor(actor);

  return actor;
}

nsresult SessionStorageManager::LoadData(nsIPrincipal& aPrincipal,
                                         SessionStorageCache& aCache) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mActor);

  nsAutoCString originKey;
  nsresult rv = aPrincipal.GetStorageOriginKey(originKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString originAttributes;
  aPrincipal.OriginAttributesRef().CreateSuffix(originAttributes);

  auto* const originRecord =
      GetOriginRecord(originAttributes, originKey, true, nullptr);
  MOZ_ASSERT(originRecord);

  if (originRecord->mLoaded) {
    return NS_OK;
  }

  RefPtr<SessionStorageCacheChild> cacheActor =
      EnsureCache(originAttributes, originKey, aCache);

  nsTArray<SSSetItemInfo> defaultData;
  nsTArray<SSSetItemInfo> sessionData;
  if (!cacheActor->SendLoad(&defaultData, &sessionData)) {
    return NS_ERROR_FAILURE;
  }

  originRecord->mCache->DeserializeData(SessionStorageCache::eDefaultSetType,
                                        defaultData);
  originRecord->mCache->DeserializeData(SessionStorageCache::eSessionSetType,
                                        sessionData);

  originRecord->mLoaded.Flip();
  aCache.SetLoadedOrCloned();

  return NS_OK;
}

void SessionStorageManager::CheckpointData(nsIPrincipal& aPrincipal,
                                           SessionStorageCache& aCache) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mActor);

  nsAutoCString originKey;
  nsresult rv = aPrincipal.GetStorageOriginKey(originKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsAutoCString originAttributes;
  aPrincipal.OriginAttributesRef().CreateSuffix(originAttributes);

  return CheckpointDataInternal(originAttributes, originKey, aCache);
}

void SessionStorageManager::CheckpointDataInternal(
    const nsCString& aOriginAttrs, const nsCString& aOriginKey,
    SessionStorageCache& aCache) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mActor);

  nsTArray<SSWriteInfo> defaultWriteInfos =
      aCache.SerializeWriteInfos(SessionStorageCache::eDefaultSetType);
  nsTArray<SSWriteInfo> sessionWriteInfos =
      aCache.SerializeWriteInfos(SessionStorageCache::eSessionSetType);

  if (defaultWriteInfos.IsEmpty() && sessionWriteInfos.IsEmpty()) {
    return;
  }

  RefPtr<SessionStorageCacheChild> cacheActor =
      EnsureCache(aOriginAttrs, aOriginKey, aCache);

  Unused << cacheActor->SendCheckpoint(defaultWriteInfos, sessionWriteInfos);

  aCache.ResetWriteInfos(SessionStorageCache::eDefaultSetType);
  aCache.ResetWriteInfos(SessionStorageCache::eSessionSetType);
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

  RefPtr<SessionStorage> storage = new SessionStorage(
      inner, aPrincipal, aStoragePrincipal, cache, this, u""_ns, aPrivate);

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

  // ToDo: At the momnet, we clone the cache on the child process and then
  // send the checkpoint.  It would be nicer if we either serailizing all the
  // data and sync to the parent process directly or clonig storage on the
  // parnet process and sync it to the child process on demand.

  RefPtr<SessionStorageCache> cache;
  nsresult rv = GetSessionStorageCacheHelper(
      aStorage->StoragePrincipal(), true,
      static_cast<SessionStorage*>(aStorage)->Cache(), &cache);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If cache was cloned from other storage, then we shouldn't load the cache
  // at the first access.
  cache->SetLoadedOrCloned();

  if (CanLoadData()) {
    rv = EnsureManager();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    CheckpointData(*aStorage->StoragePrincipal(), *cache);
  }

  return rv;
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
  if (CanLoadData()) {
    nsresult rv = EnsureManager();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

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

        if (CanLoadData()) {
          MOZ_ASSERT(ActorExists());
          CheckpointDataInternal(nsCString{iter1.Key()}, nsCString{iter2.Key()},
                                 *cache);
        }
      }
    }
  }
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
    ClearStorages(eAll, pattern, ""_ns);
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
    ClearStorages(eAll, pattern, ""_ns);
    mOATable.Clear();
    return NS_OK;
  }

  return NS_OK;
}

SessionStorageManager::OriginRecord::~OriginRecord() = default;

// static
void BackgroundSessionStorageManager::RemoveManager(uint64_t aTopContextId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  AssertIsOnMainThread();

  ::mozilla::ipc::PBackgroundChild* backgroundActor =
      ::mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return;
  }

  if (NS_WARN_IF(!backgroundActor->SendRemoveBackgroundSessionStorageManager(
          aTopContextId))) {
    return;
  }
}

// static
void BackgroundSessionStorageManager::PropagateManager(
    uint64_t aCurrentTopContextId, uint64_t aTargetTopContextId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  AssertIsOnMainThread();
  MOZ_ASSERT(aCurrentTopContextId != aTargetTopContextId);

  ::mozilla::ipc::PBackgroundChild* backgroundActor =
      ::mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return;
  }

  if (NS_WARN_IF(!backgroundActor->SendPropagateBackgroundSessionStorageManager(
          aCurrentTopContextId, aTargetTopContextId))) {
    return;
  }
}

// static
BackgroundSessionStorageManager* BackgroundSessionStorageManager::GetOrCreate(
    uint64_t aTopContextId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (!sManagers) {
    sManagers = new nsRefPtrHashtable<nsUint64HashKey,
                                      BackgroundSessionStorageManager>();
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "dom::BackgroundSessionStorageManager::GetOrCreate", [] {
          RunOnShutdown(
              [] {
                ::mozilla::ipc::PBackgroundChild* backgroundActor = ::mozilla::
                    ipc::BackgroundChild::GetOrCreateForCurrentThread();
                if (NS_WARN_IF(!backgroundActor)) {
                  return;
                }

                if (NS_WARN_IF(
                        !backgroundActor
                             ->SendShutdownBackgroundSessionStorageManagers())) {
                  return;
                }
              },
              ShutdownPhase::XPCOMShutdown);
        }));
  }

  return sManagers
      ->LookupOrInsertWith(aTopContextId,
                           [] { return new BackgroundSessionStorageManager(); })
      .get();
}

BackgroundSessionStorageManager::BackgroundSessionStorageManager() {
  MOZ_ASSERT(XRE_IsParentProcess());
  ::mozilla::ipc::AssertIsOnBackgroundThread();
}

BackgroundSessionStorageManager::~BackgroundSessionStorageManager() = default;

void BackgroundSessionStorageManager::CopyDataToContentProcess(
    const nsACString& aOriginAttrs, const nsACString& aOriginKey,
    nsTArray<SSSetItemInfo>& aDefaultData,
    nsTArray<SSSetItemInfo>& aSessionData) {
  MOZ_ASSERT(XRE_IsParentProcess());
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  auto* const originRecord =
      GetOriginRecord(aOriginAttrs, aOriginKey, false, nullptr);
  if (!originRecord) {
    return;
  }

  aDefaultData =
      originRecord->mCache->SerializeData(SessionStorageCache::eDefaultSetType);
  aSessionData =
      originRecord->mCache->SerializeData(SessionStorageCache::eSessionSetType);
}

void BackgroundSessionStorageManager::UpdateData(
    const nsACString& aOriginAttrs, const nsACString& aOriginKey,
    const nsTArray<SSWriteInfo>& aDefaultWriteInfos,
    const nsTArray<SSWriteInfo>& aSessionWriteInfos) {
  MOZ_ASSERT(XRE_IsParentProcess());
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  auto* const originRecord =
      GetOriginRecord(aOriginAttrs, aOriginKey, true, nullptr);
  MOZ_ASSERT(originRecord);

  originRecord->mCache->DeserializeWriteInfos(
      SessionStorageCache::eDefaultSetType, aDefaultWriteInfos);
  originRecord->mCache->DeserializeWriteInfos(
      SessionStorageCache::eSessionSetType, aSessionWriteInfos);
}

}  // namespace dom
}  // namespace mozilla
