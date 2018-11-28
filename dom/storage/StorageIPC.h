/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageIPC_h
#define mozilla_dom_StorageIPC_h

#include "mozilla/dom/PBackgroundLocalStorageCacheChild.h"
#include "mozilla/dom/PBackgroundLocalStorageCacheParent.h"
#include "mozilla/dom/PBackgroundStorageChild.h"
#include "mozilla/dom/PBackgroundStorageParent.h"
#include "StorageDBThread.h"
#include "LocalStorageCache.h"
#include "StorageObserver.h"
#include "mozilla/Mutex.h"
#include "nsAutoPtr.h"

namespace mozilla {

class OriginAttributesPattern;

namespace ipc {

class BackgroundChildImpl;
class PrincipalInfo;

} // namespace ipc

namespace dom {

class LocalStorageManager;
class PBackgroundStorageParent;

class LocalStorageCacheChild final
  : public PBackgroundLocalStorageCacheChild
{
  friend class mozilla::ipc::BackgroundChildImpl;
  friend class LocalStorageCache;
  friend class LocalStorageManager;

  // LocalStorageCache effectively owns this instance, although IPC handles its
  // allocation/deallocation.  When the LocalStorageCache destructor runs, it
  // will invoke SendDeleteMeInternal() which will trigger both instances to
  // drop their mutual references and cause IPC to destroy the actor after the
  // DeleteMe round-trip.
  LocalStorageCache* MOZ_NON_OWNING_REF mCache;

  NS_DECL_OWNINGTHREAD

public:
  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LocalStorageCacheChild);
  }

private:
  // Only created by LocalStorageManager.
  explicit LocalStorageCacheChild(LocalStorageCache* aCache);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LocalStorageCacheChild();

  // Only called by LocalStorageCache.
  void
  SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvObserve(const PrincipalInfo& aPrincipalInfo,
              const uint32_t& aPrivateBrowsingId,
              const nsString& aDocumentURI,
              const nsString& aKey,
              const nsString& aOldValue,
              const nsString& aNewValue) override;
};

// Child side of the IPC protocol, exposes as DB interface but
// is responsible to send all requests to the parent process
// and expects asynchronous answers. Those are then transparently
// forwarded back to consumers on the child process.
class StorageDBChild final
  : public PBackgroundStorageChild
{
  class ShutdownObserver;

  virtual ~StorageDBChild();

public:
  explicit StorageDBChild(LocalStorageManager* aManager);

  static StorageDBChild*
  Get();

  static StorageDBChild*
  GetOrCreate();

  NS_INLINE_DECL_REFCOUNTING(StorageDBChild);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  virtual nsresult Init();
  virtual nsresult Shutdown();

  virtual void AsyncPreload(LocalStorageCacheBridge* aCache,
                            bool aPriority = false);
  virtual void AsyncGetUsage(StorageUsageBridge* aUsage);

  virtual void SyncPreload(LocalStorageCacheBridge* aCache,
                           bool aForceSync = false);

  virtual nsresult AsyncAddItem(LocalStorageCacheBridge* aCache,
                                const nsAString& aKey, const nsAString& aValue);
  virtual nsresult AsyncUpdateItem(LocalStorageCacheBridge* aCache,
                                   const nsAString& aKey,
                                   const nsAString& aValue);
  virtual nsresult AsyncRemoveItem(LocalStorageCacheBridge* aCache,
                                   const nsAString& aKey);
  virtual nsresult AsyncClear(LocalStorageCacheBridge* aCache);

  virtual void AsyncClearAll()
  {
    if (mOriginsHavingData) {
      mOriginsHavingData->Clear(); /* NO-OP on the child process otherwise */
    }
  }

  virtual void AsyncClearMatchingOrigin(const nsACString& aOriginNoSuffix)
  {
    MOZ_CRASH("Shouldn't be called!");
  }

  virtual void AsyncClearMatchingOriginAttributes(const OriginAttributesPattern& aPattern)
  {
    MOZ_CRASH("Shouldn't be called!");
  }

  virtual void AsyncFlush()
  {
    MOZ_CRASH("Shouldn't be called!");
  }

  virtual bool ShouldPreloadOrigin(const nsACString& aOriginNoSuffix);

private:
  mozilla::ipc::IPCResult RecvObserve(const nsCString& aTopic,
                                      const nsString& aOriginAttributesPattern,
                                      const nsCString& aOriginScope) override;
  mozilla::ipc::IPCResult RecvLoadItem(const nsCString& aOriginSuffix,
                                       const nsCString& aOriginNoSuffix,
                                       const nsString& aKey,
                                       const nsString& aValue) override;
  mozilla::ipc::IPCResult RecvLoadDone(const nsCString& aOriginSuffix,
                                       const nsCString& aOriginNoSuffix,
                                       const nsresult& aRv) override;
  mozilla::ipc::IPCResult RecvOriginsHavingData(nsTArray<nsCString>&& aOrigins) override;
  mozilla::ipc::IPCResult RecvLoadUsage(const nsCString& aOriginNoSuffix,
                                        const int64_t& aUsage) override;
  mozilla::ipc::IPCResult RecvError(const nsresult& aRv) override;

  nsTHashtable<nsCStringHashKey>& OriginsHavingData();

  // Held to get caches to forward answers to.
  RefPtr<LocalStorageManager> mManager;

  // Origins having data hash, for optimization purposes only
  nsAutoPtr<nsTHashtable<nsCStringHashKey>> mOriginsHavingData;

  // List of caches waiting for preload.  This ensures the contract that
  // AsyncPreload call references the cache for time of the preload.
  nsTHashtable<nsRefPtrHashKey<LocalStorageCacheBridge>> mLoadingCaches;

  // Status of the remote database
  nsresult mStatus;

  bool mIPCOpen;
};

class LocalStorageCacheParent final
  : public PBackgroundLocalStorageCacheParent
{
  const PrincipalInfo mPrincipalInfo;
  const nsCString mOriginKey;
  uint32_t mPrivateBrowsingId;
  bool mActorDestroyed;

public:
  // Created in AllocPBackgroundLocalStorageCacheParent.
  LocalStorageCacheParent(const PrincipalInfo& aPrincipalInfo,
                          const nsACString& aOriginKey,
                          uint32_t aPrivateBrowsingId);

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::LocalStorageCacheParent)

private:
  // Reference counted.
  ~LocalStorageCacheParent();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvDeleteMe() override;

  mozilla::ipc::IPCResult
  RecvNotify(const nsString& aDocumentURI,
             const nsString& aKey,
             const nsString& aOldValue,
             const nsString& aNewValue) override;
};

// Receives async requests from child processes and is responsible
// to send back responses from the DB thread.  Exposes as a fake
// LocalStorageCache consumer.
// Also responsible for forwardning all chrome operation notifications
// such as cookie cleaning etc to the child process.
class StorageDBParent final : public PBackgroundStorageParent
{
  class ObserverSink;

  virtual ~StorageDBParent();

public:
  explicit StorageDBParent(const nsString& aProfilePath);

  void
  Init();

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  bool IPCOpen() { return mIPCOpen; }

public:
  // Fake cache class receiving async callbacks from DB thread, sending
  // them back to appropriate cache object on the child process.
  class CacheParentBridge : public LocalStorageCacheBridge {
  public:
    CacheParentBridge(StorageDBParent* aParentDB,
                      const nsACString& aOriginSuffix,
                      const nsACString& aOriginNoSuffix)
      : mOwningEventTarget(GetCurrentThreadSerialEventTarget())
      , mParent(aParentDB)
      , mOriginSuffix(aOriginSuffix), mOriginNoSuffix(aOriginNoSuffix)
      , mLoaded(false), mLoadedCount(0) {}
    virtual ~CacheParentBridge() {}

    // LocalStorageCacheBridge
    virtual const nsCString Origin() const override;
    virtual const nsCString& OriginNoSuffix() const override
      { return mOriginNoSuffix; }
    virtual const nsCString& OriginSuffix() const override
      { return mOriginSuffix; }
    virtual bool Loaded() override
      { return mLoaded; }
    virtual uint32_t LoadedCount() override
      { return mLoadedCount; }

    virtual bool LoadItem(const nsAString& aKey, const nsString& aValue) override;
    virtual void LoadDone(nsresult aRv) override;
    virtual void LoadWait() override;

    NS_IMETHOD_(void)
    Release(void) override;

  private:
    void
    Destroy();

    nsCOMPtr<nsISerialEventTarget> mOwningEventTarget;
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
      : mOwningEventTarget(GetCurrentThreadSerialEventTarget())
      , mParent(aParentDB)
      , mOriginScope(aOriginScope) {}
    virtual ~UsageParentBridge() {}

    // StorageUsageBridge
    virtual const nsCString& OriginScope() override { return mOriginScope; }
    virtual void LoadUsage(const int64_t usage) override;

    NS_IMETHOD_(MozExternalRefCountType)
    Release(void) override;

  private:
    void
    Destroy();

    nsCOMPtr<nsISerialEventTarget> mOwningEventTarget;
    RefPtr<StorageDBParent> mParent;
    nsCString mOriginScope;
  };

private:
  // IPC
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvDeleteMe() override;

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

  mozilla::ipc::IPCResult RecvStartup() override;
  mozilla::ipc::IPCResult RecvClearAll() override;
  mozilla::ipc::IPCResult RecvClearMatchingOrigin(
                                     const nsCString& aOriginNoSuffix) override;
  mozilla::ipc::IPCResult RecvClearMatchingOriginAttributes(
                              const OriginAttributesPattern& aPattern) override;

  void Observe(const nsCString& aTopic,
               const nsString& aOriginAttrPattern,
               const nsCString& aOriginScope);

private:
  CacheParentBridge* NewCache(const nsACString& aOriginSuffix,
                              const nsACString& aOriginNoSuffix);

  RefPtr<ObserverSink> mObserverSink;

  // A hack to deal with deadlock between the parent process main thread and
  // background thread when invoking StorageDBThread::GetOrCreate because it
  // cannot safely perform a synchronous dispatch back to the main thread
  // (because we are already synchronously doing things on the stack).
  // Populated for the same process actors, empty for other process actors.
  nsString mProfilePath;

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  // True when IPC channel is open and Send*() methods are OK to use.
  bool mIPCOpen;
};

PBackgroundLocalStorageCacheParent*
AllocPBackgroundLocalStorageCacheParent(
                              const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                              const nsCString& aOriginKey,
                              const uint32_t& aPrivateBrowsingId);

mozilla::ipc::IPCResult
RecvPBackgroundLocalStorageCacheConstructor(
                              mozilla::ipc::PBackgroundParent* aBackgroundActor,
                              PBackgroundLocalStorageCacheParent* aActor,
                              const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                              const nsCString& aOriginKey,
                              const uint32_t& aPrivateBrowsingId);

bool
DeallocPBackgroundLocalStorageCacheParent(
                                    PBackgroundLocalStorageCacheParent* aActor);

PBackgroundStorageParent*
AllocPBackgroundStorageParent(const nsString& aProfilePath);

mozilla::ipc::IPCResult
RecvPBackgroundStorageConstructor(PBackgroundStorageParent* aActor,
                                  const nsString& aProfilePath);

bool
DeallocPBackgroundStorageParent(PBackgroundStorageParent* aActor);

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StorageIPC_h
