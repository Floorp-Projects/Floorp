/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageIPC.h"

#include "LocalStorageManager.h"

#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

namespace {

typedef nsClassHashtable<nsCStringHashKey, nsTArray<LocalStorageCacheParent*>>
    LocalStorageCacheParentHashtable;

StaticAutoPtr<LocalStorageCacheParentHashtable> gLocalStorageCacheParents;

StorageDBChild* sStorageChild = nullptr;

// False until we shut the storage child down.
bool sStorageChildDown = false;

}  // namespace

LocalStorageCacheChild::LocalStorageCacheChild(LocalStorageCache* aCache)
    : mCache(aCache) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCache);
  aCache->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(LocalStorageCacheChild);
}

LocalStorageCacheChild::~LocalStorageCacheChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(LocalStorageCacheChild);
}

void LocalStorageCacheChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mCache) {
    mCache->ClearActor();
    mCache = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundLocalStorageCacheChild::SendDeleteMe());
  }
}

void LocalStorageCacheChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mCache) {
    mCache->ClearActor();
    mCache = nullptr;
  }
}

mozilla::ipc::IPCResult LocalStorageCacheChild::RecvObserve(
    const PrincipalInfo& aPrincipalInfo,
    const PrincipalInfo& aCachePrincipalInfo,
    const uint32_t& aPrivateBrowsingId, const nsString& aDocumentURI,
    const nsString& aKey, const nsString& aOldValue,
    const nsString& aNewValue) {
  AssertIsOnOwningThread();

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(aPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsCOMPtr<nsIPrincipal> cachePrincipal =
      PrincipalInfoToPrincipal(aCachePrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (StorageUtils::PrincipalsEqual(principal, cachePrincipal)) {
    Storage::NotifyChange(/* aStorage */ nullptr, principal, aKey, aOldValue,
                          aNewValue,
                          /* aStorageType */ u"localStorage", aDocumentURI,
                          /* aIsPrivate */ !!aPrivateBrowsingId,
                          /* aImmediateDispatch */ true);
  }

  return IPC_OK();
}

// ----------------------------------------------------------------------------
// Child
// ----------------------------------------------------------------------------

class StorageDBChild::ShutdownObserver final : public nsIObserver {
 public:
  ShutdownObserver() { MOZ_ASSERT(NS_IsMainThread()); }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 private:
  ~ShutdownObserver() { MOZ_ASSERT(NS_IsMainThread()); }
};

void StorageDBChild::AddIPDLReference() {
  MOZ_ASSERT(!mIPCOpen, "Attempting to retain multiple IPDL references");
  mIPCOpen = true;
  AddRef();
}

void StorageDBChild::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen, "Attempting to release non-existent IPDL reference");
  mIPCOpen = false;
  Release();
}

StorageDBChild::StorageDBChild(LocalStorageManager* aManager)
    : mManager(aManager), mStatus(NS_OK), mIPCOpen(false) {
  MOZ_ASSERT(!NextGenLocalStorageEnabled());
}

StorageDBChild::~StorageDBChild() {}

// static
StorageDBChild* StorageDBChild::Get() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!NextGenLocalStorageEnabled());

  return sStorageChild;
}

// static
StorageDBChild* StorageDBChild::GetOrCreate() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!NextGenLocalStorageEnabled());

  if (sStorageChild || sStorageChildDown) {
    // When sStorageChildDown is at true, sStorageChild is null.
    // Checking sStorageChildDown flag here prevents reinitialization of
    // the storage child after shutdown.
    return sStorageChild;
  }

  // Use LocalStorageManager::Ensure in case we're called from
  // DOMSessionStorageManager's initializer and we haven't yet initialized the
  // local storage manager.
  RefPtr<StorageDBChild> storageChild =
      new StorageDBChild(LocalStorageManager::Ensure());

  nsresult rv = storageChild->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  storageChild.forget(&sStorageChild);

  return sStorageChild;
}

nsTHashtable<nsCStringHashKey>& StorageDBChild::OriginsHavingData() {
  if (!mOriginsHavingData) {
    mOriginsHavingData = new nsTHashtable<nsCStringHashKey>;
  }

  return *mOriginsHavingData;
}

nsresult StorageDBChild::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  PBackgroundChild* actor = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actor)) {
    return NS_ERROR_FAILURE;
  }

  nsString profilePath;
  if (XRE_IsParentProcess()) {
    nsresult rv = StorageDBThread::GetProfilePath(profilePath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  AddIPDLReference();

  actor->SendPBackgroundStorageConstructor(this, profilePath);

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  MOZ_ASSERT(observerService);

  nsCOMPtr<nsIObserver> observer = new ShutdownObserver();

  MOZ_ALWAYS_SUCCEEDS(
      observerService->AddObserver(observer, "xpcom-shutdown", false));

  return NS_OK;
}

nsresult StorageDBChild::Shutdown() {
  // There is nothing to do here, IPC will release automatically and
  // the actual thread running on the parent process will also stop
  // automatically in profile-before-change topic observer.
  return NS_OK;
}

void StorageDBChild::AsyncPreload(LocalStorageCacheBridge* aCache,
                                  bool aPriority) {
  if (mIPCOpen) {
    // Adding ref to cache for the time of preload.  This ensures a reference to
    // to the cache and that all keys will load into this cache object.
    mLoadingCaches.PutEntry(aCache);
    SendAsyncPreload(aCache->OriginSuffix(), aCache->OriginNoSuffix(),
                     aPriority);
  } else {
    // No IPC, no love.  But the LoadDone call is expected.
    aCache->LoadDone(NS_ERROR_UNEXPECTED);
  }
}

void StorageDBChild::AsyncGetUsage(StorageUsageBridge* aUsage) {
  if (mIPCOpen) {
    SendAsyncGetUsage(aUsage->OriginScope());
  }
}

void StorageDBChild::SyncPreload(LocalStorageCacheBridge* aCache,
                                 bool aForceSync) {
  if (NS_FAILED(mStatus)) {
    aCache->LoadDone(mStatus);
    return;
  }

  if (!mIPCOpen) {
    aCache->LoadDone(NS_ERROR_UNEXPECTED);
    return;
  }

  // There is no way to put the child process to a wait state to receive all
  // incoming async responses from the parent, hence we have to do a sync
  // preload instead.  We are smart though, we only demand keys that are left to
  // load in case the async preload has already loaded some keys.
  InfallibleTArray<nsString> keys, values;
  nsresult rv;
  SendPreload(aCache->OriginSuffix(), aCache->OriginNoSuffix(),
              aCache->LoadedCount(), &keys, &values, &rv);

  for (uint32_t i = 0; i < keys.Length(); ++i) {
    aCache->LoadItem(keys[i], values[i]);
  }

  aCache->LoadDone(rv);
}

nsresult StorageDBChild::AsyncAddItem(LocalStorageCacheBridge* aCache,
                                      const nsAString& aKey,
                                      const nsAString& aValue) {
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncAddItem(aCache->OriginSuffix(), aCache->OriginNoSuffix(),
                   nsString(aKey), nsString(aValue));
  OriginsHavingData().PutEntry(aCache->Origin());
  return NS_OK;
}

nsresult StorageDBChild::AsyncUpdateItem(LocalStorageCacheBridge* aCache,
                                         const nsAString& aKey,
                                         const nsAString& aValue) {
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncUpdateItem(aCache->OriginSuffix(), aCache->OriginNoSuffix(),
                      nsString(aKey), nsString(aValue));
  OriginsHavingData().PutEntry(aCache->Origin());
  return NS_OK;
}

nsresult StorageDBChild::AsyncRemoveItem(LocalStorageCacheBridge* aCache,
                                         const nsAString& aKey) {
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncRemoveItem(aCache->OriginSuffix(), aCache->OriginNoSuffix(),
                      nsString(aKey));
  return NS_OK;
}

nsresult StorageDBChild::AsyncClear(LocalStorageCacheBridge* aCache) {
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncClear(aCache->OriginSuffix(), aCache->OriginNoSuffix());
  OriginsHavingData().RemoveEntry(aCache->Origin());
  return NS_OK;
}

bool StorageDBChild::ShouldPreloadOrigin(const nsACString& aOrigin) {
  // Return true if we didn't receive the origins list yet.
  // I tend to rather preserve a bit of early-after-start performance
  // than a bit of memory here.
  return !mOriginsHavingData || mOriginsHavingData->Contains(aOrigin);
}

mozilla::ipc::IPCResult StorageDBChild::RecvObserve(
    const nsCString& aTopic, const nsString& aOriginAttributesPattern,
    const nsCString& aOriginScope) {
  MOZ_ASSERT(!XRE_IsParentProcess());

  StorageObserver::Self()->Notify(aTopic.get(), aOriginAttributesPattern,
                                  aOriginScope);
  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBChild::RecvOriginsHavingData(
    nsTArray<nsCString>&& aOrigins) {
  // Force population of mOriginsHavingData even if there are no origins so that
  // ShouldPreloadOrigin does not generate false positives for all origins.
  if (!aOrigins.Length()) {
    Unused << OriginsHavingData();
  }

  for (uint32_t i = 0; i < aOrigins.Length(); ++i) {
    OriginsHavingData().PutEntry(aOrigins[i]);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBChild::RecvLoadItem(
    const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix,
    const nsString& aKey, const nsString& aValue) {
  LocalStorageCache* aCache =
      mManager->GetCache(aOriginSuffix, aOriginNoSuffix);
  if (aCache) {
    aCache->LoadItem(aKey, aValue);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBChild::RecvLoadDone(
    const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix,
    const nsresult& aRv) {
  LocalStorageCache* aCache =
      mManager->GetCache(aOriginSuffix, aOriginNoSuffix);
  if (aCache) {
    aCache->LoadDone(aRv);

    // Just drop reference to this cache now since the load is done.
    mLoadingCaches.RemoveEntry(static_cast<LocalStorageCacheBridge*>(aCache));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBChild::RecvLoadUsage(
    const nsCString& aOriginNoSuffix, const int64_t& aUsage) {
  RefPtr<StorageUsageBridge> scopeUsage =
      mManager->GetOriginUsage(aOriginNoSuffix);
  scopeUsage->LoadUsage(aUsage);
  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBChild::RecvError(const nsresult& aRv) {
  mStatus = aRv;
  return IPC_OK();
}

NS_IMPL_ISUPPORTS(StorageDBChild::ShutdownObserver, nsIObserver)

NS_IMETHODIMP
StorageDBChild::ShutdownObserver::Observe(nsISupports* aSubject,
                                          const char* aTopic,
                                          const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"));

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (NS_WARN_IF(!observerService)) {
    return NS_ERROR_FAILURE;
  }

  Unused << observerService->RemoveObserver(this, "xpcom-shutdown");

  if (sStorageChild) {
    sStorageChildDown = true;

    MOZ_ALWAYS_TRUE(sStorageChild->PBackgroundStorageChild::SendDeleteMe());

    NS_RELEASE(sStorageChild);
    sStorageChild = nullptr;
  }

  return NS_OK;
}

SessionStorageObserverChild::SessionStorageObserverChild(
    SessionStorageObserver* aObserver)
    : mObserver(aObserver) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NextGenLocalStorageEnabled());
  MOZ_ASSERT(aObserver);
  aObserver->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(SessionStorageObserverChild);
}

SessionStorageObserverChild::~SessionStorageObserverChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(SessionStorageObserverChild);
}

void SessionStorageObserverChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mObserver) {
    mObserver->ClearActor();
    mObserver = nullptr;

    // Don't check result here since IPC may no longer be available due to
    // SessionStorageManager (which holds a strong reference to
    // SessionStorageObserver) being destroyed very late in the game.
    PSessionStorageObserverChild::SendDeleteMe();
  }
}

void SessionStorageObserverChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mObserver) {
    mObserver->ClearActor();
    mObserver = nullptr;
  }
}

mozilla::ipc::IPCResult SessionStorageObserverChild::RecvObserve(
    const nsCString& aTopic, const nsString& aOriginAttributesPattern,
    const nsCString& aOriginScope) {
  AssertIsOnOwningThread();

  StorageObserver::Self()->Notify(aTopic.get(), aOriginAttributesPattern,
                                  aOriginScope);
  return IPC_OK();
}

LocalStorageCacheParent::LocalStorageCacheParent(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const nsACString& aOriginKey, uint32_t aPrivateBrowsingId)
    : mPrincipalInfo(aPrincipalInfo),
      mOriginKey(aOriginKey),
      mPrivateBrowsingId(aPrivateBrowsingId),
      mActorDestroyed(false) {
  AssertIsOnBackgroundThread();
}

LocalStorageCacheParent::~LocalStorageCacheParent() {
  MOZ_ASSERT(mActorDestroyed);
}

void LocalStorageCacheParent::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  MOZ_ASSERT(gLocalStorageCacheParents);

  nsTArray<LocalStorageCacheParent*>* array;
  gLocalStorageCacheParents->Get(mOriginKey, &array);
  MOZ_ASSERT(array);

  array->RemoveElement(this);

  if (array->IsEmpty()) {
    gLocalStorageCacheParents->Remove(mOriginKey);
  }

  if (!gLocalStorageCacheParents->Count()) {
    gLocalStorageCacheParents = nullptr;
  }
}

mozilla::ipc::IPCResult LocalStorageCacheParent::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLocalStorageCacheParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult LocalStorageCacheParent::RecvNotify(
    const nsString& aDocumentURI, const nsString& aKey,
    const nsString& aOldValue, const nsString& aNewValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(gLocalStorageCacheParents);

  nsTArray<LocalStorageCacheParent*>* array;
  gLocalStorageCacheParents->Get(mOriginKey, &array);
  MOZ_ASSERT(array);

  for (LocalStorageCacheParent* localStorageCacheParent : *array) {
    if (localStorageCacheParent != this) {
      // When bug 1443925 is fixed, we can compare mPrincipalInfo against
      // localStorageCacheParent->PrincipalInfo() here on the background thread
      // instead of posting it to the main thread.  The advantage of doing so is
      // that it would save an IPC message in the case where the principals do
      // not match.
      Unused << localStorageCacheParent->SendObserve(
          mPrincipalInfo, localStorageCacheParent->PrincipalInfo(),
          mPrivateBrowsingId, aDocumentURI, aKey, aOldValue, aNewValue);
    }
  }

  return IPC_OK();
}

// ----------------------------------------------------------------------------
// Parent
// ----------------------------------------------------------------------------

class StorageDBParent::ObserverSink : public StorageObserverSink {
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  // Only touched on the PBackground thread.
  StorageDBParent* MOZ_NON_OWNING_REF mActor;

 public:
  explicit ObserverSink(StorageDBParent* aActor)
      : mOwningEventTarget(GetCurrentThreadEventTarget()), mActor(aActor) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aActor);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(StorageDBParent::ObserverSink);

  void Start();

  void Stop();

 private:
  ~ObserverSink() = default;

  void AddSink();

  void RemoveSink();

  void Notify(const nsCString& aTopic, const nsString& aOriginAttributesPattern,
              const nsCString& aOriginScope);

  // StorageObserverSink
  nsresult Observe(const char* aTopic, const nsAString& aOriginAttrPattern,
                   const nsACString& aOriginScope) override;
};

NS_IMPL_ADDREF(StorageDBParent)
NS_IMPL_RELEASE(StorageDBParent)

void StorageDBParent::AddIPDLReference() {
  MOZ_ASSERT(!mIPCOpen, "Attempting to retain multiple IPDL references");
  mIPCOpen = true;
  AddRef();
}

void StorageDBParent::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen, "Attempting to release non-existent IPDL reference");
  mIPCOpen = false;
  Release();
}

namespace {}  // namespace

StorageDBParent::StorageDBParent(const nsString& aProfilePath)
    : mProfilePath(aProfilePath), mIPCOpen(false) {
  AssertIsOnBackgroundThread();

  // We are always open by IPC only
  AddIPDLReference();
}

StorageDBParent::~StorageDBParent() {
  AssertIsOnBackgroundThread();

  if (mObserverSink) {
    mObserverSink->Stop();
    mObserverSink = nullptr;
  }
}

void StorageDBParent::Init() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    mObserverSink = new ObserverSink(this);
    mObserverSink->Start();
  }

  StorageDBThread* storageThread = StorageDBThread::Get();
  if (storageThread) {
    InfallibleTArray<nsCString> scopes;
    storageThread->GetOriginsHavingData(&scopes);
    mozilla::Unused << SendOriginsHavingData(scopes);
  }
}

StorageDBParent::CacheParentBridge* StorageDBParent::NewCache(
    const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix) {
  return new CacheParentBridge(this, aOriginSuffix, aOriginNoSuffix);
}

void StorageDBParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Implement me! Bug 1005169
}

mozilla::ipc::IPCResult StorageDBParent::RecvDeleteMe() {
  AssertIsOnBackgroundThread();

  IProtocol* mgr = Manager();
  if (!PBackgroundStorageParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvAsyncPreload(
    const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix,
    const bool& aPriority) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  storageThread->AsyncPreload(NewCache(aOriginSuffix, aOriginNoSuffix),
                              aPriority);

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvAsyncGetUsage(
    const nsCString& aOriginNoSuffix) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  // The object releases it self in LoadUsage method
  RefPtr<UsageParentBridge> usage =
      new UsageParentBridge(this, aOriginNoSuffix);

  storageThread->AsyncGetUsage(usage);

  return IPC_OK();
}

namespace {

// We need another implementation of LocalStorageCacheBridge to do
// synchronous IPC preload.  This class just receives Load* notifications
// and fills the returning arguments of RecvPreload with the database
// values for us.
class SyncLoadCacheHelper : public LocalStorageCacheBridge {
 public:
  SyncLoadCacheHelper(const nsCString& aOriginSuffix,
                      const nsCString& aOriginNoSuffix,
                      uint32_t aAlreadyLoadedCount,
                      InfallibleTArray<nsString>* aKeys,
                      InfallibleTArray<nsString>* aValues, nsresult* rv)
      : mMonitor("DOM Storage SyncLoad IPC"),
        mSuffix(aOriginSuffix),
        mOrigin(aOriginNoSuffix),
        mKeys(aKeys),
        mValues(aValues),
        mRv(rv),
        mLoaded(false),
        mLoadedCount(aAlreadyLoadedCount) {
    // Precaution
    *mRv = NS_ERROR_UNEXPECTED;
  }

  virtual const nsCString Origin() const override {
    return LocalStorageManager::CreateOrigin(mSuffix, mOrigin);
  }
  virtual const nsCString& OriginNoSuffix() const override { return mOrigin; }
  virtual const nsCString& OriginSuffix() const override { return mSuffix; }
  virtual bool Loaded() override { return mLoaded; }
  virtual uint32_t LoadedCount() override { return mLoadedCount; }
  virtual bool LoadItem(const nsAString& aKey,
                        const nsString& aValue) override {
    // Called on the aCache background thread
    MOZ_ASSERT(!mLoaded);
    if (mLoaded) {
      return false;
    }

    ++mLoadedCount;
    mKeys->AppendElement(aKey);
    mValues->AppendElement(aValue);
    return true;
  }

  virtual void LoadDone(nsresult aRv) override {
    // Called on the aCache background thread
    MonitorAutoLock monitor(mMonitor);
    MOZ_ASSERT(!mLoaded && mRv);
    mLoaded = true;
    if (mRv) {
      *mRv = aRv;
      mRv = nullptr;
    }
    monitor.Notify();
  }

  virtual void LoadWait() override {
    // Called on the main thread, exits after LoadDone() call
    MonitorAutoLock monitor(mMonitor);
    while (!mLoaded) {
      monitor.Wait();
    }
  }

 private:
  Monitor mMonitor;
  nsCString mSuffix, mOrigin;
  InfallibleTArray<nsString>* mKeys;
  InfallibleTArray<nsString>* mValues;
  nsresult* mRv;
  bool mLoaded;
  uint32_t mLoadedCount;
};

}  // namespace

mozilla::ipc::IPCResult StorageDBParent::RecvPreload(
    const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix,
    const uint32_t& aAlreadyLoadedCount, InfallibleTArray<nsString>* aKeys,
    InfallibleTArray<nsString>* aValues, nsresult* aRv) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<SyncLoadCacheHelper> cache(
      new SyncLoadCacheHelper(aOriginSuffix, aOriginNoSuffix,
                              aAlreadyLoadedCount, aKeys, aValues, aRv));

  storageThread->SyncPreload(cache, true);

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvAsyncAddItem(
    const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix,
    const nsString& aKey, const nsString& aValue) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = storageThread->AsyncAddItem(
      NewCache(aOriginSuffix, aOriginNoSuffix), aKey, aValue);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvAsyncUpdateItem(
    const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix,
    const nsString& aKey, const nsString& aValue) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = storageThread->AsyncUpdateItem(
      NewCache(aOriginSuffix, aOriginNoSuffix), aKey, aValue);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvAsyncRemoveItem(
    const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix,
    const nsString& aKey) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = storageThread->AsyncRemoveItem(
      NewCache(aOriginSuffix, aOriginNoSuffix), aKey);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvAsyncClear(
    const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv =
      storageThread->AsyncClear(NewCache(aOriginSuffix, aOriginNoSuffix));
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvAsyncFlush() {
  StorageDBThread* storageThread = StorageDBThread::Get();
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  storageThread->AsyncFlush();

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvStartup() {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvClearAll() {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  storageThread->AsyncClearAll();

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvClearMatchingOrigin(
    const nsCString& aOriginNoSuffix) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  storageThread->AsyncClearMatchingOrigin(aOriginNoSuffix);

  return IPC_OK();
}

mozilla::ipc::IPCResult StorageDBParent::RecvClearMatchingOriginAttributes(
    const OriginAttributesPattern& aPattern) {
  StorageDBThread* storageThread = StorageDBThread::GetOrCreate(mProfilePath);
  if (!storageThread) {
    return IPC_FAIL_NO_REASON(this);
  }

  storageThread->AsyncClearMatchingOriginAttributes(aPattern);

  return IPC_OK();
}

void StorageDBParent::Observe(const nsCString& aTopic,
                              const nsString& aOriginAttributesPattern,
                              const nsCString& aOriginScope) {
  if (mIPCOpen) {
    mozilla::Unused << SendObserve(aTopic, aOriginAttributesPattern,
                                   aOriginScope);
  }
}

namespace {

// Results must be sent back on the main thread
class LoadRunnable : public Runnable {
 public:
  enum TaskType { loadItem, loadDone };

  LoadRunnable(StorageDBParent* aParent, TaskType aType,
               const nsACString& aOriginSuffix,
               const nsACString& aOriginNoSuffix,
               const nsAString& aKey = EmptyString(),
               const nsAString& aValue = EmptyString())
      : Runnable("dom::LoadRunnable"),
        mParent(aParent),
        mType(aType),
        mSuffix(aOriginSuffix),
        mOrigin(aOriginNoSuffix),
        mKey(aKey),
        mValue(aValue),
        mRv(NS_ERROR_NOT_INITIALIZED) {}

  LoadRunnable(StorageDBParent* aParent, TaskType aType,
               const nsACString& aOriginSuffix,
               const nsACString& aOriginNoSuffix, nsresult aRv)
      : Runnable("dom::LoadRunnable"),
        mParent(aParent),
        mType(aType),
        mSuffix(aOriginSuffix),
        mOrigin(aOriginNoSuffix),
        mRv(aRv) {}

 private:
  RefPtr<StorageDBParent> mParent;
  TaskType mType;
  nsCString mSuffix, mOrigin;
  nsString mKey;
  nsString mValue;
  nsresult mRv;

  NS_IMETHOD Run() override {
    if (!mParent->IPCOpen()) {
      return NS_OK;
    }

    switch (mType) {
      case loadItem:
        mozilla::Unused << mParent->SendLoadItem(mSuffix, mOrigin, mKey,
                                                 mValue);
        break;
      case loadDone:
        mozilla::Unused << mParent->SendLoadDone(mSuffix, mOrigin, mRv);
        break;
    }

    mParent = nullptr;

    return NS_OK;
  }
};

}  // namespace

// StorageDBParent::CacheParentBridge

const nsCString StorageDBParent::CacheParentBridge::Origin() const {
  return LocalStorageManager::CreateOrigin(mOriginSuffix, mOriginNoSuffix);
}

bool StorageDBParent::CacheParentBridge::LoadItem(const nsAString& aKey,
                                                  const nsString& aValue) {
  if (mLoaded) {
    return false;
  }

  ++mLoadedCount;

  RefPtr<LoadRunnable> r =
      new LoadRunnable(mParent, LoadRunnable::loadItem, mOriginSuffix,
                       mOriginNoSuffix, aKey, aValue);

  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(r, NS_DISPATCH_NORMAL));

  return true;
}

void StorageDBParent::CacheParentBridge::LoadDone(nsresult aRv) {
  // Prevent send of duplicate LoadDone.
  if (mLoaded) {
    return;
  }

  mLoaded = true;

  RefPtr<LoadRunnable> r = new LoadRunnable(
      mParent, LoadRunnable::loadDone, mOriginSuffix, mOriginNoSuffix, aRv);

  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(r, NS_DISPATCH_NORMAL));
}

void StorageDBParent::CacheParentBridge::LoadWait() {
  // Should never be called on this implementation
  MOZ_ASSERT(false);
}

// XXX Fix me!
// This should be just:
// NS_IMPL_RELEASE_WITH_DESTROY(StorageDBParent::CacheParentBridge, Destroy)
// But due to different strings used for refcount logging and different return
// types, this is done manually for now.
NS_IMETHODIMP_(void)
StorageDBParent::CacheParentBridge::Release(void) {
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "LocalStorageCacheBridge");
  if (0 == count) {
    mRefCnt = 1; /* stabilize */
    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(_class); */
    Destroy();
  }
}

void StorageDBParent::CacheParentBridge::Destroy() {
  if (mOwningEventTarget->IsOnCurrentThread()) {
    delete this;
    return;
  }

  RefPtr<Runnable> destroyRunnable = NewNonOwningRunnableMethod(
      "CacheParentBridge::Destroy", this, &CacheParentBridge::Destroy);

  MOZ_ALWAYS_SUCCEEDS(
      mOwningEventTarget->Dispatch(destroyRunnable, NS_DISPATCH_NORMAL));
}

// StorageDBParent::UsageParentBridge

namespace {

class UsageRunnable : public Runnable {
 public:
  UsageRunnable(StorageDBParent* aParent, const nsACString& aOriginScope,
                const int64_t& aUsage)
      : Runnable("dom::UsageRunnable"),
        mParent(aParent),
        mOriginScope(aOriginScope),
        mUsage(aUsage) {}

 private:
  NS_IMETHOD Run() override {
    if (!mParent->IPCOpen()) {
      return NS_OK;
    }

    mozilla::Unused << mParent->SendLoadUsage(mOriginScope, mUsage);

    mParent = nullptr;

    return NS_OK;
  }

  RefPtr<StorageDBParent> mParent;
  nsCString mOriginScope;
  int64_t mUsage;
};

}  // namespace

void StorageDBParent::UsageParentBridge::LoadUsage(const int64_t aUsage) {
  RefPtr<UsageRunnable> r = new UsageRunnable(mParent, mOriginScope, aUsage);

  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(r, NS_DISPATCH_NORMAL));
}

// XXX Fix me!
// This should be just:
// NS_IMPL_RELEASE_WITH_DESTROY(StorageDBParent::UsageParentBridge, Destroy)
// But due to different strings used for refcount logging, this is done manually
// for now.
NS_IMETHODIMP_(MozExternalRefCountType)
StorageDBParent::UsageParentBridge::Release(void) {
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "StorageUsageBridge");
  if (count == 0) {
    Destroy();
    return 0;
  }
  return count;
}

void StorageDBParent::UsageParentBridge::Destroy() {
  if (mOwningEventTarget->IsOnCurrentThread()) {
    delete this;
    return;
  }

  RefPtr<Runnable> destroyRunnable = NewNonOwningRunnableMethod(
      "UsageParentBridge::Destroy", this, &UsageParentBridge::Destroy);

  MOZ_ALWAYS_SUCCEEDS(
      mOwningEventTarget->Dispatch(destroyRunnable, NS_DISPATCH_NORMAL));
}

void StorageDBParent::ObserverSink::Start() {
  AssertIsOnBackgroundThread();

  RefPtr<Runnable> runnable =
      NewRunnableMethod("StorageDBParent::ObserverSink::AddSink", this,
                        &StorageDBParent::ObserverSink::AddSink);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));
}

void StorageDBParent::ObserverSink::Stop() {
  AssertIsOnBackgroundThread();

  mActor = nullptr;

  RefPtr<Runnable> runnable =
      NewRunnableMethod("StorageDBParent::ObserverSink::RemoveSink", this,
                        &StorageDBParent::ObserverSink::RemoveSink);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));
}

void StorageDBParent::ObserverSink::AddSink() {
  MOZ_ASSERT(NS_IsMainThread());

  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->AddSink(this);
  }
}

void StorageDBParent::ObserverSink::RemoveSink() {
  MOZ_ASSERT(NS_IsMainThread());

  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->RemoveSink(this);
  }
}

void StorageDBParent::ObserverSink::Notify(
    const nsCString& aTopic, const nsString& aOriginAttributesPattern,
    const nsCString& aOriginScope) {
  AssertIsOnBackgroundThread();

  if (mActor) {
    mActor->Observe(aTopic, aOriginAttributesPattern, aOriginScope);
  }
}

nsresult StorageDBParent::ObserverSink::Observe(
    const char* aTopic, const nsAString& aOriginAttributesPattern,
    const nsACString& aOriginScope) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Runnable> runnable = NewRunnableMethod<nsCString, nsString, nsCString>(
      "StorageDBParent::ObserverSink::Observe2", this,
      &StorageDBParent::ObserverSink::Notify, aTopic, aOriginAttributesPattern,
      aOriginScope);

  MOZ_ALWAYS_SUCCEEDS(
      mOwningEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL));

  return NS_OK;
}

SessionStorageObserverParent::SessionStorageObserverParent()
    : mActorDestroyed(false) {
  MOZ_ASSERT(NS_IsMainThread());

  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->AddSink(this);
  }
}

SessionStorageObserverParent::~SessionStorageObserverParent() {
  MOZ_ASSERT(mActorDestroyed);

  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->RemoveSink(this);
  }
}

void SessionStorageObserverParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;
}

mozilla::ipc::IPCResult SessionStorageObserverParent::RecvDeleteMe() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PSessionStorageObserverParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

nsresult SessionStorageObserverParent::Observe(
    const char* aTopic, const nsAString& aOriginAttributesPattern,
    const nsACString& aOriginScope) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mActorDestroyed) {
    mozilla::Unused << SendObserve(nsCString(aTopic),
                                   nsString(aOriginAttributesPattern),
                                   nsCString(aOriginScope));
  }
  return NS_OK;
}

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

PBackgroundLocalStorageCacheParent* AllocPBackgroundLocalStorageCacheParent(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const nsCString& aOriginKey, const uint32_t& aPrivateBrowsingId) {
  AssertIsOnBackgroundThread();

  RefPtr<LocalStorageCacheParent> actor = new LocalStorageCacheParent(
      aPrincipalInfo, aOriginKey, aPrivateBrowsingId);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult RecvPBackgroundLocalStorageCacheConstructor(
    mozilla::ipc::PBackgroundParent* aBackgroundActor,
    PBackgroundLocalStorageCacheParent* aActor,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const nsCString& aOriginKey, const uint32_t& aPrivateBrowsingId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  auto* actor = static_cast<LocalStorageCacheParent*>(aActor);

  if (!gLocalStorageCacheParents) {
    gLocalStorageCacheParents = new LocalStorageCacheParentHashtable();
  }

  nsTArray<LocalStorageCacheParent*>* array;
  if (!gLocalStorageCacheParents->Get(aOriginKey, &array)) {
    array = new nsTArray<LocalStorageCacheParent*>();
    gLocalStorageCacheParents->Put(aOriginKey, array);
  }
  array->AppendElement(actor);

  // We are currently trusting the content process not to lie to us.  It is
  // future work to consult the ClientManager to determine whether this is a
  // legitimate origin for the content process.

  return IPC_OK();
}

bool DeallocPBackgroundLocalStorageCacheParent(
    PBackgroundLocalStorageCacheParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<LocalStorageCacheParent> actor =
      dont_AddRef(static_cast<LocalStorageCacheParent*>(aActor));

  return true;
}

PBackgroundStorageParent* AllocPBackgroundStorageParent(
    const nsString& aProfilePath) {
  AssertIsOnBackgroundThread();

  return new StorageDBParent(aProfilePath);
}

mozilla::ipc::IPCResult RecvPBackgroundStorageConstructor(
    PBackgroundStorageParent* aActor, const nsString& aProfilePath) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  auto* actor = static_cast<StorageDBParent*>(aActor);
  actor->Init();
  return IPC_OK();
}

bool DeallocPBackgroundStorageParent(PBackgroundStorageParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  StorageDBParent* actor = static_cast<StorageDBParent*>(aActor);
  actor->ReleaseIPDLReference();
  return true;
}

PSessionStorageObserverParent* AllocPSessionStorageObserverParent() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<SessionStorageObserverParent> actor =
      new SessionStorageObserverParent();

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool RecvPSessionStorageObserverConstructor(
    PSessionStorageObserverParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  return true;
}

bool DeallocPSessionStorageObserverParent(
    PSessionStorageObserverParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<SessionStorageObserverParent> actor =
      dont_AddRef(static_cast<SessionStorageObserverParent*>(aActor));

  return true;
}

}  // namespace dom
}  // namespace mozilla
