/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorageManager_h
#define mozilla_dom_SessionStorageManager_h

#include "StorageObserver.h"

#include "mozilla/dom/FlippedOnce.h"
#include "nsIDOMStorageManager.h"
#include "nsClassHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"

#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"

class nsIPrincipal;
class nsITimer;

namespace mozilla {
class OriginAttributesPattern;

namespace dom {

class SSCacheCopy;

bool RecvShutdownBackgroundSessionStorageManagers();
void RecvPropagateBackgroundSessionStorageManager(uint64_t aCurrentTopContextId,
                                                  uint64_t aTargetTopContextId);
bool RecvRemoveBackgroundSessionStorageManager(uint64_t aTopContextId);

bool RecvGetSessionStorageData(
    uint64_t aTopContextId, uint32_t aSizeLimit, bool aCancelSessionStoreTimer,
    ::mozilla::ipc::PBackgroundParent::GetSessionStorageManagerDataResolver&&
        aResolver);

bool RecvLoadSessionStorageData(
    uint64_t aTopContextId,
    nsTArray<mozilla::dom::SSCacheCopy>&& aCacheCopyList);

bool RecvClearStoragesForOrigin(const nsACString& aOriginAttrs,
                                const nsACString& aOriginKey);

class BrowsingContext;
class ContentParent;
class SSSetItemInfo;
class SSWriteInfo;
class SessionStorageCache;
class SessionStorageCacheChild;
class SessionStorageManagerChild;
class SessionStorageManagerParent;
class SessionStorageObserver;
struct OriginRecord;

// sessionStorage is a data store that's unique to each tab (i.e. top-level
// browsing context) and origin. Before the advent of Fission all the data
// for a given tab could be stored in a single content process; now each
// site-specific process stores only its portion of the data. As content
// processes terminate, their sessionStorage data needs to be saved in the
// parent process, in case the same origin appears again in the tab (e.g.
// by navigating an iframe element). Therefore SessionStorageManager
// objects exist in both the parent and content processes.
//
// Whenever a write operation for SessionStorage executes, the content process
// sends the changes to the parent process at the next stable state. Whenever a
// content process navigates to an origin for the first time in a given tab, the
// parent process sends it the saved data. SessionStorageCache has a flag
// (mLoadedOrCloned) to ensure that it's only be loaded or cloned once.
//
// Note: the current implementation is expected to be replaced by a new
// implementation using LSNG.
class SessionStorageManagerBase {
 public:
  SessionStorageManagerBase() = default;

 protected:
  ~SessionStorageManagerBase() = default;

  struct OriginRecord {
    OriginRecord() = default;
    OriginRecord(OriginRecord&&) = default;
    OriginRecord& operator=(OriginRecord&&) = default;
    ~OriginRecord();

    RefPtr<SessionStorageCache> mCache;

    // A flag to ensure that cache is only loaded once.
    FlippedOnce<false> mLoaded;
  };

  void ClearStoragesInternal(const OriginAttributesPattern& aPattern,
                             const nsACString& aOriginScope);

  void ClearStoragesForOriginInternal(const nsACString& aOriginAttrs,
                                      const nsACString& aOriginKey);

  OriginRecord* GetOriginRecord(const nsACString& aOriginAttrs,
                                const nsACString& aOriginKey,
                                bool aMakeIfNeeded,
                                SessionStorageCache* aCloneFrom);

  using OriginKeyHashTable = nsClassHashtable<nsCStringHashKey, OriginRecord>;
  nsClassHashtable<nsCStringHashKey, OriginKeyHashTable> mOATable;
};

class SessionStorageManager final : public SessionStorageManagerBase,
                                    public nsIDOMSessionStorageManager,
                                    public StorageObserverSink {
 public:
  explicit SessionStorageManager(RefPtr<BrowsingContext> aBrowsingContext);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEMANAGER
  NS_DECL_NSIDOMSESSIONSTORAGEMANAGER

  NS_DECL_CYCLE_COLLECTION_CLASS(SessionStorageManager)

  bool CanLoadData();

  void SetActor(SessionStorageManagerChild* aActor);

  bool ActorExists() const;

  void ClearActor();

  nsresult EnsureManager();

  nsresult LoadData(nsIPrincipal& aPrincipal, SessionStorageCache& aCache);

  void CheckpointData(nsIPrincipal& aPrincipal, SessionStorageCache& aCache);

  nsresult ClearStoragesForOrigin(const nsACString& aOriginAttrs,
                                  const nsACString& aOriginKey);

 private:
  ~SessionStorageManager();

  // StorageObserverSink, handler to various chrome clearing notification
  nsresult Observe(const char* aTopic,
                   const nsAString& aOriginAttributesPattern,
                   const nsACString& aOriginScope) override;

  nsresult GetSessionStorageCacheHelper(nsIPrincipal* aPrincipal,
                                        bool aMakeIfNeeded,
                                        SessionStorageCache* aCloneFrom,
                                        RefPtr<SessionStorageCache>* aRetVal);

  nsresult GetSessionStorageCacheHelper(const nsACString& aOriginAttrs,
                                        const nsACString& aOriginKey,
                                        bool aMakeIfNeeded,
                                        SessionStorageCache* aCloneFrom,
                                        RefPtr<SessionStorageCache>* aRetVal);

  void ClearStorages(const OriginAttributesPattern& aPattern,
                     const nsACString& aOriginScope);

  SessionStorageCacheChild* EnsureCache(nsIPrincipal& aPrincipal,
                                        const nsACString& aOriginKey,
                                        SessionStorageCache& aCache);

  void CheckpointDataInternal(nsIPrincipal& aPrincipal,
                              const nsACString& aOriginKey,
                              SessionStorageCache& aCache);

  RefPtr<SessionStorageObserver> mObserver;

  RefPtr<BrowsingContext> mBrowsingContext;

  SessionStorageManagerChild* mActor;
};

/**
 * A specialized SessionStorageManager class that lives on the parent process
 * background thread. It is a shadow copy of SessionStorageManager and it's used
 * to preserve SessionStorageCaches for the other SessionStorageManagers.
 */
class BackgroundSessionStorageManager final : public SessionStorageManagerBase {
 public:
  // Parent process getter function.
  static BackgroundSessionStorageManager* GetOrCreate(uint64_t aTopContextId);

  NS_INLINE_DECL_REFCOUNTING(BackgroundSessionStorageManager);

  // Only called by CanonicalBrowsingContext::ReplaceBy on the parent process.
  static void PropagateManager(uint64_t aCurrentTopContextId,
                               uint64_t aTargetTopContextId);

  // Only called by CanonicalBrowsingContext::CanonicalDiscard on parent
  // process.
  static void RemoveManager(uint64_t aTopContextId);

  static void LoadData(
      uint64_t aTopContextId,
      const nsTArray<mozilla::dom::SSCacheCopy>& aCacheCopyList);

  using DataPromise =
      ::mozilla::ipc::PBackgroundChild::GetSessionStorageManagerDataPromise;
  static RefPtr<DataPromise> GetData(BrowsingContext* aContext,
                                     uint32_t aSizeLimit,
                                     bool aClearSessionStoreTimer = false);

  void GetData(uint32_t aSizeLimit, nsTArray<SSCacheCopy>& aCacheCopyList);

  void CopyDataToContentProcess(const nsACString& aOriginAttrs,
                                const nsACString& aOriginKey,
                                nsTArray<SSSetItemInfo>& aData);

  void UpdateData(const nsACString& aOriginAttrs, const nsACString& aOriginKey,
                  const nsTArray<SSWriteInfo>& aWriteInfos);

  void UpdateData(const nsACString& aOriginAttrs, const nsACString& aOriginKey,
                  const nsTArray<SSSetItemInfo>& aData);

  void ClearStorages(const OriginAttributesPattern& aPattern,
                     const nsACString& aOriginScope);

  void ClearStoragesForOrigin(const nsACString& aOriginAttrs,
                              const nsACString& aOriginKey);

  void SetCurrentBrowsingContextId(uint64_t aBrowsingContextId);

  void MaybeDispatchSessionStoreUpdate();

  void CancelSessionStoreUpdate();

  void AddParticipatingActor(SessionStorageManagerParent* aActor);

  void RemoveParticipatingActor(SessionStorageManagerParent* aActor);

 private:
  // Only be called by GetOrCreate() on the parent process.
  explicit BackgroundSessionStorageManager(uint64_t aBrowsingContextId);

  ~BackgroundSessionStorageManager();

  // Sets a timer for notifying main thread that the cache has been
  // updated. May do nothing if we're coalescing notifications.
  void MaybeScheduleSessionStoreUpdate();

  void DispatchSessionStoreUpdate();

  // The most current browsing context using this manager
  uint64_t mCurrentBrowsingContextId;

  // Callback for notifying main thread of calls to `UpdateData`.
  //
  // A timer that is held whenever this manager has dirty state that
  // has not yet been reflected to the main thread. The timer is used
  // to delay notifying the main thread to ask for changes, thereby
  // coalescing/throttling changes. (Note that SessionStorage, like
  // LocalStorage, treats attempts to set a value to its current value
  // as a no-op.)
  //

  // The timer is initialized with a fixed delay as soon as the state
  // becomes dirty; additional mutations to our state will not reset
  // the timer because then we might never flush to the main
  // thread. The timer is cleared only when a new set of data is sent
  // to the main thread and therefore this manager no longer has any
  // dirty state. This means that there is a period of time after the
  // nsITimer fires where this value is non-null but there is no
  // scheduled timer while we wait for the main thread to request the
  // new state. Callers of GetData can also optionally cancel the
  // current timer to reduce the amounts of notifications.
  //
  // When this manager is moved to a new top-level browsing context id
  // via a PropagateBackgroundSessionStorageManager message, the
  // behavior of the timer doesn't change because the main thread knows
  // about the renaming and is initiating it (and any in-flight
  // GetSessionStorageManagerData requests will be unaffected because
  // they use async-returns so the response is inherently matched up via
  // the issued promise).
  nsCOMPtr<nsITimer> mSessionStoreCallbackTimer;

  nsTArray<RefPtr<SessionStorageManagerParent>> mParticipatingActors;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStorageManager_h
