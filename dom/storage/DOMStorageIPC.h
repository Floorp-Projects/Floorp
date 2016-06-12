/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStorageIPC_h___
#define nsDOMStorageIPC_h___

#include "mozilla/dom/PStorageChild.h"
#include "mozilla/dom/PStorageParent.h"
#include "DOMStorageDBThread.h"
#include "DOMStorageCache.h"
#include "DOMStorageObserver.h"
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
class DOMStorageDBChild final : public DOMStorageDBBridge
                              , public PStorageChild
{
  virtual ~DOMStorageDBChild();

public:
  explicit DOMStorageDBChild(DOMLocalStorageManager* aManager);

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  virtual nsresult Init();
  virtual nsresult Shutdown();

  virtual void AsyncPreload(DOMStorageCacheBridge* aCache, bool aPriority = false);
  virtual void AsyncGetUsage(DOMStorageUsageBridge* aUsage);

  virtual void SyncPreload(DOMStorageCacheBridge* aCache, bool aForceSync = false);

  virtual nsresult AsyncAddItem(DOMStorageCacheBridge* aCache, const nsAString& aKey, const nsAString& aValue);
  virtual nsresult AsyncUpdateItem(DOMStorageCacheBridge* aCache, const nsAString& aKey, const nsAString& aValue);
  virtual nsresult AsyncRemoveItem(DOMStorageCacheBridge* aCache, const nsAString& aKey);
  virtual nsresult AsyncClear(DOMStorageCacheBridge* aCache);

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
  bool RecvObserve(const nsCString& aTopic,
                   const nsString& aOriginAttributesPattern,
                   const nsCString& aOriginScope);
  bool RecvLoadItem(const nsCString& aOriginSuffix,
                    const nsCString& aOriginNoSuffix,
                    const nsString& aKey,
                    const nsString& aValue);
  bool RecvLoadDone(const nsCString& aOriginSuffix,
                    const nsCString& aOriginNoSuffix,
                    const nsresult& aRv);
  bool RecvOriginsHavingData(nsTArray<nsCString>&& aOrigins);
  bool RecvLoadUsage(const nsCString& aOriginNoSuffix,
                     const int64_t& aUsage);
  bool RecvError(const nsresult& aRv);

  nsTHashtable<nsCStringHashKey>& OriginsHavingData();

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  // Held to get caches to forward answers to.
  RefPtr<DOMLocalStorageManager> mManager;

  // Origins having data hash, for optimization purposes only
  nsAutoPtr<nsTHashtable<nsCStringHashKey>> mOriginsHavingData;

  // List of caches waiting for preload.  This ensures the contract that
  // AsyncPreload call references the cache for time of the preload.
  nsTHashtable<nsRefPtrHashKey<DOMStorageCacheBridge>> mLoadingCaches;

  // Status of the remote database
  nsresult mStatus;

  bool mIPCOpen;
};


// Receives async requests from child processes and is responsible
// to send back responses from the DB thread.  Exposes as a fake
// DOMStorageCache consumer.
// Also responsible for forwardning all chrome operation notifications
// such as cookie cleaning etc to the child process.
class DOMStorageDBParent final : public PStorageParent
                               , public DOMStorageObserverSink
{
  virtual ~DOMStorageDBParent();

public:
  DOMStorageDBParent();

  virtual mozilla::ipc::IProtocol*
  CloneProtocol(Channel* aChannel,
                mozilla::ipc::ProtocolCloneContext* aCtx) override;

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  bool IPCOpen() { return mIPCOpen; }

public:
  // Fake cache class receiving async callbacks from DB thread, sending
  // them back to appropriate cache object on the child process.
  class CacheParentBridge : public DOMStorageCacheBridge {
  public:
    CacheParentBridge(DOMStorageDBParent* aParentDB,
                      const nsACString& aOriginSuffix,
                      const nsACString& aOriginNoSuffix)
      : mParent(aParentDB)
      , mOriginSuffix(aOriginSuffix), mOriginNoSuffix(aOriginNoSuffix)
      , mLoaded(false), mLoadedCount(0) {}
    virtual ~CacheParentBridge() {}

    // DOMStorageCacheBridge
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
    RefPtr<DOMStorageDBParent> mParent;
    nsCString mOriginSuffix, mOriginNoSuffix;
    bool mLoaded;
    uint32_t mLoadedCount;
  };

  // Fake usage class receiving async callbacks from DB thread
  class UsageParentBridge : public DOMStorageUsageBridge
  {
  public:
    UsageParentBridge(DOMStorageDBParent* aParentDB, const nsACString& aOriginScope)
      : mParent(aParentDB), mOriginScope(aOriginScope) {}
    virtual ~UsageParentBridge() {}

    // DOMStorageUsageBridge
    virtual const nsCString& OriginScope() { return mOriginScope; }
    virtual void LoadUsage(const int64_t usage);

  private:
    RefPtr<DOMStorageDBParent> mParent;
    nsCString mOriginScope;
  };

private:
  // IPC
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  bool RecvAsyncPreload(const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix, const bool& aPriority) override;
  bool RecvPreload(const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix, const uint32_t& aAlreadyLoadedCount,
                   InfallibleTArray<nsString>* aKeys, InfallibleTArray<nsString>* aValues,
                   nsresult* aRv) override;
  bool RecvAsyncGetUsage(const nsCString& aOriginNoSuffix) override;
  bool RecvAsyncAddItem(const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix, const nsString& aKey, const nsString& aValue) override;
  bool RecvAsyncUpdateItem(const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix, const nsString& aKey, const nsString& aValue) override;
  bool RecvAsyncRemoveItem(const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix, const nsString& aKey) override;
  bool RecvAsyncClear(const nsCString& aOriginSuffix, const nsCString& aOriginNoSuffix) override;
  bool RecvAsyncFlush() override;

  // DOMStorageObserverSink
  virtual nsresult Observe(const char* aTopic, const nsAString& aOriginAttrPattern, const nsACString& aOriginScope) override;

private:
  CacheParentBridge* NewCache(const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix);

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  // True when IPC channel is open and Send*() methods are OK to use.
  bool mIPCOpen;
};

} // namespace dom
} // namespace mozilla

#endif
