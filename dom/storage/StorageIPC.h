/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageIPC_h
#define mozilla_dom_StorageIPC_h

#include "mozilla/dom/PStorageChild.h"
#include "mozilla/dom/PStorageParent.h"
#include "StorageDBThread.h"
#include "StorageCache.h"
#include "StorageObserver.h"
#include "mozilla/Mutex.h"
#include "nsAutoPtr.h"

namespace mozilla {

class OriginAttributesPattern;

namespace dom {

class DOMLocalStorageManager;

// Child side of the IPC protocol, exposes as DB interface but
// is responsible to send all requests to the parent process
// and expects asynchronous answers. Those are then transparently
// forwarded back to consumers on the child process.
class StorageDBChild final : public StorageDBBridge
                           , public PStorageChild
{
  virtual ~StorageDBChild();

public:
  explicit StorageDBChild(DOMLocalStorageManager* aManager);

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  virtual nsresult Init();
  virtual nsresult Shutdown();

  virtual void AsyncPreload(StorageCacheBridge* aCache, bool aPriority = false);
  virtual void AsyncGetUsage(StorageUsageBridge* aUsage);

  virtual void SyncPreload(StorageCacheBridge* aCache, bool aForceSync = false);

  virtual nsresult AsyncAddItem(StorageCacheBridge* aCache,
                                const nsAString& aKey, const nsAString& aValue);
  virtual nsresult AsyncUpdateItem(StorageCacheBridge* aCache,
                                   const nsAString& aKey,
                                   const nsAString& aValue);
  virtual nsresult AsyncRemoveItem(StorageCacheBridge* aCache,
                                   const nsAString& aKey);
  virtual nsresult AsyncClear(StorageCacheBridge* aCache);

  virtual void AsyncClearAll()
  {
    if (mOriginsHavingData) {
      mOriginsHavingData->Clear(); /* NO-OP on the child process otherwise */
    }
  }

  virtual void AsyncClearMatchingOrigin(const nsACString& aOriginNoSuffix)
    { /* NO-OP on the child process */ }

  virtual void AsyncClearMatchingOriginAttributes(const OriginAttributesPattern& aPattern)
    { /* NO-OP on the child process */ }

  virtual void AsyncFlush()
    { SendAsyncFlush(); }

  virtual bool ShouldPreloadOrigin(const nsACString& aOriginNoSuffix);
  virtual void GetOriginsHavingData(InfallibleTArray<nsCString>* aOrigins)
    { NS_NOTREACHED("Not implemented for child process"); }

private:
  mozilla::ipc::IPCResult RecvObserve(const nsCString& aTopic,
                                      const nsString& aOriginAttributesPattern,
                                      const nsCString& aOriginScope);
  mozilla::ipc::IPCResult RecvLoadItem(const nsCString& aOriginSuffix,
                                       const nsCString& aOriginNoSuffix,
                                       const nsString& aKey,
                                       const nsString& aValue);
  mozilla::ipc::IPCResult RecvLoadDone(const nsCString& aOriginSuffix,
                                       const nsCString& aOriginNoSuffix,
                                       const nsresult& aRv);
  mozilla::ipc::IPCResult RecvOriginsHavingData(nsTArray<nsCString>&& aOrigins);
  mozilla::ipc::IPCResult RecvLoadUsage(const nsCString& aOriginNoSuffix,
                                        const int64_t& aUsage);
  mozilla::ipc::IPCResult RecvError(const nsresult& aRv);

  nsTHashtable<nsCStringHashKey>& OriginsHavingData();

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  // Held to get caches to forward answers to.
  RefPtr<DOMLocalStorageManager> mManager;

  // Origins having data hash, for optimization purposes only
  nsAutoPtr<nsTHashtable<nsCStringHashKey>> mOriginsHavingData;

  // List of caches waiting for preload.  This ensures the contract that
  // AsyncPreload call references the cache for time of the preload.
  nsTHashtable<nsRefPtrHashKey<StorageCacheBridge>> mLoadingCaches;

  // Status of the remote database
  nsresult mStatus;

  bool mIPCOpen;
};


// Receives async requests from child processes and is responsible
// to send back responses from the DB thread.  Exposes as a fake
// StorageCache consumer.
// Also responsible for forwardning all chrome operation notifications
// such as cookie cleaning etc to the child process.
class StorageDBParent final : public PStorageParent
                            , public StorageObserverSink
{
  virtual ~StorageDBParent();

public:
  StorageDBParent();

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  bool IPCOpen() { return mIPCOpen; }

public:
  // Fake cache class receiving async callbacks from DB thread, sending
  // them back to appropriate cache object on the child process.
  class CacheParentBridge : public StorageCacheBridge {
  public:
    CacheParentBridge(StorageDBParent* aParentDB,
                      const nsACString& aOriginSuffix,
                      const nsACString& aOriginNoSuffix)
      : mParent(aParentDB)
      , mOriginSuffix(aOriginSuffix), mOriginNoSuffix(aOriginNoSuffix)
      , mLoaded(false), mLoadedCount(0) {}
    virtual ~CacheParentBridge() {}

    // StorageCacheBridge
    virtual const nsCString Origin() const;
    virtual const nsCString& OriginNoSuffix() const
      { return mOriginNoSuffix; }
    virtual const nsCString& OriginSuffix() const
      { return mOriginSuffix; }
    virtual bool Loaded()
      { return mLoaded; }
    virtual uint32_t LoadedCount()
      { return mLoadedCount; }

    virtual bool LoadItem(const nsAString& aKey, const nsString& aValue);
    virtual void LoadDone(nsresult aRv);
    virtual void LoadWait();

  private:
    RefPtr<StorageDBParent> mParent;
    nsCString mOriginSuffix, mOriginNoSuffix;
    bool mLoaded;
    uint32_t mLoadedCount;
  };

  // Fake usage class receiving async callbacks from DB thread
  class UsageParentBridge : public StorageUsageBridge
  {
  public:
    UsageParentBridge(StorageDBParent* aParentDB,
                      const nsACString& aOriginScope)
      : mParent(aParentDB), mOriginScope(aOriginScope) {}
    virtual ~UsageParentBridge() {}

    // StorageUsageBridge
    virtual const nsCString& OriginScope() { return mOriginScope; }
    virtual void LoadUsage(const int64_t usage);

  private:
    RefPtr<StorageDBParent> mParent;
    nsCString mOriginScope;
  };

private:
  // IPC
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvAsyncPreload(const nsCString& aOriginSuffix,
                                           const nsCString& aOriginNoSuffix,
                                           const bool& aPriority) override;
  mozilla::ipc::IPCResult RecvPreload(const nsCString& aOriginSuffix,
                                      const nsCString& aOriginNoSuffix,
                                      const uint32_t& aAlreadyLoadedCount,
                                      InfallibleTArray<nsString>* aKeys,
                                      InfallibleTArray<nsString>* aValues,
                                      nsresult* aRv) override;
  mozilla::ipc::IPCResult RecvAsyncGetUsage(const nsCString& aOriginNoSuffix) override;
  mozilla::ipc::IPCResult RecvAsyncAddItem(const nsCString& aOriginSuffix,
                                           const nsCString& aOriginNoSuffix,
                                           const nsString& aKey,
                                           const nsString& aValue) override;
  mozilla::ipc::IPCResult RecvAsyncUpdateItem(const nsCString& aOriginSuffix,
                                              const nsCString& aOriginNoSuffix,
                                              const nsString& aKey,
                                              const nsString& aValue) override;
  mozilla::ipc::IPCResult RecvAsyncRemoveItem(const nsCString& aOriginSuffix,
                                              const nsCString& aOriginNoSuffix,
                                              const nsString& aKey) override;
  mozilla::ipc::IPCResult RecvAsyncClear(const nsCString& aOriginSuffix,
                                         const nsCString& aOriginNoSuffix) override;
  mozilla::ipc::IPCResult RecvAsyncFlush() override;

  // StorageObserverSink
  virtual nsresult Observe(const char* aTopic,
                           const nsAString& aOriginAttrPattern,
                           const nsACString& aOriginScope) override;

private:
  CacheParentBridge* NewCache(const nsACString& aOriginSuffix,
                              const nsACString& aOriginNoSuffix);

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  // True when IPC channel is open and Send*() methods are OK to use.
  bool mIPCOpen;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StorageIPC_h
