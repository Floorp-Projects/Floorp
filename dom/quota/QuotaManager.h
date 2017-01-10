/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotamanager_h__
#define mozilla_dom_quota_quotamanager_h__

#include "QuotaCommon.h"

#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/Mutex.h"

#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"

#include "Client.h"
#include "PersistenceType.h"

#include "prenv.h"

#define QUOTA_MANAGER_CONTRACTID "@mozilla.org/dom/quota/manager;1"

class mozIStorageConnection;
class nsIEventTarget;
class nsIPrincipal;
class nsIThread;
class nsITimer;
class nsIURI;
class nsPIDOMWindowOuter;
class nsIRunnable;

BEGIN_QUOTA_NAMESPACE

class DirectoryLockImpl;
class GroupInfo;
class GroupInfoPair;
class OriginInfo;
class OriginScope;
class QuotaObject;

class NS_NO_VTABLE RefCountedObject
{
public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() = 0;

  NS_IMETHOD_(MozExternalRefCountType)
  Release() = 0;
};

class DirectoryLock
  : public RefCountedObject
{
  friend class DirectoryLockImpl;

private:
  DirectoryLock()
  { }

  ~DirectoryLock()
  { }
};

class NS_NO_VTABLE OpenDirectoryListener
  : public RefCountedObject
{
public:
  virtual void
  DirectoryLockAcquired(DirectoryLock* aLock) = 0;

  virtual void
  DirectoryLockFailed() = 0;

protected:
  virtual ~OpenDirectoryListener()
  { }
};

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

class QuotaManager final
  : public BackgroundThreadObject
{
  friend class DirectoryLockImpl;
  friend class GroupInfo;
  friend class OriginInfo;
  friend class QuotaObject;

  typedef nsClassHashtable<nsCStringHashKey,
                           nsTArray<DirectoryLockImpl*>> DirectoryLockTable;

public:
  class CreateRunnable;

private:
  class ShutdownRunnable;
  class ShutdownObserver;

public:
  NS_INLINE_DECL_REFCOUNTING(QuotaManager)

  static bool IsRunningXPCShellTests()
  {
    static bool kRunningXPCShellTests = !!PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR");
    return kRunningXPCShellTests;
  }

  static const char kReplaceChars[];

  static void
  GetOrCreate(nsIRunnable* aCallback);

  // Returns a non-owning reference.
  static QuotaManager*
  Get();

  // Returns true if we've begun the shutdown process.
  static bool IsShuttingDown();

  bool
  IsOriginInitialized(const nsACString& aOrigin) const
  {
    AssertIsOnIOThread();

    return mInitializedOrigins.Contains(aOrigin);
  }

  bool
  IsTemporaryStorageInitialized() const
  {
    AssertIsOnIOThread();

    return mTemporaryStorageInitialized;
  }

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

  // Called when a process is being shot down. Aborts any running operations
  // for the given process.
  void
  AbortOperationsForProcess(ContentParentId aContentParentId);

  nsresult
  GetDirectoryForOrigin(PersistenceType aPersistenceType,
                        const nsACString& aASCIIOrigin,
                        nsIFile** aDirectory) const;

  nsresult
  RestoreDirectoryMetadata2(nsIFile* aDirectory, bool aPersistent);

  nsresult
  GetDirectoryMetadata2(nsIFile* aDirectory,
                        int64_t* aTimestamp,
                        nsACString& aSuffix,
                        nsACString& aGroup,
                        nsACString& aOrigin,
                        bool* aIsApp);

  nsresult
  GetDirectoryMetadata2WithRestore(nsIFile* aDirectory,
                                   bool aPersistent,
                                   int64_t* aTimestamp,
                                   nsACString& aSuffix,
                                   nsACString& aGroup,
                                   nsACString& aOrigin,
                                   bool* aIsApp);

  nsresult
  GetDirectoryMetadata2(nsIFile* aDirectory, int64_t* aTimestamp);

  nsresult
  GetDirectoryMetadata2WithRestore(nsIFile* aDirectory,
                                   bool aPersistent,
                                   int64_t* aTimestamp);

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
  void
  OpenDirectory(PersistenceType aPersistenceType,
                const nsACString& aGroup,
                const nsACString& aOrigin,
                bool aIsApp,
                Client::Type aClientType,
                bool aExclusive,
                OpenDirectoryListener* aOpenListener);

  // XXX RemoveMe once bug 1170279 gets fixed.
  void
  OpenDirectoryInternal(Nullable<PersistenceType> aPersistenceType,
                        const OriginScope& aOriginScope,
                        Nullable<Client::Type> aClientType,
                        bool aExclusive,
                        OpenDirectoryListener* aOpenListener);

  // Collect inactive and the least recently used origins.
  uint64_t
  CollectOriginsForEviction(uint64_t aMinSizeToBeFreed,
                            nsTArray<RefPtr<DirectoryLockImpl>>& aLocks);

  nsresult
  EnsureStorageIsInitialized();

  nsresult
  EnsureOriginIsInitialized(PersistenceType aPersistenceType,
                            const nsACString& aSuffix,
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
  StartIdleMaintenance()
  {
    AssertIsOnOwningThread();

    for (auto& client : mClients) {
      client->StartIdleMaintenance();
    }
  }

  void
  StopIdleMaintenance()
  {
    AssertIsOnOwningThread();

    for (auto& client : mClients) {
      client->StopIdleMaintenance();
    }
  }

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

  Client*
  GetClient(Client::Type aClientType);

  const nsString&
  GetBasePath() const
  {
    return mBasePath;
  }

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

  void
  GetGroupUsageAndLimit(const nsACString& aGroup,
                        UsageInfo* aUsageInfo);

  static void
  GetStorageId(PersistenceType aPersistenceType,
               const nsACString& aOrigin,
               Client::Type aClientType,
               nsACString& aDatabaseId);

  static nsresult
  GetInfoFromPrincipal(nsIPrincipal* aPrincipal,
                       nsACString* aSuffix,
                       nsACString* aGroup,
                       nsACString* aOrigin,
                       bool* aIsApp);

  static nsresult
  GetInfoFromWindow(nsPIDOMWindowOuter* aWindow,
                    nsACString* aSuffix,
                    nsACString* aGroup,
                    nsACString* aOrigin,
                    bool* aIsApp);

  static void
  GetInfoForChrome(nsACString* aSuffix,
                   nsACString* aGroup,
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

private:
  QuotaManager();

  virtual ~QuotaManager();

  nsresult
  Init(const nsAString& aBaseDirPath);

  void
  Shutdown();

  already_AddRefed<DirectoryLockImpl>
  CreateDirectoryLock(Nullable<PersistenceType> aPersistenceType,
                      const nsACString& aGroup,
                      const OriginScope& aOriginScope,
                      Nullable<bool> aIsApp,
                      Nullable<Client::Type> aClientType,
                      bool aExclusive,
                      bool aInternal,
                      OpenDirectoryListener* aOpenListener);

  already_AddRefed<DirectoryLockImpl>
  CreateDirectoryLockForEviction(PersistenceType aPersistenceType,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin,
                                 bool aIsApp);

  void
  RegisterDirectoryLock(DirectoryLockImpl* aLock);

  void
  UnregisterDirectoryLock(DirectoryLockImpl* aLock);

  void
  RemovePendingDirectoryLock(DirectoryLockImpl* aLock);

  uint64_t
  LockedCollectOriginsForEviction(
                                 uint64_t aMinSizeToBeFreed,
                                 nsTArray<RefPtr<DirectoryLockImpl>>& aLocks);

  void
  LockedRemoveQuotaForOrigin(PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin);

  nsresult
  MaybeUpgradeIndexedDBDirectory();

  nsresult
  MaybeUpgradePersistentStorageDirectory();

  nsresult
  MaybeRemoveOldDirectories();

  nsresult
  UpgradeStorageFrom0_0To1_0(mozIStorageConnection* aConnection);

#if 0
  nsresult
  UpgradeStorageFrom1_0To2_0(mozIStorageConnection* aConnection);
#endif

  nsresult
  InitializeRepository(PersistenceType aPersistenceType);

  nsresult
  InitializeOrigin(PersistenceType aPersistenceType,
                   const nsACString& aGroup,
                   const nsACString& aOrigin,
                   bool aIsApp,
                   int64_t aAccessTime,
                   nsIFile* aDirectory);

  void
  CheckTemporaryStorageLimits();

  void
  DeleteFilesForOrigin(PersistenceType aPersistenceType,
                       const nsACString& aOrigin);

  void
  FinalizeOriginEviction(nsTArray<RefPtr<DirectoryLockImpl>>& aLocks);

  void
  ReleaseIOThreadObjects()
  {
    AssertIsOnIOThread();

    for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
      mClients[index]->ReleaseIOThreadObjects();
    }
  }

  DirectoryLockTable&
  GetDirectoryLockTable(PersistenceType aPersistenceType);

  static void
  ShutdownTimerCallback(nsITimer* aTimer, void* aClosure);

  mozilla::Mutex mQuotaMutex;

  nsClassHashtable<nsCStringHashKey, GroupInfoPair> mGroupInfoPairs;

  // Maintains a list of directory locks that are queued.
  nsTArray<RefPtr<DirectoryLockImpl>> mPendingDirectoryLocks;

  // Maintains a list of directory locks that are acquired or queued.
  nsTArray<DirectoryLockImpl*> mDirectoryLocks;

  // Directory lock tables that are used to update origin access time.
  DirectoryLockTable mTemporaryDirectoryLockTable;
  DirectoryLockTable mDefaultDirectoryLockTable;

  // Thread on which IO is performed.
  nsCOMPtr<nsIThread> mIOThread;

  // A timer that gets activated at shutdown to ensure we close all storages.
  nsCOMPtr<nsITimer> mShutdownTimer;

  // A list of all successfully initialized origins. This list isn't protected
  // by any mutex but it is only ever touched on the IO thread.
  nsTArray<nsCString> mInitializedOrigins;

  // This array is populated at initialization time and then never modified, so
  // it can be iterated on any thread.
  AutoTArray<RefPtr<Client>, Client::TYPE_MAX> mClients;

  nsString mBasePath;
  nsString mIndexedDBPath;
  nsString mStoragePath;
  nsString mPermanentStoragePath;
  nsString mTemporaryStoragePath;
  nsString mDefaultStoragePath;

  uint64_t mTemporaryStorageLimit;
  uint64_t mTemporaryStorageUsage;
  bool mTemporaryStorageInitialized;

  bool mStorageInitialized;
};

END_QUOTA_NAMESPACE

#endif /* mozilla_dom_quota_quotamanager_h__ */
