/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageCache.h"

#include "Storage.h"
#include "StorageDBThread.h"
#include "StorageIPC.h"
#include "StorageUtils.h"
#include "LocalStorageManager.h"

#include "nsDOMString.h"
#include "nsXULAppAPI.h"
#include "mozilla/Unused.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

#define DOM_STORAGE_CACHE_KEEP_ALIVE_TIME_MS 20000

namespace {

const uint32_t kDefaultSet = 0;
const uint32_t kSessionSet = 1;

inline uint32_t GetDataSetIndex(bool aPrivateBrowsing,
                                bool aSessionScopedOrLess) {
  if (!aPrivateBrowsing && aSessionScopedOrLess) {
    return kSessionSet;
  }

  return kDefaultSet;
}

inline uint32_t GetDataSetIndex(const LocalStorage* aStorage) {
  return GetDataSetIndex(aStorage->IsPrivateBrowsing(),
                         aStorage->IsSessionScopedOrLess());
}

}  // namespace

// LocalStorageCacheBridge

NS_IMPL_ADDREF(LocalStorageCacheBridge)

// Since there is no consumer of return value of Release, we can turn this
// method to void to make implementation of asynchronous
// LocalStorageCache::Release much simpler.
NS_IMETHODIMP_(void) LocalStorageCacheBridge::Release(void) {
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "LocalStorageCacheBridge");
  if (0 == count) {
    mRefCnt = 1; /* stabilize */
    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(_class); */
    delete (this);
  }
}

// LocalStorageCache

LocalStorageCache::LocalStorageCache(const nsACString* aOriginNoSuffix)
    : mActor(nullptr),
      mOriginNoSuffix(*aOriginNoSuffix),
      mMonitor("LocalStorageCache"),
      mLoaded(false),
      mLoadResult(NS_OK),
      mInitialized(false),
      mPersistent(false),
      mPreloadTelemetryRecorded(false) {
  MOZ_COUNT_CTOR(LocalStorageCache);
}

LocalStorageCache::~LocalStorageCache() {
  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }

  if (mManager) {
    mManager->DropCache(this);
  }

  MOZ_COUNT_DTOR(LocalStorageCache);
}

void LocalStorageCache::SetActor(LocalStorageCacheChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

NS_IMETHODIMP_(void)
LocalStorageCache::Release(void) {
  // We must actually release on the main thread since the cache removes it
  // self from the manager's hash table.  And we don't want to lock access to
  // that hash table.
  if (NS_IsMainThread()) {
    LocalStorageCacheBridge::Release();
    return;
  }

  RefPtr<nsRunnableMethod<LocalStorageCacheBridge, void, false>> event =
      NewNonOwningRunnableMethod("dom::LocalStorageCacheBridge::Release",
                                 static_cast<LocalStorageCacheBridge*>(this),
                                 &LocalStorageCacheBridge::Release);

  nsresult rv = NS_DispatchToMainThread(event);
  if (NS_FAILED(rv)) {
    NS_WARNING("LocalStorageCache::Release() on a non-main thread");
    LocalStorageCacheBridge::Release();
  }
}

void LocalStorageCache::Init(LocalStorageManager* aManager, bool aPersistent,
                             nsIPrincipal* aPrincipal,
                             const nsACString& aQuotaOriginScope) {
  MOZ_ASSERT(!aQuotaOriginScope.IsEmpty());

  if (mInitialized) {
    return;
  }

  mInitialized = true;
  aPrincipal->OriginAttributesRef().CreateSuffix(mOriginSuffix);
  mPrivateBrowsingId = aPrincipal->GetPrivateBrowsingId();
  mPersistent = aPersistent;
  mQuotaOriginScope = aQuotaOriginScope;

  if (mPersistent) {
    mManager = aManager;
    Preload();
  }

  // Check the quota string has (or has not) the identical origin suffix as
  // this storage cache is bound to.
  MOZ_ASSERT(StringBeginsWith(mQuotaOriginScope, mOriginSuffix));
  MOZ_ASSERT(mOriginSuffix.IsEmpty() !=
             StringBeginsWith(mQuotaOriginScope, "^"_ns));

  mUsage = aManager->GetOriginUsage(mQuotaOriginScope, mPrivateBrowsingId);
}

void LocalStorageCache::NotifyObservers(const LocalStorage* aStorage,
                                        const nsString& aKey,
                                        const nsString& aOldValue,
                                        const nsString& aNewValue) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aStorage);

  if (!mActor) {
    return;
  }

  // We want to send a message to the parent in order to broadcast the
  // StorageEvent correctly to any child process.

  Unused << mActor->SendNotify(aStorage->DocumentURI(), aKey, aOldValue,
                               aNewValue);
}

inline bool LocalStorageCache::Persist(const LocalStorage* aStorage) const {
  return mPersistent &&
         (aStorage->IsPrivateBrowsing() || !aStorage->IsSessionScopedOrLess());
}

const nsCString LocalStorageCache::Origin() const {
  return LocalStorageManager::CreateOrigin(mOriginSuffix, mOriginNoSuffix);
}

LocalStorageCache::Data& LocalStorageCache::DataSet(
    const LocalStorage* aStorage) {
  return mData[GetDataSetIndex(aStorage)];
}

bool LocalStorageCache::ProcessUsageDelta(const LocalStorage* aStorage,
                                          int64_t aDelta,
                                          const MutationSource aSource) {
  return ProcessUsageDelta(GetDataSetIndex(aStorage), aDelta, aSource);
}

bool LocalStorageCache::ProcessUsageDelta(uint32_t aGetDataSetIndex,
                                          const int64_t aDelta,
                                          const MutationSource aSource) {
  // Check limit per this origin
  Data& data = mData[aGetDataSetIndex];
  uint64_t newOriginUsage = data.mOriginQuotaUsage + aDelta;
  if (aSource == ContentMutation && aDelta > 0 &&
      newOriginUsage > LocalStorageManager::GetOriginQuota()) {
    return false;
  }

  // Now check eTLD+1 limit
  if (mUsage &&
      !mUsage->CheckAndSetETLD1UsageDelta(aGetDataSetIndex, aDelta, aSource)) {
    return false;
  }

  // Update size in our data set
  data.mOriginQuotaUsage = newOriginUsage;
  return true;
}

void LocalStorageCache::Preload() {
  if (mLoaded || !mPersistent) {
    return;
  }

  StorageDBChild* storageChild =
      StorageDBChild::GetOrCreate(mPrivateBrowsingId);
  if (!storageChild) {
    mLoaded = true;
    mLoadResult = NS_ERROR_FAILURE;
    return;
  }

  storageChild->AsyncPreload(this);
}

void LocalStorageCache::WaitForPreload(Telemetry::HistogramID aTelemetryID) {
  if (!mPersistent) {
    return;
  }

  bool loaded = mLoaded;

  // Telemetry of rates of pending preloads
  if (!mPreloadTelemetryRecorded) {
    mPreloadTelemetryRecorded = true;
    Telemetry::Accumulate(
        Telemetry::LOCALDOMSTORAGE_PRELOAD_PENDING_ON_FIRST_ACCESS, !loaded);
  }

  if (loaded) {
    return;
  }

  // Measure which operation blocks and for how long
  Telemetry::RuntimeAutoTimer timer(aTelemetryID);

  // If preload already started (i.e. we got some first data, but not all)
  // SyncPreload will just wait for it to finish rather then synchronously
  // read from the database.  It seems to me more optimal.

  // TODO place for A/B testing (force main thread load vs. let preload finish)

  // No need to check sDatabase for being non-null since preload is either
  // done before we've shut the DB down or when the DB could not start,
  // preload has not even be started.
  StorageDBChild::Get(mPrivateBrowsingId)->SyncPreload(this);
}

nsresult LocalStorageCache::GetLength(const LocalStorage* aStorage,
                                      uint32_t* aRetval) {
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETLENGTH_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  *aRetval = DataSet(aStorage).mKeys.Count();
  return NS_OK;
}

nsresult LocalStorageCache::GetKey(const LocalStorage* aStorage,
                                   uint32_t aIndex, nsAString& aRetval) {
  // XXX: This does a linear search for the key at index, which would
  // suck if there's a large numer of indexes. Do we care? If so,
  // maybe we need to have a lazily populated key array here or
  // something?
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETKEY_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  aRetval.SetIsVoid(true);
  for (auto iter = DataSet(aStorage).mKeys.Iter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aRetval = iter.Key();
      break;
    }
    aIndex--;
  }

  return NS_OK;
}

void LocalStorageCache::GetKeys(const LocalStorage* aStorage,
                                nsTArray<nsString>& aKeys) {
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETALLKEYS_BLOCKING_MS);
  }

  if (NS_FAILED(mLoadResult)) {
    return;
  }

  AppendToArray(aKeys, DataSet(aStorage).mKeys.Keys());
}

nsresult LocalStorageCache::GetItem(const LocalStorage* aStorage,
                                    const nsAString& aKey, nsAString& aRetval) {
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETVALUE_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  // not using AutoString since we don't want to copy buffer to result
  nsString value;
  if (!DataSet(aStorage).mKeys.Get(aKey, &value)) {
    SetDOMStringToNull(value);
  }

  aRetval = value;

  return NS_OK;
}

nsresult LocalStorageCache::SetItem(const LocalStorage* aStorage,
                                    const nsAString& aKey,
                                    const nsString& aValue, nsString& aOld,
                                    const MutationSource aSource) {
  // Size of the cache that will change after this action.
  int64_t delta = 0;

  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_SETVALUE_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  Data& data = DataSet(aStorage);
  if (!data.mKeys.Get(aKey, &aOld)) {
    SetDOMStringToNull(aOld);

    // We only consider key size if the key doesn't exist before.
    delta += static_cast<int64_t>(aKey.Length());
  }

  delta += static_cast<int64_t>(aValue.Length()) -
           static_cast<int64_t>(aOld.Length());

  if (!ProcessUsageDelta(aStorage, delta, aSource)) {
    return NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
  }

  if (aValue == aOld && DOMStringIsNull(aValue) == DOMStringIsNull(aOld)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  data.mKeys.InsertOrUpdate(aKey, aValue);

  if (aSource != ContentMutation) {
    return NS_OK;
  }

#if !defined(MOZ_WIDGET_ANDROID)
  NotifyObservers(aStorage, nsString(aKey), aOld, aValue);
#endif

  if (Persist(aStorage)) {
    StorageDBChild* storageChild = StorageDBChild::Get(mPrivateBrowsingId);
    if (!storageChild) {
      NS_ERROR(
          "Writing to localStorage after the database has been shut down"
          ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    if (DOMStringIsNull(aOld)) {
      return storageChild->AsyncAddItem(this, aKey, aValue);
    }

    return storageChild->AsyncUpdateItem(this, aKey, aValue);
  }

  return NS_OK;
}

nsresult LocalStorageCache::RemoveItem(const LocalStorage* aStorage,
                                       const nsAString& aKey, nsString& aOld,
                                       const MutationSource aSource) {
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_REMOVEKEY_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  Data& data = DataSet(aStorage);
  if (!data.mKeys.Get(aKey, &aOld)) {
    SetDOMStringToNull(aOld);
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  // Recalculate the cached data size
  const int64_t delta = -(static_cast<int64_t>(aOld.Length()) +
                          static_cast<int64_t>(aKey.Length()));
  Unused << ProcessUsageDelta(aStorage, delta, aSource);
  data.mKeys.Remove(aKey);

  if (aSource != ContentMutation) {
    return NS_OK;
  }

#if !defined(MOZ_WIDGET_ANDROID)
  NotifyObservers(aStorage, nsString(aKey), aOld, VoidString());
#endif

  if (Persist(aStorage)) {
    StorageDBChild* storageChild = StorageDBChild::Get(mPrivateBrowsingId);
    if (!storageChild) {
      NS_ERROR(
          "Writing to localStorage after the database has been shut down"
          ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    return storageChild->AsyncRemoveItem(this, aKey);
  }

  return NS_OK;
}

nsresult LocalStorageCache::Clear(const LocalStorage* aStorage,
                                  const MutationSource aSource) {
  bool refresh = false;
  if (Persist(aStorage)) {
    // We need to preload all data (know the size) before we can proceeed
    // to correctly decrease cached usage number.
    // XXX as in case of unload, this is not technically needed now, but
    // after super-scope quota introduction we have to do this.  Get telemetry
    // right now.
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_CLEAR_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      // When we failed to load data from the database, force delete of the
      // scope data and make use of the storage possible again.
      refresh = true;
      mLoadResult = NS_OK;
    }
  }

  Data& data = DataSet(aStorage);
  bool hadData = !!data.mKeys.Count();

  if (hadData) {
    Unused << ProcessUsageDelta(aStorage, -data.mOriginQuotaUsage, aSource);
    data.mKeys.Clear();
  }

  if (aSource != ContentMutation) {
    return hadData ? NS_OK : NS_SUCCESS_DOM_NO_OPERATION;
  }

#if !defined(MOZ_WIDGET_ANDROID)
  if (hadData) {
    NotifyObservers(aStorage, VoidString(), VoidString(), VoidString());
  }
#endif

  if (Persist(aStorage) && (refresh || hadData)) {
    StorageDBChild* storageChild = StorageDBChild::Get(mPrivateBrowsingId);
    if (!storageChild) {
      NS_ERROR(
          "Writing to localStorage after the database has been shut down"
          ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    return storageChild->AsyncClear(this);
  }

  return hadData ? NS_OK : NS_SUCCESS_DOM_NO_OPERATION;
}

int64_t LocalStorageCache::GetOriginQuotaUsage(
    const LocalStorage* aStorage) const {
  return mData[GetDataSetIndex(aStorage)].mOriginQuotaUsage;
}

void LocalStorageCache::UnloadItems(uint32_t aUnloadFlags) {
  if (aUnloadFlags & kUnloadDefault) {
    // Must wait for preload to pass correct usage to ProcessUsageDelta
    // XXX this is not technically needed right now since there is just
    // per-origin isolated quota handling, but when we introduce super-
    // -scope quotas, we have to do this.  Better to start getting
    // telemetry right now.
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_UNLOAD_BLOCKING_MS);

    mData[kDefaultSet].mKeys.Clear();
    ProcessUsageDelta(kDefaultSet, -mData[kDefaultSet].mOriginQuotaUsage);
  }

  if (aUnloadFlags & kUnloadSession) {
    mData[kSessionSet].mKeys.Clear();
    ProcessUsageDelta(kSessionSet, -mData[kSessionSet].mOriginQuotaUsage);
  }

#ifdef DOM_STORAGE_TESTS
  if (aUnloadFlags & kTestReload) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_UNLOAD_BLOCKING_MS);

    mData[kDefaultSet].mKeys.Clear();
    mLoaded = false;  // This is only used in testing code
    Preload();
  }
#endif
}

// LocalStorageCacheBridge

uint32_t LocalStorageCache::LoadedCount() {
  MonitorAutoLock monitor(mMonitor);
  Data& data = mData[kDefaultSet];
  return data.mKeys.Count();
}

bool LocalStorageCache::LoadItem(const nsAString& aKey,
                                 const nsString& aValue) {
  MonitorAutoLock monitor(mMonitor);
  if (mLoaded) {
    return false;
  }

  Data& data = mData[kDefaultSet];
  data.mKeys.LookupOrInsertWith(aKey, [&] {
    data.mOriginQuotaUsage += aKey.Length() + aValue.Length();
    return aValue;
  });
  return true;
}

void LocalStorageCache::LoadDone(nsresult aRv) {
  MonitorAutoLock monitor(mMonitor);
  mLoadResult = aRv;
  mLoaded = true;
  monitor.Notify();
}

void LocalStorageCache::LoadWait() {
  MonitorAutoLock monitor(mMonitor);
  while (!mLoaded) {
    monitor.Wait();
  }
}

// StorageUsage

StorageUsage::StorageUsage(const nsACString& aOriginScope)
    : mOriginScope(aOriginScope) {
  mUsage[kDefaultSet] = mUsage[kSessionSet] = 0LL;
}

namespace {

class LoadUsageRunnable : public Runnable {
 public:
  LoadUsageRunnable(int64_t* aUsage, const int64_t aDelta)
      : Runnable("dom::LoadUsageRunnable"), mTarget(aUsage), mDelta(aDelta) {}

 private:
  int64_t* mTarget;
  int64_t mDelta;

  NS_IMETHOD Run() override {
    *mTarget = mDelta;
    return NS_OK;
  }
};

}  // namespace

void StorageUsage::LoadUsage(const int64_t aUsage) {
  // Using kDefaultSet index since it is the index for the persitent data
  // stored in the database we have just loaded usage for.
  if (!NS_IsMainThread()) {
    // In single process scenario we get this call from the DB thread
    RefPtr<LoadUsageRunnable> r =
        new LoadUsageRunnable(mUsage + kDefaultSet, aUsage);
    NS_DispatchToMainThread(r);
  } else {
    // On a child process we get this on the main thread already
    mUsage[kDefaultSet] += aUsage;
  }
}

bool StorageUsage::CheckAndSetETLD1UsageDelta(
    uint32_t aDataSetIndex, const int64_t aDelta,
    const LocalStorageCache::MutationSource aSource) {
  MOZ_ASSERT(NS_IsMainThread());

  int64_t newUsage = mUsage[aDataSetIndex] + aDelta;
  if (aSource == LocalStorageCache::ContentMutation && aDelta > 0 &&
      newUsage > LocalStorageManager::GetSiteQuota()) {
    return false;
  }

  mUsage[aDataSetIndex] = newUsage;
  return true;
}

}  // namespace dom
}  // namespace mozilla
