/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozIThirdPartyUtil.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsISimpleEnumerator.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsPrimitives.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"

#include <algorithm>
#include "GeckoProfiler.h"
#include "mozilla/Atomics.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CondVar.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/dom/cache/QuotaClient.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/localstorage/ActorsParent.h"
#include "mozilla/dom/quota/PQuotaParent.h"
#include "mozilla/dom/quota/PQuotaRequestParent.h"
#include "mozilla/dom/quota/PQuotaUsageRequestParent.h"
#include "mozilla/dom/simpledb/ActorsParent.h"
#include "mozilla/dom/StorageActivityService.h"
#include "mozilla/dom/StorageDBUpdater.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/net/MozURL.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Mutex.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Unused.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsAboutProtocolUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceUtils.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsScriptSecurityManager.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prio.h"
#include "xpcpublic.h"

#include "OriginScope.h"
#include "QuotaManager.h"
#include "QuotaManagerService.h"
#include "QuotaObject.h"
#include "UsageInfo.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

#define QM_LOG_TEST() MOZ_LOG_TEST(GetQuotaManagerLogger(), LogLevel::Info)
#define QM_LOG(_args) MOZ_LOG(GetQuotaManagerLogger(), LogLevel::Info, _args)

#define UNKNOWN_FILE_WARNING(_leafName)                              \
  QM_WARNING("Something (%s) in the directory that doesn't belong!", \
             NS_ConvertUTF16toUTF8(_leafName).get())

// The amount of time, in milliseconds, that our IO thread will stay alive
// after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

// The amount of time, in milliseconds, that we will wait for active storage
// transactions on shutdown before aborting them.
#define DEFAULT_SHUTDOWN_TIMER_MS 30000

// Preference that users can set to override temporary storage smart limit
// calculation.
#define PREF_FIXED_LIMIT "dom.quotaManager.temporaryStorage.fixedLimit"
#define PREF_CHUNK_SIZE "dom.quotaManager.temporaryStorage.chunkSize"

// Preference that is used to enable testing features
#define PREF_TESTING_FEATURES "dom.quotaManager.testing"

// profile-before-change, when we need to shut down quota manager
#define PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID "profile-before-change-qm"

#define KB *1024ULL
#define MB *1024ULL KB
#define GB *1024ULL MB

namespace mozilla {
namespace dom {
namespace quota {

using namespace mozilla::ipc;
using mozilla::net::MozURL;

// We want profiles to be platform-independent so we always need to replace
// the same characters on every platform. Windows has the most extensive set
// of illegal characters so we use its FILE_ILLEGAL_CHARACTERS and
// FILE_PATH_SEPARATOR.
const char QuotaManager::kReplaceChars[] = CONTROL_CHARACTERS "/:*?\"<>|\\";

namespace {

/*******************************************************************************
 * Constants
 ******************************************************************************/

const uint32_t kSQLitePageSizeOverride = 512;

const uint32_t kHackyDowngradeMajorStorageVersion = 2;
const uint32_t kHackyDowngradeMinorStorageVersion = 1;

// Important version history:
// - Bug 1290481 bumped our schema from major.minor 2.0 to 3.0 in Firefox 57
//   which caused Firefox 57 release concerns because the major schema upgrade
//   means anyone downgrading to Firefox 56 will experience a non-operational
//   QuotaManager and all of its clients.
// - Bug 1404344 got very concerned about that and so we decided to effectively
//   rename 3.0 to 2.1, effective in Firefox 57.  This works because post
//   storage.sqlite v1.0, QuotaManager doesn't care about minor storage version
//   increases.  It also works because all the upgrade did was give the DOM
//   Cache API QuotaClient an opportunity to create its newly added .padding
//   files during initialization/upgrade, which isn't functionally necessary as
//   that can be done on demand.

// Major storage version. Bump for backwards-incompatible changes.
// (The next major version should be 4 to distinguish from the Bug 1290481
// downgrade snafu.)
const uint32_t kMajorStorageVersion = 2;

// Minor storage version. Bump for backwards-compatible changes.
const uint32_t kMinorStorageVersion = 2;

// The storage version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 16 bits so the max value is
// 0xFFFF. The minor version occupies the lower 16 bits and its max is 0xFFFF.
static_assert(kMajorStorageVersion <= 0xFFFF,
              "Major version needs to fit in 16 bits.");
static_assert(kMinorStorageVersion <= 0xFFFF,
              "Minor version needs to fit in 16 bits.");

const int32_t kStorageVersion =
    int32_t((kMajorStorageVersion << 16) + kMinorStorageVersion);

// See comments above about why these are a thing.
const int32_t kHackyPreDowngradeStorageVersion = int32_t((3 << 16) + 0);
const int32_t kHackyPostDowngradeStorageVersion = int32_t((2 << 16) + 1);

static_assert(static_cast<uint32_t>(StorageType::Persistent) ==
                  static_cast<uint32_t>(PERSISTENCE_TYPE_PERSISTENT),
              "Enum values should match.");

static_assert(static_cast<uint32_t>(StorageType::Temporary) ==
                  static_cast<uint32_t>(PERSISTENCE_TYPE_TEMPORARY),
              "Enum values should match.");

static_assert(static_cast<uint32_t>(StorageType::Default) ==
                  static_cast<uint32_t>(PERSISTENCE_TYPE_DEFAULT),
              "Enum values should match.");

const char kChromeOrigin[] = "chrome";
const char kAboutHomeOriginPrefix[] = "moz-safe-about:home";
const char kIndexedDBOriginPrefix[] = "indexeddb://";
const char kResourceOriginPrefix[] = "resource://";

#define INDEXEDDB_DIRECTORY_NAME "indexedDB"
#define STORAGE_DIRECTORY_NAME "storage"
#define PERSISTENT_DIRECTORY_NAME "persistent"
#define PERMANENT_DIRECTORY_NAME "permanent"
#define TEMPORARY_DIRECTORY_NAME "temporary"
#define DEFAULT_DIRECTORY_NAME "default"

#define STORAGE_FILE_NAME "storage.sqlite"

// The name of the file that we use to load/save the last access time of an
// origin.
// XXX We should get rid of old metadata files at some point, bug 1343576.
#define METADATA_FILE_NAME ".metadata"
#define METADATA_TMP_FILE_NAME ".metadata-tmp"
#define METADATA_V2_FILE_NAME ".metadata-v2"
#define METADATA_V2_TMP_FILE_NAME ".metadata-v2-tmp"

#define WEB_APPS_STORE_FILE_NAME "webappsstore.sqlite"
#define LS_ARCHIVE_FILE_NAME "ls-archive.sqlite"
#define LS_ARCHIVE_TMP_FILE_NAME "ls-archive-tmp.sqlite"

const uint32_t kLocalStorageArchiveVersion = 4;

const char kProfileDoChangeTopic[] = "profile-do-change";

/******************************************************************************
 * SQLite functions
 ******************************************************************************/

int32_t MakeStorageVersion(uint32_t aMajorStorageVersion,
                           uint32_t aMinorStorageVersion) {
  return int32_t((aMajorStorageVersion << 16) + aMinorStorageVersion);
}

uint32_t GetMajorStorageVersion(int32_t aStorageVersion) {
  return uint32_t(aStorageVersion >> 16);
}

nsresult CreateTables(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The database doesn't have any tables for now. It's only used for storage
  // version checking.
  // However, this is the place where any future tables should be created.

  nsresult rv;

#ifdef DEBUG
  {
    int32_t storageVersion;
    rv = aConnection->GetSchemaVersion(&storageVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(storageVersion == 0);
  }
#endif

  rv = aConnection->SetSchemaVersion(kStorageVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult CreateWebAppsStoreConnection(nsIFile* aWebAppsStoreFile,
                                      mozIStorageService* aStorageService,
                                      mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aWebAppsStoreFile);
  MOZ_ASSERT(aStorageService);
  MOZ_ASSERT(aConnection);

  // Check if the old database exists at all.
  bool exists;
  nsresult rv = aWebAppsStoreFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    // webappsstore.sqlite doesn't exist, return a null connection.
    *aConnection = nullptr;
    return NS_OK;
  }

  bool isDirectory;
  rv = aWebAppsStoreFile->IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isDirectory) {
    QM_WARNING("webappsstore.sqlite is not a file!");
    *aConnection = nullptr;
    return NS_OK;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = aStorageService->OpenUnsharedDatabase(aWebAppsStoreFile,
                                             getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Don't throw an error, leave a corrupted webappsstore database as it is.
    *aConnection = nullptr;
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = StorageDBUpdater::Update(connection);
  if (NS_FAILED(rv)) {
    // Don't throw an error, leave a non-updateable webappsstore database as
    // it is.
    *aConnection = nullptr;
    return NS_OK;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult GetLocalStorageArchiveFile(const nsAString& aDirectoryPath,
                                    nsIFile** aLsArchiveFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aDirectoryPath.IsEmpty());
  MOZ_ASSERT(aLsArchiveFile);

  nsCOMPtr<nsIFile> lsArchiveFile;
  nsresult rv =
      NS_NewLocalFile(aDirectoryPath, false, getter_AddRefs(lsArchiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = lsArchiveFile->Append(NS_LITERAL_STRING(LS_ARCHIVE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  lsArchiveFile.forget(aLsArchiveFile);
  return NS_OK;
}

nsresult GetLocalStorageArchiveTmpFile(const nsAString& aDirectoryPath,
                                       nsIFile** aLsArchiveTmpFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aDirectoryPath.IsEmpty());
  MOZ_ASSERT(aLsArchiveTmpFile);

  nsCOMPtr<nsIFile> lsArchiveTmpFile;
  nsresult rv =
      NS_NewLocalFile(aDirectoryPath, false, getter_AddRefs(lsArchiveTmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = lsArchiveTmpFile->Append(NS_LITERAL_STRING(LS_ARCHIVE_TMP_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  lsArchiveTmpFile.forget(aLsArchiveTmpFile);
  return NS_OK;
}

nsresult InitializeLocalStorageArchive(mozIStorageConnection* aConnection,
                                       uint32_t aVersion) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE database(version INTEGER NOT NULL DEFAULT 0);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("INSERT INTO database (version) VALUES (:version)"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("version"), aVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult IsLocalStorageArchiveInitialized(mozIStorageConnection* aConnection,
                                          bool& aInitialized) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  bool exists;
  nsresult rv =
      aConnection->TableExists(NS_LITERAL_CSTRING("database"), &exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aInitialized = exists;
  return NS_OK;
}

nsresult LoadLocalStorageArchiveVersion(mozIStorageConnection* aConnection,
                                        uint32_t& aVersion) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT version FROM database"), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  int32_t version;
  rv = stmt->GetInt32(0, &version);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aVersion = version;
  return NS_OK;
}

/*
nsresult SaveLocalStorageArchiveVersion(mozIStorageConnection* aConnection,
                                        uint32_t aVersion) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("UPDATE database SET version = :version;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("version"), aVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}
*/

/******************************************************************************
 * Quota manager class declarations
 ******************************************************************************/

}  // namespace

class DirectoryLockImpl final : public DirectoryLock {
  RefPtr<QuotaManager> mQuotaManager;

  const Nullable<PersistenceType> mPersistenceType;
  const nsCString mGroup;
  const OriginScope mOriginScope;
  const Nullable<Client::Type> mClientType;
  RefPtr<OpenDirectoryListener> mOpenListener;

  nsTArray<DirectoryLockImpl*> mBlocking;
  nsTArray<DirectoryLockImpl*> mBlockedOn;

  const bool mExclusive;

  // Internal quota manager operations use this flag to prevent directory lock
  // registraction/unregistration from updating origin access time, etc.
  const bool mInternal;

  bool mInvalidated;

 public:
  DirectoryLockImpl(QuotaManager* aQuotaManager,
                    const Nullable<PersistenceType>& aPersistenceType,
                    const nsACString& aGroup, const OriginScope& aOriginScope,
                    const Nullable<Client::Type>& aClientType, bool aExclusive,
                    bool aInternal, OpenDirectoryListener* aOpenListener);

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  const Nullable<PersistenceType>& GetPersistenceType() const {
    return mPersistenceType;
  }

  const nsACString& GetGroup() const { return mGroup; }

  const OriginScope& GetOriginScope() const { return mOriginScope; }

  const Nullable<Client::Type>& GetClientType() const { return mClientType; }

  bool IsInternal() const { return mInternal; }

  bool ShouldUpdateLockTable() {
    return !mInternal &&
           mPersistenceType.Value() != PERSISTENCE_TYPE_PERSISTENT;
  }

  // Test whether this DirectoryLock needs to wait for the given lock.
  bool MustWaitFor(const DirectoryLockImpl& aLock);

  void AddBlockingLock(DirectoryLockImpl* aLock) {
    AssertIsOnOwningThread();

    mBlocking.AppendElement(aLock);
  }

  const nsTArray<DirectoryLockImpl*>& GetBlockedOnLocks() { return mBlockedOn; }

  void AddBlockedOnLock(DirectoryLockImpl* aLock) {
    AssertIsOnOwningThread();

    mBlockedOn.AppendElement(aLock);
  }

  void MaybeUnblock(DirectoryLockImpl* aLock) {
    AssertIsOnOwningThread();

    mBlockedOn.RemoveElement(aLock);
    if (mBlockedOn.IsEmpty()) {
      NotifyOpenListener();
    }
  }

  void NotifyOpenListener();

  void Invalidate() {
    AssertIsOnOwningThread();

    mInvalidated = true;
  }

  NS_INLINE_DECL_REFCOUNTING(DirectoryLockImpl, override)

  void LogState() override;

 private:
  ~DirectoryLockImpl();
};

class QuotaManager::Observer final : public nsIObserver {
  static Observer* sInstance;

  bool mPendingProfileChange;
  bool mShutdownComplete;

 public:
  static nsresult Initialize();

  static void ShutdownCompleted();

 private:
  Observer() : mPendingProfileChange(false), mShutdownComplete(false) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  ~Observer() { MOZ_ASSERT(NS_IsMainThread()); }

  nsresult Init();

  nsresult Shutdown();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

namespace {

/*******************************************************************************
 * Local class declarations
 ******************************************************************************/

}  // namespace

class OriginInfo final {
  friend class GroupInfo;
  friend class QuotaManager;
  friend class QuotaObject;

 public:
  OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin, uint64_t aUsage,
             int64_t aAccessTime, bool aPersisted, bool aDirectoryExists);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginInfo)

  GroupInfo* GetGroupInfo() const { return mGroupInfo; }

  const nsCString& Origin() const { return mOrigin; }

  int64_t LockedUsage() const {
    AssertCurrentThreadOwnsQuotaMutex();

    return mUsage;
  }

  int64_t LockedAccessTime() const {
    AssertCurrentThreadOwnsQuotaMutex();

    return mAccessTime;
  }

  bool LockedPersisted() const {
    AssertCurrentThreadOwnsQuotaMutex();

    return mPersisted;
  }

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~OriginInfo() {
    MOZ_COUNT_DTOR(OriginInfo);

    MOZ_ASSERT(!mQuotaObjects.Count());
  }

  void LockedDecreaseUsage(int64_t aSize);

  void LockedUpdateAccessTime(int64_t aAccessTime) {
    AssertCurrentThreadOwnsQuotaMutex();

    mAccessTime = aAccessTime;
  }

  void LockedPersist();

  nsDataHashtable<nsStringHashKey, QuotaObject*> mQuotaObjects;

  GroupInfo* mGroupInfo;
  const nsCString mOrigin;
  uint64_t mUsage;
  int64_t mAccessTime;
  bool mPersisted;
  /**
   * In some special cases like the LocalStorage client where it's possible to
   * create a Quota-using representation but not actually write any data, we
   * want to be able to track quota for an origin without creating its origin
   * directory or the per-client files until they are actually needed to store
   * data. In those cases, the OriginInfo will be created by
   * EnsureQuotaForOrigin and the resulting mDirectoryExists will be false until
   * the origin actually needs to be created. It is possible for mUsage to be
   * greater than zero while mDirectoryExists is false, representing a state
   * where a client like LocalStorage has reserved quota for disk writes, but
   * has not yet flushed the data to disk.
   */
  bool mDirectoryExists;
};

class OriginInfoLRUComparator {
 public:
  bool Equals(const OriginInfo* a, const OriginInfo* b) const {
    return a && b ? a->LockedAccessTime() == b->LockedAccessTime()
                  : !a && !b ? true : false;
  }

  bool LessThan(const OriginInfo* a, const OriginInfo* b) const {
    return a && b ? a->LockedAccessTime() < b->LockedAccessTime()
                  : b ? true : false;
  }
};

class GroupInfo final {
  friend class GroupInfoPair;
  friend class OriginInfo;
  friend class QuotaManager;
  friend class QuotaObject;

 public:
  GroupInfo(GroupInfoPair* aGroupInfoPair, PersistenceType aPersistenceType,
            const nsACString& aGroup)
      : mGroupInfoPair(aGroupInfoPair),
        mPersistenceType(aPersistenceType),
        mGroup(aGroup),
        mUsage(0) {
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    MOZ_COUNT_CTOR(GroupInfo);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GroupInfo)

  PersistenceType GetPersistenceType() const { return mPersistenceType; }

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~GroupInfo() { MOZ_COUNT_DTOR(GroupInfo); }

  already_AddRefed<OriginInfo> LockedGetOriginInfo(const nsACString& aOrigin);

  void LockedAddOriginInfo(OriginInfo* aOriginInfo);

  void LockedRemoveOriginInfo(const nsACString& aOrigin);

  void LockedRemoveOriginInfos();

  bool LockedHasOriginInfos() {
    AssertCurrentThreadOwnsQuotaMutex();

    return !mOriginInfos.IsEmpty();
  }

  nsTArray<RefPtr<OriginInfo>> mOriginInfos;

  GroupInfoPair* mGroupInfoPair;
  PersistenceType mPersistenceType;
  nsCString mGroup;
  uint64_t mUsage;
};

class GroupInfoPair {
  friend class QuotaManager;
  friend class QuotaObject;

 public:
  GroupInfoPair() { MOZ_COUNT_CTOR(GroupInfoPair); }

  ~GroupInfoPair() { MOZ_COUNT_DTOR(GroupInfoPair); }

 private:
  already_AddRefed<GroupInfo> LockedGetGroupInfo(
      PersistenceType aPersistenceType) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo> groupInfo =
        GetGroupInfoForPersistenceType(aPersistenceType);
    return groupInfo.forget();
  }

  void LockedSetGroupInfo(PersistenceType aPersistenceType,
                          GroupInfo* aGroupInfo) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo>& groupInfo =
        GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = aGroupInfo;
  }

  void LockedClearGroupInfo(PersistenceType aPersistenceType) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo>& groupInfo =
        GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = nullptr;
  }

  bool LockedHasGroupInfos() {
    AssertCurrentThreadOwnsQuotaMutex();

    return mTemporaryStorageGroupInfo || mDefaultStorageGroupInfo;
  }

  RefPtr<GroupInfo>& GetGroupInfoForPersistenceType(
      PersistenceType aPersistenceType);

  RefPtr<GroupInfo> mTemporaryStorageGroupInfo;
  RefPtr<GroupInfo> mDefaultStorageGroupInfo;
};

namespace {

class CollectOriginsHelper final : public Runnable {
  uint64_t mMinSizeToBeFreed;

  Mutex& mMutex;
  CondVar mCondVar;

  // The members below are protected by mMutex.
  nsTArray<RefPtr<DirectoryLockImpl>> mLocks;
  uint64_t mSizeToBeFreed;
  bool mWaiting;

 public:
  CollectOriginsHelper(mozilla::Mutex& aMutex, uint64_t aMinSizeToBeFreed);

  // Blocks the current thread until origins are collected on the main thread.
  // The returned value contains an aggregate size of those origins.
  int64_t BlockAndReturnOriginsForEviction(
      nsTArray<RefPtr<DirectoryLockImpl>>& aLocks);

 private:
  ~CollectOriginsHelper() {}

  NS_IMETHOD
  Run() override;
};

class OriginOperationBase : public BackgroundThreadObject, public Runnable {
 protected:
  nsresult mResultCode;

  enum State {
    // Not yet run.
    State_Initial,

    // Running quota manager initialization on the owning thread.
    State_CreatingQuotaManager,

    // Running on the owning thread in the listener for OpenDirectory.
    State_DirectoryOpenPending,

    // Running on the IO thread.
    State_DirectoryWorkOpen,

    // Running on the owning thread after all work is done.
    State_UnblockingOpen,

    // All done.
    State_Complete
  };

 private:
  State mState;
  bool mActorDestroyed;

 protected:
  bool mNeedsMainThreadInit;
  bool mNeedsQuotaManagerInit;

 public:
  void NoteActorDestroyed() {
    AssertIsOnOwningThread();

    mActorDestroyed = true;
  }

  bool IsActorDestroyed() const {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

 protected:
  explicit OriginOperationBase(
      nsIEventTarget* aOwningThread = GetCurrentThreadEventTarget())
      : BackgroundThreadObject(aOwningThread),
        Runnable("dom::quota::OriginOperationBase"),
        mResultCode(NS_OK),
        mState(State_Initial),
        mActorDestroyed(false),
        mNeedsMainThreadInit(false),
        mNeedsQuotaManagerInit(false) {}

  // Reference counted.
  virtual ~OriginOperationBase() {
    MOZ_ASSERT(mState == State_Complete);
    MOZ_ASSERT(mActorDestroyed);
  }

#ifdef DEBUG
  State GetState() const { return mState; }
#endif

  void SetState(State aState) {
    MOZ_ASSERT(mState == State_Initial);
    mState = aState;
  }

  void AdvanceState() {
    switch (mState) {
      case State_Initial:
        mState = State_CreatingQuotaManager;
        return;
      case State_CreatingQuotaManager:
        mState = State_DirectoryOpenPending;
        return;
      case State_DirectoryOpenPending:
        mState = State_DirectoryWorkOpen;
        return;
      case State_DirectoryWorkOpen:
        mState = State_UnblockingOpen;
        return;
      case State_UnblockingOpen:
        mState = State_Complete;
        return;
      default:
        MOZ_CRASH("Bad state!");
    }
  }

  NS_IMETHOD
  Run() override;

  virtual void Open() = 0;

  nsresult DirectoryOpen();

  virtual nsresult DoDirectoryWork(QuotaManager* aQuotaManager) = 0;

  void Finish(nsresult aResult);

  virtual void UnblockOpen() = 0;

 private:
  nsresult Init();

  nsresult FinishInit();

  nsresult QuotaManagerOpen();

  nsresult DirectoryWork();
};

class FinalizeOriginEvictionOp : public OriginOperationBase {
  nsTArray<RefPtr<DirectoryLockImpl>> mLocks;

 public:
  FinalizeOriginEvictionOp(nsIEventTarget* aBackgroundThread,
                           nsTArray<RefPtr<DirectoryLockImpl>>& aLocks)
      : OriginOperationBase(aBackgroundThread) {
    MOZ_ASSERT(!NS_IsMainThread());

    mLocks.SwapElements(aLocks);
  }

  void Dispatch();

  void RunOnIOThreadImmediately();

 private:
  ~FinalizeOriginEvictionOp() {}

  virtual void Open() override;

  virtual nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  virtual void UnblockOpen() override;
};

class NormalOriginOperationBase : public OriginOperationBase,
                                  public OpenDirectoryListener {
  RefPtr<DirectoryLock> mDirectoryLock;

 protected:
  Nullable<PersistenceType> mPersistenceType;
  OriginScope mOriginScope;
  Nullable<Client::Type> mClientType;
  mozilla::Atomic<bool> mCanceled;
  const bool mExclusive;

 public:
  void RunImmediately() {
    MOZ_ASSERT(GetState() == State_Initial);

    MOZ_ALWAYS_SUCCEEDS(this->Run());
  }

 protected:
  NormalOriginOperationBase(const Nullable<PersistenceType>& aPersistenceType,
                            const OriginScope& aOriginScope, bool aExclusive)
      : mPersistenceType(aPersistenceType),
        mOriginScope(aOriginScope),
        mExclusive(aExclusive) {
    AssertIsOnOwningThread();
  }

  ~NormalOriginOperationBase() {}

 private:
  // Need to declare refcounting unconditionally, because
  // OpenDirectoryListener has pure-virtual refcounting.
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Open() override;

  virtual void UnblockOpen() override;

  // OpenDirectoryListener overrides.
  virtual void DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void DirectoryLockFailed() override;

  // Used to send results before unblocking open.
  virtual void SendResults() = 0;
};

class SaveOriginAccessTimeOp : public NormalOriginOperationBase {
  int64_t mTimestamp;

 public:
  SaveOriginAccessTimeOp(PersistenceType aPersistenceType,
                         const nsACString& aOrigin, int64_t aTimestamp)
      : NormalOriginOperationBase(Nullable<PersistenceType>(aPersistenceType),
                                  OriginScope::FromOrigin(aOrigin),
                                  /* aExclusive */ false),
        mTimestamp(aTimestamp) {
    AssertIsOnOwningThread();
  }

 private:
  ~SaveOriginAccessTimeOp() {}

  virtual nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  virtual void SendResults() override;
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class Quota final : public PQuotaParent {
#ifdef DEBUG
  bool mActorDestroyed;
#endif

 public:
  Quota();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::quota::Quota)

 private:
  ~Quota();

  void StartIdleMaintenance();

  bool VerifyRequestParams(const UsageRequestParams& aParams) const;

  bool VerifyRequestParams(const RequestParams& aParams) const;

  // IPDL methods.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PQuotaUsageRequestParent* AllocPQuotaUsageRequestParent(
      const UsageRequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPQuotaUsageRequestConstructor(
      PQuotaUsageRequestParent* aActor,
      const UsageRequestParams& aParams) override;

  virtual bool DeallocPQuotaUsageRequestParent(
      PQuotaUsageRequestParent* aActor) override;

  virtual PQuotaRequestParent* AllocPQuotaRequestParent(
      const RequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPQuotaRequestConstructor(
      PQuotaRequestParent* aActor, const RequestParams& aParams) override;

  virtual bool DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvStartIdleMaintenance() override;

  virtual mozilla::ipc::IPCResult RecvStopIdleMaintenance() override;

  virtual mozilla::ipc::IPCResult RecvAbortOperationsForProcess(
      const ContentParentId& aContentParentId) override;
};

class QuotaUsageRequestBase : public NormalOriginOperationBase,
                              public PQuotaUsageRequestParent {
 public:
  // May be overridden by subclasses if they need to perform work on the
  // background thread before being run.
  virtual bool Init(Quota* aQuota);

 protected:
  QuotaUsageRequestBase()
      : NormalOriginOperationBase(Nullable<PersistenceType>(),
                                  OriginScope::FromNull(),
                                  /* aExclusive */ false) {}

  nsresult GetUsageForOrigin(QuotaManager* aQuotaManager,
                             PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin, UsageInfo* aUsageInfo);

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(UsageRequestResponse& aResponse) = 0;

 private:
  void SendResults() override;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvCancel() final;
};

// A mix-in class to simplify operations that need to process every origin in
// one or more repositories. Sub-classes should call TraverseRepository in their
// DoDirectoryWork and implement a ProcessOrigin method for their per-origin
// logic.
class TraverseRepositoryHelper {
 public:
  TraverseRepositoryHelper() = default;

 protected:
  virtual ~TraverseRepositoryHelper() = default;

  // If ProcessOrigin returns an error, TraverseRepository will immediately
  // terminate and return the received error code to its caller.
  nsresult TraverseRepository(QuotaManager* aQuotaManager,
                              PersistenceType aPersistenceType);

 private:
  virtual bool IsCanceled() = 0;

  virtual nsresult ProcessOrigin(QuotaManager* aQuotaManager,
                                 nsIFile* aOriginDir, const bool aPersistent,
                                 const PersistenceType aPersistenceType) = 0;
};

class GetUsageOp final : public QuotaUsageRequestBase,
                         public TraverseRepositoryHelper {
  nsTArray<OriginUsage> mOriginUsages;
  nsDataHashtable<nsCStringHashKey, uint32_t> mOriginUsagesIndex;

  bool mGetAll;

 public:
  explicit GetUsageOp(const UsageRequestParams& aParams);

 private:
  ~GetUsageOp() {}

  void ProcessOriginInternal(QuotaManager* aQuotaManager,
                             const PersistenceType aPersistenceType,
                             const nsACString& aOrigin,
                             const int64_t aTimestamp, const bool aPersisted,
                             const uint64_t aUsage);

  nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  bool IsCanceled() override;

  nsresult ProcessOrigin(QuotaManager* aQuotaManager, nsIFile* aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(UsageRequestResponse& aResponse) override;
};

class GetOriginUsageOp final : public QuotaUsageRequestBase {
  // If mGetGroupUsage is false, we use mUsageInfo to record the origin usage
  // and the file usage. Otherwise, we use it to record the group usage and the
  // limit.
  UsageInfo mUsageInfo;

  const OriginUsageParams mParams;
  nsCString mSuffix;
  nsCString mGroup;
  bool mGetGroupUsage;

 public:
  explicit GetOriginUsageOp(const UsageRequestParams& aParams);

  MOZ_IS_CLASS_INIT bool Init(Quota* aQuota) override;

 private:
  ~GetOriginUsageOp() {}

  virtual nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void GetResponse(UsageRequestResponse& aResponse) override;
};

class QuotaRequestBase : public NormalOriginOperationBase,
                         public PQuotaRequestParent {
 public:
  // May be overridden by subclasses if they need to perform work on the
  // background thread before being run.
  virtual bool Init(Quota* aQuota);

 protected:
  explicit QuotaRequestBase(bool aExclusive)
      : NormalOriginOperationBase(Nullable<PersistenceType>(),
                                  OriginScope::FromNull(), aExclusive) {}

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(RequestResponse& aResponse) = 0;

 private:
  virtual void SendResults() override;

  // IPDL methods.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

class InitOp final : public QuotaRequestBase {
 public:
  InitOp() : QuotaRequestBase(/* aExclusive */ false) {
    AssertIsOnOwningThread();
  }

 private:
  ~InitOp() {}

  nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitTemporaryStorageOp final : public QuotaRequestBase {
 public:
  InitTemporaryStorageOp() : QuotaRequestBase(/* aExclusive */ false) {
    AssertIsOnOwningThread();
  }

 private:
  ~InitTemporaryStorageOp() {}

  nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitOriginOp final : public QuotaRequestBase {
  const InitOriginParams mParams;
  nsCString mSuffix;
  nsCString mGroup;
  bool mCreated;

 public:
  explicit InitOriginOp(const RequestParams& aParams);

  bool Init(Quota* aQuota) override;

 private:
  ~InitOriginOp() {}

  nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class ResetOrClearOp final : public QuotaRequestBase {
  const bool mClear;

 public:
  explicit ResetOrClearOp(bool aClear)
      : QuotaRequestBase(/* aExclusive */ true), mClear(aClear) {
    AssertIsOnOwningThread();
  }

 private:
  ~ResetOrClearOp() {}

  void DeleteFiles(QuotaManager* aQuotaManager);

  virtual nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  virtual void GetResponse(RequestResponse& aResponse) override;
};

class ClearRequestBase : public QuotaRequestBase {
 protected:
  const bool mClear;

 protected:
  ClearRequestBase(bool aExclusive, bool aClear)
      : QuotaRequestBase(aExclusive), mClear(aClear) {
    AssertIsOnOwningThread();
  }

  void DeleteFiles(QuotaManager* aQuotaManager,
                   PersistenceType aPersistenceType);

  nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;
};

class ClearOriginOp final : public ClearRequestBase {
  const ClearResetOriginParams mParams;

 public:
  explicit ClearOriginOp(const RequestParams& aParams);

  bool Init(Quota* aQuota) override;

 private:
  ~ClearOriginOp() {}

  void GetResponse(RequestResponse& aResponse) override;
};

class ClearDataOp final : public ClearRequestBase {
  const ClearDataParams mParams;

 public:
  explicit ClearDataOp(const RequestParams& aParams);

  bool Init(Quota* aQuota) override;

 private:
  ~ClearDataOp() {}

  void GetResponse(RequestResponse& aResponse) override;
};

class PersistRequestBase : public QuotaRequestBase {
  const PrincipalInfo mPrincipalInfo;

 protected:
  nsCString mSuffix;
  nsCString mGroup;

 public:
  bool Init(Quota* aQuota) override;

 protected:
  explicit PersistRequestBase(const PrincipalInfo& aPrincipalInfo);
};

class PersistedOp final : public PersistRequestBase {
  bool mPersisted;

 public:
  explicit PersistedOp(const RequestParams& aParams);

 private:
  ~PersistedOp() {}

  nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class PersistOp final : public PersistRequestBase {
 public:
  explicit PersistOp(const RequestParams& aParams);

 private:
  ~PersistOp() {}

  nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class ListInitializedOriginsOp final : public QuotaRequestBase,
                                       public TraverseRepositoryHelper {
  // XXX Bug 1521541 will make each origin has it's own state.
  nsTArray<nsCString> mOrigins;

 public:
  ListInitializedOriginsOp();

  bool Init(Quota* aQuota) override;

 private:
  ~ListInitializedOriginsOp() = default;

  nsresult DoDirectoryWork(QuotaManager* aQuotaManager) override;

  bool IsCanceled() override;

  nsresult ProcessOrigin(QuotaManager* aQuotaManager, nsIFile* aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(RequestResponse& aResponse) override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

class StoragePressureRunnable final : public Runnable {
  const uint64_t mUsage;

 public:
  explicit StoragePressureRunnable(uint64_t aUsage)
      : Runnable("dom::quota::QuotaObject::StoragePressureRunnable"),
        mUsage(aUsage) {}

 private:
  ~StoragePressureRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

/*******************************************************************************
 * Helper classes
 ******************************************************************************/

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED

class PrincipalVerifier final : public Runnable {
  nsTArray<PrincipalInfo> mPrincipalInfos;

 public:
  static already_AddRefed<PrincipalVerifier> CreateAndDispatch(
      nsTArray<PrincipalInfo>&& aPrincipalInfos);

 private:
  explicit PrincipalVerifier(nsTArray<PrincipalInfo>&& aPrincipalInfos)
      : Runnable("dom::quota::PrincipalVerifier"),
        mPrincipalInfos(std::move(aPrincipalInfos)) {
    AssertIsOnIOThread();
  }

  virtual ~PrincipalVerifier() = default;

  bool IsPrincipalInfoValid(const PrincipalInfo& aPrincipalInfo);

  NS_DECL_NSIRUNNABLE
};

#endif

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

template <typename T, bool = mozilla::IsUnsigned<T>::value>
struct IntChecker {
  static void Assert(T aInt) {
    static_assert(mozilla::IsIntegral<T>::value, "Not an integer!");
    MOZ_ASSERT(aInt >= 0);
  }
};

template <typename T>
struct IntChecker<T, true> {
  static void Assert(T aInt) {
    static_assert(mozilla::IsIntegral<T>::value, "Not an integer!");
  }
};

template <typename T>
void AssertNoOverflow(uint64_t aDest, T aArg) {
  IntChecker<T>::Assert(aDest);
  IntChecker<T>::Assert(aArg);
  MOZ_ASSERT(UINT64_MAX - aDest >= uint64_t(aArg));
}

template <typename T, typename U>
void AssertNoUnderflow(T aDest, U aArg) {
  IntChecker<T>::Assert(aDest);
  IntChecker<T>::Assert(aArg);
  MOZ_ASSERT(uint64_t(aDest) >= uint64_t(aArg));
}

inline bool IsDotFile(const nsAString& aFileName) {
  return QuotaManager::IsDotFile(aFileName);
}

inline bool IsOSMetadata(const nsAString& aFileName) {
  return QuotaManager::IsOSMetadata(aFileName);
}

bool IsOriginMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(METADATA_FILE_NAME) ||
         aFileName.EqualsLiteral(METADATA_V2_FILE_NAME) ||
         IsOSMetadata(aFileName);
}

bool IsTempMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(METADATA_TMP_FILE_NAME) ||
         aFileName.EqualsLiteral(METADATA_V2_TMP_FILE_NAME);
}

}  // namespace

BackgroundThreadObject::BackgroundThreadObject()
    : mOwningThread(GetCurrentThreadEventTarget()) {
  AssertIsOnOwningThread();
}

BackgroundThreadObject::BackgroundThreadObject(nsIEventTarget* aOwningThread)
    : mOwningThread(aOwningThread) {}

#ifdef DEBUG

void BackgroundThreadObject::AssertIsOnOwningThread() const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOwningThread);
  bool current;
  MOZ_ASSERT(NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

#endif  // DEBUG

nsIEventTarget* BackgroundThreadObject::OwningThread() const {
  MOZ_ASSERT(mOwningThread);
  return mOwningThread;
}

bool IsOnIOThread() {
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Must have a manager here!");

  bool currentThread;
  return NS_SUCCEEDED(
             quotaManager->IOThread()->IsOnCurrentThread(&currentThread)) &&
         currentThread;
}

void AssertIsOnIOThread() {
  NS_ASSERTION(IsOnIOThread(), "Running on the wrong thread!");
}

void AssertCurrentThreadOwnsQuotaMutex() {
#ifdef DEBUG
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Must have a manager here!");

  quotaManager->AssertCurrentThreadOwnsQuotaMutex();
#endif
}

void ReportInternalError(const char* aFile, uint32_t aLine, const char* aStr) {
  // Get leaf of file path
  for (const char* p = aFile; *p; ++p) {
    if (*p == '/' && *(p + 1)) {
      aFile = p + 1;
    }
  }

  nsContentUtils::LogSimpleConsoleError(
      NS_ConvertUTF8toUTF16(
          nsPrintfCString("Quota %s: %s:%" PRIu32, aStr, aFile, aLine)),
      "quota", false /* Quota Manager is not active in private browsing mode */,
      true /* Quota Manager runs always in a chrome context */);
}

namespace {

StaticAutoPtr<nsString> gBaseDirPath;

#ifdef DEBUG
bool gQuotaManagerInitialized = false;
#endif

StaticRefPtr<QuotaManager> gInstance;
bool gCreateFailed = false;
mozilla::Atomic<bool> gShutdown(false);

// Constants for temporary storage limit computing.
static const int32_t kDefaultFixedLimitKB = -1;
static const uint32_t kDefaultChunkSizeKB = 10 * 1024;
Atomic<int32_t, Relaxed> gFixedLimitKB(kDefaultFixedLimitKB);
Atomic<uint32_t, Relaxed> gChunkSizeKB(kDefaultChunkSizeKB);

Atomic<bool> gTestingEnabled(false);

class StorageOperationBase {
 protected:
  struct OriginProps;

  nsTArray<OriginProps> mOriginProps;

  nsCOMPtr<nsIFile> mDirectory;

  const bool mPersistent;

 public:
  StorageOperationBase(nsIFile* aDirectory, bool aPersistent)
      : mDirectory(aDirectory), mPersistent(aPersistent) {
    AssertIsOnIOThread();
  }

  NS_INLINE_DECL_REFCOUNTING(StorageOperationBase)

 protected:
  virtual ~StorageOperationBase() {}

  nsresult GetDirectoryMetadata(nsIFile* aDirectory, int64_t& aTimestamp,
                                nsACString& aGroup, nsACString& aOrigin,
                                Nullable<bool>& aIsApp);

  // Upgrade helper to load the contents of ".metadata-v2" files from previous
  // schema versions.  Although QuotaManager has a similar GetDirectoryMetadata2
  // method, it is only intended to read current version ".metadata-v2" files.
  // And unlike the old ".metadata" files, the ".metadata-v2" format can evolve
  // because our "storage.sqlite" lets us track the overall version of the
  // storage directory.
  nsresult GetDirectoryMetadata2(nsIFile* aDirectory, int64_t& aTimestamp,
                                 nsACString& aSuffix, nsACString& aGroup,
                                 nsACString& aOrigin, bool& aIsApp);

  nsresult RemoveObsoleteOrigin(const OriginProps& aOriginProps);

  nsresult ProcessOriginDirectories();

  virtual nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) = 0;
};

struct StorageOperationBase::OriginProps {
  enum Type { eChrome, eContent, eObsolete, eInvalid };

  nsCOMPtr<nsIFile> mDirectory;
  nsString mLeafName;
  nsCString mSpec;
  OriginAttributes mAttrs;
  int64_t mTimestamp;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mOrigin;

  Type mType;
  bool mNeedsRestore;
  bool mNeedsRestore2;
  bool mIgnore;

 public:
  explicit OriginProps()
      : mTimestamp(0),
        mType(eContent),
        mNeedsRestore(false),
        mNeedsRestore2(false),
        mIgnore(false) {}

  nsresult Init(nsIFile* aDirectory);
};

class MOZ_STACK_CLASS OriginParser final {
 public:
  enum ResultType { InvalidOrigin, ObsoleteOrigin, ValidOrigin };

 private:
  static bool IgnoreWhitespace(char16_t /* aChar */) { return false; }

  typedef nsCCharSeparatedTokenizerTemplate<IgnoreWhitespace> Tokenizer;

  enum SchemeType { eNone, eFile, eAbout, eChrome };

  enum State {
    eExpectingAppIdOrScheme,
    eExpectingInMozBrowser,
    eExpectingScheme,
    eExpectingEmptyToken1,
    eExpectingEmptyToken2,
    eExpectingEmptyTokenOrUniversalFileOrigin,
    eExpectingHost,
    eExpectingPort,
    eExpectingEmptyTokenOrDriveLetterOrPathnameComponent,
    eExpectingEmptyTokenOrPathnameComponent,
    eExpectingEmptyToken1OrHost,

    // We transit from eExpectingHost to this state when we encounter a host
    // beginning with "[" which indicates an IPv6 literal. Because we mangle the
    // IPv6 ":" delimiter to be a "+", we will receive separate tokens for each
    // portion of the IPv6 address, including a final token that ends with "]".
    // (Note that we do not mangle "[" or "]".) Note that the URL spec
    // explicitly disclaims support for "<zone_id>" and so we don't have to deal
    // with that.
    eExpectingIPV6Token,
    eComplete,
    eHandledTrailingSeparator
  };

  const nsCString mOrigin;
  const OriginAttributes mOriginAttributes;
  Tokenizer mTokenizer;

  nsCString mScheme;
  nsCString mHost;
  Nullable<uint32_t> mPort;
  nsTArray<nsCString> mPathnameComponents;
  nsCString mHandledTokens;

  SchemeType mSchemeType;
  State mState;
  bool mInIsolatedMozBrowser;
  bool mUniversalFileOrigin;
  bool mMaybeDriveLetter;
  bool mError;
  bool mMaybeObsolete;

  // Number of group which a IPv6 address has. Should be less than 9.
  uint8_t mIPGroup;

 public:
  OriginParser(const nsACString& aOrigin,
               const OriginAttributes& aOriginAttributes)
      : mOrigin(aOrigin),
        mOriginAttributes(aOriginAttributes),
        mTokenizer(aOrigin, '+'),
        mPort(),
        mSchemeType(eNone),
        mState(eExpectingAppIdOrScheme),
        mInIsolatedMozBrowser(false),
        mUniversalFileOrigin(false),
        mMaybeDriveLetter(false),
        mError(false),
        mMaybeObsolete(false),
        mIPGroup(0) {}

  static ResultType ParseOrigin(const nsACString& aOrigin, nsCString& aSpec,
                                OriginAttributes* aAttrs);

  ResultType Parse(nsACString& aSpec, OriginAttributes* aAttrs);

 private:
  void HandleScheme(const nsDependentCSubstring& aToken);

  void HandlePathnameComponent(const nsDependentCSubstring& aToken);

  void HandleToken(const nsDependentCSubstring& aToken);

  void HandleTrailingSeparator();
};

class RepositoryOperationBase : public StorageOperationBase {
 public:
  RepositoryOperationBase(nsIFile* aDirectory, bool aPersistent)
      : StorageOperationBase(aDirectory, aPersistent) {}

  nsresult ProcessRepository();

 protected:
  virtual ~RepositoryOperationBase() {}

  template <typename UpgradeMethod>
  nsresult MaybeUpgradeClients(const OriginProps& aOriginsProps,
                               UpgradeMethod aMethod);

 private:
  virtual nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                          bool* aRemoved) = 0;

  virtual nsresult PrepareClientDirectory(nsIFile* aFile,
                                          const nsAString& aLeafName,
                                          bool& aRemoved);
};

class CreateOrUpgradeDirectoryMetadataHelper final
    : public RepositoryOperationBase {
  nsCOMPtr<nsIFile> mPermanentStorageDir;

 public:
  CreateOrUpgradeDirectoryMetadataHelper(nsIFile* aDirectory, bool aPersistent)
      : RepositoryOperationBase(aDirectory, aPersistent) {}

 private:
  nsresult MaybeUpgradeOriginDirectory(nsIFile* aDirectory);

  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom0_0To1_0Helper final : public RepositoryOperationBase {
 public:
  UpgradeStorageFrom0_0To1_0Helper(nsIFile* aDirectory, bool aPersistent)
      : RepositoryOperationBase(aDirectory, aPersistent) {}

 private:
  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom1_0To2_0Helper final : public RepositoryOperationBase {
 public:
  UpgradeStorageFrom1_0To2_0Helper(nsIFile* aDirectory, bool aPersistent)
      : RepositoryOperationBase(aDirectory, aPersistent) {}

 private:
  nsresult MaybeRemoveMorgueDirectory(const OriginProps& aOriginProps);

  nsresult MaybeRemoveAppsData(const OriginProps& aOriginProps, bool* aRemoved);

  nsresult MaybeStripObsoleteOriginAttributes(const OriginProps& aOriginProps,
                                              bool* aStripped);

  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom2_0To2_1Helper final : public RepositoryOperationBase {
 public:
  UpgradeStorageFrom2_0To2_1Helper(nsIFile* aDirectory, bool aPersistent)
      : RepositoryOperationBase(aDirectory, aPersistent) {}

 private:
  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom2_1To2_2Helper final : public RepositoryOperationBase {
 public:
  UpgradeStorageFrom2_1To2_2Helper(nsIFile* aDirectory, bool aPersistent)
      : RepositoryOperationBase(aDirectory, aPersistent) {}

 private:
  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;

  nsresult PrepareClientDirectory(nsIFile* aFile, const nsAString& aLeafName,
                                  bool& aRemoved) override;
};

class RestoreDirectoryMetadata2Helper final : public StorageOperationBase {
 public:
  RestoreDirectoryMetadata2Helper(nsIFile* aDirectory, bool aPersistent)
      : StorageOperationBase(aDirectory, aPersistent) {}

  nsresult RestoreMetadata2File();

 private:
  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

void SanitizeOriginString(nsCString& aOrigin) {
#ifdef XP_WIN
  NS_ASSERTION(!strcmp(QuotaManager::kReplaceChars,
                       FILE_ILLEGAL_CHARACTERS FILE_PATH_SEPARATOR),
               "Illegal file characters have changed!");
#endif

  aOrigin.ReplaceChar(QuotaManager::kReplaceChars, '+');
}

nsresult CloneStoragePath(nsIFile* aBaseDir, const nsAString& aStorageName,
                          nsAString& aStoragePath) {
  nsresult rv;

  nsCOMPtr<nsIFile> storageDir;
  rv = aBaseDir->Clone(getter_AddRefs(storageDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = storageDir->Append(aStorageName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = storageDir->GetPath(aStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

int64_t GetLastModifiedTime(nsIFile* aFile, bool aPersistent) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aFile);

  class MOZ_STACK_CLASS Helper final {
   public:
    static nsresult GetLastModifiedTime(nsIFile* aFile, int64_t* aTimestamp) {
      AssertIsOnIOThread();
      MOZ_ASSERT(aFile);
      MOZ_ASSERT(aTimestamp);

      bool isDirectory;
      nsresult rv = aFile->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!isDirectory) {
        nsString leafName;
        rv = aFile->GetLeafName(leafName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (IsOriginMetadata(leafName) || IsTempMetadata(leafName) ||
            IsDotFile(leafName)) {
          return NS_OK;
        }

        int64_t timestamp;
        rv = aFile->GetLastModifiedTime(&timestamp);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        // Need to convert from milliseconds to microseconds.
        MOZ_ASSERT((INT64_MAX / PR_USEC_PER_MSEC) > timestamp);
        timestamp *= int64_t(PR_USEC_PER_MSEC);

        if (timestamp > *aTimestamp) {
          *aTimestamp = timestamp;
        }
        return NS_OK;
      }

      nsCOMPtr<nsIDirectoryEnumerator> entries;
      rv = aFile->GetDirectoryEntries(getter_AddRefs(entries));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIFile> file;
      while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(file)))) &&
             file) {
        rv = GetLastModifiedTime(file, aTimestamp);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }
  };

  if (aPersistent) {
    return PR_Now();
  }

  int64_t timestamp = INT64_MIN;
  nsresult rv = Helper::GetLastModifiedTime(aFile, &timestamp);
  if (NS_FAILED(rv)) {
    timestamp = PR_Now();
  }

  return timestamp;
}

nsresult EnsureDirectory(nsIFile* aDirectory, bool* aCreated) {
  AssertIsOnIOThread();

  nsresult rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
  if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    bool isDirectory;
    rv = aDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_UNEXPECTED);

    *aCreated = false;
  } else {
    NS_ENSURE_SUCCESS(rv, rv);

    *aCreated = true;
  }

  return NS_OK;
}

enum FileFlag { kTruncateFileFlag, kUpdateFileFlag, kAppendFileFlag };

nsresult GetOutputStream(nsIFile* aFile, FileFlag aFileFlag,
                         nsIOutputStream** aStream) {
  AssertIsOnIOThread();

  nsresult rv;

  nsCOMPtr<nsIOutputStream> outputStream;
  switch (aFileFlag) {
    case kTruncateFileFlag: {
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), aFile);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      break;
    }

    case kUpdateFileFlag: {
      bool exists;
      rv = aFile->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!exists) {
        *aStream = nullptr;
        return NS_OK;
      }

      nsCOMPtr<nsIFileStream> stream;
      rv = NS_NewLocalFileStream(getter_AddRefs(stream), aFile);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      outputStream = do_QueryInterface(stream);
      if (NS_WARN_IF(!outputStream)) {
        return NS_ERROR_FAILURE;
      }

      break;
    }

    case kAppendFileFlag: {
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), aFile,
                                       PR_WRONLY | PR_CREATE_FILE | PR_APPEND);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  outputStream.forget(aStream);
  return NS_OK;
}

nsresult GetBinaryOutputStream(nsIFile* aFile, FileFlag aFileFlag,
                               nsIBinaryOutputStream** aStream) {
  nsCOMPtr<nsIOutputStream> outputStream;
  nsresult rv = GetOutputStream(aFile, aFileFlag, getter_AddRefs(outputStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!outputStream)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIObjectOutputStream> objectOutputStream =
      NS_NewObjectOutputStream(outputStream);

  objectOutputStream.forget(aStream);
  return NS_OK;
}

void GetJarPrefix(bool aInIsolatedMozBrowser, nsACString& aJarPrefix) {
  aJarPrefix.Truncate();

  // Fallback.
  if (!aInIsolatedMozBrowser) {
    return;
  }

  // AppId is an unused b2g identifier. Let's set it to 0 all the time (see bug
  // 1320404).
  // aJarPrefix = appId + "+" + { 't', 'f' } + "+";
  aJarPrefix.AppendInt(0);  // TODO: this is the appId, to be removed.
  aJarPrefix.Append('+');
  aJarPrefix.Append(aInIsolatedMozBrowser ? 't' : 'f');
  aJarPrefix.Append('+');
}

nsresult CreateDirectoryMetadata(nsIFile* aDirectory, int64_t aTimestamp,
                                 const nsACString& aSuffix,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin) {
  AssertIsOnIOThread();

  OriginAttributes groupAttributes;

  nsCString groupNoSuffix;
  bool ok = groupAttributes.PopulateFromOrigin(aGroup, groupNoSuffix);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  nsCString groupPrefix;
  GetJarPrefix(groupAttributes.mInIsolatedMozBrowser, groupPrefix);

  nsCString group = groupPrefix + groupNoSuffix;

  OriginAttributes originAttributes;

  nsCString originNoSuffix;
  ok = originAttributes.PopulateFromOrigin(aOrigin, originNoSuffix);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  nsCString originPrefix;
  GetJarPrefix(originAttributes.mInIsolatedMozBrowser, originPrefix);

  nsCString origin = originPrefix + originNoSuffix;

  MOZ_ASSERT(groupPrefix == originPrefix);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(METADATA_TMP_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetBinaryOutputStream(file, kTruncateFileFlag, getter_AddRefs(stream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(stream);

  rv = stream->Write64(aTimestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(group.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(origin.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Currently unused (used to be isApp).
  rv = stream->WriteBoolean(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Flush();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->RenameTo(nullptr, NS_LITERAL_STRING(METADATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult CreateDirectoryMetadata2(nsIFile* aDirectory, int64_t aTimestamp,
                                  bool aPersisted, const nsACString& aSuffix,
                                  const nsACString& aGroup,
                                  const nsACString& aOrigin) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(METADATA_V2_TMP_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetBinaryOutputStream(file, kTruncateFileFlag, getter_AddRefs(stream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(stream);

  rv = stream->Write64(aTimestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteBoolean(aPersisted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Reserved data 1
  rv = stream->Write32(0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Reserved data 2
  rv = stream->Write32(0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The suffix isn't used right now, but we might need it in future. It's
  // a bit of redundancy we can live with given how painful is to upgrade
  // metadata files.
  rv = stream->WriteStringZ(PromiseFlatCString(aSuffix).get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(PromiseFlatCString(aGroup).get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(PromiseFlatCString(aOrigin).get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Currently unused (used to be isApp).
  rv = stream->WriteBoolean(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Flush();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->RenameTo(nullptr, NS_LITERAL_STRING(METADATA_V2_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult GetBinaryInputStream(nsIFile* aDirectory, const nsAString& aFilename,
                              nsIBinaryInputStream** aStream) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aStream);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(aFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> bufferedStream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                 stream.forget(), 512);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryInputStream> binaryStream =
      do_CreateInstance("@mozilla.org/binaryinputstream;1");
  if (NS_WARN_IF(!binaryStream)) {
    return NS_ERROR_FAILURE;
  }

  rv = binaryStream->SetInputStream(bufferedStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  binaryStream.forget(aStream);
  return NS_OK;
}

// This method computes and returns our best guess for the temporary storage
// limit (in bytes), based on the amount of space users have free on their hard
// drive and on given temporary storage usage (also in bytes).
nsresult GetTemporaryStorageLimit(nsIFile* aDirectory, uint64_t aCurrentUsage,
                                  uint64_t* aLimit) {
  // Check for free space on device where temporary storage directory lives.
  int64_t bytesAvailable;
  nsresult rv = aDirectory->GetDiskSpaceAvailable(&bytesAvailable);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(bytesAvailable >= 0, "Negative bytes available?!");

  uint64_t availableKB =
      static_cast<uint64_t>((bytesAvailable + aCurrentUsage) / 1024);

  // Grow/shrink in gChunkSizeKB units, deliberately, so that in the common case
  // we don't shrink temporary storage and evict origin data every time we
  // initialize.
  availableKB = (availableKB / gChunkSizeKB) * gChunkSizeKB;

  // Allow temporary storage to consume up to half the available space.
  uint64_t resultKB = availableKB * .50;

  *aLimit = resultKB * 1024;
  return NS_OK;
}

}  // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

void InitializeQuotaManager() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gQuotaManagerInitialized);

  if (!QuotaManager::IsRunningGTests()) {
    // This service has to be started on the main thread currently.
    nsCOMPtr<mozIStorageService> ss;
    if (NS_WARN_IF(!(ss = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID)))) {
      NS_WARNING("Failed to get storage service!");
    }
  }

  if (NS_FAILED(QuotaManager::Initialize())) {
    NS_WARNING("Failed to initialize quota manager!");
  }

  if (NS_FAILED(Preferences::AddAtomicIntVarCache(
          &gFixedLimitKB, PREF_FIXED_LIMIT, kDefaultFixedLimitKB)) ||
      NS_FAILED(Preferences::AddAtomicUintVarCache(
          &gChunkSizeKB, PREF_CHUNK_SIZE, kDefaultChunkSizeKB))) {
    NS_WARNING("Unable to respond to temp storage pref changes!");
  }

  if (NS_FAILED(Preferences::AddAtomicBoolVarCache(
          &gTestingEnabled, PREF_TESTING_FEATURES, false))) {
    NS_WARNING("Unable to respond to testing pref changes!");
  }

#ifdef DEBUG
  gQuotaManagerInitialized = true;
#endif
}

PQuotaParent* AllocPQuotaParent() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

  RefPtr<Quota> actor = new Quota();

  return actor.forget().take();
}

bool DeallocPQuotaParent(PQuotaParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Quota> actor = dont_AddRef(static_cast<Quota*>(aActor));
  return true;
}

bool RecvShutdownQuotaManager() {
  AssertIsOnBackgroundThread();

  QuotaManager::ShutdownInstance();

  return true;
}

/*******************************************************************************
 * Directory lock
 ******************************************************************************/

DirectoryLockImpl::DirectoryLockImpl(
    QuotaManager* aQuotaManager,
    const Nullable<PersistenceType>& aPersistenceType, const nsACString& aGroup,
    const OriginScope& aOriginScope, const Nullable<Client::Type>& aClientType,
    bool aExclusive, bool aInternal, OpenDirectoryListener* aOpenListener)
    : mQuotaManager(aQuotaManager),
      mPersistenceType(aPersistenceType),
      mGroup(aGroup),
      mOriginScope(aOriginScope),
      mClientType(aClientType),
      mOpenListener(aOpenListener),
      mExclusive(aExclusive),
      mInternal(aInternal),
      mInvalidated(false) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuotaManager);
  MOZ_ASSERT_IF(aOriginScope.IsOrigin(), !aOriginScope.GetOrigin().IsEmpty());
  MOZ_ASSERT_IF(!aInternal, !aPersistenceType.IsNull());
  MOZ_ASSERT_IF(!aInternal,
                aPersistenceType.Value() != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT_IF(!aInternal, !aGroup.IsEmpty());
  MOZ_ASSERT_IF(!aInternal, aOriginScope.IsOrigin());
  MOZ_ASSERT_IF(!aInternal, !aClientType.IsNull());
  MOZ_ASSERT_IF(!aInternal, aClientType.Value() < Client::TypeMax());
  MOZ_ASSERT_IF(!aInternal, aOpenListener);
}

DirectoryLockImpl::~DirectoryLockImpl() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mQuotaManager);

  for (DirectoryLockImpl* blockingLock : mBlocking) {
    blockingLock->MaybeUnblock(this);
  }

  mBlocking.Clear();

  mQuotaManager->UnregisterDirectoryLock(this);
}

#ifdef DEBUG

void DirectoryLockImpl::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mQuotaManager);
  mQuotaManager->AssertIsOnOwningThread();
}

#endif  // DEBUG

bool DirectoryLockImpl::MustWaitFor(const DirectoryLockImpl& aExistingLock) {
  AssertIsOnOwningThread();

  // Waiting is never required if the ops in comparison represent shared locks.
  if (!aExistingLock.mExclusive && !mExclusive) {
    return false;
  }

  // If the persistence types don't overlap, the op can proceed.
  if (!aExistingLock.mPersistenceType.IsNull() && !mPersistenceType.IsNull() &&
      aExistingLock.mPersistenceType.Value() != mPersistenceType.Value()) {
    return false;
  }

  // If the origin scopes don't overlap, the op can proceed.
  bool match = aExistingLock.mOriginScope.Matches(mOriginScope);
  if (!match) {
    return false;
  }

  // If the client types don't overlap, the op can proceed.
  if (!aExistingLock.mClientType.IsNull() && !mClientType.IsNull() &&
      aExistingLock.mClientType.Value() != mClientType.Value()) {
    return false;
  }

  // Otherwise, when all attributes overlap (persistence type, origin scope and
  // client type) the op must wait.
  return true;
}

void DirectoryLockImpl::NotifyOpenListener() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mQuotaManager);
  MOZ_ASSERT(mOpenListener);

  if (mInvalidated) {
    mOpenListener->DirectoryLockFailed();
  } else {
    mOpenListener->DirectoryLockAcquired(this);
  }

  mOpenListener = nullptr;

  mQuotaManager->RemovePendingDirectoryLock(this);
}

void DirectoryLockImpl::LogState() {
  AssertIsOnOwningThread();

  if (!QM_LOG_TEST()) {
    return;
  }

  QM_LOG(("DirectoryLockImpl [%p]", this));

  nsCString persistenceType;
  if (mPersistenceType.IsNull()) {
    persistenceType.AssignLiteral("null");
  } else {
    PersistenceTypeToText(mPersistenceType.Value(), persistenceType);
  }
  QM_LOG(("  mPersistenceType: %s", persistenceType.get()));

  QM_LOG(("  mGroup: %s", mGroup.get()));

  nsCString originScope;
  if (mOriginScope.IsOrigin()) {
    originScope.AssignLiteral("origin:");
    originScope.Append(mOriginScope.GetOrigin());
  } else if (mOriginScope.IsPrefix()) {
    originScope.AssignLiteral("prefix:");
    originScope.Append(mOriginScope.GetOriginNoSuffix());
  } else if (mOriginScope.IsPattern()) {
    originScope.AssignLiteral("pattern:");
    // Can't call GetJSONPattern since it only works on the main thread.
  } else {
    MOZ_ASSERT(mOriginScope.IsNull());
    originScope.AssignLiteral("null");
  }
  QM_LOG(("  mOriginScope: %s", originScope.get()));

  nsString clientType;
  if (mClientType.IsNull()) {
    clientType.AssignLiteral("null");
  } else {
    Client::TypeToText(mClientType.Value(), clientType);
  }
  QM_LOG(("  mClientType: %s", NS_ConvertUTF16toUTF8(clientType).get()));

  nsCString blockedOnString;
  for (auto blockedOn : mBlockedOn) {
    blockedOnString.Append(nsPrintfCString(" [%p]", blockedOn));
  }
  QM_LOG(("  mBlockedOn:%s", blockedOnString.get()));

  QM_LOG(("  mExclusive: %d", mExclusive));

  QM_LOG(("  mInternal: %d", mInternal));

  QM_LOG(("  mInvalidated: %d", mInvalidated));

  for (auto blockedOn : mBlockedOn) {
    blockedOn->LogState();
  }
}

QuotaManager::Observer* QuotaManager::Observer::sInstance = nullptr;

// static
nsresult QuotaManager::Observer::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Observer> observer = new Observer();

  nsresult rv = observer->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  sInstance = observer;

  return NS_OK;
}

// static
void QuotaManager::Observer::ShutdownCompleted() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance);

  sInstance->mShutdownComplete = true;
}

nsresult QuotaManager::Observer::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = obs->AddObserver(this, kProfileDoChangeTopic, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    return rv;
  }

  rv = obs->AddObserver(this, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    obs->RemoveObserver(this, kProfileDoChangeTopic);
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::Observer::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ALWAYS_SUCCEEDS(
      obs->RemoveObserver(this, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID));
  MOZ_ALWAYS_SUCCEEDS(obs->RemoveObserver(this, kProfileDoChangeTopic));
  MOZ_ALWAYS_SUCCEEDS(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID));

  sInstance = nullptr;

  // In general, the instance will have died after the latter removal call, so
  // it's not safe to do anything after that point.
  // However, Shutdown is currently called from Observe which is called by the
  // Observer Service which holds a strong reference to the observer while the
  // Observe method is being called.

  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaManager::Observer, nsIObserver)

NS_IMETHODIMP
QuotaManager::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  if (!strcmp(aTopic, kProfileDoChangeTopic)) {
    if (NS_WARN_IF(gBaseDirPath)) {
      NS_WARNING(
          "profile-before-change-qm must precede repeated "
          "profile-do-change!");
      return NS_OK;
    }

    gBaseDirPath = new nsString();

    nsCOMPtr<nsIFile> baseDir;
    rv = NS_GetSpecialDirectory(NS_APP_INDEXEDDB_PARENT_DIR,
                                getter_AddRefs(baseDir));
    if (NS_FAILED(rv)) {
      rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                  getter_AddRefs(baseDir));
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = baseDir->GetPath(*gBaseDirPath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID)) {
    if (NS_WARN_IF(!gBaseDirPath)) {
      NS_WARNING("profile-do-change must precede profile-before-change-qm!");
      return NS_OK;
    }

    // mPendingProfileChange is our re-entrancy guard (the nested event loop
    // below may cause re-entrancy).
    if (mPendingProfileChange) {
      return NS_OK;
    }

    AutoRestore<bool> pending(mPendingProfileChange);
    mPendingProfileChange = true;

    mShutdownComplete = false;

    PBackgroundChild* backgroundActor =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      return NS_ERROR_FAILURE;
    }

    if (NS_WARN_IF(!backgroundActor->SendShutdownQuotaManager())) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return mShutdownComplete; }));

    gBaseDirPath = nullptr;

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    rv = Shutdown();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  NS_WARNING("Unknown observer topic!");
  return NS_OK;
}

/*******************************************************************************
 * Quota object
 ******************************************************************************/

void QuotaObject::AddRef() {
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    ++mRefCnt;

    return;
  }

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  ++mRefCnt;
}

void QuotaObject::Release() {
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    nsrefcnt count = --mRefCnt;
    if (count == 0) {
      mRefCnt = 1;
      delete this;
    }

    return;
  }

  {
    MutexAutoLock lock(quotaManager->mQuotaMutex);

    --mRefCnt;

    if (mRefCnt > 0) {
      return;
    }

    if (mOriginInfo) {
      mOriginInfo->mQuotaObjects.Remove(mPath);
    }
  }

  delete this;
}

bool QuotaObject::MaybeUpdateSize(int64_t aSize, bool aTruncate) {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  return LockedMaybeUpdateSize(aSize, aTruncate);
}

bool QuotaObject::IncreaseSize(int64_t aDelta) {
  MOZ_ASSERT(aDelta >= 0);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  AssertNoOverflow(mSize, aDelta);
  int64_t size = mSize + aDelta;

  return LockedMaybeUpdateSize(size, /* aTruncate */ false);
}

void QuotaObject::DisableQuotaCheck() {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  mQuotaCheckDisabled = true;
}

void QuotaObject::EnableQuotaCheck() {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  mQuotaCheckDisabled = false;
}

bool QuotaObject::LockedMaybeUpdateSize(int64_t aSize, bool aTruncate) {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  quotaManager->mQuotaMutex.AssertCurrentThreadOwns();

  if (mWritingDone == false && mOriginInfo) {
    mWritingDone = true;
    StorageActivityService::SendActivity(mOriginInfo->mOrigin);
  }

  if (mQuotaCheckDisabled) {
    return true;
  }

  if (mSize == aSize) {
    return true;
  }

  if (!mOriginInfo) {
    mSize = aSize;
    return true;
  }

  GroupInfo* groupInfo = mOriginInfo->mGroupInfo;
  MOZ_ASSERT(groupInfo);

  if (mSize > aSize) {
    if (aTruncate) {
      const int64_t delta = mSize - aSize;

      AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, delta);
      quotaManager->mTemporaryStorageUsage -= delta;

      if (!mOriginInfo->LockedPersisted()) {
        AssertNoUnderflow(groupInfo->mUsage, delta);
        groupInfo->mUsage -= delta;
      }

      AssertNoUnderflow(mOriginInfo->mUsage, delta);
      mOriginInfo->mUsage -= delta;

      mSize = aSize;
    }
    return true;
  }

  MOZ_ASSERT(mSize < aSize);

  RefPtr<GroupInfo> complementaryGroupInfo =
      groupInfo->mGroupInfoPair->LockedGetGroupInfo(
          ComplementaryPersistenceType(groupInfo->mPersistenceType));

  uint64_t delta = aSize - mSize;

  AssertNoOverflow(mOriginInfo->mUsage, delta);
  uint64_t newUsage = mOriginInfo->mUsage + delta;

  // Temporary storage has no limit for origin usage (there's a group and the
  // global limit though).

  uint64_t newGroupUsage = groupInfo->mUsage;
  if (!mOriginInfo->LockedPersisted()) {
    AssertNoOverflow(groupInfo->mUsage, delta);
    newGroupUsage += delta;

    uint64_t groupUsage = groupInfo->mUsage;
    if (complementaryGroupInfo) {
      AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
      groupUsage += complementaryGroupInfo->mUsage;
    }

    // Temporary storage has a hard limit for group usage (20 % of the global
    // limit).
    AssertNoOverflow(groupUsage, delta);
    if (groupUsage + delta > quotaManager->GetGroupLimit()) {
      return false;
    }
  }

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
  uint64_t newTemporaryStorageUsage =
      quotaManager->mTemporaryStorageUsage + delta;

  if (newTemporaryStorageUsage > quotaManager->mTemporaryStorageLimit) {
    // This will block the thread without holding the lock while waitting.

    AutoTArray<RefPtr<DirectoryLockImpl>, 10> locks;
    uint64_t sizeToBeFreed;

    if (IsOnBackgroundThread()) {
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      sizeToBeFreed = quotaManager->CollectOriginsForEviction(delta, locks);
    } else {
      sizeToBeFreed =
          quotaManager->LockedCollectOriginsForEviction(delta, locks);
    }

    if (!sizeToBeFreed) {
      uint64_t usage = quotaManager->mTemporaryStorageUsage;

      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      quotaManager->NotifyStoragePressure(usage);

      return false;
    }

    NS_ASSERTION(sizeToBeFreed >= delta, "Huh?");

    {
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      for (RefPtr<DirectoryLockImpl>& lock : locks) {
        MOZ_ASSERT(!lock->GetPersistenceType().IsNull());
        MOZ_ASSERT(lock->GetOriginScope().IsOrigin());
        MOZ_ASSERT(!lock->GetOriginScope().GetOrigin().IsEmpty());

        quotaManager->DeleteFilesForOrigin(lock->GetPersistenceType().Value(),
                                           lock->GetOriginScope().GetOrigin());
      }
    }

    // Relocked.

    NS_ASSERTION(mOriginInfo, "How come?!");

    for (DirectoryLockImpl* lock : locks) {
      MOZ_ASSERT(!lock->GetPersistenceType().IsNull());
      MOZ_ASSERT(!lock->GetGroup().IsEmpty());
      MOZ_ASSERT(lock->GetOriginScope().IsOrigin());
      MOZ_ASSERT(!lock->GetOriginScope().GetOrigin().IsEmpty());
      MOZ_ASSERT(
          !(lock->GetOriginScope().GetOrigin() == mOriginInfo->mOrigin &&
            lock->GetPersistenceType().Value() == groupInfo->mPersistenceType),
          "Deleted itself!");

      quotaManager->LockedRemoveQuotaForOrigin(
          lock->GetPersistenceType().Value(), lock->GetGroup(),
          lock->GetOriginScope().GetOrigin());
    }

    // We unlocked and relocked several times so we need to recompute all the
    // essential variables and recheck the group limit.

    AssertNoUnderflow(aSize, mSize);
    delta = aSize - mSize;

    AssertNoOverflow(mOriginInfo->mUsage, delta);
    newUsage = mOriginInfo->mUsage + delta;

    newGroupUsage = groupInfo->mUsage;
    if (!mOriginInfo->LockedPersisted()) {
      AssertNoOverflow(groupInfo->mUsage, delta);
      newGroupUsage += delta;

      uint64_t groupUsage = groupInfo->mUsage;
      if (complementaryGroupInfo) {
        AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
        groupUsage += complementaryGroupInfo->mUsage;
      }

      AssertNoOverflow(groupUsage, delta);
      if (groupUsage + delta > quotaManager->GetGroupLimit()) {
        // Unfortunately some other thread increased the group usage in the
        // meantime and we are not below the group limit anymore.

        // However, the origin eviction must be finalized in this case too.
        MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

        quotaManager->FinalizeOriginEviction(locks);

        return false;
      }
    }

    AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
    newTemporaryStorageUsage = quotaManager->mTemporaryStorageUsage + delta;

    NS_ASSERTION(
        newTemporaryStorageUsage <= quotaManager->mTemporaryStorageLimit,
        "How come?!");

    // Ok, we successfully freed enough space and the operation can continue
    // without throwing the quota error.
    mOriginInfo->mUsage = newUsage;
    if (!mOriginInfo->LockedPersisted()) {
      groupInfo->mUsage = newGroupUsage;
    }
    quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;
    ;

    // Some other thread could increase the size in the meantime, but no more
    // than this one.
    MOZ_ASSERT(mSize < aSize);
    mSize = aSize;

    // Finally, release IO thread only objects and allow next synchronized
    // ops for the evicted origins.
    MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

    quotaManager->FinalizeOriginEviction(locks);

    return true;
  }

  mOriginInfo->mUsage = newUsage;
  if (!mOriginInfo->LockedPersisted()) {
    groupInfo->mUsage = newGroupUsage;
  }
  quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;

  mSize = aSize;

  return true;
}

/*******************************************************************************
 * Quota manager
 ******************************************************************************/

QuotaManager::QuotaManager()
    : mQuotaMutex("QuotaManager.mQuotaMutex"),
      mTemporaryStorageLimit(0),
      mTemporaryStorageUsage(0),
      mTemporaryStorageInitialized(false),
      mStorageInitialized(false) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!gInstance);
}

QuotaManager::~QuotaManager() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!gInstance || gInstance == this);
}

// static
nsresult QuotaManager::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = Observer::Initialize();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void QuotaManager::GetOrCreate(nsIRunnable* aCallback,
                               nsIEventTarget* aMainEventTarget) {
  AssertIsOnBackgroundThread();

  if (IsShuttingDown()) {
    MOZ_ASSERT(false, "Calling QuotaManager::GetOrCreate() after shutdown!");
    return;
  }

  if (NS_WARN_IF(!gBaseDirPath)) {
    NS_WARNING("profile-do-change must precede QuotaManager::GetOrCreate()");
    MOZ_ASSERT(!gInstance);
  } else if (gInstance || gCreateFailed) {
    MOZ_ASSERT_IF(gCreateFailed, !gInstance);
  } else {
    RefPtr<QuotaManager> manager = new QuotaManager();

    nsresult rv = manager->Init(*gBaseDirPath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      gCreateFailed = true;
    } else {
      gInstance = manager;
    }
  }

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(aCallback));
}

// static
QuotaManager* QuotaManager::Get() {
  // Does not return an owning reference.
  return gInstance;
}

// static
bool QuotaManager::IsShuttingDown() { return gShutdown; }

// static
void QuotaManager::ShutdownInstance() {
  AssertIsOnBackgroundThread();

  if (gInstance) {
    gInstance->Shutdown();

    gInstance = nullptr;
  }

  RefPtr<Runnable> runnable =
      NS_NewRunnableFunction("dom::quota::QuotaManager::ShutdownCompleted",
                             []() { Observer::ShutdownCompleted(); });
  MOZ_ASSERT(runnable);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable.forget()));
}

// static
bool QuotaManager::IsOSMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(DSSTORE_FILE_NAME) ||
         aFileName.EqualsLiteral(DESKTOP_FILE_NAME) ||
         aFileName.LowerCaseEqualsLiteral(DESKTOP_INI_FILE_NAME) ||
         aFileName.LowerCaseEqualsLiteral(THUMBS_DB_FILE_NAME);
}

// static
bool QuotaManager::IsDotFile(const nsAString& aFileName) {
  return aFileName.First() == char16_t('.');
}

auto QuotaManager::CreateDirectoryLock(
    const Nullable<PersistenceType>& aPersistenceType, const nsACString& aGroup,
    const OriginScope& aOriginScope, const Nullable<Client::Type>& aClientType,
    bool aExclusive, bool aInternal, OpenDirectoryListener* aOpenListener)
    -> already_AddRefed<DirectoryLockImpl> {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(aOriginScope.IsOrigin(), !aOriginScope.GetOrigin().IsEmpty());
  MOZ_ASSERT_IF(!aInternal, !aPersistenceType.IsNull());
  MOZ_ASSERT_IF(!aInternal,
                aPersistenceType.Value() != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT_IF(!aInternal, !aGroup.IsEmpty());
  MOZ_ASSERT_IF(!aInternal, aOriginScope.IsOrigin());
  MOZ_ASSERT_IF(!aInternal, !aClientType.IsNull());
  MOZ_ASSERT_IF(!aInternal, aClientType.Value() < Client::TypeMax());
  MOZ_ASSERT_IF(!aInternal, aOpenListener);

  RefPtr<DirectoryLockImpl> lock =
      new DirectoryLockImpl(this, aPersistenceType, aGroup, aOriginScope,
                            aClientType, aExclusive, aInternal, aOpenListener);

  mPendingDirectoryLocks.AppendElement(lock);

  // See if this lock needs to wait.
  bool blocked = false;
  for (uint32_t index = mDirectoryLocks.Length(); index > 0; index--) {
    DirectoryLockImpl* existingLock = mDirectoryLocks[index - 1];
    if (lock->MustWaitFor(*existingLock)) {
      existingLock->AddBlockingLock(lock);
      lock->AddBlockedOnLock(existingLock);
      blocked = true;
    }
  }

  RegisterDirectoryLock(lock);

  // Otherwise, notify the open listener immediately.
  if (!blocked) {
    lock->NotifyOpenListener();
  }

  return lock.forget();
}

auto QuotaManager::CreateDirectoryLockForEviction(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin) -> already_AddRefed<DirectoryLockImpl> {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT(!aOrigin.IsEmpty());

  RefPtr<DirectoryLockImpl> lock = new DirectoryLockImpl(
      this, Nullable<PersistenceType>(aPersistenceType), aGroup,
      OriginScope::FromOrigin(aOrigin), Nullable<Client::Type>(),
      /* aExclusive */ true,
      /* aInternal */ true, nullptr);

#ifdef DEBUG
  for (uint32_t index = mDirectoryLocks.Length(); index > 0; index--) {
    DirectoryLockImpl* existingLock = mDirectoryLocks[index - 1];
    MOZ_ASSERT(!lock->MustWaitFor(*existingLock));
  }
#endif

  RegisterDirectoryLock(lock);

  return lock.forget();
}

void QuotaManager::RegisterDirectoryLock(DirectoryLockImpl* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);

  mDirectoryLocks.AppendElement(aLock);

  if (aLock->ShouldUpdateLockTable()) {
    const Nullable<PersistenceType>& persistenceType =
        aLock->GetPersistenceType();
    const OriginScope& originScope = aLock->GetOriginScope();

    MOZ_ASSERT(!persistenceType.IsNull());
    MOZ_ASSERT(!aLock->GetGroup().IsEmpty());
    MOZ_ASSERT(originScope.IsOrigin());
    MOZ_ASSERT(!originScope.GetOrigin().IsEmpty());

    DirectoryLockTable& directoryLockTable =
        GetDirectoryLockTable(persistenceType.Value());

    nsTArray<DirectoryLockImpl*>* array;
    if (!directoryLockTable.Get(originScope.GetOrigin(), &array)) {
      array = new nsTArray<DirectoryLockImpl*>();
      directoryLockTable.Put(originScope.GetOrigin(), array);

      if (!IsShuttingDown()) {
        UpdateOriginAccessTime(persistenceType.Value(), aLock->GetGroup(),
                               originScope.GetOrigin());
      }
    }
    array->AppendElement(aLock);
  }
}

void QuotaManager::UnregisterDirectoryLock(DirectoryLockImpl* aLock) {
  AssertIsOnOwningThread();

  MOZ_ALWAYS_TRUE(mDirectoryLocks.RemoveElement(aLock));

  if (aLock->ShouldUpdateLockTable()) {
    const Nullable<PersistenceType>& persistenceType =
        aLock->GetPersistenceType();
    const OriginScope& originScope = aLock->GetOriginScope();

    MOZ_ASSERT(!persistenceType.IsNull());
    MOZ_ASSERT(!aLock->GetGroup().IsEmpty());
    MOZ_ASSERT(originScope.IsOrigin());
    MOZ_ASSERT(!originScope.GetOrigin().IsEmpty());

    DirectoryLockTable& directoryLockTable =
        GetDirectoryLockTable(persistenceType.Value());

    nsTArray<DirectoryLockImpl*>* array;
    MOZ_ALWAYS_TRUE(directoryLockTable.Get(originScope.GetOrigin(), &array));

    MOZ_ALWAYS_TRUE(array->RemoveElement(aLock));
    if (array->IsEmpty()) {
      directoryLockTable.Remove(originScope.GetOrigin());

      if (!IsShuttingDown()) {
        UpdateOriginAccessTime(persistenceType.Value(), aLock->GetGroup(),
                               originScope.GetOrigin());
      }
    }
  }
}

void QuotaManager::RemovePendingDirectoryLock(DirectoryLockImpl* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);

  MOZ_ALWAYS_TRUE(mPendingDirectoryLocks.RemoveElement(aLock));
}

uint64_t QuotaManager::CollectOriginsForEviction(
    uint64_t aMinSizeToBeFreed, nsTArray<RefPtr<DirectoryLockImpl>>& aLocks) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLocks.IsEmpty());

  struct MOZ_STACK_CLASS Helper final {
    static void GetInactiveOriginInfos(
        nsTArray<RefPtr<OriginInfo>>& aOriginInfos,
        nsTArray<DirectoryLockImpl*>& aLocks,
        nsTArray<OriginInfo*>& aInactiveOriginInfos) {
      for (OriginInfo* originInfo : aOriginInfos) {
        MOZ_ASSERT(originInfo->mGroupInfo->mPersistenceType !=
                   PERSISTENCE_TYPE_PERSISTENT);

        if (originInfo->LockedPersisted()) {
          continue;
        }

        OriginScope originScope = OriginScope::FromOrigin(originInfo->mOrigin);

        bool match = false;
        for (uint32_t j = aLocks.Length(); j > 0; j--) {
          DirectoryLockImpl* lock = aLocks[j - 1];
          if (originScope.Matches(lock->GetOriginScope())) {
            match = true;
            break;
          }
        }

        if (!match) {
          MOZ_ASSERT(!originInfo->mQuotaObjects.Count(),
                     "Inactive origin shouldn't have open files!");
          aInactiveOriginInfos.InsertElementSorted(originInfo,
                                                   OriginInfoLRUComparator());
        }
      }
    }
  };

  // Split locks into separate arrays and filter out locks for persistent
  // storage, they can't block us.
  nsTArray<DirectoryLockImpl*> temporaryStorageLocks;
  nsTArray<DirectoryLockImpl*> defaultStorageLocks;
  for (DirectoryLockImpl* lock : mDirectoryLocks) {
    const Nullable<PersistenceType>& persistenceType =
        lock->GetPersistenceType();

    if (persistenceType.IsNull()) {
      temporaryStorageLocks.AppendElement(lock);
      defaultStorageLocks.AppendElement(lock);
    } else if (persistenceType.Value() == PERSISTENCE_TYPE_TEMPORARY) {
      temporaryStorageLocks.AppendElement(lock);
    } else if (persistenceType.Value() == PERSISTENCE_TYPE_DEFAULT) {
      defaultStorageLocks.AppendElement(lock);
    } else {
      MOZ_ASSERT(persistenceType.Value() == PERSISTENCE_TYPE_PERSISTENT);

      // Do nothing here, persistent origins don't need to be collected ever.
    }
  }

  nsTArray<OriginInfo*> inactiveOrigins;

  // Enumerate and process inactive origins. This must be protected by the
  // mutex.
  MutexAutoLock lock(mQuotaMutex);

  for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
    GroupInfoPair* pair = iter.UserData();

    MOZ_ASSERT(!iter.Key().IsEmpty());
    MOZ_ASSERT(pair);

    RefPtr<GroupInfo> groupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
    if (groupInfo) {
      Helper::GetInactiveOriginInfos(groupInfo->mOriginInfos,
                                     temporaryStorageLocks, inactiveOrigins);
    }

    groupInfo = pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (groupInfo) {
      Helper::GetInactiveOriginInfos(groupInfo->mOriginInfos,
                                     defaultStorageLocks, inactiveOrigins);
    }
  }

#ifdef DEBUG
  // Make sure the array is sorted correctly.
  for (uint32_t index = inactiveOrigins.Length(); index > 1; index--) {
    MOZ_ASSERT(inactiveOrigins[index - 1]->mAccessTime >=
               inactiveOrigins[index - 2]->mAccessTime);
  }
#endif

  // Create a list of inactive and the least recently used origins
  // whose aggregate size is greater or equals the minimal size to be freed.
  uint64_t sizeToBeFreed = 0;
  for (uint32_t count = inactiveOrigins.Length(), index = 0; index < count;
       index++) {
    if (sizeToBeFreed >= aMinSizeToBeFreed) {
      inactiveOrigins.TruncateLength(index);
      break;
    }

    sizeToBeFreed += inactiveOrigins[index]->mUsage;
  }

  if (sizeToBeFreed >= aMinSizeToBeFreed) {
    // Success, add directory locks for these origins, so any other
    // operations for them will be delayed (until origin eviction is finalized).

    for (OriginInfo* originInfo : inactiveOrigins) {
      RefPtr<DirectoryLockImpl> lock = CreateDirectoryLockForEviction(
          originInfo->mGroupInfo->mPersistenceType,
          originInfo->mGroupInfo->mGroup, originInfo->mOrigin);
      aLocks.AppendElement(lock.forget());
    }

    return sizeToBeFreed;
  }

  return 0;
}

template <typename P>
void QuotaManager::CollectPendingOriginsForListing(P aPredicate) {
  MutexAutoLock lock(mQuotaMutex);

  for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
    GroupInfoPair* pair = iter.UserData();

    MOZ_ASSERT(!iter.Key().IsEmpty());
    MOZ_ASSERT(pair);

    RefPtr<GroupInfo> groupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (groupInfo) {
      for (RefPtr<OriginInfo>& originInfo : groupInfo->mOriginInfos) {
        if (!originInfo->mDirectoryExists) {
          aPredicate(originInfo);
        }
      }
    }
  }
}

nsresult QuotaManager::Init(const nsAString& aBasePath) {
  mBasePath = aBasePath;

  nsCOMPtr<nsIFile> baseDir;
  nsresult rv = NS_NewLocalFile(aBasePath, false, getter_AddRefs(baseDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CloneStoragePath(baseDir, NS_LITERAL_STRING(INDEXEDDB_DIRECTORY_NAME),
                        mIndexedDBPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = baseDir->Append(NS_LITERAL_STRING(STORAGE_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = baseDir->GetPath(mStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CloneStoragePath(baseDir, NS_LITERAL_STRING(PERMANENT_DIRECTORY_NAME),
                        mPermanentStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CloneStoragePath(baseDir, NS_LITERAL_STRING(TEMPORARY_DIRECTORY_NAME),
                        mTemporaryStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CloneStoragePath(baseDir, NS_LITERAL_STRING(DEFAULT_DIRECTORY_NAME),
                        mDefaultStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = NS_NewNamedThread("QuotaManager IO", getter_AddRefs(mIOThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Make a timer here to avoid potential failures later. We don't actually
  // initialize the timer until shutdown.
  mShutdownTimer = NS_NewTimer();
  if (NS_WARN_IF(!mShutdownTimer)) {
    return NS_ERROR_FAILURE;
  }

  static_assert(Client::IDB == 0 && Client::DOMCACHE == 1 && Client::SDB == 2 &&
                    Client::LS == 3 && Client::TYPE_MAX == 4,
                "Fix the registration!");

  MOZ_ASSERT(mClients.Capacity() == Client::TYPE_MAX,
             "Should be using an auto array with correct capacity!");

  // Register clients.
  mClients.AppendElement(indexedDB::CreateQuotaClient());
  mClients.AppendElement(cache::CreateQuotaClient());
  mClients.AppendElement(simpledb::CreateQuotaClient());
  if (NextGenLocalStorageEnabled()) {
    mClients.AppendElement(localstorage::CreateQuotaClient());
  } else {
    mClients.SetLength(Client::TypeMax());
  }

  return NS_OK;
}

void QuotaManager::Shutdown() {
  AssertIsOnOwningThread();

  // Setting this flag prevents the service from being recreated and prevents
  // further storagess from being created.
  if (gShutdown.exchange(true)) {
    NS_ERROR("Shutdown more than once?!");
  }

  StopIdleMaintenance();

  // Kick off the shutdown timer.
  MOZ_ALWAYS_SUCCEEDS(mShutdownTimer->InitWithNamedFuncCallback(
      &ShutdownTimerCallback, this, DEFAULT_SHUTDOWN_TIMER_MS,
      nsITimer::TYPE_ONE_SHOT, "QuotaManager::ShutdownTimerCallback"));

  // Each client will spin the event loop while we wait on all the threads
  // to close. Our timer may fire during that loop.
  for (uint32_t index = 0; index < uint32_t(Client::TypeMax()); index++) {
    mClients[index]->ShutdownWorkThreads();
  }

  // Cancel the timer regardless of whether it actually fired.
  if (NS_FAILED(mShutdownTimer->Cancel())) {
    NS_WARNING("Failed to cancel shutdown timer!");
  }

  // NB: It's very important that runnable is destroyed on this thread
  // (i.e. after we join the IO thread) because we can't release the
  // QuotaManager on the IO thread. This should probably use
  // NewNonOwningRunnableMethod ...
  RefPtr<Runnable> runnable =
      NewRunnableMethod("dom::quota::QuotaManager::ReleaseIOThreadObjects",
                        this, &QuotaManager::ReleaseIOThreadObjects);
  MOZ_ASSERT(runnable);

  // Give clients a chance to cleanup IO thread only objects.
  if (NS_FAILED(mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Failed to dispatch runnable!");
  }

  // Make sure to join with our IO thread.
  if (NS_FAILED(mIOThread->Shutdown())) {
    NS_WARNING("Failed to shutdown IO thread!");
  }

  for (RefPtr<DirectoryLockImpl>& lock : mPendingDirectoryLocks) {
    lock->Invalidate();
  }
}

void QuotaManager::InitQuotaForOrigin(PersistenceType aPersistenceType,
                                      const nsACString& aGroup,
                                      const nsACString& aOrigin,
                                      uint64_t aUsageBytes, int64_t aAccessTime,
                                      bool aPersisted) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<GroupInfo> groupInfo =
      LockedGetOrCreateGroupInfo(aPersistenceType, aGroup);

  RefPtr<OriginInfo> originInfo =
      new OriginInfo(groupInfo, aOrigin, aUsageBytes, aAccessTime, aPersisted,
                     /* aDirectoryExists */ true);
  groupInfo->LockedAddOriginInfo(originInfo);
}

void QuotaManager::EnsureQuotaForOrigin(PersistenceType aPersistenceType,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<GroupInfo> groupInfo =
      LockedGetOrCreateGroupInfo(aPersistenceType, aGroup);

  RefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);
  if (!originInfo) {
    originInfo = new OriginInfo(
        groupInfo, aOrigin, /* aUsageBytes */ 0, /* aAccessTime */ PR_Now(),
        /* aPersisted */ false, /* aDirectoryExists */ false);
    groupInfo->LockedAddOriginInfo(originInfo);
  }
}

void QuotaManager::NoteOriginDirectoryCreated(PersistenceType aPersistenceType,
                                              const nsACString& aGroup,
                                              const nsACString& aOrigin,
                                              bool aPersisted,
                                              int64_t& aTimestamp) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  int64_t timestamp;

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<GroupInfo> groupInfo =
      LockedGetOrCreateGroupInfo(aPersistenceType, aGroup);

  RefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);
  if (originInfo) {
    originInfo->mPersisted = aPersisted;
    originInfo->mDirectoryExists = true;
    timestamp = originInfo->LockedAccessTime();
  } else {
    timestamp = PR_Now();
    RefPtr<OriginInfo> originInfo = new OriginInfo(
        groupInfo, aOrigin, /* aUsageBytes */ 0, /* aAccessTime */ timestamp,
        aPersisted, /* aDirectoryExists */ true);
    groupInfo->LockedAddOriginInfo(originInfo);
  }

  aTimestamp = timestamp;
}

void QuotaManager::DecreaseUsageForOrigin(PersistenceType aPersistenceType,
                                          const nsACString& aGroup,
                                          const nsACString& aOrigin,
                                          int64_t aSize) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aGroup, &pair)) {
    return;
  }

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    return;
  }

  RefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);
  if (originInfo) {
    originInfo->LockedDecreaseUsage(aSize);
  }
}

void QuotaManager::UpdateOriginAccessTime(PersistenceType aPersistenceType,
                                          const nsACString& aGroup,
                                          const nsACString& aOrigin) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aGroup, &pair)) {
    return;
  }

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    return;
  }

  RefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);
  if (originInfo) {
    int64_t timestamp = PR_Now();
    originInfo->LockedUpdateAccessTime(timestamp);

    MutexAutoUnlock autoUnlock(mQuotaMutex);

    RefPtr<SaveOriginAccessTimeOp> op =
        new SaveOriginAccessTimeOp(aPersistenceType, aOrigin, timestamp);

    op->RunImmediately();
  }
}

void QuotaManager::RemoveQuota() {
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<GroupInfoPair>& pair = iter.Data();

    MOZ_ASSERT(!iter.Key().IsEmpty(), "Empty key!");
    MOZ_ASSERT(pair, "Null pointer!");

    RefPtr<GroupInfo> groupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
    if (groupInfo) {
      groupInfo->LockedRemoveOriginInfos();
    }

    groupInfo = pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (groupInfo) {
      groupInfo->LockedRemoveOriginInfos();
    }

    iter.Remove();
  }

  NS_ASSERTION(mTemporaryStorageUsage == 0, "Should be zero!");
}

already_AddRefed<QuotaObject> QuotaManager::GetQuotaObject(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, nsIFile* aFile, int64_t aFileSize,
    int64_t* aFileSizeOut /* = nullptr */) {
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (aFileSizeOut) {
    *aFileSizeOut = 0;
  }

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    return nullptr;
  }

  nsString path;
  nsresult rv = aFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, nullptr);

  int64_t fileSize;

  if (aFileSize == -1) {
    bool exists;
    rv = aFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (exists) {
      rv = aFile->GetFileSize(&fileSize);
      NS_ENSURE_SUCCESS(rv, nullptr);
    } else {
      fileSize = 0;
    }
  } else {
    fileSize = aFileSize;
  }

  RefPtr<QuotaObject> result;
  {
    MutexAutoLock lock(mQuotaMutex);

    GroupInfoPair* pair;
    if (!mGroupInfoPairs.Get(aGroup, &pair)) {
      return nullptr;
    }

    RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);

    if (!groupInfo) {
      return nullptr;
    }

    RefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);

    if (!originInfo) {
      return nullptr;
    }

    // We need this extra raw pointer because we can't assign to the smart
    // pointer directly since QuotaObject::AddRef would try to acquire the same
    // mutex.
    QuotaObject* quotaObject;
    if (!originInfo->mQuotaObjects.Get(path, &quotaObject)) {
      // Create a new QuotaObject.
      quotaObject = new QuotaObject(originInfo, path, fileSize);

      // Put it to the hashtable. The hashtable is not responsible to delete
      // the QuotaObject.
      originInfo->mQuotaObjects.Put(path, quotaObject);
    }

    // Addref the QuotaObject and move the ownership to the result. This must
    // happen before we unlock!
    result = quotaObject->LockedAddRef();
  }

  if (aFileSizeOut) {
    *aFileSizeOut = fileSize;
  }

  // The caller becomes the owner of the QuotaObject, that is, the caller is
  // is responsible to delete it when the last reference is removed.
  return result.forget();
}

already_AddRefed<QuotaObject> QuotaManager::GetQuotaObject(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, const nsAString& aPath, int64_t aFileSize,
    int64_t* aFileSizeOut /* = nullptr */) {
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (aFileSizeOut) {
    *aFileSizeOut = 0;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(aPath, false, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return GetQuotaObject(aPersistenceType, aGroup, aOrigin, file, aFileSize,
                        aFileSizeOut);
}

Nullable<bool> QuotaManager::OriginPersisted(const nsACString& aGroup,
                                             const nsACString& aOrigin) {
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<OriginInfo> originInfo =
      LockedGetOriginInfo(PERSISTENCE_TYPE_DEFAULT, aGroup, aOrigin);
  if (originInfo) {
    return Nullable<bool>(originInfo->LockedPersisted());
  }

  return Nullable<bool>();
}

void QuotaManager::PersistOrigin(const nsACString& aGroup,
                                 const nsACString& aOrigin) {
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<OriginInfo> originInfo =
      LockedGetOriginInfo(PERSISTENCE_TYPE_DEFAULT, aGroup, aOrigin);
  if (originInfo && !originInfo->LockedPersisted()) {
    originInfo->LockedPersist();
  }
}

void QuotaManager::AbortOperationsForProcess(ContentParentId aContentParentId) {
  AssertIsOnOwningThread();

  for (RefPtr<Client>& client : mClients) {
    client->AbortOperationsForProcess(aContentParentId);
  }
}

nsresult QuotaManager::GetDirectoryForOrigin(PersistenceType aPersistenceType,
                                             const nsACString& aASCIIOrigin,
                                             nsIFile** aDirectory) const {
  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(GetStoragePath(aPersistenceType), false,
                                getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString originSanitized(aASCIIOrigin);
  SanitizeOriginString(originSanitized);

  rv = directory->Append(NS_ConvertASCIItoUTF16(originSanitized));
  NS_ENSURE_SUCCESS(rv, rv);

  directory.forget(aDirectory);
  return NS_OK;
}

nsresult QuotaManager::RestoreDirectoryMetadata2(nsIFile* aDirectory,
                                                 bool aPersistent) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(mStorageInitialized);

  RefPtr<RestoreDirectoryMetadata2Helper> helper =
      new RestoreDirectoryMetadata2Helper(aDirectory, aPersistent);

  nsresult rv = helper->RestoreMetadata2File();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::GetDirectoryMetadata2(
    nsIFile* aDirectory, int64_t* aTimestamp, bool* aPersisted,
    nsACString& aSuffix, nsACString& aGroup, nsACString& aOrigin) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp);
  MOZ_ASSERT(aPersisted);
  MOZ_ASSERT(mStorageInitialized);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv =
      GetBinaryInputStream(aDirectory, NS_LITERAL_STRING(METADATA_V2_FILE_NAME),
                           getter_AddRefs(binaryStream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  NS_ENSURE_SUCCESS(rv, rv);

  bool persisted;
  rv = binaryStream->ReadBoolean(&persisted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t reservedData1;
  rv = binaryStream->Read32(&reservedData1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t reservedData2;
  rv = binaryStream->Read32(&reservedData2);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString suffix;
  rv = binaryStream->ReadCString(suffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString group;
  rv = binaryStream->ReadCString(group);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString origin;
  rv = binaryStream->ReadCString(origin);
  NS_ENSURE_SUCCESS(rv, rv);

  // Currently unused (used to be isApp).
  bool dummy;
  rv = binaryStream->ReadBoolean(&dummy);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = binaryStream->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!origin.EqualsLiteral(kChromeOrigin)) {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    if (NS_WARN_IF(
            !originAttributes.PopulateFromOrigin(origin, originNoSuffix))) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<MozURL> url;
    rv = MozURL::Init(getter_AddRefs(url), originNoSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      QM_WARNING("A URL %s is not recognized by MozURL", originNoSuffix.get());
      return rv;
    }

    nsCString baseDomain;
    rv = url->BaseDomain(baseDomain);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString upToDateGroup = baseDomain + suffix;

    if (group != upToDateGroup) {
      group = upToDateGroup;

      // Only creating .metadata-v2 to reduce IO.
      rv = CreateDirectoryMetadata2(aDirectory, timestamp, persisted, suffix,
                                    group, origin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      ContentPrincipalInfo contentPrincipalInfo;
      contentPrincipalInfo.attrs() = originAttributes;
      contentPrincipalInfo.originNoSuffix() = originNoSuffix;
      contentPrincipalInfo.spec() = originNoSuffix;
      contentPrincipalInfo.baseDomain() = baseDomain;

      PrincipalInfo principalInfo(contentPrincipalInfo);

      nsTArray<PrincipalInfo> principalInfos;
      principalInfos.AppendElement(principalInfo);

      RefPtr<PrincipalVerifier> principalVerifier =
          PrincipalVerifier::CreateAndDispatch(std::move(principalInfos));
#endif
    }
  }

  *aTimestamp = timestamp;
  *aPersisted = persisted;
  aSuffix = suffix;
  aGroup = group;
  aOrigin = origin;
  return NS_OK;
}

nsresult QuotaManager::GetDirectoryMetadata2WithRestore(
    nsIFile* aDirectory, bool aPersistent, int64_t* aTimestamp,
    bool* aPersisted, nsACString& aSuffix, nsACString& aGroup,
    nsACString& aOrigin, const bool aTelemetry) {
  nsresult rv = GetDirectoryMetadata2(aDirectory, aTimestamp, aPersisted,
                                      aSuffix, aGroup, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = RestoreDirectoryMetadata2(aDirectory, aPersistent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      if (aTelemetry) {
        REPORT_TELEMETRY_INIT_ERR(kInternalError, Rep_RestoreDirMeta);
      }
      return rv;
    }

    rv = GetDirectoryMetadata2(aDirectory, aTimestamp, aPersisted, aSuffix,
                               aGroup, aOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      if (aTelemetry) {
        REPORT_TELEMETRY_INIT_ERR(kExternalError, Rep_GetDirMeta);
      }
      return rv;
    }
  }

  return NS_OK;
}

nsresult QuotaManager::GetDirectoryMetadata2(nsIFile* aDirectory,
                                             int64_t* aTimestamp,
                                             bool* aPersisted) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp != nullptr || aPersisted != nullptr);
  MOZ_ASSERT(mStorageInitialized);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv =
      GetBinaryInputStream(aDirectory, NS_LITERAL_STRING(METADATA_V2_FILE_NAME),
                           getter_AddRefs(binaryStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool persisted;
  if (aPersisted != nullptr) {
    rv = binaryStream->ReadBoolean(&persisted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aTimestamp != nullptr) {
    *aTimestamp = timestamp;
  }
  if (aPersisted != nullptr) {
    *aPersisted = persisted;
  }
  return NS_OK;
}

nsresult QuotaManager::GetDirectoryMetadata2WithRestore(nsIFile* aDirectory,
                                                        bool aPersistent,
                                                        int64_t* aTimestamp,
                                                        bool* aPersisted) {
  nsresult rv = GetDirectoryMetadata2(aDirectory, aTimestamp, aPersisted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = RestoreDirectoryMetadata2(aDirectory, aPersistent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = GetDirectoryMetadata2(aDirectory, aTimestamp, aPersisted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult QuotaManager::InitializeRepository(PersistenceType aPersistenceType) {
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_TEMPORARY ||
             aPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(GetStoragePath(aPersistenceType), false,
                                getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kExternalError, Rep_NewLocalFile);
    return rv;
  }

  bool created;
  rv = EnsureDirectory(directory, &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kExternalError, Rep_EnsureDirectory);
    return rv;
  }

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kExternalError, Rep_GetDirEntries);
    return rv;
  }

  // A keeper to defer the return only in Nightly, so that the telemetry data
  // for whole profile can be collected
#ifdef NIGHTLY_BUILD
  nsresult statusKeeper = NS_OK;
#endif

  nsCOMPtr<nsIFile> childDirectory;
  while (NS_SUCCEEDED(
             (rv = entries->GetNextFile(getter_AddRefs(childDirectory)))) &&
         childDirectory) {
    if (NS_WARN_IF(IsShuttingDown())) {
      RETURN_STATUS_OR_RESULT(statusKeeper, NS_ERROR_FAILURE);
    }

    bool isDirectory;
    rv = childDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kExternalError, Rep_IsDirectory);
      RECORD_IN_NIGHTLY(statusKeeper, rv);
      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
    }

    if (!isDirectory) {
      nsString leafName;
      rv = childDirectory->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kExternalError, Rep_GetLeafName);
        RECORD_IN_NIGHTLY(statusKeeper, rv);
        CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
      }

      if (IsOSMetadata(leafName) || IsDotFile(leafName)) {
        continue;
      }

      UNKNOWN_FILE_WARNING(leafName);

      REPORT_TELEMETRY_INIT_ERR(kInternalError, Rep_UnexpectedFile);
      RECORD_IN_NIGHTLY(statusKeeper, NS_ERROR_UNEXPECTED);
      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(NS_ERROR_UNEXPECTED);
    }

    int64_t timestamp;
    bool persisted;
    nsCString suffix;
    nsCString group;
    nsCString origin;
    rv = GetDirectoryMetadata2WithRestore(childDirectory,
                                          /* aPersistent */ false, &timestamp,
                                          &persisted, suffix, group, origin,
                                          /* aTelemetry */ true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // Error should have reported in GetDirectoryMetadata2WithRestore
      RECORD_IN_NIGHTLY(statusKeeper, rv);
      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
    }

    rv = InitializeOrigin(aPersistenceType, group, origin, timestamp, persisted,
                          childDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // Error should have reported in InitializeOrigin
      RECORD_IN_NIGHTLY(statusKeeper, rv);
      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kInternalError, Rep_GetNextFile);
    RECORD_IN_NIGHTLY(statusKeeper, rv);
#ifndef NIGHTLY_BUILD
    return rv;
#endif
  }

#ifdef NIGHTLY_BUILD
  if (NS_FAILED(statusKeeper)) {
    return statusKeeper;
  }
#endif

  return NS_OK;
}

nsresult QuotaManager::InitializeOrigin(PersistenceType aPersistenceType,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin,
                                        int64_t aAccessTime, bool aPersisted,
                                        nsIFile* aDirectory) {
  AssertIsOnIOThread();

  nsresult rv;

  bool trackQuota = aPersistenceType != PERSISTENCE_TYPE_PERSISTENT;

  // We need to initialize directories of all clients if they exists and also
  // get the total usage to initialize the quota.
  nsAutoPtr<UsageInfo> usageInfo;
  if (trackQuota) {
    usageInfo = new UsageInfo();
  }

  // A keeper to defer the return only in Nightly, so that the telemetry data
  // for whole profile can be collected
#ifdef NIGHTLY_BUILD
  nsresult statusKeeper = NS_OK;
#endif

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kExternalError, Ori_GetDirEntries);
    return rv;
  }

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(file)))) &&
         file) {
    if (NS_WARN_IF(IsShuttingDown())) {
      RETURN_STATUS_OR_RESULT(statusKeeper, NS_ERROR_FAILURE);
    }

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kExternalError, Ori_IsDirectory);
      RECORD_IN_NIGHTLY(statusKeeper, rv);
      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kExternalError, Ori_GetLeafName);
      RECORD_IN_NIGHTLY(statusKeeper, rv);
      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
    }

    if (!isDirectory) {
      if (IsOriginMetadata(leafName)) {
        continue;
      }

      if (IsTempMetadata(leafName)) {
        rv = file->Remove(/* recursive */ false);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          REPORT_TELEMETRY_INIT_ERR(kExternalError, Ori_Remove);
          RECORD_IN_NIGHTLY(statusKeeper, rv);
          CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
        }

        continue;
      }

      if (IsOSMetadata(leafName) || IsDotFile(leafName)) {
        continue;
      }

      UNKNOWN_FILE_WARNING(leafName);
      REPORT_TELEMETRY_INIT_ERR(kInternalError, Ori_UnexpectedFile);
      RECORD_IN_NIGHTLY(statusKeeper, NS_ERROR_UNEXPECTED);
      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(NS_ERROR_UNEXPECTED);
    }

    Client::Type clientType;
    rv = Client::TypeFromText(leafName, clientType);
    if (NS_FAILED(rv)) {
      UNKNOWN_FILE_WARNING(leafName);
      REPORT_TELEMETRY_INIT_ERR(kInternalError, Ori_UnexpectedClient);
      RECORD_IN_NIGHTLY(statusKeeper, NS_ERROR_UNEXPECTED);

      // Our upgrade process should have attempted to delete the deprecated
      // client directory and failed to upgrade if it could not be deleted. So
      // if we're here, either a) there's a bug in our code or b) a user copied
      // over parts of an old profile into a new profile for some reason and the
      // upgrade process won't be run again to fix it. If it's a bug, we want to
      // assert, but only on nightly where the bug would have been introduced
      // and we can do something about it. If it's the user, it's best for us to
      // try and delete the origin and/or mark it broken, so we do that for
      // non-nightly builds by trying to delete the deprecated client directory
      // and return the initialization error for the origin.
      if (Client::IsDeprecatedClient(leafName)) {
        rv = file->Remove(true);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
        }

        MOZ_DIAGNOSTIC_ASSERT(true, "Found a deprecated client");
      }

      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(NS_ERROR_UNEXPECTED);
    }

    Atomic<bool> dummy(false);
    rv = mClients[clientType]->InitOrigin(aPersistenceType, aGroup, aOrigin,
                                          /* aCanceled */ dummy, usageInfo,
                                          /* aForGetUsage */ false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // error should have reported in InitOrigin
      RECORD_IN_NIGHTLY(statusKeeper, rv);
      CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(rv);
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kInternalError, Ori_GetNextFile);
    RECORD_IN_NIGHTLY(statusKeeper, rv);
#ifndef NIGHTLY_BUILD
    return rv;
#endif
  }

#ifdef NIGHTLY_BUILD
  if (NS_FAILED(statusKeeper)) {
    return statusKeeper;
  }
#endif

  if (trackQuota) {
    InitQuotaForOrigin(aPersistenceType, aGroup, aOrigin,
                       usageInfo->TotalUsage(), aAccessTime, aPersisted);
  }

  return NS_OK;
}

nsresult QuotaManager::MaybeUpgradeIndexedDBDirectory() {
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> indexedDBDir;
  nsresult rv =
      NS_NewLocalFile(mIndexedDBPath, false, getter_AddRefs(indexedDBDir));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = indexedDBDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    // Nothing to upgrade.
    return NS_OK;
  }

  bool isDirectory;
  rv = indexedDBDir->IsDirectory(&isDirectory);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isDirectory) {
    NS_WARNING("indexedDB entry is not a directory!");
    return NS_OK;
  }

  nsCOMPtr<nsIFile> persistentStorageDir;
  rv = NS_NewLocalFile(mStoragePath, false,
                       getter_AddRefs(persistentStorageDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = persistentStorageDir->Append(
      NS_LITERAL_STRING(PERSISTENT_DIRECTORY_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = persistentStorageDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    NS_WARNING("indexedDB directory shouldn't exist after the upgrade!");
    return NS_OK;
  }

  nsCOMPtr<nsIFile> storageDir;
  rv = persistentStorageDir->GetParent(getter_AddRefs(storageDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // MoveTo() is atomic if the move happens on the same volume which should
  // be our case, so even if we crash in the middle of the operation nothing
  // breaks next time we try to initialize.
  // However there's a theoretical possibility that the indexedDB directory
  // is on different volume, but it should be rare enough that we don't have
  // to worry about it.
  rv = indexedDBDir->MoveTo(storageDir,
                            NS_LITERAL_STRING(PERSISTENT_DIRECTORY_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult QuotaManager::MaybeUpgradePersistentStorageDirectory() {
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> persistentStorageDir;
  nsresult rv = NS_NewLocalFile(mStoragePath, false,
                                getter_AddRefs(persistentStorageDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = persistentStorageDir->Append(
      NS_LITERAL_STRING(PERSISTENT_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = persistentStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    // Nothing to upgrade.
    return NS_OK;
  }

  bool isDirectory;
  rv = persistentStorageDir->IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isDirectory) {
    NS_WARNING("persistent entry is not a directory!");
    return NS_OK;
  }

  nsCOMPtr<nsIFile> defaultStorageDir;
  rv = NS_NewLocalFile(mDefaultStoragePath, false,
                       getter_AddRefs(defaultStorageDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = defaultStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    NS_WARNING("storage/persistent shouldn't exist after the upgrade!");
    return NS_OK;
  }

  // Create real metadata files for origin directories in persistent storage.
  RefPtr<CreateOrUpgradeDirectoryMetadataHelper> helper =
      new CreateOrUpgradeDirectoryMetadataHelper(persistentStorageDir,
                                                 /* aPersistent */ true);

  rv = helper->ProcessRepository();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Upgrade metadata files for origin directories in temporary storage.
  nsCOMPtr<nsIFile> temporaryStorageDir;
  rv = NS_NewLocalFile(mTemporaryStoragePath, false,
                       getter_AddRefs(temporaryStorageDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = temporaryStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = temporaryStorageDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      NS_WARNING("temporary entry is not a directory!");
      return NS_OK;
    }

    helper =
        new CreateOrUpgradeDirectoryMetadataHelper(temporaryStorageDir,
                                                   /* aPersistent */ false);

    rv = helper->ProcessRepository();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // And finally rename persistent to default.
  rv = persistentStorageDir->RenameTo(
      nullptr, NS_LITERAL_STRING(DEFAULT_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::MaybeRemoveOldDirectories() {
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> indexedDBDir;
  nsresult rv =
      NS_NewLocalFile(mIndexedDBPath, false, getter_AddRefs(indexedDBDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = indexedDBDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    QM_WARNING("Deleting old <profile>/indexedDB directory!");

    rv = indexedDBDir->Remove(/* aRecursive */ true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIFile> persistentStorageDir;
  rv = NS_NewLocalFile(mStoragePath, false,
                       getter_AddRefs(persistentStorageDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = persistentStorageDir->Append(
      NS_LITERAL_STRING(PERSISTENT_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = persistentStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    QM_WARNING("Deleting old <profile>/storage/persistent directory!");

    rv = persistentStorageDir->Remove(/* aRecursive */ true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

template <typename Helper>
nsresult QuotaManager::UpgradeStorage(const int32_t aOldVersion,
                                      const int32_t aNewVersion,
                                      mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aNewVersion > aOldVersion);
  MOZ_ASSERT(aNewVersion <= kStorageVersion);
  MOZ_ASSERT(aConnection);

  nsresult rv;

  for (const PersistenceType persistenceType : kAllPersistenceTypes) {
    nsCOMPtr<nsIFile> directory;
    rv = NS_NewLocalFile(GetStoragePath(persistenceType), false,
                         getter_AddRefs(directory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool exists;
    rv = directory->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!exists) {
      continue;
    }

    bool persistent = persistenceType == PERSISTENCE_TYPE_PERSISTENT;
    RefPtr<RepositoryOperationBase> helper = new Helper(directory, persistent);
    rv = helper->ProcessRepository();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

#ifdef DEBUG
  {
    int32_t storageVersion;
    rv = aConnection->GetSchemaVersion(&storageVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(storageVersion == aOldVersion);
  }
#endif

  rv = aConnection->SetSchemaVersion(aNewVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::UpgradeStorageFrom0_0To1_0(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = MaybeUpgradeIndexedDBDirectory();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = MaybeUpgradePersistentStorageDirectory();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = MaybeRemoveOldDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = UpgradeStorage<UpgradeStorageFrom0_0To1_0Helper>(
      0, MakeStorageVersion(1, 0), aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::UpgradeStorageFrom1_0To2_0(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The upgrade consists of a number of logically distinct bugs that
  // intentionally got fixed at the same time to trigger just one major
  // version bump.
  //
  //
  // Morgue directory cleanup
  // [Feature/Bug]:
  // The original bug that added "on demand" morgue cleanup is 1165119.
  //
  // [Mutations]:
  // Morgue directories are removed from all origin directories during the
  // upgrade process. Origin initialization and usage calculation doesn't try
  // to remove morgue directories anymore.
  //
  // [Downgrade-incompatible changes]:
  // Morgue directories can reappear if user runs an already upgraded profile
  // in an older version of Firefox. Morgue directories then prevent current
  // Firefox from initializing and using the storage.
  //
  //
  // App data removal
  // [Feature/Bug]:
  // The bug that removes isApp flags is 1311057.
  //
  // [Mutations]:
  // Origin directories with appIds are removed during the upgrade process.
  //
  // [Downgrade-incompatible changes]:
  // Origin directories with appIds can reappear if user runs an already
  // upgraded profile in an older version of Firefox. Origin directories with
  // appIds don't prevent current Firefox from initializing and using the
  // storage, but they wouldn't ever be removed again, potentially causing
  // problems once appId is removed from origin attributes.
  //
  //
  // Strip obsolete origin attributes
  // [Feature/Bug]:
  // The bug that strips obsolete origin attributes is 1314361.
  //
  // [Mutations]:
  // Origin directories with obsolete origin attributes are renamed and their
  // metadata files are updated during the upgrade process.
  //
  // [Downgrade-incompatible changes]:
  // Origin directories with obsolete origin attributes can reappear if user
  // runs an already upgraded profile in an older version of Firefox. Origin
  // directories with obsolete origin attributes don't prevent current Firefox
  // from initializing and using the storage, but they wouldn't ever be upgraded
  // again, potentially causing problems in future.
  //
  //
  // File manager directory renaming (client specific)
  // [Feature/Bug]:
  // The original bug that added "on demand" file manager directory renaming is
  // 1056939.
  //
  // [Mutations]:
  // All file manager directories are renamed to contain the ".files" suffix.
  //
  // [Downgrade-incompatible changes]:
  // File manager directories with the ".files" suffix prevent older versions of
  // Firefox from initializing and using the storage.
  // File manager directories without the ".files" suffix can appear if user
  // runs an already upgraded profile in an older version of Firefox. File
  // manager directories without the ".files" suffix then prevent current
  // Firefox from initializing and using the storage.

  nsresult rv = UpgradeStorage<UpgradeStorageFrom1_0To2_0Helper>(
      MakeStorageVersion(1, 0), MakeStorageVersion(2, 0), aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::UpgradeStorageFrom2_0To2_1(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The upgrade is mainly to create a directory padding file in DOM Cache
  // directory to record the overall padding size of an origin.

  nsresult rv = UpgradeStorage<UpgradeStorageFrom2_0To2_1Helper>(
      MakeStorageVersion(2, 0), MakeStorageVersion(2, 1), aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::UpgradeStorageFrom2_1To2_2(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The upgrade is mainly to clean obsolete origins in the repositoies, remove
  // asmjs client, and ".tmp" file in the idb folers.

  nsresult rv = UpgradeStorage<UpgradeStorageFrom2_1To2_2Helper>(
      MakeStorageVersion(2, 1), MakeStorageVersion(2, 2), aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::MaybeRemoveLocalStorageData() {
  AssertIsOnIOThread();
  MOZ_ASSERT(!CachedNextGenLocalStorageEnabled());

  // Cleanup the tmp file first, if there's any.
  nsCOMPtr<nsIFile> lsArchiveTmpFile;
  nsresult rv = GetLocalStorageArchiveTmpFile(mStoragePath,
                                              getter_AddRefs(lsArchiveTmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = lsArchiveTmpFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = lsArchiveTmpFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Now check the real archive file.
  nsCOMPtr<nsIFile> lsArchiveFile;
  rv = GetLocalStorageArchiveFile(mStoragePath, getter_AddRefs(lsArchiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = lsArchiveFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    // If the ls archive doesn't exist then ls directories can't exist either.
    return NS_OK;
  }

  rv = MaybeRemoveLocalStorageDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Finally remove the ls archive, so we don't have to check all origin
  // directories next time this method is called.
  rv = lsArchiveFile->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::MaybeRemoveLocalStorageDirectories() {
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> defaultStorageDir;
  nsresult rv = NS_NewLocalFile(mDefaultStoragePath, false,
                                getter_AddRefs(defaultStorageDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = defaultStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = defaultStorageDir->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!entries) {
    return NS_OK;
  }

  while (true) {
    bool hasMore;
    rv = entries->HasMoreElements(&hasMore);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!hasMore) {
      break;
    }

    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> originDir = do_QueryInterface(entry);
    MOZ_ASSERT(originDir);

    rv = originDir->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(exists);

    bool isDirectory;
    rv = originDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      nsString leafName;
      rv = originDir->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Unknown files during upgrade are allowed. Just warn if we find them.
      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }

      continue;
    }

    nsCOMPtr<nsIFile> lsDir;
    rv = originDir->Clone(getter_AddRefs(lsDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = lsDir->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = lsDir->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!exists) {
      continue;
    }

    rv = lsDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      QM_WARNING("ls entry is not a directory!");

      continue;
    }

    nsString path;
    rv = lsDir->GetPath(path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    QM_WARNING("Deleting %s directory!", NS_ConvertUTF16toUTF8(path).get());

    rv = lsDir->Remove(/* aRecursive */ true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult QuotaManager::CreateLocalStorageArchiveConnectionFromWebAppsStore(
    mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> lsArchiveFile;
  nsresult rv =
      GetLocalStorageArchiveFile(mStoragePath, getter_AddRefs(lsArchiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  bool exists;
  rv = lsArchiveFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(!exists);
#endif

  // Get the storage service first, we will need it at multiple places.
  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Get the web apps store file.
  nsCOMPtr<nsIFile> webAppsStoreFile;
  rv = NS_NewLocalFile(mBasePath, false, getter_AddRefs(webAppsStoreFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = webAppsStoreFile->Append(NS_LITERAL_STRING(WEB_APPS_STORE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now check if the web apps store is useable.
  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateWebAppsStoreConnection(webAppsStoreFile, ss,
                                    getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (connection) {
    // Find out the journal mode.
    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(NS_LITERAL_CSTRING("PRAGMA journal_mode;"),
                                     getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(hasResult);

    nsCString journalMode;
    rv = stmt->GetUTF8String(0, journalMode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Finalize();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (journalMode.EqualsLiteral("wal")) {
      // We don't copy the WAL file, so make sure the old database is fully
      // checkpointed.
      rv = connection->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("PRAGMA wal_checkpoint(TRUNCATE);"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // Explicitely close the connection before the old database is copied.
    rv = connection->Close();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Copy the old database. The database is copied from
    // <profile>/webappsstore.sqlite to
    // <profile>/storage/ls-archive-tmp.sqlite
    // We use a "-tmp" postfix since we are not done yet.
    nsCOMPtr<nsIFile> storageDir;
    rv = NS_NewLocalFile(mStoragePath, false, getter_AddRefs(storageDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = webAppsStoreFile->CopyTo(storageDir,
                                  NS_LITERAL_STRING(LS_ARCHIVE_TMP_FILE_NAME));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> lsArchiveTmpFile;
    rv = GetLocalStorageArchiveTmpFile(mStoragePath,
                                       getter_AddRefs(lsArchiveTmpFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (journalMode.EqualsLiteral("wal")) {
      nsCOMPtr<mozIStorageConnection> lsArchiveTmpConnection;
      rv = ss->OpenUnsharedDatabase(lsArchiveTmpFile,
                                    getter_AddRefs(lsArchiveTmpConnection));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // The archive will only be used for lazy data migration. There won't be
      // any concurrent readers and writers that could benefit from Write-Ahead
      // Logging. So switch to a standard rollback journal. The standard
      // rollback journal also provides atomicity across multiple attached
      // databases which is import for the lazy data migration to work safely.
      rv = lsArchiveTmpConnection->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("PRAGMA journal_mode = DELETE;"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // The connection will be now implicitely closed (it's always safer to
      // close database connection before we manipulate underlying file)
    }

    // Finally, rename ls-archive-tmp.sqlite to ls-archive.sqlite
    rv = lsArchiveTmpFile->MoveTo(nullptr,
                                  NS_LITERAL_STRING(LS_ARCHIVE_FILE_NAME));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<mozIStorageConnection> lsArchiveConnection;
    rv = ss->OpenUnsharedDatabase(lsArchiveFile,
                                  getter_AddRefs(lsArchiveConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    lsArchiveConnection.forget(aConnection);
  } else {
    // If webappsstore database is not useable, just create an empty archive.

    // Ensure the storage directory actually exists.
    nsCOMPtr<nsIFile> storageDirectory;
    rv = NS_NewLocalFile(GetStoragePath(), false,
                         getter_AddRefs(storageDirectory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool dummy;
    rv = EnsureDirectory(storageDirectory, &dummy);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<mozIStorageConnection> lsArchiveConnection;
    rv = ss->OpenUnsharedDatabase(lsArchiveFile,
                                  getter_AddRefs(lsArchiveConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = StorageDBUpdater::Update(lsArchiveConnection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    lsArchiveConnection.forget(aConnection);
  }

  return NS_OK;
}

nsresult QuotaManager::CreateLocalStorageArchiveConnection(
    mozIStorageConnection** aConnection, bool& aNewlyCreated) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> lsArchiveTmpFile;
  nsresult rv = GetLocalStorageArchiveTmpFile(mStoragePath,
                                              getter_AddRefs(lsArchiveTmpFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = lsArchiveTmpFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = lsArchiveTmpFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Check if the archive was already successfully created.
  nsCOMPtr<nsIFile> lsArchiveFile;
  rv = GetLocalStorageArchiveFile(mStoragePath, getter_AddRefs(lsArchiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = lsArchiveFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool removed = false;

    bool isDirectory;
    rv = lsArchiveFile->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (isDirectory) {
      rv = lsArchiveFile->Remove(true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      removed = true;
    }

    nsCOMPtr<mozIStorageService> ss =
        do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<mozIStorageConnection> connection;
    rv = ss->OpenUnsharedDatabase(lsArchiveFile, getter_AddRefs(connection));
    if (!removed && rv == NS_ERROR_FILE_CORRUPTED) {
      rv = lsArchiveFile->Remove(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      removed = true;

      rv = ss->OpenUnsharedDatabase(lsArchiveFile, getter_AddRefs(connection));
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = StorageDBUpdater::Update(connection);
    if (!removed && NS_FAILED(rv)) {
      rv = connection->Close();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = lsArchiveFile->Remove(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      removed = true;

      rv = ss->OpenUnsharedDatabase(lsArchiveFile, getter_AddRefs(connection));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = StorageDBUpdater::Update(connection);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    connection.forget(aConnection);
    aNewlyCreated = removed;
    return NS_OK;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateLocalStorageArchiveConnectionFromWebAppsStore(
      getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  aNewlyCreated = true;
  return NS_OK;
}

nsresult QuotaManager::RecreateLocalStorageArchive(
    nsCOMPtr<mozIStorageConnection>& aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  // Close local storage archive connection. We are going to remove underlying
  // file.
  nsresult rv = aConnection->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = MaybeRemoveLocalStorageDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> lsArchiveFile;
  rv = GetLocalStorageArchiveFile(mStoragePath, getter_AddRefs(lsArchiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  bool exists;
  rv = lsArchiveFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(exists);
#endif

  rv = lsArchiveFile->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateLocalStorageArchiveConnectionFromWebAppsStore(
      getter_AddRefs(aConnection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::DowngradeLocalStorageArchive(
    nsCOMPtr<mozIStorageConnection>& aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  nsresult rv = RecreateLocalStorageArchive(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InitializeLocalStorageArchive(aConnection, kLocalStorageArchiveVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::UpgradeLocalStorageArchiveFromLessThan4To4(
    nsCOMPtr<mozIStorageConnection>& aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  nsresult rv = RecreateLocalStorageArchive(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InitializeLocalStorageArchive(aConnection, 4);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/*
nsresult QuotaManager::UpgradeLocalStorageArchiveFrom4To5(
    nsCOMPtr<mozIStorageConnection>& aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  nsresult rv = SaveLocalStorageArchiveVersion(aConnection, 5);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}
*/

#ifdef DEBUG

void QuotaManager::AssertStorageIsInitialized() const {
  AssertIsOnIOThread();
  MOZ_ASSERT(mStorageInitialized);
}

#endif  // DEBUG

nsresult QuotaManager::EnsureStorageIsInitialized() {
  AssertIsOnIOThread();

  if (mStorageInitialized) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> storageFile;
  nsresult rv = NS_NewLocalFile(mBasePath, false, getter_AddRefs(storageFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = storageFile->Append(NS_LITERAL_STRING(STORAGE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenUnsharedDatabase(storageFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Nuke the database file.
    rv = storageFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenUnsharedDatabase(storageFile, getter_AddRefs(connection));
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We want extra durability for this important file.
  rv = connection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("PRAGMA synchronous = EXTRA;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Check to make sure that the storage version is correct.
  int32_t storageVersion;
  rv = connection->GetSchemaVersion(&storageVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Hacky downgrade logic!
  // If we see major.minor of 3.0, downgrade it to be 2.1.
  if (storageVersion == kHackyPreDowngradeStorageVersion) {
    storageVersion = kHackyPostDowngradeStorageVersion;
    rv = connection->SetSchemaVersion(storageVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_ASSERT(false, "Downgrade didn't take.");
      return rv;
    }
  }

  if (GetMajorStorageVersion(storageVersion) > kMajorStorageVersion) {
    NS_WARNING("Unable to initialize storage, version is too high!");
    return NS_ERROR_FAILURE;
  }

  if (storageVersion < kStorageVersion) {
    const bool newDatabase = !storageVersion;

    nsCOMPtr<nsIFile> storageDir;
    rv = NS_NewLocalFile(mStoragePath, false, getter_AddRefs(storageDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool exists;
    rv = storageDir->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!exists) {
      nsCOMPtr<nsIFile> indexedDBDir;
      rv = NS_NewLocalFile(mIndexedDBPath, false, getter_AddRefs(indexedDBDir));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = indexedDBDir->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    const bool newDirectory = !exists;

    if (newDatabase) {
      // Set the page size first.
      if (kSQLitePageSizeOverride) {
        rv = connection->ExecuteSimpleSQL(nsPrintfCString(
            "PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    mozStorageTransaction transaction(
        connection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    // An upgrade method can upgrade the database, the storage or both.
    // The upgrade loop below can only be avoided when there's no database and
    // no storage yet (e.g. new profile).
    if (newDatabase && newDirectory) {
      rv = CreateTables(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(NS_SUCCEEDED(connection->GetSchemaVersion(&storageVersion)));
      MOZ_ASSERT(storageVersion == kStorageVersion);
    } else {
      // This logic needs to change next time we change the storage!
      static_assert(kStorageVersion == int32_t((2 << 16) + 2),
                    "Upgrade function needed due to storage version increase.");

      while (storageVersion != kStorageVersion) {
        if (storageVersion == 0) {
          rv = UpgradeStorageFrom0_0To1_0(connection);
        } else if (storageVersion == MakeStorageVersion(1, 0)) {
          rv = UpgradeStorageFrom1_0To2_0(connection);
        } else if (storageVersion == MakeStorageVersion(2, 0)) {
          rv = UpgradeStorageFrom2_0To2_1(connection);
        } else if (storageVersion == MakeStorageVersion(2, 1)) {
          rv = UpgradeStorageFrom2_1To2_2(connection);
        } else {
          NS_WARNING(
              "Unable to initialize storage, no upgrade path is "
              "available!");
          return NS_ERROR_FAILURE;
        }

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = connection->GetSchemaVersion(&storageVersion);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      MOZ_ASSERT(storageVersion == kStorageVersion);
    }

    rv = transaction.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (CachedNextGenLocalStorageEnabled()) {
    nsCOMPtr<mozIStorageConnection> connection;
    bool newlyCreated;
    rv = CreateLocalStorageArchiveConnection(getter_AddRefs(connection),
                                             newlyCreated);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    uint32_t version = 0;

    if (!newlyCreated) {
      bool initialized;
      rv = IsLocalStorageArchiveInitialized(connection, initialized);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (initialized) {
        rv = LoadLocalStorageArchiveVersion(connection, version);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    if (version > kLocalStorageArchiveVersion) {
      rv = DowngradeLocalStorageArchive(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = LoadLocalStorageArchiveVersion(connection, version);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(version == kLocalStorageArchiveVersion);
    } else if (version != kLocalStorageArchiveVersion) {
      if (newlyCreated) {
        MOZ_ASSERT(version == 0);

        rv = InitializeLocalStorageArchive(connection,
                                           kLocalStorageArchiveVersion);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        static_assert(kLocalStorageArchiveVersion == 4,
                      "Upgrade function needed due to LocalStorage archive "
                      "version increase.");

        while (version != kLocalStorageArchiveVersion) {
          if (version < 4) {
            rv = UpgradeLocalStorageArchiveFromLessThan4To4(connection);
          } /* else if (version == 4) {
            rv = UpgradeLocalStorageArchiveFrom4To5(connection);
          } */
          else {
            QM_WARNING(
                "Unable to initialize LocalStorage archive, no upgrade path is "
                "available!");
            return NS_ERROR_FAILURE;
          }

          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          rv = LoadLocalStorageArchiveVersion(connection, version);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }

        MOZ_ASSERT(version == kLocalStorageArchiveVersion);
      }
    }
  } else {
    rv = MaybeRemoveLocalStorageData();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mStorageInitialized = true;

  return NS_OK;
}

already_AddRefed<DirectoryLock> QuotaManager::CreateDirectoryLock(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, Client::Type aClientType, bool aExclusive,
    OpenDirectoryListener* aOpenListener) {
  AssertIsOnOwningThread();

  RefPtr<DirectoryLockImpl> lock = CreateDirectoryLock(
      Nullable<PersistenceType>(aPersistenceType), aGroup,
      OriginScope::FromOrigin(aOrigin), Nullable<Client::Type>(aClientType),
      aExclusive, false, aOpenListener);
  MOZ_ASSERT(lock);

  return lock.forget();
}

void QuotaManager::OpenDirectory(PersistenceType aPersistenceType,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin,
                                 Client::Type aClientType, bool aExclusive,
                                 OpenDirectoryListener* aOpenListener) {
  AssertIsOnOwningThread();

  RefPtr<DirectoryLock> lock =
      CreateDirectoryLock(aPersistenceType, aGroup, aOrigin, aClientType,
                          aExclusive, aOpenListener);
  MOZ_ASSERT(lock);
}

void QuotaManager::OpenDirectoryInternal(
    const Nullable<PersistenceType>& aPersistenceType,
    const OriginScope& aOriginScope, const Nullable<Client::Type>& aClientType,
    bool aExclusive, OpenDirectoryListener* aOpenListener) {
  AssertIsOnOwningThread();

  RefPtr<DirectoryLockImpl> lock = CreateDirectoryLock(
      aPersistenceType, EmptyCString(), aOriginScope,
      Nullable<Client::Type>(aClientType), aExclusive, true, aOpenListener);
  MOZ_ASSERT(lock);

  if (!aExclusive) {
    return;
  }

  // All the locks that block this new exclusive lock need to be invalidated.
  // We also need to notify clients to abort operations for them.
  AutoTArray<nsAutoPtr<nsTHashtable<nsCStringHashKey>>, Client::TYPE_MAX>
      origins;
  origins.SetLength(Client::TypeMax());

  const nsTArray<DirectoryLockImpl*>& blockedOnLocks =
      lock->GetBlockedOnLocks();

  for (DirectoryLockImpl* blockedOnLock : blockedOnLocks) {
    if (!blockedOnLock->IsInternal()) {
      blockedOnLock->Invalidate();

      MOZ_ASSERT(!blockedOnLock->GetClientType().IsNull());
      Client::Type clientType = blockedOnLock->GetClientType().Value();
      MOZ_ASSERT(clientType < Client::TypeMax());

      const OriginScope& originScope = blockedOnLock->GetOriginScope();
      MOZ_ASSERT(originScope.IsOrigin());
      MOZ_ASSERT(!originScope.GetOrigin().IsEmpty());

      nsAutoPtr<nsTHashtable<nsCStringHashKey>>& origin = origins[clientType];
      if (!origin) {
        origin = new nsTHashtable<nsCStringHashKey>();
      }
      origin->PutEntry(originScope.GetOrigin());
    }
  }

  for (uint32_t index : IntegerRange(uint32_t(Client::TypeMax()))) {
    if (origins[index]) {
      for (auto iter = origins[index]->Iter(); !iter.Done(); iter.Next()) {
        MOZ_ASSERT(mClients[index]);

        mClients[index]->AbortOperations(iter.Get()->GetKey());
      }
    }
  }
}

nsresult QuotaManager::EnsureOriginIsInitialized(
    PersistenceType aPersistenceType, const nsACString& aSuffix,
    const nsACString& aGroup, const nsACString& aOrigin, nsIFile** aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIFile> directory;
  bool created;
  nsresult rv = EnsureOriginIsInitializedInternal(
      aPersistenceType, aSuffix, aGroup, aOrigin, getter_AddRefs(directory),
      &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  directory.forget(aDirectory);
  return NS_OK;
}

nsresult QuotaManager::EnsureOriginIsInitializedInternal(
    PersistenceType aPersistenceType, const nsACString& aSuffix,
    const nsACString& aGroup, const nsACString& aOrigin, nsIFile** aDirectory,
    bool* aCreated) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aCreated);

  nsresult rv = EnsureStorageIsInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  // Get directory for this origin and persistence type.
  nsCOMPtr<nsIFile> directory;
  rv = GetDirectoryForOrigin(aPersistenceType, aOrigin,
                             getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    if (mInitializedOrigins.Contains(aOrigin)) {
      directory.forget(aDirectory);
      *aCreated = false;
      return NS_OK;
    }
  } else {
    rv = EnsureTemporaryStorageIsInitialized();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool created;
  rv = EnsureOriginDirectory(directory, &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t timestamp;
  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    if (created) {
      timestamp = PR_Now();

      // Only creating .metadata-v2 to reduce IO.
      rv = CreateDirectoryMetadata2(directory, timestamp,
                                    /* aPersisted */ true, aSuffix, aGroup,
                                    aOrigin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      rv = GetDirectoryMetadata2WithRestore(directory,
                                            /* aPersistent */ true, &timestamp,
                                            /* aPersisted */ nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(timestamp <= PR_Now());
    }

    rv = InitializeOrigin(aPersistenceType, aGroup, aOrigin, timestamp,
                          /* aPersisted */ true, directory);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitializedOrigins.AppendElement(aOrigin);
  } else if (created) {
    NoteOriginDirectoryCreated(aPersistenceType, aGroup, aOrigin,
                               /* aPersisted */ false, timestamp);

    // Only creating .metadata-v2 to reduce IO.
    rv = CreateDirectoryMetadata2(directory, timestamp,
                                  /* aPersisted */ false, aSuffix, aGroup,
                                  aOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  directory.forget(aDirectory);
  *aCreated = created;
  return NS_OK;
}

nsresult QuotaManager::EnsureTemporaryStorageIsInitialized() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mStorageInitialized);

  if (mTemporaryStorageInitialized) {
    return NS_OK;
  }

  TimeStamp startTime = TimeStamp::Now();

  // A keeper to defer the return only in Nightly, so that the telemetry data
  // for whole profile can be collected
  nsresult statusKeeper = NS_OK;

  AutoTArray<RefPtr<Client>, Client::TYPE_MAX>& clients = mClients;
  auto autoNotifier = MakeScopeExit([&clients] {
    for (RefPtr<Client>& client : clients) {
      client->OnStorageInitFailed();
    }
  });

  if (NS_WARN_IF(IsShuttingDown())) {
    RETURN_STATUS_OR_RESULT(statusKeeper, NS_ERROR_FAILURE);
  }

  nsresult rv = InitializeRepository(PERSISTENCE_TYPE_DEFAULT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RECORD_IN_NIGHTLY(statusKeeper, rv);

#ifndef NIGHTLY_BUILD
    // We have to cleanup partially initialized quota.
    RemoveQuota();

    return rv;
#endif
  }

  if (NS_WARN_IF(IsShuttingDown())) {
    RETURN_STATUS_OR_RESULT(statusKeeper, NS_ERROR_FAILURE);
  }

  rv = InitializeRepository(PERSISTENCE_TYPE_TEMPORARY);
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_FAILED(statusKeeper)) {
    // We have to cleanup partially initialized quota.
    RemoveQuota();

#ifdef NIGHTLY_BUILD
    return NS_FAILED(statusKeeper) ? statusKeeper : rv;
#else
    return rv;
#endif
  }

  Telemetry::AccumulateTimeDelta(Telemetry::QM_REPOSITORIES_INITIALIZATION_TIME,
                                 startTime, TimeStamp::Now());

  if (gFixedLimitKB >= 0) {
    mTemporaryStorageLimit = static_cast<uint64_t>(gFixedLimitKB) * 1024;
  } else {
    nsCOMPtr<nsIFile> storageDir =
        do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = storageDir->InitWithPath(GetStoragePath());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = GetTemporaryStorageLimit(storageDir, mTemporaryStorageUsage,
                                  &mTemporaryStorageLimit);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mTemporaryStorageInitialized = true;

  CheckTemporaryStorageLimits();

  autoNotifier.release();

  return rv;
}

nsresult QuotaManager::EnsureOriginDirectory(nsIFile* aDirectory,
                                             bool* aCreated) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aCreated);

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    nsString leafName;
    rv = aDirectory->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!IsSanitizedOriginValid(NS_ConvertUTF16toUTF8(leafName))) {
      QM_WARNING(
          "Preventing creation of a new origin directory which is not "
          "supported by our origin parser or is obsolete!");
      return NS_ERROR_FAILURE;
    }
  }

  rv = EnsureDirectory(aDirectory, aCreated);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::AboutToClearOrigins(
    const Nullable<PersistenceType>& aPersistenceType,
    const OriginScope& aOriginScope,
    const Nullable<Client::Type>& aClientType) {
  AssertIsOnIOThread();

  nsresult rv;

  if (aClientType.IsNull()) {
    for (uint32_t index = 0; index < uint32_t(Client::TypeMax()); index++) {
      rv = mClients[index]->AboutToClearOrigins(aPersistenceType, aOriginScope);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  } else {
    rv = mClients[aClientType.Value()]->AboutToClearOrigins(aPersistenceType,
                                                            aOriginScope);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void QuotaManager::OriginClearCompleted(
    PersistenceType aPersistenceType, const nsACString& aOrigin,
    const Nullable<Client::Type>& aClientType) {
  AssertIsOnIOThread();

  if (aClientType.IsNull()) {
    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      mInitializedOrigins.RemoveElement(aOrigin);
    }

    for (uint32_t index = 0; index < uint32_t(Client::TypeMax()); index++) {
      mClients[index]->OnOriginClearCompleted(aPersistenceType, aOrigin);
    }
  } else {
    mClients[aClientType.Value()]->OnOriginClearCompleted(aPersistenceType,
                                                          aOrigin);
  }
}

void QuotaManager::ResetOrClearCompleted() {
  AssertIsOnIOThread();

  mInitializedOrigins.Clear();
  mTemporaryStorageInitialized = false;
  mStorageInitialized = false;

  ReleaseIOThreadObjects();
}

Client* QuotaManager::GetClient(Client::Type aClientType) {
  MOZ_ASSERT(aClientType >= Client::IDB);
  MOZ_ASSERT(aClientType < Client::TypeMax());

  return mClients.ElementAt(aClientType);
}

uint64_t QuotaManager::GetGroupLimit() const {
  MOZ_ASSERT(mTemporaryStorageInitialized);

  // To avoid one group evicting all the rest, limit the amount any one group
  // can use to 20%. To prevent individual sites from using exorbitant amounts
  // of storage where there is a lot of free space, cap the group limit to 2GB.
  uint64_t x = std::min<uint64_t>(mTemporaryStorageLimit * .20, 2 GB);

  // In low-storage situations, make an exception (while not exceeding the total
  // storage limit).
  return std::min<uint64_t>(mTemporaryStorageLimit,
                            std::max<uint64_t>(x, 10 MB));
}

void QuotaManager::GetGroupUsageAndLimit(const nsACString& aGroup,
                                         UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aUsageInfo);

  {
    MutexAutoLock lock(mQuotaMutex);

    aUsageInfo->SetLimit(GetGroupLimit());
    aUsageInfo->ResetUsage();

    GroupInfoPair* pair;
    if (!mGroupInfoPairs.Get(aGroup, &pair)) {
      return;
    }

    // Calculate temporary group usage
    RefPtr<GroupInfo> temporaryGroupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
    if (temporaryGroupInfo) {
      aUsageInfo->AppendToDatabaseUsage(temporaryGroupInfo->mUsage);
    }

    // Calculate default group usage
    RefPtr<GroupInfo> defaultGroupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (defaultGroupInfo) {
      aUsageInfo->AppendToDatabaseUsage(defaultGroupInfo->mUsage);
    }
  }
}

void QuotaManager::NotifyStoragePressure(uint64_t aUsage) {
  mQuotaMutex.AssertNotCurrentThreadOwns();

  RefPtr<StoragePressureRunnable> storagePressureRunnable =
      new StoragePressureRunnable(aUsage);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(storagePressureRunnable));
}

// static
void QuotaManager::GetStorageId(PersistenceType aPersistenceType,
                                const nsACString& aOrigin,
                                Client::Type aClientType,
                                nsACString& aDatabaseId) {
  nsAutoCString str;
  str.AppendInt(aPersistenceType);
  str.Append('*');
  str.Append(aOrigin);
  str.Append('*');
  str.AppendInt(aClientType);

  aDatabaseId = str;
}

// static
bool QuotaManager::IsPrincipalInfoValid(const PrincipalInfo& aPrincipalInfo) {
  switch (aPrincipalInfo.type()) {
    // A system principal is acceptable.
    case PrincipalInfo::TSystemPrincipalInfo: {
      return true;
    }

    // Validate content principals to ensure that the spec, originNoSuffix and
    // baseDomain are sane.
    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      // Verify the principal spec parses.
      RefPtr<MozURL> specURL;
      nsresult rv = MozURL::Init(getter_AddRefs(specURL), info.spec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        QM_WARNING("A URL %s is not recognized by MozURL", info.spec().get());
        return false;
      }

      // Verify the principal originNoSuffix matches spec.
      nsCString originNoSuffix;
      specURL->Origin(originNoSuffix);

      if (NS_WARN_IF(originNoSuffix != info.originNoSuffix())) {
        QM_WARNING("originNoSuffix (%s) doesn't match passed one (%s)!",
                   originNoSuffix.get(), info.originNoSuffix().get());
        return false;
      }

      if (NS_WARN_IF(info.originNoSuffix().EqualsLiteral(kChromeOrigin))) {
        return false;
      }

      if (NS_WARN_IF(info.originNoSuffix().FindChar('^', 0) != -1)) {
        QM_WARNING("originNoSuffix (%s) contains the '^' character!",
                   info.originNoSuffix().get());
        return false;
      }

      // Verify the principal baseDomain exists.
      if (NS_WARN_IF(info.baseDomain().IsVoid())) {
        return false;
      }

      // Verify the principal baseDomain matches spec.
      nsCString baseDomain;
      rv = specURL->BaseDomain(baseDomain);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      if (NS_WARN_IF(baseDomain != info.baseDomain())) {
        QM_WARNING("baseDomain (%s) doesn't match passed one (%s)!",
                   baseDomain.get(), info.baseDomain().get());
        return false;
      }

      return true;
    }

    default: {
      break;
    }
  }

  // Null and expanded principals are not acceptable.
  return false;
}

// static
void QuotaManager::GetInfoFromValidatedPrincipalInfo(
    const PrincipalInfo& aPrincipalInfo, nsACString* aSuffix,
    nsACString* aGroup, nsACString* aOrigin) {
  MOZ_ASSERT(IsPrincipalInfoValid(aPrincipalInfo));

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo: {
      GetInfoForChrome(aSuffix, aGroup, aOrigin);
      return;
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsCString suffix;
      info.attrs().CreateSuffix(suffix);

      if (aSuffix) {
        aSuffix->Assign(suffix);
      }

      if (aGroup) {
        aGroup->Assign(info.baseDomain() + suffix);
      }

      if (aOrigin) {
        aOrigin->Assign(info.originNoSuffix() + suffix);
      }

      return;
    }

    default: {
      break;
    }
  }

  MOZ_CRASH("Should never get here!");
}

// static
nsresult QuotaManager::GetInfoFromPrincipal(nsIPrincipal* aPrincipal,
                                            nsACString* aSuffix,
                                            nsACString* aGroup,
                                            nsACString* aOrigin) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    GetInfoForChrome(aSuffix, aGroup, aOrigin);
    return NS_OK;
  }

  if (aPrincipal->GetIsNullPrincipal()) {
    NS_WARNING("IndexedDB not supported from this principal!");
    return NS_ERROR_FAILURE;
  }

  nsCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  NS_ENSURE_SUCCESS(rv, rv);

  if (origin.EqualsLiteral(kChromeOrigin)) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return NS_ERROR_FAILURE;
  }

  nsCString suffix;
  aPrincipal->OriginAttributesRef().CreateSuffix(suffix);

  if (aSuffix) {
    aSuffix->Assign(suffix);
  }

  if (aGroup) {
    nsCString baseDomain;
    rv = aPrincipal->GetBaseDomain(baseDomain);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(!baseDomain.IsEmpty());

    aGroup->Assign(baseDomain + suffix);
  }

  if (aOrigin) {
    aOrigin->Assign(origin);
  }

  return NS_OK;
}

// static
nsresult QuotaManager::GetInfoFromWindow(nsPIDOMWindowOuter* aWindow,
                                         nsACString* aSuffix,
                                         nsACString* aGroup,
                                         nsACString* aOrigin) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(sop, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  nsresult rv = GetInfoFromPrincipal(principal, aSuffix, aGroup, aOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
void QuotaManager::GetInfoForChrome(nsACString* aSuffix, nsACString* aGroup,
                                    nsACString* aOrigin) {
  if (aSuffix) {
    aSuffix->Assign(EmptyCString());
  }
  if (aGroup) {
    ChromeOrigin(*aGroup);
  }
  if (aOrigin) {
    ChromeOrigin(*aOrigin);
  }
}

// static
bool QuotaManager::IsOriginInternal(const nsACString& aOrigin) {
  MOZ_ASSERT(!aOrigin.IsEmpty());

  // The first prompt is not required for these origins.
  if (aOrigin.EqualsLiteral(kChromeOrigin) ||
      StringBeginsWith(aOrigin, nsDependentCString(kAboutHomeOriginPrefix)) ||
      StringBeginsWith(aOrigin, nsDependentCString(kIndexedDBOriginPrefix)) ||
      StringBeginsWith(aOrigin, nsDependentCString(kResourceOriginPrefix))) {
    return true;
  }

  return false;
}

// static
void QuotaManager::ChromeOrigin(nsACString& aOrigin) {
  aOrigin.AssignLiteral(kChromeOrigin);
}

// static
bool QuotaManager::AreOriginsEqualOnDisk(nsACString& aOrigin1,
                                         nsACString& aOrigin2) {
  nsCString origin1Sanitized(aOrigin1);
  SanitizeOriginString(origin1Sanitized);

  nsCString origin2Sanitized(aOrigin2);
  SanitizeOriginString(origin2Sanitized);

  return origin1Sanitized == origin2Sanitized;
}

// static
bool QuotaManager::ParseOrigin(const nsACString& aOrigin, nsCString& aSpec,
                               OriginAttributes* aAttrs) {
  MOZ_ASSERT(aAttrs);

  if (aOrigin.Equals(kChromeOrigin)) {
    aSpec = kChromeOrigin;
    return true;
  }

  nsCString sanitizedOrigin(aOrigin);
  SanitizeOriginString(sanitizedOrigin);

  OriginParser::ResultType result =
      OriginParser::ParseOrigin(sanitizedOrigin, aSpec, aAttrs);
  if (NS_WARN_IF(result != OriginParser::ValidOrigin)) {
    return false;
  }

  return true;
}

uint64_t QuotaManager::LockedCollectOriginsForEviction(
    uint64_t aMinSizeToBeFreed, nsTArray<RefPtr<DirectoryLockImpl>>& aLocks) {
  mQuotaMutex.AssertCurrentThreadOwns();

  RefPtr<CollectOriginsHelper> helper =
      new CollectOriginsHelper(mQuotaMutex, aMinSizeToBeFreed);

  // Unlock while calling out to XPCOM (code behind the dispatch method needs
  // to acquire its own lock which can potentially lead to a deadlock and it
  // also calls an observer that can do various stuff like IO, so it's better
  // to not hold our mutex while that happens).
  {
    MutexAutoUnlock autoUnlock(mQuotaMutex);

    MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(helper, NS_DISPATCH_NORMAL));
  }

  return helper->BlockAndReturnOriginsForEviction(aLocks);
}

void QuotaManager::LockedRemoveQuotaForOrigin(PersistenceType aPersistenceType,
                                              const nsACString& aGroup,
                                              const nsACString& aOrigin) {
  mQuotaMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  GroupInfoPair* pair;
  mGroupInfoPairs.Get(aGroup, &pair);

  if (!pair) {
    return;
  }

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (groupInfo) {
    groupInfo->LockedRemoveOriginInfo(aOrigin);

    if (!groupInfo->LockedHasOriginInfos()) {
      pair->LockedClearGroupInfo(aPersistenceType);

      if (!pair->LockedHasGroupInfos()) {
        mGroupInfoPairs.Remove(aGroup);
      }
    }
  }
}

already_AddRefed<GroupInfo> QuotaManager::LockedGetOrCreateGroupInfo(
    PersistenceType aPersistenceType, const nsACString& aGroup) {
  mQuotaMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aGroup, &pair)) {
    pair = new GroupInfoPair();
    mGroupInfoPairs.Put(aGroup, pair);
    // The hashtable is now responsible to delete the GroupInfoPair.
  }

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    groupInfo = new GroupInfo(pair, aPersistenceType, aGroup);
    pair->LockedSetGroupInfo(aPersistenceType, groupInfo);
  }

  return groupInfo.forget();
}

already_AddRefed<OriginInfo> QuotaManager::LockedGetOriginInfo(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin) {
  mQuotaMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  GroupInfoPair* pair;
  if (mGroupInfoPairs.Get(aGroup, &pair)) {
    RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
    if (groupInfo) {
      return groupInfo->LockedGetOriginInfo(aOrigin);
    }
  }

  return nullptr;
}

void QuotaManager::CheckTemporaryStorageLimits() {
  AssertIsOnIOThread();

  nsTArray<OriginInfo*> doomedOriginInfos;
  {
    MutexAutoLock lock(mQuotaMutex);

    for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
      GroupInfoPair* pair = iter.UserData();

      MOZ_ASSERT(!iter.Key().IsEmpty(), "Empty key!");
      MOZ_ASSERT(pair, "Null pointer!");

      uint64_t groupUsage = 0;

      RefPtr<GroupInfo> temporaryGroupInfo =
          pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
      if (temporaryGroupInfo) {
        groupUsage += temporaryGroupInfo->mUsage;
      }

      RefPtr<GroupInfo> defaultGroupInfo =
          pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
      if (defaultGroupInfo) {
        groupUsage += defaultGroupInfo->mUsage;
      }

      if (groupUsage > 0) {
        QuotaManager* quotaManager = QuotaManager::Get();
        MOZ_ASSERT(quotaManager, "Shouldn't be null!");

        if (groupUsage > quotaManager->GetGroupLimit()) {
          nsTArray<OriginInfo*> originInfos;
          if (temporaryGroupInfo) {
            originInfos.AppendElements(temporaryGroupInfo->mOriginInfos);
          }
          if (defaultGroupInfo) {
            originInfos.AppendElements(defaultGroupInfo->mOriginInfos);
          }
          originInfos.Sort(OriginInfoLRUComparator());

          for (uint32_t i = 0; i < originInfos.Length(); i++) {
            OriginInfo* originInfo = originInfos[i];
            if (originInfo->LockedPersisted()) {
              continue;
            }

            doomedOriginInfos.AppendElement(originInfo);
            groupUsage -= originInfo->mUsage;

            if (groupUsage <= quotaManager->GetGroupLimit()) {
              break;
            }
          }
        }
      }
    }

    uint64_t usage = 0;
    for (uint32_t index = 0; index < doomedOriginInfos.Length(); index++) {
      usage += doomedOriginInfos[index]->mUsage;
    }

    if (mTemporaryStorageUsage - usage > mTemporaryStorageLimit) {
      nsTArray<OriginInfo*> originInfos;

      for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
        GroupInfoPair* pair = iter.UserData();

        MOZ_ASSERT(!iter.Key().IsEmpty(), "Empty key!");
        MOZ_ASSERT(pair, "Null pointer!");

        RefPtr<GroupInfo> groupInfo =
            pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
        if (groupInfo) {
          originInfos.AppendElements(groupInfo->mOriginInfos);
        }

        groupInfo = pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
        if (groupInfo) {
          originInfos.AppendElements(groupInfo->mOriginInfos);
        }
      }

      for (uint32_t index = originInfos.Length(); index > 0; index--) {
        if (doomedOriginInfos.Contains(originInfos[index - 1]) ||
            originInfos[index - 1]->LockedPersisted()) {
          originInfos.RemoveElementAt(index - 1);
        }
      }

      originInfos.Sort(OriginInfoLRUComparator());

      for (uint32_t i = 0; i < originInfos.Length(); i++) {
        if (mTemporaryStorageUsage - usage <= mTemporaryStorageLimit) {
          originInfos.TruncateLength(i);
          break;
        }

        usage += originInfos[i]->mUsage;
      }

      doomedOriginInfos.AppendElements(originInfos);
    }
  }

  for (uint32_t index = 0; index < doomedOriginInfos.Length(); index++) {
    OriginInfo* doomedOriginInfo = doomedOriginInfos[index];

#ifdef DEBUG
    {
      MutexAutoLock lock(mQuotaMutex);
      MOZ_ASSERT(!doomedOriginInfo->LockedPersisted());
    }
#endif

    DeleteFilesForOrigin(doomedOriginInfo->mGroupInfo->mPersistenceType,
                         doomedOriginInfo->mOrigin);
  }

  nsTArray<OriginParams> doomedOrigins;
  {
    MutexAutoLock lock(mQuotaMutex);

    for (uint32_t index = 0; index < doomedOriginInfos.Length(); index++) {
      OriginInfo* doomedOriginInfo = doomedOriginInfos[index];

      PersistenceType persistenceType =
          doomedOriginInfo->mGroupInfo->mPersistenceType;
      nsCString group = doomedOriginInfo->mGroupInfo->mGroup;
      nsCString origin = doomedOriginInfo->mOrigin;
      LockedRemoveQuotaForOrigin(persistenceType, group, origin);

#ifdef DEBUG
      doomedOriginInfos[index] = nullptr;
#endif

      doomedOrigins.AppendElement(OriginParams(persistenceType, origin));
    }
  }

  for (const OriginParams& doomedOrigin : doomedOrigins) {
    OriginClearCompleted(doomedOrigin.mPersistenceType, doomedOrigin.mOrigin,
                         Nullable<Client::Type>());
  }

  if (mTemporaryStorageUsage > mTemporaryStorageLimit) {
    // If disk space is still low after origin clear, notify storage pressure.
    NotifyStoragePressure(mTemporaryStorageUsage);
  }
}

void QuotaManager::DeleteFilesForOrigin(PersistenceType aPersistenceType,
                                        const nsACString& aOrigin) {
  nsCOMPtr<nsIFile> directory;
  nsresult rv = GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                      getter_AddRefs(directory));
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    NS_ERROR("Failed to remove directory!");
  }
}

void QuotaManager::FinalizeOriginEviction(
    nsTArray<RefPtr<DirectoryLockImpl>>& aLocks) {
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  RefPtr<FinalizeOriginEvictionOp> op =
      new FinalizeOriginEvictionOp(mOwningThread, aLocks);

  if (IsOnIOThread()) {
    op->RunOnIOThreadImmediately();
  } else {
    op->Dispatch();
  }
}

void QuotaManager::ShutdownTimerCallback(nsITimer* aTimer, void* aClosure) {
  AssertIsOnBackgroundThread();

  auto quotaManager = static_cast<QuotaManager*>(aClosure);
  MOZ_ASSERT(quotaManager);

  NS_WARNING(
      "Some storage operations are taking longer than expected "
      "during shutdown and will be aborted!");

  // Abort all operations.
  for (RefPtr<Client>& client : quotaManager->mClients) {
    client->AbortOperations(VoidCString());
  }
}

auto QuotaManager::GetDirectoryLockTable(PersistenceType aPersistenceType)
    -> DirectoryLockTable& {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryDirectoryLockTable;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultDirectoryLockTable;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
}

bool QuotaManager::IsSanitizedOriginValid(const nsACString& aSanitizedOrigin) {
  AssertIsOnIOThread();

  bool valid;
  if (auto entry = mValidOrigins.LookupForAdd(aSanitizedOrigin)) {
    // We already parsed this sanitized origin string.
    valid = entry.Data();
  } else {
    nsCString spec;
    OriginAttributes attrs;
    OriginParser::ResultType result =
        OriginParser::ParseOrigin(aSanitizedOrigin, spec, &attrs);

    valid = result == OriginParser::ValidOrigin;
    entry.OrInsert([valid]() { return valid; });
  }

  return valid;
}

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

OriginInfo::OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin,
                       uint64_t aUsage, int64_t aAccessTime, bool aPersisted,
                       bool aDirectoryExists)
    : mGroupInfo(aGroupInfo),
      mOrigin(aOrigin),
      mUsage(aUsage),
      mAccessTime(aAccessTime),
      mPersisted(aPersisted),
      mDirectoryExists(aDirectoryExists) {
  MOZ_ASSERT(aGroupInfo);
  MOZ_ASSERT_IF(aPersisted,
                aGroupInfo->mPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  MOZ_COUNT_CTOR(OriginInfo);
}

void OriginInfo::LockedDecreaseUsage(int64_t aSize) {
  AssertCurrentThreadOwnsQuotaMutex();

  AssertNoUnderflow(mUsage, aSize);
  mUsage -= aSize;

  if (!LockedPersisted()) {
    AssertNoUnderflow(mGroupInfo->mUsage, aSize);
    mGroupInfo->mUsage -= aSize;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, aSize);
  quotaManager->mTemporaryStorageUsage -= aSize;
}

void OriginInfo::LockedPersist() {
  AssertCurrentThreadOwnsQuotaMutex();
  MOZ_ASSERT(mGroupInfo->mPersistenceType == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(!mPersisted);

  mPersisted = true;

  // Remove Usage from GroupInfo
  AssertNoUnderflow(mGroupInfo->mUsage, mUsage);
  mGroupInfo->mUsage -= mUsage;
}

already_AddRefed<OriginInfo> GroupInfo::LockedGetOriginInfo(
    const nsACString& aOrigin) {
  AssertCurrentThreadOwnsQuotaMutex();

  for (RefPtr<OriginInfo>& originInfo : mOriginInfos) {
    if (originInfo->mOrigin == aOrigin) {
      RefPtr<OriginInfo> result = originInfo;
      return result.forget();
    }
  }

  return nullptr;
}

void GroupInfo::LockedAddOriginInfo(OriginInfo* aOriginInfo) {
  AssertCurrentThreadOwnsQuotaMutex();

  NS_ASSERTION(!mOriginInfos.Contains(aOriginInfo),
               "Replacing an existing entry!");
  mOriginInfos.AppendElement(aOriginInfo);

  if (!aOriginInfo->LockedPersisted()) {
    AssertNoOverflow(mUsage, aOriginInfo->mUsage);
    mUsage += aOriginInfo->mUsage;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, aOriginInfo->mUsage);
  quotaManager->mTemporaryStorageUsage += aOriginInfo->mUsage;
}

void GroupInfo::LockedRemoveOriginInfo(const nsACString& aOrigin) {
  AssertCurrentThreadOwnsQuotaMutex();

  for (uint32_t index = 0; index < mOriginInfos.Length(); index++) {
    if (mOriginInfos[index]->mOrigin == aOrigin) {
      if (!mOriginInfos[index]->LockedPersisted()) {
        AssertNoUnderflow(mUsage, mOriginInfos[index]->mUsage);
        mUsage -= mOriginInfos[index]->mUsage;
      }

      QuotaManager* quotaManager = QuotaManager::Get();
      MOZ_ASSERT(quotaManager);

      AssertNoUnderflow(quotaManager->mTemporaryStorageUsage,
                        mOriginInfos[index]->mUsage);
      quotaManager->mTemporaryStorageUsage -= mOriginInfos[index]->mUsage;

      mOriginInfos.RemoveElementAt(index);

      return;
    }
  }
}

void GroupInfo::LockedRemoveOriginInfos() {
  AssertCurrentThreadOwnsQuotaMutex();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  for (uint32_t index = mOriginInfos.Length(); index > 0; index--) {
    OriginInfo* originInfo = mOriginInfos[index - 1];

    if (!originInfo->LockedPersisted()) {
      AssertNoUnderflow(mUsage, originInfo->mUsage);
      mUsage -= originInfo->mUsage;
    }

    AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, originInfo->mUsage);
    quotaManager->mTemporaryStorageUsage -= originInfo->mUsage;

    mOriginInfos.RemoveElementAt(index - 1);
  }
}

RefPtr<GroupInfo>& GroupInfoPair::GetGroupInfoForPersistenceType(
    PersistenceType aPersistenceType) {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryStorageGroupInfo;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultStorageGroupInfo;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
}

CollectOriginsHelper::CollectOriginsHelper(mozilla::Mutex& aMutex,
                                           uint64_t aMinSizeToBeFreed)
    : Runnable("dom::quota::CollectOriginsHelper"),
      mMinSizeToBeFreed(aMinSizeToBeFreed),
      mMutex(aMutex),
      mCondVar(aMutex, "CollectOriginsHelper::mCondVar"),
      mSizeToBeFreed(0),
      mWaiting(true) {
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();
}

int64_t CollectOriginsHelper::BlockAndReturnOriginsForEviction(
    nsTArray<RefPtr<DirectoryLockImpl>>& aLocks) {
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();

  while (mWaiting) {
    mCondVar.Wait();
  }

  mLocks.SwapElements(aLocks);
  return mSizeToBeFreed;
}

NS_IMETHODIMP
CollectOriginsHelper::Run() {
  AssertIsOnBackgroundThread();

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  // We use extra stack vars here to avoid race detector warnings (the same
  // memory accessed with and without the lock held).
  nsTArray<RefPtr<DirectoryLockImpl>> locks;
  uint64_t sizeToBeFreed =
      quotaManager->CollectOriginsForEviction(mMinSizeToBeFreed, locks);

  MutexAutoLock lock(mMutex);

  NS_ASSERTION(mWaiting, "Huh?!");

  mLocks.SwapElements(locks);
  mSizeToBeFreed = sizeToBeFreed;
  mWaiting = false;
  mCondVar.Notify();

  return NS_OK;
}

/*******************************************************************************
 * OriginOperationBase
 ******************************************************************************/

NS_IMETHODIMP
OriginOperationBase::Run() {
  nsresult rv;

  switch (mState) {
    case State_Initial: {
      rv = Init();
      break;
    }

    case State_CreatingQuotaManager: {
      rv = QuotaManagerOpen();
      break;
    }

    case State_DirectoryOpenPending: {
      rv = DirectoryOpen();
      break;
    }

    case State_DirectoryWorkOpen: {
      rv = DirectoryWork();
      break;
    }

    case State_UnblockingOpen: {
      UnblockOpen();
      return NS_OK;
    }

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State_UnblockingOpen) {
    Finish(rv);
  }

  return NS_OK;
}

nsresult OriginOperationBase::DirectoryOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return NS_ERROR_FAILURE;
  }

  // Must set this before dispatching otherwise we will race with the IO thread.
  AdvanceState();

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void OriginOperationBase::Finish(nsresult aResult) {
  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = aResult;
  }

  // Must set mState before dispatching otherwise we will race with the main
  // thread.
  mState = State_UnblockingOpen;

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
}

nsresult OriginOperationBase::Init() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_Initial);

  if (QuotaManager::IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  AdvanceState();

  if (mNeedsQuotaManagerInit && !QuotaManager::Get()) {
    QuotaManager::GetOrCreate(this);
  } else {
    Open();
  }

  return NS_OK;
}

nsresult OriginOperationBase::QuotaManagerOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_CreatingQuotaManager);

  if (NS_WARN_IF(!QuotaManager::Get())) {
    return NS_ERROR_FAILURE;
  }

  Open();

  return NS_OK;
}

nsresult OriginOperationBase::DirectoryWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DirectoryWorkOpen);

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  if (mNeedsQuotaManagerInit) {
    rv = quotaManager->EnsureStorageIsInitialized();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = DoDirectoryWork(quotaManager);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  AdvanceState();

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void FinalizeOriginEvictionOp::Dispatch() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(GetState() == State_Initial);

  SetState(State_DirectoryOpenPending);

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
}

void FinalizeOriginEvictionOp::RunOnIOThreadImmediately() {
  AssertIsOnIOThread();
  MOZ_ASSERT(GetState() == State_Initial);

  SetState(State_DirectoryWorkOpen);

  MOZ_ALWAYS_SUCCEEDS(this->Run());
}

void FinalizeOriginEvictionOp::Open() { MOZ_CRASH("Shouldn't get here!"); }

nsresult FinalizeOriginEvictionOp::DoDirectoryWork(
    QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("FinalizeOriginEvictionOp::DoDirectoryWork", OTHER);

  for (RefPtr<DirectoryLockImpl>& lock : mLocks) {
    aQuotaManager->OriginClearCompleted(lock->GetPersistenceType().Value(),
                                        lock->GetOriginScope().GetOrigin(),
                                        Nullable<Client::Type>());
  }

  return NS_OK;
}

void FinalizeOriginEvictionOp::UnblockOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_UnblockingOpen);

#ifdef DEBUG
  NoteActorDestroyed();
#endif

  mLocks.Clear();

  AdvanceState();
}

NS_IMPL_ISUPPORTS_INHERITED0(NormalOriginOperationBase, Runnable)

void NormalOriginOperationBase::Open() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_CreatingQuotaManager);
  MOZ_ASSERT(QuotaManager::Get());

  AdvanceState();

  QuotaManager::Get()->OpenDirectoryInternal(mPersistenceType, mOriginScope,
                                             mClientType, mExclusive, this);
}

void NormalOriginOperationBase::UnblockOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_UnblockingOpen);

  SendResults();

  mDirectoryLock = nullptr;

  AdvanceState();
}

void NormalOriginOperationBase::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  nsresult rv = DirectoryOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv);
    return;
  }
}

void NormalOriginOperationBase::DirectoryLockFailed() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  Finish(NS_ERROR_FAILURE);
}

nsresult SaveOriginAccessTimeOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("SaveOriginAccessTimeOp::DoDirectoryWork", OTHER);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(
      mPersistenceType.Value(), mOriginScope.GetOrigin(), getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(METADATA_V2_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetBinaryOutputStream(file, kUpdateFileFlag, getter_AddRefs(stream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The origin directory may not exist anymore.
  if (stream) {
    rv = stream->Write64(mTimestamp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void SaveOriginAccessTimeOp::SendResults() {
#ifdef DEBUG
  NoteActorDestroyed();
#endif
}

NS_IMETHODIMP
StoragePressureRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obsSvc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper =
      do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
  if (NS_WARN_IF(!wrapper)) {
    return NS_ERROR_FAILURE;
  }

  wrapper->SetData(mUsage);

  obsSvc->NotifyObservers(wrapper, "QuotaManager::StoragePressure", u"");

  return NS_OK;
}

/*******************************************************************************
 * Quota
 ******************************************************************************/

Quota::Quota()
#ifdef DEBUG
    : mActorDestroyed(false)
#endif
{
}

Quota::~Quota() { MOZ_ASSERT(mActorDestroyed); }

void Quota::StartIdleMaintenance() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return;
  }

  quotaManager->StartIdleMaintenance();
}

bool Quota::VerifyRequestParams(const UsageRequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  switch (aParams.type()) {
    case UsageRequestParams::TAllUsageParams:
      break;

    case UsageRequestParams::TOriginUsageParams: {
      const OriginUsageParams& params = aParams.get_OriginUsageParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

bool Quota::VerifyRequestParams(const RequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  switch (aParams.type()) {
    case RequestParams::TInitParams:
    case RequestParams::TInitTemporaryStorageParams:
      break;

    case RequestParams::TInitOriginParams: {
      const InitOriginParams& params = aParams.get_InitOriginParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TClearOriginParams: {
      const ClearResetOriginParams& params =
          aParams.get_ClearOriginParams().commonParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TResetOriginParams: {
      const ClearResetOriginParams& params =
          aParams.get_ResetOriginParams().commonParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TClearDataParams: {
      if (BackgroundParent::IsOtherProcessActor(Manager())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TClearAllParams:
    case RequestParams::TResetAllParams:
    case RequestParams::TListInitializedOriginsParams:
      break;

    case RequestParams::TPersistedParams: {
      const PersistedParams& params = aParams.get_PersistedParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TPersistParams: {
      const PersistParams& params = aParams.get_PersistParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

void Quota::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
#ifdef DEBUG
  MOZ_ASSERT(!mActorDestroyed);
  mActorDestroyed = true;
#endif
}

PQuotaUsageRequestParent* Quota::AllocPQuotaUsageRequestParent(
    const UsageRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  if (!trustParams && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<QuotaUsageRequestBase> actor;

  switch (aParams.type()) {
    case UsageRequestParams::TAllUsageParams:
      actor = new GetUsageOp(aParams);
      break;

    case UsageRequestParams::TOriginUsageParams:
      actor = new GetOriginUsageOp(aParams);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Quota::RecvPQuotaUsageRequestConstructor(
    PQuotaUsageRequestParent* aActor, const UsageRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  auto* op = static_cast<QuotaUsageRequestBase*>(aActor);

  if (NS_WARN_IF(!op->Init(this))) {
    return IPC_FAIL_NO_REASON(this);
  }

  op->RunImmediately();
  return IPC_OK();
}

bool Quota::DeallocPQuotaUsageRequestParent(PQuotaUsageRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaUsageRequestBase> actor =
      dont_AddRef(static_cast<QuotaUsageRequestBase*>(aActor));
  return true;
}

PQuotaRequestParent* Quota::AllocPQuotaRequestParent(
    const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  if (!trustParams && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<QuotaRequestBase> actor;

  switch (aParams.type()) {
    case RequestParams::TInitParams:
      actor = new InitOp();
      break;

    case RequestParams::TInitTemporaryStorageParams:
      actor = new InitTemporaryStorageOp();
      break;

    case RequestParams::TInitOriginParams:
      actor = new InitOriginOp(aParams);
      break;

    case RequestParams::TClearOriginParams:
    case RequestParams::TResetOriginParams:
      actor = new ClearOriginOp(aParams);
      break;

    case RequestParams::TClearDataParams:
      actor = new ClearDataOp(aParams);
      break;

    case RequestParams::TClearAllParams:
      actor = new ResetOrClearOp(/* aClear */ true);
      break;

    case RequestParams::TResetAllParams:
      actor = new ResetOrClearOp(/* aClear */ false);
      break;

    case RequestParams::TPersistedParams:
      actor = new PersistedOp(aParams);
      break;

    case RequestParams::TPersistParams:
      actor = new PersistOp(aParams);
      break;

    case RequestParams::TListInitializedOriginsParams:
      actor = new ListInitializedOriginsOp();
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Quota::RecvPQuotaRequestConstructor(
    PQuotaRequestParent* aActor, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  auto* op = static_cast<QuotaRequestBase*>(aActor);

  if (NS_WARN_IF(!op->Init(this))) {
    return IPC_FAIL_NO_REASON(this);
  }

  op->RunImmediately();
  return IPC_OK();
}

bool Quota::DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaRequestBase> actor =
      dont_AddRef(static_cast<QuotaRequestBase*>(aActor));
  return true;
}

mozilla::ipc::IPCResult Quota::RecvStartIdleMaintenance() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("dom::quota::Quota::StartIdleMaintenance", this,
                          &Quota::StartIdleMaintenance);

    QuotaManager::GetOrCreate(callback);
    return IPC_OK();
  }

  quotaManager->StartIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvStopIdleMaintenance() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->StopIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvAbortOperationsForProcess(
    const ContentParentId& aContentParentId) {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->AbortOperationsForProcess(aContentParentId);

  return IPC_OK();
}

bool QuotaUsageRequestBase::Init(Quota* aQuota) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  mNeedsQuotaManagerInit = true;

  return true;
}

nsresult QuotaUsageRequestBase::GetUsageForOrigin(
    QuotaManager* aQuotaManager, PersistenceType aPersistenceType,
    const nsACString& aGroup, const nsACString& aOrigin,
    UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);
  MOZ_ASSERT(aUsageInfo);
  MOZ_ASSERT(aUsageInfo->TotalUsage() == 0);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                                     getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = directory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the directory exists then enumerate all the files inside, adding up
  // the sizes to get the final usage statistic.
  if (exists && !mCanceled) {
    bool initialized;

    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      initialized = aQuotaManager->IsOriginInitialized(aOrigin);
    } else {
      initialized = aQuotaManager->IsTemporaryStorageInitialized();
    }

    nsCOMPtr<nsIDirectoryEnumerator> entries;
    rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file;
    while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(file)))) &&
           file && !mCanceled) {
      bool isDirectory;
      rv = file->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsString leafName;
      rv = file->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!isDirectory) {
        // We are maintaining existing behavior here (failing if the origin is
        // not yet initialized or just continuing otherwise).
        // This can possibly be used by developers to add temporary backups into
        // origin directories without losing get usage functionality.
        if (IsOriginMetadata(leafName)) {
          continue;
        }

        if (IsTempMetadata(leafName)) {
          if (!initialized) {
            rv = file->Remove(/* recursive */ false);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
          }

          continue;
        }

        if (IsOSMetadata(leafName) || IsDotFile(leafName)) {
          continue;
        }

        UNKNOWN_FILE_WARNING(leafName);
        if (!initialized) {
          return NS_ERROR_UNEXPECTED;
        }
        continue;
      }

      Client::Type clientType;
      rv = Client::TypeFromText(leafName, clientType);
      if (NS_FAILED(rv)) {
        UNKNOWN_FILE_WARNING(leafName);
        if (!initialized) {
          return NS_ERROR_UNEXPECTED;
        }
        continue;
      }

      Client* client = aQuotaManager->GetClient(clientType);
      MOZ_ASSERT(client);

      if (initialized) {
        rv = client->GetUsageForOrigin(aPersistenceType, aGroup, aOrigin,
                                       mCanceled, aUsageInfo);
      } else {
        rv = client->InitOrigin(aPersistenceType, aGroup, aOrigin, mCanceled,
                                aUsageInfo, /* aForGetUsage */ true);
      }
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

void QuotaUsageRequestBase::SendResults() {
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    if (mCanceled) {
      mResultCode = NS_ERROR_FAILURE;
    }

    UsageRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaUsageRequestParent::Send__delete__(this, response);
  }
}

void QuotaUsageRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult QuotaUsageRequestBase::RecvCancel() {
  AssertIsOnOwningThread();

  if (mCanceled.exchange(true)) {
    NS_WARNING("Canceled more than once?!");
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

nsresult TraverseRepositoryHelper::TraverseRepository(
    QuotaManager* aQuotaManager, PersistenceType aPersistenceType) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(aQuotaManager->GetStoragePath(aPersistenceType),
                                false, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = directory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool persistent = aPersistenceType == PERSISTENCE_TYPE_PERSISTENT;

  nsCOMPtr<nsIFile> originDir;
  while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(originDir)))) &&
         originDir && !IsCanceled()) {
    bool isDirectory;
    rv = originDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      nsString leafName;
      rv = originDir->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Unknown files during getting usages are allowed. Just warn if we find
      // them.
      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    rv = ProcessOrigin(aQuotaManager, originDir, persistent, aPersistenceType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

GetUsageOp::GetUsageOp(const UsageRequestParams& aParams)
    : mGetAll(aParams.get_AllUsageParams().getAll()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TAllUsageParams);
}

void GetUsageOp::ProcessOriginInternal(QuotaManager* aQuotaManager,
                                       const PersistenceType aPersistenceType,
                                       const nsACString& aOrigin,
                                       const int64_t aTimestamp,
                                       const bool aPersisted,
                                       const uint64_t aUsage) {
  if (!mGetAll && aQuotaManager->IsOriginInternal(aOrigin)) {
    return;
  }

  OriginUsage* originUsage;

  // We can't store pointers to OriginUsage objects in the hashtable
  // since AppendElement() reallocates its internal array buffer as number
  // of elements grows.
  uint32_t index;
  if (mOriginUsagesIndex.Get(aOrigin, &index)) {
    originUsage = &mOriginUsages[index];
  } else {
    index = mOriginUsages.Length();

    originUsage = mOriginUsages.AppendElement();

    originUsage->origin() = aOrigin;
    originUsage->persisted() = false;
    originUsage->usage() = 0;
    originUsage->lastAccessed() = 0;

    mOriginUsagesIndex.Put(aOrigin, index);
  }

  if (aPersistenceType == PERSISTENCE_TYPE_DEFAULT) {
    originUsage->persisted() = aPersisted;
  }

  originUsage->usage() = originUsage->usage() + aUsage;

  originUsage->lastAccessed() =
      std::max<int64_t>(originUsage->lastAccessed(), aTimestamp);
}

bool GetUsageOp::IsCanceled() {
  AssertIsOnIOThread();

  return mCanceled;
}

nsresult GetUsageOp::ProcessOrigin(QuotaManager* aQuotaManager,
                                   nsIFile* aOriginDir, const bool aPersistent,
                                   const PersistenceType aPersistenceType) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);
  MOZ_ASSERT(aOriginDir);

  int64_t timestamp;
  bool persisted;
  nsCString suffix;
  nsCString group;
  nsCString origin;
  nsresult rv = aQuotaManager->GetDirectoryMetadata2WithRestore(
      aOriginDir, aPersistent, &timestamp, &persisted, suffix, group, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  UsageInfo usageInfo;
  rv = GetUsageForOrigin(aQuotaManager, aPersistenceType, group, origin,
                         &usageInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ProcessOriginInternal(aQuotaManager, aPersistenceType, origin, timestamp,
                        persisted, usageInfo.TotalUsage());

  return NS_OK;
}

nsresult GetUsageOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("GetUsageOp::DoDirectoryWork", OTHER);

  nsresult rv;

  for (const PersistenceType type : kAllPersistenceTypes) {
    rv = TraverseRepository(aQuotaManager, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // TraverseRepository above only consulted the filesystem. We also need to
  // consider origins which may have pending quota usage, such as buffered
  // LocalStorage writes for an origin which didn't previously have any
  // LocalStorage data.

  aQuotaManager->CollectPendingOriginsForListing([&](OriginInfo* aOriginInfo) {
    ProcessOriginInternal(
        aQuotaManager, aOriginInfo->GetGroupInfo()->GetPersistenceType(),
        aOriginInfo->Origin(), aOriginInfo->LockedAccessTime(),
        aOriginInfo->LockedPersisted(), aOriginInfo->LockedUsage());
  });

  return NS_OK;
}

void GetUsageOp::GetResponse(UsageRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = AllUsageResponse();

  if (!mOriginUsages.IsEmpty()) {
    nsTArray<OriginUsage>& originUsages =
        aResponse.get_AllUsageResponse().originUsages();

    mOriginUsages.SwapElements(originUsages);
  }
}

GetOriginUsageOp::GetOriginUsageOp(const UsageRequestParams& aParams)
    : mParams(aParams.get_OriginUsageParams()),
      mGetGroupUsage(aParams.get_OriginUsageParams().getGroupUsage()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TOriginUsageParams);
}

bool GetOriginUsageOp::Init(Quota* aQuota) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaUsageRequestBase::Init(aQuota))) {
    return false;
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  QuotaManager::GetInfoFromValidatedPrincipalInfo(mParams.principalInfo(),
                                                  &mSuffix, &mGroup, &origin);

  mOriginScope.SetFromOrigin(origin);

  return true;
}

nsresult GetOriginUsageOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(mUsageInfo.TotalUsage() == 0);

  AUTO_PROFILER_LABEL("GetOriginUsageOp::DoDirectoryWork", OTHER);

  nsresult rv;

  if (mGetGroupUsage) {
    // Ensure temporary storage is initialized first. It will initialize all
    // origins for temporary storage including origins belonging to our group by
    // traversing the repositories. EnsureStorageIsInitialized is needed before
    // EnsureTemporaryStorageIsInitialized.
    rv = aQuotaManager->EnsureStorageIsInitialized();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aQuotaManager->EnsureTemporaryStorageIsInitialized();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Get cached usage and limit (the method doesn't have to stat any files).
    aQuotaManager->GetGroupUsageAndLimit(mGroup, &mUsageInfo);

    return NS_OK;
  }

  // Add all the persistent/temporary/default storage files we care about.
  for (const PersistenceType type : kAllPersistenceTypes) {
    UsageInfo usageInfo;
    rv = GetUsageForOrigin(aQuotaManager, type, mGroup,
                           mOriginScope.GetOrigin(), &usageInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mUsageInfo.Append(usageInfo);
  }

  return NS_OK;
}

void GetOriginUsageOp::GetResponse(UsageRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  OriginUsageResponse usageResponse;

  // We'll get the group usage when mGetGroupUsage is true and get the
  // origin usage when mGetGroupUsage is false.
  usageResponse.usage() = mUsageInfo.TotalUsage();

  if (mGetGroupUsage) {
    usageResponse.limit() = mUsageInfo.Limit();
  } else {
    usageResponse.fileUsage() = mUsageInfo.FileUsage();
  }

  aResponse = usageResponse;
}

bool QuotaRequestBase::Init(Quota* aQuota) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  mNeedsQuotaManagerInit = true;

  return true;
}

void QuotaRequestBase::SendResults() {
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    RequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaRequestParent::Send__delete__(this, response);
  }
}

void QuotaRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

nsresult InitOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitOp::DoDirectoryWork", OTHER);

  aQuotaManager->AssertStorageIsInitialized();

  return NS_OK;
}

void InitOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitResponse();
}

nsresult InitTemporaryStorageOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitTemporaryStorageOp::DoDirectoryWork", OTHER);

  aQuotaManager->AssertStorageIsInitialized();

  nsresult rv = aQuotaManager->EnsureTemporaryStorageIsInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void InitTemporaryStorageOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitTemporaryStorageResponse();
}

InitOriginOp::InitOriginOp(const RequestParams& aParams)
    : QuotaRequestBase(/* aExclusive */ false),
      mParams(aParams.get_InitOriginParams()),
      mCreated(false) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TInitOriginParams);
}

bool InitOriginOp::Init(Quota* aQuota) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  MOZ_ASSERT(mParams.persistenceType() != PERSISTENCE_TYPE_INVALID);

  mPersistenceType.SetValue(mParams.persistenceType());

  // Figure out which origin we're dealing with.
  nsCString origin;
  QuotaManager::GetInfoFromValidatedPrincipalInfo(mParams.principalInfo(),
                                                  &mSuffix, &mGroup, &origin);

  mOriginScope.SetFromOrigin(origin);

  return true;
}

nsresult InitOriginOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());

  AUTO_PROFILER_LABEL("InitOriginOp::DoDirectoryWork", OTHER);

  nsCOMPtr<nsIFile> directory;
  bool created;
  nsresult rv = aQuotaManager->EnsureOriginIsInitializedInternal(
      mPersistenceType.Value(), mSuffix, mGroup, mOriginScope.GetOrigin(),
      getter_AddRefs(directory), &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mCreated = created;

  return NS_OK;
}

void InitOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  InitOriginResponse response;

  response.created() = mCreated;

  aResponse = response;
}

void ResetOrClearOp::DeleteFiles(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);

  nsresult rv = aQuotaManager->AboutToClearOrigins(Nullable<PersistenceType>(),
                                                   OriginScope::FromNull(),
                                                   Nullable<Client::Type>());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIFile> directory;
  rv = NS_NewLocalFile(aQuotaManager->GetStoragePath(), false,
                       getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage directory!");
  }

  nsCOMPtr<nsIFile> storageFile;
  rv = NS_NewLocalFile(aQuotaManager->GetBasePath(), false,
                       getter_AddRefs(storageFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = storageFile->Append(NS_LITERAL_STRING(STORAGE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = storageFile->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed the storage connection
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage file!");
  }
}

nsresult ResetOrClearOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ResetOrClearOp::DoDirectoryWork", OTHER);

  if (mClear) {
    DeleteFiles(aQuotaManager);
  }

  aQuotaManager->RemoveQuota();

  aQuotaManager->ResetOrClearCompleted();

  return NS_OK;
}

void ResetOrClearOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();
  if (mClear) {
    aResponse = ClearAllResponse();
  } else {
    aResponse = ResetAllResponse();
  }
}

void ClearRequestBase::DeleteFiles(QuotaManager* aQuotaManager,
                                   PersistenceType aPersistenceType) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);

  nsresult rv = aQuotaManager->AboutToClearOrigins(
      Nullable<PersistenceType>(aPersistenceType), mOriginScope, mClientType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIFile> directory;
  rv = NS_NewLocalFile(aQuotaManager->GetStoragePath(aPersistenceType), false,
                       getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  if (NS_WARN_IF(
          NS_FAILED(directory->GetDirectoryEntries(getter_AddRefs(entries)))) ||
      !entries) {
    return;
  }

  OriginScope originScope = mOriginScope.Clone();
  if (originScope.IsOrigin()) {
    nsCString originSanitized(originScope.GetOrigin());
    SanitizeOriginString(originSanitized);
    originScope.SetOrigin(originSanitized);
  } else if (originScope.IsPrefix()) {
    nsCString originNoSuffixSanitized(originScope.GetOriginNoSuffix());
    SanitizeOriginString(originNoSuffixSanitized);
    originScope.SetOriginNoSuffix(originNoSuffixSanitized);
  }

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(file)))) &&
         file) {
    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (!isDirectory) {
      // Unknown files during clearing are allowed. Just warn if we find them.
      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    // Skip the origin directory if it doesn't match the pattern.
    if (!originScope.Matches(
            OriginScope::FromOrigin(NS_ConvertUTF16toUTF8(leafName)))) {
      continue;
    }

    bool persistent = aPersistenceType == PERSISTENCE_TYPE_PERSISTENT;

    int64_t timestamp;
    nsCString suffix;
    nsCString group;
    nsCString origin;
    bool persisted;
    rv = aQuotaManager->GetDirectoryMetadata2WithRestore(
        file, persistent, &timestamp, &persisted, suffix, group, origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    bool initialized;
    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      initialized = aQuotaManager->IsOriginInitialized(origin);
    } else {
      initialized = aQuotaManager->IsTemporaryStorageInitialized();
    }

    bool hasOtherClient = false;

    UsageInfo usageInfo;

    if (!mClientType.IsNull()) {
      // Checking whether there is any other client in the directory is needed.
      // If there is not, removing whole directory is needed.
      nsCOMPtr<nsIDirectoryEnumerator> originEntries;
      if (NS_WARN_IF(NS_FAILED(
              file->GetDirectoryEntries(getter_AddRefs(originEntries)))) ||
          !originEntries) {
        return;
      }

      nsCOMPtr<nsIFile> clientFile;
      while (NS_SUCCEEDED((rv = originEntries->GetNextFile(
                               getter_AddRefs(clientFile)))) &&
             clientFile) {
        bool isDirectory;
        rv = clientFile->IsDirectory(&isDirectory);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        if (!isDirectory) {
          continue;
        }

        nsString leafName;
        rv = clientFile->GetLeafName(leafName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        Client::Type clientType;
        rv = Client::TypeFromText(leafName, clientType);
        if (NS_FAILED(rv)) {
          UNKNOWN_FILE_WARNING(leafName);
          continue;
        }

        if (clientType != mClientType.Value()) {
          hasOtherClient = true;
          break;
        }
      }

      if (hasOtherClient) {
        nsAutoString clientDirectoryName;
        rv = Client::TypeToText(mClientType.Value(), clientDirectoryName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        rv = file->Append(clientDirectoryName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        bool exists;
        rv = file->Exists(&exists);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        if (!exists) {
          continue;
        }
      }

      if (initialized) {
        Client* client = aQuotaManager->GetClient(mClientType.Value());
        MOZ_ASSERT(client);

        Atomic<bool> dummy(false);
        rv = client->GetUsageForOrigin(aPersistenceType, group, origin, dummy,
                                       &usageInfo);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }
      }
    }

    for (uint32_t index = 0; index < 10; index++) {
      // We can't guarantee that this will always succeed on Windows...
      if (NS_SUCCEEDED((rv = file->Remove(true)))) {
        break;
      }

      NS_WARNING("Failed to remove directory, retrying after a short delay.");

      PR_Sleep(PR_MillisecondsToInterval(200));
    }

    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to remove directory, giving up!");
    }

    // If it hasn't been initialized, we don't need to update the quota and
    // notify the removing client.
    if (!initialized) {
      return;
    }

    if (aPersistenceType != PERSISTENCE_TYPE_PERSISTENT) {
      if (hasOtherClient) {
        MOZ_ASSERT(!mClientType.IsNull());

        aQuotaManager->DecreaseUsageForOrigin(aPersistenceType, group, origin,
                                              usageInfo.TotalUsage());
      } else {
        aQuotaManager->RemoveQuotaForOrigin(aPersistenceType, group, origin);
      }
    }

    aQuotaManager->OriginClearCompleted(aPersistenceType, origin, mClientType);
  }
}

nsresult ClearRequestBase::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ClearRequestBase::DoDirectoryWork", OTHER);

  if (mClear) {
    if (mPersistenceType.IsNull()) {
      for (const PersistenceType type : kAllPersistenceTypes) {
        DeleteFiles(aQuotaManager, type);
      }
    } else {
      DeleteFiles(aQuotaManager, mPersistenceType.Value());
    }
  }

  return NS_OK;
}

ClearOriginOp::ClearOriginOp(const RequestParams& aParams)
    : ClearRequestBase(/* aExclusive */ true,
                       aParams.type() == RequestParams::TClearOriginParams),
      mParams(aParams.type() == RequestParams::TClearOriginParams
                  ? aParams.get_ClearOriginParams().commonParams()
                  : aParams.get_ResetOriginParams().commonParams())

{
  MOZ_ASSERT(aParams.type() == RequestParams::TClearOriginParams ||
             aParams.type() == RequestParams::TResetOriginParams);
}

bool ClearOriginOp::Init(Quota* aQuota) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  if (mParams.persistenceTypeIsExplicit()) {
    MOZ_ASSERT(mParams.persistenceType() != PERSISTENCE_TYPE_INVALID);

    mPersistenceType.SetValue(mParams.persistenceType());
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  QuotaManager::GetInfoFromValidatedPrincipalInfo(mParams.principalInfo(),
                                                  nullptr, nullptr, &origin);

  if (mParams.matchAll()) {
    mOriginScope.SetFromPrefix(origin);
  } else {
    mOriginScope.SetFromOrigin(origin);
  }

  if (mParams.clientTypeIsExplicit()) {
    MOZ_ASSERT(mParams.clientType() != Client::TYPE_MAX);

    mClientType.SetValue(mParams.clientType());
  }

  return true;
}

void ClearOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  if (mClear) {
    aResponse = ClearOriginResponse();
  } else {
    aResponse = ResetOriginResponse();
  }
}

ClearDataOp::ClearDataOp(const RequestParams& aParams)
    : ClearRequestBase(/* aExclusive */ true,
                       /* aClear */ true),
      mParams(aParams) {
  MOZ_ASSERT(aParams.type() == RequestParams::TClearDataParams);
}

bool ClearDataOp::Init(Quota* aQuota) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  mOriginScope.SetFromPattern(mParams.pattern());

  return true;
}

void ClearDataOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ClearDataResponse();
}

PersistRequestBase::PersistRequestBase(const PrincipalInfo& aPrincipalInfo)
    : QuotaRequestBase(/* aExclusive */ false), mPrincipalInfo(aPrincipalInfo) {
  AssertIsOnOwningThread();
}

bool PersistRequestBase::Init(Quota* aQuota) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  mPersistenceType.SetValue(PERSISTENCE_TYPE_DEFAULT);

  // Figure out which origin we're dealing with.
  nsCString origin;
  QuotaManager::GetInfoFromValidatedPrincipalInfo(mPrincipalInfo, &mSuffix,
                                                  &mGroup, &origin);

  mOriginScope.SetFromOrigin(origin);

  return true;
}

PersistedOp::PersistedOp(const RequestParams& aParams)
    : PersistRequestBase(aParams.get_PersistedParams().principalInfo()),
      mPersisted(false) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistedParams);
}

nsresult PersistedOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("PersistedOp::DoDirectoryWork", OTHER);

  Nullable<bool> persisted =
      aQuotaManager->OriginPersisted(mGroup, mOriginScope.GetOrigin());

  if (!persisted.IsNull()) {
    mPersisted = persisted.Value();
    return NS_OK;
  }

  // If we get here, it means the origin hasn't been initialized yet.
  // Try to get the persisted flag from directory metadata on disk.

  nsCOMPtr<nsIFile> directory;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(mPersistenceType.Value(),
                                                     mOriginScope.GetOrigin(),
                                                     getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = directory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    // Get the persisted flag.
    bool persisted;
    rv = aQuotaManager->GetDirectoryMetadata2WithRestore(
        directory,
        /* aPersistent */ false,
        /* aTimestamp */ nullptr, &persisted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mPersisted = persisted;
  } else {
    // The directory has not been created yet.
    mPersisted = false;
  }

  return NS_OK;
}

void PersistedOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  PersistedResponse persistedResponse;
  persistedResponse.persisted() = mPersisted;

  aResponse = persistedResponse;
}

PersistOp::PersistOp(const RequestParams& aParams)
    : PersistRequestBase(aParams.get_PersistParams().principalInfo()) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistParams);
}

nsresult PersistOp::DoDirectoryWork(QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("PersistOp::DoDirectoryWork", OTHER);

  // Update directory metadata on disk first. Then, create/update the originInfo
  // if needed.
  nsCOMPtr<nsIFile> directory;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(mPersistenceType.Value(),
                                                     mOriginScope.GetOrigin(),
                                                     getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool created;
  rv = aQuotaManager->EnsureOriginDirectory(directory, &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (created) {
    int64_t timestamp;

    // Origin directory has been successfully created.
    // Create OriginInfo too if temporary storage was already initialized.
    if (aQuotaManager->IsTemporaryStorageInitialized()) {
      aQuotaManager->NoteOriginDirectoryCreated(
          mPersistenceType.Value(), mGroup, mOriginScope.GetOrigin(),
          /* aPersisted */ true, timestamp);
    } else {
      timestamp = PR_Now();
    }

    rv = CreateDirectoryMetadata2(directory, timestamp, /* aPersisted */ true,
                                  mSuffix, mGroup, mOriginScope.GetOrigin());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    // Get the persisted flag (restore the metadata file if necessary).
    bool persisted;
    rv = aQuotaManager->GetDirectoryMetadata2WithRestore(
        directory,
        /* aPersistent */ false,
        /* aTimestamp */ nullptr, &persisted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!persisted) {
      nsCOMPtr<nsIFile> file;
      nsresult rv = directory->Clone(getter_AddRefs(file));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = file->Append(NS_LITERAL_STRING(METADATA_V2_FILE_NAME));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIBinaryOutputStream> stream;
      rv = GetBinaryOutputStream(file, kUpdateFileFlag, getter_AddRefs(stream));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(stream);

      // Update origin access time while we are here.
      rv = stream->Write64(PR_Now());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Set the persisted flag to true.
      rv = stream->WriteBoolean(true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // Directory metadata has been successfully updated.
    // Update OriginInfo too if temporary storage was already initialized.
    if (aQuotaManager->IsTemporaryStorageInitialized()) {
      aQuotaManager->PersistOrigin(mGroup, mOriginScope.GetOrigin());
    }
  }

  return NS_OK;
}

void PersistOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = PersistResponse();
}

ListInitializedOriginsOp::ListInitializedOriginsOp()
    : QuotaRequestBase(/* aExclusive */ false), TraverseRepositoryHelper() {
  AssertIsOnOwningThread();
}

bool ListInitializedOriginsOp::Init(Quota* aQuota) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  mNeedsQuotaManagerInit = true;

  return true;
}

nsresult ListInitializedOriginsOp::DoDirectoryWork(
    QuotaManager* aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);

  AUTO_PROFILER_LABEL("ListInitializedOriginsOp::DoDirectoryWork", OTHER);

  nsresult rv;
  if (!aQuotaManager->IsTemporaryStorageInitialized()) {
    return NS_OK;
  }

  for (const PersistenceType type : kAllPersistenceTypes) {
    rv = TraverseRepository(aQuotaManager, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // TraverseRepository above only consulted the file-system to get a list of
  // known origins, but we also need to include origins that have pending quota
  // usage.

  aQuotaManager->CollectPendingOriginsForListing([&](OriginInfo* aOriginInfo) {
    mOrigins.AppendElement(aOriginInfo->Origin());
  });

  return NS_OK;
}

bool ListInitializedOriginsOp::IsCanceled() {
  AssertIsOnIOThread();

  return mCanceled;
}

nsresult ListInitializedOriginsOp::ProcessOrigin(
    QuotaManager* aQuotaManager, nsIFile* aOriginDir, const bool aPersistent,
    const PersistenceType aPersistenceType) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);
  MOZ_ASSERT(aOriginDir);

  int64_t timestamp;
  bool persisted;
  nsCString suffix;
  nsCString group;
  nsCString origin;
  nsresult rv = aQuotaManager->GetDirectoryMetadata2WithRestore(
      aOriginDir, aPersistent, &timestamp, &persisted, suffix, group, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aQuotaManager->IsOriginInternal(origin)) {
    return NS_OK;
  }

  mOrigins.AppendElement(origin);

  return NS_OK;
}

void ListInitializedOriginsOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ListInitializedOriginsResponse();
  if (mOrigins.IsEmpty()) {
    return;
  }

  nsTArray<nsCString>& origins =
      aResponse.get_ListInitializedOriginsResponse().origins();
  mOrigins.SwapElements(origins);
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED

// static
already_AddRefed<PrincipalVerifier> PrincipalVerifier::CreateAndDispatch(
    nsTArray<PrincipalInfo>&& aPrincipalInfos) {
  AssertIsOnIOThread();

  RefPtr<PrincipalVerifier> verifier =
      new PrincipalVerifier(std::move(aPrincipalInfos));

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(verifier));

  return verifier.forget();
}

bool PrincipalVerifier::IsPrincipalInfoValid(
    const PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  switch (aPrincipalInfo.type()) {
    // A system principal is acceptable.
    case PrincipalInfo::TSystemPrincipalInfo: {
      return true;
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), info.spec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      nsCOMPtr<nsIPrincipal> principal =
          BasePrincipal::CreateCodebasePrincipal(uri, info.attrs());
      if (NS_WARN_IF(!principal)) {
        return false;
      }

      nsCString originNoSuffix;
      rv = principal->GetOriginNoSuffix(originNoSuffix);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      if (NS_WARN_IF(originNoSuffix != info.originNoSuffix())) {
        QM_WARNING("originNoSuffix (%s) doesn't match passed one (%s)!",
                   originNoSuffix.get(), info.originNoSuffix().get());
        return false;
      }

      nsCString baseDomain;
      rv = principal->GetBaseDomain(baseDomain);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      if (NS_WARN_IF(baseDomain != info.baseDomain())) {
        QM_WARNING("baseDomain (%s) doesn't match passed one (%s)!",
                   baseDomain.get(), info.baseDomain().get());
        return false;
      }

      return true;
    }

    default: {
      break;
    }
  }

  // Null and expanded principals are not acceptable.
  return false;
}

NS_IMETHODIMP
PrincipalVerifier::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  for (auto& principalInfo : mPrincipalInfos) {
    MOZ_DIAGNOSTIC_ASSERT(IsPrincipalInfoValid(principalInfo));
  }

  return NS_OK;
}

#endif

nsresult StorageOperationBase::GetDirectoryMetadata(nsIFile* aDirectory,
                                                    int64_t& aTimestamp,
                                                    nsACString& aGroup,
                                                    nsACString& aOrigin,
                                                    Nullable<bool>& aIsApp) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv =
      GetBinaryInputStream(aDirectory, NS_LITERAL_STRING(METADATA_FILE_NAME),
                           getter_AddRefs(binaryStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString group;
  rv = binaryStream->ReadCString(group);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString origin;
  rv = binaryStream->ReadCString(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Nullable<bool> isApp;
  bool value;
  if (NS_SUCCEEDED(binaryStream->ReadBoolean(&value))) {
    isApp.SetValue(value);
  }

  aTimestamp = timestamp;
  aGroup = group;
  aOrigin = origin;
  aIsApp = std::move(isApp);
  return NS_OK;
}

nsresult StorageOperationBase::GetDirectoryMetadata2(
    nsIFile* aDirectory, int64_t& aTimestamp, nsACString& aSuffix,
    nsACString& aGroup, nsACString& aOrigin, bool& aIsApp) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv =
      GetBinaryInputStream(aDirectory, NS_LITERAL_STRING(METADATA_V2_FILE_NAME),
                           getter_AddRefs(binaryStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool persisted;
  rv = binaryStream->ReadBoolean(&persisted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t reservedData1;
  rv = binaryStream->Read32(&reservedData1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t reservedData2;
  rv = binaryStream->Read32(&reservedData2);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString suffix;
  rv = binaryStream->ReadCString(suffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString group;
  rv = binaryStream->ReadCString(group);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString origin;
  rv = binaryStream->ReadCString(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isApp;
  rv = binaryStream->ReadBoolean(&isApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aTimestamp = timestamp;
  aSuffix = suffix;
  aGroup = group;
  aOrigin = origin;
  aIsApp = isApp;
  return NS_OK;
}

nsresult StorageOperationBase::RemoveObsoleteOrigin(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);

  QM_WARNING(
      "Deleting obsolete %s directory that is no longer a legal "
      "origin!",
      NS_ConvertUTF16toUTF8(aOriginProps.mLeafName).get());

  nsresult rv = aOriginProps.mDirectory->Remove(/* recursive */ true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult StorageOperationBase::ProcessOriginDirectories() {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mOriginProps.IsEmpty());

  nsresult rv;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  nsTArray<PrincipalInfo> principalInfos;
#endif

  for (auto& originProps : mOriginProps) {
    switch (originProps.mType) {
      case OriginProps::eChrome: {
        QuotaManager::GetInfoForChrome(
            &originProps.mSuffix, &originProps.mGroup, &originProps.mOrigin);
        break;
      }

      case OriginProps::eContent: {
        RefPtr<MozURL> specURL;
        nsresult rv = MozURL::Init(getter_AddRefs(specURL), originProps.mSpec);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          // If a URL cannot be understood by MozURL during restoring or
          // upgrading, either marking the directory as broken or removing that
          // corresponding directory should be considered. While the cost of
          // marking the directory as broken during a upgrade is too high,
          // removing the directory is a better choice rather than blocking the
          // initialization or the upgrade.
          QM_WARNING(
              "A URL (%s) for the origin directory is not recognized by "
              "MozURL. The directory will be deleted for now to pass the "
              "initialization or the upgrade.",
              originProps.mSpec.get());

          originProps.mType = OriginProps::eObsolete;
          break;
        }

        nsCString originNoSuffix;
        specURL->Origin(originNoSuffix);

        nsCString baseDomain;
        rv = specURL->BaseDomain(baseDomain);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        ContentPrincipalInfo contentPrincipalInfo;
        contentPrincipalInfo.attrs() = originProps.mAttrs;
        contentPrincipalInfo.originNoSuffix() = originNoSuffix;
        contentPrincipalInfo.spec() = originProps.mSpec;
        contentPrincipalInfo.baseDomain() = baseDomain;

        PrincipalInfo principalInfo(contentPrincipalInfo);

        QuotaManager::GetInfoFromValidatedPrincipalInfo(
            principalInfo, &originProps.mSuffix, &originProps.mGroup,
            &originProps.mOrigin);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
        principalInfos.AppendElement(principalInfo);
#endif

        break;
      }

      case OriginProps::eObsolete: {
        // There's no way to get info for obsolete origins.
        break;
      }

      default:
        MOZ_CRASH("Bad type!");
    }
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (!principalInfos.IsEmpty()) {
    RefPtr<PrincipalVerifier> principalVerifier =
        PrincipalVerifier::CreateAndDispatch(std::move(principalInfos));
  }
#endif

  // Don't try to upgrade obsolete origins, remove them right after we detect
  // them.
  for (auto& originProps : mOriginProps) {
    if (originProps.mType == OriginProps::eObsolete) {
      MOZ_ASSERT(originProps.mSuffix.IsEmpty());
      MOZ_ASSERT(originProps.mGroup.IsEmpty());
      MOZ_ASSERT(originProps.mOrigin.IsEmpty());

      rv = RemoveObsoleteOrigin(originProps);
    } else {
      MOZ_ASSERT(!originProps.mGroup.IsEmpty());
      MOZ_ASSERT(!originProps.mOrigin.IsEmpty());

      rv = ProcessOriginDirectory(originProps);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult StorageOperationBase::OriginProps::Init(nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsString leafName;
  nsresult rv = aDirectory->GetLeafName(leafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString spec;
  OriginAttributes attrs;
  OriginParser::ResultType result =
      OriginParser::ParseOrigin(NS_ConvertUTF16toUTF8(leafName), spec, &attrs);
  if (NS_WARN_IF(result == OriginParser::InvalidOrigin)) {
    mType = OriginProps::eInvalid;
    return NS_ERROR_FAILURE;
  }

  mDirectory = aDirectory;
  mLeafName = leafName;
  mSpec = spec;
  mAttrs = attrs;
  if (result == OriginParser::ObsoleteOrigin) {
    mType = eObsolete;
  } else if (mSpec.EqualsLiteral(kChromeOrigin)) {
    mType = eChrome;
  } else {
    mType = eContent;
  }

  return NS_OK;
}

// static
auto OriginParser::ParseOrigin(const nsACString& aOrigin, nsCString& aSpec,
                               OriginAttributes* aAttrs) -> ResultType {
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(aAttrs);

  OriginAttributes originAttributes;

  nsCString originNoSuffix;
  bool ok = originAttributes.PopulateFromOrigin(aOrigin, originNoSuffix);
  if (!ok) {
    return InvalidOrigin;
  }

  OriginParser parser(originNoSuffix, originAttributes);
  return parser.Parse(aSpec, aAttrs);
}

auto OriginParser::Parse(nsACString& aSpec, OriginAttributes* aAttrs)
    -> ResultType {
  MOZ_ASSERT(aAttrs);

  while (mTokenizer.hasMoreTokens()) {
    const nsDependentCSubstring& token = mTokenizer.nextToken();

    HandleToken(token);

    if (mError) {
      break;
    }

    if (!mHandledTokens.IsEmpty()) {
      mHandledTokens.AppendLiteral(", ");
    }
    mHandledTokens.Append('\'');
    mHandledTokens.Append(token);
    mHandledTokens.Append('\'');
  }

  if (!mError && mTokenizer.separatorAfterCurrentToken()) {
    HandleTrailingSeparator();
  }

  if (mError) {
    QM_WARNING("Origin '%s' failed to parse, handled tokens: %s", mOrigin.get(),
               mHandledTokens.get());

    return mSchemeType == eChrome ? ObsoleteOrigin : InvalidOrigin;
  }

  MOZ_ASSERT(mState == eComplete || mState == eHandledTrailingSeparator);

  // For IPv6 URL, it should at least have three groups.
  MOZ_ASSERT_IF(mIPGroup > 0, mIPGroup >= 3);

  *aAttrs = mOriginAttributes;
  nsAutoCString spec(mScheme);

  if (mSchemeType == eFile) {
    spec.AppendLiteral("://");

    if (mUniversalFileOrigin) {
      MOZ_ASSERT(mPathnameComponents.Length() == 1);

      spec.Append(mPathnameComponents[0]);
    } else {
      for (uint32_t count = mPathnameComponents.Length(), index = 0;
           index < count; index++) {
        spec.Append('/');
        spec.Append(mPathnameComponents[index]);
      }
    }

    aSpec = spec;

    return ValidOrigin;
  }

  if (mSchemeType == eAbout) {
    if (mMaybeObsolete) {
      // The "moz-safe-about+++home" was acciedntally created by a buggy nightly
      // and can be safely removed.
      return mHost.EqualsLiteral("home") ? ObsoleteOrigin : InvalidOrigin;
    }
    spec.Append(':');
  } else if (mSchemeType != eChrome) {
    spec.AppendLiteral("://");
  }

  spec.Append(mHost);

  if (!mPort.IsNull()) {
    spec.Append(':');
    spec.AppendInt(mPort.Value());
  }

  aSpec = spec;

  return mScheme.EqualsLiteral("app") ? ObsoleteOrigin : ValidOrigin;
}

void OriginParser::HandleScheme(const nsDependentCSubstring& aToken) {
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingAppIdOrScheme || mState == eExpectingScheme);

  bool isAbout = false;
  bool isMozSafeAbout = false;
  bool isFile = false;
  bool isChrome = false;
  if (aToken.EqualsLiteral("http") || aToken.EqualsLiteral("https") ||
      (isAbout = aToken.EqualsLiteral("about") ||
                 (isMozSafeAbout = aToken.EqualsLiteral("moz-safe-about"))) ||
      aToken.EqualsLiteral("indexeddb") ||
      (isFile = aToken.EqualsLiteral("file")) || aToken.EqualsLiteral("app") ||
      aToken.EqualsLiteral("resource") ||
      aToken.EqualsLiteral("moz-extension") ||
      (isChrome = aToken.EqualsLiteral(kChromeOrigin))) {
    mScheme = aToken;

    if (isAbout) {
      mSchemeType = eAbout;
      mState = isMozSafeAbout ? eExpectingEmptyToken1OrHost : eExpectingHost;
    } else if (isChrome) {
      mSchemeType = eChrome;
      if (mTokenizer.hasMoreTokens()) {
        mError = true;
      }
      mState = eComplete;
    } else {
      if (isFile) {
        mSchemeType = eFile;
      }
      mState = eExpectingEmptyToken1;
    }

    return;
  }

  QM_WARNING("'%s' is not a valid scheme!", nsCString(aToken).get());

  mError = true;
}

void OriginParser::HandlePathnameComponent(
    const nsDependentCSubstring& aToken) {
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingEmptyTokenOrDriveLetterOrPathnameComponent ||
             mState == eExpectingEmptyTokenOrPathnameComponent);
  MOZ_ASSERT(mSchemeType == eFile);

  mPathnameComponents.AppendElement(aToken);

  mState = mTokenizer.hasMoreTokens() ? eExpectingEmptyTokenOrPathnameComponent
                                      : eComplete;
}

void OriginParser::HandleToken(const nsDependentCSubstring& aToken) {
  switch (mState) {
    case eExpectingAppIdOrScheme: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected an app id or scheme (not an empty string)!");

        mError = true;
        return;
      }

      if (IsAsciiDigit(aToken.First())) {
        // nsDependentCSubstring doesn't provice ToInteger()
        nsCString token(aToken);

        nsresult rv;
        Unused << token.ToInteger(&rv);
        if (NS_SUCCEEDED(rv)) {
          mState = eExpectingInMozBrowser;
          return;
        }
      }

      HandleScheme(aToken);

      return;
    }

    case eExpectingInMozBrowser: {
      if (aToken.Length() != 1) {
        QM_WARNING("'%d' is not a valid length for the inMozBrowser flag!",
                   aToken.Length());

        mError = true;
        return;
      }

      if (aToken.First() == 't') {
        mInIsolatedMozBrowser = true;
      } else if (aToken.First() == 'f') {
        mInIsolatedMozBrowser = false;
      } else {
        QM_WARNING("'%s' is not a valid value for the inMozBrowser flag!",
                   nsCString(aToken).get());

        mError = true;
        return;
      }

      mState = eExpectingScheme;

      return;
    }

    case eExpectingScheme: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a scheme (not an empty string)!");

        mError = true;
        return;
      }

      HandleScheme(aToken);

      return;
    }

    case eExpectingEmptyToken1: {
      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the first empty token!");

        mError = true;
        return;
      }

      mState = eExpectingEmptyToken2;

      return;
    }

    case eExpectingEmptyToken2: {
      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the second empty token!");

        mError = true;
        return;
      }

      if (mSchemeType == eFile) {
        mState = eExpectingEmptyTokenOrUniversalFileOrigin;
      } else {
        if (mSchemeType == eAbout) {
          mMaybeObsolete = true;
        }
        mState = eExpectingHost;
      }

      return;
    }

    case eExpectingEmptyTokenOrUniversalFileOrigin: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrDriveLetterOrPathnameComponent
                     : eComplete;

        return;
      }

      if (aToken.EqualsLiteral("UNIVERSAL_FILE_URI_ORIGIN")) {
        mUniversalFileOrigin = true;

        mPathnameComponents.AppendElement(aToken);

        mState = eComplete;

        return;
      }

      QM_WARNING(
          "Expected the third empty token or "
          "UNIVERSAL_FILE_URI_ORIGIN!");

      mError = true;
      return;
    }

    case eExpectingHost: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a host (not an empty string)!");

        mError = true;
        return;
      }

      mHost = aToken;

      if (aToken.First() == '[') {
        MOZ_ASSERT(mIPGroup == 0);

        ++mIPGroup;
        mState = eExpectingIPV6Token;

        MOZ_ASSERT(mTokenizer.hasMoreTokens());
        return;
      }

      mState = mTokenizer.hasMoreTokens() ? eExpectingPort : eComplete;

      return;
    }

    case eExpectingPort: {
      MOZ_ASSERT(mSchemeType == eNone);

      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a port (not an empty string)!");

        mError = true;
        return;
      }

      // nsDependentCSubstring doesn't provice ToInteger()
      nsCString token(aToken);

      nsresult rv;
      uint32_t port = token.ToInteger(&rv);
      if (NS_SUCCEEDED(rv)) {
        mPort.SetValue() = port;
      } else {
        QM_WARNING("'%s' is not a valid port number!", token.get());

        mError = true;
        return;
      }

      mState = eComplete;

      return;
    }

    case eExpectingEmptyTokenOrDriveLetterOrPathnameComponent: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        mPathnameComponents.AppendElement(EmptyCString());

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      if (aToken.Length() == 1 && IsAsciiAlpha(aToken.First())) {
        mMaybeDriveLetter = true;

        mPathnameComponents.AppendElement(aToken);

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    case eExpectingEmptyTokenOrPathnameComponent: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        if (mMaybeDriveLetter) {
          MOZ_ASSERT(mPathnameComponents.Length() == 1);

          nsCString& pathnameComponent = mPathnameComponents[0];
          pathnameComponent.Append(':');

          mMaybeDriveLetter = false;
        } else {
          mPathnameComponents.AppendElement(EmptyCString());
        }

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    case eExpectingEmptyToken1OrHost: {
      MOZ_ASSERT(mSchemeType == eAbout &&
                 mScheme.EqualsLiteral("moz-safe-about"));

      if (aToken.IsEmpty()) {
        mState = eExpectingEmptyToken2;
      } else {
        mHost = aToken;
        mState = mTokenizer.hasMoreTokens() ? eExpectingPort : eComplete;
      }

      return;
    }

    case eExpectingIPV6Token: {
      // A safe check for preventing infinity recursion.
      if (++mIPGroup > 8) {
        mError = true;
        return;
      }

      mHost.AppendLiteral(":");
      mHost.Append(aToken);
      if (!aToken.IsEmpty() && aToken.Last() == ']') {
        mState = mTokenizer.hasMoreTokens() ? eExpectingPort : eComplete;
      }

      return;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }
}

void OriginParser::HandleTrailingSeparator() {
  MOZ_ASSERT(mState == eComplete);
  MOZ_ASSERT(mSchemeType == eFile);

  mPathnameComponents.AppendElement(EmptyCString());

  mState = eHandledTrailingSeparator;
}

nsresult RepositoryOperationBase::ProcessRepository() {
  AssertIsOnIOThread();

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(mDirectory->Exists(&exists)));
  MOZ_ASSERT(exists);

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv = mDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  while (true) {
    nsCOMPtr<nsIFile> originDir;
    rv = entries->GetNextFile(getter_AddRefs(originDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!originDir) {
      break;
    }

    bool isDirectory;
    rv = originDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      nsString leafName;
      rv = originDir->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Unknown files during upgrade are allowed. Just warn if we find them.
      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    OriginProps originProps;
    rv = originProps.Init(originDir);
    // Bypass invalid origins while upgrading
    if (NS_WARN_IF(originProps.mType == OriginProps::eInvalid)) {
      continue;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (originProps.mType != OriginProps::eObsolete) {
      bool removed;
      rv = PrepareOriginDirectory(originProps, &removed);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (removed) {
        continue;
      }
    }

    mOriginProps.AppendElement(std::move(originProps));
  }

  if (mOriginProps.IsEmpty()) {
    return NS_OK;
  }

  rv = ProcessOriginDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

template <typename UpgradeMethod>
nsresult RepositoryOperationBase::MaybeUpgradeClients(
    const OriginProps& aOriginProps, UpgradeMethod aMethod) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);
  MOZ_ASSERT(aMethod);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv =
      aOriginProps.mDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  while (true) {
    nsCOMPtr<nsIFile> file;
    rv = entries->GetNextFile(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!file) {
      break;
    }

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      // Unknown files during upgrade are allowed. Just warn if we find them.
      if (!IsOriginMetadata(leafName) && !IsTempMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    bool removed;
    rv = PrepareClientDirectory(file, leafName, removed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (removed) {
      continue;
    }

    Client::Type clientType;
    rv = Client::TypeFromText(leafName, clientType);
    if (NS_FAILED(rv)) {
      UNKNOWN_FILE_WARNING(leafName);
      continue;
    }

    Client* client = quotaManager->GetClient(clientType);
    MOZ_ASSERT(client);

    rv = (client->*aMethod)(file);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult RepositoryOperationBase::PrepareClientDirectory(
    nsIFile* aFile, const nsAString& aLeafName, bool& aRemoved) {
  AssertIsOnIOThread();

  aRemoved = false;
  return NS_OK;
}

nsresult CreateOrUpgradeDirectoryMetadataHelper::MaybeUpgradeOriginDirectory(
    nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIFile> metadataFile;
  nsresult rv = aDirectory->Clone(getter_AddRefs(metadataFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = metadataFile->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = metadataFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    // Directory structure upgrade needed.
    // Move all files to IDB specific directory.

    nsString idbDirectoryName;
    rv = Client::TypeToText(Client::IDB, idbDirectoryName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> idbDirectory;
    rv = aDirectory->Clone(getter_AddRefs(idbDirectory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = idbDirectory->Append(idbDirectoryName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = idbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
      NS_WARNING("IDB directory already exists!");

      bool isDirectory;
      rv = idbDirectory->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (NS_WARN_IF(!isDirectory)) {
        return NS_ERROR_UNEXPECTED;
      }
    } else {
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    nsCOMPtr<nsIDirectoryEnumerator> entries;
    rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> file;
    while (NS_SUCCEEDED((rv = entries->GetNextFile(getter_AddRefs(file)))) &&
           file) {
      nsString leafName;
      rv = file->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!leafName.Equals(idbDirectoryName)) {
        rv = file->MoveTo(idbDirectory, EmptyString());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    rv = metadataFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult CreateOrUpgradeDirectoryMetadataHelper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);
  MOZ_ASSERT(aRemoved);

  nsresult rv;

  if (mPersistent) {
    rv = MaybeUpgradeOriginDirectory(aOriginProps.mDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool persistent = QuotaManager::IsOriginInternal(aOriginProps.mSpec);
    aOriginProps.mTimestamp =
        GetLastModifiedTime(aOriginProps.mDirectory, persistent);
  } else {
    int64_t timestamp;
    nsCString group;
    nsCString origin;
    Nullable<bool> isApp;
    rv = GetDirectoryMetadata(aOriginProps.mDirectory, timestamp, group, origin,
                              isApp);
    if (NS_FAILED(rv)) {
      aOriginProps.mTimestamp =
          GetLastModifiedTime(aOriginProps.mDirectory, mPersistent);
      aOriginProps.mNeedsRestore = true;
    } else if (!isApp.IsNull()) {
      aOriginProps.mIgnore = true;
    }
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult CreateOrUpgradeDirectoryMetadataHelper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  nsresult rv;

  if (mPersistent) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp, aOriginProps.mSuffix,
                                 aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Move internal origins to new persistent storage.
    if (QuotaManager::IsOriginInternal(aOriginProps.mSpec)) {
      if (!mPermanentStorageDir) {
        QuotaManager* quotaManager = QuotaManager::Get();
        MOZ_ASSERT(quotaManager);

        const nsString& permanentStoragePath =
            quotaManager->GetStoragePath(PERSISTENCE_TYPE_PERSISTENT);

        rv = NS_NewLocalFile(permanentStoragePath, false,
                             getter_AddRefs(mPermanentStorageDir));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      nsString leafName;
      rv = aOriginProps.mDirectory->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIFile> newDirectory;
      rv = mPermanentStorageDir->Clone(getter_AddRefs(newDirectory));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = newDirectory->Append(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      bool exists;
      rv = newDirectory->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (exists) {
        QM_WARNING("Found %s in storage/persistent and storage/permanent !",
                   NS_ConvertUTF16toUTF8(leafName).get());

        rv = aOriginProps.mDirectory->Remove(/* recursive */ true);
      } else {
        rv = aOriginProps.mDirectory->MoveTo(mPermanentStorageDir,
                                             EmptyString());
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  } else if (aOriginProps.mNeedsRestore) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp, aOriginProps.mSuffix,
                                 aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (!aOriginProps.mIgnore) {
    nsCOMPtr<nsIFile> file;
    rv = aOriginProps.mDirectory->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = file->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIBinaryOutputStream> stream;
    rv = GetBinaryOutputStream(file, kAppendFileFlag, getter_AddRefs(stream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(stream);

    // Currently unused (used to be isApp).
    rv = stream->WriteBoolean(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom0_0To1_0Helper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);
  MOZ_ASSERT(aRemoved);

  int64_t timestamp;
  nsCString group;
  nsCString origin;
  Nullable<bool> isApp;
  nsresult rv = GetDirectoryMetadata(aOriginProps.mDirectory, timestamp, group,
                                     origin, isApp);
  if (NS_FAILED(rv) || isApp.IsNull()) {
    aOriginProps.mTimestamp =
        GetLastModifiedTime(aOriginProps.mDirectory, mPersistent);
    aOriginProps.mNeedsRestore = true;
  } else {
    aOriginProps.mTimestamp = timestamp;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom0_0To1_0Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  nsresult rv;

  if (aOriginProps.mNeedsRestore) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp, aOriginProps.mSuffix,
                                 aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv =
      CreateDirectoryMetadata2(aOriginProps.mDirectory, aOriginProps.mTimestamp,
                               /* aPersisted */ false, aOriginProps.mSuffix,
                               aOriginProps.mGroup, aOriginProps.mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString oldName;
  rv = aOriginProps.mDirectory->GetLeafName(oldName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString originSanitized(aOriginProps.mOrigin);
  SanitizeOriginString(originSanitized);

  NS_ConvertASCIItoUTF16 newName(originSanitized);

  if (!oldName.Equals(newName)) {
    rv = aOriginProps.mDirectory->RenameTo(nullptr, newName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom1_0To2_0Helper::MaybeRemoveMorgueDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);

  // The Cache API was creating top level morgue directories by accident for
  // a short time in nightly.  This unfortunately prevents all storage from
  // working.  So recover these profiles permanently by removing these corrupt
  // directories as part of this upgrade.

  nsCOMPtr<nsIFile> morgueDir;
  nsresult rv = aOriginProps.mDirectory->Clone(getter_AddRefs(morgueDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = morgueDir->Append(NS_LITERAL_STRING("morgue"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = morgueDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    QM_WARNING("Deleting accidental morgue directory!");

    rv = morgueDir->Remove(/* recursive */ true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom1_0To2_0Helper::MaybeRemoveAppsData(
    const OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom1_0To2_0Helper::MaybeStripObsoleteOriginAttributes(
    const OriginProps& aOriginProps, bool* aStripped) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);

  const nsAString& oldLeafName = aOriginProps.mLeafName;

  nsCString originSanitized(aOriginProps.mOrigin);
  SanitizeOriginString(originSanitized);

  NS_ConvertUTF8toUTF16 newLeafName(originSanitized);

  if (oldLeafName == newLeafName) {
    *aStripped = false;
    return NS_OK;
  }

  nsresult rv = CreateDirectoryMetadata(
      aOriginProps.mDirectory, aOriginProps.mTimestamp, aOriginProps.mSuffix,
      aOriginProps.mGroup, aOriginProps.mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv =
      CreateDirectoryMetadata2(aOriginProps.mDirectory, aOriginProps.mTimestamp,
                               /* aPersisted */ false, aOriginProps.mSuffix,
                               aOriginProps.mGroup, aOriginProps.mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> newFile;
  rv = aOriginProps.mDirectory->GetParent(getter_AddRefs(newFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = newFile->Append(newLeafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = newFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    QM_WARNING(
        "Can't rename %s directory, %s directory already exists, "
        "removing!",
        NS_ConvertUTF16toUTF8(oldLeafName).get(),
        NS_ConvertUTF16toUTF8(newLeafName).get());

    rv = aOriginProps.mDirectory->Remove(/* recursive */ true);
  } else {
    rv = aOriginProps.mDirectory->RenameTo(nullptr, newLeafName);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aStripped = true;
  return NS_OK;
}

nsresult UpgradeStorageFrom1_0To2_0Helper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);
  MOZ_ASSERT(aRemoved);

  nsresult rv = MaybeRemoveMorgueDirectory(aOriginProps);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = MaybeUpgradeClients(aOriginProps, &Client::UpgradeStorageFrom1_0To2_0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool removed;
  rv = MaybeRemoveAppsData(aOriginProps, &removed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (removed) {
    *aRemoved = true;
    return NS_OK;
  }

  int64_t timestamp;
  nsCString group;
  nsCString origin;
  Nullable<bool> isApp;
  rv = GetDirectoryMetadata(aOriginProps.mDirectory, timestamp, group, origin,
                            isApp);
  if (NS_FAILED(rv) || isApp.IsNull()) {
    aOriginProps.mNeedsRestore = true;
  }

  nsCString suffix;
  rv = GetDirectoryMetadata2(aOriginProps.mDirectory, timestamp, suffix, group,
                             origin, isApp.SetValue());
  if (NS_FAILED(rv)) {
    aOriginProps.mTimestamp =
        GetLastModifiedTime(aOriginProps.mDirectory, mPersistent);
    aOriginProps.mNeedsRestore2 = true;
  } else {
    aOriginProps.mTimestamp = timestamp;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom1_0To2_0Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  bool stripped;
  nsresult rv = MaybeStripObsoleteOriginAttributes(aOriginProps, &stripped);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (stripped) {
    return NS_OK;
  }

  if (aOriginProps.mNeedsRestore) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp, aOriginProps.mSuffix,
                                 aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aOriginProps.mNeedsRestore2) {
    rv = CreateDirectoryMetadata2(aOriginProps.mDirectory,
                                  aOriginProps.mTimestamp,
                                  /* aPersisted */ false, aOriginProps.mSuffix,
                                  aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom2_0To2_1Helper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);
  MOZ_ASSERT(aRemoved);

  nsresult rv =
      MaybeUpgradeClients(aOriginProps, &Client::UpgradeStorageFrom2_0To2_1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t timestamp;
  nsCString group;
  nsCString origin;
  Nullable<bool> isApp;
  rv = GetDirectoryMetadata(aOriginProps.mDirectory, timestamp, group, origin,
                            isApp);
  if (NS_FAILED(rv) || isApp.IsNull()) {
    aOriginProps.mNeedsRestore = true;
  }

  nsCString suffix;
  rv = GetDirectoryMetadata2(aOriginProps.mDirectory, timestamp, suffix, group,
                             origin, isApp.SetValue());
  if (NS_FAILED(rv)) {
    aOriginProps.mTimestamp =
        GetLastModifiedTime(aOriginProps.mDirectory, mPersistent);
    aOriginProps.mNeedsRestore2 = true;
  } else {
    aOriginProps.mTimestamp = timestamp;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom2_0To2_1Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  nsresult rv;

  if (aOriginProps.mNeedsRestore) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp, aOriginProps.mSuffix,
                                 aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aOriginProps.mNeedsRestore2) {
    rv = CreateDirectoryMetadata2(aOriginProps.mDirectory,
                                  aOriginProps.mTimestamp,
                                  /* aPersisted */ false, aOriginProps.mSuffix,
                                  aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom2_1To2_2Helper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);
  MOZ_ASSERT(aRemoved);

  nsresult rv =
      MaybeUpgradeClients(aOriginProps, &Client::UpgradeStorageFrom2_1To2_2);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t timestamp;
  nsCString group;
  nsCString origin;
  Nullable<bool> isApp;
  rv = GetDirectoryMetadata(aOriginProps.mDirectory, timestamp, group, origin,
                            isApp);
  if (NS_FAILED(rv) || isApp.IsNull()) {
    aOriginProps.mNeedsRestore = true;
  }

  nsCString suffix;
  rv = GetDirectoryMetadata2(aOriginProps.mDirectory, timestamp, suffix, group,
                             origin, isApp.SetValue());
  if (NS_FAILED(rv)) {
    aOriginProps.mTimestamp =
        GetLastModifiedTime(aOriginProps.mDirectory, mPersistent);
    aOriginProps.mNeedsRestore2 = true;
  } else {
    aOriginProps.mTimestamp = timestamp;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom2_1To2_2Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  nsresult rv;

  if (aOriginProps.mNeedsRestore) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp, aOriginProps.mSuffix,
                                 aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aOriginProps.mNeedsRestore2) {
    rv = CreateDirectoryMetadata2(aOriginProps.mDirectory,
                                  aOriginProps.mTimestamp,
                                  /* aPersisted */ false, aOriginProps.mSuffix,
                                  aOriginProps.mGroup, aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom2_1To2_2Helper::PrepareClientDirectory(
    nsIFile* aFile, const nsAString& aLeafName, bool& aRemoved) {
  AssertIsOnIOThread();

  if (Client::IsDeprecatedClient(aLeafName)) {
    nsresult rv = aFile->Remove(true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aRemoved = true;
  } else {
    aRemoved = false;
  }

  return NS_OK;
}

nsresult RestoreDirectoryMetadata2Helper::RestoreMetadata2File() {
  AssertIsOnIOThread();

  nsresult rv;

  OriginProps originProps;
  rv = originProps.Init(mDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  originProps.mTimestamp = GetLastModifiedTime(mDirectory, mPersistent);

  mOriginProps.AppendElement(std::move(originProps));

  rv = ProcessOriginDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult RestoreDirectoryMetadata2Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  // We don't have any approach to restore aPersisted, so reset it to false.
  nsresult rv =
      CreateDirectoryMetadata2(aOriginProps.mDirectory, aOriginProps.mTimestamp,
                               /* aPersisted */ false, aOriginProps.mSuffix,
                               aOriginProps.mGroup, aOriginProps.mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

}  // namespace quota
}  // namespace dom
}  // namespace mozilla
