/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotamanager_h__
#define mozilla_dom_quota_quotamanager_h__

#include "QuotaCommon.h"

#include "nsIObserver.h"
#include "nsIQuotaManager.h"

#include "mozilla/dom/Nullable.h"
#include "mozilla/Mutex.h"

#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"

#include "ArrayCluster.h"
#include "Client.h"
#include "PersistenceType.h"
#include "StoragePrivilege.h"

#define QUOTA_MANAGER_CONTRACTID "@mozilla.org/dom/quota/manager;1"

class nsIOfflineStorage;
class nsIPrincipal;
class nsIThread;
class nsITimer;
class nsIURI;
class nsPIDOMWindow;
class nsIRunnable;

BEGIN_QUOTA_NAMESPACE

class AcquireListener;
class AsyncUsageRunnable;
class CheckQuotaHelper;
class CollectOriginsHelper;
class FinalizeOriginEvictionRunnable;
class GroupInfo;
class GroupInfoPair;
class OriginClearRunnable;
class OriginInfo;
class OriginOrPatternString;
class QuotaObject;
class ResetOrClearRunnable;
struct SynchronizedOp;

class QuotaManager MOZ_FINAL : public nsIQuotaManager,
                               public nsIObserver
{
  friend class AsyncUsageRunnable;
  friend class CollectOriginsHelper;
  friend class FinalizeOriginEvictionRunnable;
  friend class GroupInfo;
  friend class OriginClearRunnable;
  friend class OriginInfo;
  friend class QuotaObject;
  friend class ResetOrClearRunnable;

  enum MozBrowserPatternFlag
  {
    MozBrowser = 0,
    NotMozBrowser,
    IgnoreMozBrowser
  };

  typedef void
  (*WaitingOnStoragesCallback)(nsTArray<nsCOMPtr<nsIOfflineStorage> >&, void*);

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUOTAMANAGER
  NS_DECL_NSIOBSERVER

  // Returns a non-owning reference.
  static QuotaManager*
  GetOrCreate();

  // Returns a non-owning reference.
  static QuotaManager*
  Get();

  // Returns an owning reference! No one should call this but the factory.
  static QuotaManager*
  FactoryCreate();

  // Returns true if we've begun the shutdown process.
  static bool IsShuttingDown();

  void
  InitQuotaForOrigin(PersistenceType aPersistenceType,
                     const nsACString& aGroup,
                     const nsACString& aOrigin,
                     uint64_t aLimitBytes,
                     uint64_t aUsageBytes,
                     int64_t aAccessTime);

  void
  DecreaseUsageForOrigin(PersistenceType aPersistenceType,
                         const nsACString& aGroup,
                         const nsACString& aOrigin,
                         int64_t aSize);

  void
  UpdateOriginAccessTime(PersistenceType aPersistenceType,
                         const nsACString& aGroup,
                         const nsACString& aOrigin);

  void
  RemoveQuota();

  void
  RemoveQuotaForPersistenceType(PersistenceType);

  void
  RemoveQuotaForOrigin(PersistenceType aPersistenceType,
                       const nsACString& aGroup,
                       const nsACString& aOrigin)
  {
    MutexAutoLock lock(mQuotaMutex);
    LockedRemoveQuotaForOrigin(aPersistenceType, aGroup, aOrigin);
  }

  void
  RemoveQuotaForPattern(PersistenceType aPersistenceType,
                        const nsACString& aPattern);

  already_AddRefed<QuotaObject>
  GetQuotaObject(PersistenceType aPersistenceType,
                 const nsACString& aGroup,
                 const nsACString& aOrigin,
                 nsIFile* aFile);

  already_AddRefed<QuotaObject>
  GetQuotaObject(PersistenceType aPersistenceType,
                 const nsACString& aGroup,
                 const nsACString& aOrigin,
                 const nsAString& aPath);

  // Set the Window that the current thread is doing operations for.
  // The caller is responsible for ensuring that aWindow is held alive.
  static void
  SetCurrentWindow(nsPIDOMWindow* aWindow)
  {
    QuotaManager* quotaManager = Get();
    NS_ASSERTION(quotaManager, "Must have a manager here!");

    quotaManager->SetCurrentWindowInternal(aWindow);
  }

  static void
  CancelPromptsForWindow(nsPIDOMWindow* aWindow)
  {
    NS_ASSERTION(aWindow, "Passed null window!");

    QuotaManager* quotaManager = Get();
    NS_ASSERTION(quotaManager, "Must have a manager here!");

    quotaManager->CancelPromptsForWindowInternal(aWindow);
  }

  // Called when a storage is created.
  bool
  RegisterStorage(nsIOfflineStorage* aStorage);

  // Called when a storage is being unlinked or destroyed.
  void
  UnregisterStorage(nsIOfflineStorage* aStorage);

  // Called when a storage has been closed.
  void
  OnStorageClosed(nsIOfflineStorage* aStorage);

  // Called when a window is being purged from the bfcache or the user leaves
  // a page which isn't going into the bfcache. Forces any live storage
  // objects to close themselves and aborts any running transactions.
  void
  AbortCloseStoragesForWindow(nsPIDOMWindow* aWindow);

  // Used to check if there are running transactions in a given window.
  bool
  HasOpenTransactions(nsPIDOMWindow* aWindow);

  // Waits for storages to be cleared and for version change transactions to
  // complete before dispatching the given runnable.
  nsresult
  WaitForOpenAllowed(const OriginOrPatternString& aOriginOrPattern,
                     Nullable<PersistenceType> aPersistenceType,
                     const nsACString& aId, nsIRunnable* aRunnable);

  // Acquire exclusive access to the storage given (waits for all others to
  // close).  If storages need to close first, the callback will be invoked
  // with an array of said storages.
  nsresult
  AcquireExclusiveAccess(nsIOfflineStorage* aStorage,
                         const nsACString& aOrigin,
                         Nullable<PersistenceType> aPersistenceType,
                         AcquireListener* aListener,
                         WaitingOnStoragesCallback aCallback,
                         void* aClosure)
  {
    NS_ASSERTION(aStorage, "Need a storage here!");
    return AcquireExclusiveAccess(aOrigin, aPersistenceType, aStorage,
                                  aListener, aCallback, aClosure);
  }

  nsresult
  AcquireExclusiveAccess(const nsACString& aOrigin,
                         Nullable<PersistenceType> aPersistenceType,
                         AcquireListener* aListener,
                         WaitingOnStoragesCallback aCallback,
                         void* aClosure)
  {
    return AcquireExclusiveAccess(aOrigin, aPersistenceType, nullptr,
                                  aListener, aCallback, aClosure);
  }

  void
  AllowNextSynchronizedOp(const OriginOrPatternString& aOriginOrPattern,
                          Nullable<PersistenceType> aPersistenceType,
                          const nsACString& aId);

  bool
  IsClearOriginPending(const nsACString& aPattern,
                       Nullable<PersistenceType> aPersistenceType)
  {
    return !!FindSynchronizedOp(aPattern, aPersistenceType, EmptyCString());
  }

  nsresult
  GetDirectoryForOrigin(PersistenceType aPersistenceType,
                        const nsACString& aASCIIOrigin,
                        nsIFile** aDirectory) const;

  nsresult
  EnsureOriginIsInitialized(PersistenceType aPersistenceType,
                            const nsACString& aGroup,
                            const nsACString& aOrigin,
                            bool aTrackQuota,
                            nsIFile** aDirectory);

  void
  OriginClearCompleted(PersistenceType aPersistenceType,
                       const OriginOrPatternString& aOriginOrPattern);

  void
  ResetOrClearCompleted();

  void
  AssertCurrentThreadOwnsQuotaMutex()
  {
    mQuotaMutex.AssertCurrentThreadOwns();
  }

  nsIThread*
  IOThread()
  {
    NS_ASSERTION(mIOThread, "This should never be null!");
    return mIOThread;
  }

  already_AddRefed<Client>
  GetClient(Client::Type aClientType);

  const nsString&
  GetStoragePath(PersistenceType aPersistenceType) const
  {
    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      return mPersistentStoragePath;
    }

    NS_ASSERTION(aPersistenceType == PERSISTENCE_TYPE_TEMPORARY, "Huh?");

    return mTemporaryStoragePath;
  }

  uint64_t
  GetGroupLimit() const;

  static uint32_t
  GetStorageQuotaMB();

  static void
  GetStorageId(PersistenceType aPersistenceType,
               const nsACString& aOrigin,
               Client::Type aClientType,
               const nsAString& aName,
               nsACString& aDatabaseId);

  static nsresult
  GetInfoFromURI(nsIURI* aURI,
                 uint32_t aAppId,
                 bool aInMozBrowser,
                 nsACString* aGroup,
                 nsACString* aASCIIOrigin,
                 StoragePrivilege* aPrivilege,
                 PersistenceType* aDefaultPersistenceType);

  static nsresult
  GetInfoFromPrincipal(nsIPrincipal* aPrincipal,
                       nsACString* aGroup,
                       nsACString* aASCIIOrigin,
                       StoragePrivilege* aPrivilege,
                       PersistenceType* aDefaultPersistenceType);

  static nsresult
  GetInfoFromWindow(nsPIDOMWindow* aWindow,
                    nsACString* aGroup,
                    nsACString* aASCIIOrigin,
                    StoragePrivilege* aPrivilege,
                    PersistenceType* aDefaultPersistenceType);

  static void
  GetInfoForChrome(nsACString* aGroup,
                   nsACString* aASCIIOrigin,
                   StoragePrivilege* aPrivilege,
                   PersistenceType* aDefaultPersistenceType);

  static void
  GetOriginPatternString(uint32_t aAppId, bool aBrowserOnly,
                         const nsACString& aOrigin, nsAutoCString& _retval)
  {
    return GetOriginPatternString(aAppId,
                                  aBrowserOnly ? MozBrowser : NotMozBrowser,
                                  aOrigin, _retval);
  }

  static void
  GetOriginPatternStringMaybeIgnoreBrowser(uint32_t aAppId, bool aBrowserOnly,
                                           nsAutoCString& _retval)
  {
    return GetOriginPatternString(aAppId,
                                  aBrowserOnly ? MozBrowser : IgnoreMozBrowser,
                                  EmptyCString(), _retval);
  }

private:
  QuotaManager();

  virtual ~QuotaManager();

  nsresult
  Init();

  void
  SetCurrentWindowInternal(nsPIDOMWindow* aWindow);

  void
  CancelPromptsForWindowInternal(nsPIDOMWindow* aWindow);

  // Determine if the quota is lifted for the Window the current thread is
  // using.
  bool
  LockedQuotaIsLifted();

  uint64_t
  LockedCollectOriginsForEviction(uint64_t aMinSizeToBeFreed,
                                  nsTArray<OriginInfo*>& aOriginInfos);

  void
  LockedRemoveQuotaForOrigin(PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin);

  nsresult
  AcquireExclusiveAccess(const nsACString& aOrigin,
                         Nullable<PersistenceType> aPersistenceType,
                         nsIOfflineStorage* aStorage,
                         AcquireListener* aListener,
                         WaitingOnStoragesCallback aCallback,
                         void* aClosure);

  void
  AddSynchronizedOp(const OriginOrPatternString& aOriginOrPattern,
                    Nullable<PersistenceType> aPersistenceType);

  nsresult
  RunSynchronizedOp(nsIOfflineStorage* aStorage,
                    SynchronizedOp* aOp);

  SynchronizedOp*
  FindSynchronizedOp(const nsACString& aPattern,
                     Nullable<PersistenceType> aPersistenceType,
                     const nsACString& aId);

  nsresult
  MaybeUpgradeIndexedDBDirectory();

  nsresult
  InitializeOrigin(PersistenceType aPersistenceType,
                   const nsACString& aGroup,
                   const nsACString& aOrigin,
                   bool aTrackQuota,
                   int64_t aAccessTime,
                   nsIFile* aDirectory);

  nsresult
  ClearStoragesForApp(uint32_t aAppId, bool aBrowserOnly);

  void
  CheckTemporaryStorageLimits();

  // Collect inactive and the least recently used origins.
  uint64_t
  CollectOriginsForEviction(uint64_t aMinSizeToBeFreed,
                            nsTArray<OriginInfo*>& aOriginInfos);

  void
  DeleteTemporaryFilesForOrigin(const nsACString& aOrigin);

  void
  FinalizeOriginEviction(nsTArray<nsCString>& aOrigins);

  void
  SaveOriginAccessTime(const nsACString& aOrigin, int64_t aTimestamp);

  void
  ReleaseIOThreadObjects()
  {
    AssertIsOnIOThread();

    for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
      mClients[index]->ReleaseIOThreadObjects();
    }
  }

  static void
  GetOriginPatternString(uint32_t aAppId,
                         MozBrowserPatternFlag aBrowserFlag,
                         const nsACString& aOrigin,
                         nsAutoCString& _retval);

  static PLDHashOperator
  RemoveQuotaForPersistenceTypeCallback(const nsACString& aKey,
                                        nsAutoPtr<GroupInfoPair>& aValue,
                                        void* aUserArg);

  static PLDHashOperator
  RemoveQuotaCallback(const nsACString& aKey,
                      nsAutoPtr<GroupInfoPair>& aValue,
                      void* aUserArg);

  static PLDHashOperator
  RemoveQuotaForPatternCallback(const nsACString& aKey,
                                nsAutoPtr<GroupInfoPair>& aValue,
                                void* aUserArg);

  static PLDHashOperator
  GetOriginsExceedingGroupLimit(const nsACString& aKey,
                                GroupInfoPair* aValue,
                                void* aUserArg);

  static PLDHashOperator
  GetAllTemporaryStorageOrigins(const nsACString& aKey,
                                GroupInfoPair* aValue,
                                void* aUserArg);

  static PLDHashOperator
  AddTemporaryStorageOrigins(const nsACString& aKey,
                             ArrayCluster<nsIOfflineStorage*>* aValue,
                             void* aUserArg);

  static PLDHashOperator
  GetInactiveTemporaryStorageOrigins(const nsACString& aKey,
                                     GroupInfoPair* aValue,
                                     void* aUserArg);

  // TLS storage index for the current thread's window.
  unsigned int mCurrentWindowIndex;

  mozilla::Mutex mQuotaMutex;

  nsClassHashtable<nsCStringHashKey, GroupInfoPair> mGroupInfoPairs;

  // A map of Windows to the corresponding quota helper.
  nsRefPtrHashtable<nsPtrHashKey<nsPIDOMWindow>,
                    CheckQuotaHelper> mCheckQuotaHelpers;

  // Maintains a list of live storages per origin.
  nsClassHashtable<nsCStringHashKey,
                   ArrayCluster<nsIOfflineStorage*> > mLiveStorages;

  // Maintains a list of synchronized operatons that are in progress or queued.
  nsAutoTArray<nsAutoPtr<SynchronizedOp>, 5> mSynchronizedOps;

  // Thread on which IO is performed.
  nsCOMPtr<nsIThread> mIOThread;

  // A timer that gets activated at shutdown to ensure we close all storages.
  nsCOMPtr<nsITimer> mShutdownTimer;

  // A list of all successfully initialized origins. This list isn't protected
  // by any mutex but it is only ever touched on the IO thread.
  nsTArray<nsCString> mInitializedOrigins;

  nsAutoTArray<nsRefPtr<Client>, Client::TYPE_MAX> mClients;

  nsString mIndexedDBPath;
  nsString mPersistentStoragePath;
  nsString mTemporaryStoragePath;

  uint64_t mTemporaryStorageLimit;
  uint64_t mTemporaryStorageUsage;
  bool mTemporaryStorageInitialized;

  bool mStorageAreaInitialized;
};

class AutoEnterWindow
{
public:
  AutoEnterWindow(nsPIDOMWindow* aWindow)
  {
    QuotaManager::SetCurrentWindow(aWindow);
  }

  ~AutoEnterWindow()
  {
    QuotaManager::SetCurrentWindow(nullptr);
  }
};

END_QUOTA_NAMESPACE

#endif /* mozilla_dom_quota_quotamanager_h__ */
