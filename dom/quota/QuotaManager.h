/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotamanager_h__
#define mozilla_dom_quota_quotamanager_h__

#include <cstdint>
#include <utility>
#include "Client.h"
#include "ErrorList.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/quota/InitializationTypes.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaInfo.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsDebug.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTStringRepr.h"
#include "nscore.h"
#include "prenv.h"

#define QUOTA_MANAGER_CONTRACTID "@mozilla.org/dom/quota/manager;1"

class mozIStorageConnection;
class nsIEventTarget;
class nsIFile;
class nsIPrincipal;
class nsIRunnable;
class nsIThread;
class nsITimer;
class nsPIDOMWindowOuter;

namespace mozilla {

class OriginAttributes;

namespace ipc {

class PrincipalInfo;

}  // namespace ipc

}  // namespace mozilla

namespace mozilla::dom::quota {

class ClientUsageArray;
class DirectoryLockImpl;
class GroupInfo;
class GroupInfoPair;
class OriginInfo;
class OriginScope;
class QuotaObject;

class NS_NO_VTABLE RefCountedObject {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
};

class DirectoryLock : public RefCountedObject {
  friend class DirectoryLockImpl;

 public:
  int64_t Id() const;

  // 'Get' prefix is to avoid name collisions with the enum
  PersistenceType GetPersistenceType() const;

  quota::GroupAndOrigin GroupAndOrigin() const;

  const nsACString& Origin() const;

  Client::Type ClientType() const;

  already_AddRefed<DirectoryLock> Specialize(
      PersistenceType aPersistenceType,
      const quota::GroupAndOrigin& aGroupAndOrigin,
      Client::Type aClientType) const;

  void Log() const;

 private:
  DirectoryLock() = default;

  ~DirectoryLock() = default;
};

class NS_NO_VTABLE OpenDirectoryListener : public RefCountedObject {
 public:
  virtual void DirectoryLockAcquired(DirectoryLock* aLock) = 0;

  virtual void DirectoryLockFailed() = 0;

 protected:
  virtual ~OpenDirectoryListener() = default;
};

struct OriginParams {
  OriginParams(PersistenceType aPersistenceType, const nsACString& aOrigin)
      : mOrigin(aOrigin), mPersistenceType(aPersistenceType) {}

  nsCString mOrigin;
  PersistenceType mPersistenceType;
};

class QuotaManager final : public BackgroundThreadObject {
  friend class DirectoryLockImpl;
  friend class GroupInfo;
  friend class OriginInfo;
  friend class QuotaObject;

  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;
  typedef nsClassHashtable<nsCStringHashKey, nsTArray<DirectoryLockImpl*>>
      DirectoryLockTable;

  class Observer;

 public:
  NS_INLINE_DECL_REFCOUNTING(QuotaManager)

  static nsresult Initialize();

  static bool IsRunningXPCShellTests() {
    static bool kRunningXPCShellTests =
        !!PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR");
    return kRunningXPCShellTests;
  }

  static bool IsRunningGTests() {
    static bool kRunningGTests = !!PR_GetEnv("MOZ_RUN_GTEST");
    return kRunningGTests;
  }

  static const char kReplaceChars[];

  static void GetOrCreate(nsIRunnable* aCallback,
                          nsIEventTarget* aMainEventTarget = nullptr);

  // Returns a non-owning reference.
  static QuotaManager* Get();

  static QuotaManager& GetRef();

  // Returns true if we've begun the shutdown process.
  static bool IsShuttingDown();

  static void ShutdownInstance();

  static bool IsOSMetadata(const nsAString& aFileName);

  static bool IsDotFile(const nsAString& aFileName);

  bool IsOriginInitialized(const nsACString& aOrigin) const {
    AssertIsOnIOThread();

    return mInitializedOrigins.Contains(aOrigin);
  }

  bool IsTemporaryStorageInitialized() const {
    AssertIsOnIOThread();

    return mTemporaryStorageInitialized;
  }

  /**
   * For initialization of an origin where the directory already exists. This is
   * used by EnsureTemporaryStorageIsInitialized/InitializeRepository once it
   * has tallied origin usage by calling each of the QuotaClient InitOrigin
   * methods.
   */
  void InitQuotaForOrigin(PersistenceType aPersistenceType,
                          const GroupAndOrigin& aGroupAndOrigin,
                          const ClientUsageArray& aClientUsages,
                          uint64_t aUsageBytes, int64_t aAccessTime,
                          bool aPersisted);

  /**
   * For use in special-cases like LSNG where we need to be able to know that
   * there is no data stored for an origin. LSNG knows that there is 0 usage for
   * its storage of an origin and wants to make sure there is a QuotaObject
   * tracking this. This method will create a non-persisted, 0-usage,
   * mDirectoryExists=false OriginInfo if there isn't already an OriginInfo. If
   * an OriginInfo already exists, it will be left as-is, because that implies a
   * different client has usages for the origin (and there's no need to add
   * LSNG's 0 usage to the QuotaObject).
   */
  void EnsureQuotaForOrigin(PersistenceType aPersistenceType,
                            const GroupAndOrigin& aGroupAndOrigin);

  /**
   * For use when creating an origin directory. It's possible that origin usage
   * is already being tracked due to a call to EnsureQuotaForOrigin, and in that
   * case we need to update the existing OriginInfo rather than create a new
   * one.
   */
  void NoteOriginDirectoryCreated(PersistenceType aPersistenceType,
                                  const GroupAndOrigin& aGroupAndOrigin,
                                  bool aPersisted, int64_t& aTimestamp);

  // XXX clients can use QuotaObject instead of calling this method directly.
  void DecreaseUsageForOrigin(PersistenceType aPersistenceType,
                              const GroupAndOrigin& aGroupAndOrigin,
                              Client::Type aClientType, int64_t aSize);

  void ResetUsageForClient(PersistenceType aPersistenceType,
                           const GroupAndOrigin& aGroupAndOrigin,
                           Client::Type aClientType);

  UsageInfo GetUsageForClient(PersistenceType aPersistenceType,
                              const GroupAndOrigin& aGroupAndOrigin,
                              Client::Type aClientType);

  void UpdateOriginAccessTime(PersistenceType aPersistenceType,
                              const GroupAndOrigin& aGroupAndOrigin);

  void RemoveQuota();

  void RemoveQuotaForOrigin(PersistenceType aPersistenceType,
                            const GroupAndOrigin& aGroupAndOrigin) {
    MutexAutoLock lock(mQuotaMutex);
    LockedRemoveQuotaForOrigin(aPersistenceType, aGroupAndOrigin);
  }

  nsresult LoadQuota();

  void UnloadQuota();

  already_AddRefed<QuotaObject> GetQuotaObject(
      PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
      Client::Type aClientType, nsIFile* aFile, int64_t aFileSize = -1,
      int64_t* aFileSizeOut = nullptr);

  already_AddRefed<QuotaObject> GetQuotaObject(
      PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
      Client::Type aClientType, const nsAString& aPath, int64_t aFileSize = -1,
      int64_t* aFileSizeOut = nullptr);

  already_AddRefed<QuotaObject> GetQuotaObject(const int64_t aDirectoryLockId,
                                               const nsAString& aPath);

  Nullable<bool> OriginPersisted(const GroupAndOrigin& aGroupAndOrigin);

  void PersistOrigin(const GroupAndOrigin& aGroupAndOrigin);

  // Called when a process is being shot down. Aborts any running operations
  // for the given process.
  void AbortOperationsForProcess(ContentParentId aContentParentId);

  Result<nsCOMPtr<nsIFile>, nsresult> GetDirectoryForOrigin(
      PersistenceType aPersistenceType, const nsACString& aASCIIOrigin) const;

  nsresult RestoreDirectoryMetadata2(nsIFile* aDirectory, bool aPersistent);

  nsresult GetDirectoryMetadata2(nsIFile* aDirectory, int64_t* aTimestamp,
                                 bool* aPersisted, QuotaInfo& aQuotaInfo);

  nsresult GetDirectoryMetadata2WithRestore(
      nsIFile* aDirectory, bool aPersistent, int64_t* aTimestamp,
      bool* aPersisted, QuotaInfo& aQuotaInfo, const bool aTelemetry = false);

  nsresult GetDirectoryMetadata2(nsIFile* aDirectory, int64_t* aTimestamp,
                                 bool* aPersisted);

  nsresult GetDirectoryMetadata2WithRestore(nsIFile* aDirectory,
                                            bool aPersistent,
                                            int64_t* aTimestamp,
                                            bool* aPersisted);

  // This is the main entry point into the QuotaManager API.
  // Any storage API implementation (quota client) that participates in
  // centralized quota and storage handling should call this method to get
  // a directory lock which will protect client's files from being deleted
  // while they are still in use.
  // After a lock is acquired, client is notified via the open listener's
  // method DirectoryLockAcquired. If the lock couldn't be acquired, client
  // gets DirectoryLockFailed notification.
  // A lock is a reference counted object and at the time DirectoryLockAcquired
  // is called, quota manager holds just one strong reference to it which is
  // then immediatelly cleared by quota manager. So it's up to client to add
  // a new reference in order to keep the lock alive.
  // Unlocking is simply done by dropping all references to the lock object.
  // In other words, protection which the lock represents dies with the lock
  // object itself.
  already_AddRefed<DirectoryLock> OpenDirectory(
      PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
      Client::Type aClientType, bool aExclusive,
      RefPtr<OpenDirectoryListener> aOpenListener);

  // XXX RemoveMe once bug 1170279 gets fixed.
  already_AddRefed<DirectoryLock> OpenDirectoryInternal(
      const Nullable<PersistenceType>& aPersistenceType,
      const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType, bool aExclusive,
      OpenDirectoryListener* aOpenListener);

  // Collect inactive and the least recently used origins.
  uint64_t CollectOriginsForEviction(
      uint64_t aMinSizeToBeFreed, nsTArray<RefPtr<DirectoryLockImpl>>& aLocks);

  /**
   * Helper method to invoke the provided predicate on all "pending" OriginInfo
   * instances. These are origins for which the origin directory has not yet
   * been created but for which quota is already being tracked. This happens,
   * for example, for the LocalStorage client where an origin that previously
   * was not using LocalStorage can start issuing writes which it buffers until
   * eventually flushing them. We defer creating the origin directory for as
   * long as possible in that case, so the directory won't exist. Logic that
   * would otherwise only consult the filesystem also needs to use this method.
   */
  template <typename P>
  void CollectPendingOriginsForListing(P aPredicate);

  bool IsStorageInitialized() const {
    AssertIsOnIOThread();
    return static_cast<bool>(mStorageConnection);
  }

  void AssertStorageIsInitialized() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  nsresult EnsureStorageIsInitialized();

  // Returns a pair of an nsIFile object referring to the directory, and a bool
  // indicating whether the directory was newly created.
  Result<std::pair<nsCOMPtr<nsIFile>, bool>, nsresult>
  EnsurePersistentOriginIsInitialized(const QuotaInfo& aQuotaInfo);

  // Returns a pair of an nsIFile object referring to the directory, and a bool
  // indicating whether the directory was newly created.
  Result<std::pair<nsCOMPtr<nsIFile>, bool>, nsresult>
  EnsureTemporaryOriginIsInitialized(PersistenceType aPersistenceType,
                                     const QuotaInfo& aQuotaInfo);

  nsresult EnsureTemporaryStorageIsInitialized();

  void ShutdownStorage();

  // Returns a bool indicating whether the directory was newly created.
  Result<bool, nsresult> EnsureOriginDirectory(nsIFile& aDirectory);

  nsresult AboutToClearOrigins(
      const Nullable<PersistenceType>& aPersistenceType,
      const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType);

  void OriginClearCompleted(PersistenceType aPersistenceType,
                            const nsACString& aOrigin,
                            const Nullable<Client::Type>& aClientType);

  void StartIdleMaintenance() {
    AssertIsOnOwningThread();

    for (auto& client : mClients) {
      client->StartIdleMaintenance();
    }
  }

  void StopIdleMaintenance() {
    AssertIsOnOwningThread();

    for (auto& client : mClients) {
      client->StopIdleMaintenance();
    }
  }

  void AssertCurrentThreadOwnsQuotaMutex() {
    mQuotaMutex.AssertCurrentThreadOwns();
  }

  nsIThread* IOThread() {
    NS_ASSERTION(mIOThread, "This should never be null!");
    return mIOThread;
  }

  Client* GetClient(Client::Type aClientType);

  const AutoTArray<Client::Type, Client::TYPE_MAX>& AllClientTypes();

  const nsString& GetBasePath() const { return mBasePath; }

  const nsString& GetStorageName() const { return mStorageName; }

  const nsString& GetStoragePath() const { return mStoragePath; }

  const nsString& GetStoragePath(PersistenceType aPersistenceType) const {
    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      return mPermanentStoragePath;
    }

    if (aPersistenceType == PERSISTENCE_TYPE_TEMPORARY) {
      return mTemporaryStoragePath;
    }

    MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT);

    return mDefaultStoragePath;
  }

  uint64_t GetGroupLimit() const;

  uint64_t GetGroupUsage(const nsACString& aGroup);

  uint64_t GetOriginUsage(const GroupAndOrigin& aGroupAndOrigin);

  void NotifyStoragePressure(uint64_t aUsage);

  void MaybeRecordShutdownStep(Client::Type aClientType,
                               const nsACString& aStepDescription);

  static void GetStorageId(PersistenceType aPersistenceType,
                           const nsACString& aOrigin, Client::Type aClientType,
                           nsACString& aDatabaseId);

  static bool IsPrincipalInfoValid(const PrincipalInfo& aPrincipalInfo);

  static QuotaInfo GetInfoFromValidatedPrincipalInfo(
      const PrincipalInfo& aPrincipalInfo);

  static nsAutoCString GetOriginFromValidatedPrincipalInfo(
      const PrincipalInfo& aPrincipalInfo);

  static Result<QuotaInfo, nsresult> GetInfoFromPrincipal(
      nsIPrincipal* aPrincipal);

  static Result<nsAutoCString, nsresult> GetOriginFromPrincipal(
      nsIPrincipal* aPrincipal);

  static Result<nsAutoCString, nsresult> GetOriginFromWindow(
      nsPIDOMWindowOuter* aWindow);

  static nsLiteralCString GetOriginForChrome();

  static QuotaInfo GetInfoForChrome();

  static bool IsOriginInternal(const nsACString& aOrigin);

  static bool AreOriginsEqualOnDisk(const nsACString& aOrigin1,
                                    const nsACString& aOrigin2);

  static bool ParseOrigin(const nsACString& aOrigin, nsCString& aSpec,
                          OriginAttributes* aAttrs);

  static void InvalidateQuotaCache();

 private:
  QuotaManager(const nsAString& aBasePath, const nsAString& aStorageName);

  virtual ~QuotaManager();

  nsresult Init();

  void Shutdown();

  already_AddRefed<DirectoryLockImpl> CreateDirectoryLock(
      const Nullable<PersistenceType>& aPersistenceType,
      const nsACString& aGroup, const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType, bool aExclusive,
      bool aInternal, RefPtr<OpenDirectoryListener> aOpenListener,
      bool& aBlockedOut);

  already_AddRefed<DirectoryLockImpl> CreateDirectoryLockForEviction(
      PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin);

  void RegisterDirectoryLock(DirectoryLockImpl& aLock);

  void UnregisterDirectoryLock(DirectoryLockImpl& aLock);

  void RemovePendingDirectoryLock(DirectoryLockImpl& aLock);

  uint64_t LockedCollectOriginsForEviction(
      uint64_t aMinSizeToBeFreed, nsTArray<RefPtr<DirectoryLockImpl>>& aLocks);

  void LockedRemoveQuotaForOrigin(PersistenceType aPersistenceType,
                                  const GroupAndOrigin& aGroupAndOrigin);

  already_AddRefed<GroupInfo> LockedGetOrCreateGroupInfo(
      PersistenceType aPersistenceType, const nsACString& aGroup);

  already_AddRefed<OriginInfo> LockedGetOriginInfo(
      PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin);

  nsresult UpgradeFromIndexedDBDirectoryToPersistentStorageDirectory(
      nsIFile* aIndexedDBDir);

  nsresult UpgradeFromPersistentStorageDirectoryToDefaultStorageDirectory(
      nsIFile* aPersistentStorageDir);

  template <typename Helper>
  nsresult UpgradeStorage(const int32_t aOldVersion, const int32_t aNewVersion,
                          mozIStorageConnection* aConnection);

  nsresult UpgradeStorageFrom0_0To1_0(mozIStorageConnection* aConnection);

  nsresult UpgradeStorageFrom1_0To2_0(mozIStorageConnection* aConnection);

  nsresult UpgradeStorageFrom2_0To2_1(mozIStorageConnection* aConnection);

  nsresult UpgradeStorageFrom2_1To2_2(mozIStorageConnection* aConnection);

  nsresult UpgradeStorageFrom2_2To2_3(mozIStorageConnection* aConnection);

  nsresult MaybeRemoveLocalStorageData();

  nsresult MaybeRemoveLocalStorageDirectories();

  nsresult CreateLocalStorageArchiveConnectionFromWebAppsStore(
      mozIStorageConnection** aConnection);

  // The second object in the pair is used to signal if the localStorage
  // archive database was newly created or recreated.
  Result<std::pair<nsCOMPtr<mozIStorageConnection>, bool>, nsresult>
  CreateLocalStorageArchiveConnection();

  nsresult RecreateLocalStorageArchive(
      nsCOMPtr<mozIStorageConnection>& aConnection);

  nsresult DowngradeLocalStorageArchive(
      nsCOMPtr<mozIStorageConnection>& aConnection);

  nsresult UpgradeLocalStorageArchiveFromLessThan4To4(
      nsCOMPtr<mozIStorageConnection>& aConnection);

  /*
  nsresult UpgradeLocalStorageArchiveFrom4To5();
  */

  nsresult InitializeRepository(PersistenceType aPersistenceType);

  nsresult InitializeOrigin(PersistenceType aPersistenceType,
                            const GroupAndOrigin& aGroupAndOrigin,
                            int64_t aAccessTime, bool aPersisted,
                            nsIFile* aDirectory);

  void CheckTemporaryStorageLimits();

  void DeleteFilesForOrigin(PersistenceType aPersistenceType,
                            const nsACString& aOrigin);

  void FinalizeOriginEviction(nsTArray<RefPtr<DirectoryLockImpl>>&& aLocks);

  void ReleaseIOThreadObjects() {
    AssertIsOnIOThread();

    for (Client::Type type : AllClientTypes()) {
      mClients[type]->ReleaseIOThreadObjects();
    }
  }

  DirectoryLockTable& GetDirectoryLockTable(PersistenceType aPersistenceType);

  bool IsSanitizedOriginValid(const nsACString& aSanitizedOrigin);

  int64_t GenerateDirectoryLockId();

  // Thread on which IO is performed.
  nsCOMPtr<nsIThread> mIOThread;

  nsCOMPtr<mozIStorageConnection> mStorageConnection;

  // A timer that gets activated at shutdown to ensure we close all storages.
  nsCOMPtr<nsITimer> mShutdownTimer;

  EnumeratedArray<Client::Type, Client::TYPE_MAX, nsCString> mShutdownSteps;
  LazyInitializedOnce<const TimeStamp> mShutdownStartedAt;

  mozilla::Mutex mQuotaMutex;

  nsClassHashtable<nsCStringHashKey, GroupInfoPair> mGroupInfoPairs;

  // Maintains a list of directory locks that are queued.
  nsTArray<RefPtr<DirectoryLockImpl>> mPendingDirectoryLocks;

  // Maintains a list of directory locks that are acquired or queued. It can be
  // accessed on the owning (PBackground) thread only.
  nsTArray<DirectoryLockImpl*> mDirectoryLocks;

  // Only modifed on the owning thread, but read on multiple threads. Therefore
  // all modifications (including those on the owning thread) and all reads off
  // the owning thread must be protected by mQuotaMutex. In other words, only
  // reads on the owning thread don't have to be protected by mQuotaMutex.
  nsDataHashtable<nsUint64HashKey, DirectoryLockImpl*> mDirectoryLockIdTable;

  // Directory lock tables that are used to update origin access time.
  DirectoryLockTable mTemporaryDirectoryLockTable;
  DirectoryLockTable mDefaultDirectoryLockTable;

  // A list of all successfully initialized persistent origins. This list isn't
  // protected by any mutex but it is only ever touched on the IO thread.
  nsTArray<nsCString> mInitializedOrigins;

  // A hash table that is used to cache origin parser results for given
  // sanitized origin strings. This hash table isn't protected by any mutex but
  // it is only ever touched on the IO thread.
  nsDataHashtable<nsCStringHashKey, bool> mValidOrigins;

  struct OriginInitializationInfo {
    bool mPersistentOriginAttempted : 1;
    bool mTemporaryOriginAttempted : 1;
  };

  // A hash table that is currently used to track origin initialization
  // attempts. This hash table isn't protected by any mutex but it is only ever
  // touched on the IO thread.
  nsDataHashtable<nsCStringHashKey, OriginInitializationInfo>
      mOriginInitializationInfos;

  // This array is populated at initialization time and then never modified, so
  // it can be iterated on any thread.
  AutoTArray<RefPtr<Client>, Client::TYPE_MAX> mClients;

  AutoTArray<Client::Type, Client::TYPE_MAX> mAllClientTypes;
  AutoTArray<Client::Type, Client::TYPE_MAX> mAllClientTypesExceptLS;

  InitializationInfo mInitializationInfo;

  nsString mBasePath;
  nsString mStorageName;
  nsString mIndexedDBPath;
  nsString mStoragePath;
  nsString mPermanentStoragePath;
  nsString mTemporaryStoragePath;
  nsString mDefaultStoragePath;

  uint64_t mTemporaryStorageLimit;
  uint64_t mTemporaryStorageUsage;
  int64_t mNextDirectoryLockId;
  bool mTemporaryStorageInitialized;
  bool mCacheUsable;
};

}  // namespace mozilla::dom::quota

#endif /* mozilla_dom_quota_quotamanager_h__ */
