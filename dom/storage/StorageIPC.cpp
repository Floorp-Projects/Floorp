/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageIPC.h"

#include "LocalStorageManager.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/Unused.h"
#include "nsIDiskSpaceWatcher.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

// ----------------------------------------------------------------------------
// Child
// ----------------------------------------------------------------------------

NS_IMPL_ADDREF(StorageDBChild)

NS_IMETHODIMP_(MozExternalRefCountType) StorageDBChild::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "StorageDBChild");
  if (count == 1 && mIPCOpen) {
    Send__delete__(this);
    return 0;
  }
  if (count == 0) {
    mRefCnt = 1;
    delete this;
    return 0;
  }
  return count;
}

void
StorageDBChild::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen, "Attempting to retain multiple IPDL references");
  mIPCOpen = true;
  AddRef();
}

void
StorageDBChild::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen, "Attempting to release non-existent IPDL reference");
  mIPCOpen = false;
  Release();
}

StorageDBChild::StorageDBChild(LocalStorageManager* aManager)
  : mManager(aManager)
  , mStatus(NS_OK)
  , mIPCOpen(false)
{
}

StorageDBChild::~StorageDBChild()
{
}

nsTHashtable<nsCStringHashKey>&
StorageDBChild::OriginsHavingData()
{
  if (!mOriginsHavingData) {
    mOriginsHavingData = new nsTHashtable<nsCStringHashKey>;
  }

  return *mOriginsHavingData;
}

nsresult
StorageDBChild::Init()
{
  ContentChild* child = ContentChild::GetSingleton();
  AddIPDLReference();
  child->SendPStorageConstructor(this);
  return NS_OK;
}

nsresult
StorageDBChild::Shutdown()
{
  // There is nothing to do here, IPC will release automatically and
  // the actual thread running on the parent process will also stop
  // automatically in profile-before-change topic observer.
  return NS_OK;
}

void
StorageDBChild::AsyncPreload(LocalStorageCacheBridge* aCache, bool aPriority)
{
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

void
StorageDBChild::AsyncGetUsage(StorageUsageBridge* aUsage)
{
  if (mIPCOpen) {
    SendAsyncGetUsage(aUsage->OriginScope());
  }
}

void
StorageDBChild::SyncPreload(LocalStorageCacheBridge* aCache, bool aForceSync)
{
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

nsresult
StorageDBChild::AsyncAddItem(LocalStorageCacheBridge* aCache,
                             const nsAString& aKey,
                             const nsAString& aValue)
{
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncAddItem(aCache->OriginSuffix(), aCache->OriginNoSuffix(),
                   nsString(aKey), nsString(aValue));
  OriginsHavingData().PutEntry(aCache->Origin());
  return NS_OK;
}

nsresult
StorageDBChild::AsyncUpdateItem(LocalStorageCacheBridge* aCache,
                                const nsAString& aKey,
                                const nsAString& aValue)
{
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncUpdateItem(aCache->OriginSuffix(), aCache->OriginNoSuffix(),
                      nsString(aKey), nsString(aValue));
  OriginsHavingData().PutEntry(aCache->Origin());
  return NS_OK;
}

nsresult
StorageDBChild::AsyncRemoveItem(LocalStorageCacheBridge* aCache,
                                const nsAString& aKey)
{
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncRemoveItem(aCache->OriginSuffix(), aCache->OriginNoSuffix(),
                      nsString(aKey));
  return NS_OK;
}

nsresult
StorageDBChild::AsyncClear(LocalStorageCacheBridge* aCache)
{
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncClear(aCache->OriginSuffix(), aCache->OriginNoSuffix());
  OriginsHavingData().RemoveEntry(aCache->Origin());
  return NS_OK;
}

bool
StorageDBChild::ShouldPreloadOrigin(const nsACString& aOrigin)
{
  // Return true if we didn't receive the origins list yet.
  // I tend to rather preserve a bit of early-after-start performance
  // than a bit of memory here.
  return !mOriginsHavingData || mOriginsHavingData->Contains(aOrigin);
}

mozilla::ipc::IPCResult
StorageDBChild::RecvObserve(const nsCString& aTopic,
                            const nsString& aOriginAttributesPattern,
                            const nsCString& aOriginScope)
{
  StorageObserver::Self()->Notify(
    aTopic.get(), aOriginAttributesPattern, aOriginScope);
  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBChild::RecvOriginsHavingData(nsTArray<nsCString>&& aOrigins)
{
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

mozilla::ipc::IPCResult
StorageDBChild::RecvLoadItem(const nsCString& aOriginSuffix,
                             const nsCString& aOriginNoSuffix,
                             const nsString& aKey,
                             const nsString& aValue)
{
  LocalStorageCache* aCache =
    mManager->GetCache(aOriginSuffix, aOriginNoSuffix);
  if (aCache) {
    aCache->LoadItem(aKey, aValue);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBChild::RecvLoadDone(const nsCString& aOriginSuffix,
                             const nsCString& aOriginNoSuffix,
                             const nsresult& aRv)
{
  LocalStorageCache* aCache =
    mManager->GetCache(aOriginSuffix, aOriginNoSuffix);
  if (aCache) {
    aCache->LoadDone(aRv);

    // Just drop reference to this cache now since the load is done.
    mLoadingCaches.RemoveEntry(static_cast<LocalStorageCacheBridge*>(aCache));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBChild::RecvLoadUsage(const nsCString& aOriginNoSuffix,
                              const int64_t& aUsage)
{
  RefPtr<StorageUsageBridge> scopeUsage =
    mManager->GetOriginUsage(aOriginNoSuffix);
  scopeUsage->LoadUsage(aUsage);
  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBChild::RecvError(const nsresult& aRv)
{
  mStatus = aRv;
  return IPC_OK();
}

// ----------------------------------------------------------------------------
// Parent
// ----------------------------------------------------------------------------

NS_IMPL_ADDREF(StorageDBParent)
NS_IMPL_RELEASE(StorageDBParent)

void
StorageDBParent::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen, "Attempting to retain multiple IPDL references");
  mIPCOpen = true;
  AddRef();
}

void
StorageDBParent::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen, "Attempting to release non-existent IPDL reference");
  mIPCOpen = false;
  Release();
}

namespace {

class SendInitialChildDataRunnable : public Runnable
{
public:
  explicit SendInitialChildDataRunnable(StorageDBParent* aParent)
    : mParent(aParent)
  {}

private:
  NS_IMETHOD Run() override
  {
    if (!mParent->IPCOpen()) {
      return NS_OK;
    }

    StorageDBBridge* db = LocalStorageCache::GetDatabase();
    if (db) {
      InfallibleTArray<nsCString> scopes;
      db->GetOriginsHavingData(&scopes);
      mozilla::Unused << mParent->SendOriginsHavingData(scopes);
    }

    // We need to check if the device is in a low disk space situation, so
    // we can forbid in that case any write in localStorage.
    nsCOMPtr<nsIDiskSpaceWatcher> diskSpaceWatcher =
      do_GetService("@mozilla.org/toolkit/disk-space-watcher;1");
    if (!diskSpaceWatcher) {
      return NS_OK;
    }

    bool lowDiskSpace = false;
    diskSpaceWatcher->GetIsDiskFull(&lowDiskSpace);

    if (lowDiskSpace) {
      mozilla::Unused << mParent->SendObserve(
        nsDependentCString("low-disk-space"), EmptyString(), EmptyCString());
    }

    return NS_OK;
  }

  RefPtr<StorageDBParent> mParent;
};

} // namespace

StorageDBParent::StorageDBParent()
: mIPCOpen(false)
{
  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->AddSink(this);
  }

  // We are always open by IPC only
  AddIPDLReference();

  // Cannot send directly from here since the channel
  // is not completely built at this moment.
  RefPtr<SendInitialChildDataRunnable> r =
    new SendInitialChildDataRunnable(this);
  NS_DispatchToCurrentThread(r);
}

StorageDBParent::~StorageDBParent()
{
  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->RemoveSink(this);
  }
}

StorageDBParent::CacheParentBridge*
StorageDBParent::NewCache(const nsACString& aOriginSuffix,
                          const nsACString& aOriginNoSuffix)
{
  return new CacheParentBridge(this, aOriginSuffix, aOriginNoSuffix);
}

void
StorageDBParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005169
}

mozilla::ipc::IPCResult
StorageDBParent::RecvAsyncPreload(const nsCString& aOriginSuffix,
                                  const nsCString& aOriginNoSuffix,
                                  const bool& aPriority)
{
  StorageDBBridge* db = LocalStorageCache::StartDatabase();
  if (!db) {
    return IPC_FAIL_NO_REASON(this);
  }

  db->AsyncPreload(NewCache(aOriginSuffix, aOriginNoSuffix), aPriority);
  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBParent::RecvAsyncGetUsage(const nsCString& aOriginNoSuffix)
{
  StorageDBBridge* db = LocalStorageCache::StartDatabase();
  if (!db) {
    return IPC_FAIL_NO_REASON(this);
  }

  // The object releases it self in LoadUsage method
  RefPtr<UsageParentBridge> usage =
    new UsageParentBridge(this, aOriginNoSuffix);
  db->AsyncGetUsage(usage);
  return IPC_OK();
}

namespace {

// We need another implementation of LocalStorageCacheBridge to do
// synchronous IPC preload.  This class just receives Load* notifications
// and fills the returning arguments of RecvPreload with the database
// values for us.
class SyncLoadCacheHelper : public LocalStorageCacheBridge
{
public:
  SyncLoadCacheHelper(const nsCString& aOriginSuffix,
                      const nsCString& aOriginNoSuffix,
                      uint32_t aAlreadyLoadedCount,
                      InfallibleTArray<nsString>* aKeys,
                      InfallibleTArray<nsString>* aValues,
                      nsresult* rv)
  : mMonitor("DOM Storage SyncLoad IPC")
  , mSuffix(aOriginSuffix)
  , mOrigin(aOriginNoSuffix)
  , mKeys(aKeys)
  , mValues(aValues)
  , mRv(rv)
  , mLoaded(false)
  , mLoadedCount(aAlreadyLoadedCount)
  {
    // Precaution
    *mRv = NS_ERROR_UNEXPECTED;
  }

  virtual const nsCString Origin() const
  {
    return LocalStorageManager::CreateOrigin(mSuffix, mOrigin);
  }
  virtual const nsCString& OriginNoSuffix() const { return mOrigin; }
  virtual const nsCString& OriginSuffix() const { return mSuffix; }
  virtual bool Loaded() { return mLoaded; }
  virtual uint32_t LoadedCount() { return mLoadedCount; }
  virtual bool LoadItem(const nsAString& aKey, const nsString& aValue)
  {
    // Called on the aCache background thread
    if (mLoaded) {
      return false;
    }

    ++mLoadedCount;
    mKeys->AppendElement(aKey);
    mValues->AppendElement(aValue);
    return true;
  }

  virtual void LoadDone(nsresult aRv)
  {
    // Called on the aCache background thread
    MonitorAutoLock monitor(mMonitor);
    mLoaded = true;
    *mRv = aRv;
    monitor.Notify();
  }

  virtual void LoadWait()
  {
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

} // namespace

mozilla::ipc::IPCResult
StorageDBParent::RecvPreload(const nsCString& aOriginSuffix,
                             const nsCString& aOriginNoSuffix,
                             const uint32_t& aAlreadyLoadedCount,
                             InfallibleTArray<nsString>* aKeys,
                             InfallibleTArray<nsString>* aValues,
                             nsresult* aRv)
{
  StorageDBBridge* db = LocalStorageCache::StartDatabase();
  if (!db) {
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<SyncLoadCacheHelper> cache(
    new SyncLoadCacheHelper(aOriginSuffix, aOriginNoSuffix, aAlreadyLoadedCount,
                            aKeys, aValues, aRv));

  db->SyncPreload(cache, true);
  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBParent::RecvAsyncAddItem(const nsCString& aOriginSuffix,
                                  const nsCString& aOriginNoSuffix,
                                  const nsString& aKey,
                                  const nsString& aValue)
{
  StorageDBBridge* db = LocalStorageCache::StartDatabase();
  if (!db) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = db->AsyncAddItem(NewCache(aOriginSuffix, aOriginNoSuffix), aKey,
                                 aValue);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBParent::RecvAsyncUpdateItem(const nsCString& aOriginSuffix,
                                     const nsCString& aOriginNoSuffix,
                                     const nsString& aKey,
                                     const nsString& aValue)
{
  StorageDBBridge* db = LocalStorageCache::StartDatabase();
  if (!db) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = db->AsyncUpdateItem(NewCache(aOriginSuffix, aOriginNoSuffix),
                                    aKey, aValue);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBParent::RecvAsyncRemoveItem(const nsCString& aOriginSuffix,
                                     const nsCString& aOriginNoSuffix,
                                     const nsString& aKey)
{
  StorageDBBridge* db = LocalStorageCache::StartDatabase();
  if (!db) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = db->AsyncRemoveItem(NewCache(aOriginSuffix, aOriginNoSuffix),
                                    aKey);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBParent::RecvAsyncClear(const nsCString& aOriginSuffix,
                                const nsCString& aOriginNoSuffix)
{
  StorageDBBridge* db = LocalStorageCache::StartDatabase();
  if (!db) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = db->AsyncClear(NewCache(aOriginSuffix, aOriginNoSuffix));
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
StorageDBParent::RecvAsyncFlush()
{
  StorageDBBridge* db = LocalStorageCache::GetDatabase();
  if (!db) {
    return IPC_FAIL_NO_REASON(this);
  }

  db->AsyncFlush();
  return IPC_OK();
}

// StorageObserverSink

nsresult
StorageDBParent::Observe(const char* aTopic,
                         const nsAString& aOriginAttributesPattern,
                         const nsACString& aOriginScope)
{
  if (mIPCOpen) {
      mozilla::Unused << SendObserve(nsDependentCString(aTopic),
                                     nsString(aOriginAttributesPattern),
                                     nsCString(aOriginScope));
  }

  return NS_OK;
}

namespace {

// Results must be sent back on the main thread
class LoadRunnable : public Runnable
{
public:
  enum TaskType {
    loadItem,
    loadDone
  };

  LoadRunnable(StorageDBParent* aParent,
               TaskType aType,
               const nsACString& aOriginSuffix,
               const nsACString& aOriginNoSuffix,
               const nsAString& aKey = EmptyString(),
               const nsAString& aValue = EmptyString())
  : mParent(aParent)
  , mType(aType)
  , mSuffix(aOriginSuffix)
  , mOrigin(aOriginNoSuffix)
  , mKey(aKey)
  , mValue(aValue)
  { }

  LoadRunnable(StorageDBParent* aParent,
               TaskType aType,
               const nsACString& aOriginSuffix,
               const nsACString& aOriginNoSuffix,
               nsresult aRv)
  : mParent(aParent)
  , mType(aType)
  , mSuffix(aOriginSuffix)
  , mOrigin(aOriginNoSuffix)
  , mRv(aRv)
  { }

private:
  RefPtr<StorageDBParent> mParent;
  TaskType mType;
  nsCString mSuffix, mOrigin;
  nsString mKey;
  nsString mValue;
  nsresult mRv;

  NS_IMETHOD Run() override
  {
    if (!mParent->IPCOpen()) {
      return NS_OK;
    }

    switch (mType)
    {
    case loadItem:
      mozilla::Unused << mParent->SendLoadItem(mSuffix, mOrigin, mKey, mValue);
      break;
    case loadDone:
      mozilla::Unused << mParent->SendLoadDone(mSuffix, mOrigin, mRv);
      break;
    }

    return NS_OK;
  }
};

} // namespace

// StorageDBParent::CacheParentBridge

const nsCString
StorageDBParent::CacheParentBridge::Origin() const
{
  return LocalStorageManager::CreateOrigin(mOriginSuffix, mOriginNoSuffix);
}

bool
StorageDBParent::CacheParentBridge::LoadItem(const nsAString& aKey,
                                             const nsString& aValue)
{
  if (mLoaded) {
    return false;
  }

  ++mLoadedCount;

  RefPtr<LoadRunnable> r =
    new LoadRunnable(mParent, LoadRunnable::loadItem, mOriginSuffix,
                     mOriginNoSuffix, aKey, aValue);
  NS_DispatchToMainThread(r);
  return true;
}

void
StorageDBParent::CacheParentBridge::LoadDone(nsresult aRv)
{
  // Prevent send of duplicate LoadDone.
  if (mLoaded) {
    return;
  }

  mLoaded = true;

  RefPtr<LoadRunnable> r =
    new LoadRunnable(mParent, LoadRunnable::loadDone, mOriginSuffix,
                     mOriginNoSuffix, aRv);
  NS_DispatchToMainThread(r);
}

void
StorageDBParent::CacheParentBridge::LoadWait()
{
  // Should never be called on this implementation
  MOZ_ASSERT(false);
}

// StorageDBParent::UsageParentBridge

namespace {

class UsageRunnable : public Runnable
{
public:
  UsageRunnable(StorageDBParent* aParent, const nsACString& aOriginScope,
                const int64_t& aUsage)
    : mParent(aParent)
    , mOriginScope(aOriginScope)
    , mUsage(aUsage)
  {}

private:
  NS_IMETHOD Run() override
  {
    if (!mParent->IPCOpen()) {
      return NS_OK;
    }

    mozilla::Unused << mParent->SendLoadUsage(mOriginScope, mUsage);
    return NS_OK;
  }

  RefPtr<StorageDBParent> mParent;
  nsCString mOriginScope;
  int64_t mUsage;
};

} // namespace

void
StorageDBParent::UsageParentBridge::LoadUsage(const int64_t aUsage)
{
  RefPtr<UsageRunnable> r = new UsageRunnable(mParent, mOriginScope, aUsage);
  NS_DispatchToMainThread(r);
}

} // namespace dom
} // namespace mozilla
