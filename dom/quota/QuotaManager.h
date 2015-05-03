/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#define QUOTA_MANAGER_CONTRACTID "@mozilla.org/dom/quota/manager;1"

class nsIOfflineStorage;
class nsIPrincipal;
class nsIThread;
class nsITimer;
class nsIURI;
class nsPIDOMWindow;
class nsIRunnable;

namespace mozilla {
namespace dom {
class ContentParent;
}
}

BEGIN_QUOTA_NAMESPACE

class AsyncUsageRunnable;
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

struct OriginParams
{
  OriginParams(PersistenceType aPersistenceType,
               const nsACString& aOrigin,
               bool aIsApp)
  : mOrigin(aOrigin)
  , mPersistenceType(aPersistenceType)
  , mIsApp(aIsApp)
  { }

  nsCString mOrigin;
  PersistenceType mPersistenceType;
  bool mIsApp;
};

class QuotaManager final : public nsIQuotaManager,
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

  typedef mozilla::dom::ContentParent ContentParent;

  enum MozBrowserPatternFlag
  {
    MozBrowser = 0,
    NotMozBrowser,
    IgnoreMozBrowser
  };

  typedef nsClassHashtable<nsCStringHashKey,
                           nsTArray<nsIOfflineStorage*>> LiveStorageTable;

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
                     bool aIsApp,
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
  RemoveQuotaForOrigin(PersistenceType aPersistenceType,
                       const nsACString& aGroup,
                       const nsACString& aOrigin)
  {
    MutexAutoLock lock(mQuotaMutex);
    LockedRemoveQuotaForOrigin(aPersistenceType, aGroup, aOrigin);
  }

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

  // Called when a storage is created.
  bool
  RegisterStorage(nsIOfflineStorage* aStorage);

  // Called when a storage is being unlinked or destroyed.
  void
  UnregisterStorage(nsIOfflineStorage* aStorage);

  // Called when a process is being shot down. Forces any live storage objects
  // to close themselves and aborts any running transactions.
  void
  AbortCloseStoragesForProcess(ContentParent* aContentParent);

  // Waits for storages to be cleared and for version change transactions to
  // complete before dispatching the given runnable.
  nsresult
  WaitForOpenAllowed(const OriginOrPatternString& aOriginOrPattern,
                     Nullable<PersistenceType> aPersistenceType,
                     const nsACString& aId, nsIRunnable* aRunnable);

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
                            bool aIsApp,
                            nsIFile** aDirectory);

  void
  OriginClearCompleted(PersistenceType aPersistenceType,
                       const nsACString& aOrigin,
                       bool aIsApp);

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
  GetStoragePath() const
  {
    return mStoragePath;
  }

  const nsString&
  GetStoragePath(PersistenceType aPersistenceType) const
  {
    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      return mPermanentStoragePath;
    }

    if (aPersistenceType == PERSISTENCE_TYPE_TEMPORARY) {
      return mTemporaryStoragePath;
    }

    MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT);

    return mDefaultStoragePath;
  }

  uint64_t
  GetGroupLimit() const;

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
                 nsACString* aOrigin,
                 bool* aIsApp);

  static nsresult
  GetInfoFromPrincipal(nsIPrincipal* aPrincipal,
                       nsACString* aGroup,
                       nsACString* aOrigin,
                       bool* aIsApp);

  static nsresult
  GetInfoFromWindow(nsPIDOMWindow* aWindow,
                    nsACString* aGroup,
                    nsACString* aOrigin,
                    bool* aIsApp);

  static void
  GetInfoForChrome(nsACString* aGroup,
                   nsACString* aOrigin,
                   bool* aIsApp);

  static bool
  IsOriginWhitelistedForPersistentStorage(const nsACString& aOrigin);

  static bool
  IsFirstPromptRequired(PersistenceType aPersistenceType,
                        const nsACString& aOrigin,
                        bool aIsApp);

  static bool
  IsQuotaEnforced(PersistenceType aPersistenceType,
                  const nsACString& aOrigin,
                  bool aIsApp);

  static void
  ChromeOrigin(nsACString& aOrigin);

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
                         nsIRunnable* aRunnable);

  void
  AddSynchronizedOp(const OriginOrPatternString& aOriginOrPattern,
                    Nullable<PersistenceType> aPersistenceType);

  SynchronizedOp*
  FindSynchronizedOp(const nsACString& aPattern,
                     Nullable<PersistenceType> aPersistenceType,
                     const nsACString& aId);

  nsresult
  MaybeUpgradeIndexedDBDirectory();

  nsresult
  MaybeUpgradePersistentStorageDirectory();

  nsresult
  MaybeUpgradeStorageArea();

  nsresult
  InitializeRepository(PersistenceType aPersistenceType);

  nsresult
  InitializeOrigin(PersistenceType aPersistenceType,
                   const nsACString& aGroup,
                   const nsACString& aOrigin,
                   bool aIsApp,
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
  DeleteFilesForOrigin(PersistenceType aPersistenceType,
                       const nsACString& aOrigin);

  void
  FinalizeOriginEviction(nsTArray<OriginParams>& aOrigins);

  void
  SaveOriginAccessTime(PersistenceType aPersistenceType,
                       const nsACString& aOrigin,
                       int64_t aTimestamp);

  void
  ReleaseIOThreadObjects()
  {
    AssertIsOnIOThread();

    for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
      mClients[index]->ReleaseIOThreadObjects();
    }
  }

  LiveStorageTable&
  GetLiveStorageTable(PersistenceType aPersistenceType);

  static void
  GetOriginPatternString(uint32_t aAppId,
                         MozBrowserPatternFlag aBrowserFlag,
                         const nsACString& aOrigin,
                         nsAutoCString& _retval);

  static PLDHashOperator
  RemoveQuotaCallback(const nsACString& aKey,
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
  AddLiveStorageOrigins(const nsACString& aKey,
                        nsTArray<nsIOfflineStorage*>* aValue,
                        void* aUserArg);

  static PLDHashOperator
  GetInactiveTemporaryStorageOrigins(const nsACString& aKey,
                                     GroupInfoPair* aValue,
                                     void* aUserArg);

  mozilla::Mutex mQuotaMutex;

  nsClassHashtable<nsCStringHashKey, GroupInfoPair> mGroupInfoPairs;

  // Maintains a list of live storages per origin.
  nsClassHashtable<nsCStringHashKey,
                   ArrayCluster<nsIOfflineStorage*> > mLiveStorages;

  LiveStorageTable mTemporaryLiveStorageTable;
  LiveStorageTable mDefaultLiveStorageTable;

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
  nsString mStoragePath;
  nsString mPermanentStoragePath;
  nsString mTemporaryStoragePath;
  nsString mDefaultStoragePath;

  uint64_t mTemporaryStorageLimit;
  uint64_t mTemporaryStorageUsage;
  bool mTemporaryStorageInitialized;

  bool mStorageAreaInitialized;
};

END_QUOTA_NAMESPACE

#endif /* mozilla_dom_quota_quotamanager_h__ */
