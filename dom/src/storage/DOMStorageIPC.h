/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

namespace mozilla {
namespace dom {

class DOMLocalStorageManager;

// Child side of the IPC protocol, exposes as DB interface but
// is responsible to send all requests to the parent process
// and expects asynchronous answers. Those are then transparently
// forwarded back to consumers on the child process.
class DOMStorageDBChild MOZ_FINAL : public DOMStorageDBBridge
                                  , public PStorageChild
{
public:
  DOMStorageDBChild(DOMLocalStorageManager* aManager);
  virtual ~DOMStorageDBChild();

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
    if (mScopesHavingData) {
      mScopesHavingData->Clear(); /* NO-OP on the child process otherwise */
    }
  }

  virtual void AsyncClearMatchingScope(const nsACString& aScope)
    { /* NO-OP on the child process */ }

  virtual void AsyncFlush()
    { SendAsyncFlush(); }

  virtual bool ShouldPreloadScope(const nsACString& aScope);
  virtual void GetScopesHavingData(InfallibleTArray<nsCString>* aScopes)
    { NS_NOTREACHED("Not implemented for child process"); }

private:
  bool RecvObserve(const nsCString& aTopic,
                   const nsCString& aScopePrefix);
  bool RecvLoadItem(const nsCString& aScope,
                    const nsString& aKey,
                    const nsString& aValue);
  bool RecvLoadDone(const nsCString& aScope,
                    const nsresult& aRv);
  bool RecvScopesHavingData(const InfallibleTArray<nsCString>& aScopes);
  bool RecvLoadUsage(const nsCString& aScope,
                     const int64_t& aUsage);
  bool RecvError(const nsresult& aRv);

  nsTHashtable<nsCStringHashKey>& ScopesHavingData();

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  // Held to get caches to forward answers to.
  nsRefPtr<DOMLocalStorageManager> mManager;

  // Scopes having data hash, for optimization purposes only
  nsAutoPtr<nsTHashtable<nsCStringHashKey> > mScopesHavingData;

  // List of caches waiting for preload.  This ensures the contract that
  // AsyncPreload call references the cache for time of the preload.
  nsTHashtable<nsRefPtrHashKey<DOMStorageCacheBridge> > mLoadingCaches;

  // Status of the remote database
  nsresult mStatus;

  bool mIPCOpen;
};


// Receives async requests from child processes and is responsible
// to send back responses from the DB thread.  Exposes as a fake
// DOMStorageCache consumer.
// Also responsible for forwardning all chrome operation notifications
// such as cookie cleaning etc to the child process.
class DOMStorageDBParent MOZ_FINAL : public PStorageParent
                                   , public DOMStorageObserverSink
{
public:
  DOMStorageDBParent();
  virtual ~DOMStorageDBParent();

  virtual mozilla::ipc::IProtocol*
  CloneProtocol(Channel* aChannel,
                mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

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
    CacheParentBridge(DOMStorageDBParent* aParentDB, const nsACString& aScope)
      : mParent(aParentDB), mScope(aScope), mLoaded(false), mLoadedCount(0) {}
    virtual ~CacheParentBridge() {}

    // DOMStorageCacheBridge
    virtual const nsCString& Scope() const
      { return mScope; }
    virtual bool Loaded()
      { return mLoaded; }
    virtual uint32_t LoadedCount()
      { return mLoadedCount; }

    virtual bool LoadItem(const nsAString& aKey, const nsString& aValue);
    virtual void LoadDone(nsresult aRv);
    virtual void LoadWait();

  private:
    nsRefPtr<DOMStorageDBParent> mParent;
    nsCString mScope;
    bool mLoaded;
    uint32_t mLoadedCount;
  };

  // Fake usage class receiving async callbacks from DB thread
  class UsageParentBridge : public DOMStorageUsageBridge
  {
  public:
    UsageParentBridge(DOMStorageDBParent* aParentDB, const nsACString& aScope)
      : mParent(aParentDB), mScope(aScope) {}
    virtual ~UsageParentBridge() {}

    // DOMStorageUsageBridge
    virtual const nsCString& Scope() { return mScope; }
    virtual void LoadUsage(const int64_t usage);

  private:
    nsRefPtr<DOMStorageDBParent> mParent;
    nsCString mScope;
  };

private:
  // IPC
  bool RecvAsyncPreload(const nsCString& aScope, const bool& aPriority);
  bool RecvPreload(const nsCString& aScope, const uint32_t& aAlreadyLoadedCount,
                   InfallibleTArray<nsString>* aKeys, InfallibleTArray<nsString>* aValues,
                   nsresult* aRv);
  bool RecvAsyncGetUsage(const nsCString& aScope);
  bool RecvAsyncAddItem(const nsCString& aScope, const nsString& aKey, const nsString& aValue);
  bool RecvAsyncUpdateItem(const nsCString& aScope, const nsString& aKey, const nsString& aValue);
  bool RecvAsyncRemoveItem(const nsCString& aScope, const nsString& aKey);
  bool RecvAsyncClear(const nsCString& aScope);
  bool RecvAsyncFlush();

  // DOMStorageObserverSink
  virtual nsresult Observe(const char* aTopic, const nsACString& aScopePrefix);

private:
  CacheParentBridge* NewCache(const nsACString& aScope);

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
	
	// True when IPC channel is open and Send*() methods are OK to use.
  bool mIPCOpen;
};

} // ::dom
} // ::mozilla

#endif
