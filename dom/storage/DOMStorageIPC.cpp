/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMStorageIPC.h"

#include "DOMStorageManager.h"

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

NS_IMPL_ADDREF(DOMStorageDBChild)

NS_IMETHODIMP_(MozExternalRefCountType) DOMStorageDBChild::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "DOMStorageDBChild");
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
DOMStorageDBChild::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen, "Attempting to retain multiple IPDL references");
  mIPCOpen = true;
  AddRef();
}

void
DOMStorageDBChild::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen, "Attempting to release non-existent IPDL reference");
  mIPCOpen = false;
  Release();
}

DOMStorageDBChild::DOMStorageDBChild(DOMLocalStorageManager* aManager)
  : mManager(aManager)
  , mStatus(NS_OK)
  , mIPCOpen(false)
{
}

DOMStorageDBChild::~DOMStorageDBChild()
{
}

nsTHashtable<nsCStringHashKey>&
DOMStorageDBChild::OriginsHavingData()
{
  if (!mOriginsHavingData) {
    mOriginsHavingData = new nsTHashtable<nsCStringHashKey>;
  }

  return *mOriginsHavingData;
}

nsresult
DOMStorageDBChild::Init()
{
  ContentChild* child = ContentChild::GetSingleton();
  AddIPDLReference();
  child->SendPStorageConstructor(this);
  return NS_OK;
}

nsresult
DOMStorageDBChild::Shutdown()
{
  // There is nothing to do here, IPC will release automatically and
  // the actual thread running on the parent process will also stop
  // automatically in profile-before-change topic observer.
  return NS_OK;
}

void
DOMStorageDBChild::AsyncPreload(DOMStorageCacheBridge* aCache, bool aPriority)
{
  if (mIPCOpen) {
    // Adding ref to cache for the time of preload.  This ensures a reference to
    // to the cache and that all keys will load into this cache object.
    mLoadingCaches.PutEntry(aCache);
    SendAsyncPreload(aCache->OriginSuffix(), aCache->OriginNoSuffix(), aPriority);
  } else {
    // No IPC, no love.  But the LoadDone call is expected.
    aCache->LoadDone(NS_ERROR_UNEXPECTED);
  }
}

void
DOMStorageDBChild::AsyncGetUsage(DOMStorageUsageBridge* aUsage)
{
  if (mIPCOpen) {
    SendAsyncGetUsage(aUsage->OriginScope());
  }
}

void
DOMStorageDBChild::SyncPreload(DOMStorageCacheBridge* aCache, bool aForceSync)
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
  // incoming async responses from the parent, hence we have to do a sync preload
  // instead.  We are smart though, we only demand keys that are left to load in
  // case the async preload has already loaded some keys.
  InfallibleTArray<nsString> keys, values;
  nsresult rv;
  SendPreload(aCache->OriginSuffix(), aCache->OriginNoSuffix(), aCache->LoadedCount(),
              &keys, &values, &rv);

  for (uint32_t i = 0; i < keys.Length(); ++i) {
    aCache->LoadItem(keys[i], values[i]);
  }

  aCache->LoadDone(rv);
}

nsresult
DOMStorageDBChild::AsyncAddItem(DOMStorageCacheBridge* aCache,
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
DOMStorageDBChild::AsyncUpdateItem(DOMStorageCacheBridge* aCache,
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
DOMStorageDBChild::AsyncRemoveItem(DOMStorageCacheBridge* aCache,
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
DOMStorageDBChild::AsyncClear(DOMStorageCacheBridge* aCache)
{
  if (NS_FAILED(mStatus) || !mIPCOpen) {
    return mStatus;
  }

  SendAsyncClear(aCache->OriginSuffix(), aCache->OriginNoSuffix());
  OriginsHavingData().RemoveEntry(aCache->Origin());
  return NS_OK;
}

bool
DOMStorageDBChild::ShouldPreloadOrigin(const nsACString& aOrigin)
{
  // Return true if we didn't receive the origins list yet.
  // I tend to rather preserve a bit of early-after-start performance
  // than a bit of memory here.
  return !mOriginsHavingData || mOriginsHavingData->Contains(aOrigin);
}

bool
DOMStorageDBChild::RecvObserve(const nsCString& aTopic,
                               const nsString& aOriginAttributesPattern,
                               const nsCString& aOriginScope)
{
  DOMStorageObserver::Self()->Notify(
    aTopic.get(), aOriginAttributesPattern, aOriginScope);
  return true;
}

bool
DOMStorageDBChild::RecvOriginsHavingData(nsTArray<nsCString>&& aOrigins)
{
  for (uint32_t i = 0; i < aOrigins.Length(); ++i) {
    OriginsHavingData().PutEntry(aOrigins[i]);
  }

  return true;
}

bool
DOMStorageDBChild::RecvLoadItem(const nsCString& aOriginSuffix,
                                const nsCString& aOriginNoSuffix,
                                const nsString& aKey,
                                const nsString& aValue)
{
  DOMStorageCache* aCache = mManager->GetCache(aOriginSuffix, aOriginNoSuffix);
  if (aCache) {
    aCache->LoadItem(aKey, aValue);
  }

  return true;
}

bool
DOMStorageDBChild::RecvLoadDone(const nsCString& aOriginSuffix,
                                const nsCString& aOriginNoSuffix,
                                const nsresult& aRv)
{
  DOMStorageCache* aCache = mManager->GetCache(aOriginSuffix, aOriginNoSuffix);
  if (aCache) {
    aCache->LoadDone(aRv);

    // Just drop reference to this cache now since the load is done.
    mLoadingCaches.RemoveEntry(static_cast<DOMStorageCacheBridge*>(aCache));
  }

  return true;
}

bool
DOMStorageDBChild::RecvLoadUsage(const nsCString& aOriginNoSuffix, const int64_t& aUsage)
{
  RefPtr<DOMStorageUsageBridge> scopeUsage = mManager->GetOriginUsage(aOriginNoSuffix);
  scopeUsage->LoadUsage(aUsage);
  return true;
}

bool
DOMStorageDBChild::RecvError(const nsresult& aRv)
{
  mStatus = aRv;
  return true;
}

// ----------------------------------------------------------------------------
// Parent
// ----------------------------------------------------------------------------

NS_IMPL_ADDREF(DOMStorageDBParent)
NS_IMPL_RELEASE(DOMStorageDBParent)

void
DOMStorageDBParent::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen, "Attempting to retain multiple IPDL references");
  mIPCOpen = true;
  AddRef();
}

void
DOMStorageDBParent::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen, "Attempting to release non-existent IPDL reference");
  mIPCOpen = false;
  Release();
}

namespace {

class SendInitialChildDataRunnable : public Runnable
{
public:
  explicit SendInitialChildDataRunnable(DOMStorageDBParent* aParent)
    : mParent(aParent)
  {}

private:
  NS_IMETHOD Run() override
  {
    if (!mParent->IPCOpen()) {
      return NS_OK;
    }

    DOMStorageDBBridge* db = DOMStorageCache::GetDatabase();
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

  RefPtr<DOMStorageDBParent> mParent;
};

} // namespace

DOMStorageDBParent::DOMStorageDBParent()
: mIPCOpen(false)
{
  DOMStorageObserver* observer = DOMStorageObserver::Self();
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

DOMStorageDBParent::~DOMStorageDBParent()
{
  DOMStorageObserver* observer = DOMStorageObserver::Self();
  if (observer) {
    observer->RemoveSink(this);
  }
}

mozilla::ipc::IProtocol*
DOMStorageDBParent::CloneProtocol(Channel* aChannel,
                                  mozilla::ipc::ProtocolCloneContext* aCtx)
{
  ContentParent* contentParent = aCtx->GetContentParent();
  nsAutoPtr<PStorageParent> actor(contentParent->AllocPStorageParent());
  if (!actor || !contentParent->RecvPStorageConstructor(actor)) {
    return nullptr;
  }
  return actor.forget();
}

DOMStorageDBParent::CacheParentBridge*
DOMStorageDBParent::NewCache(const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix)
{
  return new CacheParentBridge(this, aOriginSuffix, aOriginNoSuffix);
}

void
DOMStorageDBParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005169
}

bool
DOMStorageDBParent::RecvAsyncPreload(const nsCString& aOriginSuffix,
                                     const nsCString& aOriginNoSuffix,
                                     const bool& aPriority)
{
  DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
  if (!db) {
    return false;
  }

  db->AsyncPreload(NewCache(aOriginSuffix, aOriginNoSuffix), aPriority);
  return true;
}

bool
DOMStorageDBParent::RecvAsyncGetUsage(const nsCString& aOriginNoSuffix)
{
  DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
  if (!db) {
    return false;
  }

  // The object releases it self in LoadUsage method
  RefPtr<UsageParentBridge> usage = new UsageParentBridge(this, aOriginNoSuffix);
  db->AsyncGetUsage(usage);
  return true;
}

namespace {

// We need another implementation of DOMStorageCacheBridge to do
// synchronous IPC preload.  This class just receives Load* notifications
// and fills the returning arguments of RecvPreload with the database
// values for us.
class SyncLoadCacheHelper : public DOMStorageCacheBridge
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
    return DOMStorageManager::CreateOrigin(mSuffix, mOrigin);
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

bool
DOMStorageDBParent::RecvPreload(const nsCString& aOriginSuffix,
                                const nsCString& aOriginNoSuffix,
                                const uint32_t& aAlreadyLoadedCount,
                                InfallibleTArray<nsString>* aKeys,
                                InfallibleTArray<nsString>* aValues,
                                nsresult* aRv)
{
  DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
  if (!db) {
    return false;
  }

  RefPtr<SyncLoadCacheHelper> cache(
    new SyncLoadCacheHelper(aOriginSuffix, aOriginNoSuffix, aAlreadyLoadedCount, aKeys, aValues, aRv));

  db->SyncPreload(cache, true);
  return true;
}

bool
DOMStorageDBParent::RecvAsyncAddItem(const nsCString& aOriginSuffix,
                                     const nsCString& aOriginNoSuffix,
                                     const nsString& aKey,
                                     const nsString& aValue)
{
  DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
  if (!db) {
    return false;
  }

  nsresult rv = db->AsyncAddItem(NewCache(aOriginSuffix, aOriginNoSuffix), aKey, aValue);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return true;
}

bool
DOMStorageDBParent::RecvAsyncUpdateItem(const nsCString& aOriginSuffix,
                                        const nsCString& aOriginNoSuffix,
                                        const nsString& aKey,
                                        const nsString& aValue)
{
  DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
  if (!db) {
    return false;
  }

  nsresult rv = db->AsyncUpdateItem(NewCache(aOriginSuffix, aOriginNoSuffix), aKey, aValue);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return true;
}

bool
DOMStorageDBParent::RecvAsyncRemoveItem(const nsCString& aOriginSuffix,
                                        const nsCString& aOriginNoSuffix,
                                        const nsString& aKey)
{
  DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
  if (!db) {
    return false;
  }

  nsresult rv = db->AsyncRemoveItem(NewCache(aOriginSuffix, aOriginNoSuffix), aKey);
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return true;
}

bool
DOMStorageDBParent::RecvAsyncClear(const nsCString& aOriginSuffix,
                                   const nsCString& aOriginNoSuffix)
{
  DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
  if (!db) {
    return false;
  }

  nsresult rv = db->AsyncClear(NewCache(aOriginSuffix, aOriginNoSuffix));
  if (NS_FAILED(rv) && mIPCOpen) {
    mozilla::Unused << SendError(rv);
  }

  return true;
}

bool
DOMStorageDBParent::RecvAsyncFlush()
{
  DOMStorageDBBridge* db = DOMStorageCache::GetDatabase();
  if (!db) {
    return false;
  }

  db->AsyncFlush();
  return true;
}

// DOMStorageObserverSink

nsresult
DOMStorageDBParent::Observe(const char* aTopic,
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

  LoadRunnable(DOMStorageDBParent* aParent,
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

  LoadRunnable(DOMStorageDBParent* aParent,
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
  RefPtr<DOMStorageDBParent> mParent;
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

// DOMStorageDBParent::CacheParentBridge

const nsCString
DOMStorageDBParent::CacheParentBridge::Origin() const
{
  return DOMStorageManager::CreateOrigin(mOriginSuffix, mOriginNoSuffix);
}

bool
DOMStorageDBParent::CacheParentBridge::LoadItem(const nsAString& aKey, const nsString& aValue)
{
  if (mLoaded) {
    return false;
  }

  ++mLoadedCount;

  RefPtr<LoadRunnable> r =
    new LoadRunnable(mParent, LoadRunnable::loadItem, mOriginSuffix, mOriginNoSuffix, aKey, aValue);
  NS_DispatchToMainThread(r);
  return true;
}

void
DOMStorageDBParent::CacheParentBridge::LoadDone(nsresult aRv)
{
  // Prevent send of duplicate LoadDone.
  if (mLoaded) {
    return;
  }

  mLoaded = true;

  RefPtr<LoadRunnable> r =
    new LoadRunnable(mParent, LoadRunnable::loadDone, mOriginSuffix, mOriginNoSuffix, aRv);
  NS_DispatchToMainThread(r);
}

void
DOMStorageDBParent::CacheParentBridge::LoadWait()
{
  // Should never be called on this implementation
  MOZ_ASSERT(false);
}

// DOMStorageDBParent::UsageParentBridge

namespace {

class UsageRunnable : public Runnable
{
public:
  UsageRunnable(DOMStorageDBParent* aParent, const nsACString& aOriginScope, const int64_t& aUsage)
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

  RefPtr<DOMStorageDBParent> mParent;
  nsCString mOriginScope;
  int64_t mUsage;
};

} // namespace

void
DOMStorageDBParent::UsageParentBridge::LoadUsage(const int64_t aUsage)
{
  RefPtr<UsageRunnable> r = new UsageRunnable(mParent, mOriginScope, aUsage);
  NS_DispatchToMainThread(r);
}

} // namespace dom
} // namespace mozilla
