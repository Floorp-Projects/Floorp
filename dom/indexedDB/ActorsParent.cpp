/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <new>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <utility>
#include "ActorsParentCommon.h"
#include "CrashAnnotations.h"
#include "DBSchema.h"
#include "ErrorList.h"
#include "FileInfoFwd.h"
#include "FileInfoT.h"
#include "FileManager.h"
#include "FileManagerBase.h"
#include "IDBCursorType.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IndexedDBCommon.h"
#include "IndexedDatabaseInlines.h"
#include "IndexedDatabaseManager.h"
#include "KeyPath.h"
#include "MainThreadUtils.h"
#include "PermissionRequestBase.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"
#include "SafeRefPtr.h"
#include "SchemaUpgrades.h"
#include "chrome/common/ipc_channel.h"
#include "ipc/IPCMessageUtils.h"
#include "js/RootingAPI.h"
#include "js/StructuredClone.h"
#include "js/Value.h"
#include "jsapi.h"
#include "mozIStorageAsyncConnection.h"
#include "mozIStorageConnection.h"
#include "mozIStorageFunction.h"
#include "mozIStorageProgressHandler.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozIStorageValueArray.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozilla/Algorithm.h"
#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/ArrayIterator.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Casting.h"
#include "mozilla/CondVar.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/Logging.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/NotNull.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/RefCountType.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RemoteLazyInputStreamParent.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Scoped.h"
#include "mozilla/SnappyCompressOutputStream.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/FileHandleStorage.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/dom/IPCBlob.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/IndexedDatabase.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/PBackgroundMutableFileParent.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/filehandle/ActorsParent.h"
#include "mozilla/dom/indexedDB/IDBResult.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBCursor.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBCursorParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabase.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseFileParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseRequestParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactory.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryRequestParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBRequest.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBRequestParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBTransactionParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBVersionChangeTransactionParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIndexedDBUtilsParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/quota/CachingDatabaseConnection.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/ClientImpl.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/DecryptingInputStream_impl.h"
#include "mozilla/dom/quota/EncryptingOutputStream_impl.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/fallible.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/mozalloc.h"
#include "mozilla/PRemoteLazyInputStreamParent.h"
#include "mozilla/storage/Variant.h"
#include "nsBaseHashtable.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsContentUtils.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsEscape.h"
#include "nsExceptionHandler.h"
#include "nsHashKeys.h"
#include "nsIAsyncInputStream.h"
#include "nsID.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIEventTarget.h"
#include "nsIFile.h"
#include "nsIFileProtocolHandler.h"
#include "nsIFileStreams.h"
#include "nsIFileURL.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIProtocolHandler.h"
#include "nsIRunnable.h"
#include "nsISupports.h"
#include "nsISupportsPriority.h"
#include "nsISupportsUtils.h"
#include "nsIThread.h"
#include "nsIThreadInternal.h"
#include "nsITimer.h"
#include "nsIURIMutator.h"
#include "nsIVariant.h"
#include "nsLiteralString.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsRefPtrHashtable.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsTLiteralString.h"
#include "nsTStringRepr.h"
#include "nsThreadPool.h"
#include "nsThreadUtils.h"
#include "nscore.h"
#include "prinrval.h"
#include "prio.h"
#include "prsystem.h"
#include "prthread.h"
#include "prtime.h"
#include "prtypes.h"
#include "snappy/snappy.h"

struct JSContext;
class JSObject;
template <class T>
class nsPtrHashKey;

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

#define IDB_DEBUG_LOG(_args) \
  MOZ_LOG(IndexedDatabaseManager::GetLoggingModule(), LogLevel::Debug, _args)

#if defined(MOZ_WIDGET_ANDROID)
#  define IDB_MOBILE
#endif

namespace mozilla {

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRFileDesc, PRFileDesc,
                                          PR_Close);

namespace dom::indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;
using mozilla::dom::quota::Client;

namespace {

class ConnectionPool;
class Database;
struct DatabaseActorInfo;
class DatabaseFile;
class DatabaseLoggingInfo;
class DatabaseMaintenance;
class Factory;
class Maintenance;
class MutableFile;
class OpenDatabaseOp;
class TransactionBase;
class TransactionDatabaseOperationBase;
class VersionChangeTransaction;
template <bool StatementHasIndexKeyBindings>
struct ValuePopulateResponseHelper;

/*******************************************************************************
 * Constants
 ******************************************************************************/

const int32_t kStorageProgressGranularity = 1000;

// Changing the value here will override the page size of new databases only.
// A journal mode change and VACUUM are needed to change existing databases, so
// the best way to do that is to use the schema version upgrade mechanism.
const uint32_t kSQLitePageSizeOverride =
#ifdef IDB_MOBILE
    2048;
#else
    4096;
#endif

static_assert(kSQLitePageSizeOverride == /* mozStorage default */ 0 ||
                  (kSQLitePageSizeOverride % 2 == 0 &&
                   kSQLitePageSizeOverride >= 512 &&
                   kSQLitePageSizeOverride <= 65536),
              "Must be 0 (disabled) or a power of 2 between 512 and 65536!");

// Set to -1 to use SQLite's default, 0 to disable, or some positive number to
// enforce a custom limit.
const int32_t kMaxWALPages = 5000;  // 20MB on desktop, 10MB on mobile.

// Set to some multiple of the page size to grow the database in larger chunks.
const uint32_t kSQLiteGrowthIncrement = kSQLitePageSizeOverride * 2;

static_assert(kSQLiteGrowthIncrement >= 0 &&
                  kSQLiteGrowthIncrement % kSQLitePageSizeOverride == 0 &&
                  kSQLiteGrowthIncrement < uint32_t(INT32_MAX),
              "Must be 0 (disabled) or a positive multiple of the page size!");

// The maximum number of threads that can be used for database activity at a
// single time.
const uint32_t kMaxConnectionThreadCount = 20;

static_assert(kMaxConnectionThreadCount, "Must have at least one thread!");

// The maximum number of threads to keep when idle. Threads that become idle in
// excess of this number will be shut down immediately.
const uint32_t kMaxIdleConnectionThreadCount = 2;

static_assert(kMaxConnectionThreadCount >= kMaxIdleConnectionThreadCount,
              "Idle thread limit must be less than total thread limit!");

// The length of time that database connections will be held open after all
// transactions have completed before doing idle maintenance.
const uint32_t kConnectionIdleMaintenanceMS = 2 * 1000;  // 2 seconds

// The length of time that database connections will be held open after all
// transactions and maintenance have completed.
const uint32_t kConnectionIdleCloseMS = 10 * 1000;  // 10 seconds

// The length of time that idle threads will stay alive before being shut down.
const uint32_t kConnectionThreadIdleMS = 30 * 1000;  // 30 seconds

#define SAVEPOINT_CLAUSE "SAVEPOINT sp;"_ns

// For efficiency reasons, kEncryptedStreamBlockSize must be a multiple of large
// 4k disk sectors.
static_assert(kEncryptedStreamBlockSize % 4096 == 0);
// Similarly, the file copy buffer size must be a multiple of the encrypted
// block size.
static_assert(kFileCopyBufferSize % kEncryptedStreamBlockSize == 0);

constexpr auto kJournalDirectoryName = u"journals"_ns;

constexpr auto kFileManagerDirectoryNameSuffix = u".files"_ns;
constexpr auto kSQLiteSuffix = u".sqlite"_ns;
constexpr auto kSQLiteJournalSuffix = u".sqlite-journal"_ns;
constexpr auto kSQLiteSHMSuffix = u".sqlite-shm"_ns;
constexpr auto kSQLiteWALSuffix = u".sqlite-wal"_ns;

const char kPrefFileHandleEnabled[] = "dom.fileHandle.enabled";

constexpr auto kPermissionStringBase = "indexedDB-chrome-"_ns;
constexpr auto kPermissionReadSuffix = "-read"_ns;
constexpr auto kPermissionWriteSuffix = "-write"_ns;

// The following constants define all names of binding parameters in statements,
// where they are bound by name. This should include all parameter names which
// are bound by name. Binding may be done by index when the statement definition
// and binding are done in the same local scope, and no other reasons prevent
// using the indexes (e.g. multiple statement variants with differing number or
// order of parameters). Neither the styles of specifying parameter names
// (literally vs. via these constants) nor the binding styles (by index vs. by
// name) should not be mixed for the same statement. The decision must be made
// for each statement based on the proximity of statement and binding calls.
constexpr auto kStmtParamNameCurrentKey = "current_key"_ns;
constexpr auto kStmtParamNameRangeBound = "range_bound"_ns;
constexpr auto kStmtParamNameObjectStorePosition = "object_store_position"_ns;
constexpr auto kStmtParamNameLowerKey = "lower_key"_ns;
constexpr auto kStmtParamNameUpperKey = "upper_key"_ns;
constexpr auto kStmtParamNameKey = "key"_ns;
constexpr auto kStmtParamNameObjectStoreId = "object_store_id"_ns;
constexpr auto kStmtParamNameIndexId = "index_id"_ns;
// TODO: Maybe the uses of kStmtParamNameId should be replaced by more
// specific constants such as kStmtParamNameObjectStoreId.
constexpr auto kStmtParamNameId = "id"_ns;
constexpr auto kStmtParamNameValue = "value"_ns;
constexpr auto kStmtParamNameObjectDataKey = "object_data_key"_ns;
constexpr auto kStmtParamNameIndexDataValues = "index_data_values"_ns;
constexpr auto kStmtParamNameData = "data"_ns;
constexpr auto kStmtParamNameFileIds = "file_ids"_ns;
constexpr auto kStmtParamNameValueLocale = "value_locale"_ns;
constexpr auto kStmtParamNameLimit = "limit"_ns;

// The following constants define some names of columns in tables, which are
// referred to in remote locations, e.g. in calls to
// GetBindingClauseForKeyRange.
constexpr auto kColumnNameKey = "key"_ns;
constexpr auto kColumnNameValue = "value"_ns;
constexpr auto kColumnNameAliasSortKey = "sort_column"_ns;

// SQL fragments used at multiple locations.
constexpr auto kOpenLimit = " LIMIT "_ns;

// The deletion marker file is created before RemoveDatabaseFilesAndDirectory
// begins deleting a database. It is removed as the last step of deletion. If a
// deletion marker file is found when initializing the origin, the deletion
// routine is run again to ensure that the database and all of its related files
// are removed. The primary goal of this mechanism is to avoid situations where
// a database has been partially deleted, leading to inconsistent state for the
// origin.
constexpr auto kIdbDeletionMarkerFilePrefix = u"idb-deleting-"_ns;

const uint32_t kDeleteTimeoutMs = 1000;

#ifdef DEBUG

const int32_t kDEBUGThreadPriority = nsISupportsPriority::PRIORITY_NORMAL;
const uint32_t kDEBUGThreadSleepMS = 0;

const int32_t kDEBUGTransactionThreadPriority =
    nsISupportsPriority::PRIORITY_NORMAL;
const uint32_t kDEBUGTransactionThreadSleepMS = 0;

#endif

/*******************************************************************************
 * Metadata classes
 ******************************************************************************/

// Can be instantiated either on the QuotaManager IO thread or on a
// versionchange transaction thread. These threads can never race so this is
// totally safe.
struct FullIndexMetadata {
  IndexMetadata mCommonMetadata = {0,     nsString(), KeyPath(0), nsCString(),
                                   false, false,      false};

  FlippedOnce<false> mDeleted;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FullIndexMetadata)

 private:
  ~FullIndexMetadata() = default;
};

typedef nsRefPtrHashtable<nsUint64HashKey, FullIndexMetadata> IndexTable;

// Can be instantiated either on the QuotaManager IO thread or on a
// versionchange transaction thread. These threads can never race so this is
// totally safe.
struct FullObjectStoreMetadata {
  ObjectStoreMetadata mCommonMetadata = {0, nsString(), KeyPath(0), false};
  IndexTable mIndexes;

  // These two members are only ever touched on a transaction thread!
  int64_t mNextAutoIncrementId = 0;
  int64_t mCommittedAutoIncrementId = 0;

  FlippedOnce<false> mDeleted;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FullObjectStoreMetadata);

  bool HasLiveIndexes() const;

 private:
  ~FullObjectStoreMetadata() = default;
};

typedef nsRefPtrHashtable<nsUint64HashKey, FullObjectStoreMetadata>
    ObjectStoreTable;

static_assert(
    std::is_same_v<
        IndexOrObjectStoreId,
        std::remove_cv_t<std::remove_reference_t<decltype(
            std::declval<const ObjectStoreGetParams&>().objectStoreId())>>>);
static_assert(std::is_same_v<
              IndexOrObjectStoreId,
              std::remove_cv_t<std::remove_reference_t<decltype(
                  std::declval<const IndexGetParams&>().objectStoreId())>>>);

struct FullDatabaseMetadata final : AtomicSafeRefCounted<FullDatabaseMetadata> {
  DatabaseMetadata mCommonMetadata;
  nsCString mDatabaseId;
  nsString mFilePath;
  ObjectStoreTable mObjectStores;

  IndexOrObjectStoreId mNextObjectStoreId = 0;
  IndexOrObjectStoreId mNextIndexId = 0;

 public:
  explicit FullDatabaseMetadata(const DatabaseMetadata& aCommonMetadata)
      : mCommonMetadata(aCommonMetadata) {
    AssertIsOnBackgroundThread();
  }

  [[nodiscard]] SafeRefPtr<FullDatabaseMetadata> Duplicate() const;

  MOZ_DECLARE_REFCOUNTED_TYPENAME(FullDatabaseMetadata)
};

template <class Enumerable>
auto MatchMetadataNameOrId(const Enumerable& aEnumerable,
                           IndexOrObjectStoreId aId,
                           Maybe<const nsAString&> aName = Nothing()) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aId);

  const auto it = std::find_if(
      aEnumerable.cbegin(), aEnumerable.cend(),
      [aId, aName](const auto& entry) {
        MOZ_ASSERT(entry.GetKey() != 0);

        const auto& value = entry.GetData();
        MOZ_ASSERT(value);

        return !value->mDeleted &&
               (aId == value->mCommonMetadata.id() ||
                (aName && *aName == value->mCommonMetadata.name()));
      });

  return ToMaybeRef(it != aEnumerable.cend() ? it->GetData().get() : nullptr);
}

/*******************************************************************************
 * SQLite functions
 ******************************************************************************/

// WARNING: the hash function used for the database name must not change.
// That's why this function exists separately from mozilla::HashString(), even
// though it is (at the time of writing) equivalent. See bug 780408 and bug
// 940315 for details.
uint32_t HashName(const nsAString& aName) {
  struct Helper {
    static uint32_t RotateBitsLeft32(uint32_t aValue, uint8_t aBits) {
      MOZ_ASSERT(aBits < 32);
      return (aValue << aBits) | (aValue >> (32 - aBits));
    }
  };

  static const uint32_t kGoldenRatioU32 = 0x9e3779b9u;

  return std::accumulate(aName.BeginReading(), aName.EndReading(), uint32_t(0),
                         [](uint32_t hash, char16_t ch) {
                           return kGoldenRatioU32 *
                                  (Helper::RotateBitsLeft32(hash, 5) ^ ch);
                         });
}

nsresult ClampResultCode(nsresult aResultCode) {
  if (NS_SUCCEEDED(aResultCode) ||
      NS_ERROR_GET_MODULE(aResultCode) == NS_ERROR_MODULE_DOM_INDEXEDDB) {
    return aResultCode;
  }

  switch (aResultCode) {
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
      return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    case NS_ERROR_STORAGE_CONSTRAINT:
      return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
    default:
#ifdef DEBUG
      nsPrintfCString message("Converting non-IndexedDB error code (0x%" PRIX32
                              ") to "
                              "NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR",
                              static_cast<uint32_t>(aResultCode));
      NS_WARNING(message.get());
#else
        ;
#endif
  }

  IDB_REPORT_INTERNAL_ERR();
  return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
}

nsAutoString GetDatabaseFilenameBase(const nsAString& aDatabaseName) {
  nsAutoString databaseFilenameBase;

  // WARNING: do not change this hash function. See the comment in HashName()
  // for details.
  databaseFilenameBase.AppendInt(HashName(aDatabaseName));

  nsAutoCString escapedName;
  if (!NS_Escape(NS_ConvertUTF16toUTF8(aDatabaseName), escapedName,
                 url_XPAlphas)) {
    MOZ_CRASH("Can't escape database name!");
  }

  const char* forwardIter = escapedName.BeginReading();
  const char* backwardIter = escapedName.EndReading() - 1;

  nsAutoCString substring;
  while (forwardIter <= backwardIter && substring.Length() < 21) {
    if (substring.Length() % 2) {
      substring.Append(*backwardIter--);
    } else {
      substring.Append(*forwardIter++);
    }
  }

  databaseFilenameBase.AppendASCII(substring.get(), substring.Length());

  return databaseFilenameBase;
}

Result<nsCOMPtr<nsIFileURL>, nsresult> GetDatabaseFileURL(
    nsIFile& aDatabaseFile, const int64_t aDirectoryLockId,
    const Maybe<CipherKey>& aMaybeKey) {
  MOZ_ASSERT(aDirectoryLockId >= -1);

  IDB_TRY_INSPECT(const auto& protocolHandler,
                  ToResultGet<nsCOMPtr<nsIProtocolHandler>>(
                      MOZ_SELECT_OVERLOAD(do_GetService),
                      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "file"));

  IDB_TRY_INSPECT(const auto& fileHandler,
                  ToResultGet<nsCOMPtr<nsIFileProtocolHandler>>(
                      MOZ_SELECT_OVERLOAD(do_QueryInterface), protocolHandler));

  IDB_TRY_INSPECT(const auto& mutator, MOZ_TO_RESULT_INVOKE_TYPED(
                                           nsCOMPtr<nsIURIMutator>, fileHandler,
                                           NewFileURIMutator, &aDatabaseFile));

  // aDirectoryLockId should only be -1 when we are called
  // - from FileManager::InitDirectory when the temporary storage hasn't been
  //    initialized yet. At that time, the in-memory objects (e.g. OriginInfo)
  //    are only being created so it doesn't make sense to tunnel quota
  //    information to TelemetryVFS to get corresponding QuotaObject instances
  //    for SQLite files.
  // - from DeleteDatabaseOp::LoadPreviousVersion, since this might require
  //   temporarily exceeding the quota limit before the database can be deleted.
  const auto directoryLockIdClause =
      aDirectoryLockId >= 0
          ? "&directoryLockId="_ns + IntToCString(aDirectoryLockId)
          : EmptyCString();

  const auto keyClause = [&aMaybeKey] {
    nsAutoCString keyClause;
    if (aMaybeKey) {
      keyClause.AssignLiteral("&key=");
      for (uint8_t byte : IndexedDBCipherStrategy::SerializeKey(*aMaybeKey)) {
        keyClause.AppendPrintf("%02x", byte);
      }
    }
    return keyClause;
  }();

  IDB_TRY_UNWRAP(
      auto result, ([&mutator, &directoryLockIdClause, &keyClause] {
        nsCOMPtr<nsIFileURL> result;
        nsresult rv = NS_MutateURI(mutator)
                          .SetQuery("cache=private"_ns + directoryLockIdClause +
                                    keyClause)
                          .Finalize(result);
        return NS_SUCCEEDED(rv) ? Result<nsCOMPtr<nsIFileURL>, nsresult>{result}
                                : Err(rv);
      }()));

  return result;
}

nsresult SetDefaultPragmas(mozIStorageConnection& aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());

  static constexpr auto kBuiltInPragmas =
      // We use foreign keys in DEBUG builds only because there is a performance
      // cost to using them.
      "PRAGMA foreign_keys = "
#ifdef DEBUG
      "ON"
#else
      "OFF"
#endif
      ";"

      // The "INSERT OR REPLACE" statement doesn't fire the update trigger,
      // instead it fires only the insert trigger. This confuses the update
      // refcount function. This behavior changes with enabled recursive
      // triggers, so the statement fires the delete trigger first and then the
      // insert trigger.
      "PRAGMA recursive_triggers = ON;"

      // We aggressively truncate the database file when idle so don't bother
      // overwriting the WAL with 0 during active periods.
      "PRAGMA secure_delete = OFF;"_ns;

  IDB_TRY(aConnection.ExecuteSimpleSQL(kBuiltInPragmas));

  IDB_TRY(aConnection.ExecuteSimpleSQL(nsAutoCString{
      "PRAGMA synchronous = "_ns +
      (IndexedDatabaseManager::FullSynchronous() ? "FULL"_ns : "NORMAL"_ns) +
      ";"_ns}));

#ifndef IDB_MOBILE
  if (kSQLiteGrowthIncrement) {
    // This is just an optimization so ignore the failure if the disk is
    // currently too full.
    IDB_TRY(
        ToResult(aConnection.SetGrowthIncrement(kSQLiteGrowthIncrement, ""_ns))
            .orElse(ErrToDefaultOkOrErr<NS_ERROR_FILE_TOO_BIG, Ok>));
  }
#endif  // IDB_MOBILE

  return NS_OK;
}

nsresult SetJournalMode(mozIStorageConnection& aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());

  // Try enabling WAL mode. This can fail in various circumstances so we have to
  // check the results here.
  constexpr auto journalModeQueryStart = "PRAGMA journal_mode = "_ns;
  constexpr auto journalModeWAL = "wal"_ns;

  IDB_TRY_INSPECT(const auto& stmt,
                  CreateAndExecuteSingleStepStatement(
                      aConnection, journalModeQueryStart + journalModeWAL));

  IDB_TRY_INSPECT(
      const auto& journalMode,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCString, *stmt, GetUTF8String, 0));

  if (journalMode.Equals(journalModeWAL)) {
    // WAL mode successfully enabled. Maybe set limits on its size here.
    if (kMaxWALPages >= 0) {
      IDB_TRY(aConnection.ExecuteSimpleSQL("PRAGMA wal_autocheckpoint = "_ns +
                                           IntToCString(kMaxWALPages)));
    }
  } else {
    NS_WARNING("Failed to set WAL mode, falling back to normal journal mode.");
#ifdef IDB_MOBILE
    IDB_TRY(
        aConnection.ExecuteSimpleSQL(journalModeQueryStart + "truncate"_ns));
#endif
  }

  return NS_OK;
}

Result<MovingNotNull<nsCOMPtr<mozIStorageConnection>>, nsresult> OpenDatabase(
    mozIStorageService& aStorageService, nsIFileURL& aFileURL,
    const uint32_t aTelemetryId = 0) {
  const nsAutoCString telemetryFilename =
      aTelemetryId ? "indexedDB-"_ns + IntToCString(aTelemetryId) +
                         NS_ConvertUTF16toUTF8(kSQLiteSuffix)
                   : nsAutoCString();

  IDB_TRY_UNWRAP(auto connection,
                 MOZ_TO_RESULT_INVOKE_TYPED(
                     nsCOMPtr<mozIStorageConnection>, aStorageService,
                     OpenDatabaseWithFileURL, &aFileURL, telemetryFilename));

  return WrapMovingNotNull(std::move(connection));
}

Result<MovingNotNull<nsCOMPtr<mozIStorageConnection>>, nsresult>
OpenDatabaseAndHandleBusy(mozIStorageService& aStorageService,
                          nsIFileURL& aFileURL,
                          const uint32_t aTelemetryId = 0) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  using ConnectionType = Maybe<MovingNotNull<nsCOMPtr<mozIStorageConnection>>>;

  IDB_TRY_UNWRAP(
      auto connection,
      OpenDatabase(aStorageService, aFileURL, aTelemetryId)
          .map([](auto connection) -> ConnectionType {
            return Some(std::move(connection));
          })
          .orElse(ErrToDefaultOkOrErr<NS_ERROR_STORAGE_BUSY, ConnectionType>));

  if (connection.isNothing()) {
#ifdef DEBUG
    {
      nsCString path;
      MOZ_ALWAYS_SUCCEEDS(aFileURL.GetFileName(path));

      nsPrintfCString message(
          "Received NS_ERROR_STORAGE_BUSY when attempting to open database "
          "'%s', retrying for up to 10 seconds",
          path.get());
      NS_WARNING(message.get());
    }
#endif

    // Another thread must be checkpointing the WAL. Wait up to 10 seconds for
    // that to complete.
    const TimeStamp start = TimeStamp::NowLoRes();

    do {
      PR_Sleep(PR_MillisecondsToInterval(100));

      IDB_TRY_UNWRAP(
          connection,
          OpenDatabase(aStorageService, aFileURL, aTelemetryId)
              .map([](auto connection) -> ConnectionType {
                return Some(std::move(connection));
              })
              .orElse([&start](
                          nsresult aValue) -> Result<ConnectionType, nsresult> {
                if (aValue != NS_ERROR_STORAGE_BUSY ||
                    TimeStamp::NowLoRes() - start >
                        TimeDuration::FromSeconds(10)) {
                  return Err(aValue);
                }

                return ConnectionType();
              }));
    } while (connection.isNothing());
  }

  return connection.extract();
}

// Returns true if a given nsIFile exists and is a directory. Returns false if
// it doesn't exist. Returns an error if it exists, but is not a directory, or
// any other error occurs.
Result<bool, nsresult> ExistsAsDirectory(nsIFile& aDirectory) {
  IDB_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(aDirectory, Exists));

  if (exists) {
    IDB_TRY_INSPECT(const bool& isDirectory,
                    MOZ_TO_RESULT_INVOKE(aDirectory, IsDirectory));

    IDB_TRY(OkIf(isDirectory), Err(NS_ERROR_FAILURE));
  }

  return exists;
}

constexpr nsresult mapNoDeviceSpaceError(nsresult aRv) {
  if (aRv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
    // mozstorage translates SQLITE_FULL to
    // NS_ERROR_FILE_NO_DEVICE_SPACE, which we know better as
    // NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR.
    return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
  }
  return aRv;
}

Result<MovingNotNull<nsCOMPtr<mozIStorageConnection>>, nsresult>
CreateStorageConnection(nsIFile& aDBFile, nsIFile& aFMDirectory,
                        const nsAString& aName, const nsACString& aOrigin,
                        const int64_t aDirectoryLockId,
                        const uint32_t aTelemetryId,
                        const Maybe<CipherKey>& aMaybeKey) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectoryLockId >= -1);

  AUTO_PROFILER_LABEL("CreateStorageConnection", DOM);

  IDB_TRY_INSPECT(const auto& dbFileUrl,
                  GetDatabaseFileURL(aDBFile, aDirectoryLockId, aMaybeKey));

  IDB_TRY_INSPECT(
      const auto& storageService,
      ToResultGet<nsCOMPtr<mozIStorageService>>(
          MOZ_SELECT_OVERLOAD(do_GetService), MOZ_STORAGE_SERVICE_CONTRACTID));

  IDB_TRY_UNWRAP(
      auto connection,
      OpenDatabaseAndHandleBusy(*storageService, *dbFileUrl, aTelemetryId)
          .map([](auto connection) -> nsCOMPtr<mozIStorageConnection> {
            return std::move(connection).unwrapBasePtr();
          })
          .orElse([&aName](nsresult aValue)
                      -> Result<nsCOMPtr<mozIStorageConnection>, nsresult> {
            // If we're just opening the database during origin initialization,
            // then we don't want to erase any files. The failure here will fail
            // origin initialization too.
            if (!IsDatabaseCorruptionError(aValue) || aName.IsVoid()) {
              return Err(aValue);
            }

            return nsCOMPtr<mozIStorageConnection>();
          }));

  if (!connection) {
    // XXX Shouldn't we also update quota usage?

    // Nuke the database file.
    IDB_TRY(aDBFile.Remove(false));
    IDB_TRY_INSPECT(const bool& existsAsDirectory,
                    ExistsAsDirectory(aFMDirectory));

    if (existsAsDirectory) {
      IDB_TRY(aFMDirectory.Remove(true));
    }

    IDB_TRY_UNWRAP(connection, OpenDatabaseAndHandleBusy(
                                   *storageService, *dbFileUrl, aTelemetryId));
  }

  IDB_TRY(SetDefaultPragmas(*connection));
  IDB_TRY(connection->EnableModule("filesystem"_ns));

  // Check to make sure that the database schema is correct.
  IDB_TRY_INSPECT(const int32_t& schemaVersion,
                  MOZ_TO_RESULT_INVOKE(connection, GetSchemaVersion));

  // Unknown schema will fail origin initialization too.
  IDB_TRY(
      OkIf(schemaVersion || !aName.IsVoid()),
      Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR), [](const auto&) {
        IDB_WARNING("Unable to open IndexedDB database, schema is not set!");
      });

  IDB_TRY(
      OkIf(schemaVersion <= kSQLiteSchemaVersion),
      Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR), [](const auto&) {
        IDB_WARNING("Unable to open IndexedDB database, schema is too high!");
      });

  bool journalModeSet = false;

  if (schemaVersion != kSQLiteSchemaVersion) {
    const bool newDatabase = !schemaVersion;

    if (newDatabase) {
      // Set the page size first.
      const auto sqlitePageSizeOverride =
          aMaybeKey ? 8192 : kSQLitePageSizeOverride;
      if (sqlitePageSizeOverride) {
        IDB_TRY(connection->ExecuteSimpleSQL(nsPrintfCString(
            "PRAGMA page_size = %" PRIu32 ";", sqlitePageSizeOverride)));
      }

      // We have to set the auto_vacuum mode before opening a transaction.
      IDB_TRY((MOZ_TO_RESULT_INVOKE(
                   connection, ExecuteSimpleSQL,
#ifdef IDB_MOBILE
                   // Turn on full auto_vacuum mode to reclaim disk space on
                   // mobile devices (at the cost of some COMMIT speed).
                   "PRAGMA auto_vacuum = FULL;"_ns
#else
                   // Turn on incremental auto_vacuum mode on desktop builds.
                   "PRAGMA auto_vacuum = INCREMENTAL;"_ns
#endif
                   )
                   .mapErr(mapNoDeviceSpaceError)));

      IDB_TRY(SetJournalMode(*connection));

      journalModeSet = true;
    } else {
#ifdef DEBUG
      // Disable foreign key support while upgrading. This has to be done before
      // starting a transaction.
      MOZ_ALWAYS_SUCCEEDS(
          connection->ExecuteSimpleSQL("PRAGMA foreign_keys = OFF;"_ns));
#endif
    }

    bool vacuumNeeded = false;

    mozStorageTransaction transaction(
        connection.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    IDB_TRY(transaction.Start());

    if (newDatabase) {
      IDB_TRY(CreateTables(*connection));

#ifdef DEBUG
      {
        IDB_TRY_INSPECT(const int32_t& schemaVersion,
                        MOZ_TO_RESULT_INVOKE(connection, GetSchemaVersion),
                        QM_ASSERT_UNREACHABLE);
        MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);
      }
#endif

      // The parameter names are not used, parameters are bound by index only
      // locally in the same function.
      IDB_TRY_INSPECT(
          const auto& stmt,
          MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageStatement>, connection,
                                     CreateStatement,
                                     "INSERT INTO database (name, origin) "
                                     "VALUES (:name, :origin)"_ns));

      IDB_TRY(stmt->BindStringByIndex(0, aName));
      IDB_TRY(stmt->BindUTF8StringByIndex(1, aOrigin));
      IDB_TRY(stmt->Execute());
    } else {
      IDB_TRY_UNWRAP(vacuumNeeded,
                     MaybeUpgradeSchema(*connection, schemaVersion,
                                        aFMDirectory, aOrigin));
    }

    IDB_TRY(MOZ_TO_RESULT_INVOKE(transaction, Commit)
                .mapErr(mapNoDeviceSpaceError));

#ifdef DEBUG
    if (!newDatabase) {
      // Re-enable foreign key support after doing a foreign key check.
      IDB_TRY_INSPECT(const bool& foreignKeyError,
                      CreateAndExecuteSingleStepStatement<
                          SingleStepResult::ReturnNullIfNoResult>(
                          *connection, "PRAGMA foreign_key_check;"_ns),
                      QM_ASSERT_UNREACHABLE);

      MOZ_ASSERT(!foreignKeyError, "Database has inconsisistent foreign keys!");

      MOZ_ALWAYS_SUCCEEDS(
          connection->ExecuteSimpleSQL("PRAGMA foreign_keys = OFF;"_ns));
    }
#endif

    if (kSQLitePageSizeOverride && !newDatabase) {
      IDB_TRY_INSPECT(const auto& stmt,
                      CreateAndExecuteSingleStepStatement(
                          *connection, "PRAGMA page_size;"_ns));

      IDB_TRY_INSPECT(const int32_t& pageSize,
                      MOZ_TO_RESULT_INVOKE(*stmt, GetInt32, 0));
      MOZ_ASSERT(pageSize >= 512 && pageSize <= 65536);

      if (kSQLitePageSizeOverride != uint32_t(pageSize)) {
        // We must not be in WAL journal mode to change the page size.
        IDB_TRY(
            connection->ExecuteSimpleSQL("PRAGMA journal_mode = DELETE;"_ns));

        IDB_TRY_INSPECT(const auto& stmt,
                        CreateAndExecuteSingleStepStatement(
                            *connection, "PRAGMA journal_mode;"_ns));

        IDB_TRY_INSPECT(
            const auto& journalMode,
            MOZ_TO_RESULT_INVOKE_TYPED(nsCString, *stmt, GetUTF8String, 0));

        if (journalMode.EqualsLiteral("delete")) {
          // Successfully set to rollback journal mode so changing the page size
          // is possible with a VACUUM.
          IDB_TRY(connection->ExecuteSimpleSQL(nsPrintfCString(
              "PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride)));

          // We will need to VACUUM in order to change the page size.
          vacuumNeeded = true;
        } else {
          NS_WARNING(
              "Failed to set journal_mode for database, unable to "
              "change the page size!");
        }
      }
    }

    if (vacuumNeeded) {
      IDB_TRY(connection->ExecuteSimpleSQL("VACUUM;"_ns));
    }

    if (newDatabase || vacuumNeeded) {
      if (journalModeSet) {
        // Make sure we checkpoint to get an accurate file size.
        IDB_TRY(
            connection->ExecuteSimpleSQL("PRAGMA wal_checkpoint(FULL);"_ns));
      }

      IDB_TRY_INSPECT(const int64_t& fileSize,
                      MOZ_TO_RESULT_INVOKE(aDBFile, GetFileSize));
      MOZ_ASSERT(fileSize > 0);

      PRTime vacuumTime = PR_Now();
      MOZ_ASSERT(vacuumTime);

      // The parameter names are not used, parameters are bound by index only
      // locally in the same function.
      IDB_TRY_INSPECT(
          const auto& vacuumTimeStmt,
          MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageStatement>, connection,
                                     CreateStatement,
                                     "UPDATE database "
                                     "SET last_vacuum_time = :time"
                                     ", last_vacuum_size = :size;"_ns));

      IDB_TRY(vacuumTimeStmt->BindInt64ByIndex(0, vacuumTime));
      IDB_TRY(vacuumTimeStmt->BindInt64ByIndex(1, fileSize));
      IDB_TRY(vacuumTimeStmt->Execute());
    }
  }

  if (!journalModeSet) {
    IDB_TRY(SetJournalMode(*connection));
  }

  return WrapMovingNotNullUnchecked(std::move(connection));
}

nsCOMPtr<nsIFile> GetFileForPath(const nsAString& aPath) {
  MOZ_ASSERT(!aPath.IsEmpty());

  IDB_TRY_RETURN(QM_NewLocalFile(aPath), nullptr);
}

Result<MovingNotNull<nsCOMPtr<mozIStorageConnection>>, nsresult>
GetStorageConnection(nsIFile& aDatabaseFile, const int64_t aDirectoryLockId,
                     const uint32_t aTelemetryId,
                     const Maybe<CipherKey>& aMaybeKey) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aDirectoryLockId >= 0);

  AUTO_PROFILER_LABEL("GetStorageConnection", DOM);

  IDB_TRY_INSPECT(const bool& exists,
                  MOZ_TO_RESULT_INVOKE(aDatabaseFile, Exists));

  IDB_TRY(OkIf(exists), Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
          IDB_REPORT_INTERNAL_ERR_LAMBDA);

  IDB_TRY_INSPECT(
      const auto& dbFileUrl,
      GetDatabaseFileURL(aDatabaseFile, aDirectoryLockId, aMaybeKey));

  IDB_TRY_INSPECT(
      const auto& storageService,
      ToResultGet<nsCOMPtr<mozIStorageService>>(
          MOZ_SELECT_OVERLOAD(do_GetService), MOZ_STORAGE_SERVICE_CONTRACTID));

  IDB_TRY_UNWRAP(
      nsCOMPtr<mozIStorageConnection> connection,
      OpenDatabaseAndHandleBusy(*storageService, *dbFileUrl, aTelemetryId));

  IDB_TRY(SetDefaultPragmas(*connection));

  IDB_TRY(SetJournalMode(*connection));

  return WrapMovingNotNullUnchecked(std::move(connection));
}

Result<MovingNotNull<nsCOMPtr<mozIStorageConnection>>, nsresult>
GetStorageConnection(const nsAString& aDatabaseFilePath,
                     const int64_t aDirectoryLockId,
                     const uint32_t aTelemetryId,
                     const Maybe<CipherKey>& aMaybeKey) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!aDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(StringEndsWith(aDatabaseFilePath, kSQLiteSuffix));
  MOZ_ASSERT(aDirectoryLockId >= 0);

  nsCOMPtr<nsIFile> dbFile = GetFileForPath(aDatabaseFilePath);

  IDB_TRY(OkIf(dbFile), Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
          IDB_REPORT_INTERNAL_ERR_LAMBDA);

  return GetStorageConnection(*dbFile, aDirectoryLockId, aTelemetryId,
                              aMaybeKey);
}

/*******************************************************************************
 * ConnectionPool declarations
 ******************************************************************************/

class DatabaseConnection final : public CachingDatabaseConnection {
  friend class ConnectionPool;

  enum class CheckpointMode { Full, Restart, Truncate };

 public:
  class AutoSavepoint;
  class UpdateRefcountFunction;

 private:
  InitializedOnce<const NotNull<SafeRefPtr<FileManager>>> mFileManager;
  RefPtr<UpdateRefcountFunction> mUpdateRefcountFunction;
  RefPtr<QuotaObject> mQuotaObject;
  RefPtr<QuotaObject> mJournalQuotaObject;
  bool mInReadTransaction;
  bool mInWriteTransaction;

#ifdef DEBUG
  uint32_t mDEBUGSavepointCount;
#endif

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DatabaseConnection)

  UpdateRefcountFunction* GetUpdateRefcountFunction() const {
    AssertIsOnConnectionThread();

    return mUpdateRefcountFunction;
  }

  nsresult BeginWriteTransaction();

  nsresult CommitWriteTransaction();

  void RollbackWriteTransaction();

  void FinishWriteTransaction();

  nsresult StartSavepoint();

  nsresult ReleaseSavepoint();

  nsresult RollbackSavepoint();

  nsresult Checkpoint() {
    AssertIsOnConnectionThread();

    return CheckpointInternal(CheckpointMode::Full);
  }

  void DoIdleProcessing(bool aNeedsCheckpoint);

  void Close();

  nsresult DisableQuotaChecks();

  void EnableQuotaChecks();

 private:
  DatabaseConnection(
      MovingNotNull<nsCOMPtr<mozIStorageConnection>> aStorageConnection,
      MovingNotNull<SafeRefPtr<FileManager>> aFileManager);

  ~DatabaseConnection();

  nsresult Init();

  nsresult CheckpointInternal(CheckpointMode aMode);

  Result<uint32_t, nsresult> GetFreelistCount(
      CachedStatement& aCachedStatement);

  /**
   * On success, returns whether some pages were freed.
   */
  Result<bool, nsresult> ReclaimFreePagesWhileIdle(
      CachedStatement& aFreelistStatement, CachedStatement& aRollbackStatement,
      uint32_t aFreelistCount, bool aNeedsCheckpoint);

  Result<int64_t, nsresult> GetFileSize(const nsAString& aPath);
};

class MOZ_STACK_CLASS DatabaseConnection::AutoSavepoint final {
  DatabaseConnection* mConnection;
#ifdef DEBUG
  const TransactionBase* mDEBUGTransaction;
#endif

 public:
  AutoSavepoint();
  ~AutoSavepoint();

  nsresult Start(const TransactionBase& aTransaction);

  nsresult Commit();
};

class DatabaseConnection::UpdateRefcountFunction final
    : public mozIStorageFunction {
  class FileInfoEntry;

  enum class UpdateType { Increment, Decrement };

  DatabaseConnection* const mConnection;
  FileManager& mFileManager;
  nsClassHashtable<nsUint64HashKey, FileInfoEntry> mFileInfoEntries;
  nsTHashMap<nsUint64HashKey, NotNull<FileInfoEntry*>> mSavepointEntriesIndex;

  nsTArray<int64_t> mJournalsToCreateBeforeCommit;
  nsTArray<int64_t> mJournalsToRemoveAfterCommit;
  nsTArray<int64_t> mJournalsToRemoveAfterAbort;

  bool mInSavepoint;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  UpdateRefcountFunction(DatabaseConnection* aConnection,
                         FileManager& aFileManager);

  nsresult WillCommit();

  void DidCommit();

  void DidAbort();

  void StartSavepoint();

  void ReleaseSavepoint();

  void RollbackSavepoint();

  void Reset();

 private:
  ~UpdateRefcountFunction() = default;

  nsresult ProcessValue(mozIStorageValueArray* aValues, int32_t aIndex,
                        UpdateType aUpdateType);

  nsresult CreateJournals();

  nsresult RemoveJournals(const nsTArray<int64_t>& aJournals);
};

class DatabaseConnection::UpdateRefcountFunction::FileInfoEntry final {
  SafeRefPtr<FileInfo> mFileInfo;
  int32_t mDelta;
  int32_t mSavepointDelta;

 public:
  explicit FileInfoEntry(SafeRefPtr<FileInfo> aFileInfo)
      : mFileInfo(std::move(aFileInfo)), mDelta(0), mSavepointDelta(0) {
    MOZ_COUNT_CTOR(DatabaseConnection::UpdateRefcountFunction::FileInfoEntry);
  }

  void IncDeltas(bool aUpdateSavepointDelta) {
    ++mDelta;
    if (aUpdateSavepointDelta) {
      ++mSavepointDelta;
    }
  }
  void DecDeltas(bool aUpdateSavepointDelta) {
    --mDelta;
    if (aUpdateSavepointDelta) {
      --mSavepointDelta;
    }
  }
  void DecBySavepointDelta() { mDelta -= mSavepointDelta; }
  SafeRefPtr<FileInfo> ReleaseFileInfo() { return std::move(mFileInfo); }
  void MaybeUpdateDBRefs() {
    if (mDelta) {
      mFileInfo->UpdateDBRefs(mDelta);
    }
  }

  int32_t Delta() const { return mDelta; }
  int32_t SavepointDelta() const { return mSavepointDelta; }

  ~FileInfoEntry() {
    MOZ_COUNT_DTOR(DatabaseConnection::UpdateRefcountFunction::FileInfoEntry);
  }
};

class ConnectionPool final {
 public:
  class FinishCallback;

 private:
  class ConnectionRunnable;
  class CloseConnectionRunnable;
  struct DatabaseInfo;
  struct DatabasesCompleteCallback;
  class FinishCallbackWrapper;
  class IdleConnectionRunnable;

  class ThreadRunnable;
  class TransactionInfo;
  struct TransactionInfoPair;

  struct IdleResource {
    TimeStamp mIdleTime;

    IdleResource(const IdleResource& aOther) = delete;
    IdleResource(IdleResource&& aOther) noexcept
        : IdleResource(aOther.mIdleTime) {}
    IdleResource& operator=(const IdleResource& aOther) = delete;
    IdleResource& operator=(IdleResource&& aOther) = delete;

   protected:
    explicit IdleResource(const TimeStamp& aIdleTime);

    ~IdleResource();
  };

  struct IdleDatabaseInfo final : public IdleResource {
    InitializedOnce<const NotNull<DatabaseInfo*>> mDatabaseInfo;

   public:
    explicit IdleDatabaseInfo(DatabaseInfo& aDatabaseInfo);

    IdleDatabaseInfo(const IdleDatabaseInfo& aOther) = delete;
    IdleDatabaseInfo(IdleDatabaseInfo&& aOther) noexcept
        : IdleResource(std::move(aOther)),
          mDatabaseInfo{std::move(aOther.mDatabaseInfo)} {
      MOZ_ASSERT(mDatabaseInfo);

      MOZ_COUNT_CTOR(ConnectionPool::IdleDatabaseInfo);
    }
    IdleDatabaseInfo& operator=(const IdleDatabaseInfo& aOther) = delete;
    IdleDatabaseInfo& operator=(IdleDatabaseInfo&& aOther) = delete;

    ~IdleDatabaseInfo();

    bool operator==(const IdleDatabaseInfo& aOther) const {
      return *mDatabaseInfo == *aOther.mDatabaseInfo;
    }

    bool operator==(const DatabaseInfo* aDatabaseInfo) const {
      return *mDatabaseInfo == aDatabaseInfo;
    }

    bool operator<(const IdleDatabaseInfo& aOther) const {
      return mIdleTime < aOther.mIdleTime;
    }
  };

  class ThreadInfo {
   public:
    ThreadInfo();

    ThreadInfo(nsCOMPtr<nsIThread> aThread, RefPtr<ThreadRunnable> aRunnable)
        : mThread{std::move(aThread)}, mRunnable{std::move(aRunnable)} {
      AssertIsOnBackgroundThread();
      AssertValid();

      MOZ_COUNT_CTOR(ConnectionPool::ThreadInfo);
    }

    ThreadInfo(const ThreadInfo& aOther) = delete;
    ThreadInfo& operator=(const ThreadInfo& aOther) = delete;

    ThreadInfo(ThreadInfo&& aOther) noexcept;
    ThreadInfo& operator=(ThreadInfo&& aOther) = default;

    bool IsValid() const {
      const bool res = mThread;
      if (res) {
        AssertValid();
      } else {
        AssertEmpty();
      }
      return res;
    }

    void AssertValid() const {
      MOZ_ASSERT(mThread);
      MOZ_ASSERT(mRunnable);
    }

    void AssertEmpty() const {
      MOZ_ASSERT(!mThread);
      MOZ_ASSERT(!mRunnable);
    }

    nsIThread& ThreadRef() {
      AssertValid();
      return *mThread;
    }

    std::tuple<nsCOMPtr<nsIThread>, RefPtr<ThreadRunnable>> Forget() {
      AssertValid();

      return {std::move(mThread), std::move(mRunnable)};
    }

    ~ThreadInfo();

    bool operator==(const ThreadInfo& aOther) const {
      return mThread == aOther.mThread && mRunnable == aOther.mRunnable;
    }

   private:
    nsCOMPtr<nsIThread> mThread;
    RefPtr<ThreadRunnable> mRunnable;
  };

  struct IdleThreadInfo final : public IdleResource {
    ThreadInfo mThreadInfo;

    explicit IdleThreadInfo(ThreadInfo aThreadInfo);

    IdleThreadInfo(const IdleThreadInfo& aOther) = delete;
    IdleThreadInfo(IdleThreadInfo&& aOther) noexcept
        : IdleResource(std::move(aOther)),
          mThreadInfo(std::move(aOther.mThreadInfo)) {
      AssertIsOnBackgroundThread();
      mThreadInfo.AssertValid();

      MOZ_COUNT_CTOR(ConnectionPool::IdleThreadInfo);
    }
    IdleThreadInfo& operator=(const IdleThreadInfo& aOther) = delete;
    IdleThreadInfo& operator=(IdleThreadInfo&& aOther) = delete;

    ~IdleThreadInfo();

    bool operator==(const IdleThreadInfo& aOther) const {
      return mThreadInfo == aOther.mThreadInfo;
    }

    bool operator<(const IdleThreadInfo& aOther) const {
      return mIdleTime < aOther.mIdleTime;
    }
  };

  // This mutex guards mDatabases, see below.
  Mutex mDatabasesMutex;

  nsTArray<IdleThreadInfo> mIdleThreads;
  nsTArray<IdleDatabaseInfo> mIdleDatabases;
  nsTArray<NotNull<DatabaseInfo*>> mDatabasesPerformingIdleMaintenance;
  nsCOMPtr<nsITimer> mIdleTimer;
  TimeStamp mTargetIdleTime;

  // Only modifed on the owning thread, but read on multiple threads. Therefore
  // all modifications and all reads off the owning thread must be protected by
  // mDatabasesMutex.
  nsClassHashtable<nsCStringHashKey, DatabaseInfo> mDatabases;

  nsClassHashtable<nsUint64HashKey, TransactionInfo> mTransactions;
  nsTArray<NotNull<TransactionInfo*>> mQueuedTransactions;

  nsTArray<UniquePtr<DatabasesCompleteCallback>> mCompleteCallbacks;

  uint64_t mNextTransactionId;
  uint32_t mTotalThreadCount;
  FlippedOnce<false> mShutdownRequested;
  FlippedOnce<false> mShutdownComplete;

 public:
  ConnectionPool();

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(ConnectionPool);
  }

  Result<RefPtr<DatabaseConnection>, nsresult> GetOrCreateConnection(
      const Database& aDatabase);

  uint64_t Start(const nsID& aBackgroundChildLoggingId,
                 const nsACString& aDatabaseId, int64_t aLoggingSerialNumber,
                 const nsTArray<nsString>& aObjectStoreNames,
                 bool aIsWriteTransaction,
                 TransactionDatabaseOperationBase* aTransactionOp);

  void Dispatch(uint64_t aTransactionId, nsIRunnable* aRunnable);

  void Finish(uint64_t aTransactionId, FinishCallback* aCallback);

  void CloseDatabaseWhenIdle(const nsACString& aDatabaseId) {
    Unused << CloseDatabaseWhenIdleInternal(aDatabaseId);
  }

  void WaitForDatabasesToComplete(nsTArray<nsCString>&& aDatabaseIds,
                                  nsIRunnable* aCallback);

  void Shutdown();

  NS_INLINE_DECL_REFCOUNTING(ConnectionPool)

 private:
  ~ConnectionPool();

  static void IdleTimerCallback(nsITimer* aTimer, void* aClosure);

  void Cleanup();

  void AdjustIdleTimer();

  void CancelIdleTimer();

  void ShutdownThread(ThreadInfo aThreadInfo);

  void CloseIdleDatabases();

  void ShutdownIdleThreads();

  bool ScheduleTransaction(TransactionInfo& aTransactionInfo,
                           bool aFromQueuedTransactions);

  void NoteFinishedTransaction(uint64_t aTransactionId);

  void ScheduleQueuedTransactions(ThreadInfo aThreadInfo);

  void NoteIdleDatabase(DatabaseInfo& aDatabaseInfo);

  void NoteClosedDatabase(DatabaseInfo& aDatabaseInfo);

  bool MaybeFireCallback(DatabasesCompleteCallback* aCallback);

  void PerformIdleDatabaseMaintenance(DatabaseInfo& aDatabaseInfo);

  void CloseDatabase(DatabaseInfo& aDatabaseInfo);

  bool CloseDatabaseWhenIdleInternal(const nsACString& aDatabaseId);
};

class ConnectionPool::ConnectionRunnable : public Runnable {
 protected:
  DatabaseInfo& mDatabaseInfo;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  explicit ConnectionRunnable(DatabaseInfo& aDatabaseInfo);

  ~ConnectionRunnable() override = default;
};

class ConnectionPool::IdleConnectionRunnable final : public ConnectionRunnable {
  const bool mNeedsCheckpoint;

 public:
  IdleConnectionRunnable(DatabaseInfo& aDatabaseInfo, bool aNeedsCheckpoint)
      : ConnectionRunnable(aDatabaseInfo), mNeedsCheckpoint(aNeedsCheckpoint) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(IdleConnectionRunnable,
                                       ConnectionRunnable)

 private:
  ~IdleConnectionRunnable() override = default;

  NS_DECL_NSIRUNNABLE
};

class ConnectionPool::CloseConnectionRunnable final
    : public ConnectionRunnable {
 public:
  explicit CloseConnectionRunnable(DatabaseInfo& aDatabaseInfo)
      : ConnectionRunnable(aDatabaseInfo) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(CloseConnectionRunnable,
                                       ConnectionRunnable)

 private:
  ~CloseConnectionRunnable() override = default;

  NS_DECL_NSIRUNNABLE
};

struct ConnectionPool::DatabaseInfo final {
  friend class mozilla::DefaultDelete<DatabaseInfo>;

  RefPtr<ConnectionPool> mConnectionPool;
  const nsCString mDatabaseId;
  RefPtr<DatabaseConnection> mConnection;
  nsClassHashtable<nsStringHashKey, TransactionInfoPair> mBlockingTransactions;
  nsTArray<NotNull<TransactionInfo*>> mTransactionsScheduledDuringClose;
  nsTArray<NotNull<TransactionInfo*>> mScheduledWriteTransactions;
  Maybe<TransactionInfo&> mRunningWriteTransaction;
  ThreadInfo mThreadInfo;
  uint32_t mReadTransactionCount;
  uint32_t mWriteTransactionCount;
  bool mNeedsCheckpoint;
  bool mIdle;
  FlippedOnce<false> mCloseOnIdle;
  bool mClosing;

#ifdef DEBUG
  PRThread* mDEBUGConnectionThread;
#endif

  DatabaseInfo(ConnectionPool* aConnectionPool, const nsACString& aDatabaseId);

  void AssertIsOnConnectionThread() const {
    MOZ_ASSERT(mDEBUGConnectionThread);
    MOZ_ASSERT(PR_GetCurrentThread() == mDEBUGConnectionThread);
  }

  uint64_t TotalTransactionCount() const {
    return mReadTransactionCount + mWriteTransactionCount;
  }

 private:
  ~DatabaseInfo();

  DatabaseInfo(const DatabaseInfo&) = delete;
  DatabaseInfo& operator=(const DatabaseInfo&) = delete;
};

struct ConnectionPool::DatabasesCompleteCallback final {
  friend class DefaultDelete<DatabasesCompleteCallback>;

  nsTArray<nsCString> mDatabaseIds;
  nsCOMPtr<nsIRunnable> mCallback;

  DatabasesCompleteCallback(nsTArray<nsCString>&& aDatabaseIds,
                            nsIRunnable* aCallback);

 private:
  ~DatabasesCompleteCallback();
};

class NS_NO_VTABLE ConnectionPool::FinishCallback : public nsIRunnable {
 public:
  // Called on the owning thread before any additional transactions are
  // unblocked.
  virtual void TransactionFinishedBeforeUnblock() = 0;

  // Called on the owning thread after additional transactions may have been
  // unblocked.
  virtual void TransactionFinishedAfterUnblock() = 0;

 protected:
  FinishCallback() = default;

  virtual ~FinishCallback() = default;
};

class ConnectionPool::FinishCallbackWrapper final : public Runnable {
  RefPtr<ConnectionPool> mConnectionPool;
  RefPtr<FinishCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  uint64_t mTransactionId;
  bool mHasRunOnce;

 public:
  FinishCallbackWrapper(ConnectionPool* aConnectionPool,
                        uint64_t aTransactionId, FinishCallback* aCallback);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(FinishCallbackWrapper, Runnable)

 private:
  ~FinishCallbackWrapper() override;

  NS_DECL_NSIRUNNABLE
};

class ConnectionPool::ThreadRunnable final : public Runnable {
  // Only touched on the background thread.
  static uint32_t sNextSerialNumber;

  // Set at construction for logging.
  const uint32_t mSerialNumber;

  // These two values are only modified on the connection thread.
  FlippedOnce<true> mFirstRun;
  FlippedOnce<true> mContinueRunning;

 public:
  ThreadRunnable();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ThreadRunnable, Runnable)

  uint32_t SerialNumber() const { return mSerialNumber; }

  nsCString GetThreadName() const {
    return nsPrintfCString("IndexedDB #%" PRIu32, mSerialNumber);
  }

 private:
  ~ThreadRunnable() override;

  NS_DECL_NSIRUNNABLE
};

class ConnectionPool::TransactionInfo final {
  friend class mozilla::DefaultDelete<TransactionInfo>;

  nsTHashtable<nsPtrHashKey<TransactionInfo>> mBlocking;
  nsTArray<NotNull<TransactionInfo*>> mBlockingOrdered;

 public:
  DatabaseInfo& mDatabaseInfo;
  const nsID mBackgroundChildLoggingId;
  const nsCString mDatabaseId;
  const uint64_t mTransactionId;
  const int64_t mLoggingSerialNumber;
  const nsTArray<nsString> mObjectStoreNames;
  nsTHashtable<nsPtrHashKey<TransactionInfo>> mBlockedOn;
  nsTArray<nsCOMPtr<nsIRunnable>> mQueuedRunnables;
  const bool mIsWriteTransaction;
  bool mRunning;

#ifdef DEBUG
  FlippedOnce<false> mFinished;
#endif

  TransactionInfo(DatabaseInfo& aDatabaseInfo,
                  const nsID& aBackgroundChildLoggingId,
                  const nsACString& aDatabaseId, uint64_t aTransactionId,
                  int64_t aLoggingSerialNumber,
                  const nsTArray<nsString>& aObjectStoreNames,
                  bool aIsWriteTransaction,
                  TransactionDatabaseOperationBase* aTransactionOp);

  void AddBlockingTransaction(TransactionInfo& aTransactionInfo);

  void RemoveBlockingTransactions();

 private:
  ~TransactionInfo();

  void MaybeUnblock(TransactionInfo& aTransactionInfo);
};

struct ConnectionPool::TransactionInfoPair final {
  // Multiple reading transactions can block future writes.
  nsTArray<NotNull<TransactionInfo*>> mLastBlockingWrites;
  // But only a single writing transaction can block future reads.
  Maybe<TransactionInfo&> mLastBlockingReads;

#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING)
  TransactionInfoPair();
  ~TransactionInfoPair();
#endif
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

template <IDBCursorType CursorType>
class CommonOpenOpHelper;
template <IDBCursorType CursorType>
class IndexOpenOpHelper;
template <IDBCursorType CursorType>
class ObjectStoreOpenOpHelper;
template <IDBCursorType CursorType>
class OpenOpHelper;

class DatabaseOperationBase : public Runnable,
                              public mozIStorageProgressHandler {
  template <IDBCursorType CursorType>
  friend class OpenOpHelper;

 protected:
  class AutoSetProgressHandler;

  typedef nsTHashMap<nsUint64HashKey, bool> UniqueIndexTable;

  const nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  const nsID mBackgroundChildLoggingId;
  const uint64_t mLoggingSerialNumber;

 private:
  nsresult mResultCode = NS_OK;
  Atomic<bool> mOperationMayProceed;
  FlippedOnce<false> mActorDestroyed;

 public:
  NS_DECL_ISUPPORTS_INHERITED

  bool IsOnOwningThread() const {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) &&
           current;
  }

  void AssertIsOnOwningThread() const {
    MOZ_ASSERT(IsOnBackgroundThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  void NoteActorDestroyed() {
    AssertIsOnOwningThread();

    mActorDestroyed.Flip();
    mOperationMayProceed = false;
  }

  bool IsActorDestroyed() const {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

  // May be called on any thread, but you should call IsActorDestroyed() if
  // you know you're on the background thread because it is slightly faster.
  bool OperationMayProceed() const { return mOperationMayProceed; }

  const nsID& BackgroundChildLoggingId() const {
    return mBackgroundChildLoggingId;
  }

  uint64_t LoggingSerialNumber() const { return mLoggingSerialNumber; }

  nsresult ResultCode() const { return mResultCode; }

  void SetFailureCode(nsresult aFailureCode) {
    MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
    OverrideFailureCode(aFailureCode);
  }

  void SetFailureCodeIfUnset(nsresult aFailureCode) {
    if (NS_SUCCEEDED(mResultCode)) {
      OverrideFailureCode(aFailureCode);
    }
  }

  bool HasFailed() const { return NS_FAILED(mResultCode); }

 protected:
  DatabaseOperationBase(const nsID& aBackgroundChildLoggingId,
                        uint64_t aLoggingSerialNumber)
      : Runnable("dom::indexedDB::DatabaseOperationBase"),
        mOwningEventTarget(GetCurrentEventTarget()),
        mBackgroundChildLoggingId(aBackgroundChildLoggingId),
        mLoggingSerialNumber(aLoggingSerialNumber),
        mOperationMayProceed(true) {
    AssertIsOnOwningThread();
  }

  ~DatabaseOperationBase() override { MOZ_ASSERT(mActorDestroyed); }

  void OverrideFailureCode(nsresult aFailureCode) {
    MOZ_ASSERT(NS_FAILED(aFailureCode));

    mResultCode = aFailureCode;
  }

  static nsAutoCString MaybeGetBindingClauseForKeyRange(
      const Maybe<SerializedKeyRange>& aOptionalKeyRange,
      const nsACString& aKeyColumnName);

  static nsAutoCString GetBindingClauseForKeyRange(
      const SerializedKeyRange& aKeyRange, const nsACString& aKeyColumnName);

  static uint64_t ReinterpretDoubleAsUInt64(double aDouble);

  static nsresult BindKeyRangeToStatement(const SerializedKeyRange& aKeyRange,
                                          mozIStorageStatement* aStatement);

  static nsresult BindKeyRangeToStatement(const SerializedKeyRange& aKeyRange,
                                          mozIStorageStatement* aStatement,
                                          const nsCString& aLocale);

  static Result<IndexDataValuesAutoArray, nsresult>
  IndexDataValuesFromUpdateInfos(const nsTArray<IndexUpdateInfo>& aUpdateInfos,
                                 const UniqueIndexTable& aUniqueIndexTable);

  static nsresult InsertIndexTableRows(
      DatabaseConnection* aConnection, IndexOrObjectStoreId aObjectStoreId,
      const Key& aObjectStoreKey, const nsTArray<IndexDataValue>& aIndexValues);

  static nsresult DeleteIndexDataTableRows(
      DatabaseConnection* aConnection, const Key& aObjectStoreKey,
      const nsTArray<IndexDataValue>& aIndexValues);

  static nsresult DeleteObjectStoreDataTableRowsWithIndexes(
      DatabaseConnection* aConnection, IndexOrObjectStoreId aObjectStoreId,
      const Maybe<SerializedKeyRange>& aKeyRange);

  static nsresult UpdateIndexValues(
      DatabaseConnection* aConnection, IndexOrObjectStoreId aObjectStoreId,
      const Key& aObjectStoreKey, const nsTArray<IndexDataValue>& aIndexValues);

  static Result<bool, nsresult> ObjectStoreHasIndexes(
      DatabaseConnection& aConnection, IndexOrObjectStoreId aObjectStoreId);

 private:
  template <typename KeyTransformation>
  static nsresult MaybeBindKeyToStatement(
      const Key& aKey, mozIStorageStatement* aStatement,
      const nsCString& aParameterName,
      const KeyTransformation& aKeyTransformation);

  template <typename KeyTransformation>
  static nsresult BindTransformedKeyRangeToStatement(
      const SerializedKeyRange& aKeyRange, mozIStorageStatement* aStatement,
      const KeyTransformation& aKeyTransformation);

  // Not to be overridden by subclasses.
  NS_DECL_MOZISTORAGEPROGRESSHANDLER
};

class MOZ_STACK_CLASS DatabaseOperationBase::AutoSetProgressHandler final {
  Maybe<mozIStorageConnection&> mConnection;
#ifdef DEBUG
  DatabaseOperationBase* mDEBUGDatabaseOp;
#endif

 public:
  AutoSetProgressHandler();

  ~AutoSetProgressHandler();

  nsresult Register(mozIStorageConnection& aConnection,
                    DatabaseOperationBase* aDatabaseOp);

  void Unregister();
};

class TransactionDatabaseOperationBase : public DatabaseOperationBase {
  enum class InternalState {
    Initial,
    DatabaseWork,
    SendingPreprocess,
    WaitingForContinue,
    SendingResults,
    Completed
  };

  InitializedOnce<const NotNull<SafeRefPtr<TransactionBase>>> mTransaction;
  InternalState mInternalState = InternalState::Initial;
  bool mWaitingForContinue = false;
  const bool mTransactionIsAborted;

 protected:
  const int64_t mTransactionLoggingSerialNumber;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
 protected:
  // A check only enables when the diagnostic assert turns on. It assumes the
  // mUpdateRefcountFunction is a nullptr because the previous
  // StartTransactionOp failed on the connection thread and the next write
  // operation (e.g. ObjectstoreAddOrPutRequestOp) doesn't have enough time to
  // catch up the failure information.
  bool mAssumingPreviousOperationFail = false;
#endif

 public:
  void AssertIsOnConnectionThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  uint64_t StartOnConnectionPool(const nsID& aBackgroundChildLoggingId,
                                 const nsACString& aDatabaseId,
                                 int64_t aLoggingSerialNumber,
                                 const nsTArray<nsString>& aObjectStoreNames,
                                 bool aIsWriteTransaction);

  void DispatchToConnectionPool();

  TransactionBase& Transaction() { return **mTransaction; }

  const TransactionBase& Transaction() const { return **mTransaction; }

  bool IsWaitingForContinue() const {
    AssertIsOnOwningThread();

    return mWaitingForContinue;
  }

  void NoteContinueReceived();

  int64_t TransactionLoggingSerialNumber() const {
    return mTransactionLoggingSerialNumber;
  }

  // May be overridden by subclasses if they need to perform work on the
  // background thread before being dispatched. Returning false will kill the
  // child actors and prevent dispatch.
  virtual bool Init(TransactionBase& aTransaction);

  // This callback will be called on the background thread before releasing the
  // final reference to this request object. Subclasses may perform any
  // additional cleanup here but must always call the base class implementation.
  virtual void Cleanup();

 protected:
  explicit TransactionDatabaseOperationBase(
      SafeRefPtr<TransactionBase> aTransaction);

  TransactionDatabaseOperationBase(SafeRefPtr<TransactionBase> aTransaction,
                                   uint64_t aLoggingSerialNumber);

  ~TransactionDatabaseOperationBase() override;

  virtual void RunOnConnectionThread();

  // Must be overridden in subclasses. Called on the target thread to allow the
  // subclass to perform necessary database or file operations. A successful
  // return value will trigger a SendSuccessResult callback on the background
  // thread while a failure value will trigger a SendFailureResult callback.
  virtual nsresult DoDatabaseWork(DatabaseConnection* aConnection) = 0;

  // May be overriden in subclasses. Called on the background thread to decide
  // if the subclass needs to send any preprocess info to the child actor.
  virtual bool HasPreprocessInfo();

  // May be overriden in subclasses. Called on the background thread to allow
  // the subclass to serialize its preprocess info and send it to the child
  // actor. A successful return value will trigger a wait for a
  // NoteContinueReceived callback on the background thread while a failure
  // value will trigger a SendFailureResult callback.
  virtual nsresult SendPreprocessInfo();

  // Must be overridden in subclasses. Called on the background thread to allow
  // the subclass to serialize its results and send them to the child actor. A
  // failed return value will trigger a SendFailureResult callback.
  virtual nsresult SendSuccessResult() = 0;

  // Must be overridden in subclasses. Called on the background thread to allow
  // the subclass to send its failure code. Returning false will cause the
  // transaction to be aborted with aResultCode. Returning true will not cause
  // the transaction to be aborted.
  virtual bool SendFailureResult(nsresult aResultCode) = 0;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  auto MakeAutoSavepointCleanupHandler(DatabaseConnection& aConnection) {
    return [this, &aConnection](const auto) {
      if (!aConnection.GetUpdateRefcountFunction()) {
        mAssumingPreviousOperationFail = true;
      }
    };
  }
#endif

 private:
  void SendToConnectionPool();

  void SendPreprocess();

  void SendResults();

  void SendPreprocessInfoOrResults(bool aSendPreprocessInfo);

  // Not to be overridden by subclasses.
  NS_DECL_NSIRUNNABLE
};

class Factory final : public PBackgroundIDBFactoryParent,
                      public AtomicSafeRefCounted<Factory> {
  RefPtr<DatabaseLoggingInfo> mLoggingInfo;

#ifdef DEBUG
  bool mActorDestroyed;
#endif

  // Reference counted.
  ~Factory() override;

 public:
  [[nodiscard]] static SafeRefPtr<Factory> Create(
      const LoggingInfo& aLoggingInfo);

  DatabaseLoggingInfo* GetLoggingInfo() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo);

    return mLoggingInfo;
  }

  MOZ_DECLARE_REFCOUNTED_TYPENAME(mozilla::dom::indexedDB::Factory)
  MOZ_INLINE_DECL_SAFEREFCOUNTING_INHERITED(Factory, AtomicSafeRefCounted)

  // Only constructed in Create().
  explicit Factory(RefPtr<DatabaseLoggingInfo> aLoggingInfo);

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  PBackgroundIDBFactoryRequestParent* AllocPBackgroundIDBFactoryRequestParent(
      const FactoryRequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBFactoryRequestConstructor(
      PBackgroundIDBFactoryRequestParent* aActor,
      const FactoryRequestParams& aParams) override;

  bool DeallocPBackgroundIDBFactoryRequestParent(
      PBackgroundIDBFactoryRequestParent* aActor) override;

  PBackgroundIDBDatabaseParent* AllocPBackgroundIDBDatabaseParent(
      const DatabaseSpec& aSpec,
      PBackgroundIDBFactoryRequestParent* aRequest) override;

  bool DeallocPBackgroundIDBDatabaseParent(
      PBackgroundIDBDatabaseParent* aActor) override;
};

class WaitForTransactionsHelper final : public Runnable {
  const nsCString mDatabaseId;
  nsCOMPtr<nsIRunnable> mCallback;

  enum class State {
    Initial = 0,
    WaitingForTransactions,
    WaitingForFileHandles,
    Complete
  } mState;

 public:
  WaitForTransactionsHelper(const nsCString& aDatabaseId,
                            nsIRunnable* aCallback)
      : Runnable("dom::indexedDB::WaitForTransactionsHelper"),
        mDatabaseId(aDatabaseId),
        mCallback(aCallback),
        mState(State::Initial) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!aDatabaseId.IsEmpty());
    MOZ_ASSERT(aCallback);
  }

  void WaitForTransactions();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(WaitForTransactionsHelper, Runnable)

 private:
  ~WaitForTransactionsHelper() override {
    MOZ_ASSERT(!mCallback);
    MOZ_ASSERT(mState == State::Complete);
  }

  void MaybeWaitForTransactions();

  void MaybeWaitForFileHandles();

  void CallCallback();

  NS_DECL_NSIRUNNABLE
};

class Database final
    : public PBackgroundIDBDatabaseParent,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>>,
      public AtomicSafeRefCounted<Database> {
  friend class VersionChangeTransaction;

  class StartTransactionOp;
  class UnmapBlobCallback;

 private:
  SafeRefPtr<Factory> mFactory;
  SafeRefPtr<FullDatabaseMetadata> mMetadata;
  SafeRefPtr<FileManager> mFileManager;
  RefPtr<DirectoryLock> mDirectoryLock;
  nsTHashtable<nsPtrHashKey<TransactionBase>> mTransactions;
  nsTHashtable<nsPtrHashKey<MutableFile>> mMutableFiles;
  nsRefPtrHashtable<nsIDHashKey, FileInfo> mMappedBlobs;
  RefPtr<DatabaseConnection> mConnection;
  const PrincipalInfo mPrincipalInfo;
  const Maybe<ContentParentId> mOptionalContentParentId;
  const quota::OriginMetadata mOriginMetadata;
  const nsCString mId;
  const nsString mFilePath;
  const Maybe<const CipherKey> mKey;
  uint32_t mActiveMutableFileCount;
  uint32_t mPendingCreateFileOpCount;
  int64_t mDirectoryLockId;
  const uint32_t mTelemetryId;
  const PersistenceType mPersistenceType;
  const bool mFileHandleDisabled;
  const bool mChromeWriteAccessAllowed;
  const bool mInPrivateBrowsing;
  FlippedOnce<false> mClosed;
  FlippedOnce<false> mInvalidated;
  FlippedOnce<false> mActorWasAlive;
  FlippedOnce<false> mActorDestroyed;
  nsCOMPtr<nsIEventTarget> mBackgroundThread;
#ifdef DEBUG
  bool mAllBlobsUnmapped;
#endif

 public:
  // Created by OpenDatabaseOp.
  Database(SafeRefPtr<Factory> aFactory, const PrincipalInfo& aPrincipalInfo,
           const Maybe<ContentParentId>& aOptionalContentParentId,
           const quota::OriginMetadata& aOriginMetadata, uint32_t aTelemetryId,
           SafeRefPtr<FullDatabaseMetadata> aMetadata,
           SafeRefPtr<FileManager> aFileManager,
           RefPtr<DirectoryLock> aDirectoryLock, bool aFileHandleDisabled,
           bool aChromeWriteAccessAllowed, bool aInPrivateBrowsing,
           const Maybe<const CipherKey>& aMaybeKey);

  void AssertIsOnConnectionThread() const {
#ifdef DEBUG
    if (mConnection) {
      MOZ_ASSERT(mConnection);
      mConnection->AssertIsOnConnectionThread();
    } else {
      MOZ_ASSERT(!NS_IsMainThread());
      MOZ_ASSERT(!IsOnBackgroundThread());
      MOZ_ASSERT(mInvalidated);
    }
#endif
  }

  MOZ_DECLARE_REFCOUNTED_TYPENAME(mozilla::dom::indexedDB::Database)

  void Invalidate();

  bool IsOwnedByProcess(ContentParentId aContentParentId) const {
    return mOptionalContentParentId &&
           mOptionalContentParentId.value() == aContentParentId;
  }

  const quota::OriginMetadata& OriginMetadata() const {
    return mOriginMetadata;
  }

  const nsCString& Id() const { return mId; }

  Maybe<DirectoryLock&> MaybeDirectoryLockRef() const {
    AssertIsOnBackgroundThread();

    return ToMaybeRef(mDirectoryLock.get());
  }

  int64_t DirectoryLockId() const { return mDirectoryLockId; }

  uint32_t TelemetryId() const { return mTelemetryId; }

  PersistenceType Type() const { return mPersistenceType; }

  const nsString& FilePath() const { return mFilePath; }

  FileManager& GetFileManager() const { return *mFileManager; }

  MovingNotNull<SafeRefPtr<FileManager>> GetFileManagerPtr() const {
    return WrapMovingNotNull(mFileManager.clonePtr());
  }

  const FullDatabaseMetadata& Metadata() const {
    MOZ_ASSERT(mMetadata);
    return *mMetadata;
  }

  SafeRefPtr<FullDatabaseMetadata> MetadataPtr() const {
    MOZ_ASSERT(mMetadata);
    return mMetadata.clonePtr();
  }

  PBackgroundParent* GetBackgroundParent() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!IsActorDestroyed());

    return Manager()->Manager();
  }

  DatabaseLoggingInfo* GetLoggingInfo() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mFactory);

    return mFactory->GetLoggingInfo();
  }

  bool RegisterTransaction(TransactionBase& aTransaction);

  void UnregisterTransaction(TransactionBase& aTransaction);

  bool IsFileHandleDisabled() const { return mFileHandleDisabled; }

  bool RegisterMutableFile(MutableFile* aMutableFile);

  void UnregisterMutableFile(MutableFile* aMutableFile);

  void NoteActiveMutableFile();

  void NoteInactiveMutableFile();

  void NotePendingCreateFileOp();

  void NoteCompletedCreateFileOp();

  void SetActorAlive();

  void MapBlob(const IPCBlob& aIPCBlob, SafeRefPtr<FileInfo> aFileInfo);

  bool IsActorAlive() const {
    AssertIsOnBackgroundThread();

    return mActorWasAlive && !mActorDestroyed;
  }

  bool IsActorDestroyed() const {
    AssertIsOnBackgroundThread();

    return mActorWasAlive && mActorDestroyed;
  }

  bool IsClosed() const {
    AssertIsOnBackgroundThread();

    return mClosed;
  }

  bool IsInvalidated() const {
    AssertIsOnBackgroundThread();

    return mInvalidated;
  }

  nsresult EnsureConnection();

  DatabaseConnection* GetConnection() const {
#ifdef DEBUG
    if (mConnection) {
      mConnection->AssertIsOnConnectionThread();
    }
#endif

    return mConnection;
  }

  void Stringify(nsACString& aResult) const;

  bool IsInPrivateBrowsing() const {
    AssertIsOnBackgroundThread();

    return mInPrivateBrowsing;
  }

  const Maybe<const CipherKey>& MaybeKeyRef() const {
    // This can be called on any thread, as it is const.
    MOZ_ASSERT(mKey.isSome() == mInPrivateBrowsing);
    return mKey;
  }

  ~Database() override {
    MOZ_ASSERT(mClosed);
    MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);

    NS_ProxyRelease("ReleaseIDBFactory", mBackgroundThread.get(),
                    mFactory.forget());
  }

 private:
  [[nodiscard]] SafeRefPtr<FileInfo> GetBlob(const IPCBlob& aID);

  void UnmapBlob(const nsID& aID);

  void UnmapAllBlobs();

  bool CloseInternal();

  void MaybeCloseConnection();

  void ConnectionClosedCallback();

  void CleanupMetadata();

  bool VerifyRequestParams(const DatabaseRequestParams& aParams) const;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  PBackgroundIDBDatabaseFileParent* AllocPBackgroundIDBDatabaseFileParent(
      const IPCBlob& aIPCBlob) override;

  bool DeallocPBackgroundIDBDatabaseFileParent(
      PBackgroundIDBDatabaseFileParent* aActor) override;

  PBackgroundIDBDatabaseRequestParent* AllocPBackgroundIDBDatabaseRequestParent(
      const DatabaseRequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBDatabaseRequestConstructor(
      PBackgroundIDBDatabaseRequestParent* aActor,
      const DatabaseRequestParams& aParams) override;

  bool DeallocPBackgroundIDBDatabaseRequestParent(
      PBackgroundIDBDatabaseRequestParent* aActor) override;

  already_AddRefed<PBackgroundIDBTransactionParent>
  AllocPBackgroundIDBTransactionParent(
      const nsTArray<nsString>& aObjectStoreNames, const Mode& aMode) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBTransactionConstructor(
      PBackgroundIDBTransactionParent* aActor,
      nsTArray<nsString>&& aObjectStoreNames, const Mode& aMode) override;

  PBackgroundMutableFileParent* AllocPBackgroundMutableFileParent(
      const nsString& aName, const nsString& aType) override;

  bool DeallocPBackgroundMutableFileParent(
      PBackgroundMutableFileParent* aActor) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvBlocked() override;

  mozilla::ipc::IPCResult RecvClose() override;

  template <typename T>
  static bool InvalidateAll(const nsTHashtable<nsPtrHashKey<T>>& aTable);
};

class Database::StartTransactionOp final
    : public TransactionDatabaseOperationBase {
  friend class Database;

 private:
  explicit StartTransactionOp(SafeRefPtr<TransactionBase> aTransaction)
      : TransactionDatabaseOperationBase(std::move(aTransaction),
                                         /* aLoggingSerialNumber */ 0) {}

  ~StartTransactionOp() override = default;

  void RunOnConnectionThread() override;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  nsresult SendSuccessResult() override;

  bool SendFailureResult(nsresult aResultCode) override;

  void Cleanup() override;
};

class Database::UnmapBlobCallback final
    : public RemoteLazyInputStreamParentCallback {
  SafeRefPtr<Database> mDatabase;

 public:
  explicit UnmapBlobCallback(SafeRefPtr<Database> aDatabase)
      : mDatabase(std::move(aDatabase)) {
    AssertIsOnBackgroundThread();
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Database::UnmapBlobCallback, override)

  void ActorDestroyed(const nsID& aID) override {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatabase);

    const SafeRefPtr<Database> database = std::move(mDatabase);
    database->UnmapBlob(aID);
  }

 private:
  ~UnmapBlobCallback() = default;
};

/**
 * In coordination with IDBDatabase's mFileActors weak-map on the child side, a
 * long-lived mapping from a child process's live Blobs to their corresponding
 * FileInfo in our owning database.  Assists in avoiding redundant IPC traffic
 * and disk storage.  This includes both:
 * - Blobs retrieved from this database and sent to the child that do not need
 *   to be written to disk because they already exist on disk in this database's
 *   files directory.
 * - Blobs retrieved from other databases or from anywhere else that will need
 *   to be written to this database's files directory.  In this case we will
 *   hold a reference to its BlobImpl in mBlobImpl until we have successfully
 *   written the Blob to disk.
 *
 * Relevant Blob context: Blobs sent from the parent process to child processes
 * are automatically linked back to their source BlobImpl when the child process
 * references the Blob via IPC. This is done using the internal IPCBlob
 * inputStream actor ID to FileInfo mapping. However, when getting an actor
 * in the child process for sending an in-child-created Blob to the parent
 * process, there is (currently) no Blob machinery to automatically establish
 * and reuse a long-lived Actor.  As a result, without IDB's weak-map
 * cleverness, a memory-backed Blob repeatedly sent from the child to the parent
 * would appear as a different Blob each time, requiring the Blob data to be
 * sent over IPC each time as well as potentially needing to be written to disk
 * each time.
 *
 * This object remains alive as long as there is an active child actor or an
 * ObjectStoreAddOrPutRequestOp::StoredFileInfo for a queued or active add/put
 * op is holding a reference to us.
 */
class DatabaseFile final : public PBackgroundIDBDatabaseFileParent {
  // mBlobImpl's ownership lifecycle:
  // - Initialized on the background thread at creation time.  Then
  //   responsibility is handed off to the connection thread.
  // - Checked and used by the connection thread to generate a stream to write
  //   the blob to disk by an add/put operation.
  // - Cleared on the connection thread once the file has successfully been
  //   written to disk.
  InitializedOnce<const RefPtr<BlobImpl>> mBlobImpl;
  const SafeRefPtr<FileInfo> mFileInfo;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::DatabaseFile);

  const FileInfo& GetFileInfo() const {
    AssertIsOnBackgroundThread();

    return *mFileInfo;
  }

  SafeRefPtr<FileInfo> GetFileInfoPtr() const {
    AssertIsOnBackgroundThread();

    return mFileInfo.clonePtr();
  }

  /**
   * If mBlobImpl is non-null (implying the contents of this file have not yet
   * been written to disk), then return an input stream. Otherwise, if mBlobImpl
   * is null (because the contents have been written to disk), returns null.
   */
  [[nodiscard]] nsCOMPtr<nsIInputStream> GetInputStream(ErrorResult& rv) const;

  /**
   * To be called upon successful copying of the stream GetInputStream()
   * returned so that we won't try and redundantly write the file to disk in the
   * future.  This is a separate step from GetInputStream() because
   * the write could fail due to quota errors that happen now but that might
   * not happen in a future attempt.
   */
  void WriteSucceededClearBlobImpl() {
    MOZ_ASSERT(!IsOnBackgroundThread());

    MOZ_ASSERT(*mBlobImpl);
    mBlobImpl.destroy();
  }

 public:
  // Called when sending to the child.
  explicit DatabaseFile(SafeRefPtr<FileInfo> aFileInfo)
      : mBlobImpl{nullptr}, mFileInfo(std::move(aFileInfo)) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mFileInfo);
  }

  // Called when receiving from the child.
  DatabaseFile(RefPtr<BlobImpl> aBlobImpl, SafeRefPtr<FileInfo> aFileInfo)
      : mBlobImpl(std::move(aBlobImpl)), mFileInfo(std::move(aFileInfo)) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(*mBlobImpl);
    MOZ_ASSERT(mFileInfo);
  }

 private:
  ~DatabaseFile() override = default;

  void ActorDestroy(ActorDestroyReason aWhy) override {
    AssertIsOnBackgroundThread();
  }
};

nsCOMPtr<nsIInputStream> DatabaseFile::GetInputStream(ErrorResult& rv) const {
  // We should only be called from our DB connection thread, not the background
  // thread.
  MOZ_ASSERT(!IsOnBackgroundThread());

  // If we were constructed without a BlobImpl, or WriteSucceededClearBlobImpl
  // was already called, return nullptr.
  if (!mBlobImpl || !*mBlobImpl) {
    return nullptr;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  (*mBlobImpl)->CreateInputStream(getter_AddRefs(inputStream), rv);
  if (rv.Failed()) {
    return nullptr;
  }

  return inputStream;
}

class TransactionBase : public AtomicSafeRefCounted<TransactionBase> {
  friend class CursorBase;

  template <IDBCursorType CursorType>
  friend class Cursor;

  class CommitOp;

 protected:
  typedef IDBTransaction::Mode Mode;

 private:
  const SafeRefPtr<Database> mDatabase;
  nsTArray<RefPtr<FullObjectStoreMetadata>>
      mModifiedAutoIncrementObjectStoreMetadataArray;
  LazyInitializedOnceNotNull<const uint64_t> mTransactionId;
  const nsCString mDatabaseId;
  const int64_t mLoggingSerialNumber;
  uint64_t mActiveRequestCount;
  Atomic<bool> mInvalidatedOnAnyThread;
  const Mode mMode;
  FlippedOnce<false> mInitialized;
  FlippedOnce<false> mHasBeenActiveOnConnectionThread;
  FlippedOnce<false> mActorDestroyed;
  FlippedOnce<false> mInvalidated;

 protected:
  nsresult mResultCode;
  FlippedOnce<false> mCommitOrAbortReceived;
  FlippedOnce<false> mCommittedOrAborted;
  FlippedOnce<false> mForceAborted;
  LazyInitializedOnce<const Maybe<int64_t>> mLastRequestBeforeCommit;
  Maybe<int64_t> mLastFailedRequest;

 public:
  void AssertIsOnConnectionThread() const {
    MOZ_ASSERT(mDatabase);
    mDatabase->AssertIsOnConnectionThread();
  }

  bool IsActorDestroyed() const {
    AssertIsOnBackgroundThread();

    return mActorDestroyed;
  }

  // Must be called on the background thread.
  bool IsInvalidated() const {
    MOZ_ASSERT(IsOnBackgroundThread(), "Use IsInvalidatedOnAnyThread()");
    MOZ_ASSERT_IF(mInvalidated, NS_FAILED(mResultCode));

    return mInvalidated;
  }

  // May be called on any thread, but is more expensive than IsInvalidated().
  bool IsInvalidatedOnAnyThread() const { return mInvalidatedOnAnyThread; }

  void Init(const uint64_t aTransactionId) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aTransactionId);

    mTransactionId.init(aTransactionId);
    mInitialized.Flip();
  }

  void SetActiveOnConnectionThread() {
    AssertIsOnConnectionThread();
    mHasBeenActiveOnConnectionThread.Flip();
  }

  MOZ_DECLARE_REFCOUNTED_TYPENAME(mozilla::dom::indexedDB::TransactionBase)

  void Abort(nsresult aResultCode, bool aForce);

  uint64_t TransactionId() const { return *mTransactionId; }

  const nsCString& DatabaseId() const { return mDatabaseId; }

  Mode GetMode() const { return mMode; }

  const Database& GetDatabase() const {
    MOZ_ASSERT(mDatabase);

    return *mDatabase;
  }

  Database& GetMutableDatabase() const {
    MOZ_ASSERT(mDatabase);

    return *mDatabase;
  }

  SafeRefPtr<Database> GetDatabasePtr() const {
    MOZ_ASSERT(mDatabase);

    return mDatabase.clonePtr();
  }

  DatabaseLoggingInfo* GetLoggingInfo() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatabase);

    return mDatabase->GetLoggingInfo();
  }

  int64_t LoggingSerialNumber() const { return mLoggingSerialNumber; }

  bool IsAborted() const {
    AssertIsOnBackgroundThread();

    return NS_FAILED(mResultCode);
  }

  [[nodiscard]] RefPtr<FullObjectStoreMetadata> GetMetadataForObjectStoreId(
      IndexOrObjectStoreId aObjectStoreId) const;

  [[nodiscard]] RefPtr<FullIndexMetadata> GetMetadataForIndexId(
      FullObjectStoreMetadata* const aObjectStoreMetadata,
      IndexOrObjectStoreId aIndexId) const;

  PBackgroundParent* GetBackgroundParent() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!IsActorDestroyed());

    return GetDatabase().GetBackgroundParent();
  }

  void NoteModifiedAutoIncrementObjectStore(FullObjectStoreMetadata* aMetadata);

  void ForgetModifiedAutoIncrementObjectStore(
      FullObjectStoreMetadata* aMetadata);

  void NoteActiveRequest();

  void NoteFinishedRequest(int64_t aRequestId, nsresult aResultCode);

  void Invalidate();

  virtual ~TransactionBase();

 protected:
  TransactionBase(SafeRefPtr<Database> aDatabase, Mode aMode);

  void NoteActorDestroyed() {
    AssertIsOnBackgroundThread();

    mActorDestroyed.Flip();
  }

#ifdef DEBUG
  // Only called by VersionChangeTransaction.
  void FakeActorDestroyed() { mActorDestroyed.EnsureFlipped(); }
#endif

  bool RecvCommit(Maybe<int64_t> aLastRequest);

  bool RecvAbort(nsresult aResultCode);

  void MaybeCommitOrAbort() {
    AssertIsOnBackgroundThread();

    // If we've already committed or aborted then there's nothing else to do.
    if (mCommittedOrAborted) {
      return;
    }

    // If there are active requests then we have to wait for those requests to
    // complete (see NoteFinishedRequest).
    if (mActiveRequestCount) {
      return;
    }

    // If we haven't yet received a commit or abort message then there could be
    // additional requests coming so we should wait unless we're being forced to
    // abort.
    if (!mCommitOrAbortReceived && !mForceAborted) {
      return;
    }

    CommitOrAbort();
  }

  PBackgroundIDBRequestParent* AllocRequest(RequestParams&& aParams,
                                            bool aTrustParams);

  bool StartRequest(PBackgroundIDBRequestParent* aActor);

  bool DeallocRequest(PBackgroundIDBRequestParent* aActor);

  already_AddRefed<PBackgroundIDBCursorParent> AllocCursor(
      const OpenCursorParams& aParams, bool aTrustParams);

  bool StartCursor(PBackgroundIDBCursorParent* aActor,
                   const OpenCursorParams& aParams);

  virtual void UpdateMetadata(nsresult aResult) {}

  virtual void SendCompleteNotification(nsresult aResult) = 0;

 private:
  bool VerifyRequestParams(const RequestParams& aParams) const;

  bool VerifyRequestParams(const SerializedKeyRange& aKeyRange) const;

  bool VerifyRequestParams(const ObjectStoreAddPutParams& aParams) const;

  bool VerifyRequestParams(const Maybe<SerializedKeyRange>& aKeyRange) const;

  void CommitOrAbort();
};

class TransactionBase::CommitOp final : public DatabaseOperationBase,
                                        public ConnectionPool::FinishCallback {
  friend class TransactionBase;

  SafeRefPtr<TransactionBase> mTransaction;
  nsresult mResultCode;  ///< TODO: There is also a mResultCode in
                         ///< DatabaseOperationBase. Is there a reason not to
                         ///< use that? At least a more specific name should be
                         ///< given to this one.

 private:
  CommitOp(SafeRefPtr<TransactionBase> aTransaction, nsresult aResultCode);

  ~CommitOp() override = default;

  // Writes new autoIncrement counts to database.
  nsresult WriteAutoIncrementCounts();

  // Updates counts after a database activity has finished.
  void CommitOrRollbackAutoIncrementCounts();

  void AssertForeignKeyConsistency(DatabaseConnection* aConnection)
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  NS_DECL_NSIRUNNABLE

  void TransactionFinishedBeforeUnblock() override;

  void TransactionFinishedAfterUnblock() override;

 public:
  // We need to declare all of nsISupports, because FinishCallback has
  // a pure-virtual nsISupports declaration.
  NS_DECL_ISUPPORTS_INHERITED
};

class NormalTransaction final : public TransactionBase,
                                public PBackgroundIDBTransactionParent {
  nsTArray<RefPtr<FullObjectStoreMetadata>> mObjectStores;

  // Reference counted.
  ~NormalTransaction() override = default;

  bool IsSameProcessActor();

  // Only called by TransactionBase.
  void SendCompleteNotification(nsresult aResult) override;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvCommit(
      const Maybe<int64_t>& aLastRequest) override;

  mozilla::ipc::IPCResult RecvAbort(const nsresult& aResultCode) override;

  PBackgroundIDBRequestParent* AllocPBackgroundIDBRequestParent(
      const RequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBRequestConstructor(
      PBackgroundIDBRequestParent* aActor,
      const RequestParams& aParams) override;

  bool DeallocPBackgroundIDBRequestParent(
      PBackgroundIDBRequestParent* aActor) override;

  already_AddRefed<PBackgroundIDBCursorParent> AllocPBackgroundIDBCursorParent(
      const OpenCursorParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBCursorConstructor(
      PBackgroundIDBCursorParent* aActor,
      const OpenCursorParams& aParams) override;

 public:
  // This constructor is only called by Database.
  NormalTransaction(SafeRefPtr<Database> aDatabase, TransactionBase::Mode aMode,
                    nsTArray<RefPtr<FullObjectStoreMetadata>>&& aObjectStores);

  MOZ_INLINE_DECL_SAFEREFCOUNTING_INHERITED(NormalTransaction, TransactionBase)
};

class VersionChangeTransaction final
    : public TransactionBase,
      public PBackgroundIDBVersionChangeTransactionParent {
  friend class OpenDatabaseOp;

  RefPtr<OpenDatabaseOp> mOpenDatabaseOp;
  SafeRefPtr<FullDatabaseMetadata> mOldMetadata;

  FlippedOnce<false> mActorWasAlive;

 public:
  // Only called by OpenDatabaseOp.
  explicit VersionChangeTransaction(OpenDatabaseOp* aOpenDatabaseOp);

  MOZ_INLINE_DECL_SAFEREFCOUNTING_INHERITED(VersionChangeTransaction,
                                            TransactionBase)

 private:
  // Reference counted.
  ~VersionChangeTransaction() override;

  bool IsSameProcessActor();

  // Only called by OpenDatabaseOp.
  bool CopyDatabaseMetadata();

  void SetActorAlive();

  // Only called by TransactionBase.
  void UpdateMetadata(nsresult aResult) override;

  // Only called by TransactionBase.
  void SendCompleteNotification(nsresult aResult) override;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvCommit(
      const Maybe<int64_t>& aLastRequest) override;

  mozilla::ipc::IPCResult RecvAbort(const nsresult& aResultCode) override;

  mozilla::ipc::IPCResult RecvCreateObjectStore(
      const ObjectStoreMetadata& aMetadata) override;

  mozilla::ipc::IPCResult RecvDeleteObjectStore(
      const IndexOrObjectStoreId& aObjectStoreId) override;

  mozilla::ipc::IPCResult RecvRenameObjectStore(
      const IndexOrObjectStoreId& aObjectStoreId,
      const nsString& aName) override;

  mozilla::ipc::IPCResult RecvCreateIndex(
      const IndexOrObjectStoreId& aObjectStoreId,
      const IndexMetadata& aMetadata) override;

  mozilla::ipc::IPCResult RecvDeleteIndex(
      const IndexOrObjectStoreId& aObjectStoreId,
      const IndexOrObjectStoreId& aIndexId) override;

  mozilla::ipc::IPCResult RecvRenameIndex(
      const IndexOrObjectStoreId& aObjectStoreId,
      const IndexOrObjectStoreId& aIndexId, const nsString& aName) override;

  PBackgroundIDBRequestParent* AllocPBackgroundIDBRequestParent(
      const RequestParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBRequestConstructor(
      PBackgroundIDBRequestParent* aActor,
      const RequestParams& aParams) override;

  bool DeallocPBackgroundIDBRequestParent(
      PBackgroundIDBRequestParent* aActor) override;

  already_AddRefed<PBackgroundIDBCursorParent> AllocPBackgroundIDBCursorParent(
      const OpenCursorParams& aParams) override;

  mozilla::ipc::IPCResult RecvPBackgroundIDBCursorConstructor(
      PBackgroundIDBCursorParent* aActor,
      const OpenCursorParams& aParams) override;
};

class MutableFile : public BackgroundMutableFileParentBase {
  const SafeRefPtr<Database> mDatabase;
  const SafeRefPtr<FileInfo> mFileInfo;

 public:
  [[nodiscard]] static RefPtr<MutableFile> Create(
      nsIFile* aFile, SafeRefPtr<Database> aDatabase,
      SafeRefPtr<FileInfo> aFileInfo);

  const Database& GetDatabase() const {
    AssertIsOnBackgroundThread();

    return *mDatabase;
  }

  SafeRefPtr<FileInfo> GetFileInfoPtr() const {
    AssertIsOnBackgroundThread();

    return mFileInfo.clonePtr();
  }

  void NoteActiveState() override;

  void NoteInactiveState() override;

  PBackgroundParent* GetBackgroundParent() const override;

  already_AddRefed<nsISupports> CreateStream(bool aReadOnly) override;

  already_AddRefed<BlobImpl> CreateBlobImpl() override;

 private:
  MutableFile(nsIFile* aFile, SafeRefPtr<Database> aDatabase,
              SafeRefPtr<FileInfo> aFileInfo);

  ~MutableFile() override;

  PBackgroundFileHandleParent* AllocPBackgroundFileHandleParent(
      const FileMode& aMode) final;

  mozilla::ipc::IPCResult RecvPBackgroundFileHandleConstructor(
      PBackgroundFileHandleParent* aActor, const FileMode& aMode) final;

  mozilla::ipc::IPCResult RecvGetFileId(int64_t* aFileId) override;
};

class FactoryOp
    : public DatabaseOperationBase,
      public OpenDirectoryListener,
      public PBackgroundIDBFactoryRequestParent,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
 public:
  struct MaybeBlockedDatabaseInfo final {
    SafeRefPtr<Database> mDatabase;
    bool mBlocked;

    MaybeBlockedDatabaseInfo(MaybeBlockedDatabaseInfo&&) = default;
    MaybeBlockedDatabaseInfo& operator=(MaybeBlockedDatabaseInfo&&) = default;

    MOZ_IMPLICIT MaybeBlockedDatabaseInfo(SafeRefPtr<Database> aDatabase)
        : mDatabase(std::move(aDatabase)), mBlocked(false) {
      MOZ_ASSERT(mDatabase);

      MOZ_COUNT_CTOR(FactoryOp::MaybeBlockedDatabaseInfo);
    }

    ~MaybeBlockedDatabaseInfo() {
      MOZ_COUNT_DTOR(FactoryOp::MaybeBlockedDatabaseInfo);
    }

    bool operator==(const Database* aOther) const {
      return mDatabase == aOther;
    }

    Database* operator->() const& MOZ_NO_ADDREF_RELEASE_ON_RETURN {
      return mDatabase.unsafeGetRawPtr();
    }
  };

 protected:
  enum class State {
    // Just created on the PBackground thread, dispatched to the main thread.
    // Next step is either SendingResults if permission is denied,
    // PermissionChallenge if the permission is unknown, or FinishOpen
    // if permission is granted.
    Initial,

    // Sending a permission challenge message to the child on the PBackground
    // thread. Next step is PermissionRetry.
    PermissionChallenge,

    // Retrying permission check after a challenge on the main thread. Next step
    // is either SendingResults if permission is denied or FinishOpen
    // if permission is granted.
    PermissionRetry,

    // Opening directory or initializing quota manager on the PBackground
    // thread. Next step is either DirectoryOpenPending if quota manager is
    // already initialized or QuotaManagerPending if quota manager needs to be
    // initialized.
    FinishOpen,

    // Waiting for quota manager initialization to complete on the PBackground
    // thread. Next step is either SendingResults if initialization failed or
    // DirectoryOpenPending if initialization succeeded.
    QuotaManagerPending,

    // Waiting for directory open allowed on the PBackground thread. The next
    // step is either SendingResults if directory lock failed to acquire, or
    // DatabaseOpenPending if directory lock is acquired.
    DirectoryOpenPending,

    // Waiting for database open allowed on the PBackground thread. The next
    // step is DatabaseWorkOpen.
    DatabaseOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. Its next step is
    // either BeginVersionChange if the requested version doesn't match the
    // existing database version or SendingResults if the versions match.
    DatabaseWorkOpen,

    // Starting a version change transaction or deleting a database on the
    // PBackground thread. We need to notify other databases that a version
    // change is about to happen, and maybe tell the request that a version
    // change has been blocked. If databases are notified then the next step is
    // WaitingForOtherDatabasesToClose. Otherwise the next step is
    // WaitingForTransactionsToComplete.
    BeginVersionChange,

    // Waiting for other databases to close on the PBackground thread. This
    // state may persist until all databases are closed. The next state is
    // WaitingForTransactionsToComplete.
    WaitingForOtherDatabasesToClose,

    // Waiting for all transactions that could interfere with this operation to
    // complete on the PBackground thread. Next state is
    // DatabaseWorkVersionChange.
    WaitingForTransactionsToComplete,

    // Waiting to do/doing work on the "work thread". This involves waiting for
    // the VersionChangeOp (OpenDatabaseOp and DeleteDatabaseOp each have a
    // different implementation) to do its work. Eventually the state will
    // transition to SendingResults.
    DatabaseWorkVersionChange,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  // Must be released on the background thread!
  SafeRefPtr<Factory> mFactory;

  // Must be released on the main thread!
  RefPtr<ContentParent> mContentParent;

  // Must be released on the main thread!
  RefPtr<DirectoryLock> mDirectoryLock;

  RefPtr<FactoryOp> mDelayedOp;
  nsTArray<MaybeBlockedDatabaseInfo> mMaybeBlockedDatabases;

  const CommonFactoryRequestParams mCommonParams;
  OriginMetadata mOriginMetadata;
  nsCString mDatabaseId;
  nsString mDatabaseFilePath;
  int64_t mDirectoryLockId;
  State mState;
  bool mWaitingForPermissionRetry;
  bool mEnforcingQuota;
  const bool mDeleting;
  bool mChromeWriteAccessAllowed;
  bool mFileHandleDisabled;
  FlippedOnce<false> mInPrivateBrowsing;

 public:
  const nsCString& Origin() const {
    AssertIsOnOwningThread();

    return mOriginMetadata.mOrigin;
  }

  bool DatabaseFilePathIsKnown() const {
    AssertIsOnOwningThread();

    return !mDatabaseFilePath.IsEmpty();
  }

  const nsString& DatabaseFilePath() const {
    AssertIsOnOwningThread();
    MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());

    return mDatabaseFilePath;
  }

  void NoteDatabaseBlocked(Database* aDatabase);

  void NoteDatabaseClosed(Database* aDatabase);

#ifdef DEBUG
  bool HasBlockedDatabases() const { return !mMaybeBlockedDatabases.IsEmpty(); }
#endif

  void StringifyState(nsACString& aResult) const;

  void Stringify(nsACString& aResult) const;

 protected:
  FactoryOp(SafeRefPtr<Factory> aFactory, RefPtr<ContentParent> aContentParent,
            const CommonFactoryRequestParams& aCommonParams, bool aDeleting);

  ~FactoryOp() override {
    // Normally this would be out-of-line since it is a virtual function but
    // MSVC 2010 fails to link for some reason if it is not inlined here...
    MOZ_ASSERT_IF(OperationMayProceed(),
                  mState == State::Initial || mState == State::Completed);
  }

  nsresult Open();

  nsresult ChallengePermission();

  void PermissionRetry();

  nsresult RetryCheckPermission();

  nsresult DirectoryOpen();

  nsresult SendToIOThread();

  void WaitForTransactions();

  void CleanupMetadata();

  void FinishSendResults();

  nsresult SendVersionChangeMessages(DatabaseActorInfo* aDatabaseActorInfo,
                                     Maybe<Database&> aOpeningDatabase,
                                     uint64_t aOldVersion,
                                     const Maybe<uint64_t>& aNewVersion);

  // Methods that subclasses must implement.
  virtual nsresult DatabaseOpen() = 0;

  virtual nsresult DoDatabaseWork() = 0;

  virtual nsresult BeginVersionChange() = 0;

  virtual bool AreActorsAlive() = 0;

  virtual nsresult DispatchToWorkThread() = 0;

  // Should only be called by Run().
  virtual void SendResults() = 0;

  // We need to declare refcounting unconditionally, because
  // OpenDirectoryListener has pure-virtual refcounting.
  NS_DECL_ISUPPORTS_INHERITED

  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // OpenDirectoryListener overrides.
  void DirectoryLockAcquired(DirectoryLock* aLock) override;

  void DirectoryLockFailed() override;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvPermissionRetry() final;

  virtual void SendBlockedNotification() = 0;

 private:
  mozilla::Result<PermissionRequestBase::PermissionValue, nsresult>
  CheckPermission(ContentParent* aContentParent);

  static bool CheckAtLeastOneAppHasPermission(
      ContentParent* aContentParent, const nsACString& aPermissionString);

  nsresult FinishOpen();

  nsresult QuotaManagerOpen();

  nsresult OpenDirectory();

  // Test whether this FactoryOp needs to wait for the given op.
  bool MustWaitFor(const FactoryOp& aExistingOp);
};

class OpenDatabaseOp final : public FactoryOp {
  friend class Database;
  friend class VersionChangeTransaction;

  class VersionChangeOp;

  Maybe<ContentParentId> mOptionalContentParentId;

  SafeRefPtr<FullDatabaseMetadata> mMetadata;

  uint64_t mRequestedVersion;
  SafeRefPtr<FileManager> mFileManager;

  SafeRefPtr<Database> mDatabase;
  SafeRefPtr<VersionChangeTransaction> mVersionChangeTransaction;

  // This is only set while a VersionChangeOp is live. It holds a strong
  // reference to its OpenDatabaseOp object so this is a weak pointer to avoid
  // cycles.
  VersionChangeOp* mVersionChangeOp;

  uint32_t mTelemetryId;

 public:
  OpenDatabaseOp(SafeRefPtr<Factory> aFactory,
                 RefPtr<ContentParent> aContentParent,
                 const CommonFactoryRequestParams& aParams);

 private:
  ~OpenDatabaseOp() override { MOZ_ASSERT(!mVersionChangeOp); }

  nsresult LoadDatabaseInformation(mozIStorageConnection& aConnection);

  nsresult SendUpgradeNeeded();

  void EnsureDatabaseActor();

  nsresult EnsureDatabaseActorIsAlive();

  mozilla::Result<DatabaseSpec, nsresult> MetadataToSpec() const;

  void AssertMetadataConsistency(const FullDatabaseMetadata& aMetadata)
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  void ConnectionClosedCallback();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult DatabaseOpen() override;

  nsresult DoDatabaseWork() override;

  nsresult BeginVersionChange() override;

  bool AreActorsAlive() override;

  void SendBlockedNotification() override;

  nsresult DispatchToWorkThread() override;

  void SendResults() override;

  static nsresult UpdateLocaleAwareIndex(mozIStorageConnection& aConnection,
                                         const IndexMetadata& aIndexMetadata,
                                         const nsCString& aLocale);
};

class OpenDatabaseOp::VersionChangeOp final
    : public TransactionDatabaseOperationBase {
  friend class OpenDatabaseOp;

  RefPtr<OpenDatabaseOp> mOpenDatabaseOp;
  const uint64_t mRequestedVersion;
  uint64_t mPreviousVersion;

 private:
  explicit VersionChangeOp(OpenDatabaseOp* aOpenDatabaseOp)
      : TransactionDatabaseOperationBase(
            aOpenDatabaseOp->mVersionChangeTransaction.clonePtr(),
            aOpenDatabaseOp->LoggingSerialNumber()),
        mOpenDatabaseOp(aOpenDatabaseOp),
        mRequestedVersion(aOpenDatabaseOp->mRequestedVersion),
        mPreviousVersion(
            aOpenDatabaseOp->mMetadata->mCommonMetadata.version()) {
    MOZ_ASSERT(aOpenDatabaseOp);
    MOZ_ASSERT(mRequestedVersion);
  }

  ~VersionChangeOp() override { MOZ_ASSERT(!mOpenDatabaseOp); }

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  nsresult SendSuccessResult() override;

  bool SendFailureResult(nsresult aResultCode) override;

  void Cleanup() override;
};

class DeleteDatabaseOp final : public FactoryOp {
  class VersionChangeOp;

  nsString mDatabaseDirectoryPath;
  nsString mDatabaseFilenameBase;
  uint64_t mPreviousVersion;

 public:
  DeleteDatabaseOp(SafeRefPtr<Factory> aFactory,
                   RefPtr<ContentParent> aContentParent,
                   const CommonFactoryRequestParams& aParams)
      : FactoryOp(std::move(aFactory), std::move(aContentParent), aParams,
                  /* aDeleting */ true),
        mPreviousVersion(0) {}

 private:
  ~DeleteDatabaseOp() override = default;

  void LoadPreviousVersion(nsIFile& aDatabaseFile);

  nsresult DatabaseOpen() override;

  nsresult DoDatabaseWork() override;

  nsresult BeginVersionChange() override;

  bool AreActorsAlive() override;

  void SendBlockedNotification() override;

  nsresult DispatchToWorkThread() override;

  void SendResults() override;
};

class DeleteDatabaseOp::VersionChangeOp final : public DatabaseOperationBase {
  friend class DeleteDatabaseOp;

  RefPtr<DeleteDatabaseOp> mDeleteDatabaseOp;

 private:
  explicit VersionChangeOp(DeleteDatabaseOp* aDeleteDatabaseOp)
      : DatabaseOperationBase(aDeleteDatabaseOp->BackgroundChildLoggingId(),
                              aDeleteDatabaseOp->LoggingSerialNumber()),
        mDeleteDatabaseOp(aDeleteDatabaseOp) {
    MOZ_ASSERT(aDeleteDatabaseOp);
    MOZ_ASSERT(!aDeleteDatabaseOp->mDatabaseDirectoryPath.IsEmpty());
  }

  ~VersionChangeOp() override = default;

  nsresult RunOnIOThread();

  void RunOnOwningThread();

  NS_DECL_NSIRUNNABLE
};

class DatabaseOp : public DatabaseOperationBase,
                   public PBackgroundIDBDatabaseRequestParent {
 protected:
  SafeRefPtr<Database> mDatabase;

  enum class State {
    // Just created on the PBackground thread, dispatched to the main thread.
    // Next step is DatabaseWork.
    Initial,

    // Waiting to do/doing work on the QuotaManager IO thread. Next step is
    // SendingResults.
    DatabaseWork,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  State mState;

 public:
  void RunImmediately() {
    MOZ_ASSERT(mState == State::Initial);

    Unused << this->Run();
  }

 protected:
  DatabaseOp(SafeRefPtr<Database> aDatabase);

  ~DatabaseOp() override {
    MOZ_ASSERT_IF(OperationMayProceed(),
                  mState == State::Initial || mState == State::Completed);
  }

  nsresult SendToIOThread();

  // Methods that subclasses must implement.
  virtual nsresult DoDatabaseWork() = 0;

  virtual void SendResults() = 0;

  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

class CreateFileOp final : public DatabaseOp {
  const CreateFileParams mParams;

  LazyInitializedOnce<const SafeRefPtr<FileInfo>> mFileInfo;

 public:
  CreateFileOp(SafeRefPtr<Database> aDatabase,
               const DatabaseRequestParams& aParams);

 private:
  ~CreateFileOp() override = default;

  mozilla::Result<RefPtr<MutableFile>, nsresult> CreateMutableFile();

  nsresult DoDatabaseWork() override;

  void SendResults() override;
};

class VersionChangeTransactionOp : public TransactionDatabaseOperationBase {
 public:
  void Cleanup() override;

 protected:
  explicit VersionChangeTransactionOp(
      SafeRefPtr<VersionChangeTransaction> aTransaction)
      : TransactionDatabaseOperationBase(std::move(aTransaction)) {}

  ~VersionChangeTransactionOp() override = default;

 private:
  nsresult SendSuccessResult() override;

  bool SendFailureResult(nsresult aResultCode) override;
};

class CreateObjectStoreOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const ObjectStoreMetadata mMetadata;

 private:
  // Only created by VersionChangeTransaction.
  CreateObjectStoreOp(SafeRefPtr<VersionChangeTransaction> aTransaction,
                      const ObjectStoreMetadata& aMetadata)
      : VersionChangeTransactionOp(std::move(aTransaction)),
        mMetadata(aMetadata) {
    MOZ_ASSERT(aMetadata.id());
  }

  ~CreateObjectStoreOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class DeleteObjectStoreOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const RefPtr<FullObjectStoreMetadata> mMetadata;
  const bool mIsLastObjectStore;

 private:
  // Only created by VersionChangeTransaction.
  DeleteObjectStoreOp(SafeRefPtr<VersionChangeTransaction> aTransaction,
                      FullObjectStoreMetadata* const aMetadata,
                      const bool aIsLastObjectStore)
      : VersionChangeTransactionOp(std::move(aTransaction)),
        mMetadata(aMetadata),
        mIsLastObjectStore(aIsLastObjectStore) {
    MOZ_ASSERT(aMetadata->mCommonMetadata.id());
  }

  ~DeleteObjectStoreOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class RenameObjectStoreOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const int64_t mId;
  const nsString mNewName;

 private:
  // Only created by VersionChangeTransaction.
  RenameObjectStoreOp(SafeRefPtr<VersionChangeTransaction> aTransaction,
                      FullObjectStoreMetadata* const aMetadata)
      : VersionChangeTransactionOp(std::move(aTransaction)),
        mId(aMetadata->mCommonMetadata.id()),
        mNewName(aMetadata->mCommonMetadata.name()) {
    MOZ_ASSERT(mId);
  }

  ~RenameObjectStoreOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class CreateIndexOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  class UpdateIndexDataValuesFunction;

  const IndexMetadata mMetadata;
  Maybe<UniqueIndexTable> mMaybeUniqueIndexTable;
  const SafeRefPtr<FileManager> mFileManager;
  const nsCString mDatabaseId;
  const IndexOrObjectStoreId mObjectStoreId;

 private:
  // Only created by VersionChangeTransaction.
  CreateIndexOp(SafeRefPtr<VersionChangeTransaction> aTransaction,
                IndexOrObjectStoreId aObjectStoreId,
                const IndexMetadata& aMetadata);

  ~CreateIndexOp() override = default;

  nsresult InsertDataFromObjectStore(DatabaseConnection* aConnection);

  nsresult InsertDataFromObjectStoreInternal(DatabaseConnection* aConnection);

  bool Init(TransactionBase& aTransaction) override;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class CreateIndexOp::UpdateIndexDataValuesFunction final
    : public mozIStorageFunction {
  RefPtr<CreateIndexOp> mOp;
  RefPtr<DatabaseConnection> mConnection;
  const NotNull<SafeRefPtr<Database>> mDatabase;

 public:
  UpdateIndexDataValuesFunction(CreateIndexOp* aOp,
                                DatabaseConnection* aConnection,
                                SafeRefPtr<Database> aDatabase)
      : mOp(aOp),
        mConnection(aConnection),
        mDatabase(WrapNotNull(std::move(aDatabase))) {
    MOZ_ASSERT(aOp);
    MOZ_ASSERT(aConnection);
    aConnection->AssertIsOnConnectionThread();
  }

  NS_DECL_ISUPPORTS

 private:
  ~UpdateIndexDataValuesFunction() = default;

  NS_DECL_MOZISTORAGEFUNCTION
};

class DeleteIndexOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const IndexOrObjectStoreId mObjectStoreId;
  const IndexOrObjectStoreId mIndexId;
  const bool mUnique;
  const bool mIsLastIndex;

 private:
  // Only created by VersionChangeTransaction.
  DeleteIndexOp(SafeRefPtr<VersionChangeTransaction> aTransaction,
                IndexOrObjectStoreId aObjectStoreId,
                IndexOrObjectStoreId aIndexId, const bool aUnique,
                const bool aIsLastIndex);

  ~DeleteIndexOp() override = default;

  nsresult RemoveReferencesToIndex(DatabaseConnection* aConnection,
                                   const Key& aObjectDataKey,
                                   nsTArray<IndexDataValue>& aIndexValues);

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class RenameIndexOp final : public VersionChangeTransactionOp {
  friend class VersionChangeTransaction;

  const IndexOrObjectStoreId mObjectStoreId;
  const IndexOrObjectStoreId mIndexId;
  const nsString mNewName;

 private:
  // Only created by VersionChangeTransaction.
  RenameIndexOp(SafeRefPtr<VersionChangeTransaction> aTransaction,
                FullIndexMetadata* const aMetadata,
                IndexOrObjectStoreId aObjectStoreId)
      : VersionChangeTransactionOp(std::move(aTransaction)),
        mObjectStoreId(aObjectStoreId),
        mIndexId(aMetadata->mCommonMetadata.id()),
        mNewName(aMetadata->mCommonMetadata.name()) {
    MOZ_ASSERT(mIndexId);
  }

  ~RenameIndexOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

class NormalTransactionOp : public TransactionDatabaseOperationBase,
                            public PBackgroundIDBRequestParent {
#ifdef DEBUG
  bool mResponseSent;
#endif

 public:
  void Cleanup() override;

 protected:
  explicit NormalTransactionOp(SafeRefPtr<TransactionBase> aTransaction)
      : TransactionDatabaseOperationBase(std::move(aTransaction))
#ifdef DEBUG
        ,
        mResponseSent(false)
#endif
  {
  }

  ~NormalTransactionOp() override = default;

  // An overload of DatabaseOperationBase's function that can avoid doing extra
  // work on non-versionchange transactions.
  mozilla::Result<bool, nsresult> ObjectStoreHasIndexes(
      DatabaseConnection& aConnection, IndexOrObjectStoreId aObjectStoreId,
      bool aMayHaveIndexes);

  virtual mozilla::Result<PreprocessParams, nsresult> GetPreprocessParams();

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(RequestResponse& aResponse,
                           size_t* aResponseSize) = 0;

 private:
  nsresult SendPreprocessInfo() override;

  nsresult SendSuccessResult() override;

  bool SendFailureResult(nsresult aResultCode) override;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvContinue(
      const PreprocessResponse& aResponse) final;
};

class ObjectStoreAddOrPutRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  typedef mozilla::dom::quota::PersistenceType PersistenceType;

  class StoredFileInfo final {
    InitializedOnce<const NotNull<SafeRefPtr<FileInfo>>> mFileInfo;
    // Either nothing, a file actor or a non-Blob-backed inputstream to write to
    // disk.
    using FileActorOrInputStream =
        Variant<Nothing, RefPtr<DatabaseFile>, nsCOMPtr<nsIInputStream>>;
    InitializedOnce<const FileActorOrInputStream> mFileActorOrInputStream;
#ifdef DEBUG
    const StructuredCloneFileBase::FileType mType;
#endif

    void AssertInvariants() const;

    explicit StoredFileInfo(SafeRefPtr<FileInfo> aFileInfo);

    StoredFileInfo(SafeRefPtr<FileInfo> aFileInfo,
                   RefPtr<DatabaseFile> aFileActor);

    StoredFileInfo(SafeRefPtr<FileInfo> aFileInfo,
                   nsCOMPtr<nsIInputStream> aInputStream);

   public:
#if defined(NS_BUILD_REFCNT_LOGGING)
    // Only for MOZ_COUNT_CTOR.
    StoredFileInfo(StoredFileInfo&& aOther)
        : mFileInfo{std::move(aOther.mFileInfo)}, mFileActorOrInputStream {
      std::move(aOther.mFileActorOrInputStream)
    }
#  ifdef DEBUG
    , mType { aOther.mType }
#  endif
    { MOZ_COUNT_CTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo); }
#else
    StoredFileInfo(StoredFileInfo&&) = default;
#endif

    static StoredFileInfo CreateForMutableFile(SafeRefPtr<FileInfo> aFileInfo);
    static StoredFileInfo CreateForBlob(SafeRefPtr<FileInfo> aFileInfo,
                                        RefPtr<DatabaseFile> aFileActor);
    static StoredFileInfo CreateForStructuredClone(
        SafeRefPtr<FileInfo> aFileInfo, nsCOMPtr<nsIInputStream> aInputStream);

#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING)
    ~StoredFileInfo() {
      AssertIsOnBackgroundThread();
      AssertInvariants();

      MOZ_COUNT_DTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo);
    }
#endif

    bool IsValid() const { return static_cast<bool>(mFileInfo); }

    const FileInfo& GetFileInfo() const { return **mFileInfo; }

    bool ShouldCompress() const;

    void NotifyWriteSucceeded() const;

    using InputStreamResult =
        mozilla::Result<nsCOMPtr<nsIInputStream>, nsresult>;
    InputStreamResult GetInputStream();

    void Serialize(nsString& aText) const;
  };
  class SCInputStream;

  const ObjectStoreAddPutParams mParams;
  Maybe<UniqueIndexTable> mUniqueIndexTable;

  // This must be non-const so that we can update the mNextAutoIncrementId field
  // if we are modifying an autoIncrement objectStore.
  RefPtr<FullObjectStoreMetadata> mMetadata;

  nsTArray<StoredFileInfo> mStoredFileInfos;

  Key mResponse;
  const OriginMetadata mOriginMetadata;
  const PersistenceType mPersistenceType;
  const bool mOverwrite;
  bool mObjectStoreMayHaveIndexes;
  bool mDataOverThreshold;

 private:
  // Only created by TransactionBase.
  ObjectStoreAddOrPutRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                               RequestParams&& aParams);

  ~ObjectStoreAddOrPutRequestOp() override = default;

  nsresult RemoveOldIndexDataValues(DatabaseConnection* aConnection);

  bool Init(TransactionBase& aTransaction) override;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;

  void Cleanup() override;
};

void ObjectStoreAddOrPutRequestOp::StoredFileInfo::AssertInvariants() const {
  // The only allowed types are eStructuredClone, eBlob and eMutableFile.
  MOZ_ASSERT(StructuredCloneFileBase::eStructuredClone == mType ||
             StructuredCloneFileBase::eBlob == mType ||
             StructuredCloneFileBase::eMutableFile == mType);

  // mFileInfo and a file actor in mFileActorOrInputStream are present until
  // the object is moved away, but an inputStream in mFileActorOrInputStream
  // can be released early.
  MOZ_ASSERT_IF(static_cast<bool>(mFileActorOrInputStream) &&
                    mFileActorOrInputStream->is<RefPtr<DatabaseFile>>(),
                static_cast<bool>(mFileInfo));

  if (mFileInfo) {
    // In a non-moved StoredFileInfo, one of the following is true:
    // - This was an overflow structured clone (eStructuredClone) and
    //   storedFileInfo.mFileActorOrInputStream CAN be a non-nullptr input
    //   stream (but that might have been release by ReleaseInputStream).
    MOZ_ASSERT_IF(
        StructuredCloneFileBase::eStructuredClone == mType,
        !mFileActorOrInputStream ||
            (mFileActorOrInputStream->is<nsCOMPtr<nsIInputStream>>() &&
             mFileActorOrInputStream->as<nsCOMPtr<nsIInputStream>>()));

    // - This is a reference to a Blob (eBlob) that may or may not have
    //   already been written to disk.  storedFileInfo.mFileActorOrInputStream
    //   MUST be a non-null file actor, but its GetInputStream may return
    //   nullptr (so don't assert on that).
    MOZ_ASSERT_IF(StructuredCloneFileBase::eBlob == mType,
                  mFileActorOrInputStream->is<RefPtr<DatabaseFile>>() &&
                      mFileActorOrInputStream->as<RefPtr<DatabaseFile>>());

    // - It's a mutable file (eMutableFile).  No writing will be performed,
    // and storedFileInfo.mFileActorOrInputStream is Nothing.
    MOZ_ASSERT_IF(StructuredCloneFileBase::eMutableFile == mType,
                  mFileActorOrInputStream->is<Nothing>());
  }
}

ObjectStoreAddOrPutRequestOp::StoredFileInfo::StoredFileInfo(
    SafeRefPtr<FileInfo> aFileInfo)
    : mFileInfo{WrapNotNull(std::move(aFileInfo))}, mFileActorOrInputStream {
  Nothing {}
}
#ifdef DEBUG
, mType { StructuredCloneFileBase::eMutableFile }
#endif
{
  AssertIsOnBackgroundThread();
  AssertInvariants();

  MOZ_COUNT_CTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo);
}

ObjectStoreAddOrPutRequestOp::StoredFileInfo::StoredFileInfo(
    SafeRefPtr<FileInfo> aFileInfo, RefPtr<DatabaseFile> aFileActor)
    : mFileInfo{WrapNotNull(std::move(aFileInfo))}, mFileActorOrInputStream {
  std::move(aFileActor)
}
#ifdef DEBUG
, mType { StructuredCloneFileBase::eBlob }
#endif
{
  AssertIsOnBackgroundThread();
  AssertInvariants();

  MOZ_COUNT_CTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo);
}

ObjectStoreAddOrPutRequestOp::StoredFileInfo::StoredFileInfo(
    SafeRefPtr<FileInfo> aFileInfo, nsCOMPtr<nsIInputStream> aInputStream)
    : mFileInfo{WrapNotNull(std::move(aFileInfo))}, mFileActorOrInputStream {
  std::move(aInputStream)
}
#ifdef DEBUG
, mType { StructuredCloneFileBase::eStructuredClone }
#endif
{
  AssertIsOnBackgroundThread();
  AssertInvariants();

  MOZ_COUNT_CTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo);
}

ObjectStoreAddOrPutRequestOp::StoredFileInfo
ObjectStoreAddOrPutRequestOp::StoredFileInfo::CreateForMutableFile(
    SafeRefPtr<FileInfo> aFileInfo) {
  return StoredFileInfo{std::move(aFileInfo)};
}

ObjectStoreAddOrPutRequestOp::StoredFileInfo
ObjectStoreAddOrPutRequestOp::StoredFileInfo::CreateForBlob(
    SafeRefPtr<FileInfo> aFileInfo, RefPtr<DatabaseFile> aFileActor) {
  return {std::move(aFileInfo), std::move(aFileActor)};
}

ObjectStoreAddOrPutRequestOp::StoredFileInfo
ObjectStoreAddOrPutRequestOp::StoredFileInfo::CreateForStructuredClone(
    SafeRefPtr<FileInfo> aFileInfo, nsCOMPtr<nsIInputStream> aInputStream) {
  return {std::move(aFileInfo), std::move(aInputStream)};
}

bool ObjectStoreAddOrPutRequestOp::StoredFileInfo::ShouldCompress() const {
  // Must not be called after moving.
  MOZ_ASSERT(IsValid());

  // Compression is only necessary for eStructuredClone, i.e. when
  // mFileActorOrInputStream stored an input stream. However, this is only
  // called after GetInputStream, when mFileActorOrInputStream has been
  // cleared, which is only possible for this type.
  const bool res = !mFileActorOrInputStream;
  MOZ_ASSERT(res == (StructuredCloneFileBase::eStructuredClone == mType));
  return res;
}

void ObjectStoreAddOrPutRequestOp::StoredFileInfo::NotifyWriteSucceeded()
    const {
  MOZ_ASSERT(IsValid());

  // For eBlob, clear the blob implementation.
  if (mFileActorOrInputStream &&
      mFileActorOrInputStream->is<RefPtr<DatabaseFile>>()) {
    mFileActorOrInputStream->as<RefPtr<DatabaseFile>>()
        ->WriteSucceededClearBlobImpl();
  }

  // For the other types, no action is necessary.
}

ObjectStoreAddOrPutRequestOp::StoredFileInfo::InputStreamResult
ObjectStoreAddOrPutRequestOp::StoredFileInfo::GetInputStream() {
  if (!mFileActorOrInputStream) {
    MOZ_ASSERT(StructuredCloneFileBase::eStructuredClone == mType);
    return nsCOMPtr<nsIInputStream>{};
  }

  // For the different cases, see also the comments in AssertInvariants.
  return mFileActorOrInputStream->match(
      [](const Nothing&) -> InputStreamResult {
        return nsCOMPtr<nsIInputStream>{};
      },
      [](const RefPtr<DatabaseFile>& databaseActor) -> InputStreamResult {
        ErrorResult rv;
        auto inputStream = databaseActor->GetInputStream(rv);
        if (NS_WARN_IF(rv.Failed())) {
          return Err(rv.StealNSResult());
        }

        return inputStream;
      },
      [this](const nsCOMPtr<nsIInputStream>& inputStream) -> InputStreamResult {
        auto res = inputStream;
        // destroy() clears the inputStream parameter, so we needed to make a
        // copy before
        mFileActorOrInputStream.destroy();
        AssertInvariants();
        return res;
      });
}

void ObjectStoreAddOrPutRequestOp::StoredFileInfo::Serialize(
    nsString& aText) const {
  AssertInvariants();
  MOZ_ASSERT(IsValid());

  const int64_t id = (*mFileInfo)->Id();

  auto structuredCloneHandler = [&aText, id](const nsCOMPtr<nsIInputStream>&) {
    // eStructuredClone
    aText.Append('.');
    aText.AppendInt(id);
  };

  // If mFileActorOrInputStream was moved, we had an inputStream before.
  if (!mFileActorOrInputStream) {
    structuredCloneHandler(nullptr);
    return;
  }

  // This encoding is parsed in DeserializeStructuredCloneFile.
  mFileActorOrInputStream->match(
      [&aText, id](const Nothing&) {
        // eMutableFile
        aText.AppendInt(-id);
      },
      [&aText, id](const RefPtr<DatabaseFile>&) {
        // eBlob
        aText.AppendInt(id);
      },
      structuredCloneHandler);
}

class ObjectStoreAddOrPutRequestOp::SCInputStream final
    : public nsIInputStream {
  const JSStructuredCloneData& mData;
  JSStructuredCloneData::Iterator mIter;

 public:
  explicit SCInputStream(const JSStructuredCloneData& aData)
      : mData(aData), mIter(aData.Start()) {}

 private:
  virtual ~SCInputStream() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
};

class ObjectStoreGetRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const IndexOrObjectStoreId mObjectStoreId;
  SafeRefPtr<Database> mDatabase;
  const Maybe<SerializedKeyRange> mOptionalKeyRange;
  AutoTArray<StructuredCloneReadInfoParent, 1> mResponse;
  PBackgroundParent* mBackgroundParent;
  uint32_t mPreprocessInfoCount;
  const uint32_t mLimit;
  const bool mGetAll;

 private:
  // Only created by TransactionBase.
  ObjectStoreGetRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                          const RequestParams& aParams, bool aGetAll);

  ~ObjectStoreGetRequestOp() override = default;

  template <typename T>
  mozilla::Result<T, nsresult> ConvertResponse(
      StructuredCloneReadInfoParent&& aInfo);

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  bool HasPreprocessInfo() override;

  mozilla::Result<PreprocessParams, nsresult> GetPreprocessParams() override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;
};

class ObjectStoreGetKeyRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const IndexOrObjectStoreId mObjectStoreId;
  const Maybe<SerializedKeyRange> mOptionalKeyRange;
  const uint32_t mLimit;
  const bool mGetAll;
  nsTArray<Key> mResponse;

 private:
  // Only created by TransactionBase.
  ObjectStoreGetKeyRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                             const RequestParams& aParams, bool aGetAll);

  ~ObjectStoreGetKeyRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;
};

class ObjectStoreDeleteRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const ObjectStoreDeleteParams mParams;
  ObjectStoreDeleteResponse mResponse;
  bool mObjectStoreMayHaveIndexes;

 private:
  ObjectStoreDeleteRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                             const ObjectStoreDeleteParams& aParams);

  ~ObjectStoreDeleteRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override {
    aResponse = std::move(mResponse);
    *aResponseSize = 0;
  }
};

class ObjectStoreClearRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const ObjectStoreClearParams mParams;
  ObjectStoreClearResponse mResponse;
  bool mObjectStoreMayHaveIndexes;

 private:
  ObjectStoreClearRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                            const ObjectStoreClearParams& aParams);

  ~ObjectStoreClearRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override {
    aResponse = std::move(mResponse);
    *aResponseSize = 0;
  }
};

class ObjectStoreCountRequestOp final : public NormalTransactionOp {
  friend class TransactionBase;

  const ObjectStoreCountParams mParams;
  ObjectStoreCountResponse mResponse;

 private:
  ObjectStoreCountRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                            const ObjectStoreCountParams& aParams)
      : NormalTransactionOp(std::move(aTransaction)), mParams(aParams) {}

  ~ObjectStoreCountRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override {
    aResponse = std::move(mResponse);
    *aResponseSize = sizeof(uint64_t);
  }
};

class IndexRequestOpBase : public NormalTransactionOp {
 protected:
  const RefPtr<FullIndexMetadata> mMetadata;

 protected:
  IndexRequestOpBase(SafeRefPtr<TransactionBase> aTransaction,
                     const RequestParams& aParams)
      : NormalTransactionOp(std::move(aTransaction)),
        mMetadata(IndexMetadataForParams(Transaction(), aParams)) {}

  ~IndexRequestOpBase() override = default;

 private:
  static RefPtr<FullIndexMetadata> IndexMetadataForParams(
      const TransactionBase& aTransaction, const RequestParams& aParams);
};

class IndexGetRequestOp final : public IndexRequestOpBase {
  friend class TransactionBase;

  SafeRefPtr<Database> mDatabase;
  const Maybe<SerializedKeyRange> mOptionalKeyRange;
  AutoTArray<StructuredCloneReadInfoParent, 1> mResponse;
  PBackgroundParent* mBackgroundParent;
  const uint32_t mLimit;
  const bool mGetAll;

 private:
  // Only created by TransactionBase.
  IndexGetRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                    const RequestParams& aParams, bool aGetAll);

  ~IndexGetRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;
};

class IndexGetKeyRequestOp final : public IndexRequestOpBase {
  friend class TransactionBase;

  const Maybe<SerializedKeyRange> mOptionalKeyRange;
  AutoTArray<Key, 1> mResponse;
  const uint32_t mLimit;
  const bool mGetAll;

 private:
  // Only created by TransactionBase.
  IndexGetKeyRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                       const RequestParams& aParams, bool aGetAll);

  ~IndexGetKeyRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override;
};

class IndexCountRequestOp final : public IndexRequestOpBase {
  friend class TransactionBase;

  const IndexCountParams mParams;
  IndexCountResponse mResponse;

 private:
  // Only created by TransactionBase.
  IndexCountRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                      const RequestParams& aParams)
      : IndexRequestOpBase(std::move(aTransaction), aParams),
        mParams(aParams.get_IndexCountParams()) {}

  ~IndexCountRequestOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  void GetResponse(RequestResponse& aResponse, size_t* aResponseSize) override {
    aResponse = std::move(mResponse);
    *aResponseSize = sizeof(uint64_t);
  }
};

template <IDBCursorType CursorType>
class Cursor;

constexpr IDBCursorType ToKeyOnlyType(const IDBCursorType aType) {
  MOZ_ASSERT(aType == IDBCursorType::ObjectStore ||
             aType == IDBCursorType::ObjectStoreKey ||
             aType == IDBCursorType::Index || aType == IDBCursorType::IndexKey);
  switch (aType) {
    case IDBCursorType::ObjectStore:
    case IDBCursorType::ObjectStoreKey:
      return IDBCursorType::ObjectStoreKey;
    case IDBCursorType::Index:
    case IDBCursorType::IndexKey:
      return IDBCursorType::IndexKey;
  }
}

template <IDBCursorType CursorType>
using CursorPosition = CursorData<ToKeyOnlyType(CursorType)>;

#ifdef DEBUG
constexpr indexedDB::OpenCursorParams::Type ToOpenCursorParamsType(
    const IDBCursorType aType) {
  MOZ_ASSERT(aType == IDBCursorType::ObjectStore ||
             aType == IDBCursorType::ObjectStoreKey ||
             aType == IDBCursorType::Index || aType == IDBCursorType::IndexKey);
  switch (aType) {
    case IDBCursorType::ObjectStore:
      return indexedDB::OpenCursorParams::TObjectStoreOpenCursorParams;
    case IDBCursorType::ObjectStoreKey:
      return indexedDB::OpenCursorParams::TObjectStoreOpenKeyCursorParams;
    case IDBCursorType::Index:
      return indexedDB::OpenCursorParams::TIndexOpenCursorParams;
    case IDBCursorType::IndexKey:
      return indexedDB::OpenCursorParams::TIndexOpenKeyCursorParams;
  }
}
#endif

class CursorBase : public PBackgroundIDBCursorParent {
  friend class TransactionBase;
  template <IDBCursorType CursorType>
  friend class CommonOpenOpHelper;

 protected:
  const SafeRefPtr<TransactionBase> mTransaction;

  // This should only be touched on the PBackground thread to check whether
  // the objectStore has been deleted. Holding these saves a hash lookup for
  // every call to continue()/advance().
  InitializedOnce<const NotNull<RefPtr<FullObjectStoreMetadata>>>
      mObjectStoreMetadata;

  const IndexOrObjectStoreId mObjectStoreId;

  LazyInitializedOnce<const Key>
      mLocaleAwareRangeBound;  ///< If the cursor is based on a key range, the
                               ///< bound in the direction of iteration (e.g.
                               ///< the upper bound in case of mDirection ==
                               ///< NEXT). If the cursor is based on a key, it
                               ///< is unset. If mLocale is set, this was
                               ///< converted to mLocale.

  const Direction mDirection;

  const int32_t mMaxExtraCount;

  const bool mIsSameProcessActor;

  struct ConstructFromTransactionBase {};

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::CursorBase,
                                        final)

  CursorBase(SafeRefPtr<TransactionBase> aTransaction,
             RefPtr<FullObjectStoreMetadata> aObjectStoreMetadata,
             Direction aDirection,
             ConstructFromTransactionBase aConstructionTag);

 protected:
  // Reference counted.
  ~CursorBase() override { MOZ_ASSERT(!mObjectStoreMetadata); }

 private:
  virtual bool Start(const OpenCursorParams& aParams) = 0;
};

class IndexCursorBase : public CursorBase {
 public:
  bool IsLocaleAware() const { return !mLocale.IsEmpty(); }

  IndexCursorBase(SafeRefPtr<TransactionBase> aTransaction,
                  RefPtr<FullObjectStoreMetadata> aObjectStoreMetadata,
                  RefPtr<FullIndexMetadata> aIndexMetadata,
                  Direction aDirection,
                  ConstructFromTransactionBase aConstructionTag)
      : CursorBase{std::move(aTransaction), std::move(aObjectStoreMetadata),
                   aDirection, aConstructionTag},
        mIndexMetadata(WrapNotNull(std::move(aIndexMetadata))),
        mIndexId((*mIndexMetadata)->mCommonMetadata.id()),
        mUniqueIndex((*mIndexMetadata)->mCommonMetadata.unique()),
        mLocale((*mIndexMetadata)->mCommonMetadata.locale()) {}

 protected:
  IndexOrObjectStoreId Id() const { return mIndexId; }

  // This should only be touched on the PBackground thread to check whether
  // the index has been deleted. Holding these saves a hash lookup for every
  // call to continue()/advance().
  InitializedOnce<const NotNull<RefPtr<FullIndexMetadata>>> mIndexMetadata;
  const IndexOrObjectStoreId mIndexId;
  const bool mUniqueIndex;
  const nsCString
      mLocale;  ///< The locale if the cursor is locale-aware, otherwise empty.

  struct ContinueQueries {
    nsCString mContinueQuery;
    nsCString mContinueToQuery;
    nsCString mContinuePrimaryKeyQuery;

    const nsCString& GetContinueQuery(const bool hasContinueKey,
                                      const bool hasContinuePrimaryKey) const {
      return hasContinuePrimaryKey ? mContinuePrimaryKeyQuery
             : hasContinueKey      ? mContinueToQuery
                                   : mContinueQuery;
    }
  };
};

class ObjectStoreCursorBase : public CursorBase {
 public:
  using CursorBase::CursorBase;

  static constexpr bool IsLocaleAware() { return false; }

 protected:
  IndexOrObjectStoreId Id() const { return mObjectStoreId; }

  struct ContinueQueries {
    nsCString mContinueQuery;
    nsCString mContinueToQuery;

    const nsCString& GetContinueQuery(const bool hasContinueKey,
                                      const bool hasContinuePrimaryKey) const {
      MOZ_ASSERT(!hasContinuePrimaryKey);
      return hasContinueKey ? mContinueToQuery : mContinueQuery;
    }
  };
};

using FilesArray = nsTArray<nsTArray<StructuredCloneFileParent>>;

struct PseudoFilesArray {
  static constexpr bool IsEmpty() { return true; }

  static constexpr void Clear() {}
};

template <IDBCursorType CursorType>
using FilesArrayT =
    std::conditional_t<!CursorTypeTraits<CursorType>::IsKeyOnlyCursor,
                       FilesArray, PseudoFilesArray>;

class ValueCursorBase {
  friend struct ValuePopulateResponseHelper<true>;
  friend struct ValuePopulateResponseHelper<false>;

 protected:
  explicit ValueCursorBase(TransactionBase* const aTransaction)
      : mDatabase(aTransaction->GetDatabasePtr()),
        mFileManager(mDatabase->GetFileManagerPtr()),
        mBackgroundParent(WrapNotNull(aTransaction->GetBackgroundParent())) {
    MOZ_ASSERT(mDatabase);
  }

  void ProcessFiles(CursorResponse& aResponse, const FilesArray& aFiles);

  ~ValueCursorBase() { MOZ_ASSERT(!mBackgroundParent); }

  const SafeRefPtr<Database> mDatabase;
  const NotNull<SafeRefPtr<FileManager>> mFileManager;

  InitializedOnce<const NotNull<PBackgroundParent*>> mBackgroundParent;
};

class KeyCursorBase {
 protected:
  explicit KeyCursorBase(TransactionBase* const /*aTransaction*/) {}

  static constexpr void ProcessFiles(CursorResponse& aResponse,
                                     const PseudoFilesArray& aFiles) {}
};

template <IDBCursorType CursorType>
class CursorOpBaseHelperBase;

template <IDBCursorType CursorType>
class Cursor final
    : public std::conditional_t<
          CursorTypeTraits<CursorType>::IsObjectStoreCursor,
          ObjectStoreCursorBase, IndexCursorBase>,
      public std::conditional_t<CursorTypeTraits<CursorType>::IsKeyOnlyCursor,
                                KeyCursorBase, ValueCursorBase> {
  using Base =
      std::conditional_t<CursorTypeTraits<CursorType>::IsObjectStoreCursor,
                         ObjectStoreCursorBase, IndexCursorBase>;

  using KeyValueBase =
      std::conditional_t<CursorTypeTraits<CursorType>::IsKeyOnlyCursor,
                         KeyCursorBase, ValueCursorBase>;

  static constexpr bool IsIndexCursor =
      !CursorTypeTraits<CursorType>::IsObjectStoreCursor;

  static constexpr bool IsValueCursor =
      !CursorTypeTraits<CursorType>::IsKeyOnlyCursor;

  class CursorOpBase;
  class OpenOp;
  class ContinueOp;

  using Base::Id;
  using CursorBase::Manager;
  using CursorBase::mDirection;
  using CursorBase::mObjectStoreId;
  using CursorBase::mTransaction;
  using typename CursorBase::ActorDestroyReason;

  using TypedOpenOpHelper =
      std::conditional_t<IsIndexCursor, IndexOpenOpHelper<CursorType>,
                         ObjectStoreOpenOpHelper<CursorType>>;

  friend class CursorOpBaseHelperBase<CursorType>;
  friend class CommonOpenOpHelper<CursorType>;
  friend TypedOpenOpHelper;
  friend class OpenOpHelper<CursorType>;

  CursorOpBase* mCurrentlyRunningOp = nullptr;

  LazyInitializedOnce<const typename Base::ContinueQueries> mContinueQueries;

  // Only called by TransactionBase.
  bool Start(const OpenCursorParams& aParams) final;

  void SendResponseInternal(CursorResponse& aResponse,
                            const FilesArrayT<CursorType>& aFiles);

  // Must call SendResponseInternal!
  bool SendResponse(const CursorResponse& aResponse) = delete;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvContinue(
      const CursorRequestParams& aParams, const Key& aCurrentKey,
      const Key& aCurrentObjectStoreKey) override;

 public:
  Cursor(SafeRefPtr<TransactionBase> aTransaction,
         RefPtr<FullObjectStoreMetadata> aObjectStoreMetadata,
         RefPtr<FullIndexMetadata> aIndexMetadata,
         typename Base::Direction aDirection,
         typename Base::ConstructFromTransactionBase aConstructionTag)
      : Base{std::move(aTransaction), std::move(aObjectStoreMetadata),
             std::move(aIndexMetadata), aDirection, aConstructionTag},
        KeyValueBase{this->mTransaction.unsafeGetRawPtr()} {}

  Cursor(SafeRefPtr<TransactionBase> aTransaction,
         RefPtr<FullObjectStoreMetadata> aObjectStoreMetadata,
         typename Base::Direction aDirection,
         typename Base::ConstructFromTransactionBase aConstructionTag)
      : Base{std::move(aTransaction), std::move(aObjectStoreMetadata),
             aDirection, aConstructionTag},
        KeyValueBase{this->mTransaction.unsafeGetRawPtr()} {}

 private:
  void SetOptionalKeyRange(const Maybe<SerializedKeyRange>& aOptionalKeyRange,
                           bool* aOpen);

  bool VerifyRequestParams(const CursorRequestParams& aParams,
                           const CursorPosition<CursorType>& aPosition) const;

  ~Cursor() final = default;
};

template <IDBCursorType CursorType>
class Cursor<CursorType>::CursorOpBase
    : public TransactionDatabaseOperationBase {
  friend class CursorOpBaseHelperBase<CursorType>;

 protected:
  RefPtr<Cursor> mCursor;
  FilesArrayT<CursorType> mFiles;  // TODO: Consider removing this member
                                   // entirely if we are no value cursor.

  CursorResponse mResponse;

#ifdef DEBUG
  bool mResponseSent;
#endif

 protected:
  explicit CursorOpBase(Cursor* aCursor)
      : TransactionDatabaseOperationBase(aCursor->mTransaction.clonePtr()),
        mCursor(aCursor)
#ifdef DEBUG
        ,
        mResponseSent(false)
#endif
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aCursor);
  }

  ~CursorOpBase() override = default;

  bool SendFailureResult(nsresult aResultCode) final;
  nsresult SendSuccessResult() final;

  void Cleanup() override;
};

template <IDBCursorType CursorType>
class OpenOpHelper;

using ResponseSizeOrError = Result<size_t, nsresult>;

template <IDBCursorType CursorType>
class CursorOpBaseHelperBase {
 public:
  explicit CursorOpBaseHelperBase(
      typename Cursor<CursorType>::CursorOpBase& aOp)
      : mOp{aOp} {}

  ResponseSizeOrError PopulateResponseFromStatement(mozIStorageStatement* aStmt,
                                                    bool aInitializeResponse,
                                                    Key* const aOptOutSortKey);

  void PopulateExtraResponses(mozIStorageStatement* aStmt,
                              uint32_t aMaxExtraCount,
                              const size_t aInitialResponseSize,
                              const nsCString& aOperation,
                              Key* const aOptPreviousSortKey);

 protected:
  Cursor<CursorType>& GetCursor() {
    MOZ_ASSERT(mOp.mCursor);
    return *mOp.mCursor;
  }

  void SetResponse(CursorResponse aResponse) {
    mOp.mResponse = std::move(aResponse);
  }

 protected:
  typename Cursor<CursorType>::CursorOpBase& mOp;
};

class CommonOpenOpHelperBase {
 protected:
  static void AppendConditionClause(const nsACString& aColumnName,
                                    const nsACString& aStatementParameterName,
                                    bool aLessThan, bool aEquals,
                                    nsCString& aResult);
};

template <IDBCursorType CursorType>
class CommonOpenOpHelper : public CursorOpBaseHelperBase<CursorType>,
                           protected CommonOpenOpHelperBase {
 public:
  explicit CommonOpenOpHelper(typename Cursor<CursorType>::OpenOp& aOp)
      : CursorOpBaseHelperBase<CursorType>{aOp} {}

 protected:
  using CursorOpBaseHelperBase<CursorType>::GetCursor;
  using CursorOpBaseHelperBase<CursorType>::PopulateExtraResponses;
  using CursorOpBaseHelperBase<CursorType>::PopulateResponseFromStatement;
  using CursorOpBaseHelperBase<CursorType>::SetResponse;

  const Maybe<SerializedKeyRange>& GetOptionalKeyRange() const {
    // This downcast is safe, since we initialized mOp from an OpenOp in the
    // ctor.
    return static_cast<typename Cursor<CursorType>::OpenOp&>(this->mOp)
        .mOptionalKeyRange;
  }

  nsresult ProcessStatementSteps(mozIStorageStatement* aStmt);
};

template <IDBCursorType CursorType>
class ObjectStoreOpenOpHelper : protected CommonOpenOpHelper<CursorType> {
 public:
  using CommonOpenOpHelper<CursorType>::CommonOpenOpHelper;

 protected:
  using CommonOpenOpHelper<CursorType>::GetCursor;
  using CommonOpenOpHelper<CursorType>::GetOptionalKeyRange;
  using CommonOpenOpHelper<CursorType>::AppendConditionClause;

  void PrepareKeyConditionClauses(const nsACString& aDirectionClause,
                                  const nsCString& aQueryStart);
};

template <IDBCursorType CursorType>
class IndexOpenOpHelper : protected CommonOpenOpHelper<CursorType> {
 public:
  using CommonOpenOpHelper<CursorType>::CommonOpenOpHelper;

 protected:
  using CommonOpenOpHelper<CursorType>::GetCursor;
  using CommonOpenOpHelper<CursorType>::GetOptionalKeyRange;
  using CommonOpenOpHelper<CursorType>::AppendConditionClause;

  void PrepareIndexKeyConditionClause(
      const nsACString& aDirectionClause,
      const nsLiteralCString& aObjectDataKeyPrefix, nsAutoCString aQueryStart);
};

template <>
class OpenOpHelper<IDBCursorType::ObjectStore>
    : public ObjectStoreOpenOpHelper<IDBCursorType::ObjectStore> {
 public:
  using ObjectStoreOpenOpHelper<
      IDBCursorType::ObjectStore>::ObjectStoreOpenOpHelper;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection);
};

template <>
class OpenOpHelper<IDBCursorType::ObjectStoreKey>
    : public ObjectStoreOpenOpHelper<IDBCursorType::ObjectStoreKey> {
 public:
  using ObjectStoreOpenOpHelper<
      IDBCursorType::ObjectStoreKey>::ObjectStoreOpenOpHelper;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection);
};

template <>
class OpenOpHelper<IDBCursorType::Index>
    : IndexOpenOpHelper<IDBCursorType::Index> {
 private:
  void PrepareKeyConditionClauses(const nsACString& aDirectionClause,
                                  nsAutoCString aQueryStart) {
    PrepareIndexKeyConditionClause(aDirectionClause, "index_table."_ns,
                                   std::move(aQueryStart));
  }

 public:
  using IndexOpenOpHelper<IDBCursorType::Index>::IndexOpenOpHelper;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection);
};

template <>
class OpenOpHelper<IDBCursorType::IndexKey>
    : IndexOpenOpHelper<IDBCursorType::IndexKey> {
 private:
  void PrepareKeyConditionClauses(const nsACString& aDirectionClause,
                                  nsAutoCString aQueryStart) {
    PrepareIndexKeyConditionClause(aDirectionClause, ""_ns,
                                   std::move(aQueryStart));
  }

 public:
  using IndexOpenOpHelper<IDBCursorType::IndexKey>::IndexOpenOpHelper;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection);
};

template <IDBCursorType CursorType>
class Cursor<CursorType>::OpenOp final : public CursorOpBase {
  friend class Cursor<CursorType>;
  friend class CommonOpenOpHelper<CursorType>;

  const Maybe<SerializedKeyRange> mOptionalKeyRange;

  using CursorOpBase::mCursor;
  using CursorOpBase::mResponse;

  // Only created by Cursor.
  OpenOp(Cursor* const aCursor,
         const Maybe<SerializedKeyRange>& aOptionalKeyRange)
      : CursorOpBase(aCursor), mOptionalKeyRange(aOptionalKeyRange) {}

  // Reference counted.
  ~OpenOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;
};

template <IDBCursorType CursorType>
class Cursor<CursorType>::ContinueOp final
    : public Cursor<CursorType>::CursorOpBase {
  friend class Cursor<CursorType>;

  using CursorOpBase::mCursor;
  using CursorOpBase::mResponse;
  const CursorRequestParams mParams;

  // Only created by Cursor.
  ContinueOp(Cursor* const aCursor, CursorRequestParams aParams,
             CursorPosition<CursorType> aPosition)
      : CursorOpBase(aCursor),
        mParams(std::move(aParams)),
        mCurrentPosition{std::move(aPosition)} {
    MOZ_ASSERT(mParams.type() != CursorRequestParams::T__None);
  }

  // Reference counted.
  ~ContinueOp() override = default;

  nsresult DoDatabaseWork(DatabaseConnection* aConnection) override;

  const CursorPosition<CursorType> mCurrentPosition;
};

class Utils final : public PBackgroundIndexedDBUtilsParent {
#ifdef DEBUG
  bool mActorDestroyed;
#endif

 public:
  Utils();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::Utils)

 private:
  // Reference counted.
  ~Utils() override;

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvGetFileReferences(
      const PersistenceType& aPersistenceType, const nsCString& aOrigin,
      const nsString& aDatabaseName, const int64_t& aFileId, int32_t* aRefCnt,
      int32_t* aDBRefCnt, bool* aResult) override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

struct DatabaseActorInfo final {
  friend class mozilla::DefaultDelete<DatabaseActorInfo>;

  SafeRefPtr<FullDatabaseMetadata> mMetadata;
  nsTArray<NotNull<CheckedUnsafePtr<Database>>> mLiveDatabases;
  RefPtr<FactoryOp> mWaitingFactoryOp;

  DatabaseActorInfo(SafeRefPtr<FullDatabaseMetadata> aMetadata,
                    NotNull<Database*> aDatabase)
      : mMetadata(std::move(aMetadata)) {
    MOZ_COUNT_CTOR(DatabaseActorInfo);

    mLiveDatabases.AppendElement(aDatabase);
  }

 private:
  ~DatabaseActorInfo() {
    MOZ_ASSERT(mLiveDatabases.IsEmpty());
    MOZ_ASSERT(!mWaitingFactoryOp || !mWaitingFactoryOp->HasBlockedDatabases());

    MOZ_COUNT_DTOR(DatabaseActorInfo);
  }
};

class DatabaseLoggingInfo final {
#ifdef DEBUG
  // Just for potential warnings.
  friend class Factory;
#endif

  LoggingInfo mLoggingInfo;

 public:
  explicit DatabaseLoggingInfo(const LoggingInfo& aLoggingInfo)
      : mLoggingInfo(aLoggingInfo) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aLoggingInfo.nextTransactionSerialNumber());
    MOZ_ASSERT(aLoggingInfo.nextVersionChangeTransactionSerialNumber());
    MOZ_ASSERT(aLoggingInfo.nextRequestSerialNumber());
  }

  const nsID& Id() const {
    AssertIsOnBackgroundThread();

    return mLoggingInfo.backgroundChildLoggingId();
  }

  int64_t NextTransactionSN(IDBTransaction::Mode aMode) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo.nextTransactionSerialNumber() < INT64_MAX);
    MOZ_ASSERT(mLoggingInfo.nextVersionChangeTransactionSerialNumber() >
               INT64_MIN);

    if (aMode == IDBTransaction::Mode::VersionChange) {
      return mLoggingInfo.nextVersionChangeTransactionSerialNumber()--;
    }

    return mLoggingInfo.nextTransactionSerialNumber()++;
  }

  uint64_t NextRequestSN() {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo.nextRequestSerialNumber() < UINT64_MAX);

    return mLoggingInfo.nextRequestSerialNumber()++;
  }

  NS_INLINE_DECL_REFCOUNTING(DatabaseLoggingInfo)

 private:
  ~DatabaseLoggingInfo();
};

class QuotaClient final : public mozilla::dom::quota::Client {
  static QuotaClient* sInstance;

  nsCOMPtr<nsIEventTarget> mBackgroundThread;
  nsCOMPtr<nsITimer> mDeleteTimer;
  nsTArray<RefPtr<Maintenance>> mMaintenanceQueue;
  RefPtr<Maintenance> mCurrentMaintenance;
  RefPtr<nsThreadPool> mMaintenanceThreadPool;
  nsClassHashtable<nsRefPtrHashKey<FileManager>, nsTArray<int64_t>>
      mPendingDeleteInfos;
  FlippedOnce<false> mShutdownRequested;

 public:
  QuotaClient();

  static QuotaClient* GetInstance() {
    AssertIsOnBackgroundThread();

    return sInstance;
  }

  static bool IsShuttingDownOnBackgroundThread() {
    AssertIsOnBackgroundThread();

    if (sInstance) {
      return sInstance->IsShuttingDown();
    }

    return QuotaManager::IsShuttingDown();
  }

  static bool IsShuttingDownOnNonBackgroundThread() {
    MOZ_ASSERT(!IsOnBackgroundThread());

    return QuotaManager::IsShuttingDown();
  }

  nsIEventTarget* BackgroundThread() const {
    MOZ_ASSERT(mBackgroundThread);
    return mBackgroundThread;
  }

  bool IsShuttingDown() const {
    AssertIsOnBackgroundThread();

    return mShutdownRequested;
  }

  nsresult AsyncDeleteFile(FileManager* aFileManager, int64_t aFileId);

  nsresult FlushPendingFileDeletions();

  RefPtr<Maintenance> GetCurrentMaintenance() const {
    return mCurrentMaintenance;
  }

  void NoteFinishedMaintenance(Maintenance* aMaintenance) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aMaintenance);
    MOZ_ASSERT(mCurrentMaintenance == aMaintenance);

    mCurrentMaintenance = nullptr;

    QuotaManager::GetRef().MaybeRecordShutdownStep(quota::Client::IDB,
                                                   "Maintenance finished"_ns);

    ProcessMaintenanceQueue();
  }

  nsThreadPool* GetOrCreateThreadPool();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::QuotaClient,
                                        override)

  mozilla::dom::quota::Client::Type GetType() override;

  nsresult UpgradeStorageFrom1_0To2_0(nsIFile* aDirectory) override;

  nsresult UpgradeStorageFrom2_1To2_2(nsIFile* aDirectory) override;

  Result<UsageInfo, nsresult> InitOrigin(PersistenceType aPersistenceType,
                                         const OriginMetadata& aOriginMetadata,
                                         const AtomicBool& aCanceled) override;

  nsresult InitOriginWithoutTracking(PersistenceType aPersistenceType,
                                     const OriginMetadata& aOriginMetadata,
                                     const AtomicBool& aCanceled) override;

  Result<UsageInfo, nsresult> GetUsageForOrigin(
      PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) override;

  void OnOriginClearCompleted(PersistenceType aPersistenceType,
                              const nsACString& aOrigin) override;

  void ReleaseIOThreadObjects() override;

  void AbortOperationsForLocks(
      const DirectoryLockIdTable& aDirectoryLockIds) override;

  void AbortOperationsForProcess(ContentParentId aContentParentId) override;

  void AbortAllOperations() override;

  void StartIdleMaintenance() override;

  void StopIdleMaintenance() override;

 private:
  ~QuotaClient() override;

  void InitiateShutdown() override;
  bool IsShutdownCompleted() const override;
  nsCString GetShutdownStatus() const override;
  void ForceKillActors() override;
  void FinalizeShutdown() override;

  static void DeleteTimerCallback(nsITimer* aTimer, void* aClosure);

  Result<nsCOMPtr<nsIFile>, nsresult> GetDirectory(
      PersistenceType aPersistenceType, const nsACString& aOrigin);

  struct SubdirectoriesToProcessAndDatabaseFilenames {
    AutoTArray<nsString, 20> subdirsToProcess;
    nsTHashtable<nsStringHashKey> databaseFilenames{20};
  };

  struct SubdirectoriesToProcessAndDatabaseFilenamesAndObsoleteFilenames {
    AutoTArray<nsString, 20> subdirsToProcess;
    nsTHashtable<nsStringHashKey> databaseFilenames{20};
    nsTHashtable<nsStringHashKey> obsoleteFilenames{20};
  };

  enum class ObsoleteFilenamesHandling { Include, Omit };

  template <ObsoleteFilenamesHandling ObsoleteFilenames>
  using GetDatabaseFilenamesResult = std::conditional_t<
      ObsoleteFilenames == ObsoleteFilenamesHandling::Include,
      SubdirectoriesToProcessAndDatabaseFilenamesAndObsoleteFilenames,
      SubdirectoriesToProcessAndDatabaseFilenames>;

  // Returns a two-part or three-part structure:
  //
  // The first part is an array of subdirectories to process.
  //
  // The second part is a hashtable of database filenames.
  //
  // When ObsoleteFilenames is ObsoleteFilenamesHandling::Include, will also
  // collect files based on the marker files. For now,
  // GetUsageForOriginInternal() is the only consumer of this result because it
  // checks those unfinished deletion and clean them up after that.
  template <ObsoleteFilenamesHandling ObsoleteFilenames =
                ObsoleteFilenamesHandling::Omit>
  Result<GetDatabaseFilenamesResult<ObsoleteFilenames>, nsresult>
  GetDatabaseFilenames(nsIFile& aDirectory, const AtomicBool& aCanceled);

  nsresult GetUsageForOriginInternal(PersistenceType aPersistenceType,
                                     const OriginMetadata& aOriginMetadata,
                                     const AtomicBool& aCanceled,
                                     bool aInitializing, UsageInfo* aUsageInfo);

  // Runs on the PBackground thread. Checks to see if there's a queued
  // Maintenance to run.
  void ProcessMaintenanceQueue();
};

class DeleteFilesRunnable final : public Runnable,
                                  public OpenDirectoryListener {
  typedef mozilla::dom::quota::DirectoryLock DirectoryLock;

  enum State {
    // Just created on the PBackground thread. Next step is
    // State_DirectoryOpenPending.
    State_Initial,

    // Waiting for directory open allowed on the main thread. The next step is
    // State_DatabaseWorkOpen.
    State_DirectoryOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. The next step is
    // State_UnblockingOpen.
    State_DatabaseWorkOpen,

    // Notifying the QuotaManager that it can proceed to the next operation on
    // the main thread. Next step is State_Completed.
    State_UnblockingOpen,

    // All done.
    State_Completed
  };

  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  SafeRefPtr<FileManager> mFileManager;
  RefPtr<DirectoryLock> mDirectoryLock;
  nsTArray<int64_t> mFileIds;
  State mState;

 public:
  DeleteFilesRunnable(SafeRefPtr<FileManager> aFileManager,
                      nsTArray<int64_t>&& aFileIds);

  void RunImmediately();

 private:
  ~DeleteFilesRunnable() = default;

  void Open();

  void DoDatabaseWork();

  void Finish();

  void UnblockOpen();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  // OpenDirectoryListener overrides.
  virtual void DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void DirectoryLockFailed() override;
};

class Maintenance final : public Runnable, public OpenDirectoryListener {
  struct DirectoryInfo final {
    InitializedOnce<const FullOriginMetadata> mFullOriginMetadata;
    InitializedOnce<const nsTArray<nsString>> mDatabasePaths;
    const PersistenceType mPersistenceType;

    DirectoryInfo(PersistenceType aPersistenceType,
                  FullOriginMetadata aFullOriginMetadata,
                  nsTArray<nsString>&& aDatabasePaths);

    DirectoryInfo(const DirectoryInfo& aOther) = delete;
    DirectoryInfo(DirectoryInfo&& aOther) = delete;

    ~DirectoryInfo() { MOZ_COUNT_DTOR(Maintenance::DirectoryInfo); }
  };

  enum class State {
    // Newly created on the PBackground thread. Will proceed immediately or be
    // added to the maintenance queue. The next step is either
    // DirectoryOpenPending if IndexedDatabaseManager is running, or
    // CreateIndexedDatabaseManager if not.
    Initial = 0,

    // Create IndexedDatabaseManager on the main thread. The next step is either
    // Finishing if IndexedDatabaseManager initialization fails, or
    // IndexedDatabaseManagerOpen if initialization succeeds.
    CreateIndexedDatabaseManager,

    // Call OpenDirectory() on the PBackground thread. The next step is
    // DirectoryOpenPending.
    IndexedDatabaseManagerOpen,

    // Waiting for directory open allowed on the PBackground thread. The next
    // step is either Finishing if directory lock failed to acquire, or
    // DirectoryWorkOpen if directory lock is acquired.
    DirectoryOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. The next step is
    // BeginDatabaseMaintenance.
    DirectoryWorkOpen,

    // Dispatching a runnable for each database on the PBackground thread. The
    // next state is either WaitingForDatabaseMaintenancesToComplete if at least
    // one runnable has been dispatched, or Finishing otherwise.
    BeginDatabaseMaintenance,

    // Waiting for DatabaseMaintenance to finish on maintenance thread pool.
    // The next state is Finishing if the last runnable has finished.
    WaitingForDatabaseMaintenancesToComplete,

    // Waiting to finish/finishing on the PBackground thread. The next step is
    // Completed.
    Finishing,

    // All done.
    Complete
  };

  RefPtr<QuotaClient> mQuotaClient;
  PRTime mStartTime;
  RefPtr<DirectoryLock> mDirectoryLock;
  nsTArray<DirectoryInfo> mDirectoryInfos;
  nsTHashMap<nsStringHashKey, DatabaseMaintenance*> mDatabaseMaintenances;
  nsresult mResultCode;
  Atomic<bool> mAborted;
  State mState;

 public:
  explicit Maintenance(QuotaClient* aQuotaClient)
      : Runnable("dom::indexedDB::Maintenance"),
        mQuotaClient(aQuotaClient),
        mStartTime(PR_Now()),
        mResultCode(NS_OK),
        mAborted(false),
        mState(State::Initial) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aQuotaClient);
    MOZ_ASSERT(QuotaClient::GetInstance() == aQuotaClient);
    MOZ_ASSERT(mStartTime);
  }

  nsIEventTarget* BackgroundThread() const {
    MOZ_ASSERT(mQuotaClient);
    return mQuotaClient->BackgroundThread();
  }

  PRTime StartTime() const { return mStartTime; }

  bool IsAborted() const { return mAborted; }

  void RunImmediately() {
    MOZ_ASSERT(mState == State::Initial);

    Unused << this->Run();
  }

  void Abort() {
    AssertIsOnBackgroundThread();

    mAborted = true;
  }

  void RegisterDatabaseMaintenance(DatabaseMaintenance* aDatabaseMaintenance);

  void UnregisterDatabaseMaintenance(DatabaseMaintenance* aDatabaseMaintenance);

  RefPtr<DatabaseMaintenance> GetDatabaseMaintenance(
      const nsAString& aDatabasePath) const {
    AssertIsOnBackgroundThread();

    return mDatabaseMaintenances.Get(aDatabasePath);
  }

  void Stringify(nsACString& aResult) const;

 private:
  ~Maintenance() override {
    MOZ_ASSERT(mState == State::Complete);
    MOZ_ASSERT(!mDatabaseMaintenances.Count());
  }

  // Runs on the PBackground thread. Checks if IndexedDatabaseManager is
  // running. Calls OpenDirectory() or dispatches to the main thread on which
  // CreateIndexedDatabaseManager() is called.
  nsresult Start();

  // Runs on the main thread. Once IndexedDatabaseManager is created it will
  // dispatch to the PBackground thread on which OpenDirectory() is called.
  nsresult CreateIndexedDatabaseManager();

  // Runs on the PBackground thread. Once QuotaManager has given a lock it will
  // call DirectoryOpen().
  nsresult OpenDirectory();

  // Runs on the PBackground thread. Dispatches to the QuotaManager I/O thread.
  nsresult DirectoryOpen();

  // Runs on the QuotaManager I/O thread. Once it finds databases it will
  // dispatch to the PBackground thread on which BeginDatabaseMaintenance()
  // is called.
  nsresult DirectoryWork();

  // Runs on the PBackground thread. It dispatches a runnable for each database.
  nsresult BeginDatabaseMaintenance();

  // Runs on the PBackground thread. Called when the maintenance is finished or
  // if any of above methods fails.
  void Finish();

  // We need to declare refcounting unconditionally, because
  // OpenDirectoryListener has pure-virtual refcounting.
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIRUNNABLE

  // OpenDirectoryListener overrides.
  void DirectoryLockAcquired(DirectoryLock* aLock) override;

  void DirectoryLockFailed() override;
};

Maintenance::DirectoryInfo::DirectoryInfo(
    PersistenceType aPersistenceType, FullOriginMetadata aFullOriginMetadata,
    nsTArray<nsString>&& aDatabasePaths)
    : mFullOriginMetadata(std::move(aFullOriginMetadata)),
      mDatabasePaths(std::move(aDatabasePaths)),
      mPersistenceType(aPersistenceType) {
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT(!mFullOriginMetadata->mGroup.IsEmpty());
  MOZ_ASSERT(!mFullOriginMetadata->mOrigin.IsEmpty());
#ifdef DEBUG
  MOZ_ASSERT(!mDatabasePaths->IsEmpty());
  for (const nsString& databasePath : *mDatabasePaths) {
    MOZ_ASSERT(!databasePath.IsEmpty());
  }
#endif

  MOZ_COUNT_CTOR(Maintenance::DirectoryInfo);
}

class DatabaseMaintenance final : public Runnable {
  // The minimum amount of time that has passed since the last vacuum before we
  // will attempt to analyze the database for fragmentation.
  static const PRTime kMinVacuumAge =
      PRTime(PR_USEC_PER_SEC) * 60 * 60 * 24 * 7;

  // If the percent of database pages that are not in contiguous order is higher
  // than this percentage we will attempt a vacuum.
  static const int32_t kPercentUnorderedThreshold = 30;

  // If the percent of file size growth since the last vacuum is higher than
  // this percentage we will attempt a vacuum.
  static const int32_t kPercentFileSizeGrowthThreshold = 10;

  // The number of freelist pages beyond which we will favor an incremental
  // vacuum over a full vacuum.
  static const int32_t kMaxFreelistThreshold = 5;

  // If the percent of unused file bytes in the database exceeds this percentage
  // then we will attempt a full vacuum.
  static const int32_t kPercentUnusedThreshold = 20;

  class AutoProgressHandler;

  enum class MaintenanceAction { Nothing = 0, IncrementalVacuum, FullVacuum };

  RefPtr<Maintenance> mMaintenance;
  RefPtr<DirectoryLock> mDirectoryLock;
  const OriginMetadata mOriginMetadata;
  const nsString mDatabasePath;
  int64_t mDirectoryLockId;
  nsCOMPtr<nsIRunnable> mCompleteCallback;
  const PersistenceType mPersistenceType;
  const Maybe<CipherKey> mMaybeKey;

 public:
  DatabaseMaintenance(Maintenance* aMaintenance, DirectoryLock* aDirectoryLock,
                      PersistenceType aPersistenceType,
                      const OriginMetadata& aOriginMetadata,
                      const nsString& aDatabasePath,
                      const Maybe<CipherKey>& aMaybeKey)
      : Runnable("dom::indexedDB::DatabaseMaintenance"),
        mMaintenance(aMaintenance),
        mDirectoryLock(aDirectoryLock),
        mOriginMetadata(aOriginMetadata),
        mDatabasePath(aDatabasePath),
        mPersistenceType(aPersistenceType),
        mMaybeKey{aMaybeKey} {
    MOZ_ASSERT(aDirectoryLock);

    MOZ_ASSERT(mDirectoryLock->Id() >= 0);
    mDirectoryLockId = mDirectoryLock->Id();
  }

  const nsString& DatabasePath() const { return mDatabasePath; }

  void WaitForCompletion(nsIRunnable* aCallback) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!mCompleteCallback);

    mCompleteCallback = aCallback;
  }

  void Stringify(nsACString& aResult) const;

 private:
  ~DatabaseMaintenance() override = default;

  // Runs on maintenance thread pool. Does maintenance on the database.
  void PerformMaintenanceOnDatabase();

  // Runs on maintenance thread pool as part of PerformMaintenanceOnDatabase.
  nsresult CheckIntegrity(mozIStorageConnection& aConnection, bool* aOk);

  // Runs on maintenance thread pool as part of PerformMaintenanceOnDatabase.
  nsresult DetermineMaintenanceAction(mozIStorageConnection& aConnection,
                                      nsIFile* aDatabaseFile,
                                      MaintenanceAction* aMaintenanceAction);

  // Runs on maintenance thread pool as part of PerformMaintenanceOnDatabase.
  void IncrementalVacuum(mozIStorageConnection& aConnection);

  // Runs on maintenance thread pool as part of PerformMaintenanceOnDatabase.
  void FullVacuum(mozIStorageConnection& aConnection, nsIFile* aDatabaseFile);

  // Runs on the PBackground thread. It dispatches a complete callback and
  // unregisters from Maintenance.
  void RunOnOwningThread();

  // Runs on maintenance thread pool. Once it performs database maintenance
  // it will dispatch to the PBackground thread on which RunOnOwningThread()
  // is called.
  void RunOnConnectionThread();

  NS_DECL_NSIRUNNABLE
};

class MOZ_STACK_CLASS DatabaseMaintenance::AutoProgressHandler final
    : public mozIStorageProgressHandler {
  Maintenance* mMaintenance;
  LazyInitializedOnce<const NotNull<mozIStorageConnection*>> mConnection;

  NS_DECL_OWNINGTHREAD

#ifdef DEBUG
  // This class is stack-based so we never actually allow AddRef/Release to do
  // anything. But we need to know if any consumer *thinks* that they have a
  // reference to this object so we track the reference countin DEBUG builds.
  nsrefcnt mDEBUGRefCnt;
#endif

 public:
  explicit AutoProgressHandler(Maintenance* aMaintenance)
      : mMaintenance(aMaintenance),
        mConnection()
#ifdef DEBUG
        ,
        mDEBUGRefCnt(0)
#endif
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsOnBackgroundThread());
    NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);
    MOZ_ASSERT(aMaintenance);
  }

  ~AutoProgressHandler() {
    NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);

    if (mConnection) {
      Unregister();
    }

    MOZ_ASSERT(!mDEBUGRefCnt);
  }

  nsresult Register(NotNull<mozIStorageConnection*> aConnection);

  // We don't want the mRefCnt member but this class does not "inherit"
  // nsISupports.
  NS_DECL_ISUPPORTS_INHERITED

 private:
  void Unregister();

  NS_DECL_MOZISTORAGEPROGRESSHANDLER

  // Not available for the heap!
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
  void operator delete(void*) = delete;
  void operator delete[](void*) = delete;
};

#ifdef DEBUG

class DEBUGThreadSlower final : public nsIThreadObserver {
 public:
  DEBUGThreadSlower() {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(kDEBUGThreadSleepMS);
  }

  NS_DECL_ISUPPORTS

 private:
  ~DEBUGThreadSlower() { AssertIsOnBackgroundThread(); }

  NS_DECL_NSITHREADOBSERVER
};

#endif  // DEBUG

/*******************************************************************************
 * Helper classes
 ******************************************************************************/

// XXX Get rid of FileHelper and move the functions into FileManager.
// Then, FileManager::Get(Journal)Directory and FileManager::GetFileForId might
// eventually be made private.
class MOZ_STACK_CLASS FileHelper final {
  const SafeRefPtr<FileManager> mFileManager;

  LazyInitializedOnce<const NotNull<nsCOMPtr<nsIFile>>> mFileDirectory;
  LazyInitializedOnce<const NotNull<nsCOMPtr<nsIFile>>> mJournalDirectory;

  class ReadCallback;
  LazyInitializedOnce<const NotNull<RefPtr<ReadCallback>>> mReadCallback;

 public:
  explicit FileHelper(SafeRefPtr<FileManager>&& aFileManager)
      : mFileManager(std::move(aFileManager)) {
    MOZ_ASSERT(mFileManager);
  }

  nsresult Init();

  [[nodiscard]] nsCOMPtr<nsIFile> GetFile(const FileInfo& aFileInfo);

  [[nodiscard]] nsCOMPtr<nsIFile> GetJournalFile(const FileInfo& aFileInfo);

  nsresult CreateFileFromStream(nsIFile& aFile, nsIFile& aJournalFile,
                                nsIInputStream& aInputStream, bool aCompress,
                                const Maybe<CipherKey>& aMaybeKey);

 private:
  nsresult SyncCopy(nsIInputStream& aInputStream,
                    nsIOutputStream& aOutputStream, char* aBuffer,
                    uint32_t aBufferSize);

  nsresult SyncRead(nsIInputStream& aInputStream, char* aBuffer,
                    uint32_t aBufferSize, uint32_t* aRead);
};

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

bool GetFilenameBase(const nsAString& aFilename, const nsAString& aSuffix,
                     nsDependentSubstring& aFilenameBase) {
  MOZ_ASSERT(!aFilename.IsEmpty());
  MOZ_ASSERT(aFilenameBase.IsEmpty());

  if (!StringEndsWith(aFilename, aSuffix) ||
      aFilename.Length() == aSuffix.Length()) {
    return false;
  }

  MOZ_ASSERT(aFilename.Length() > aSuffix.Length());

  aFilenameBase.Rebind(aFilename, 0, aFilename.Length() - aSuffix.Length());
  return true;
}

class EncryptedFileBlobImpl final : public FileBlobImpl {
 public:
  EncryptedFileBlobImpl(const nsCOMPtr<nsIFile>& aNativeFile,
                        const FileInfo::IdType aId, const CipherKey& aKey)
      : FileBlobImpl{aNativeFile}, mKey{aKey} {
    SetFileId(aId);
  }

  uint64_t GetSize(ErrorResult& aRv) override {
    nsCOMPtr<nsIInputStream> inputStream;
    CreateInputStream(getter_AddRefs(inputStream), aRv);

    if (aRv.Failed()) {
      return 0;
    }

    MOZ_ASSERT(inputStream);

    IDB_TRY_RETURN(MOZ_TO_RESULT_INVOKE(inputStream, Available), 0,
                   [&aRv](const nsresult rv) { aRv = rv; });
  }

  void CreateInputStream(nsIInputStream** aInputStream,
                         ErrorResult& aRv) override {
    nsCOMPtr<nsIInputStream> baseInputStream;
    FileBlobImpl::CreateInputStream(getter_AddRefs(baseInputStream), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    *aInputStream =
        MakeAndAddRef<DecryptingInputStream<IndexedDBCipherStrategy>>(
            WrapNotNull(std::move(baseInputStream)), kEncryptedStreamBlockSize,
            mKey)
            .take();
  }

  void GetBlobImplType(nsAString& aBlobImplType) const override {
    aBlobImplType = u"EncryptedFileBlobImpl"_ns;
  }

  already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) override {
    MOZ_CRASH("Not implemented because this should be unreachable.");
  }

 private:
  const CipherKey& mKey;
};

RefPtr<BlobImpl> CreateFileBlobImpl(const Database& aDatabase,
                                    const nsCOMPtr<nsIFile>& aNativeFile,
                                    const FileInfo::IdType aId) {
  const auto& maybeKey = aDatabase.MaybeKeyRef();
  if (maybeKey) {
    return MakeRefPtr<EncryptedFileBlobImpl>(aNativeFile, aId, *maybeKey);
  }

  auto impl = MakeRefPtr<FileBlobImpl>(aNativeFile);
  impl->SetFileId(aId);
  return impl;
}

Result<nsTArray<SerializedStructuredCloneFile>, nsresult>
SerializeStructuredCloneFiles(PBackgroundParent* aBackgroundActor,
                              const SafeRefPtr<Database>& aDatabase,
                              const nsTArray<StructuredCloneFileParent>& aFiles,
                              bool aForPreprocess) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(aDatabase);

  if (aFiles.IsEmpty()) {
    return nsTArray<SerializedStructuredCloneFile>{};
  }

  const nsCOMPtr<nsIFile> directory =
      aDatabase->GetFileManager().GetCheckedDirectory();
  IDB_TRY(OkIf(directory), Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
          IDB_REPORT_INTERNAL_ERR_LAMBDA);

  nsTArray<SerializedStructuredCloneFile> serializedStructuredCloneFiles;
  IDB_TRY(OkIf(serializedStructuredCloneFiles.SetCapacity(aFiles.Length(),
                                                          fallible)),
          Err(NS_ERROR_OUT_OF_MEMORY));

  IDB_TRY(TransformIfAbortOnErr(
      aFiles, MakeBackInserter(serializedStructuredCloneFiles),
      [aForPreprocess](const auto& file) {
        return !aForPreprocess ||
               file.Type() == StructuredCloneFileBase::eStructuredClone;
      },
      [&directory, &aDatabase, aBackgroundActor, aForPreprocess](
          const auto& file) -> Result<SerializedStructuredCloneFile, nsresult> {
        const int64_t fileId = file.FileInfo().Id();
        MOZ_ASSERT(fileId > 0);

        const nsCOMPtr<nsIFile> nativeFile =
            mozilla::dom::indexedDB::FileManager::GetCheckedFileForId(directory,
                                                                      fileId);
        IDB_TRY(OkIf(nativeFile), Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
                IDB_REPORT_INTERNAL_ERR_LAMBDA);

        switch (file.Type()) {
          case StructuredCloneFileBase::eStructuredClone:
            if (!aForPreprocess) {
              return SerializedStructuredCloneFile{
                  null_t(), StructuredCloneFileBase::eStructuredClone};
            }

            [[fallthrough]];

          case StructuredCloneFileBase::eBlob: {
            const auto impl = CreateFileBlobImpl(*aDatabase, nativeFile,
                                                 file.FileInfo().Id());

            IPCBlob ipcBlob;

            // This can only fail if the child has crashed.
            IDB_TRY(IPCBlobUtils::Serialize(impl, aBackgroundActor, ipcBlob),
                    Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
                    IDB_REPORT_INTERNAL_ERR_LAMBDA);

            aDatabase->MapBlob(ipcBlob, file.FileInfoPtr());

            return SerializedStructuredCloneFile{ipcBlob, file.Type()};
          }

          case StructuredCloneFileBase::eMutableFile: {
            if (aDatabase->IsFileHandleDisabled()) {
              return SerializedStructuredCloneFile{
                  null_t(), StructuredCloneFileBase::eMutableFile};
            }

            const RefPtr<MutableFile> actor = MutableFile::Create(
                nativeFile, aDatabase.clonePtr(), file.FileInfoPtr());
            IDB_TRY(OkIf(actor), Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
                    IDB_REPORT_INTERNAL_ERR_LAMBDA);

            // Transfer ownership to IPDL.
            actor->SetActorAlive();

            if (!aDatabase->SendPBackgroundMutableFileConstructor(actor, u""_ns,
                                                                  u""_ns)) {
              // This can only fail if the child has crashed.
              IDB_REPORT_INTERNAL_ERR();
              return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
            }

            return SerializedStructuredCloneFile{
                actor.get(), StructuredCloneFileBase::eMutableFile};
          }

          case StructuredCloneFileBase::eWasmBytecode:
          case StructuredCloneFileBase::eWasmCompiled: {
            // Set file() to null, support for storing WebAssembly.Modules has
            // been removed in bug 1469395. Support for de-serialization of
            // WebAssembly.Modules modules has been removed in bug 1561876. Full
            // removal is tracked in bug 1487479.

            return SerializedStructuredCloneFile{null_t(), file.Type()};
          }

          default:
            MOZ_CRASH("Should never get here!");
        }
      }));

  return std::move(serializedStructuredCloneFiles);
}

enum struct Idempotency { Yes, No };

template <typename R>
Result<Maybe<R>, nsresult> IdempotentFilter(const nsresult aRv) {
  if (aRv == NS_ERROR_FILE_NOT_FOUND ||
      aRv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    return Maybe<R>{};
  }

  return Err(aRv);
}

template <typename R>
auto MakeMaybeIdempotentFilter(const Idempotency aIdempotent)
    -> Result<Maybe<R>, nsresult> (*)(nsresult) {
  if (aIdempotent == Idempotency::Yes) {
    return IdempotentFilter<R>;
  }

  return [](const nsresult rv) { return Result<Maybe<R>, nsresult>{Err(rv)}; };
}

// Delete a file, decreasing the quota usage as appropriate. If the file no
// longer exists but aIdempotent is true, success is returned, although quota
// usage can't be decreased. (With the assumption being that the file was
// already deleted prior to this logic running, and the non-existent file was no
// longer tracked by quota because it didn't exist at initialization time or a
// previous deletion call updated the usage.)
nsresult DeleteFile(nsIFile& aFile, QuotaManager* const aQuotaManager,
                    const PersistenceType aPersistenceType,
                    const OriginMetadata& aOriginMetadata,
                    const Idempotency aIdempotent) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  IDB_TRY_INSPECT(
      const auto& fileSize,
      ([aQuotaManager, &aFile,
        aIdempotent]() -> Result<Maybe<int64_t>, nsresult> {
        if (aQuotaManager) {
          IDB_TRY_INSPECT(
              const Maybe<int64_t>& fileSize,
              MOZ_TO_RESULT_INVOKE(aFile, GetFileSize)
                  .map([](const int64_t val) { return Some(val); })
                  .orElse(MakeMaybeIdempotentFilter<int64_t>(aIdempotent)));

          // XXX Can we really assert that the file size is not 0 if
          // it existed? This might be violated by external
          // influences.
          MOZ_ASSERT(!fileSize || fileSize.value() >= 0);

          return fileSize;
        }

        return Some(int64_t(0));
      }()));

  if (!fileSize) {
    return NS_OK;
  }

  IDB_TRY_INSPECT(const auto& didExist,
                  ToResult(aFile.Remove(false))
                      .map(Some<Ok>)
                      .orElse(MakeMaybeIdempotentFilter<Ok>(aIdempotent)));

  if (!didExist) {
    // XXX If we get here, this means that the file still existed when we
    // queried its size, but no longer when we tried to remove it. Not sure if
    // this should really be silently accepted in idempotent mode.
    return NS_OK;
  }

  if (fileSize.value() > 0) {
    MOZ_ASSERT(aQuotaManager);

    aQuotaManager->DecreaseUsageForOrigin(aPersistenceType, aOriginMetadata,
                                          Client::IDB, fileSize.value());
  }

  return NS_OK;
}

nsresult DeleteFile(nsIFile& aDirectory, const nsAString& aFilename,
                    QuotaManager* const aQuotaManager,
                    const PersistenceType aPersistenceType,
                    const OriginMetadata& aOriginMetadata,
                    const Idempotency aIdempotent) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aFilename.IsEmpty());

  IDB_TRY_INSPECT(const auto& file, CloneFileAndAppend(aDirectory, aFilename));

  return DeleteFile(*file, aQuotaManager, aPersistenceType, aOriginMetadata,
                    aIdempotent);
}

nsresult DeleteFilesNoQuota(nsIFile* aDirectory, const nsAString& aFilename) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(!aFilename.IsEmpty());

  // The current using function hasn't initialized the origin, so in here we
  // don't update the size of origin. Adding this assertion for preventing from
  // misusing.
  DebugOnly<QuotaManager*> quotaManager = QuotaManager::Get();
  MOZ_ASSERT(!quotaManager->IsTemporaryStorageInitialized());

  IDB_TRY_INSPECT(const auto& file, CloneFileAndAppend(*aDirectory, aFilename));

  IDB_TRY_INSPECT(
      const auto& didExist,
      ToResult(file->Remove(true)).map(Some<Ok>).orElse(IdempotentFilter<Ok>));

  Unused << didExist;

  return NS_OK;
}

// CreateMarkerFile and RemoveMarkerFile are a pair of functions to indicate
// whether having removed all the files successfully. The marker file should
// be checked before executing the next operation or initialization.
Result<nsCOMPtr<nsIFile>, nsresult> CreateMarkerFile(
    nsIFile& aBaseDirectory, const nsAString& aDatabaseNameBase) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aDatabaseNameBase.IsEmpty());

  IDB_TRY_INSPECT(
      const auto& markerFile,
      CloneFileAndAppend(aBaseDirectory,
                         kIdbDeletionMarkerFilePrefix + aDatabaseNameBase));

  IDB_TRY(
      MOZ_TO_RESULT_INVOKE(markerFile, Create, nsIFile::NORMAL_FILE_TYPE, 0644)
          .orElse(ErrToDefaultOkOrErr<NS_ERROR_FILE_ALREADY_EXISTS, Ok>));

  return markerFile;
}

nsresult RemoveMarkerFile(nsIFile* aMarkerFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aMarkerFile);

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(aMarkerFile->Exists(&exists)));
  MOZ_ASSERT(exists);

  IDB_TRY(aMarkerFile->Remove(false));

  return NS_OK;
}

Result<Ok, nsresult> DeleteFileManagerDirectory(
    nsIFile& aFileManagerDirectory, QuotaManager* aQuotaManager,
    const PersistenceType aPersistenceType,
    const OriginMetadata& aOriginMetadata) {
  if (!aQuotaManager) {
    IDB_TRY(aFileManagerDirectory.Remove(true));

    return Ok{};
  }

  IDB_TRY_UNWRAP(auto fileUsage, FileManager::GetUsage(&aFileManagerDirectory));

  uint64_t usageValue = fileUsage.GetValue().valueOr(0);

  auto res =
      MOZ_TO_RESULT_INVOKE(aFileManagerDirectory, Remove, true)
          .orElse([&usageValue, &aFileManagerDirectory](nsresult rv) {
            // We may have deleted some files, try to update quota
            // information before returning the error.

            // failures of GetUsage are intentionally ignored
            Unused << FileManager::GetUsage(&aFileManagerDirectory)
                          .andThen([&usageValue](const auto& newFileUsage) {
                            const auto newFileUsageValue =
                                newFileUsage.GetValue().valueOr(0);
                            MOZ_ASSERT(newFileUsageValue <= usageValue);
                            usageValue -= newFileUsageValue;

                            // XXX andThen does not support void return
                            // values right now, we must return a Result
                            return Result<Ok, nsresult>{Ok{}};
                          });

            return Result<Ok, nsresult>{Err(rv)};
          });

  if (usageValue) {
    aQuotaManager->DecreaseUsageForOrigin(aPersistenceType, aOriginMetadata,
                                          Client::IDB, usageValue);
  }

  return res;
}

// Idempotently delete all the parts of an IndexedDB database including its
// SQLite database file, its WAL journal, it's shared-memory file, and its
// Blob/Files sub-directory. A marker file is created prior to performing the
// deletion so that in the event we crash or fail to successfully delete the
// database and its files, we will re-attempt the deletion the next time the
// origin is initialized using this method. Because this means the method may be
// called on a partially deleted database, this method uses DeleteFile which
// succeeds when the file we ask it to delete does not actually exist. The
// marker file is removed once deletion has successfully completed.
nsresult RemoveDatabaseFilesAndDirectory(nsIFile& aBaseDirectory,
                                         const nsAString& aDatabaseFilenameBase,
                                         QuotaManager* aQuotaManager,
                                         const PersistenceType aPersistenceType,
                                         const OriginMetadata& aOriginMetadata,
                                         const nsAString& aDatabaseName) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aDatabaseFilenameBase.IsEmpty());

  AUTO_PROFILER_LABEL("RemoveDatabaseFilesAndDirectory", DOM);

  IDB_TRY_UNWRAP(auto markerFile,
                 CreateMarkerFile(aBaseDirectory, aDatabaseFilenameBase));

  // The database file counts towards quota.
  IDB_TRY(DeleteFile(aBaseDirectory, aDatabaseFilenameBase + kSQLiteSuffix,
                     aQuotaManager, aPersistenceType, aOriginMetadata,
                     Idempotency::Yes));

  // .sqlite-journal files don't count towards quota.
  IDB_TRY(DeleteFile(aBaseDirectory,
                     aDatabaseFilenameBase + kSQLiteJournalSuffix,
                     /* doesn't count */ nullptr, aPersistenceType,
                     aOriginMetadata, Idempotency::Yes));

  // .sqlite-shm files don't count towards quota.
  IDB_TRY(DeleteFile(aBaseDirectory, aDatabaseFilenameBase + kSQLiteSHMSuffix,
                     /* doesn't count */ nullptr, aPersistenceType,
                     aOriginMetadata, Idempotency::Yes));

  // .sqlite-wal files do count towards quota.
  IDB_TRY(DeleteFile(aBaseDirectory, aDatabaseFilenameBase + kSQLiteWALSuffix,
                     aQuotaManager, aPersistenceType, aOriginMetadata,
                     Idempotency::Yes));

  // The files directory counts towards quota.
  IDB_TRY_INSPECT(
      const auto& fmDirectory,
      CloneFileAndAppend(aBaseDirectory, aDatabaseFilenameBase +
                                             kFileManagerDirectoryNameSuffix));

  IDB_TRY_INSPECT(const bool& exists,
                  MOZ_TO_RESULT_INVOKE(fmDirectory, Exists));

  if (exists) {
    IDB_TRY_INSPECT(const bool& isDirectory,
                    MOZ_TO_RESULT_INVOKE(fmDirectory, IsDirectory));

    IDB_TRY(OkIf(isDirectory), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    IDB_TRY(DeleteFileManagerDirectory(*fmDirectory, aQuotaManager,
                                       aPersistenceType, aOriginMetadata));
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  MOZ_ASSERT_IF(aQuotaManager, mgr);

  if (mgr) {
    mgr->InvalidateFileManager(aPersistenceType, aOriginMetadata.mOrigin,
                               aDatabaseName);
  }

  IDB_TRY(RemoveMarkerFile(markerFile));

  return NS_OK;
}

/*******************************************************************************
 * Globals
 ******************************************************************************/

// Counts the number of "live" Factory, FactoryOp and Database instances.
uint64_t gBusyCount = 0;

typedef nsTArray<CheckedUnsafePtr<FactoryOp>> FactoryOpArray;

StaticAutoPtr<FactoryOpArray> gFactoryOps;

// Maps a database id to information about live database actors.
typedef nsClassHashtable<nsCStringHashKey, DatabaseActorInfo>
    DatabaseActorHashtable;

StaticAutoPtr<DatabaseActorHashtable> gLiveDatabaseHashtable;

using PrivateBrowsingInfoHashtable = nsTHashMap<nsCStringHashKey, CipherKey>;
// XXX Maybe we can avoid a mutex here by moving all accesses to the background
// thread.
StaticAutoPtr<DataMutex<PrivateBrowsingInfoHashtable>>
    gPrivateBrowsingInfoHashtable;

StaticRefPtr<ConnectionPool> gConnectionPool;

StaticRefPtr<FileHandleThreadPool> gFileHandleThreadPool;

typedef nsTHashMap<nsIDHashKey, DatabaseLoggingInfo*>
    DatabaseLoggingInfoHashtable;

StaticAutoPtr<DatabaseLoggingInfoHashtable> gLoggingInfoHashtable;

typedef nsTHashMap<nsUint32HashKey, uint32_t> TelemetryIdHashtable;

StaticAutoPtr<TelemetryIdHashtable> gTelemetryIdHashtable;

// Protects all reads and writes to gTelemetryIdHashtable.
StaticAutoPtr<Mutex> gTelemetryIdMutex;

#ifdef DEBUG

StaticRefPtr<DEBUGThreadSlower> gDEBUGThreadSlower;

#endif  // DEBUG

void IncreaseBusyCount() {
  AssertIsOnBackgroundThread();

  // If this is the first instance then we need to do some initialization.
  if (!gBusyCount) {
    MOZ_ASSERT(!gFactoryOps);
    gFactoryOps = new FactoryOpArray();

    MOZ_ASSERT(!gLiveDatabaseHashtable);
    gLiveDatabaseHashtable = new DatabaseActorHashtable();

    MOZ_ASSERT(!gPrivateBrowsingInfoHashtable);
    gPrivateBrowsingInfoHashtable = new DataMutex<PrivateBrowsingInfoHashtable>(
        "gPrivateBrowsingInfoHashtable");

    MOZ_ASSERT(!gLoggingInfoHashtable);
    gLoggingInfoHashtable = new DatabaseLoggingInfoHashtable();

#ifdef DEBUG
    if (kDEBUGThreadPriority != nsISupportsPriority::PRIORITY_NORMAL) {
      NS_WARNING(
          "PBackground thread debugging enabled, priority has been "
          "modified!");
      nsCOMPtr<nsISupportsPriority> thread =
          do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_SUCCEEDS(thread->SetPriority(kDEBUGThreadPriority));
    }

    if (kDEBUGThreadSleepMS) {
      NS_WARNING(
          "PBackground thread debugging enabled, sleeping after every "
          "event!");
      nsCOMPtr<nsIThreadInternal> thread =
          do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      gDEBUGThreadSlower = new DEBUGThreadSlower();

      MOZ_ALWAYS_SUCCEEDS(thread->AddObserver(gDEBUGThreadSlower));
    }
#endif  // DEBUG
  }

  gBusyCount++;
}

void DecreaseBusyCount() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(gBusyCount);

  // Clean up if there are no more instances.
  if (--gBusyCount == 0) {
    MOZ_ASSERT(gLoggingInfoHashtable);
    gLoggingInfoHashtable = nullptr;

    MOZ_ASSERT(gLiveDatabaseHashtable);
    MOZ_ASSERT(!gLiveDatabaseHashtable->Count());
    gLiveDatabaseHashtable = nullptr;

    MOZ_ASSERT(gPrivateBrowsingInfoHashtable);
    // XXX After we add the private browsing session end listener, we can assert
    // this.
    // MOZ_ASSERT(!gPrivateBrowsingInfoHashtable->Count());
    gPrivateBrowsingInfoHashtable = nullptr;

    MOZ_ASSERT(gFactoryOps);
    MOZ_ASSERT(gFactoryOps->IsEmpty());
    gFactoryOps = nullptr;

#ifdef DEBUG
    if (kDEBUGThreadPriority != nsISupportsPriority::PRIORITY_NORMAL) {
      nsCOMPtr<nsISupportsPriority> thread =
          do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_SUCCEEDS(
          thread->SetPriority(nsISupportsPriority::PRIORITY_NORMAL));
    }

    if (kDEBUGThreadSleepMS) {
      MOZ_ASSERT(gDEBUGThreadSlower);

      nsCOMPtr<nsIThreadInternal> thread =
          do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_SUCCEEDS(thread->RemoveObserver(gDEBUGThreadSlower));

      gDEBUGThreadSlower = nullptr;
    }
#endif  // DEBUG
  }
}

template <typename Condition>
void InvalidateLiveDatabasesMatching(const Condition& aCondition) {
  AssertIsOnBackgroundThread();

  if (!gLiveDatabaseHashtable) {
    return;
  }

  // Invalidating a Database will cause it to be removed from the
  // gLiveDatabaseHashtable entries' mLiveDatabases, and, if it was the last
  // element in mLiveDatabases, to remove the whole hashtable entry. Therefore,
  // we need to make a temporary list of the databases to invalidate to avoid
  // iterator invalidation.

  nsTArray<SafeRefPtr<Database>> databases;

  for (const auto& liveDatabasesEntry : *gLiveDatabaseHashtable) {
    for (const auto& database : liveDatabasesEntry.GetData()->mLiveDatabases) {
      if (aCondition(*database)) {
        databases.AppendElement(
            SafeRefPtr{database.get(), AcquireStrongRefFromRawPtr{}});
      }
    }
  }

  for (const auto& database : databases) {
    database->Invalidate();
  }
}

uint32_t TelemetryIdForFile(nsIFile* aFile) {
  // May be called on any thread!

  MOZ_ASSERT(aFile);
  MOZ_ASSERT(gTelemetryIdMutex);

  // The storage directory is structured like this:
  //
  //   <profile>/storage/<persistence>/<origin>/idb/<filename>.sqlite
  //
  // For the purposes of this function we're only concerned with the
  // <persistence>, <origin>, and <filename> pieces.

  nsString filename;
  MOZ_ALWAYS_SUCCEEDS(aFile->GetLeafName(filename));

  // Make sure we were given a database file.
  MOZ_ASSERT(StringEndsWith(filename, kSQLiteSuffix));

  filename.Truncate(filename.Length() - kSQLiteSuffix.Length());

  // Get the "idb" directory.
  nsCOMPtr<nsIFile> idbDirectory;
  MOZ_ALWAYS_SUCCEEDS(aFile->GetParent(getter_AddRefs(idbDirectory)));

  DebugOnly<nsString> idbLeafName;
  MOZ_ASSERT(NS_SUCCEEDED(idbDirectory->GetLeafName(idbLeafName)));
  MOZ_ASSERT(static_cast<nsString&>(idbLeafName).EqualsLiteral("idb"));

  // Get the <origin> directory.
  nsCOMPtr<nsIFile> originDirectory;
  MOZ_ALWAYS_SUCCEEDS(idbDirectory->GetParent(getter_AddRefs(originDirectory)));

  nsString origin;
  MOZ_ALWAYS_SUCCEEDS(originDirectory->GetLeafName(origin));

  // Any databases in these directories are owned by the application and should
  // not have their filenames masked. Hopefully they also appear in the
  // Telemetry.cpp whitelist.
  if (origin.EqualsLiteral("chrome") ||
      origin.EqualsLiteral("moz-safe-about+home")) {
    return 0;
  }

  // Get the <persistence> directory.
  nsCOMPtr<nsIFile> persistenceDirectory;
  MOZ_ALWAYS_SUCCEEDS(
      originDirectory->GetParent(getter_AddRefs(persistenceDirectory)));

  nsString persistence;
  MOZ_ALWAYS_SUCCEEDS(persistenceDirectory->GetLeafName(persistence));

  constexpr auto separator = u"*"_ns;

  uint32_t hashValue =
      HashString(persistence + separator + origin + separator + filename);

  MutexAutoLock lock(*gTelemetryIdMutex);

  if (!gTelemetryIdHashtable) {
    gTelemetryIdHashtable = new TelemetryIdHashtable();
  }

  return gTelemetryIdHashtable->LookupOrInsertWith(hashValue, [] {
    static uint32_t sNextId = 1;

    // We're locked, no need for atomics.
    return sNextId++;
  });
}

const CommonIndexOpenCursorParams& GetCommonIndexOpenCursorParams(
    const OpenCursorParams& aParams) {
  switch (aParams.type()) {
    case OpenCursorParams::TIndexOpenCursorParams:
      return aParams.get_IndexOpenCursorParams().commonIndexParams();
    case OpenCursorParams::TIndexOpenKeyCursorParams:
      return aParams.get_IndexOpenKeyCursorParams().commonIndexParams();
    default:
      MOZ_CRASH("Should never get here!");
  }
}

const CommonOpenCursorParams& GetCommonOpenCursorParams(
    const OpenCursorParams& aParams) {
  switch (aParams.type()) {
    case OpenCursorParams::TObjectStoreOpenCursorParams:
      return aParams.get_ObjectStoreOpenCursorParams().commonParams();
    case OpenCursorParams::TObjectStoreOpenKeyCursorParams:
      return aParams.get_ObjectStoreOpenKeyCursorParams().commonParams();
    case OpenCursorParams::TIndexOpenCursorParams:
    case OpenCursorParams::TIndexOpenKeyCursorParams:
      return GetCommonIndexOpenCursorParams(aParams).commonParams();
    default:
      MOZ_CRASH("Should never get here!");
  }
}

// TODO: Using nsCString as a return type here seems to lead to a dependency on
// some temporaries, which I did not expect. Is it a good idea that the default
// operator+ behaviour constructs such strings? It is certainly useful as an
// optimization, but this should be better done via an appropriately named
// function rather than an operator.
nsAutoCString MakeColumnPairSelectionList(
    const nsLiteralCString& aPlainColumnName,
    const nsLiteralCString& aLocaleAwareColumnName,
    const nsLiteralCString& aSortColumnAlias, const bool aIsLocaleAware) {
  return aPlainColumnName +
         (aIsLocaleAware ? EmptyCString() : " as "_ns + aSortColumnAlias) +
         ", "_ns + aLocaleAwareColumnName +
         (aIsLocaleAware ? " as "_ns + aSortColumnAlias : EmptyCString());
}

constexpr bool IsIncreasingOrder(const IDBCursorDirection aDirection) {
  MOZ_ASSERT(aDirection == IDBCursorDirection::Next ||
             aDirection == IDBCursorDirection::Nextunique ||
             aDirection == IDBCursorDirection::Prev ||
             aDirection == IDBCursorDirection::Prevunique);

  return aDirection == IDBCursorDirection::Next ||
         aDirection == IDBCursorDirection::Nextunique;
}

constexpr bool IsUnique(const IDBCursorDirection aDirection) {
  MOZ_ASSERT(aDirection == IDBCursorDirection::Next ||
             aDirection == IDBCursorDirection::Nextunique ||
             aDirection == IDBCursorDirection::Prev ||
             aDirection == IDBCursorDirection::Prevunique);

  return aDirection == IDBCursorDirection::Nextunique ||
         aDirection == IDBCursorDirection::Prevunique;
}

// TODO: In principle, this could be constexpr, if operator+(nsLiteralCString,
// nsLiteralCString) were constexpr and returned a literal type.
nsAutoCString MakeDirectionClause(const IDBCursorDirection aDirection) {
  return " ORDER BY "_ns + kColumnNameKey +
         (IsIncreasingOrder(aDirection) ? " ASC"_ns : " DESC"_ns);
}

enum struct ComparisonOperator {
  LessThan,
  LessOrEquals,
  Equals,
  GreaterThan,
  GreaterOrEquals,
};

constexpr nsLiteralCString GetComparisonOperatorString(
    const ComparisonOperator aComparisonOperator) {
  switch (aComparisonOperator) {
    case ComparisonOperator::LessThan:
      return "<"_ns;
    case ComparisonOperator::LessOrEquals:
      return "<="_ns;
    case ComparisonOperator::Equals:
      return "=="_ns;
    case ComparisonOperator::GreaterThan:
      return ">"_ns;
    case ComparisonOperator::GreaterOrEquals:
      return ">="_ns;
  }

  // TODO: This is just to silence the "control reaches end of non-void
  // function" warning. Cannot use MOZ_CRASH in a constexpr function,
  // unfortunately.
  return ""_ns;
}

nsAutoCString GetKeyClause(const nsCString& aColumnName,
                           const ComparisonOperator aComparisonOperator,
                           const nsLiteralCString& aStmtParamName) {
  return aColumnName + " "_ns +
         GetComparisonOperatorString(aComparisonOperator) + " :"_ns +
         aStmtParamName;
}

nsAutoCString GetSortKeyClause(const ComparisonOperator aComparisonOperator,
                               const nsLiteralCString& aStmtParamName) {
  return GetKeyClause(kColumnNameAliasSortKey, aComparisonOperator,
                      aStmtParamName);
}

template <IDBCursorType CursorType>
struct PopulateResponseHelper;

struct CommonPopulateResponseHelper {
  explicit CommonPopulateResponseHelper(
      const TransactionDatabaseOperationBase& aOp)
      : mOp{aOp} {}

  nsresult GetKeys(mozIStorageStatement* const aStmt,
                   Key* const aOptOutSortKey) {
    IDB_TRY(GetCommonKeys(aStmt));

    if (aOptOutSortKey) {
      *aOptOutSortKey = mPosition;
    }

    return NS_OK;
  }

  nsresult GetCommonKeys(mozIStorageStatement* const aStmt) {
    MOZ_ASSERT(mPosition.IsUnset());

    IDB_TRY(mPosition.SetFromStatement(aStmt, 0));

    IDB_LOG_MARK_PARENT_TRANSACTION_REQUEST(
        "PRELOAD: Populating response with key %s", "Populating%.0s",
        IDB_LOG_ID_STRING(mOp.BackgroundChildLoggingId()),
        mOp.TransactionLoggingSerialNumber(), mOp.LoggingSerialNumber(),
        mPosition.GetBuffer().get());

    return NS_OK;
  }

  template <typename Response>
  void FillKeys(Response& aResponse) {
    MOZ_ASSERT(!mPosition.IsUnset());
    aResponse.key() = std::move(mPosition);
  }

  template <typename Response>
  static size_t GetKeySize(const Response& aResponse) {
    return aResponse.key().GetBuffer().Length();
  }

 protected:
  const Key& GetPosition() const { return mPosition; }

 private:
  const TransactionDatabaseOperationBase& mOp;
  Key mPosition;
};

struct IndexPopulateResponseHelper : CommonPopulateResponseHelper {
  using CommonPopulateResponseHelper::CommonPopulateResponseHelper;

  nsresult GetKeys(mozIStorageStatement* const aStmt,
                   Key* const aOptOutSortKey) {
    MOZ_ASSERT(mLocaleAwarePosition.IsUnset());
    MOZ_ASSERT(mObjectStorePosition.IsUnset());

    IDB_TRY(CommonPopulateResponseHelper::GetCommonKeys(aStmt));

    IDB_TRY(mLocaleAwarePosition.SetFromStatement(aStmt, 1));

    IDB_TRY(mObjectStorePosition.SetFromStatement(aStmt, 2));

    if (aOptOutSortKey) {
      *aOptOutSortKey =
          mLocaleAwarePosition.IsUnset() ? GetPosition() : mLocaleAwarePosition;
    }

    return NS_OK;
  }

  template <typename Response>
  void FillKeys(Response& aResponse) {
    MOZ_ASSERT(!mLocaleAwarePosition.IsUnset());
    MOZ_ASSERT(!mObjectStorePosition.IsUnset());

    CommonPopulateResponseHelper::FillKeys(aResponse);
    aResponse.sortKey() = std::move(mLocaleAwarePosition);
    aResponse.objectKey() = std::move(mObjectStorePosition);
  }

  template <typename Response>
  static size_t GetKeySize(Response& aResponse) {
    return CommonPopulateResponseHelper::GetKeySize(aResponse) +
           aResponse.sortKey().GetBuffer().Length() +
           aResponse.objectKey().GetBuffer().Length();
  }

 private:
  Key mLocaleAwarePosition, mObjectStorePosition;
};

struct KeyPopulateResponseHelper {
  static constexpr nsresult MaybeGetCloneInfo(
      mozIStorageStatement* const /*aStmt*/, const CursorBase& /*aCursor*/) {
    return NS_OK;
  }

  template <typename Response>
  static constexpr void MaybeFillCloneInfo(Response& /*aResponse*/,
                                           FilesArray* const /*aFiles*/) {}

  template <typename Response>
  static constexpr size_t MaybeGetCloneInfoSize(const Response& /*aResponse*/) {
    return 0;
  }
};

template <bool StatementHasIndexKeyBindings>
struct ValuePopulateResponseHelper {
  nsresult MaybeGetCloneInfo(mozIStorageStatement* const aStmt,
                             const ValueCursorBase& aCursor) {
    constexpr auto offset = StatementHasIndexKeyBindings ? 2 : 0;

    IDB_TRY_UNWRAP(auto cloneInfo,
                   GetStructuredCloneReadInfoFromStatement(
                       aStmt, 2 + offset, 1 + offset, *aCursor.mFileManager,
                       aCursor.mDatabase->MaybeKeyRef()));

    mCloneInfo.init(std::move(cloneInfo));

    if (mCloneInfo->HasPreprocessInfo()) {
      IDB_WARNING("Preprocessing for cursors not yet implemented!");
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
  }

  template <typename Response>
  void MaybeFillCloneInfo(Response& aResponse, FilesArray* const aFiles) {
    auto cloneInfo = mCloneInfo.release();
    aResponse.cloneInfo().data().data = cloneInfo.ReleaseData();
    aFiles->AppendElement(cloneInfo.ReleaseFiles());
  }

  template <typename Response>
  static size_t MaybeGetCloneInfoSize(const Response& aResponse) {
    return aResponse.cloneInfo().data().data.Size();
  }

 private:
  LazyInitializedOnceEarlyDestructible<const StructuredCloneReadInfoParent>
      mCloneInfo;
};

template <>
struct PopulateResponseHelper<IDBCursorType::ObjectStore>
    : ValuePopulateResponseHelper<false>, CommonPopulateResponseHelper {
  using CommonPopulateResponseHelper::CommonPopulateResponseHelper;

  static auto& GetTypedResponse(CursorResponse* const aResponse) {
    return aResponse->get_ArrayOfObjectStoreCursorResponse();
  }
};

template <>
struct PopulateResponseHelper<IDBCursorType::ObjectStoreKey>
    : KeyPopulateResponseHelper, CommonPopulateResponseHelper {
  using CommonPopulateResponseHelper::CommonPopulateResponseHelper;

  static auto& GetTypedResponse(CursorResponse* const aResponse) {
    return aResponse->get_ArrayOfObjectStoreKeyCursorResponse();
  }
};

template <>
struct PopulateResponseHelper<IDBCursorType::Index>
    : ValuePopulateResponseHelper<true>, IndexPopulateResponseHelper {
  using IndexPopulateResponseHelper::IndexPopulateResponseHelper;

  static auto& GetTypedResponse(CursorResponse* const aResponse) {
    return aResponse->get_ArrayOfIndexCursorResponse();
  }
};

template <>
struct PopulateResponseHelper<IDBCursorType::IndexKey>
    : KeyPopulateResponseHelper, IndexPopulateResponseHelper {
  using IndexPopulateResponseHelper::IndexPopulateResponseHelper;

  static auto& GetTypedResponse(CursorResponse* const aResponse) {
    return aResponse->get_ArrayOfIndexKeyCursorResponse();
  }
};

nsresult DispatchAndReturnFileReferences(
    PersistenceType aPersistenceType, const nsACString& aOrigin,
    const nsAString& aDatabaseName, const int64_t aFileId,
    int32_t* const aMemRefCnt, int32_t* const aDBRefCnt, bool* const aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aMemRefCnt);
  MOZ_ASSERT(aDBRefCnt);
  MOZ_ASSERT(aResult);

  *aResult = false;
  *aMemRefCnt = -1;
  *aDBRefCnt = -1;

  mozilla::Monitor monitor(__func__);
  bool waiting = true;

  auto lambda = [&] {
    AssertIsOnIOThread();

    {
      IndexedDatabaseManager* const mgr = IndexedDatabaseManager::Get();
      MOZ_ASSERT(mgr);

      const SafeRefPtr<FileManager> fileManager =
          mgr->GetFileManager(aPersistenceType, aOrigin, aDatabaseName);

      if (fileManager) {
        const SafeRefPtr<FileInfo> fileInfo = fileManager->GetFileInfo(aFileId);

        if (fileInfo) {
          fileInfo->GetReferences(aMemRefCnt, aDBRefCnt);

          if (*aMemRefCnt != -1) {
            // We added an extra temp ref, so account for that accordingly.
            (*aMemRefCnt)--;
          }

          *aResult = true;
        }
      }
    }

    mozilla::MonitorAutoLock lock(monitor);
    MOZ_ASSERT(waiting);

    waiting = false;
    lock.Notify();
  };

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // XXX can't we simply use NS_DISPATCH_SYNC instead of using a monitor?
  IDB_TRY(quotaManager->IOThread()->Dispatch(
      NS_NewRunnableFunction("GetFileReferences", std::move(lambda)),
      NS_DISPATCH_NORMAL));

  mozilla::MonitorAutoLock autolock(monitor);
  while (waiting) {
    autolock.Wait();
  }

  return NS_OK;
}

class DeserializeIndexValueHelper final : public Runnable {
 public:
  DeserializeIndexValueHelper(int64_t aIndexID, const KeyPath& aKeyPath,
                              bool aMultiEntry, const nsCString& aLocale,
                              StructuredCloneReadInfoParent& aCloneReadInfo,
                              nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
      : Runnable("DeserializeIndexValueHelper"),
        mMonitor("DeserializeIndexValueHelper::mMonitor"),
        mIndexID(aIndexID),
        mKeyPath(aKeyPath),
        mMultiEntry(aMultiEntry),
        mLocale(aLocale),
        mCloneReadInfo(aCloneReadInfo),
        mUpdateInfoArray(aUpdateInfoArray),
        mStatus(NS_ERROR_FAILURE) {}

  nsresult DispatchAndWait() {
    // FIXME(Bug 1637530) Re-enable optimization using a non-system-principaled
    // JS context
#if 0
    // We don't need to go to the main-thread and use the sandbox. Let's create
    // the updateInfo data here.
    if (!mCloneReadInfo.Data().Size()) {
      AutoJSAPI jsapi;
      jsapi.Init();

      JS::Rooted<JS::Value> value(jsapi.cx());
      value.setUndefined();

      ErrorResult rv;
      IDBObjectStore::AppendIndexUpdateInfo(mIndexID, mKeyPath, mMultiEntry,
                                            mLocale, jsapi.cx(), value,
                                            &mUpdateInfoArray, &rv);
      return rv.Failed() ? rv.StealNSResult() : NS_OK;
    }
#endif

    // The operation will continue on the main-thread.

    MOZ_ASSERT(!(mCloneReadInfo.Data().Size() % sizeof(uint64_t)));

    MonitorAutoLock lock(mMonitor);

    RefPtr<Runnable> self = this;
    IDB_TRY(SchedulerGroup::Dispatch(TaskCategory::Other, self.forget()));

    lock.Wait();
    return mStatus;
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* const cx = jsapi.cx();

    JS::Rooted<JSObject*> global(cx, GetSandbox(cx));

    IDB_TRY(OkIf(global), NS_OK,
            [this](const NotOk) { OperationCompleted(NS_ERROR_FAILURE); });

    const JSAutoRealm ar(cx, global);

    JS::Rooted<JS::Value> value(cx);
    IDB_TRY(DeserializeIndexValue(cx, &value), NS_OK,
            [this](const nsresult rv) { OperationCompleted(rv); });

    ErrorResult errorResult;
    IDBObjectStore::AppendIndexUpdateInfo(mIndexID, mKeyPath, mMultiEntry,
                                          mLocale, cx, value, &mUpdateInfoArray,
                                          &errorResult);
    IDB_TRY(OkIf(!errorResult.Failed()), NS_OK,
            ([this, &errorResult](const NotOk) {
              OperationCompleted(errorResult.StealNSResult());
            }));

    OperationCompleted(NS_OK);
    return NS_OK;
  }

 private:
  nsresult DeserializeIndexValue(JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aValue) {
    static const JSStructuredCloneCallbacks callbacks = {
        StructuredCloneReadCallback<StructuredCloneReadInfoParent>,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr};

    if (!JS_ReadStructuredClone(
            aCx, mCloneReadInfo.Data(), JS_STRUCTURED_CLONE_VERSION,
            JS::StructuredCloneScope::DifferentProcessForIndexedDB, aValue,
            JS::CloneDataPolicy(), &callbacks, &mCloneReadInfo)) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }

    return NS_OK;
  }

  void OperationCompleted(nsresult aStatus) {
    mStatus = aStatus;

    MonitorAutoLock lock(mMonitor);
    lock.Notify();
  }

  Monitor mMonitor;

  const int64_t mIndexID;
  const KeyPath& mKeyPath;
  const bool mMultiEntry;
  const nsCString mLocale;
  StructuredCloneReadInfoParent& mCloneReadInfo;
  nsTArray<IndexUpdateInfo>& mUpdateInfoArray;
  nsresult mStatus;
};

auto DeserializeIndexValueToUpdateInfos(
    int64_t aIndexID, const KeyPath& aKeyPath, bool aMultiEntry,
    const nsCString& aLocale, StructuredCloneReadInfoParent& aCloneReadInfo) {
  MOZ_ASSERT(!NS_IsMainThread());

  using ArrayType = AutoTArray<IndexUpdateInfo, 32>;
  using ResultType = Result<ArrayType, nsresult>;

  ArrayType updateInfoArray;
  const auto helper = MakeRefPtr<DeserializeIndexValueHelper>(
      aIndexID, aKeyPath, aMultiEntry, aLocale, aCloneReadInfo,
      updateInfoArray);
  const nsresult rv = helper->DispatchAndWait();
  return NS_FAILED(rv) ? Err(rv) : ResultType{std::move(updateInfoArray)};
}

bool IsSome(
    const Maybe<CachingDatabaseConnection::BorrowedStatement>& aMaybeStmt) {
  return aMaybeStmt.isSome();
}

}  // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

already_AddRefed<PBackgroundIDBFactoryParent> AllocPBackgroundIDBFactoryParent(
    const LoggingInfo& aLoggingInfo) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  if (NS_WARN_IF(!aLoggingInfo.nextTransactionSerialNumber()) ||
      NS_WARN_IF(!aLoggingInfo.nextVersionChangeTransactionSerialNumber()) ||
      NS_WARN_IF(!aLoggingInfo.nextRequestSerialNumber())) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  SafeRefPtr<Factory> actor = Factory::Create(aLoggingInfo);
  MOZ_ASSERT(actor);

  return actor.forget();
}

bool RecvPBackgroundIDBFactoryConstructor(
    PBackgroundIDBFactoryParent* aActor,
    const LoggingInfo& /* aLoggingInfo */) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  return true;
}

PBackgroundIndexedDBUtilsParent* AllocPBackgroundIndexedDBUtilsParent() {
  AssertIsOnBackgroundThread();

  RefPtr<Utils> actor = new Utils();

  return actor.forget().take();
}

bool DeallocPBackgroundIndexedDBUtilsParent(
    PBackgroundIndexedDBUtilsParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Utils> actor = dont_AddRef(static_cast<Utils*>(aActor));
  return true;
}

bool RecvFlushPendingFileDeletions() {
  AssertIsOnBackgroundThread();

  QuotaClient* quotaClient = QuotaClient::GetInstance();
  if (quotaClient) {
    if (NS_FAILED(quotaClient->FlushPendingFileDeletions())) {
      NS_WARNING("Failed to flush pending file deletions!");
    }
  }

  return true;
}

RefPtr<mozilla::dom::quota::Client> CreateQuotaClient() {
  AssertIsOnBackgroundThread();

  return MakeRefPtr<QuotaClient>();
}

FileHandleThreadPool* GetFileHandleThreadPool() {
  AssertIsOnBackgroundThread();

  if (!gFileHandleThreadPool) {
    RefPtr<FileHandleThreadPool> fileHandleThreadPool =
        FileHandleThreadPool::Create();
    if (NS_WARN_IF(!fileHandleThreadPool)) {
      return nullptr;
    }

    gFileHandleThreadPool = fileHandleThreadPool;
  }

  return gFileHandleThreadPool;
}

nsresult FileManager::AsyncDeleteFile(int64_t aFileId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mFileInfos.Contains(aFileId));

  QuotaClient* quotaClient = QuotaClient::GetInstance();
  if (quotaClient) {
    IDB_TRY(quotaClient->AsyncDeleteFile(this, aFileId));
  }

  return NS_OK;
}

/*******************************************************************************
 * DatabaseConnection implementation
 ******************************************************************************/

DatabaseConnection::DatabaseConnection(
    MovingNotNull<nsCOMPtr<mozIStorageConnection>> aStorageConnection,
    MovingNotNull<SafeRefPtr<FileManager>> aFileManager)
    : CachingDatabaseConnection(std::move(aStorageConnection)),
      mFileManager(std::move(aFileManager)),
      mInReadTransaction(false),
      mInWriteTransaction(false)
#ifdef DEBUG
      ,
      mDEBUGSavepointCount(0)
#endif
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mFileManager);
}

DatabaseConnection::~DatabaseConnection() {
  MOZ_ASSERT(!mFileManager);
  MOZ_ASSERT(!mUpdateRefcountFunction);
  MOZ_DIAGNOSTIC_ASSERT(!mInWriteTransaction);
  MOZ_ASSERT(!mDEBUGSavepointCount);
}

nsresult DatabaseConnection::Init() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  IDB_TRY(ExecuteCachedStatement("BEGIN;"_ns));

  mInReadTransaction = true;

  return NS_OK;
}

nsresult DatabaseConnection::BeginWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(HasStorageConnection());
  MOZ_ASSERT(mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::BeginWriteTransaction", DOM);

  // Release our read locks.
  IDB_TRY(ExecuteCachedStatement("ROLLBACK;"_ns));

  mInReadTransaction = false;

  if (!mUpdateRefcountFunction) {
    MOZ_ASSERT(mFileManager);

    RefPtr<UpdateRefcountFunction> function =
        new UpdateRefcountFunction(this, **mFileManager);

    IDB_TRY(MutableStorageConnection().CreateFunction("update_refcount"_ns,
                                                      /* aNumArguments */ 2,
                                                      function));

    mUpdateRefcountFunction = std::move(function);
  }

  // This one cannot obviously use ExecuteCachedStatement because of the custom
  // error handling for Execute only. If only Execute can produce
  // NS_ERROR_STORAGE_BUSY, we could actually use ExecuteCachedStatement and
  // simplify this.
  IDB_TRY_INSPECT(const auto& beginStmt,
                  BorrowCachedStatement("BEGIN IMMEDIATE;"_ns));

  IDB_TRY(ToResult(beginStmt->Execute()).orElse([&beginStmt](nsresult rv) {
    if (rv == NS_ERROR_STORAGE_BUSY) {
      NS_WARNING(
          "Received NS_ERROR_STORAGE_BUSY when attempting to start write "
          "transaction, retrying for up to 10 seconds");

      // Another thread must be using the database. Wait up to 10 seconds for
      // that to complete.
      const TimeStamp start = TimeStamp::NowLoRes();

      while (true) {
        PR_Sleep(PR_MillisecondsToInterval(100));

        rv = beginStmt->Execute();
        if (rv != NS_ERROR_STORAGE_BUSY ||
            TimeStamp::NowLoRes() - start > TimeDuration::FromSeconds(10)) {
          break;
        }
      }
    }

    return Result<Ok, nsresult>{rv};
  }));

  mInWriteTransaction = true;

  return NS_OK;
}

nsresult DatabaseConnection::CommitWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(HasStorageConnection());
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::CommitWriteTransaction", DOM);

  IDB_TRY(ExecuteCachedStatement("COMMIT;"_ns));

  mInWriteTransaction = false;
  return NS_OK;
}

void DatabaseConnection::RollbackWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_DIAGNOSTIC_ASSERT(HasStorageConnection());

  AUTO_PROFILER_LABEL("DatabaseConnection::RollbackWriteTransaction", DOM);

  if (!mInWriteTransaction) {
    return;
  }

  IDB_TRY_INSPECT(const auto& stmt, BorrowCachedStatement("ROLLBACK;"_ns),
                  QM_VOID);

  // This may fail if SQLite already rolled back the transaction so ignore any
  // errors.
  Unused << stmt->Execute();

  mInWriteTransaction = false;
}

void DatabaseConnection::FinishWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(HasStorageConnection());
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::FinishWriteTransaction", DOM);

  if (mUpdateRefcountFunction) {
    mUpdateRefcountFunction->Reset();
  }

  IDB_TRY(ExecuteCachedStatement("BEGIN;"_ns), QM_VOID);

  mInReadTransaction = true;
}

nsresult DatabaseConnection::StartSavepoint() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(HasStorageConnection());
  MOZ_ASSERT(mUpdateRefcountFunction);
  MOZ_ASSERT(mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::StartSavepoint", DOM);

  IDB_TRY(ExecuteCachedStatement(SAVEPOINT_CLAUSE));

  mUpdateRefcountFunction->StartSavepoint();

#ifdef DEBUG
  MOZ_ASSERT(mDEBUGSavepointCount < UINT32_MAX);
  mDEBUGSavepointCount++;
#endif

  return NS_OK;
}

nsresult DatabaseConnection::ReleaseSavepoint() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(HasStorageConnection());
  MOZ_ASSERT(mUpdateRefcountFunction);
  MOZ_ASSERT(mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::ReleaseSavepoint", DOM);

  IDB_TRY(ExecuteCachedStatement("RELEASE "_ns SAVEPOINT_CLAUSE));

  mUpdateRefcountFunction->ReleaseSavepoint();

#ifdef DEBUG
  MOZ_ASSERT(mDEBUGSavepointCount);
  mDEBUGSavepointCount--;
#endif

  return NS_OK;
}

nsresult DatabaseConnection::RollbackSavepoint() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(HasStorageConnection());
  MOZ_ASSERT(mUpdateRefcountFunction);
  MOZ_ASSERT(mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::RollbackSavepoint", DOM);

#ifdef DEBUG
  MOZ_ASSERT(mDEBUGSavepointCount);
  mDEBUGSavepointCount--;
#endif

  mUpdateRefcountFunction->RollbackSavepoint();

  IDB_TRY_INSPECT(const auto& stmt,
                  BorrowCachedStatement("ROLLBACK TO "_ns SAVEPOINT_CLAUSE));

  // This may fail if SQLite already rolled back the savepoint so ignore any
  // errors.
  Unused << stmt->Execute();

  return NS_OK;
}

nsresult DatabaseConnection::CheckpointInternal(CheckpointMode aMode) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::CheckpointInternal", DOM);

  nsAutoCString stmtString;
  stmtString.AssignLiteral("PRAGMA wal_checkpoint(");

  switch (aMode) {
    case CheckpointMode::Full:
      // Ensures that the database is completely checkpointed and flushed to
      // disk.
      stmtString.AppendLiteral("FULL");
      break;

    case CheckpointMode::Restart:
      // Like Full, but also ensures that the next write will start overwriting
      // the existing WAL file rather than letting the WAL file grow.
      stmtString.AppendLiteral("RESTART");
      break;

    case CheckpointMode::Truncate:
      // Like Restart but also truncates the existing WAL file.
      stmtString.AppendLiteral("TRUNCATE");
      break;

    default:
      MOZ_CRASH("Unknown CheckpointMode!");
  }

  stmtString.AppendLiteral(");");

  IDB_TRY(ExecuteCachedStatement(stmtString));

  return NS_OK;
}

void DatabaseConnection::DoIdleProcessing(bool aNeedsCheckpoint) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::DoIdleProcessing", DOM);

  CachingDatabaseConnection::CachedStatement freelistStmt;
  const uint32_t freelistCount = [this, &freelistStmt] {
    IDB_TRY_RETURN(GetFreelistCount(freelistStmt), 0u);
  }();

  CachedStatement rollbackStmt;
  CachedStatement beginStmt;
  if (aNeedsCheckpoint || freelistCount) {
    IDB_TRY_UNWRAP(rollbackStmt, GetCachedStatement("ROLLBACK;"_ns), QM_VOID);
    IDB_TRY_UNWRAP(beginStmt, GetCachedStatement("BEGIN;"_ns), QM_VOID);

    // Release the connection's normal transaction. It's possible that it could
    // fail, but that isn't a problem here.
    Unused << rollbackStmt.Borrow()->Execute();

    mInReadTransaction = false;
  }

  const bool freedSomePages = freelistCount && [this, &freelistStmt,
                                                &rollbackStmt, freelistCount,
                                                aNeedsCheckpoint] {
    // Warn in case of an error, but do not propagate it. Just indicate we
    // didn't free any pages.
    IDB_TRY_INSPECT(const bool& res,
                    ReclaimFreePagesWhileIdle(freelistStmt, rollbackStmt,
                                              freelistCount, aNeedsCheckpoint),
                    false);

    // Make sure we didn't leave a transaction running.
    MOZ_ASSERT(!mInReadTransaction);
    MOZ_ASSERT(!mInWriteTransaction);

    return res;
  }();

  // Truncate the WAL if we were asked to or if we managed to free some space.
  if (aNeedsCheckpoint || freedSomePages) {
    nsresult rv = CheckpointInternal(CheckpointMode::Truncate);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  // Finally try to restart the read transaction if we rolled it back earlier.
  if (beginStmt) {
    nsresult rv = beginStmt.Borrow()->Execute();
    if (NS_SUCCEEDED(rv)) {
      mInReadTransaction = true;
    } else {
      NS_WARNING("Failed to restart read transaction!");
    }
  }
}

Result<bool, nsresult> DatabaseConnection::ReclaimFreePagesWhileIdle(
    CachedStatement& aFreelistStatement, CachedStatement& aRollbackStatement,
    uint32_t aFreelistCount, bool aNeedsCheckpoint) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aFreelistStatement);
  MOZ_ASSERT(aRollbackStatement);
  MOZ_ASSERT(aFreelistCount);
  MOZ_ASSERT(!mInReadTransaction);
  MOZ_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::ReclaimFreePagesWhileIdle", DOM);

  // Make sure we don't keep working if anything else needs this thread.
  nsIThread* currentThread = NS_GetCurrentThread();
  MOZ_ASSERT(currentThread);

  if (NS_HasPendingEvents(currentThread)) {
    return false;
  }

  // Make all the statements we'll need up front.

  // Only try to free 10% at a time so that we can bail out if this connection
  // suddenly becomes active or if the thread is needed otherwise.
  IDB_TRY_INSPECT(
      const auto& incrementalVacuumStmt,
      GetCachedStatement(
          "PRAGMA incremental_vacuum("_ns +
          IntToCString(std::max(uint64_t(1), uint64_t(aFreelistCount / 10))) +
          ");"_ns));

  IDB_TRY_INSPECT(const auto& beginImmediateStmt,
                  GetCachedStatement("BEGIN IMMEDIATE;"_ns));

  IDB_TRY_INSPECT(const auto& commitStmt, GetCachedStatement("COMMIT;"_ns));

  if (aNeedsCheckpoint) {
    // Freeing pages is a journaled operation, so it will require additional WAL
    // space. However, we're idle and are about to checkpoint anyway, so doing a
    // RESTART checkpoint here should allow us to reuse any existing space.
    IDB_TRY(CheckpointInternal(CheckpointMode::Restart));
  }

  // Start the write transaction.
  IDB_TRY(beginImmediateStmt.Borrow()->Execute());

  mInWriteTransaction = true;

  bool freedSomePages = false, interrupted = false;

  const auto rollback = [&aRollbackStatement, this](const auto&) {
    MOZ_ASSERT(mInWriteTransaction);

    // Something failed, make sure we roll everything back.
    Unused << aRollbackStatement.Borrow()->Execute();

    // XXX Is rollback infallible? Shouldn't we check the result?

    mInWriteTransaction = false;
  };

  IDB_TRY(CollectWhile(
              [&aFreelistCount, &interrupted,
               currentThread]() -> Result<bool, nsresult> {
                if (NS_HasPendingEvents(currentThread)) {
                  // Abort if something else wants to use the thread, and
                  // roll back this transaction. It's ok if we never make
                  // progress here because the idle service should
                  // eventually reclaim this space.
                  interrupted = true;
                  return false;
                }
                return aFreelistCount != 0;
              },
              [&aFreelistStatement, &aFreelistCount, &incrementalVacuumStmt,
               &freedSomePages, this]() -> mozilla::Result<Ok, nsresult> {
                IDB_TRY(incrementalVacuumStmt.Borrow()->Execute());

                freedSomePages = true;

                IDB_TRY_UNWRAP(aFreelistCount,
                               GetFreelistCount(aFreelistStatement));

                return Ok{};
              })
              .andThen([&commitStmt, &freedSomePages, &interrupted, &rollback,
                        this](Ok) -> Result<Ok, nsresult> {
                if (interrupted) {
                  rollback(Ok{});
                  freedSomePages = false;
                }

                if (freedSomePages) {
                  // Commit the write transaction.
                  IDB_TRY(commitStmt.Borrow()->Execute(), QM_PROPAGATE,
                          [](const auto&) { NS_WARNING("Failed to commit!"); });

                  mInWriteTransaction = false;
                }

                return Ok{};
              }),
          QM_PROPAGATE, rollback);

  return freedSomePages;
}

Result<uint32_t, nsresult> DatabaseConnection::GetFreelistCount(
    CachedStatement& aCachedStatement) {
  AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("DatabaseConnection::GetFreelistCount", DOM);

  if (!aCachedStatement) {
    IDB_TRY_UNWRAP(aCachedStatement,
                   GetCachedStatement("PRAGMA freelist_count;"_ns));
  }

  const auto borrowedStatement = aCachedStatement.Borrow();

  IDB_TRY_UNWRAP(const DebugOnly<bool> hasResult,
                 MOZ_TO_RESULT_INVOKE(&*borrowedStatement, ExecuteStep));

  MOZ_ASSERT(hasResult);

  IDB_TRY_INSPECT(const int32_t& freelistCount,
                  MOZ_TO_RESULT_INVOKE(*borrowedStatement, GetInt32, 0));

  MOZ_ASSERT(freelistCount >= 0);

  return uint32_t(freelistCount);
}

void DatabaseConnection::Close() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!mDEBUGSavepointCount);
  MOZ_DIAGNOSTIC_ASSERT(!mInWriteTransaction);

  AUTO_PROFILER_LABEL("DatabaseConnection::Close", DOM);

  if (mUpdateRefcountFunction) {
    MOZ_ALWAYS_SUCCEEDS(
        MutableStorageConnection().RemoveFunction("update_refcount"_ns));
    mUpdateRefcountFunction = nullptr;
  }

  CachingDatabaseConnection::Close();

  mFileManager.destroy();
}

nsresult DatabaseConnection::DisableQuotaChecks() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(HasStorageConnection());

  if (!mQuotaObject) {
    MOZ_ASSERT(!mJournalQuotaObject);

    IDB_TRY(MutableStorageConnection().GetQuotaObjects(
        getter_AddRefs(mQuotaObject), getter_AddRefs(mJournalQuotaObject)));

    MOZ_ASSERT(mQuotaObject);
    MOZ_ASSERT(mJournalQuotaObject);
  }

  mQuotaObject->DisableQuotaCheck();
  mJournalQuotaObject->DisableQuotaCheck();

  return NS_OK;
}

void DatabaseConnection::EnableQuotaChecks() {
  AssertIsOnConnectionThread();
  if (!mQuotaObject) {
    MOZ_ASSERT(!mJournalQuotaObject);

    // DisableQuotaChecks failed earlier, so we don't need to enable quota
    // checks again.
    return;
  }

  MOZ_ASSERT(mJournalQuotaObject);

  const RefPtr<QuotaObject> quotaObject = std::move(mQuotaObject);
  const RefPtr<QuotaObject> journalQuotaObject = std::move(mJournalQuotaObject);

  quotaObject->EnableQuotaCheck();
  journalQuotaObject->EnableQuotaCheck();

  IDB_TRY_INSPECT(const int64_t& fileSize, GetFileSize(quotaObject->Path()),
                  QM_VOID);
  IDB_TRY_INSPECT(const int64_t& journalFileSize,
                  GetFileSize(journalQuotaObject->Path()), QM_VOID);

  DebugOnly<bool> result = journalQuotaObject->MaybeUpdateSize(
      journalFileSize, /* aTruncate */ true);
  MOZ_ASSERT(result);

  result = quotaObject->MaybeUpdateSize(fileSize, /* aTruncate */ true);
  MOZ_ASSERT(result);
}

Result<int64_t, nsresult> DatabaseConnection::GetFileSize(
    const nsAString& aPath) {
  MOZ_ASSERT(!aPath.IsEmpty());

  IDB_TRY_INSPECT(const auto& file, QM_NewLocalFile(aPath));
  IDB_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(file, Exists));

  if (exists) {
    IDB_TRY_RETURN(MOZ_TO_RESULT_INVOKE(file, GetFileSize));
  }

  return 0;
}

DatabaseConnection::AutoSavepoint::AutoSavepoint()
    : mConnection(nullptr)
#ifdef DEBUG
      ,
      mDEBUGTransaction(nullptr)
#endif
{
  MOZ_COUNT_CTOR(DatabaseConnection::AutoSavepoint);
}

DatabaseConnection::AutoSavepoint::~AutoSavepoint() {
  MOZ_COUNT_DTOR(DatabaseConnection::AutoSavepoint);

  if (mConnection) {
    mConnection->AssertIsOnConnectionThread();
    MOZ_ASSERT(mDEBUGTransaction);
    MOZ_ASSERT(
        mDEBUGTransaction->GetMode() == IDBTransaction::Mode::ReadWrite ||
        mDEBUGTransaction->GetMode() == IDBTransaction::Mode::ReadWriteFlush ||
        mDEBUGTransaction->GetMode() == IDBTransaction::Mode::Cleanup ||
        mDEBUGTransaction->GetMode() == IDBTransaction::Mode::VersionChange);

    if (NS_FAILED(mConnection->RollbackSavepoint())) {
      NS_WARNING("Failed to rollback savepoint!");
    }
  }
}

nsresult DatabaseConnection::AutoSavepoint::Start(
    const TransactionBase& aTransaction) {
  MOZ_ASSERT(aTransaction.GetMode() == IDBTransaction::Mode::ReadWrite ||
             aTransaction.GetMode() == IDBTransaction::Mode::ReadWriteFlush ||
             aTransaction.GetMode() == IDBTransaction::Mode::Cleanup ||
             aTransaction.GetMode() == IDBTransaction::Mode::VersionChange);

  DatabaseConnection* connection = aTransaction.GetDatabase().GetConnection();
  MOZ_ASSERT(connection);
  connection->AssertIsOnConnectionThread();

  // The previous operation failed to begin a write transaction and the
  // following opertion jumped to the connection thread before the previous
  // operation has updated its failure to the transaction.
  if (!connection->GetUpdateRefcountFunction()) {
    NS_WARNING(
        "The connection was closed because the previous operation "
        "failed!");
    return NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  MOZ_ASSERT(!mConnection);
  MOZ_ASSERT(!mDEBUGTransaction);

  IDB_TRY(connection->StartSavepoint());

  mConnection = connection;
#ifdef DEBUG
  mDEBUGTransaction = &aTransaction;
#endif

  return NS_OK;
}

nsresult DatabaseConnection::AutoSavepoint::Commit() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mDEBUGTransaction);

  IDB_TRY(mConnection->ReleaseSavepoint());

  mConnection = nullptr;
#ifdef DEBUG
  mDEBUGTransaction = nullptr;
#endif

  return NS_OK;
}

DatabaseConnection::UpdateRefcountFunction::UpdateRefcountFunction(
    DatabaseConnection* const aConnection, FileManager& aFileManager)
    : mConnection(aConnection),
      mFileManager(aFileManager),
      mInSavepoint(false) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
}

nsresult DatabaseConnection::UpdateRefcountFunction::WillCommit() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection->HasStorageConnection());

  AUTO_PROFILER_LABEL("DatabaseConnection::UpdateRefcountFunction::WillCommit",
                      DOM);

  // The parameter names are not used, parameters are bound by index
  // only locally in the same function.
  auto update =
      [updateStatement = LazyStatement{*mConnection,
                                       "UPDATE file "
                                       "SET refcount = refcount + :delta "
                                       "WHERE id = :id"_ns},
       selectStatement = LazyStatement{*mConnection,
                                       "SELECT id "
                                       "FROM file "
                                       "WHERE id = :id"_ns},
       insertStatement =
           LazyStatement{
               *mConnection,
               "INSERT INTO file (id, refcount) VALUES(:id, :delta)"_ns},
       this](int64_t aId, int32_t aDelta) mutable -> Result<Ok, nsresult> {
    AUTO_PROFILER_LABEL(
        "DatabaseConnection::UpdateRefcountFunction::WillCommit::Update", DOM);
    {
      IDB_TRY_INSPECT(const auto& borrowedUpdateStatement,
                      updateStatement.Borrow());

      IDB_TRY(borrowedUpdateStatement->BindInt32ByIndex(0, aDelta));
      IDB_TRY(borrowedUpdateStatement->BindInt64ByIndex(1, aId));
      IDB_TRY(borrowedUpdateStatement->Execute());
    }

    IDB_TRY_INSPECT(
        const int32_t& rows,
        MOZ_TO_RESULT_INVOKE(mConnection->MutableStorageConnection(),
                             GetAffectedRows));

    if (rows > 0) {
      IDB_TRY_INSPECT(const bool& hasResult,
                      selectStatement
                          .BorrowAndExecuteSingleStep(
                              [aId](auto& stmt) -> Result<Ok, nsresult> {
                                IDB_TRY(stmt.BindInt64ByIndex(0, aId));
                                return Ok{};
                              })
                          .map(IsSome));

      if (!hasResult) {
        // Don't have to create the journal here, we can create all at once,
        // just before commit
        mJournalsToCreateBeforeCommit.AppendElement(aId);
      }

      return Ok{};
    }

    IDB_TRY_INSPECT(const auto& borrowedInsertStatement,
                    insertStatement.Borrow());

    IDB_TRY(borrowedInsertStatement->BindInt64ByIndex(0, aId));
    IDB_TRY(borrowedInsertStatement->BindInt32ByIndex(1, aDelta));
    IDB_TRY(borrowedInsertStatement->Execute());

    mJournalsToRemoveAfterCommit.AppendElement(aId);

    return Ok{};
  };

  IDB_TRY(CollectEachInRange(
      mFileInfoEntries, [&update](const auto& entry) -> Result<Ok, nsresult> {
        const auto delta = entry.GetData()->Delta();
        if (delta) {
          IDB_TRY(update(entry.GetKey(), delta));
        }

        return Ok{};
      }));

  IDB_TRY(CreateJournals());

  return NS_OK;
}

void DatabaseConnection::UpdateRefcountFunction::DidCommit() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("DatabaseConnection::UpdateRefcountFunction::DidCommit",
                      DOM);

  for (const auto& entry : mFileInfoEntries) {
    entry.GetData()->MaybeUpdateDBRefs();
  }

  if (NS_FAILED(RemoveJournals(mJournalsToRemoveAfterCommit))) {
    NS_WARNING("RemoveJournals failed!");
  }
}

void DatabaseConnection::UpdateRefcountFunction::DidAbort() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("DatabaseConnection::UpdateRefcountFunction::DidAbort",
                      DOM);

  if (NS_FAILED(RemoveJournals(mJournalsToRemoveAfterAbort))) {
    NS_WARNING("RemoveJournals failed!");
  }
}

void DatabaseConnection::UpdateRefcountFunction::StartSavepoint() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!mInSavepoint);
  MOZ_ASSERT(!mSavepointEntriesIndex.Count());

  mInSavepoint = true;
}

void DatabaseConnection::UpdateRefcountFunction::ReleaseSavepoint() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mInSavepoint);

  mSavepointEntriesIndex.Clear();
  mInSavepoint = false;
}

void DatabaseConnection::UpdateRefcountFunction::RollbackSavepoint() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mInSavepoint);

  for (const auto& entry : mSavepointEntriesIndex) {
    entry.GetData()->DecBySavepointDelta();
  }

  mInSavepoint = false;
  mSavepointEntriesIndex.Clear();
}

void DatabaseConnection::UpdateRefcountFunction::Reset() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!mSavepointEntriesIndex.Count());
  MOZ_ASSERT(!mInSavepoint);

  mJournalsToCreateBeforeCommit.Clear();
  mJournalsToRemoveAfterCommit.Clear();
  mJournalsToRemoveAfterAbort.Clear();

  // FileInfo implementation automatically removes unreferenced files, but it's
  // done asynchronously and with a delay. We want to remove them (and decrease
  // quota usage) before we fire the commit event.
  for (const auto& entry : mFileInfoEntries) {
    // We need to move mFileInfo into a raw pointer in order to release it
    // explicitly with aSyncDeleteFile == true.
    FileInfo* const fileInfo =
        entry.GetData()->ReleaseFileInfo().forget().take();
    MOZ_ASSERT(fileInfo);

    fileInfo->Release(/* aSyncDeleteFile */ true);
  }

  mFileInfoEntries.Clear();
}

nsresult DatabaseConnection::UpdateRefcountFunction::ProcessValue(
    mozIStorageValueArray* aValues, int32_t aIndex, UpdateType aUpdateType) {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aValues);

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::ProcessValue", DOM);

  IDB_TRY_INSPECT(const int32_t& type,
                  MOZ_TO_RESULT_INVOKE(aValues, GetTypeOfIndex, aIndex));

  if (type == mozIStorageValueArray::VALUE_TYPE_NULL) {
    return NS_OK;
  }

  IDB_TRY_INSPECT(const auto& ids, MOZ_TO_RESULT_INVOKE_TYPED(
                                       nsString, aValues, GetString, aIndex));

  IDB_TRY_INSPECT(const auto& files,
                  DeserializeStructuredCloneFiles(mFileManager, ids));

  for (const StructuredCloneFileParent& file : files) {
    const int64_t id = file.FileInfo().Id();
    MOZ_ASSERT(id > 0);

    const auto entry =
        WrapNotNull(mFileInfoEntries.GetOrInsertNew(id, file.FileInfoPtr()));

    if (mInSavepoint) {
      mSavepointEntriesIndex.InsertOrUpdate(id, entry);
    }

    switch (aUpdateType) {
      case UpdateType::Increment:
        entry->IncDeltas(mInSavepoint);
        break;
      case UpdateType::Decrement:
        entry->DecDeltas(mInSavepoint);
        break;
      default:
        MOZ_CRASH("Unknown update type!");
    }
  }

  return NS_OK;
}

nsresult DatabaseConnection::UpdateRefcountFunction::CreateJournals() {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::CreateJournals", DOM);

  const nsCOMPtr<nsIFile> journalDirectory = mFileManager.GetJournalDirectory();
  IDB_TRY(OkIf(journalDirectory), NS_ERROR_FAILURE);

  for (const int64_t id : mJournalsToCreateBeforeCommit) {
    const nsCOMPtr<nsIFile> file =
        FileManager::GetFileForId(journalDirectory, id);
    IDB_TRY(OkIf(file), NS_ERROR_FAILURE);

    IDB_TRY(file->Create(nsIFile::NORMAL_FILE_TYPE, 0644));

    mJournalsToRemoveAfterAbort.AppendElement(id);
  }

  return NS_OK;
}

nsresult DatabaseConnection::UpdateRefcountFunction::RemoveJournals(
    const nsTArray<int64_t>& aJournals) {
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::RemoveJournals", DOM);

  nsCOMPtr<nsIFile> journalDirectory = mFileManager.GetJournalDirectory();
  IDB_TRY(OkIf(journalDirectory), NS_ERROR_FAILURE);

  for (const auto& journal : aJournals) {
    nsCOMPtr<nsIFile> file =
        FileManager::GetFileForId(journalDirectory, journal);
    IDB_TRY(OkIf(file), NS_ERROR_FAILURE);

    [&file] { IDB_TRY(file->Remove(false), QM_VOID); }();
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(DatabaseConnection::UpdateRefcountFunction,
                  mozIStorageFunction)

NS_IMETHODIMP
DatabaseConnection::UpdateRefcountFunction::OnFunctionCall(
    mozIStorageValueArray* aValues, nsIVariant** _retval) {
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);

  AUTO_PROFILER_LABEL(
      "DatabaseConnection::UpdateRefcountFunction::OnFunctionCall", DOM);

#ifdef DEBUG
  {
    IDB_TRY_INSPECT(const uint32_t& numEntries,
                    MOZ_TO_RESULT_INVOKE(aValues, GetNumEntries),
                    QM_ASSERT_UNREACHABLE);

    MOZ_ASSERT(numEntries == 2);

    IDB_TRY_INSPECT(const int32_t& type1,
                    MOZ_TO_RESULT_INVOKE(aValues, GetTypeOfIndex, 0),
                    QM_ASSERT_UNREACHABLE);

    IDB_TRY_INSPECT(const int32_t& type2,
                    MOZ_TO_RESULT_INVOKE(aValues, GetTypeOfIndex, 1),
                    QM_ASSERT_UNREACHABLE);

    MOZ_ASSERT(!(type1 == mozIStorageValueArray::VALUE_TYPE_NULL &&
                 type2 == mozIStorageValueArray::VALUE_TYPE_NULL));
  }
#endif

  IDB_TRY(ProcessValue(aValues, 0, UpdateType::Decrement));

  IDB_TRY(ProcessValue(aValues, 1, UpdateType::Increment));

  return NS_OK;
}

/*******************************************************************************
 * ConnectionPool implementation
 ******************************************************************************/

ConnectionPool::ConnectionPool()
    : mDatabasesMutex("ConnectionPool::mDatabasesMutex"),
      mIdleTimer(NS_NewTimer()),
      mNextTransactionId(0),
      mTotalThreadCount(0) {
  AssertIsOnOwningThread();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mIdleTimer);
}

ConnectionPool::~ConnectionPool() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mIdleThreads.IsEmpty());
  MOZ_ASSERT(mIdleDatabases.IsEmpty());
  MOZ_ASSERT(!mIdleTimer);
  MOZ_ASSERT(mTargetIdleTime.IsNull());
  MOZ_ASSERT(!mDatabases.Count());
  MOZ_ASSERT(!mTransactions.Count());
  MOZ_ASSERT(mQueuedTransactions.IsEmpty());
  MOZ_ASSERT(mCompleteCallbacks.IsEmpty());
  MOZ_ASSERT(!mTotalThreadCount);
  MOZ_ASSERT(mShutdownRequested);
  MOZ_ASSERT(mShutdownComplete);
}

// static
void ConnectionPool::IdleTimerCallback(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(aTimer);
  MOZ_ASSERT(aClosure);

  AUTO_PROFILER_LABEL("ConnectionPool::IdleTimerCallback", DOM);

  auto& self = *static_cast<ConnectionPool*>(aClosure);
  MOZ_ASSERT(self.mIdleTimer);
  MOZ_ASSERT(SameCOMIdentity(self.mIdleTimer, aTimer));
  MOZ_ASSERT(!self.mTargetIdleTime.IsNull());
  MOZ_ASSERT_IF(self.mIdleDatabases.IsEmpty(), !self.mIdleThreads.IsEmpty());
  MOZ_ASSERT_IF(self.mIdleThreads.IsEmpty(), !self.mIdleDatabases.IsEmpty());

  self.mTargetIdleTime = TimeStamp();

  // Cheat a little.
  const TimeStamp now =
      TimeStamp::NowLoRes() + TimeDuration::FromMilliseconds(500);

  // XXX Move this to ArrayAlgorithm.h?
  const auto removeUntil = [](auto& array, auto&& cond) {
    const auto begin = array.begin(), end = array.end();
    array.RemoveElementsRange(
        begin, std::find_if(begin, end, std::forward<decltype(cond)>(cond)));
  };

  removeUntil(self.mIdleDatabases, [now, &self](const auto& info) {
    if (now >= info.mIdleTime) {
      if ((*info.mDatabaseInfo)->mIdle) {
        self.PerformIdleDatabaseMaintenance(*info.mDatabaseInfo.ref());
      } else {
        self.CloseDatabase(*info.mDatabaseInfo.ref());
      }

      return false;
    }

    return true;
  });

  removeUntil(self.mIdleThreads, [now, &self](auto& info) {
    info.mThreadInfo.AssertValid();

    if (now >= info.mIdleTime) {
      self.ShutdownThread(std::move(info.mThreadInfo));

      return false;
    }

    return true;
  });

  self.AdjustIdleTimer();
}

Result<RefPtr<DatabaseConnection>, nsresult>
ConnectionPool::GetOrCreateConnection(const Database& aDatabase) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  AUTO_PROFILER_LABEL("ConnectionPool::GetOrCreateConnection", DOM);

  DatabaseInfo* dbInfo;
  {
    MutexAutoLock lock(mDatabasesMutex);

    dbInfo = mDatabases.Get(aDatabase.Id());
  }

  MOZ_ASSERT(dbInfo);

  if (dbInfo->mConnection) {
    dbInfo->AssertIsOnConnectionThread();

    return dbInfo->mConnection;
  }

  MOZ_ASSERT(!dbInfo->mDEBUGConnectionThread);

  IDB_TRY_UNWRAP(
      MovingNotNull<nsCOMPtr<mozIStorageConnection>> storageConnection,
      GetStorageConnection(aDatabase.FilePath(), aDatabase.DirectoryLockId(),
                           aDatabase.TelemetryId(), aDatabase.MaybeKeyRef()));

  RefPtr<DatabaseConnection> connection = new DatabaseConnection(
      std::move(storageConnection), aDatabase.GetFileManagerPtr());

  IDB_TRY(connection->Init());

  dbInfo->mConnection = connection;

  IDB_DEBUG_LOG(("ConnectionPool created connection 0x%p for '%s'",
                 dbInfo->mConnection.get(),
                 NS_ConvertUTF16toUTF8(aDatabase.FilePath()).get()));

#ifdef DEBUG
  dbInfo->mDEBUGConnectionThread = PR_GetCurrentThread();
#endif

  return connection;
}

uint64_t ConnectionPool::Start(
    const nsID& aBackgroundChildLoggingId, const nsACString& aDatabaseId,
    int64_t aLoggingSerialNumber, const nsTArray<nsString>& aObjectStoreNames,
    bool aIsWriteTransaction,
    TransactionDatabaseOperationBase* aTransactionOp) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseId.IsEmpty());
  MOZ_ASSERT(mNextTransactionId < UINT64_MAX);
  MOZ_ASSERT(!mShutdownRequested);

  AUTO_PROFILER_LABEL("ConnectionPool::Start", DOM);

  const uint64_t transactionId = ++mNextTransactionId;

  // To avoid always acquiring a lock, we don't use WithEntryHandle here, which
  // would require a lock in any case.
  DatabaseInfo* dbInfo = mDatabases.Get(aDatabaseId);

  const bool databaseInfoIsNew = !dbInfo;

  if (databaseInfoIsNew) {
    MutexAutoLock lock(mDatabasesMutex);

    dbInfo = mDatabases
                 .InsertOrUpdate(aDatabaseId,
                                 MakeUnique<DatabaseInfo>(this, aDatabaseId))
                 .get();
  }

  MOZ_ASSERT(!mTransactions.Contains(transactionId));
  auto& transactionInfo = *mTransactions.InsertOrUpdate(
      transactionId, MakeUnique<TransactionInfo>(
                         *dbInfo, aBackgroundChildLoggingId, aDatabaseId,
                         transactionId, aLoggingSerialNumber, aObjectStoreNames,
                         aIsWriteTransaction, aTransactionOp));

  if (aIsWriteTransaction) {
    MOZ_ASSERT(dbInfo->mWriteTransactionCount < UINT32_MAX);
    dbInfo->mWriteTransactionCount++;
  } else {
    MOZ_ASSERT(dbInfo->mReadTransactionCount < UINT32_MAX);
    dbInfo->mReadTransactionCount++;
  }

  auto& blockingTransactions = dbInfo->mBlockingTransactions;

  for (const nsString& objectStoreName : aObjectStoreNames) {
    TransactionInfoPair* blockInfo =
        blockingTransactions.GetOrInsertNew(objectStoreName);

    // Mark what we are blocking on.
    if (const auto maybeBlockingRead = blockInfo->mLastBlockingReads) {
      transactionInfo.mBlockedOn.PutEntry(&maybeBlockingRead.ref());
      maybeBlockingRead->AddBlockingTransaction(transactionInfo);
    }

    if (aIsWriteTransaction) {
      for (const auto blockingWrite : blockInfo->mLastBlockingWrites) {
        transactionInfo.mBlockedOn.PutEntry(blockingWrite);
        blockingWrite->AddBlockingTransaction(transactionInfo);
      }

      blockInfo->mLastBlockingReads = SomeRef(transactionInfo);
      blockInfo->mLastBlockingWrites.Clear();
    } else {
      blockInfo->mLastBlockingWrites.AppendElement(
          WrapNotNullUnchecked(&transactionInfo));
    }
  }

  if (!transactionInfo.mBlockedOn.Count()) {
    Unused << ScheduleTransaction(transactionInfo,
                                  /* aFromQueuedTransactions */ false);
  }

  if (!databaseInfoIsNew &&
      (mIdleDatabases.RemoveElement(dbInfo) ||
       mDatabasesPerformingIdleMaintenance.RemoveElement(dbInfo))) {
    AdjustIdleTimer();
  }

  return transactionId;
}

void ConnectionPool::Dispatch(uint64_t aTransactionId, nsIRunnable* aRunnable) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRunnable);

  AUTO_PROFILER_LABEL("ConnectionPool::Dispatch", DOM);

  auto* const transactionInfo = mTransactions.Get(aTransactionId);
  MOZ_ASSERT(transactionInfo);
  MOZ_ASSERT(!transactionInfo->mFinished);

  if (transactionInfo->mRunning) {
    DatabaseInfo& dbInfo = transactionInfo->mDatabaseInfo;
    dbInfo.mThreadInfo.AssertValid();
    MOZ_ASSERT(!dbInfo.mClosing);
    MOZ_ASSERT_IF(
        transactionInfo->mIsWriteTransaction,
        dbInfo.mRunningWriteTransaction &&
            dbInfo.mRunningWriteTransaction.refEquals(*transactionInfo));

    MOZ_ALWAYS_SUCCEEDS(
        dbInfo.mThreadInfo.ThreadRef().Dispatch(aRunnable, NS_DISPATCH_NORMAL));
  } else {
    transactionInfo->mQueuedRunnables.AppendElement(aRunnable);
  }
}

void ConnectionPool::Finish(uint64_t aTransactionId,
                            FinishCallback* aCallback) {
  AssertIsOnOwningThread();

#ifdef DEBUG
  auto* const transactionInfo = mTransactions.Get(aTransactionId);
  MOZ_ASSERT(transactionInfo);
  MOZ_ASSERT(!transactionInfo->mFinished);
#endif

  AUTO_PROFILER_LABEL("ConnectionPool::Finish", DOM);

  RefPtr<FinishCallbackWrapper> wrapper =
      new FinishCallbackWrapper(this, aTransactionId, aCallback);

  Dispatch(aTransactionId, wrapper);

#ifdef DEBUG
  transactionInfo->mFinished.Flip();
#endif
}

void ConnectionPool::WaitForDatabasesToComplete(
    nsTArray<nsCString>&& aDatabaseIds, nsIRunnable* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseIds.IsEmpty());
  MOZ_ASSERT(aCallback);

  AUTO_PROFILER_LABEL("ConnectionPool::WaitForDatabasesToComplete", DOM);

  bool mayRunCallbackImmediately = true;

  for (const nsCString& databaseId : aDatabaseIds) {
    MOZ_ASSERT(!databaseId.IsEmpty());

    if (CloseDatabaseWhenIdleInternal(databaseId)) {
      mayRunCallbackImmediately = false;
    }
  }

  if (mayRunCallbackImmediately) {
    Unused << aCallback->Run();
    return;
  }

  mCompleteCallbacks.EmplaceBack(MakeUnique<DatabasesCompleteCallback>(
      std::move(aDatabaseIds), aCallback));
}

void ConnectionPool::Shutdown() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mShutdownComplete);

  AUTO_PROFILER_LABEL("ConnectionPool::Shutdown", DOM);

  mShutdownRequested.Flip();

  CancelIdleTimer();
  MOZ_ASSERT(mTargetIdleTime.IsNull());

  mIdleTimer = nullptr;

  CloseIdleDatabases();

  ShutdownIdleThreads();

  if (!mDatabases.Count()) {
    MOZ_ASSERT(!mTransactions.Count());

    Cleanup();

    MOZ_ASSERT(mShutdownComplete);
    return;
  }

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil(
      [&]() { return static_cast<bool>(mShutdownComplete); }));
}

void ConnectionPool::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mShutdownRequested);
  MOZ_ASSERT(!mShutdownComplete);
  MOZ_ASSERT(!mDatabases.Count());
  MOZ_ASSERT(!mTransactions.Count());
  MOZ_ASSERT(mIdleThreads.IsEmpty());

  AUTO_PROFILER_LABEL("ConnectionPool::Cleanup", DOM);

  if (!mCompleteCallbacks.IsEmpty()) {
    // Run all callbacks manually now.

    {
      auto completeCallbacks = std::move(mCompleteCallbacks);
      for (const auto& completeCallback : completeCallbacks) {
        MOZ_ASSERT(completeCallback);
        MOZ_ASSERT(completeCallback->mCallback);

        Unused << completeCallback->mCallback->Run();
      }

      // We expect no new callbacks being completed by running the existing
      // ones.
      MOZ_ASSERT(mCompleteCallbacks.IsEmpty());
    }

    // And make sure they get processed.
    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

    MOZ_ALWAYS_SUCCEEDS(NS_ProcessPendingEvents(currentThread));
  }

  mShutdownComplete.Flip();
}

void ConnectionPool::AdjustIdleTimer() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mIdleTimer);

  AUTO_PROFILER_LABEL("ConnectionPool::AdjustIdleTimer", DOM);

  // Figure out the next time at which we should release idle resources. This
  // includes both databases and threads.
  TimeStamp newTargetIdleTime;
  MOZ_ASSERT(newTargetIdleTime.IsNull());

  if (!mIdleDatabases.IsEmpty()) {
    newTargetIdleTime = mIdleDatabases[0].mIdleTime;
  }

  if (!mIdleThreads.IsEmpty()) {
    const TimeStamp& idleTime = mIdleThreads[0].mIdleTime;

    if (newTargetIdleTime.IsNull() || idleTime < newTargetIdleTime) {
      newTargetIdleTime = idleTime;
    }
  }

  MOZ_ASSERT_IF(newTargetIdleTime.IsNull(), mIdleDatabases.IsEmpty());
  MOZ_ASSERT_IF(newTargetIdleTime.IsNull(), mIdleThreads.IsEmpty());

  // Cancel the timer if it was running and the new target time is different.
  if (!mTargetIdleTime.IsNull() &&
      (newTargetIdleTime.IsNull() || mTargetIdleTime != newTargetIdleTime)) {
    CancelIdleTimer();

    MOZ_ASSERT(mTargetIdleTime.IsNull());
  }

  // Schedule the timer if we have a target time different than before.
  if (!newTargetIdleTime.IsNull() &&
      (mTargetIdleTime.IsNull() || mTargetIdleTime != newTargetIdleTime)) {
    double delta = (newTargetIdleTime - TimeStamp::NowLoRes()).ToMilliseconds();

    uint32_t delay;
    if (delta > 0) {
      delay = uint32_t(std::min(delta, double(UINT32_MAX)));
    } else {
      delay = 0;
    }

    MOZ_ALWAYS_SUCCEEDS(mIdleTimer->InitWithNamedFuncCallback(
        IdleTimerCallback, this, delay, nsITimer::TYPE_ONE_SHOT,
        "ConnectionPool::IdleTimerCallback"));

    mTargetIdleTime = newTargetIdleTime;
  }
}

void ConnectionPool::CancelIdleTimer() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mIdleTimer);

  if (!mTargetIdleTime.IsNull()) {
    MOZ_ALWAYS_SUCCEEDS(mIdleTimer->Cancel());

    mTargetIdleTime = TimeStamp();
    MOZ_ASSERT(mTargetIdleTime.IsNull());
  }
}

void ConnectionPool::ShutdownThread(ThreadInfo aThreadInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTotalThreadCount);

  // We need to move thread and runnable separately.
  auto [thread, runnable] = aThreadInfo.Forget();

  IDB_DEBUG_LOG(("ConnectionPool shutting down thread %" PRIu32,
                 runnable->SerialNumber()));

  // This should clean up the thread with the profiler.
  MOZ_ALWAYS_SUCCEEDS(thread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(NewRunnableMethod(
      "nsIThread::AsyncShutdown", thread, &nsIThread::AsyncShutdown)));

  mTotalThreadCount--;
}

void ConnectionPool::CloseIdleDatabases() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mShutdownRequested);

  AUTO_PROFILER_LABEL("ConnectionPool::CloseIdleDatabases", DOM);

  if (!mIdleDatabases.IsEmpty()) {
    for (IdleDatabaseInfo& idleInfo : mIdleDatabases) {
      CloseDatabase(*idleInfo.mDatabaseInfo.ref());
    }
    mIdleDatabases.Clear();
  }

  if (!mDatabasesPerformingIdleMaintenance.IsEmpty()) {
    for (const auto dbInfo : mDatabasesPerformingIdleMaintenance) {
      CloseDatabase(*dbInfo);
    }
    mDatabasesPerformingIdleMaintenance.Clear();
  }
}

void ConnectionPool::ShutdownIdleThreads() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mShutdownRequested);

  AUTO_PROFILER_LABEL("ConnectionPool::ShutdownIdleThreads", DOM);

  for (auto& idleThread : mIdleThreads) {
    ShutdownThread(std::move(idleThread.mThreadInfo));
  }
  mIdleThreads.Clear();
}

bool ConnectionPool::ScheduleTransaction(TransactionInfo& aTransactionInfo,
                                         bool aFromQueuedTransactions) {
  AssertIsOnOwningThread();

  AUTO_PROFILER_LABEL("ConnectionPool::ScheduleTransaction", DOM);

  DatabaseInfo& dbInfo = aTransactionInfo.mDatabaseInfo;

  dbInfo.mIdle = false;

  if (dbInfo.mClosing) {
    MOZ_ASSERT(!mIdleDatabases.Contains(&dbInfo));
    MOZ_ASSERT(
        !dbInfo.mTransactionsScheduledDuringClose.Contains(&aTransactionInfo));

    dbInfo.mTransactionsScheduledDuringClose.AppendElement(
        WrapNotNullUnchecked(&aTransactionInfo));
    return true;
  }

  if (!dbInfo.mThreadInfo.IsValid()) {
    if (mIdleThreads.IsEmpty()) {
      bool created = false;

      if (mTotalThreadCount < kMaxConnectionThreadCount) {
        // This will set the thread up with the profiler.
        RefPtr<ThreadRunnable> runnable = new ThreadRunnable();

        nsCOMPtr<nsIThread> newThread;
        nsresult rv = NS_NewNamedThread(runnable->GetThreadName(),
                                        getter_AddRefs(newThread), runnable);
        if (NS_SUCCEEDED(rv)) {
          newThread->SetNameForWakeupTelemetry("IndexedDB (all)"_ns);
          MOZ_ASSERT(newThread);

          IDB_DEBUG_LOG(("ConnectionPool created thread %" PRIu32,
                         runnable->SerialNumber()));

          dbInfo.mThreadInfo =
              ThreadInfo{std::move(newThread), std::move(runnable)};

          mTotalThreadCount++;
          created = true;
        } else {
          NS_WARNING("Failed to make new thread!");
        }
      } else if (!mDatabasesPerformingIdleMaintenance.IsEmpty()) {
        // We need a thread right now so force all idle processing to stop by
        // posting a dummy runnable to each thread that might be doing idle
        // maintenance.
        //
        // This is copied for each database inside the loop below, it is
        // deliberately const to prevent the attempt to wrongly optimize the
        // refcounting by passing runnable.forget() to the Dispatch method, see
        // bug 1598559.
        const nsCOMPtr<nsIRunnable> runnable =
            new Runnable("IndexedDBDummyRunnable");

        for (uint32_t index = mDatabasesPerformingIdleMaintenance.Length();
             index > 0; index--) {
          const auto dbInfo = mDatabasesPerformingIdleMaintenance[index - 1];
          dbInfo->mThreadInfo.AssertValid();

          MOZ_ALWAYS_SUCCEEDS(dbInfo->mThreadInfo.ThreadRef().Dispatch(
              runnable, NS_DISPATCH_NORMAL));
        }
      }

      if (!created) {
        if (!aFromQueuedTransactions) {
          MOZ_ASSERT(!mQueuedTransactions.Contains(&aTransactionInfo));
          mQueuedTransactions.AppendElement(
              WrapNotNullUnchecked(&aTransactionInfo));
        }
        return false;
      }
    } else {
      dbInfo.mThreadInfo = std::move(mIdleThreads.PopLastElement().mThreadInfo);

      AdjustIdleTimer();
    }
  }

  dbInfo.mThreadInfo.AssertValid();

  if (aTransactionInfo.mIsWriteTransaction) {
    if (dbInfo.mRunningWriteTransaction) {
      // SQLite only allows one write transaction at a time so queue this
      // transaction for later.
      MOZ_ASSERT(
          !dbInfo.mScheduledWriteTransactions.Contains(&aTransactionInfo));

      dbInfo.mScheduledWriteTransactions.AppendElement(
          WrapNotNullUnchecked(&aTransactionInfo));
      return true;
    }

    dbInfo.mRunningWriteTransaction = SomeRef(aTransactionInfo);
    dbInfo.mNeedsCheckpoint = true;
  }

  MOZ_ASSERT(!aTransactionInfo.mRunning);
  aTransactionInfo.mRunning = true;

  nsTArray<nsCOMPtr<nsIRunnable>>& queuedRunnables =
      aTransactionInfo.mQueuedRunnables;

  if (!queuedRunnables.IsEmpty()) {
    for (auto& queuedRunnable : queuedRunnables) {
      MOZ_ALWAYS_SUCCEEDS(dbInfo.mThreadInfo.ThreadRef().Dispatch(
          queuedRunnable.forget(), NS_DISPATCH_NORMAL));
    }

    queuedRunnables.Clear();
  }

  return true;
}

void ConnectionPool::NoteFinishedTransaction(uint64_t aTransactionId) {
  AssertIsOnOwningThread();

  AUTO_PROFILER_LABEL("ConnectionPool::NoteFinishedTransaction", DOM);

  auto* const transactionInfo = mTransactions.Get(aTransactionId);
  MOZ_ASSERT(transactionInfo);
  MOZ_ASSERT(transactionInfo->mRunning);
  MOZ_ASSERT(transactionInfo->mFinished);

  transactionInfo->mRunning = false;

  DatabaseInfo& dbInfo = transactionInfo->mDatabaseInfo;
  MOZ_ASSERT(mDatabases.Get(transactionInfo->mDatabaseId) == &dbInfo);
  dbInfo.mThreadInfo.AssertValid();

  // Schedule the next write transaction if there are any queued.
  if (dbInfo.mRunningWriteTransaction &&
      dbInfo.mRunningWriteTransaction.refEquals(*transactionInfo)) {
    MOZ_ASSERT(transactionInfo->mIsWriteTransaction);
    MOZ_ASSERT(dbInfo.mNeedsCheckpoint);

    dbInfo.mRunningWriteTransaction = Nothing();

    if (!dbInfo.mScheduledWriteTransactions.IsEmpty()) {
      const auto nextWriteTransaction = dbInfo.mScheduledWriteTransactions[0];

      dbInfo.mScheduledWriteTransactions.RemoveElementAt(0);

      MOZ_ALWAYS_TRUE(ScheduleTransaction(*nextWriteTransaction,
                                          /* aFromQueuedTransactions */ false));
    }
  }

  for (const auto& objectStoreName : transactionInfo->mObjectStoreNames) {
    TransactionInfoPair* blockInfo =
        dbInfo.mBlockingTransactions.Get(objectStoreName);
    MOZ_ASSERT(blockInfo);

    if (transactionInfo->mIsWriteTransaction && blockInfo->mLastBlockingReads &&
        blockInfo->mLastBlockingReads.refEquals(*transactionInfo)) {
      blockInfo->mLastBlockingReads = Nothing();
    }

    blockInfo->mLastBlockingWrites.RemoveElement(transactionInfo);
  }

  transactionInfo->RemoveBlockingTransactions();

  if (transactionInfo->mIsWriteTransaction) {
    MOZ_ASSERT(dbInfo.mWriteTransactionCount);
    dbInfo.mWriteTransactionCount--;
  } else {
    MOZ_ASSERT(dbInfo.mReadTransactionCount);
    dbInfo.mReadTransactionCount--;
  }

  mTransactions.Remove(aTransactionId);

  if (!dbInfo.TotalTransactionCount()) {
    MOZ_ASSERT(!dbInfo.mIdle);
    dbInfo.mIdle = true;

    NoteIdleDatabase(dbInfo);
  }
}

void ConnectionPool::ScheduleQueuedTransactions(ThreadInfo aThreadInfo) {
  AssertIsOnOwningThread();
  aThreadInfo.AssertValid();
  MOZ_ASSERT(!mQueuedTransactions.IsEmpty());

  AUTO_PROFILER_LABEL("ConnectionPool::ScheduleQueuedTransactions", DOM);

  auto idleThreadInfo = IdleThreadInfo{std::move(aThreadInfo)};
  MOZ_ASSERT(!mIdleThreads.Contains(idleThreadInfo));
  mIdleThreads.InsertElementSorted(std::move(idleThreadInfo));

  const auto foundIt = std::find_if(
      mQueuedTransactions.begin(), mQueuedTransactions.end(),
      [&me = *this](const auto& queuedTransaction) {
        return !me.ScheduleTransaction(*queuedTransaction,
                                       /* aFromQueuedTransactions */ true);
      });

  mQueuedTransactions.RemoveElementsRange(mQueuedTransactions.begin(), foundIt);

  AdjustIdleTimer();
}

void ConnectionPool::NoteIdleDatabase(DatabaseInfo& aDatabaseInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseInfo.TotalTransactionCount());
  aDatabaseInfo.mThreadInfo.AssertValid();
  MOZ_ASSERT(!mIdleDatabases.Contains(&aDatabaseInfo));

  AUTO_PROFILER_LABEL("ConnectionPool::NoteIdleDatabase", DOM);

  const bool otherDatabasesWaiting = !mQueuedTransactions.IsEmpty();

  if (mShutdownRequested || otherDatabasesWaiting ||
      aDatabaseInfo.mCloseOnIdle) {
    // Make sure we close the connection if we're shutting down or giving the
    // thread to another database.
    CloseDatabase(aDatabaseInfo);

    if (otherDatabasesWaiting) {
      // Let another database use this thread.
      ScheduleQueuedTransactions(std::move(aDatabaseInfo.mThreadInfo));
    } else if (mShutdownRequested) {
      // If there are no other databases that need to run then we can shut this
      // thread down immediately instead of going through the idle thread
      // mechanism.
      ShutdownThread(std::move(aDatabaseInfo.mThreadInfo));
    }

    return;
  }

  mIdleDatabases.InsertElementSorted(IdleDatabaseInfo{aDatabaseInfo});

  AdjustIdleTimer();
}

void ConnectionPool::NoteClosedDatabase(DatabaseInfo& aDatabaseInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabaseInfo.mClosing);
  MOZ_ASSERT(!mIdleDatabases.Contains(&aDatabaseInfo));

  AUTO_PROFILER_LABEL("ConnectionPool::NoteClosedDatabase", DOM);

  aDatabaseInfo.mClosing = false;

  // Figure out what to do with this database's thread. It may have already been
  // given to another database, in which case there's nothing to do here.
  // Otherwise we prioritize the thread as follows:
  //   1. Databases that haven't had an opportunity to run at all are highest
  //      priority. Those live in the |mQueuedTransactions| list.
  //   2. If this database has additional transactions that were started after
  //      we began closing the connection then the thread can be reused for
  //      those transactions.
  //   3. If we're shutting down then we can get rid of the thread.
  //   4. Finally, if nothing above took the thread then we can add it to our
  //      list of idle threads. It may be reused or it may time out. If we have
  //      too many idle threads then we will shut down the oldest.
  if (aDatabaseInfo.mThreadInfo.IsValid()) {
    if (!mQueuedTransactions.IsEmpty()) {
      // Give the thread to another database.
      ScheduleQueuedTransactions(std::move(aDatabaseInfo.mThreadInfo));
    } else if (!aDatabaseInfo.TotalTransactionCount()) {
      if (mShutdownRequested) {
        ShutdownThread(std::move(aDatabaseInfo.mThreadInfo));
      } else {
        auto idleThreadInfo =
            IdleThreadInfo{std::move(aDatabaseInfo.mThreadInfo)};
        MOZ_ASSERT(!mIdleThreads.Contains(idleThreadInfo));

        mIdleThreads.InsertElementSorted(std::move(idleThreadInfo));

        if (mIdleThreads.Length() > kMaxIdleConnectionThreadCount) {
          ShutdownThread(std::move(mIdleThreads[0].mThreadInfo));
          mIdleThreads.RemoveElementAt(0);
        }

        AdjustIdleTimer();
      }
    }
  }

  // Schedule any transactions that were started while we were closing the
  // connection.
  if (aDatabaseInfo.TotalTransactionCount()) {
    auto& scheduledTransactions =
        aDatabaseInfo.mTransactionsScheduledDuringClose;

    MOZ_ASSERT(!scheduledTransactions.IsEmpty());

    for (const auto& scheduledTransaction : scheduledTransactions) {
      Unused << ScheduleTransaction(*scheduledTransaction,
                                    /* aFromQueuedTransactions */ false);
    }

    scheduledTransactions.Clear();

    return;
  }

  // There are no more transactions and the connection has been closed. We're
  // done with this database.
  {
    MutexAutoLock lock(mDatabasesMutex);

    mDatabases.Remove(aDatabaseInfo.mDatabaseId);
  }

  // That just deleted |aDatabaseInfo|, we must not access that below.

  // See if we need to fire any complete callbacks now that the database is
  // finished.
  mCompleteCallbacks.RemoveLastElements(
      mCompleteCallbacks.end() -
      std::remove_if(mCompleteCallbacks.begin(), mCompleteCallbacks.end(),
                     [&me = *this](const auto& completeCallback) {
                       return me.MaybeFireCallback(completeCallback.get());
                     }));

  // If that was the last database and we're supposed to be shutting down then
  // we are finished.
  if (mShutdownRequested && !mDatabases.Count()) {
    MOZ_ASSERT(!mTransactions.Count());
    Cleanup();
  }
}

bool ConnectionPool::MaybeFireCallback(DatabasesCompleteCallback* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!aCallback->mDatabaseIds.IsEmpty());
  MOZ_ASSERT(aCallback->mCallback);

  AUTO_PROFILER_LABEL("ConnectionPool::MaybeFireCallback", DOM);

  if (std::any_of(aCallback->mDatabaseIds.begin(),
                  aCallback->mDatabaseIds.end(),
                  [&databases = mDatabases](const auto& databaseId) {
                    MOZ_ASSERT(!databaseId.IsEmpty());

                    return databases.Get(databaseId);
                  })) {
    return false;
  }

  Unused << aCallback->mCallback->Run();
  return true;
}

void ConnectionPool::PerformIdleDatabaseMaintenance(
    DatabaseInfo& aDatabaseInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseInfo.TotalTransactionCount());
  aDatabaseInfo.mThreadInfo.AssertValid();
  MOZ_ASSERT(aDatabaseInfo.mIdle);
  MOZ_ASSERT(!aDatabaseInfo.mCloseOnIdle);
  MOZ_ASSERT(!aDatabaseInfo.mClosing);
  MOZ_ASSERT(mIdleDatabases.Contains(&aDatabaseInfo));
  MOZ_ASSERT(!mDatabasesPerformingIdleMaintenance.Contains(&aDatabaseInfo));

  const bool neededCheckpoint = aDatabaseInfo.mNeedsCheckpoint;

  aDatabaseInfo.mNeedsCheckpoint = false;
  aDatabaseInfo.mIdle = false;

  mDatabasesPerformingIdleMaintenance.AppendElement(
      WrapNotNullUnchecked(&aDatabaseInfo));

  MOZ_ALWAYS_SUCCEEDS(aDatabaseInfo.mThreadInfo.ThreadRef().Dispatch(
      MakeAndAddRef<IdleConnectionRunnable>(aDatabaseInfo, neededCheckpoint),
      NS_DISPATCH_NORMAL));
}

void ConnectionPool::CloseDatabase(DatabaseInfo& aDatabaseInfo) {
  AssertIsOnOwningThread();
  MOZ_DIAGNOSTIC_ASSERT(!aDatabaseInfo.TotalTransactionCount());
  aDatabaseInfo.mThreadInfo.AssertValid();
  MOZ_ASSERT(!aDatabaseInfo.mClosing);

  aDatabaseInfo.mIdle = false;
  aDatabaseInfo.mNeedsCheckpoint = false;
  aDatabaseInfo.mClosing = true;

  MOZ_ALWAYS_SUCCEEDS(aDatabaseInfo.mThreadInfo.ThreadRef().Dispatch(
      MakeAndAddRef<CloseConnectionRunnable>(aDatabaseInfo),
      NS_DISPATCH_NORMAL));
}

bool ConnectionPool::CloseDatabaseWhenIdleInternal(
    const nsACString& aDatabaseId) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseId.IsEmpty());

  AUTO_PROFILER_LABEL("ConnectionPool::CloseDatabaseWhenIdleInternal", DOM);

  if (DatabaseInfo* dbInfo = mDatabases.Get(aDatabaseId)) {
    if (mIdleDatabases.RemoveElement(dbInfo) ||
        mDatabasesPerformingIdleMaintenance.RemoveElement(dbInfo)) {
      CloseDatabase(*dbInfo);
      AdjustIdleTimer();
    } else {
      dbInfo->mCloseOnIdle.EnsureFlipped();
    }

    return true;
  }

  return false;
}

ConnectionPool::ConnectionRunnable::ConnectionRunnable(
    DatabaseInfo& aDatabaseInfo)
    : Runnable("dom::indexedDB::ConnectionPool::ConnectionRunnable"),
      mDatabaseInfo(aDatabaseInfo),
      mOwningEventTarget(GetCurrentEventTarget()) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabaseInfo.mConnectionPool);
  aDatabaseInfo.mConnectionPool->AssertIsOnOwningThread();
  MOZ_ASSERT(mOwningEventTarget);
}

NS_IMETHODIMP
ConnectionPool::IdleConnectionRunnable::Run() {
  MOZ_ASSERT(!mDatabaseInfo.mIdle);

  const nsCOMPtr<nsIEventTarget> owningThread = std::move(mOwningEventTarget);

  if (owningThread) {
    mDatabaseInfo.AssertIsOnConnectionThread();

    // The connection could be null if EnsureConnection() didn't run or was not
    // successful in TransactionDatabaseOperationBase::RunOnConnectionThread().
    if (mDatabaseInfo.mConnection) {
      mDatabaseInfo.mConnection->DoIdleProcessing(mNeedsCheckpoint);
    }

    MOZ_ALWAYS_SUCCEEDS(owningThread->Dispatch(this, NS_DISPATCH_NORMAL));
    return NS_OK;
  }

  AssertIsOnBackgroundThread();

  RefPtr<ConnectionPool> connectionPool = mDatabaseInfo.mConnectionPool;
  MOZ_ASSERT(connectionPool);

  if (mDatabaseInfo.mClosing || mDatabaseInfo.TotalTransactionCount()) {
    MOZ_ASSERT(!connectionPool->mDatabasesPerformingIdleMaintenance.Contains(
        &mDatabaseInfo));
  } else {
    MOZ_ALWAYS_TRUE(
        connectionPool->mDatabasesPerformingIdleMaintenance.RemoveElement(
            &mDatabaseInfo));

    connectionPool->NoteIdleDatabase(mDatabaseInfo);
  }

  return NS_OK;
}

NS_IMETHODIMP
ConnectionPool::CloseConnectionRunnable::Run() {
  AUTO_PROFILER_LABEL("ConnectionPool::CloseConnectionRunnable::Run", DOM);

  if (mOwningEventTarget) {
    MOZ_ASSERT(mDatabaseInfo.mClosing);

    const nsCOMPtr<nsIEventTarget> owningThread = std::move(mOwningEventTarget);

    // The connection could be null if EnsureConnection() didn't run or was not
    // successful in TransactionDatabaseOperationBase::RunOnConnectionThread().
    if (mDatabaseInfo.mConnection) {
      mDatabaseInfo.AssertIsOnConnectionThread();

      mDatabaseInfo.mConnection->Close();

      IDB_DEBUG_LOG(("ConnectionPool closed connection 0x%p",
                     mDatabaseInfo.mConnection.get()));

      mDatabaseInfo.mConnection = nullptr;

#ifdef DEBUG
      mDatabaseInfo.mDEBUGConnectionThread = nullptr;
#endif
    }

    MOZ_ALWAYS_SUCCEEDS(owningThread->Dispatch(this, NS_DISPATCH_NORMAL));
    return NS_OK;
  }

  RefPtr<ConnectionPool> connectionPool = mDatabaseInfo.mConnectionPool;
  MOZ_ASSERT(connectionPool);

  connectionPool->NoteClosedDatabase(mDatabaseInfo);
  return NS_OK;
}

ConnectionPool::DatabaseInfo::DatabaseInfo(ConnectionPool* aConnectionPool,
                                           const nsACString& aDatabaseId)
    : mConnectionPool(aConnectionPool),
      mDatabaseId(aDatabaseId),
      mReadTransactionCount(0),
      mWriteTransactionCount(0),
      mNeedsCheckpoint(false),
      mIdle(false),
      mClosing(false)
#ifdef DEBUG
      ,
      mDEBUGConnectionThread(nullptr)
#endif
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aConnectionPool);
  aConnectionPool->AssertIsOnOwningThread();
  MOZ_ASSERT(!aDatabaseId.IsEmpty());

  MOZ_COUNT_CTOR(ConnectionPool::DatabaseInfo);
}

ConnectionPool::DatabaseInfo::~DatabaseInfo() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mConnection);
  MOZ_ASSERT(mScheduledWriteTransactions.IsEmpty());
  MOZ_ASSERT(!mRunningWriteTransaction);
  mThreadInfo.AssertEmpty();
  MOZ_ASSERT(!TotalTransactionCount());

  MOZ_COUNT_DTOR(ConnectionPool::DatabaseInfo);
}

ConnectionPool::DatabasesCompleteCallback::DatabasesCompleteCallback(
    nsTArray<nsCString>&& aDatabaseIds, nsIRunnable* aCallback)
    : mDatabaseIds(std::move(aDatabaseIds)), mCallback(aCallback) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mDatabaseIds.IsEmpty());
  MOZ_ASSERT(aCallback);

  MOZ_COUNT_CTOR(ConnectionPool::DatabasesCompleteCallback);
}

ConnectionPool::DatabasesCompleteCallback::~DatabasesCompleteCallback() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::DatabasesCompleteCallback);
}

ConnectionPool::FinishCallbackWrapper::FinishCallbackWrapper(
    ConnectionPool* aConnectionPool, uint64_t aTransactionId,
    FinishCallback* aCallback)
    : Runnable("dom::indexedDB::ConnectionPool::FinishCallbackWrapper"),
      mConnectionPool(aConnectionPool),
      mCallback(aCallback),
      mOwningEventTarget(GetCurrentEventTarget()),
      mTransactionId(aTransactionId),
      mHasRunOnce(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aConnectionPool);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(mOwningEventTarget);
}

ConnectionPool::FinishCallbackWrapper::~FinishCallbackWrapper() {
  MOZ_ASSERT(!mConnectionPool);
  MOZ_ASSERT(!mCallback);
}

nsresult ConnectionPool::FinishCallbackWrapper::Run() {
  MOZ_ASSERT(mConnectionPool);
  MOZ_ASSERT(mCallback);
  MOZ_ASSERT(mOwningEventTarget);

  AUTO_PROFILER_LABEL("ConnectionPool::FinishCallbackWrapper::Run", DOM);

  if (!mHasRunOnce) {
    MOZ_ASSERT(!IsOnBackgroundThread());

    mHasRunOnce = true;

    Unused << mCallback->Run();

    MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));

    return NS_OK;
  }

  mConnectionPool->AssertIsOnOwningThread();
  MOZ_ASSERT(mHasRunOnce);

  RefPtr<ConnectionPool> connectionPool = std::move(mConnectionPool);
  RefPtr<FinishCallback> callback = std::move(mCallback);

  callback->TransactionFinishedBeforeUnblock();

  connectionPool->NoteFinishedTransaction(mTransactionId);

  callback->TransactionFinishedAfterUnblock();

  return NS_OK;
}

uint32_t ConnectionPool::ThreadRunnable::sNextSerialNumber = 0;

ConnectionPool::ThreadRunnable::ThreadRunnable()
    : Runnable("dom::indexedDB::ConnectionPool::ThreadRunnable"),
      mSerialNumber(++sNextSerialNumber) {
  AssertIsOnBackgroundThread();
}

ConnectionPool::ThreadRunnable::~ThreadRunnable() {
  MOZ_ASSERT(!mFirstRun);
  MOZ_ASSERT(!mContinueRunning);
}

nsresult ConnectionPool::ThreadRunnable::Run() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mContinueRunning);

  if (!mFirstRun) {
    mContinueRunning.Flip();
    return NS_OK;
  }

  mFirstRun.Flip();

  {
    // Scope for the profiler label.
    AUTO_PROFILER_LABEL("ConnectionPool::ThreadRunnable::Run", DOM);

    DebugOnly<nsIThread*> currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

#ifdef DEBUG
    if (kDEBUGTransactionThreadPriority !=
        nsISupportsPriority::PRIORITY_NORMAL) {
      NS_WARNING(
          "ConnectionPool thread debugging enabled, priority has been "
          "modified!");

      nsCOMPtr<nsISupportsPriority> thread = do_QueryInterface(currentThread);
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_SUCCEEDS(thread->SetPriority(kDEBUGTransactionThreadPriority));
    }

    if (kDEBUGTransactionThreadSleepMS) {
      NS_WARNING(
          "TransactionThreadPool thread debugging enabled, sleeping "
          "after every event!");
    }
#endif  // DEBUG

    DebugOnly<bool> b = SpinEventLoopUntil([&]() -> bool {
      if (!mContinueRunning) {
        return true;
      }

#ifdef DEBUG
      if (kDEBUGTransactionThreadSleepMS) {
        MOZ_ALWAYS_TRUE(PR_Sleep(PR_MillisecondsToInterval(
                            kDEBUGTransactionThreadSleepMS)) == PR_SUCCESS);
      }
#endif  // DEBUG

      return false;
    });
    // MSVC can't stringify lambdas, so we have to separate the expression
    // generating the value from the assert itself.
#if DEBUG
    MOZ_ALWAYS_TRUE(b);
#endif
  }

  return NS_OK;
}

ConnectionPool::ThreadInfo::ThreadInfo() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_CTOR(ConnectionPool::ThreadInfo);
}

ConnectionPool::ThreadInfo::ThreadInfo(ThreadInfo&& aOther) noexcept
    : mThread(std::move(aOther.mThread)),
      mRunnable(std::move(aOther.mRunnable)) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mRunnable);

  MOZ_COUNT_CTOR(ConnectionPool::ThreadInfo);
}

ConnectionPool::ThreadInfo::~ThreadInfo() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::ThreadInfo);
}

ConnectionPool::IdleResource::IdleResource(const TimeStamp& aIdleTime)
    : mIdleTime(aIdleTime) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!aIdleTime.IsNull());

  MOZ_COUNT_CTOR(ConnectionPool::IdleResource);
}

ConnectionPool::IdleResource::~IdleResource() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::IdleResource);
}

ConnectionPool::IdleDatabaseInfo::IdleDatabaseInfo(DatabaseInfo& aDatabaseInfo)
    : IdleResource(
          TimeStamp::NowLoRes() +
          (aDatabaseInfo.mIdle
               ? TimeDuration::FromMilliseconds(kConnectionIdleMaintenanceMS)
               : TimeDuration::FromMilliseconds(kConnectionIdleCloseMS))),
      mDatabaseInfo(WrapNotNullUnchecked(&aDatabaseInfo)) {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_CTOR(ConnectionPool::IdleDatabaseInfo);
}

ConnectionPool::IdleDatabaseInfo::~IdleDatabaseInfo() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::IdleDatabaseInfo);
}

ConnectionPool::IdleThreadInfo::IdleThreadInfo(ThreadInfo aThreadInfo)
    : IdleResource(TimeStamp::NowLoRes() +
                   TimeDuration::FromMilliseconds(kConnectionThreadIdleMS)),
      mThreadInfo(std::move(aThreadInfo)) {
  AssertIsOnBackgroundThread();
  mThreadInfo.AssertValid();

  MOZ_COUNT_CTOR(ConnectionPool::IdleThreadInfo);
}

ConnectionPool::IdleThreadInfo::~IdleThreadInfo() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::IdleThreadInfo);
}

ConnectionPool::TransactionInfo::TransactionInfo(
    DatabaseInfo& aDatabaseInfo, const nsID& aBackgroundChildLoggingId,
    const nsACString& aDatabaseId, uint64_t aTransactionId,
    int64_t aLoggingSerialNumber, const nsTArray<nsString>& aObjectStoreNames,
    bool aIsWriteTransaction, TransactionDatabaseOperationBase* aTransactionOp)
    : mDatabaseInfo(aDatabaseInfo),
      mBackgroundChildLoggingId(aBackgroundChildLoggingId),
      mDatabaseId(aDatabaseId),
      mTransactionId(aTransactionId),
      mLoggingSerialNumber(aLoggingSerialNumber),
      mObjectStoreNames(aObjectStoreNames.Clone()),
      mIsWriteTransaction(aIsWriteTransaction),
      mRunning(false) {
  AssertIsOnBackgroundThread();
  aDatabaseInfo.mConnectionPool->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(ConnectionPool::TransactionInfo);

  if (aTransactionOp) {
    mQueuedRunnables.AppendElement(aTransactionOp);
  }
}

ConnectionPool::TransactionInfo::~TransactionInfo() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mBlockedOn.Count());
  MOZ_ASSERT(mQueuedRunnables.IsEmpty());
  MOZ_ASSERT(!mRunning);
  MOZ_ASSERT(mFinished);

  MOZ_COUNT_DTOR(ConnectionPool::TransactionInfo);
}

void ConnectionPool::TransactionInfo::AddBlockingTransaction(
    TransactionInfo& aTransactionInfo) {
  AssertIsOnBackgroundThread();

  // XXX Does it really make sense to have both mBlocking and mBlockingOrdered,
  // just to reduce the algorithmic complexity of this Contains check? This was
  // mentioned in the context of Bug 1290853, but no real justification was
  // given. There was the suggestion of encapsulating this in an
  // insertion-ordered hashtable implementation, which seems like a good idea.
  // If we had that, this would be the appropriate data structure to use here.
  if (mBlocking.EnsureInserted(&aTransactionInfo)) {
    mBlockingOrdered.AppendElement(WrapNotNullUnchecked(&aTransactionInfo));
  }
}

void ConnectionPool::TransactionInfo::RemoveBlockingTransactions() {
  AssertIsOnBackgroundThread();

  for (const auto blockedInfo : mBlockingOrdered) {
    blockedInfo->MaybeUnblock(*this);
  }

  mBlocking.Clear();
  mBlockingOrdered.Clear();
}

void ConnectionPool::TransactionInfo::MaybeUnblock(
    TransactionInfo& aTransactionInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mBlockedOn.Contains(&aTransactionInfo));

  mBlockedOn.RemoveEntry(&aTransactionInfo);
  if (mBlockedOn.IsEmpty()) {
    ConnectionPool* connectionPool = mDatabaseInfo.mConnectionPool;
    MOZ_ASSERT(connectionPool);
    connectionPool->AssertIsOnOwningThread();

    Unused << connectionPool->ScheduleTransaction(
        *this,
        /* aFromQueuedTransactions */ false);
  }
}

#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING)
ConnectionPool::TransactionInfoPair::TransactionInfoPair() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_CTOR(ConnectionPool::TransactionInfoPair);
}

ConnectionPool::TransactionInfoPair::~TransactionInfoPair() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(ConnectionPool::TransactionInfoPair);
}
#endif

/*******************************************************************************
 * Metadata classes
 ******************************************************************************/

bool FullObjectStoreMetadata::HasLiveIndexes() const {
  AssertIsOnBackgroundThread();

  return std::any_of(mIndexes.cbegin(), mIndexes.cend(), [](const auto& entry) {
    return !entry.GetData()->mDeleted;
  });
}

SafeRefPtr<FullDatabaseMetadata> FullDatabaseMetadata::Duplicate() const {
  AssertIsOnBackgroundThread();

  // FullDatabaseMetadata contains two hash tables of pointers that we need to
  // duplicate so we can't just use the copy constructor.
  auto newMetadata = MakeSafeRefPtr<FullDatabaseMetadata>(mCommonMetadata);

  newMetadata->mDatabaseId = mDatabaseId;
  newMetadata->mFilePath = mFilePath;
  newMetadata->mNextObjectStoreId = mNextObjectStoreId;
  newMetadata->mNextIndexId = mNextIndexId;

  for (const auto& objectStoreEntry : mObjectStores) {
    const auto& objectStoreValue = objectStoreEntry.GetData();

    auto newOSMetadata = MakeRefPtr<FullObjectStoreMetadata>();

    newOSMetadata->mCommonMetadata = objectStoreValue->mCommonMetadata;
    newOSMetadata->mNextAutoIncrementId =
        objectStoreValue->mNextAutoIncrementId;
    newOSMetadata->mCommittedAutoIncrementId =
        objectStoreValue->mCommittedAutoIncrementId;

    for (const auto& indexEntry : objectStoreValue->mIndexes) {
      const auto& value = indexEntry.GetData();

      auto newIndexMetadata = MakeRefPtr<FullIndexMetadata>();

      newIndexMetadata->mCommonMetadata = value->mCommonMetadata;

      if (NS_WARN_IF(!newOSMetadata->mIndexes.InsertOrUpdate(
              indexEntry.GetKey(), std::move(newIndexMetadata), fallible))) {
        return nullptr;
      }
    }

    MOZ_ASSERT(objectStoreValue->mIndexes.Count() ==
               newOSMetadata->mIndexes.Count());

    if (NS_WARN_IF(!newMetadata->mObjectStores.InsertOrUpdate(
            objectStoreEntry.GetKey(), std::move(newOSMetadata), fallible))) {
      return nullptr;
    }
  }

  MOZ_ASSERT(mObjectStores.Count() == newMetadata->mObjectStores.Count());

  return newMetadata;
}

DatabaseLoggingInfo::~DatabaseLoggingInfo() {
  AssertIsOnBackgroundThread();

  if (gLoggingInfoHashtable) {
    const nsID& backgroundChildLoggingId =
        mLoggingInfo.backgroundChildLoggingId();

    MOZ_ASSERT(gLoggingInfoHashtable->Get(backgroundChildLoggingId) == this);

    gLoggingInfoHashtable->Remove(backgroundChildLoggingId);
  }
}

/*******************************************************************************
 * Factory
 ******************************************************************************/

Factory::Factory(RefPtr<DatabaseLoggingInfo> aLoggingInfo)
    : mLoggingInfo(std::move(aLoggingInfo))
#ifdef DEBUG
      ,
      mActorDestroyed(false)
#endif
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
}

Factory::~Factory() { MOZ_ASSERT(mActorDestroyed); }

// static
SafeRefPtr<Factory> Factory::Create(const LoggingInfo& aLoggingInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // Balanced in ActoryDestroy().
  IncreaseBusyCount();

  MOZ_ASSERT(gLoggingInfoHashtable);
  RefPtr<DatabaseLoggingInfo> loggingInfo =
      gLoggingInfoHashtable->WithEntryHandle(
          aLoggingInfo.backgroundChildLoggingId(), [&](auto&& entry) {
            if (entry) {
              [[maybe_unused]] const auto& loggingInfo = entry.Data();
              MOZ_ASSERT(aLoggingInfo.backgroundChildLoggingId() ==
                         loggingInfo->Id());
#if !DISABLE_ASSERTS_FOR_FUZZING
              NS_WARNING_ASSERTION(
                  aLoggingInfo.nextTransactionSerialNumber() ==
                      loggingInfo->mLoggingInfo.nextTransactionSerialNumber(),
                  "NextTransactionSerialNumber doesn't match!");
              NS_WARNING_ASSERTION(
                  aLoggingInfo.nextVersionChangeTransactionSerialNumber() ==
                      loggingInfo->mLoggingInfo
                          .nextVersionChangeTransactionSerialNumber(),
                  "NextVersionChangeTransactionSerialNumber doesn't match!");
              NS_WARNING_ASSERTION(
                  aLoggingInfo.nextRequestSerialNumber() ==
                      loggingInfo->mLoggingInfo.nextRequestSerialNumber(),
                  "NextRequestSerialNumber doesn't match!");
#endif  // !DISABLE_ASSERTS_FOR_FUZZING
            } else {
              entry.Insert(new DatabaseLoggingInfo(aLoggingInfo));
            }

            return do_AddRef(entry.Data());
          });

  return MakeSafeRefPtr<Factory>(std::move(loggingInfo));
}

void Factory::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorDestroyed = true;
#endif

  // Match the IncreaseBusyCount in Create().
  DecreaseBusyCount();
}

mozilla::ipc::IPCResult Factory::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBFactoryParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

PBackgroundIDBFactoryRequestParent*
Factory::AllocPBackgroundIDBFactoryRequestParent(
    const FactoryRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != FactoryRequestParams::T__None);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  const CommonFactoryRequestParams* commonParams;

  switch (aParams.type()) {
    case FactoryRequestParams::TOpenDatabaseRequestParams: {
      const OpenDatabaseRequestParams& params =
          aParams.get_OpenDatabaseRequestParams();
      commonParams = &params.commonParams();
      break;
    }

    case FactoryRequestParams::TDeleteDatabaseRequestParams: {
      const DeleteDatabaseRequestParams& params =
          aParams.get_DeleteDatabaseRequestParams();
      commonParams = &params.commonParams();
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(commonParams);

  const DatabaseMetadata& metadata = commonParams->metadata();
  if (NS_WARN_IF(!IsValidPersistenceType(metadata.persistenceType()))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  const PrincipalInfo& principalInfo = commonParams->principalInfo();
  if (NS_WARN_IF(principalInfo.type() == PrincipalInfo::TNullPrincipalInfo)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo &&
                 metadata.persistenceType() != PERSISTENCE_TYPE_PERSISTENT)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(!QuotaManager::IsPrincipalInfoValid(principalInfo))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<ContentParent> contentParent =
      BackgroundParent::GetContentParent(Manager());

  auto actor = [&]() -> RefPtr<FactoryOp> {
    if (aParams.type() == FactoryRequestParams::TOpenDatabaseRequestParams) {
      return MakeRefPtr<OpenDatabaseOp>(
          SafeRefPtrFromThis(), std::move(contentParent), *commonParams);
    } else {
      return MakeRefPtr<DeleteDatabaseOp>(
          SafeRefPtrFromThis(), std::move(contentParent), *commonParams);
    }
  }();

  gFactoryOps->AppendElement(actor);

  // Balanced in CleanupMetadata() which is/must always called by SendResults().
  IncreaseBusyCount();

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Factory::RecvPBackgroundIDBFactoryRequestConstructor(
    PBackgroundIDBFactoryRequestParent* aActor,
    const FactoryRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != FactoryRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  auto* op = static_cast<FactoryOp*>(aActor);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(op));
  return IPC_OK();
}

bool Factory::DeallocPBackgroundIDBFactoryRequestParent(
    PBackgroundIDBFactoryRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<FactoryOp> op = dont_AddRef(static_cast<FactoryOp*>(aActor));
  return true;
}

PBackgroundIDBDatabaseParent* Factory::AllocPBackgroundIDBDatabaseParent(
    const DatabaseSpec& aSpec, PBackgroundIDBFactoryRequestParent* aRequest) {
  MOZ_CRASH(
      "PBackgroundIDBDatabaseParent actors should be constructed "
      "manually!");
}

bool Factory::DeallocPBackgroundIDBDatabaseParent(
    PBackgroundIDBDatabaseParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Database> database = dont_AddRef(static_cast<Database*>(aActor));
  return true;
}

/*******************************************************************************
 * WaitForTransactionsHelper
 ******************************************************************************/

void WaitForTransactionsHelper::WaitForTransactions() {
  MOZ_ASSERT(mState == State::Initial);

  Unused << this->Run();
}

void WaitForTransactionsHelper::MaybeWaitForTransactions() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial);

  RefPtr<ConnectionPool> connectionPool = gConnectionPool.get();
  if (connectionPool) {
    mState = State::WaitingForTransactions;

    connectionPool->WaitForDatabasesToComplete(nsTArray<nsCString>{mDatabaseId},
                                               this);
    return;
  }

  MaybeWaitForFileHandles();
}

void WaitForTransactionsHelper::MaybeWaitForFileHandles() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial ||
             mState == State::WaitingForTransactions);

  RefPtr<FileHandleThreadPool> fileHandleThreadPool =
      gFileHandleThreadPool.get();
  if (fileHandleThreadPool) {
    mState = State::WaitingForFileHandles;

    fileHandleThreadPool->WaitForDirectoriesToComplete(
        nsTArray<nsCString>{mDatabaseId}, this);
    return;
  }

  CallCallback();
}

void WaitForTransactionsHelper::CallCallback() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial ||
             mState == State::WaitingForTransactions ||
             mState == State::WaitingForFileHandles);

  const nsCOMPtr<nsIRunnable> callback = std::move(mCallback);

  callback->Run();

  mState = State::Complete;
}

NS_IMETHODIMP
WaitForTransactionsHelper::Run() {
  MOZ_ASSERT(mState != State::Complete);
  MOZ_ASSERT(mCallback);

  switch (mState) {
    case State::Initial:
      MaybeWaitForTransactions();
      break;

    case State::WaitingForTransactions:
      MaybeWaitForFileHandles();
      break;

    case State::WaitingForFileHandles:
      CallCallback();
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

/*******************************************************************************
 * Database
 ******************************************************************************/

Database::Database(
    SafeRefPtr<Factory> aFactory, const PrincipalInfo& aPrincipalInfo,
    const Maybe<ContentParentId>& aOptionalContentParentId,
    const quota::OriginMetadata& aOriginMetadata, uint32_t aTelemetryId,
    SafeRefPtr<FullDatabaseMetadata> aMetadata,
    SafeRefPtr<FileManager> aFileManager, RefPtr<DirectoryLock> aDirectoryLock,
    bool aFileHandleDisabled, bool aChromeWriteAccessAllowed,
    bool aInPrivateBrowsing, const Maybe<const CipherKey>& aMaybeKey)
    : mFactory(std::move(aFactory)),
      mMetadata(std::move(aMetadata)),
      mFileManager(std::move(aFileManager)),
      mDirectoryLock(std::move(aDirectoryLock)),
      mPrincipalInfo(aPrincipalInfo),
      mOptionalContentParentId(aOptionalContentParentId),
      mOriginMetadata(aOriginMetadata),
      mId(mMetadata->mDatabaseId),
      mFilePath(mMetadata->mFilePath),
      mKey(aMaybeKey),
      mActiveMutableFileCount(0),
      mPendingCreateFileOpCount(0),
      mTelemetryId(aTelemetryId),
      mPersistenceType(mMetadata->mCommonMetadata.persistenceType()),
      mFileHandleDisabled(aFileHandleDisabled),
      mChromeWriteAccessAllowed(aChromeWriteAccessAllowed),
      mInPrivateBrowsing(aInPrivateBrowsing),
      mBackgroundThread(GetCurrentEventTarget())
#ifdef DEBUG
      ,
      mAllBlobsUnmapped(false)
#endif
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mFactory);
  MOZ_ASSERT(mMetadata);
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT_IF(aChromeWriteAccessAllowed,
                aPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo);

  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mDirectoryLock->Id() >= 0);
  mDirectoryLockId = mDirectoryLock->Id();
}

template <typename T>
bool Database::InvalidateAll(const nsTHashtable<nsPtrHashKey<T>>& aTable) {
  AssertIsOnBackgroundThread();

  const uint32_t count = aTable.Count();
  if (!count) {
    return true;
  }

  IDB_TRY_INSPECT(
      const auto& elementsToInvalidate,
      TransformIntoNewArray(
          aTable, [](const auto& entry) { return entry.GetKey(); }, fallible),
      false);

  IDB_REPORT_INTERNAL_ERR();

  for (const auto& elementToInvalidate : elementsToInvalidate) {
    MOZ_ASSERT(elementToInvalidate);

    elementToInvalidate->Invalidate();
  }

  return true;
}

void Database::Invalidate() {
  AssertIsOnBackgroundThread();

  if (mInvalidated) {
    return;
  }

  mInvalidated.Flip();

  if (mActorWasAlive && !mActorDestroyed) {
    Unused << SendInvalidate();
  }

  if (!InvalidateAll(mTransactions)) {
    NS_WARNING("Failed to abort all transactions!");
  }

  if (!InvalidateAll(mMutableFiles)) {
    NS_WARNING("Failed to abort all mutable files!");
  }

  MOZ_ALWAYS_TRUE(CloseInternal());
}

nsresult Database::EnsureConnection() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  AUTO_PROFILER_LABEL("Database::EnsureConnection", DOM);

  if (!mConnection || !mConnection->HasStorageConnection()) {
    IDB_TRY_UNWRAP(mConnection, gConnectionPool->GetOrCreateConnection(*this));
  }

  AssertIsOnConnectionThread();

  return NS_OK;
}

bool Database::RegisterTransaction(TransactionBase& aTransaction) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mTransactions.GetEntry(&aTransaction));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mInvalidated);
  MOZ_ASSERT(!mClosed);

  if (NS_WARN_IF(!mTransactions.PutEntry(&aTransaction, fallible))) {
    return false;
  }

  return true;
}

void Database::UnregisterTransaction(TransactionBase& aTransaction) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransactions.GetEntry(&aTransaction));

  mTransactions.RemoveEntry(&aTransaction);

  MaybeCloseConnection();
}

bool Database::RegisterMutableFile(MutableFile* aMutableFile) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aMutableFile);
  MOZ_ASSERT(!mMutableFiles.GetEntry(aMutableFile));
  MOZ_ASSERT(mDirectoryLock);

  if (NS_WARN_IF(!mMutableFiles.PutEntry(aMutableFile, fallible))) {
    return false;
  }

  return true;
}

void Database::UnregisterMutableFile(MutableFile* aMutableFile) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aMutableFile);
  MOZ_ASSERT(mMutableFiles.GetEntry(aMutableFile));

  mMutableFiles.RemoveEntry(aMutableFile);
}

void Database::NoteActiveMutableFile() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mActiveMutableFileCount < UINT32_MAX);

  ++mActiveMutableFileCount;
}

void Database::NoteInactiveMutableFile() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveMutableFileCount > 0);

  --mActiveMutableFileCount;

  MaybeCloseConnection();
}

void Database::NotePendingCreateFileOp() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mPendingCreateFileOpCount < UINT32_MAX);

  ++mPendingCreateFileOpCount;
}

void Database::NoteCompletedCreateFileOp() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mPendingCreateFileOpCount > 0);

  --mPendingCreateFileOpCount;

  MaybeCloseConnection();
}

void Database::SetActorAlive() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorWasAlive.Flip();

  // This reference will be absorbed by IPDL and released when the actor is
  // destroyed.
  AddRef();
}

void Database::MapBlob(const IPCBlob& aIPCBlob,
                       SafeRefPtr<FileInfo> aFileInfo) {
  AssertIsOnBackgroundThread();

  const RemoteLazyStream& stream = aIPCBlob.inputStream();
  MOZ_ASSERT(stream.type() == RemoteLazyStream::TPRemoteLazyInputStreamParent);

  RemoteLazyInputStreamParent* actor =
      static_cast<RemoteLazyInputStreamParent*>(
          stream.get_PRemoteLazyInputStreamParent());

  MOZ_ASSERT(!mMappedBlobs.GetWeak(actor->ID()));
  mMappedBlobs.InsertOrUpdate(actor->ID(), AsRefPtr(std::move(aFileInfo)));

  RefPtr<UnmapBlobCallback> callback =
      new UnmapBlobCallback(SafeRefPtrFromThis());
  actor->SetCallback(callback);
}

void Database::Stringify(nsACString& aResult) const {
  AssertIsOnBackgroundThread();

  constexpr auto kQuotaGenericDelimiterString = "|"_ns;

  aResult.Append(
      "DirectoryLock:"_ns + IntToCString(!!mDirectoryLock) +
      kQuotaGenericDelimiterString +
      //
      "Transactions:"_ns + IntToCString(mTransactions.Count()) +
      kQuotaGenericDelimiterString +
      //
      "OtherProcessActor:"_ns +
      IntToCString(
          BackgroundParent::IsOtherProcessActor(GetBackgroundParent())) +
      kQuotaGenericDelimiterString +
      //
      "Origin:"_ns + AnonymizedOriginString(mOriginMetadata.mOrigin) +
      kQuotaGenericDelimiterString +
      //
      "PersistenceType:"_ns + PersistenceTypeToString(mPersistenceType) +
      kQuotaGenericDelimiterString +
      //
      "Closed:"_ns + IntToCString(static_cast<bool>(mClosed)) +
      kQuotaGenericDelimiterString +
      //
      "Invalidated:"_ns + IntToCString(static_cast<bool>(mInvalidated)) +
      kQuotaGenericDelimiterString +
      //
      "ActorWasAlive:"_ns + IntToCString(static_cast<bool>(mActorWasAlive)) +
      kQuotaGenericDelimiterString +
      //
      "ActorDestroyed:"_ns + IntToCString(static_cast<bool>(mActorDestroyed)));
}

SafeRefPtr<FileInfo> Database::GetBlob(const IPCBlob& aIPCBlob) {
  AssertIsOnBackgroundThread();

  const RemoteLazyStream& stream = aIPCBlob.inputStream();
  MOZ_ASSERT(stream.type() == RemoteLazyStream::TIPCStream);

  const IPCStream& ipcStream = stream.get_IPCStream();

  const InputStreamParams& inputStreamParams = ipcStream.stream();
  if (inputStreamParams.type() !=
      InputStreamParams::TRemoteLazyInputStreamParams) {
    return nullptr;
  }

  const RemoteLazyInputStreamParams& ipcBlobInputStreamParams =
      inputStreamParams.get_RemoteLazyInputStreamParams();
  if (ipcBlobInputStreamParams.type() !=
      RemoteLazyInputStreamParams::TRemoteLazyInputStreamRef) {
    return nullptr;
  }

  const nsID& id = ipcBlobInputStreamParams.get_RemoteLazyInputStreamRef().id();

  RefPtr<FileInfo> fileInfo;
  if (!mMappedBlobs.Get(id, getter_AddRefs(fileInfo))) {
    return nullptr;
  }

  return SafeRefPtr{std::move(fileInfo)};
}

void Database::UnmapBlob(const nsID& aID) {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT_IF(!mAllBlobsUnmapped, mMappedBlobs.GetWeak(aID));
  mMappedBlobs.Remove(aID);
}

void Database::UnmapAllBlobs() {
  AssertIsOnBackgroundThread();

#ifdef DEBUG
  mAllBlobsUnmapped = true;
#endif

  mMappedBlobs.Clear();
}

bool Database::CloseInternal() {
  AssertIsOnBackgroundThread();

  if (mClosed) {
    if (NS_WARN_IF(!IsInvalidated())) {
      // Kill misbehaving child for sending the close message twice.
      return false;
    }

    // Ignore harmless race when we just invalidated the database.
    return true;
  }

  mClosed.Flip();

  if (gConnectionPool) {
    gConnectionPool->CloseDatabaseWhenIdle(Id());
  }

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));

  MOZ_ASSERT(info->mLiveDatabases.Contains(this));

  if (info->mWaitingFactoryOp) {
    info->mWaitingFactoryOp->NoteDatabaseClosed(this);
  }

  MaybeCloseConnection();

  return true;
}

void Database::MaybeCloseConnection() {
  AssertIsOnBackgroundThread();

  if (!mTransactions.Count() && !mActiveMutableFileCount &&
      !mPendingCreateFileOpCount && IsClosed() && mDirectoryLock) {
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("dom::indexedDB::Database::ConnectionClosedCallback",
                          this, &Database::ConnectionClosedCallback);

    RefPtr<WaitForTransactionsHelper> helper =
        new WaitForTransactionsHelper(Id(), callback);
    helper->WaitForTransactions();
  }
}

void Database::ConnectionClosedCallback() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mClosed);
  MOZ_ASSERT(!mTransactions.Count());
  MOZ_ASSERT(!mActiveMutableFileCount);
  MOZ_ASSERT(!mPendingCreateFileOpCount);

  mDirectoryLock = nullptr;

  CleanupMetadata();

  UnmapAllBlobs();

  if (IsInvalidated() && IsActorAlive()) {
    // Step 3 and 4 of "5.2 Closing a Database":
    // 1. Wait for all transactions to complete.
    // 2. Fire a close event if forced flag is set, i.e., IsInvalidated() in our
    //    implementation.
    Unused << SendCloseAfterInvalidationComplete();
  }
}

void Database::CleanupMetadata() {
  AssertIsOnBackgroundThread();

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));
  MOZ_ALWAYS_TRUE(info->mLiveDatabases.RemoveElement(this));

  QuotaManager::GetRef().MaybeRecordShutdownStep(
      quota::Client::IDB, "Live database entry removed"_ns);

  if (info->mLiveDatabases.IsEmpty()) {
    MOZ_ASSERT(!info->mWaitingFactoryOp ||
               !info->mWaitingFactoryOp->HasBlockedDatabases());
    gLiveDatabaseHashtable->Remove(Id());

    QuotaManager::GetRef().MaybeRecordShutdownStep(
        quota::Client::IDB, "gLiveDatabaseHashtable entry removed"_ns);
  }

  // Match the IncreaseBusyCount in OpenDatabaseOp::EnsureDatabaseActor().
  DecreaseBusyCount();
}

bool Database::VerifyRequestParams(const DatabaseRequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != DatabaseRequestParams::T__None);

  switch (aParams.type()) {
    case DatabaseRequestParams::TCreateFileParams: {
      if (NS_WARN_IF(mFileHandleDisabled)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const CreateFileParams& params = aParams.get_CreateFileParams();

      if (NS_WARN_IF(params.name().IsEmpty())) {
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

void Database::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  mActorDestroyed.Flip();

  if (!IsInvalidated()) {
    Invalidate();
  }
}

PBackgroundIDBDatabaseFileParent*
Database::AllocPBackgroundIDBDatabaseFileParent(const IPCBlob& aIPCBlob) {
  AssertIsOnBackgroundThread();

  SafeRefPtr<FileInfo> fileInfo = GetBlob(aIPCBlob);
  RefPtr<DatabaseFile> actor;

  if (fileInfo) {
    actor = new DatabaseFile(std::move(fileInfo));
  } else {
    // This is a blob we haven't seen before.
    fileInfo = mFileManager->CreateFileInfo();
    if (NS_WARN_IF(!fileInfo)) {
      return nullptr;
    }

    actor = new DatabaseFile(IPCBlobUtils::Deserialize(aIPCBlob),
                             std::move(fileInfo));
  }

  MOZ_ASSERT(actor);

  return actor.forget().take();
}

bool Database::DeallocPBackgroundIDBDatabaseFileParent(
    PBackgroundIDBDatabaseFileParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<DatabaseFile> actor = dont_AddRef(static_cast<DatabaseFile*>(aActor));
  return true;
}

PBackgroundIDBDatabaseRequestParent*
Database::AllocPBackgroundIDBDatabaseRequestParent(
    const DatabaseRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != DatabaseRequestParams::T__None);

  // TODO: Check here that the database has not been closed?

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  PBackgroundParent* backgroundActor = GetBackgroundParent();
  MOZ_ASSERT(backgroundActor);

  bool trustParams = !BackgroundParent::IsOtherProcessActor(backgroundActor);
#endif

  if (NS_WARN_IF(!trustParams && !VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<DatabaseOp> actor;

  switch (aParams.type()) {
    case DatabaseRequestParams::TCreateFileParams: {
      actor = new CreateFileOp(SafeRefPtrFromThis(), aParams);

      NotePendingCreateFileOp();
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Database::RecvPBackgroundIDBDatabaseRequestConstructor(
    PBackgroundIDBDatabaseRequestParent* aActor,
    const DatabaseRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != DatabaseRequestParams::T__None);

  auto* op = static_cast<DatabaseOp*>(aActor);

  op->RunImmediately();

  return IPC_OK();
}

bool Database::DeallocPBackgroundIDBDatabaseRequestParent(
    PBackgroundIDBDatabaseRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<DatabaseOp> op = dont_AddRef(static_cast<DatabaseOp*>(aActor));
  return true;
}

already_AddRefed<PBackgroundIDBTransactionParent>
Database::AllocPBackgroundIDBTransactionParent(
    const nsTArray<nsString>& aObjectStoreNames, const Mode& aMode) {
  AssertIsOnBackgroundThread();

  // Once a database is closed it must not try to open new transactions.
  if (NS_WARN_IF(mClosed)) {
    if (!mInvalidated) {
      ASSERT_UNLESS_FUZZING();
    }
    return nullptr;
  }

  if (NS_WARN_IF(aObjectStoreNames.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(aMode != IDBTransaction::Mode::ReadOnly &&
                 aMode != IDBTransaction::Mode::ReadWrite &&
                 aMode != IDBTransaction::Mode::ReadWriteFlush &&
                 aMode != IDBTransaction::Mode::Cleanup)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // If this is a readwrite transaction to a chrome database make sure the child
  // has write access.
  if (NS_WARN_IF((aMode == IDBTransaction::Mode::ReadWrite ||
                  aMode == IDBTransaction::Mode::ReadWriteFlush ||
                  aMode == IDBTransaction::Mode::Cleanup) &&
                 mPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo &&
                 !mChromeWriteAccessAllowed)) {
    return nullptr;
  }

  const ObjectStoreTable& objectStores = mMetadata->mObjectStores;
  const uint32_t nameCount = aObjectStoreNames.Length();

  if (NS_WARN_IF(nameCount > objectStores.Count())) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  IDB_TRY_UNWRAP(
      auto objectStoreMetadatas,
      TransformIntoNewArrayAbortOnErr(
          aObjectStoreNames,
          [lastName = Maybe<const nsString&>{},
           &objectStores](const nsString& name) mutable
          -> mozilla::Result<RefPtr<FullObjectStoreMetadata>, nsresult> {
            if (lastName) {
              // Make sure that this name is sorted properly and not a
              // duplicate.
              if (NS_WARN_IF(name <= lastName.ref())) {
                ASSERT_UNLESS_FUZZING();
                return Err(NS_ERROR_FAILURE);
              }
            }
            lastName = SomeRef(name);

            const auto foundIt =
                std::find_if(objectStores.cbegin(), objectStores.cend(),
                             [&name](const auto& entry) {
                               const auto& value = entry.GetData();
                               MOZ_ASSERT(entry.GetKey());
                               return name == value->mCommonMetadata.name() &&
                                      !value->mDeleted;
                             });
            if (foundIt == objectStores.cend()) {
              ASSERT_UNLESS_FUZZING();
              return Err(NS_ERROR_FAILURE);
            }

            return foundIt->GetData();
          },
          fallible),
      nullptr);

  return MakeSafeRefPtr<NormalTransaction>(SafeRefPtrFromThis(), aMode,
                                           std::move(objectStoreMetadatas))
      .forget();
}

mozilla::ipc::IPCResult Database::RecvPBackgroundIDBTransactionConstructor(
    PBackgroundIDBTransactionParent* aActor,
    nsTArray<nsString>&& aObjectStoreNames, const Mode& aMode) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!aObjectStoreNames.IsEmpty());
  MOZ_ASSERT(aMode == IDBTransaction::Mode::ReadOnly ||
             aMode == IDBTransaction::Mode::ReadWrite ||
             aMode == IDBTransaction::Mode::ReadWriteFlush ||
             aMode == IDBTransaction::Mode::Cleanup);
  MOZ_ASSERT(!mClosed);

  if (IsInvalidated()) {
    // This is an expected race. We don't want the child to die here, just don't
    // actually do any work.
    return IPC_OK();
  }

  if (!gConnectionPool) {
    gConnectionPool = new ConnectionPool();
  }

  auto* transaction = static_cast<NormalTransaction*>(aActor);

  RefPtr<StartTransactionOp> startOp = new StartTransactionOp(
      SafeRefPtr{transaction, AcquireStrongRefFromRawPtr{}});

  uint64_t transactionId = startOp->StartOnConnectionPool(
      GetLoggingInfo()->Id(), mMetadata->mDatabaseId,
      transaction->LoggingSerialNumber(), aObjectStoreNames,
      aMode != IDBTransaction::Mode::ReadOnly);

  transaction->Init(transactionId);

  if (NS_WARN_IF(!RegisterTransaction(*transaction))) {
    IDB_REPORT_INTERNAL_ERR();
    transaction->Abort(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, /* aForce */ false);
    return IPC_OK();
  }

  return IPC_OK();
}

Database::PBackgroundMutableFileParent*
Database::AllocPBackgroundMutableFileParent(const nsString& aName,
                                            const nsString& aType) {
  MOZ_CRASH(
      "PBackgroundMutableFileParent actors should be constructed "
      "manually!");
}

bool Database::DeallocPBackgroundMutableFileParent(
    PBackgroundMutableFileParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<MutableFile> mutableFile =
      dont_AddRef(static_cast<MutableFile*>(aActor));
  return true;
}

mozilla::ipc::IPCResult Database::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBDatabaseParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Database::RecvBlocked() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mClosed)) {
    return IPC_FAIL_NO_REASON(this);
  }

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));

  MOZ_ASSERT(info->mLiveDatabases.Contains(this));
  MOZ_ASSERT(info->mWaitingFactoryOp);

  info->mWaitingFactoryOp->NoteDatabaseBlocked(this);

  return IPC_OK();
}

mozilla::ipc::IPCResult Database::RecvClose() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!CloseInternal())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

void Database::StartTransactionOp::RunOnConnectionThread() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!HasFailed());

  IDB_LOG_MARK_PARENT_TRANSACTION("Beginning database work", "DB Start",
                                  IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
                                  mTransactionLoggingSerialNumber);

  TransactionDatabaseOperationBase::RunOnConnectionThread();
}

nsresult Database::StartTransactionOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  Transaction().SetActiveOnConnectionThread();

  if (Transaction().GetMode() == IDBTransaction::Mode::Cleanup) {
    DebugOnly<nsresult> rv = aConnection->DisableQuotaChecks();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "DisableQuotaChecks failed, trying to continue "
                         "cleanup transaction with quota checks enabled");
  }

  if (Transaction().GetMode() != IDBTransaction::Mode::ReadOnly) {
    IDB_TRY(aConnection->BeginWriteTransaction());
  }

  return NS_OK;
}

nsresult Database::StartTransactionOp::SendSuccessResult() {
  // We don't need to do anything here.
  return NS_OK;
}

bool Database::StartTransactionOp::SendFailureResult(
    nsresult /* aResultCode */) {
  IDB_REPORT_INTERNAL_ERR();

  // Abort the transaction.
  return false;
}

void Database::StartTransactionOp::Cleanup() {
#ifdef DEBUG
  // StartTransactionOp is not a normal database operation that is tied to an
  // actor. Do this to make our assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

/*******************************************************************************
 * TransactionBase
 ******************************************************************************/

TransactionBase::TransactionBase(SafeRefPtr<Database> aDatabase, Mode aMode)
    : mDatabase(std::move(aDatabase)),
      mDatabaseId(mDatabase->Id()),
      mLoggingSerialNumber(
          mDatabase->GetLoggingInfo()->NextTransactionSN(aMode)),
      mActiveRequestCount(0),
      mInvalidatedOnAnyThread(false),
      mMode(aMode),
      mResultCode(NS_OK) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mLoggingSerialNumber);
}

TransactionBase::~TransactionBase() {
  MOZ_ASSERT(!mActiveRequestCount);
  MOZ_ASSERT(mActorDestroyed);
  MOZ_ASSERT_IF(mInitialized, mCommittedOrAborted);
}

void TransactionBase::Abort(nsresult aResultCode, bool aForce) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));

  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = aResultCode;
  }

  if (aForce) {
    mForceAborted.EnsureFlipped();
  }

  MaybeCommitOrAbort();
}

bool TransactionBase::RecvCommit(const Maybe<int64_t> aLastRequest) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  mCommitOrAbortReceived.Flip();
  mLastRequestBeforeCommit.init(aLastRequest);

  MaybeCommitOrAbort();
  return true;
}

bool TransactionBase::RecvAbort(nsresult aResultCode) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(NS_SUCCEEDED(aResultCode))) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(NS_ERROR_GET_MODULE(aResultCode) !=
                 NS_ERROR_MODULE_DOM_INDEXEDDB)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  mCommitOrAbortReceived.Flip();

  Abort(aResultCode, /* aForce */ false);
  return true;
}

void TransactionBase::CommitOrAbort() {
  AssertIsOnBackgroundThread();

  mCommittedOrAborted.Flip();

  if (!mInitialized) {
    return;
  }

  // In case of a failed request that was started after committing was
  // initiated, abort (cf.
  // https://w3c.github.io/IndexedDB/#async-execute-request step 5.3 vs. 5.4).
  // Note this can only happen here when we are committing explicitly, otherwise
  // the decision is made by the child.
  if (NS_SUCCEEDED(mResultCode) && mLastFailedRequest &&
      *mLastRequestBeforeCommit &&
      *mLastFailedRequest >= **mLastRequestBeforeCommit) {
    mResultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  RefPtr<CommitOp> commitOp =
      new CommitOp(SafeRefPtrFromThis(), ClampResultCode(mResultCode));

  gConnectionPool->Finish(TransactionId(), commitOp);
}

RefPtr<FullObjectStoreMetadata> TransactionBase::GetMetadataForObjectStoreId(
    IndexOrObjectStoreId aObjectStoreId) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aObjectStoreId);

  if (!aObjectStoreId) {
    return nullptr;
  }

  RefPtr<FullObjectStoreMetadata> metadata;
  if (!mDatabase->Metadata().mObjectStores.Get(aObjectStoreId,
                                               getter_AddRefs(metadata)) ||
      metadata->mDeleted) {
    return nullptr;
  }

  MOZ_ASSERT(metadata->mCommonMetadata.id() == aObjectStoreId);

  return metadata;
}

RefPtr<FullIndexMetadata> TransactionBase::GetMetadataForIndexId(
    FullObjectStoreMetadata* const aObjectStoreMetadata,
    IndexOrObjectStoreId aIndexId) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aIndexId);

  if (!aIndexId) {
    return nullptr;
  }

  RefPtr<FullIndexMetadata> metadata;
  if (!aObjectStoreMetadata->mIndexes.Get(aIndexId, getter_AddRefs(metadata)) ||
      metadata->mDeleted) {
    return nullptr;
  }

  MOZ_ASSERT(metadata->mCommonMetadata.id() == aIndexId);

  return metadata;
}

void TransactionBase::NoteModifiedAutoIncrementObjectStore(
    FullObjectStoreMetadata* aMetadata) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aMetadata);

  if (!mModifiedAutoIncrementObjectStoreMetadataArray.Contains(aMetadata)) {
    mModifiedAutoIncrementObjectStoreMetadataArray.AppendElement(aMetadata);
  }
}

void TransactionBase::ForgetModifiedAutoIncrementObjectStore(
    FullObjectStoreMetadata* aMetadata) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aMetadata);

  mModifiedAutoIncrementObjectStoreMetadataArray.RemoveElement(aMetadata);
}

bool TransactionBase::VerifyRequestParams(const RequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  switch (aParams.type()) {
    case RequestParams::TObjectStoreAddParams: {
      const ObjectStoreAddPutParams& params =
          aParams.get_ObjectStoreAddParams().commonParams();
      if (NS_WARN_IF(!VerifyRequestParams(params))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStorePutParams: {
      const ObjectStoreAddPutParams& params =
          aParams.get_ObjectStorePutParams().commonParams();
      if (NS_WARN_IF(!VerifyRequestParams(params))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreGetParams: {
      const ObjectStoreGetParams& params = aParams.get_ObjectStoreGetParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreGetKeyParams: {
      const ObjectStoreGetKeyParams& params =
          aParams.get_ObjectStoreGetKeyParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreGetAllParams: {
      const ObjectStoreGetAllParams& params =
          aParams.get_ObjectStoreGetAllParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreGetAllKeysParams: {
      const ObjectStoreGetAllKeysParams& params =
          aParams.get_ObjectStoreGetAllKeysParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreDeleteParams: {
      if (NS_WARN_IF(mMode != IDBTransaction::Mode::ReadWrite &&
                     mMode != IDBTransaction::Mode::ReadWriteFlush &&
                     mMode != IDBTransaction::Mode::Cleanup &&
                     mMode != IDBTransaction::Mode::VersionChange)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const ObjectStoreDeleteParams& params =
          aParams.get_ObjectStoreDeleteParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreClearParams: {
      if (NS_WARN_IF(mMode != IDBTransaction::Mode::ReadWrite &&
                     mMode != IDBTransaction::Mode::ReadWriteFlush &&
                     mMode != IDBTransaction::Mode::Cleanup &&
                     mMode != IDBTransaction::Mode::VersionChange)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const ObjectStoreClearParams& params =
          aParams.get_ObjectStoreClearParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TObjectStoreCountParams: {
      const ObjectStoreCountParams& params =
          aParams.get_ObjectStoreCountParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexGetParams: {
      const IndexGetParams& params = aParams.get_IndexGetParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexGetKeyParams: {
      const IndexGetKeyParams& params = aParams.get_IndexGetKeyParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.keyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexGetAllParams: {
      const IndexGetAllParams& params = aParams.get_IndexGetAllParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexGetAllKeysParams: {
      const IndexGetAllKeysParams& params = aParams.get_IndexGetAllKeysParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;
    }

    case RequestParams::TIndexCountParams: {
      const IndexCountParams& params = aParams.get_IndexCountParams();
      const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
          GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const RefPtr<FullIndexMetadata> indexMetadata =
          GetMetadataForIndexId(objectStoreMetadata, params.indexId());
      if (NS_WARN_IF(!indexMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      if (NS_WARN_IF(!VerifyRequestParams(params.optionalKeyRange()))) {
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

bool TransactionBase::VerifyRequestParams(
    const SerializedKeyRange& aParams) const {
  AssertIsOnBackgroundThread();

  // XXX Check more here?

  if (aParams.isOnly()) {
    if (NS_WARN_IF(aParams.lower().IsUnset())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
    if (NS_WARN_IF(!aParams.upper().IsUnset())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
    if (NS_WARN_IF(aParams.lowerOpen())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
    if (NS_WARN_IF(aParams.upperOpen())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  } else if (NS_WARN_IF(aParams.lower().IsUnset() &&
                        aParams.upper().IsUnset())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  return true;
}

bool TransactionBase::VerifyRequestParams(
    const ObjectStoreAddPutParams& aParams) const {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mMode != IDBTransaction::Mode::ReadWrite &&
                 mMode != IDBTransaction::Mode::ReadWriteFlush &&
                 mMode != IDBTransaction::Mode::VersionChange)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  RefPtr<FullObjectStoreMetadata> objMetadata =
      GetMetadataForObjectStoreId(aParams.objectStoreId());
  if (NS_WARN_IF(!objMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(!aParams.cloneInfo().data().data.Size())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (objMetadata->mCommonMetadata.autoIncrement() &&
      objMetadata->mCommonMetadata.keyPath().IsValid() &&
      aParams.key().IsUnset()) {
    const SerializedStructuredCloneWriteInfo& cloneInfo = aParams.cloneInfo();

    if (NS_WARN_IF(!cloneInfo.offsetToKeyProp())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(cloneInfo.data().data.Size() < sizeof(uint64_t))) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(cloneInfo.offsetToKeyProp() >
                   (cloneInfo.data().data.Size() - sizeof(uint64_t)))) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  } else if (NS_WARN_IF(aParams.cloneInfo().offsetToKeyProp())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  for (const auto& updateInfo : aParams.indexUpdateInfos()) {
    RefPtr<FullIndexMetadata> indexMetadata =
        GetMetadataForIndexId(objMetadata, updateInfo.indexId());
    if (NS_WARN_IF(!indexMetadata)) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(updateInfo.value().IsUnset())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    MOZ_ASSERT(!updateInfo.value().GetBuffer().IsEmpty());
  }

  for (const FileAddInfo& fileAddInfo : aParams.fileAddInfos()) {
    const DatabaseOrMutableFile& file = fileAddInfo.file();
    MOZ_ASSERT(file.type() != DatabaseOrMutableFile::T__None);

    switch (fileAddInfo.type()) {
      case StructuredCloneFileBase::eBlob:
        if (NS_WARN_IF(
                file.type() !=
                DatabaseOrMutableFile::TPBackgroundIDBDatabaseFileParent)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
        if (NS_WARN_IF(!file.get_PBackgroundIDBDatabaseFileParent())) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
        break;

      case StructuredCloneFileBase::eMutableFile: {
        if (NS_WARN_IF(file.type() !=
                       DatabaseOrMutableFile::TPBackgroundMutableFileParent)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        if (NS_WARN_IF(mDatabase->IsFileHandleDisabled())) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        auto mutableFile =
            static_cast<MutableFile*>(file.get_PBackgroundMutableFileParent());

        if (NS_WARN_IF(!mutableFile)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        const Database& database = mutableFile->GetDatabase();
        if (NS_WARN_IF(database.Id() != mDatabase->Id())) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }

        break;
      }

      case StructuredCloneFileBase::eStructuredClone:
      case StructuredCloneFileBase::eWasmBytecode:
      case StructuredCloneFileBase::eWasmCompiled:
      case StructuredCloneFileBase::eEndGuard:
        ASSERT_UNLESS_FUZZING();
        return false;

      default:
        MOZ_CRASH("Should never get here!");
    }
  }

  return true;
}

bool TransactionBase::VerifyRequestParams(
    const Maybe<SerializedKeyRange>& aParams) const {
  AssertIsOnBackgroundThread();

  if (aParams.isSome()) {
    if (NS_WARN_IF(!VerifyRequestParams(aParams.ref()))) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  }

  return true;
}

void TransactionBase::NoteActiveRequest() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveRequestCount < UINT64_MAX);

  mActiveRequestCount++;
}

void TransactionBase::NoteFinishedRequest(const int64_t aRequestId,
                                          const nsresult aResultCode) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveRequestCount);

  mActiveRequestCount--;

  if (NS_FAILED(aResultCode)) {
    mLastFailedRequest = Some(aRequestId);
  }

  MaybeCommitOrAbort();
}

void TransactionBase::Invalidate() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mInvalidated == mInvalidatedOnAnyThread);

  if (!mInvalidated) {
    mInvalidated.Flip();
    mInvalidatedOnAnyThread = true;

    Abort(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR, /* aForce */ false);
  }
}

PBackgroundIDBRequestParent* TransactionBase::AllocRequest(
    RequestParams&& aParams, bool aTrustParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  aTrustParams = false;
#endif

  if (!aTrustParams && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<NormalTransactionOp> actor;

  switch (aParams.type()) {
    case RequestParams::TObjectStoreAddParams:
    case RequestParams::TObjectStorePutParams:
      actor = new ObjectStoreAddOrPutRequestOp(SafeRefPtrFromThis(),
                                               std::move(aParams));
      break;

    case RequestParams::TObjectStoreGetParams:
      actor = new ObjectStoreGetRequestOp(SafeRefPtrFromThis(), aParams,
                                          /* aGetAll */ false);
      break;

    case RequestParams::TObjectStoreGetAllParams:
      actor = new ObjectStoreGetRequestOp(SafeRefPtrFromThis(), aParams,
                                          /* aGetAll */ true);
      break;

    case RequestParams::TObjectStoreGetKeyParams:
      actor = new ObjectStoreGetKeyRequestOp(SafeRefPtrFromThis(), aParams,
                                             /* aGetAll */ false);
      break;

    case RequestParams::TObjectStoreGetAllKeysParams:
      actor = new ObjectStoreGetKeyRequestOp(SafeRefPtrFromThis(), aParams,
                                             /* aGetAll */ true);
      break;

    case RequestParams::TObjectStoreDeleteParams:
      actor = new ObjectStoreDeleteRequestOp(
          SafeRefPtrFromThis(), aParams.get_ObjectStoreDeleteParams());
      break;

    case RequestParams::TObjectStoreClearParams:
      actor = new ObjectStoreClearRequestOp(
          SafeRefPtrFromThis(), aParams.get_ObjectStoreClearParams());
      break;

    case RequestParams::TObjectStoreCountParams:
      actor = new ObjectStoreCountRequestOp(
          SafeRefPtrFromThis(), aParams.get_ObjectStoreCountParams());
      break;

    case RequestParams::TIndexGetParams:
      actor = new IndexGetRequestOp(SafeRefPtrFromThis(), aParams,
                                    /* aGetAll */ false);
      break;

    case RequestParams::TIndexGetKeyParams:
      actor = new IndexGetKeyRequestOp(SafeRefPtrFromThis(), aParams,
                                       /* aGetAll */ false);
      break;

    case RequestParams::TIndexGetAllParams:
      actor = new IndexGetRequestOp(SafeRefPtrFromThis(), aParams,
                                    /* aGetAll */ true);
      break;

    case RequestParams::TIndexGetAllKeysParams:
      actor = new IndexGetKeyRequestOp(SafeRefPtrFromThis(), aParams,
                                       /* aGetAll */ true);
      break;

    case RequestParams::TIndexCountParams:
      actor = new IndexCountRequestOp(SafeRefPtrFromThis(), aParams);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool TransactionBase::StartRequest(PBackgroundIDBRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  auto* op = static_cast<NormalTransactionOp*>(aActor);

  if (NS_WARN_IF(!op->Init(*this))) {
    op->Cleanup();
    return false;
  }

  op->DispatchToConnectionPool();
  return true;
}

bool TransactionBase::DeallocRequest(
    PBackgroundIDBRequestParent* const aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  const RefPtr<NormalTransactionOp> actor =
      dont_AddRef(static_cast<NormalTransactionOp*>(aActor));
  return true;
}

already_AddRefed<PBackgroundIDBCursorParent> TransactionBase::AllocCursor(
    const OpenCursorParams& aParams, bool aTrustParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  aTrustParams = false;
#endif

  const OpenCursorParams::Type type = aParams.type();
  RefPtr<FullObjectStoreMetadata> objectStoreMetadata;
  RefPtr<FullIndexMetadata> indexMetadata;
  CursorBase::Direction direction;

  // First extract the parameters common to all open cursor variants.
  const auto& commonParams = GetCommonOpenCursorParams(aParams);
  objectStoreMetadata =
      GetMetadataForObjectStoreId(commonParams.objectStoreId());
  if (NS_WARN_IF(!objectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }
  if (aTrustParams &&
      NS_WARN_IF(!VerifyRequestParams(commonParams.optionalKeyRange()))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }
  direction = commonParams.direction();

  // Now, for the index open cursor variants, extract the additional parameter.
  if (type == OpenCursorParams::TIndexOpenCursorParams ||
      type == OpenCursorParams::TIndexOpenKeyCursorParams) {
    const auto& commonIndexParams = GetCommonIndexOpenCursorParams(aParams);
    indexMetadata =
        GetMetadataForIndexId(objectStoreMetadata, commonIndexParams.indexId());
    if (NS_WARN_IF(!indexMetadata)) {
      ASSERT_UNLESS_FUZZING();
      return nullptr;
    }
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // Create Cursor and transfer ownership to IPDL.
  switch (type) {
    case OpenCursorParams::TObjectStoreOpenCursorParams:
      MOZ_ASSERT(!indexMetadata);
      return MakeAndAddRef<Cursor<IDBCursorType::ObjectStore>>(
          SafeRefPtrFromThis(), std::move(objectStoreMetadata), direction,
          CursorBase::ConstructFromTransactionBase{});
    case OpenCursorParams::TObjectStoreOpenKeyCursorParams:
      MOZ_ASSERT(!indexMetadata);
      return MakeAndAddRef<Cursor<IDBCursorType::ObjectStoreKey>>(
          SafeRefPtrFromThis(), std::move(objectStoreMetadata), direction,
          CursorBase::ConstructFromTransactionBase{});
    case OpenCursorParams::TIndexOpenCursorParams:
      return MakeAndAddRef<Cursor<IDBCursorType::Index>>(
          SafeRefPtrFromThis(), std::move(objectStoreMetadata),
          std::move(indexMetadata), direction,
          CursorBase::ConstructFromTransactionBase{});
    case OpenCursorParams::TIndexOpenKeyCursorParams:
      return MakeAndAddRef<Cursor<IDBCursorType::IndexKey>>(
          SafeRefPtrFromThis(), std::move(objectStoreMetadata),
          std::move(indexMetadata), direction,
          CursorBase::ConstructFromTransactionBase{});
    default:
      MOZ_CRASH("Cannot get here.");
  }
}

bool TransactionBase::StartCursor(PBackgroundIDBCursorParent* const aActor,
                                  const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  auto* const op = static_cast<CursorBase*>(aActor);

  if (NS_WARN_IF(!op->Start(aParams))) {
    return false;
  }

  return true;
}

/*******************************************************************************
 * NormalTransaction
 ******************************************************************************/

NormalTransaction::NormalTransaction(
    SafeRefPtr<Database> aDatabase, TransactionBase::Mode aMode,
    nsTArray<RefPtr<FullObjectStoreMetadata>>&& aObjectStores)
    : TransactionBase(std::move(aDatabase), aMode),
      mObjectStores{std::move(aObjectStores)} {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mObjectStores.IsEmpty());
}

bool NormalTransaction::IsSameProcessActor() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* const actor = Manager()->Manager()->Manager();
  MOZ_ASSERT(actor);

  return !BackgroundParent::IsOtherProcessActor(actor);
}

void NormalTransaction::SendCompleteNotification(nsresult aResult) {
  AssertIsOnBackgroundThread();

  if (!IsActorDestroyed()) {
    Unused << SendComplete(aResult);
  }
}

void NormalTransaction::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();

  if (!mCommittedOrAborted) {
    if (NS_SUCCEEDED(mResultCode)) {
      IDB_REPORT_INTERNAL_ERR();
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    mForceAborted.EnsureFlipped();

    MaybeCommitOrAbort();
  }
}

mozilla::ipc::IPCResult NormalTransaction::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  IProtocol* const mgr = Manager();
  if (!PBackgroundIDBTransactionParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult NormalTransaction::RecvCommit(
    const Maybe<int64_t>& aLastRequest) {
  AssertIsOnBackgroundThread();

  if (!TransactionBase::RecvCommit(aLastRequest)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult NormalTransaction::RecvAbort(
    const nsresult& aResultCode) {
  AssertIsOnBackgroundThread();

  if (!TransactionBase::RecvAbort(aResultCode)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PBackgroundIDBRequestParent*
NormalTransaction::AllocPBackgroundIDBRequestParent(
    const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  return AllocRequest(std::move(const_cast<RequestParams&>(aParams)),
                      IsSameProcessActor());
}

mozilla::ipc::IPCResult NormalTransaction::RecvPBackgroundIDBRequestConstructor(
    PBackgroundIDBRequestParent* const aActor, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (!StartRequest(aActor)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool NormalTransaction::DeallocPBackgroundIDBRequestParent(
    PBackgroundIDBRequestParent* const aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocRequest(aActor);
}

already_AddRefed<PBackgroundIDBCursorParent>
NormalTransaction::AllocPBackgroundIDBCursorParent(
    const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();

  return AllocCursor(aParams, IsSameProcessActor());
}

mozilla::ipc::IPCResult NormalTransaction::RecvPBackgroundIDBCursorConstructor(
    PBackgroundIDBCursorParent* const aActor, const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  if (!StartCursor(aActor, aParams)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

/*******************************************************************************
 * VersionChangeTransaction
 ******************************************************************************/

VersionChangeTransaction::VersionChangeTransaction(
    OpenDatabaseOp* aOpenDatabaseOp)
    : TransactionBase(aOpenDatabaseOp->mDatabase.clonePtr(),
                      IDBTransaction::Mode::VersionChange),
      mOpenDatabaseOp(aOpenDatabaseOp) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aOpenDatabaseOp);
}

VersionChangeTransaction::~VersionChangeTransaction() {
#ifdef DEBUG
  // Silence the base class' destructor assertion if we never made this actor
  // live.
  FakeActorDestroyed();
#endif
}

bool VersionChangeTransaction::IsSameProcessActor() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager()->Manager()->Manager();
  MOZ_ASSERT(actor);

  return !BackgroundParent::IsOtherProcessActor(actor);
}

void VersionChangeTransaction::SetActorAlive() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  mActorWasAlive.Flip();
}

bool VersionChangeTransaction::CopyDatabaseMetadata() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mOldMetadata);

  const auto& origMetadata = GetDatabase().Metadata();

  SafeRefPtr<FullDatabaseMetadata> newMetadata = origMetadata.Duplicate();
  if (NS_WARN_IF(!newMetadata)) {
    return false;
  }

  // Replace the live metadata with the new mutable copy.
  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(origMetadata.mDatabaseId, &info));
  MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());
  MOZ_ASSERT(info->mMetadata == &origMetadata);

  mOldMetadata = std::move(info->mMetadata);
  info->mMetadata = std::move(newMetadata);

  // Replace metadata pointers for all live databases.
  for (const auto& liveDatabase : info->mLiveDatabases) {
    liveDatabase->mMetadata = info->mMetadata.clonePtr();
  }

  return true;
}

void VersionChangeTransaction::UpdateMetadata(nsresult aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(!!mActorWasAlive == !!mOpenDatabaseOp->mDatabase);
  MOZ_ASSERT_IF(mActorWasAlive, !mOpenDatabaseOp->mDatabaseId.IsEmpty());

  if (IsActorDestroyed() || !mActorWasAlive) {
    return;
  }

  SafeRefPtr<FullDatabaseMetadata> oldMetadata = std::move(mOldMetadata);

  DatabaseActorInfo* info;
  if (!gLiveDatabaseHashtable->Get(oldMetadata->mDatabaseId, &info)) {
    return;
  }

  MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());

  if (NS_SUCCEEDED(aResult)) {
    // Remove all deleted objectStores and indexes, then mark immutable.
    info->mMetadata->mObjectStores.RemoveIf([](const auto& objectStoreIter) {
      MOZ_ASSERT(objectStoreIter.Key());
      const RefPtr<FullObjectStoreMetadata>& metadata = objectStoreIter.Data();
      MOZ_ASSERT(metadata);

      if (metadata->mDeleted) {
        return true;
      }

      metadata->mIndexes.RemoveIf([](const auto& indexIter) -> bool {
        MOZ_ASSERT(indexIter.Key());
        const RefPtr<FullIndexMetadata>& index = indexIter.Data();
        MOZ_ASSERT(index);

        return index->mDeleted;
      });
      metadata->mIndexes.MarkImmutable();

      return false;
    });

    info->mMetadata->mObjectStores.MarkImmutable();
  } else {
    // Replace metadata pointers for all live databases.
    info->mMetadata = std::move(oldMetadata);

    for (auto& liveDatabase : info->mLiveDatabases) {
      liveDatabase->mMetadata = info->mMetadata.clonePtr();
    }
  }
}

void VersionChangeTransaction::SendCompleteNotification(nsresult aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT_IF(!mActorWasAlive, mOpenDatabaseOp->HasFailed());
  MOZ_ASSERT_IF(!mActorWasAlive, mOpenDatabaseOp->mState >
                                     OpenDatabaseOp::State::SendingResults);

  const RefPtr<OpenDatabaseOp> openDatabaseOp = std::move(mOpenDatabaseOp);

  if (!mActorWasAlive) {
    return;
  }

  if (NS_FAILED(aResult)) {
    // 3.3.1 Opening a database:
    // "If the upgrade transaction was aborted, run the steps for closing a
    //  database connection with connection, create and return a new AbortError
    //  exception and abort these steps."
    openDatabaseOp->SetFailureCodeIfUnset(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  }

  openDatabaseOp->mState = OpenDatabaseOp::State::SendingResults;

  if (!IsActorDestroyed()) {
    Unused << SendComplete(aResult);
  }

  MOZ_ALWAYS_SUCCEEDS(openDatabaseOp->Run());
}

void VersionChangeTransaction::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();

  if (!mCommittedOrAborted) {
    if (NS_SUCCEEDED(mResultCode)) {
      IDB_REPORT_INTERNAL_ERR();
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    mForceAborted.EnsureFlipped();

    MaybeCommitOrAbort();
  }
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBVersionChangeTransactionParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvCommit(
    const Maybe<int64_t>& aLastRequest) {
  AssertIsOnBackgroundThread();

  if (!TransactionBase::RecvCommit(aLastRequest)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvAbort(
    const nsresult& aResultCode) {
  AssertIsOnBackgroundThread();

  if (!TransactionBase::RecvAbort(aResultCode)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvCreateObjectStore(
    const ObjectStoreMetadata& aMetadata) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aMetadata.id())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const SafeRefPtr<FullDatabaseMetadata> dbMetadata =
      GetDatabase().MetadataPtr();

  if (NS_WARN_IF(aMetadata.id() != dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(
          MatchMetadataNameOrId(dbMetadata->mObjectStores, aMetadata.id(),
                                SomeRef<const nsAString&>(aMetadata.name()))
              .isSome())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> newMetadata = new FullObjectStoreMetadata();
  newMetadata->mCommonMetadata = aMetadata;
  newMetadata->mNextAutoIncrementId = aMetadata.autoIncrement() ? 1 : 0;
  newMetadata->mCommittedAutoIncrementId = newMetadata->mNextAutoIncrementId;

  if (NS_WARN_IF(!dbMetadata->mObjectStores.InsertOrUpdate(
          aMetadata.id(), std::move(newMetadata), fallible))) {
    return IPC_FAIL_NO_REASON(this);
  }

  dbMetadata->mNextObjectStoreId++;

  RefPtr<CreateObjectStoreOp> op = new CreateObjectStoreOp(
      SafeRefPtrFromThis().downcast<VersionChangeTransaction>(), aMetadata);

  if (NS_WARN_IF(!op->Init(*this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvDeleteObjectStore(
    const IndexOrObjectStoreId& aObjectStoreId) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const auto& dbMetadata = GetDatabase().Metadata();
  MOZ_ASSERT(dbMetadata.mNextObjectStoreId > 0);

  if (NS_WARN_IF(aObjectStoreId >= dbMetadata.mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> foundMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  foundMetadata->mDeleted.Flip();

  DebugOnly<bool> foundTargetId = false;
  const bool isLastObjectStore = std::all_of(
      dbMetadata.mObjectStores.begin(), dbMetadata.mObjectStores.end(),
      [&foundTargetId, aObjectStoreId](const auto& objectStoreEntry) -> bool {
        if (uint64_t(aObjectStoreId) == objectStoreEntry.GetKey()) {
          foundTargetId = true;
          return true;
        }

        return objectStoreEntry.GetData()->mDeleted;
      });
  MOZ_ASSERT_IF(isLastObjectStore, foundTargetId);

  RefPtr<DeleteObjectStoreOp> op = new DeleteObjectStoreOp(
      SafeRefPtrFromThis().downcast<VersionChangeTransaction>(), foundMetadata,
      isLastObjectStore);

  if (NS_WARN_IF(!op->Init(*this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvRenameObjectStore(
    const IndexOrObjectStoreId& aObjectStoreId, const nsString& aName) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  {
    const auto& dbMetadata = GetDatabase().Metadata();
    MOZ_ASSERT(dbMetadata.mNextObjectStoreId > 0);

    if (NS_WARN_IF(aObjectStoreId >= dbMetadata.mNextObjectStoreId)) {
      ASSERT_UNLESS_FUZZING();
      return IPC_FAIL_NO_REASON(this);
    }
  }

  RefPtr<FullObjectStoreMetadata> foundMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  foundMetadata->mCommonMetadata.name() = aName;

  RefPtr<RenameObjectStoreOp> renameOp = new RenameObjectStoreOp(
      SafeRefPtrFromThis().downcast<VersionChangeTransaction>(), foundMetadata);

  if (NS_WARN_IF(!renameOp->Init(*this))) {
    renameOp->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  renameOp->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvCreateIndex(
    const IndexOrObjectStoreId& aObjectStoreId,
    const IndexMetadata& aMetadata) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!aMetadata.id())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const auto dbMetadata = GetDatabase().MetadataPtr();

  if (NS_WARN_IF(aMetadata.id() != dbMetadata->mNextIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> foundObjectStoreMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundObjectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(MatchMetadataNameOrId(
                     foundObjectStoreMetadata->mIndexes, aMetadata.id(),
                     SomeRef<const nsAString&>(aMetadata.name()))
                     .isSome())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullIndexMetadata> newMetadata = new FullIndexMetadata();
  newMetadata->mCommonMetadata = aMetadata;

  if (NS_WARN_IF(!foundObjectStoreMetadata->mIndexes.InsertOrUpdate(
          aMetadata.id(), std::move(newMetadata), fallible))) {
    return IPC_FAIL_NO_REASON(this);
  }

  dbMetadata->mNextIndexId++;

  RefPtr<CreateIndexOp> op = new CreateIndexOp(
      SafeRefPtrFromThis().downcast<VersionChangeTransaction>(), aObjectStoreId,
      aMetadata);

  if (NS_WARN_IF(!op->Init(*this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvDeleteIndex(
    const IndexOrObjectStoreId& aObjectStoreId,
    const IndexOrObjectStoreId& aIndexId) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!aIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }
  {
    const auto& dbMetadata = GetDatabase().Metadata();
    MOZ_ASSERT(dbMetadata.mNextObjectStoreId > 0);
    MOZ_ASSERT(dbMetadata.mNextIndexId > 0);

    if (NS_WARN_IF(aObjectStoreId >= dbMetadata.mNextObjectStoreId)) {
      ASSERT_UNLESS_FUZZING();
      return IPC_FAIL_NO_REASON(this);
    }

    if (NS_WARN_IF(aIndexId >= dbMetadata.mNextIndexId)) {
      ASSERT_UNLESS_FUZZING();
      return IPC_FAIL_NO_REASON(this);
    }
  }

  RefPtr<FullObjectStoreMetadata> foundObjectStoreMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundObjectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullIndexMetadata> foundIndexMetadata =
      GetMetadataForIndexId(foundObjectStoreMetadata, aIndexId);

  if (NS_WARN_IF(!foundIndexMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  foundIndexMetadata->mDeleted.Flip();

  DebugOnly<bool> foundTargetId = false;
  const bool isLastIndex =
      std::all_of(foundObjectStoreMetadata->mIndexes.cbegin(),
                  foundObjectStoreMetadata->mIndexes.cend(),
                  [&foundTargetId, aIndexId](const auto& indexEntry) -> bool {
                    if (uint64_t(aIndexId) == indexEntry.GetKey()) {
                      foundTargetId = true;
                      return true;
                    }

                    return indexEntry.GetData()->mDeleted;
                  });
  MOZ_ASSERT_IF(isLastIndex, foundTargetId);

  RefPtr<DeleteIndexOp> op = new DeleteIndexOp(
      SafeRefPtrFromThis().downcast<VersionChangeTransaction>(), aObjectStoreId,
      aIndexId, foundIndexMetadata->mCommonMetadata.unique(), isLastIndex);

  if (NS_WARN_IF(!op->Init(*this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->DispatchToConnectionPool();

  return IPC_OK();
}

mozilla::ipc::IPCResult VersionChangeTransaction::RecvRenameIndex(
    const IndexOrObjectStoreId& aObjectStoreId,
    const IndexOrObjectStoreId& aIndexId, const nsString& aName) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!aIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const SafeRefPtr<FullDatabaseMetadata> dbMetadata =
      GetDatabase().MetadataPtr();
  MOZ_ASSERT(dbMetadata);
  MOZ_ASSERT(dbMetadata->mNextObjectStoreId > 0);
  MOZ_ASSERT(dbMetadata->mNextIndexId > 0);

  if (NS_WARN_IF(aObjectStoreId >= dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aIndexId >= dbMetadata->mNextIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullObjectStoreMetadata> foundObjectStoreMetadata =
      GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundObjectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<FullIndexMetadata> foundIndexMetadata =
      GetMetadataForIndexId(foundObjectStoreMetadata, aIndexId);

  if (NS_WARN_IF(!foundIndexMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  foundIndexMetadata->mCommonMetadata.name() = aName;

  RefPtr<RenameIndexOp> renameOp = new RenameIndexOp(
      SafeRefPtrFromThis().downcast<VersionChangeTransaction>(),
      foundIndexMetadata, aObjectStoreId);

  if (NS_WARN_IF(!renameOp->Init(*this))) {
    renameOp->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  renameOp->DispatchToConnectionPool();

  return IPC_OK();
}

PBackgroundIDBRequestParent*
VersionChangeTransaction::AllocPBackgroundIDBRequestParent(
    const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  return AllocRequest(std::move(const_cast<RequestParams&>(aParams)),
                      IsSameProcessActor());
}

mozilla::ipc::IPCResult
VersionChangeTransaction::RecvPBackgroundIDBRequestConstructor(
    PBackgroundIDBRequestParent* aActor, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (!StartRequest(aActor)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool VersionChangeTransaction::DeallocPBackgroundIDBRequestParent(
    PBackgroundIDBRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocRequest(aActor);
}

already_AddRefed<PBackgroundIDBCursorParent>
VersionChangeTransaction::AllocPBackgroundIDBCursorParent(
    const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();

  return AllocCursor(aParams, IsSameProcessActor());
}

mozilla::ipc::IPCResult
VersionChangeTransaction::RecvPBackgroundIDBCursorConstructor(
    PBackgroundIDBCursorParent* aActor, const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  if (!StartCursor(aActor, aParams)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

/*******************************************************************************
 * CursorBase
 ******************************************************************************/

CursorBase::CursorBase(SafeRefPtr<TransactionBase> aTransaction,
                       RefPtr<FullObjectStoreMetadata> aObjectStoreMetadata,
                       const Direction aDirection,
                       const ConstructFromTransactionBase /*aConstructionTag*/)
    : mTransaction(std::move(aTransaction)),
      mObjectStoreMetadata(WrapNotNull(std::move(aObjectStoreMetadata))),
      mObjectStoreId((*mObjectStoreMetadata)->mCommonMetadata.id()),
      mDirection(aDirection),
      mMaxExtraCount(IndexedDatabaseManager::MaxPreloadExtraRecords()),
      mIsSameProcessActor(!BackgroundParent::IsOtherProcessActor(
          mTransaction->GetBackgroundParent())) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransaction);

  static_assert(
      OpenCursorParams::T__None == 0 && OpenCursorParams::T__Last == 4,
      "Lots of code here assumes only four types of cursors!");
}

template <IDBCursorType CursorType>
bool Cursor<CursorType>::VerifyRequestParams(
    const CursorRequestParams& aParams,
    const CursorPosition<CursorType>& aPosition) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != CursorRequestParams::T__None);
  MOZ_ASSERT(this->mObjectStoreMetadata);
  if constexpr (IsIndexCursor) {
    MOZ_ASSERT(this->mIndexMetadata);
  }

#ifdef DEBUG
  {
    const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        mTransaction->GetMetadataForObjectStoreId(mObjectStoreId);
    if (objectStoreMetadata) {
      MOZ_ASSERT(objectStoreMetadata == (*this->mObjectStoreMetadata));
    } else {
      MOZ_ASSERT((*this->mObjectStoreMetadata)->mDeleted);
    }

    if constexpr (IsIndexCursor) {
      if (objectStoreMetadata) {
        const RefPtr<FullIndexMetadata> indexMetadata =
            mTransaction->GetMetadataForIndexId(objectStoreMetadata,
                                                this->mIndexId);
        if (indexMetadata) {
          MOZ_ASSERT(indexMetadata == *this->mIndexMetadata);
        } else {
          MOZ_ASSERT((*this->mIndexMetadata)->mDeleted);
        }
      }
    }
  }
#endif

  if (NS_WARN_IF((*this->mObjectStoreMetadata)->mDeleted)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if constexpr (IsIndexCursor) {
    if (this->mIndexMetadata && NS_WARN_IF((*this->mIndexMetadata)->mDeleted)) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  }

  const Key& sortKey = aPosition.GetSortKey(this->IsLocaleAware());

  switch (aParams.type()) {
    case CursorRequestParams::TContinueParams: {
      const Key& key = aParams.get_ContinueParams().key();
      if (!key.IsUnset()) {
        switch (mDirection) {
          case IDBCursorDirection::Next:
          case IDBCursorDirection::Nextunique:
            if (NS_WARN_IF(key <= sortKey)) {
              ASSERT_UNLESS_FUZZING();
              return false;
            }
            break;

          case IDBCursorDirection::Prev:
          case IDBCursorDirection::Prevunique:
            if (NS_WARN_IF(key >= sortKey)) {
              ASSERT_UNLESS_FUZZING();
              return false;
            }
            break;

          default:
            MOZ_CRASH("Should never get here!");
        }
      }
      break;
    }

    case CursorRequestParams::TContinuePrimaryKeyParams: {
      if constexpr (IsIndexCursor) {
        const Key& key = aParams.get_ContinuePrimaryKeyParams().key();
        const Key& primaryKey =
            aParams.get_ContinuePrimaryKeyParams().primaryKey();
        MOZ_ASSERT(!key.IsUnset());
        MOZ_ASSERT(!primaryKey.IsUnset());
        switch (mDirection) {
          case IDBCursorDirection::Next:
            if (NS_WARN_IF(key < sortKey ||
                           (key == sortKey &&
                            primaryKey <= aPosition.mObjectStoreKey))) {
              ASSERT_UNLESS_FUZZING();
              return false;
            }
            break;

          case IDBCursorDirection::Prev:
            if (NS_WARN_IF(key > sortKey ||
                           (key == sortKey &&
                            primaryKey >= aPosition.mObjectStoreKey))) {
              ASSERT_UNLESS_FUZZING();
              return false;
            }
            break;

          default:
            MOZ_CRASH("Should never get here!");
        }
      }
      break;
    }

    case CursorRequestParams::TAdvanceParams:
      if (NS_WARN_IF(!aParams.get_AdvanceParams().count())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

template <IDBCursorType CursorType>
bool Cursor<CursorType>::Start(const OpenCursorParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() == ToOpenCursorParamsType(CursorType));
  MOZ_ASSERT(this->mObjectStoreMetadata);

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const Maybe<SerializedKeyRange>& optionalKeyRange =
      GetCommonOpenCursorParams(aParams).optionalKeyRange();

  const RefPtr<OpenOp> openOp = new OpenOp(this, optionalKeyRange);

  if (NS_WARN_IF(!openOp->Init(*mTransaction))) {
    openOp->Cleanup();
    return false;
  }

  openOp->DispatchToConnectionPool();
  mCurrentlyRunningOp = openOp;

  return true;
}

void ValueCursorBase::ProcessFiles(CursorResponse& aResponse,
                                   const FilesArray& aFiles) {
  MOZ_ASSERT_IF(
      aResponse.type() == CursorResponse::Tnsresult ||
          aResponse.type() == CursorResponse::Tvoid_t ||
          aResponse.type() ==
              CursorResponse::TArrayOfObjectStoreKeyCursorResponse ||
          aResponse.type() == CursorResponse::TArrayOfIndexKeyCursorResponse,
      aFiles.IsEmpty());

  for (size_t i = 0; i < aFiles.Length(); ++i) {
    const auto& files = aFiles[i];
    if (!files.IsEmpty()) {
      // TODO: Replace this assertion by one that checks if the response type
      // matches the cursor type, at a more generic location.
      MOZ_ASSERT(aResponse.type() ==
                     CursorResponse::TArrayOfObjectStoreCursorResponse ||
                 aResponse.type() ==
                     CursorResponse::TArrayOfIndexCursorResponse);

      SerializedStructuredCloneReadInfo* serializedInfo = nullptr;
      switch (aResponse.type()) {
        case CursorResponse::TArrayOfObjectStoreCursorResponse: {
          auto& responses = aResponse.get_ArrayOfObjectStoreCursorResponse();
          MOZ_ASSERT(i < responses.Length());
          serializedInfo = &responses[i].cloneInfo();
          break;
        }

        case CursorResponse::TArrayOfIndexCursorResponse: {
          auto& responses = aResponse.get_ArrayOfIndexCursorResponse();
          MOZ_ASSERT(i < responses.Length());
          serializedInfo = &responses[i].cloneInfo();
          break;
        }

        default:
          MOZ_CRASH("Should never get here!");
      }

      MOZ_ASSERT(serializedInfo);
      MOZ_ASSERT(serializedInfo->files().IsEmpty());
      MOZ_ASSERT(this->mDatabase);

      IDB_TRY_UNWRAP(serializedInfo->files(),
                     SerializeStructuredCloneFiles((*this->mBackgroundParent),
                                                   this->mDatabase, files,
                                                   /* aForPreprocess */ false),
                     QM_VOID, [&aResponse](const nsresult result) {
                       aResponse = ClampResultCode(result);
                     });
    }
  }
}

template <IDBCursorType CursorType>
void Cursor<CursorType>::SendResponseInternal(
    CursorResponse& aResponse, const FilesArrayT<CursorType>& aFiles) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aResponse.type() != CursorResponse::T__None);
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tnsresult,
                NS_FAILED(aResponse.get_nsresult()));
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tnsresult,
                NS_ERROR_GET_MODULE(aResponse.get_nsresult()) ==
                    NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT(this->mObjectStoreMetadata);
  MOZ_ASSERT(mCurrentlyRunningOp);

  KeyValueBase::ProcessFiles(aResponse, aFiles);

  // Work around the deleted function by casting to the base class.
  auto* const base = static_cast<PBackgroundIDBCursorParent*>(this);
  if (!base->SendResponse(aResponse)) {
    NS_WARNING("Failed to send response!");
  }

  mCurrentlyRunningOp = nullptr;
}

template <IDBCursorType CursorType>
void Cursor<CursorType>::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  if (mCurrentlyRunningOp) {
    mCurrentlyRunningOp->NoteActorDestroyed();
  }

  if constexpr (IsValueCursor) {
    this->mBackgroundParent.destroy();
  }
  this->mObjectStoreMetadata.destroy();
  if constexpr (IsIndexCursor) {
    this->mIndexMetadata.destroy();
  }
}

template <IDBCursorType CursorType>
mozilla::ipc::IPCResult Cursor<CursorType>::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(this->mObjectStoreMetadata);

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  IProtocol* mgr = Manager();
  if (!PBackgroundIDBCursorParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

template <IDBCursorType CursorType>
mozilla::ipc::IPCResult Cursor<CursorType>::RecvContinue(
    const CursorRequestParams& aParams, const Key& aCurrentKey,
    const Key& aCurrentObjectStoreKey) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != CursorRequestParams::T__None);
  MOZ_ASSERT(this->mObjectStoreMetadata);
  if constexpr (IsIndexCursor) {
    MOZ_ASSERT(this->mIndexMetadata);
  }

  const bool trustParams =
#ifdef DEBUG
      // Always verify parameters in DEBUG builds!
      false
#else
      this->mIsSameProcessActor
#endif
      ;

  MOZ_ASSERT(!aCurrentKey.IsUnset());

  IDB_TRY_UNWRAP(
      auto position,
      ([&]() -> Result<CursorPosition<CursorType>, mozilla::ipc::IPCResult> {
        if constexpr (IsIndexCursor) {
          auto localeAwarePosition = Key{};
          if (this->IsLocaleAware()) {
            IDB_TRY_UNWRAP(localeAwarePosition,
                           aCurrentKey.ToLocaleAwareKey(this->mLocale),
                           Err(IPC_FAIL_NO_REASON(this)),
                           [](const auto&) { ASSERT_UNLESS_FUZZING(); });
          }
          return CursorPosition<CursorType>{aCurrentKey, localeAwarePosition,
                                            aCurrentObjectStoreKey};
        } else {
          return CursorPosition<CursorType>{aCurrentKey};
        }
      }()));

  if (!trustParams && !VerifyRequestParams(aParams, position)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mTransaction->mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  const RefPtr<ContinueOp> continueOp =
      new ContinueOp(this, aParams, std::move(position));
  if (NS_WARN_IF(!continueOp->Init(*mTransaction))) {
    continueOp->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  continueOp->DispatchToConnectionPool();
  mCurrentlyRunningOp = continueOp;

  return IPC_OK();
}

/*******************************************************************************
 * FileManager
 ******************************************************************************/

FileManager::MutexType FileManager::sMutex;

FileManager::FileManager(PersistenceType aPersistenceType,
                         const quota::OriginMetadata& aOriginMetadata,
                         const nsAString& aDatabaseName, bool aEnforcingQuota)
    : mPersistenceType(aPersistenceType),
      mOriginMetadata(aOriginMetadata),
      mDatabaseName(aDatabaseName),
      mEnforcingQuota(aEnforcingQuota) {}

nsresult FileManager::Init(nsIFile* aDirectory,
                           mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  {
    IDB_TRY_INSPECT(const bool& existsAsDirectory,
                    ExistsAsDirectory(*aDirectory));

    if (!existsAsDirectory) {
      IDB_TRY(aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755));
    }

    IDB_TRY_UNWRAP(auto path,
                   MOZ_TO_RESULT_INVOKE_TYPED(nsString, aDirectory, GetPath));

    mDirectoryPath.init(std::move(path));
  }

  IDB_TRY_INSPECT(const auto& journalDirectory,
                  CloneFileAndAppend(*aDirectory, kJournalDirectoryName));

  // We don't care if it doesn't exist at all, but if it does exist, make sure
  // it's a directory.
  IDB_TRY_INSPECT(const bool& existsAsDirectory,
                  ExistsAsDirectory(*journalDirectory));
  Unused << existsAsDirectory;

  {
    IDB_TRY_UNWRAP(auto path, MOZ_TO_RESULT_INVOKE_TYPED(
                                  nsString, journalDirectory, GetPath));

    mJournalDirectoryPath.init(std::move(path));
  }

  IDB_TRY_INSPECT(const auto& stmt,
                  MOZ_TO_RESULT_INVOKE_TYPED(
                      nsCOMPtr<mozIStorageStatement>, aConnection,
                      CreateStatement, "SELECT id, refcount FROM file"_ns));

  IDB_TRY(
      CollectWhileHasResult(*stmt, [this](auto& stmt) -> Result<Ok, nsresult> {
        IDB_TRY_INSPECT(const int64_t& id,
                        MOZ_TO_RESULT_INVOKE(stmt, GetInt64, 0));
        IDB_TRY_INSPECT(const int32_t& dbRefCnt,
                        MOZ_TO_RESULT_INVOKE(stmt, GetInt32, 1));

        // We put a raw pointer into the hash table, so the memory refcount will
        // be 0, but the dbRefCnt is non-zero, which will keep the FileInfo
        // object alive.
        MOZ_ASSERT(dbRefCnt > 0);
        mFileInfos.InsertOrUpdate(
            id, MakeNotNull<FileInfo*>(FileManagerGuard{}, SafeRefPtrFromThis(),
                                       id, static_cast<nsrefcnt>(dbRefCnt)));

        mLastFileId = std::max(id, mLastFileId);

        return Ok{};
      }));

  return NS_OK;
}

nsCOMPtr<nsIFile> FileManager::GetDirectory() {
  if (!this->AssertValid()) {
    return nullptr;
  }

  return GetFileForPath(*mDirectoryPath);
}

nsCOMPtr<nsIFile> FileManager::GetCheckedDirectory() {
  auto directory = GetDirectory();
  if (NS_WARN_IF(!directory)) {
    return nullptr;
  }

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(directory->Exists(&exists)));
  MOZ_ASSERT(exists);

  DebugOnly<bool> isDirectory;
  MOZ_ASSERT(NS_SUCCEEDED(directory->IsDirectory(&isDirectory)));
  MOZ_ASSERT(isDirectory);

  return directory;
}

nsCOMPtr<nsIFile> FileManager::GetJournalDirectory() {
  if (!this->AssertValid()) {
    return nullptr;
  }

  return GetFileForPath(*mJournalDirectoryPath);
}

nsCOMPtr<nsIFile> FileManager::EnsureJournalDirectory() {
  // This can happen on the IO or on a transaction thread.
  MOZ_ASSERT(!NS_IsMainThread());

  auto journalDirectory = GetFileForPath(*mJournalDirectoryPath);
  IDB_TRY(OkIf(journalDirectory), nullptr);

  IDB_TRY_INSPECT(const bool& exists,
                  MOZ_TO_RESULT_INVOKE(journalDirectory, Exists), nullptr);

  if (exists) {
    IDB_TRY_INSPECT(const bool& isDirectory,
                    MOZ_TO_RESULT_INVOKE(journalDirectory, IsDirectory),
                    nullptr);

    IDB_TRY(OkIf(isDirectory), nullptr);
  } else {
    IDB_TRY(journalDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755), nullptr);
  }

  return journalDirectory;
}

// static
nsCOMPtr<nsIFile> FileManager::GetFileForId(nsIFile* aDirectory, int64_t aId) {
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aId > 0);

  IDB_TRY_RETURN(CloneFileAndAppend(*aDirectory, IntToString(aId)), nullptr);
}

// static
nsCOMPtr<nsIFile> FileManager::GetCheckedFileForId(nsIFile* aDirectory,
                                                   int64_t aId) {
  auto file = GetFileForId(aDirectory, aId);
  if (NS_WARN_IF(!file)) {
    return nullptr;
  }

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(file->Exists(&exists)));
  MOZ_ASSERT(exists);

  DebugOnly<bool> isFile;
  MOZ_ASSERT(NS_SUCCEEDED(file->IsFile(&isFile)));
  MOZ_ASSERT(isFile);

  return file;
}

// static
nsresult FileManager::InitDirectory(nsIFile& aDirectory, nsIFile& aDatabaseFile,
                                    const nsACString& aOrigin,
                                    uint32_t aTelemetryId) {
  AssertIsOnIOThread();

  {
    IDB_TRY_INSPECT(const bool& exists,
                    MOZ_TO_RESULT_INVOKE(aDirectory, Exists));

    if (!exists) {
      return NS_OK;
    }

    IDB_TRY_INSPECT(const bool& isDirectory,
                    MOZ_TO_RESULT_INVOKE(aDirectory, IsDirectory));
    IDB_TRY(OkIf(isDirectory), NS_ERROR_FAILURE);
  }

  IDB_TRY_INSPECT(const auto& journalDirectory,
                  CloneFileAndAppend(aDirectory, kJournalDirectoryName));

  IDB_TRY_INSPECT(const bool& exists,
                  MOZ_TO_RESULT_INVOKE(journalDirectory, Exists));

  if (exists) {
    IDB_TRY_INSPECT(const bool& isDirectory,
                    MOZ_TO_RESULT_INVOKE(journalDirectory, IsDirectory));
    IDB_TRY(OkIf(isDirectory), NS_ERROR_FAILURE);

    bool hasJournals = false;

    IDB_TRY(CollectEachFile(
        *journalDirectory,
        [&hasJournals](const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
          IDB_TRY_INSPECT(
              const auto& leafName,
              MOZ_TO_RESULT_INVOKE_TYPED(nsString, file, GetLeafName));

          nsresult rv;
          leafName.ToInteger64(&rv);
          if (NS_SUCCEEDED(rv)) {
            hasJournals = true;
          } else {
            UNKNOWN_FILE_WARNING(leafName);
          }

          return Ok{};
        }));

    if (hasJournals) {
      IDB_TRY_UNWRAP(const NotNull<nsCOMPtr<mozIStorageConnection>> connection,
                     CreateStorageConnection(
                         aDatabaseFile, aDirectory, VoidString(), aOrigin,
                         /* aDirectoryLockId */ -1, aTelemetryId, Nothing{}));

      mozStorageTransaction transaction(connection.get(), false);

      IDB_TRY(transaction.Start())

      IDB_TRY(connection->ExecuteSimpleSQL(
          "CREATE VIRTUAL TABLE fs USING filesystem;"_ns));

      // The parameter names are not used, parameters are bound by index only
      // locally in the same function.
      IDB_TRY_INSPECT(
          const auto& stmt,
          MOZ_TO_RESULT_INVOKE_TYPED(
              nsCOMPtr<mozIStorageStatement>, *connection, CreateStatement,
              "SELECT name, (name IN (SELECT id FROM file)) FROM fs WHERE path = :path"_ns));

      IDB_TRY_INSPECT(
          const auto& path,
          MOZ_TO_RESULT_INVOKE_TYPED(nsString, journalDirectory, GetPath));

      IDB_TRY(stmt->BindStringByIndex(0, path));

      IDB_TRY(CollectWhileHasResult(
          *stmt,
          [&aDirectory, &journalDirectory](auto& stmt) -> Result<Ok, nsresult> {
            nsString name;
            IDB_TRY(stmt.GetString(0, name));

            nsresult rv;
            name.ToInteger64(&rv);
            if (NS_FAILED(rv)) {
              return Ok{};
            }

            int32_t flag = stmt.AsInt32(1);

            if (!flag) {
              IDB_TRY_INSPECT(const auto& file,
                              CloneFileAndAppend(aDirectory, name));

              if (NS_FAILED(file->Remove(false))) {
                NS_WARNING("Failed to remove orphaned file!");
              }
            }

            IDB_TRY_INSPECT(const auto& journalFile,
                            CloneFileAndAppend(*journalDirectory, name));

            if (NS_FAILED(journalFile->Remove(false))) {
              NS_WARNING("Failed to remove journal file!");
            }

            return Ok{};
          }));

      IDB_TRY(connection->ExecuteSimpleSQL("DROP TABLE fs;"_ns));
      IDB_TRY(transaction.Commit());
    }
  }

  return NS_OK;
}

// static
Result<FileUsageType, nsresult> FileManager::GetUsage(nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  IDB_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(aDirectory, Exists));

  if (!exists) {
    return FileUsageType{};
  }

  FileUsageType usage;

  IDB_TRY(CollectEachFile(
      *aDirectory,
      [&usage](const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
        IDB_TRY_INSPECT(const auto& leafName, MOZ_TO_RESULT_INVOKE_TYPED(
                                                  nsString, file, GetLeafName));

        if (leafName.Equals(kJournalDirectoryName)) {
          return Ok{};
        }

        nsresult rv;
        leafName.ToInteger64(&rv);
        if (NS_SUCCEEDED(rv)) {
          IDB_TRY_INSPECT(
              const auto& thisUsage,
              MOZ_TO_RESULT_INVOKE(file, GetFileSize)
                  .map([](const int64_t fileSize) {
                    return FileUsageType(Some(uint64_t(fileSize)));
                  })
                  .orElse(
                      [](const nsresult rv) -> Result<FileUsageType, nsresult> {
                        if (rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST ||
                            rv == NS_ERROR_FILE_NOT_FOUND) {
                          // If the file does no longer exist, treat it as
                          // 0-sized.
                          return FileUsageType{};
                        }

                        return Err(rv);
                      }));

          usage += thisUsage;

          return Ok{};
        }

        UNKNOWN_FILE_WARNING(leafName);

        return Ok{};
      }));

  return usage;
}

nsresult FileManager::SyncDeleteFile(const int64_t aId) {
  MOZ_ASSERT(!mFileInfos.Contains(aId));

  if (!this->AssertValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  const auto directory = GetDirectory();
  IDB_TRY(OkIf(directory), NS_ERROR_FAILURE);

  const auto journalDirectory = GetJournalDirectory();
  IDB_TRY(OkIf(journalDirectory), NS_ERROR_FAILURE);

  const nsCOMPtr<nsIFile> file = GetFileForId(directory, aId);
  IDB_TRY(OkIf(file), NS_ERROR_FAILURE);

  const nsCOMPtr<nsIFile> journalFile = GetFileForId(journalDirectory, aId);
  IDB_TRY(OkIf(journalFile), NS_ERROR_FAILURE);

  return SyncDeleteFile(*file, *journalFile);
}

nsresult FileManager::SyncDeleteFile(nsIFile& aFile, nsIFile& aJournalFile) {
  QuotaManager* const quotaManager =
      EnforcingQuota() ? QuotaManager::Get() : nullptr;
  MOZ_ASSERT_IF(EnforcingQuota(), quotaManager);

  IDB_TRY(DeleteFile(aFile, quotaManager, Type(), OriginMetadata(),
                     Idempotency::No));

  IDB_TRY(aJournalFile.Remove(false));

  return NS_OK;
}

/*******************************************************************************
 * QuotaClient
 ******************************************************************************/

QuotaClient* QuotaClient::sInstance = nullptr;

QuotaClient::QuotaClient() : mDeleteTimer(NS_NewTimer()) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!sInstance, "We expect this to be a singleton!");
  MOZ_ASSERT(!gTelemetryIdMutex);

  // Always create this so that later access to gTelemetryIdHashtable can be
  // properly synchronized.
  gTelemetryIdMutex = new Mutex("IndexedDB gTelemetryIdMutex");

  sInstance = this;
}

QuotaClient::~QuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this, "We expect this to be a singleton!");
  MOZ_ASSERT(gTelemetryIdMutex);
  MOZ_ASSERT(!mMaintenanceThreadPool);

  // No one else should be able to touch gTelemetryIdHashtable now that the
  // QuotaClient has gone away.
  gTelemetryIdHashtable = nullptr;
  gTelemetryIdMutex = nullptr;

  sInstance = nullptr;
}

nsresult QuotaClient::AsyncDeleteFile(FileManager* aFileManager,
                                      int64_t aFileId) {
  AssertIsOnBackgroundThread();

  if (mShutdownRequested) {
    // Whoops! We want to delete an IndexedDB disk-backed File but it's too late
    // to actually delete the file! This means we're going to "leak" the file
    // and leave it around when we shouldn't! (The file will stay around until
    // next storage initialization is triggered when the app is started again).
    // Fixing this is tracked by bug 1539377.

    return NS_OK;
  }

  MOZ_ASSERT(mDeleteTimer);
  MOZ_ALWAYS_SUCCEEDS(mDeleteTimer->Cancel());

  IDB_TRY(mDeleteTimer->InitWithNamedFuncCallback(
      DeleteTimerCallback, this, kDeleteTimeoutMs, nsITimer::TYPE_ONE_SHOT,
      "dom::indexeddb::QuotaClient::AsyncDeleteFile"));

  mPendingDeleteInfos.GetOrInsertNew(aFileManager)->AppendElement(aFileId);

  return NS_OK;
}

nsresult QuotaClient::FlushPendingFileDeletions() {
  AssertIsOnBackgroundThread();

  IDB_TRY(mDeleteTimer->Cancel());

  DeleteTimerCallback(mDeleteTimer, this);

  return NS_OK;
}

nsThreadPool* QuotaClient::GetOrCreateThreadPool() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  if (!mMaintenanceThreadPool) {
    RefPtr<nsThreadPool> threadPool = new nsThreadPool();

    // PR_GetNumberOfProcessors() can return -1 on error, so make sure we
    // don't set some huge number here. We add 2 in case some threads block on
    // the disk I/O.
    const uint32_t threadCount =
        std::max(int32_t(PR_GetNumberOfProcessors()), int32_t(1)) + 2;

    MOZ_ALWAYS_SUCCEEDS(threadPool->SetThreadLimit(threadCount));

    // Don't keep more than one idle thread.
    MOZ_ALWAYS_SUCCEEDS(threadPool->SetIdleThreadLimit(1));

    // Don't keep idle threads alive very long.
    MOZ_ALWAYS_SUCCEEDS(threadPool->SetIdleThreadTimeout(5 * PR_MSEC_PER_SEC));

    MOZ_ALWAYS_SUCCEEDS(threadPool->SetName("IndexedDB Mnt"_ns));

    mMaintenanceThreadPool = std::move(threadPool);
  }

  return mMaintenanceThreadPool;
}

mozilla::dom::quota::Client::Type QuotaClient::GetType() {
  return QuotaClient::IDB;
}

nsresult QuotaClient::UpgradeStorageFrom1_0To2_0(nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  IDB_TRY_INSPECT((const auto& [subdirsToProcess, databaseFilenames]),
                  GetDatabaseFilenames(*aDirectory,
                                       /* aCanceled */ AtomicBool{false}));

  IDB_TRY(CollectEachInRange(
      subdirsToProcess,
      [&databaseFilenames = databaseFilenames,
       aDirectory](const nsString& subdirName) -> Result<Ok, nsresult> {
        // If the directory has the correct suffix then it should exist in
        // databaseFilenames.
        nsDependentSubstring subdirNameBase;
        if (GetFilenameBase(subdirName, kFileManagerDirectoryNameSuffix,
                            subdirNameBase)) {
          IDB_TRY(OkIf(databaseFilenames.GetEntry(subdirNameBase)), Ok{});
          return Ok{};
        }

        // The directory didn't have the right suffix but we might need to
        // rename it. Check to see if we have a database that references this
        // directory.
        IDB_TRY_INSPECT(
            const auto& subdirNameWithSuffix,
            ([&databaseFilenames,
              &subdirName]() -> Result<nsAutoString, NotOk> {
              if (databaseFilenames.GetEntry(subdirName)) {
                return nsAutoString{subdirName +
                                    kFileManagerDirectoryNameSuffix};
              }

              // Windows doesn't allow a directory to end with a dot ('.'), so
              // we have to check that possibility here too. We do this on all
              // platforms, because the origin directory may have been created
              // on Windows and now accessed on different OS.
              const nsAutoString subdirNameWithDot = subdirName + u"."_ns;
              IDB_TRY(OkIf(databaseFilenames.GetEntry(subdirNameWithDot)),
                      Err(NotOk{}));

              return nsAutoString{subdirNameWithDot +
                                  kFileManagerDirectoryNameSuffix};
            }()),
            Ok{});

        // We do have a database that uses this subdir so we should rename it
        // now.
        IDB_TRY_INSPECT(const auto& subdir,
                        CloneFileAndAppend(*aDirectory, subdirName));

        DebugOnly<bool> isDirectory;
        MOZ_ASSERT(NS_SUCCEEDED(subdir->IsDirectory(&isDirectory)));
        MOZ_ASSERT(isDirectory);

        // Check if the subdir with suffix already exists before renaming.
        IDB_TRY_INSPECT(const auto& subdirWithSuffix,
                        CloneFileAndAppend(*aDirectory, subdirNameWithSuffix));

        IDB_TRY_INSPECT(const bool& exists,
                        MOZ_TO_RESULT_INVOKE(subdirWithSuffix, Exists));

        if (exists) {
          IDB_WARNING("Deleting old %s files directory!",
                      NS_ConvertUTF16toUTF8(subdirName).get());

          IDB_TRY(subdir->Remove(/* aRecursive */ true));

          return Ok{};
        }

        // Finally, rename the subdir.
        IDB_TRY(subdir->RenameTo(nullptr, subdirNameWithSuffix));

        return Ok{};
      }));

  return NS_OK;
}

nsresult QuotaClient::UpgradeStorageFrom2_1To2_2(nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  IDB_TRY(CollectEachFile(
      *aDirectory, [](const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
        IDB_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory:
            break;

          case nsIFileKind::ExistsAsFile: {
            IDB_TRY_INSPECT(
                const auto& leafName,
                MOZ_TO_RESULT_INVOKE_TYPED(nsString, file, GetLeafName));

            // It's reported that files ending with ".tmp" somehow live in the
            // indexedDB directories in Bug 1503883. Such files shouldn't exist
            // in the indexedDB directory so remove them in this upgrade.
            if (StringEndsWith(leafName, u".tmp"_ns)) {
              IDB_WARNING("Deleting unknown temporary file!");

              IDB_TRY(file->Remove(false));
            }

            break;
          }

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return Ok{};
      }));

  return NS_OK;
}

Result<UsageInfo, nsresult> QuotaClient::InitOrigin(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  IDB_TRY_RETURN(MOZ_TO_RESULT_INVOKE(this, GetUsageForOriginInternal,
                                      aPersistenceType, aOriginMetadata,
                                      aCanceled,
                                      /* aInitializing*/ true));
}

nsresult QuotaClient::InitOriginWithoutTracking(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  return GetUsageForOriginInternal(aPersistenceType, aOriginMetadata, aCanceled,
                                   /* aInitializing*/ true, nullptr);
}

Result<UsageInfo, nsresult> QuotaClient::GetUsageForOrigin(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  IDB_TRY_RETURN(MOZ_TO_RESULT_INVOKE(this, GetUsageForOriginInternal,
                                      aPersistenceType, aOriginMetadata,
                                      aCanceled,
                                      /* aInitializing*/ false));
}

nsresult QuotaClient::GetUsageForOriginInternal(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    const AtomicBool& aCanceled, const bool aInitializing,
    UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();

  IDB_TRY_INSPECT(const nsCOMPtr<nsIFile>& directory,
                  GetDirectory(aPersistenceType, aOriginMetadata.mOrigin));

  // We need to see if there are any files in the directory already. If they
  // are database files then we need to cleanup stored files (if it's needed)
  // and also get the usage.

  // XXX Can we avoid unwrapping into non-const variables here? (Only
  // databaseFilenames is currently modified below)
  IDB_TRY_UNWRAP(
      (auto [subdirsToProcess, databaseFilenames, obsoleteFilenames]),
      GetDatabaseFilenames<ObsoleteFilenamesHandling::Include>(*directory,
                                                               aCanceled));

  if (aInitializing) {
    IDB_TRY(CollectEachInRange(
        subdirsToProcess,
        [&directory, &obsoleteFilenames = obsoleteFilenames,
         &databaseFilenames = databaseFilenames, aPersistenceType,
         &aOriginMetadata](const nsString& subdirName) -> Result<Ok, nsresult> {
          // The directory must have the correct suffix.
          nsDependentSubstring subdirNameBase;
          IDB_TRY(([&subdirName, &subdirNameBase] {
                    IDB_TRY_RETURN(OkIf(GetFilenameBase(
                        subdirName, kFileManagerDirectoryNameSuffix,
                        subdirNameBase)));
                  }()
                       .orElse([&directory, &subdirName](
                                   const NotOk) -> Result<Ok, nsresult> {
                         // If there is an unexpected directory in the idb
                         // directory, trying to delete at first instead of
                         // breaking the whole initialization.
                         IDB_TRY(DeleteFilesNoQuota(directory, subdirName),
                                 Err(NS_ERROR_UNEXPECTED));

                         return Ok{};
                       })),
                  Ok{});

          if (obsoleteFilenames.Contains(subdirNameBase)) {
            // If this fails, it probably means we are in a serious situation.
            // e.g. Filesystem corruption. Will handle this in bug 1521541.
            IDB_TRY(RemoveDatabaseFilesAndDirectory(*directory, subdirNameBase,
                                                    nullptr, aPersistenceType,
                                                    aOriginMetadata, u""_ns),
                    Err(NS_ERROR_UNEXPECTED));

            databaseFilenames.RemoveEntry(subdirNameBase);
            return Ok{};
          }

          // The directory base must exist in databaseFilenames.
          // If there is an unexpected directory in the idb directory, trying to
          // delete at first instead of breaking the whole initialization.

          IDB_TRY(([&databaseFilenames, &subdirNameBase] {
                    IDB_TRY_RETURN(
                        OkIf(databaseFilenames.GetEntry(subdirNameBase)));
                  }()
                       .orElse([&directory, &subdirName](
                                   const NotOk) -> Result<Ok, nsresult> {
                         // XXX It seems if we really got here, we can fail the
                         // MOZ_ASSERT(!quotaManager->IsTemporaryStorageInitialized());
                         // assertion in DeleteFilesNoQuota.
                         IDB_TRY(DeleteFilesNoQuota(directory, subdirName),
                                 Err(NS_ERROR_UNEXPECTED));

                         return Ok{};
                       })),
                  Ok{});

          return Ok{};
        }));
  }

  for (const auto& databaseEntry : databaseFilenames) {
    if (aCanceled) {
      break;
    }

    const auto& databaseFilename = databaseEntry.GetKey();

    IDB_TRY_INSPECT(
        const auto& fmDirectory,
        CloneFileAndAppend(*directory,
                           databaseFilename + kFileManagerDirectoryNameSuffix));

    IDB_TRY_INSPECT(
        const auto& databaseFile,
        CloneFileAndAppend(*directory, databaseFilename + kSQLiteSuffix));

    if (aInitializing) {
      IDB_TRY(FileManager::InitDirectory(*fmDirectory, *databaseFile,
                                         aOriginMetadata.mOrigin,
                                         TelemetryIdForFile(databaseFile)));
    }

    if (aUsageInfo) {
      {
        IDB_TRY_INSPECT(const int64_t& fileSize,
                        MOZ_TO_RESULT_INVOKE(databaseFile, GetFileSize));

        MOZ_ASSERT(fileSize >= 0);

        *aUsageInfo += DatabaseUsageType(Some(uint64_t(fileSize)));
      }

      {
        IDB_TRY_INSPECT(const auto& walFile,
                        CloneFileAndAppend(
                            *directory, databaseFilename + kSQLiteWALSuffix));

        IDB_TRY_INSPECT(const int64_t& walFileSize,
                        MOZ_TO_RESULT_INVOKE(walFile, GetFileSize)
                            .orElse([](const nsresult rv) {
                              return (rv == NS_ERROR_FILE_NOT_FOUND ||
                                      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
                                         ? Result<int64_t, nsresult>{0}
                                         : Err(rv);
                            }));
        MOZ_ASSERT(walFileSize >= 0);
        *aUsageInfo += DatabaseUsageType(Some(uint64_t(walFileSize)));
      }

      {
        IDB_TRY_INSPECT(const auto& fileUsage,
                        FileManager::GetUsage(fmDirectory));

        *aUsageInfo += fileUsage;
      }
    }
  }

  return NS_OK;
}

void QuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                         const nsACString& aOrigin) {
  AssertIsOnIOThread();

  if (IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get()) {
    mgr->InvalidateFileManagers(aPersistenceType, aOrigin);
  }
}

void QuotaClient::ReleaseIOThreadObjects() {
  AssertIsOnIOThread();

  if (IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get()) {
    mgr->InvalidateAllFileManagers();
  }
}

void QuotaClient::AbortOperationsForLocks(
    const DirectoryLockIdTable& aDirectoryLockIds) {
  AssertIsOnBackgroundThread();

  InvalidateLiveDatabasesMatching([&aDirectoryLockIds](const auto& database) {
    // If the database is registered in gLiveDatabaseHashtable then it must have
    // a directory lock.
    return IsLockForObjectContainedInLockTable(database, aDirectoryLockIds);
  });
}

void QuotaClient::AbortOperationsForProcess(ContentParentId aContentParentId) {
  AssertIsOnBackgroundThread();

  InvalidateLiveDatabasesMatching([&aContentParentId](const auto& database) {
    return database.IsOwnedByProcess(aContentParentId);
  });
}

void QuotaClient::AbortAllOperations() {
  AssertIsOnBackgroundThread();

  InvalidateLiveDatabasesMatching([](const auto&) { return true; });
}

void QuotaClient::StartIdleMaintenance() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  mBackgroundThread = GetCurrentEventTarget();

  mMaintenanceQueue.EmplaceBack(MakeRefPtr<Maintenance>(this));
  ProcessMaintenanceQueue();
}

void QuotaClient::StopIdleMaintenance() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  if (mCurrentMaintenance) {
    mCurrentMaintenance->Abort();
  }

  for (const auto& maintenance : mMaintenanceQueue) {
    maintenance->Abort();
  }
}

void QuotaClient::InitiateShutdown() {
  AssertIsOnBackgroundThread();

  mShutdownRequested.Flip();

  AbortAllOperations();
}

bool QuotaClient::IsShutdownCompleted() const {
  return (!gFactoryOps || gFactoryOps->IsEmpty()) &&
         (!gLiveDatabaseHashtable || !gLiveDatabaseHashtable->Count()) &&
         !mCurrentMaintenance;
}

void QuotaClient::ForceKillActors() {
  // Currently we don't implement force killing actors.
}

nsCString QuotaClient::GetShutdownStatus() const {
  AssertIsOnBackgroundThread();

  nsCString data;

  if (gFactoryOps && !gFactoryOps->IsEmpty()) {
    data.Append("FactoryOperations: "_ns +
                IntToCString(static_cast<uint32_t>(gFactoryOps->Length())) +
                " ("_ns);

    nsTHashtable<nsCStringHashKey> ids;

    for (auto const& factoryOp : *gFactoryOps) {
      MOZ_ASSERT(factoryOp);

      nsCString id;
      factoryOp->Stringify(id);

      ids.PutEntry(id);
    }

    StringifyTableKeys(ids, data);

    data.Append(")\n");
  }

  if (gLiveDatabaseHashtable && gLiveDatabaseHashtable->Count()) {
    data.Append("LiveDatabases: "_ns +
                IntToCString(gLiveDatabaseHashtable->Count()) + " ("_ns);

    // TODO: This is a basic join-sequence-of-strings operation. Don't we have
    // that available, i.e. something similar to
    // https://searchfox.org/mozilla-central/source/security/sandbox/chromium/base/strings/string_util.cc#940
    nsTHashtable<nsCStringHashKey> ids;

    for (const auto& entry : *gLiveDatabaseHashtable) {
      MOZ_ASSERT(entry.GetData());

      for (const auto& database : entry.GetData()->mLiveDatabases) {
        nsCString id;
        database->Stringify(id);

        ids.PutEntry(id);
      }
    }

    StringifyTableKeys(ids, data);

    data.Append(")\n");
  }

  if (mCurrentMaintenance) {
    data.Append("IdleMaintenance: 1 (");
    mCurrentMaintenance->Stringify(data);
    data.Append(")\n");
  }

  return data;
}

void QuotaClient::FinalizeShutdown() {
  RefPtr<ConnectionPool> connectionPool = gConnectionPool.get();
  if (connectionPool) {
    connectionPool->Shutdown();

    gConnectionPool = nullptr;
  }

  RefPtr<FileHandleThreadPool> fileHandleThreadPool =
      gFileHandleThreadPool.get();
  if (fileHandleThreadPool) {
    fileHandleThreadPool->Shutdown();

    gFileHandleThreadPool = nullptr;
  }

  if (mMaintenanceThreadPool) {
    mMaintenanceThreadPool->Shutdown();
    mMaintenanceThreadPool = nullptr;
  }

  if (mDeleteTimer) {
    MOZ_ALWAYS_SUCCEEDS(mDeleteTimer->Cancel());
    mDeleteTimer = nullptr;
  }
}

void QuotaClient::DeleteTimerCallback(nsITimer* aTimer, void* aClosure) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTimer);

  auto* const self = static_cast<QuotaClient*>(aClosure);
  MOZ_ASSERT(self);
  MOZ_ASSERT(self->mDeleteTimer);
  MOZ_ASSERT(SameCOMIdentity(self->mDeleteTimer, aTimer));

  for (const auto& pendingDeleteInfoEntry : self->mPendingDeleteInfos) {
    const auto& key = pendingDeleteInfoEntry.GetKey();
    const auto& value = pendingDeleteInfoEntry.GetData();
    MOZ_ASSERT(!value->IsEmpty());

    RefPtr<DeleteFilesRunnable> runnable = new DeleteFilesRunnable(
        SafeRefPtr{key, AcquireStrongRefFromRawPtr{}}, std::move(*value));

    MOZ_ASSERT(value->IsEmpty());

    runnable->RunImmediately();
  }

  self->mPendingDeleteInfos.Clear();
}

Result<nsCOMPtr<nsIFile>, nsresult> QuotaClient::GetDirectory(
    PersistenceType aPersistenceType, const nsACString& aOrigin) {
  QuotaManager* const quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  IDB_TRY_INSPECT(const auto& directory, quotaManager->GetDirectoryForOrigin(
                                             aPersistenceType, aOrigin));

  MOZ_ASSERT(directory);

  IDB_TRY(
      directory->Append(NS_LITERAL_STRING_FROM_CSTRING(IDB_DIRECTORY_NAME)));

  return directory;
}

template <QuotaClient::ObsoleteFilenamesHandling ObsoleteFilenames>
Result<QuotaClient::GetDatabaseFilenamesResult<ObsoleteFilenames>, nsresult>
QuotaClient::GetDatabaseFilenames(nsIFile& aDirectory,
                                  const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  GetDatabaseFilenamesResult<ObsoleteFilenames> result;

  IDB_TRY(CollectEachFileAtomicCancelable(
      aDirectory, aCanceled,
      [&result](const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
        IDB_TRY_INSPECT(const auto& leafName, MOZ_TO_RESULT_INVOKE_TYPED(
                                                  nsString, file, GetLeafName));

        IDB_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory:
            result.subdirsToProcess.AppendElement(leafName);
            break;

          case nsIFileKind::ExistsAsFile: {
            if constexpr (ObsoleteFilenames ==
                          ObsoleteFilenamesHandling::Include) {
              if (StringBeginsWith(leafName, kIdbDeletionMarkerFilePrefix)) {
                result.obsoleteFilenames.PutEntry(
                    Substring(leafName, kIdbDeletionMarkerFilePrefix.Length()));
                break;
              }
            }

            // Skip OS metadata files. These files are only used in different
            // platforms, but the profile can be shared across different
            // operating systems, so we check it on all platforms.
            if (QuotaManager::IsOSMetadata(leafName)) {
              break;
            }

            // Skip files starting with ".".
            if (QuotaManager::IsDotFile(leafName)) {
              break;
            }

            // Skip SQLite temporary files. These files take up space on disk
            // but will be deleted as soon as the database is opened, so we
            // don't count them towards quota.
            if (StringEndsWith(leafName, kSQLiteJournalSuffix) ||
                StringEndsWith(leafName, kSQLiteSHMSuffix)) {
              break;
            }

            // The SQLite WAL file does count towards quota, but it is handled
            // below once we find the actual database file.
            if (StringEndsWith(leafName, kSQLiteWALSuffix)) {
              break;
            }

            nsDependentSubstring leafNameBase;
            if (!GetFilenameBase(leafName, kSQLiteSuffix, leafNameBase)) {
              UNKNOWN_FILE_WARNING(leafName);
              break;
            }

            result.databaseFilenames.PutEntry(leafNameBase);
            break;
          }

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return Ok{};
      }));

  return result;
}

void QuotaClient::ProcessMaintenanceQueue() {
  AssertIsOnBackgroundThread();

  if (mCurrentMaintenance || mMaintenanceQueue.IsEmpty()) {
    return;
  }

  mCurrentMaintenance = mMaintenanceQueue[0];
  mMaintenanceQueue.RemoveElementAt(0);

  mCurrentMaintenance->RunImmediately();
}

/*******************************************************************************
 * DeleteFilesRunnable
 ******************************************************************************/

DeleteFilesRunnable::DeleteFilesRunnable(SafeRefPtr<FileManager> aFileManager,
                                         nsTArray<int64_t>&& aFileIds)
    : Runnable("dom::indexeddb::DeleteFilesRunnable"),
      mOwningEventTarget(GetCurrentEventTarget()),
      mFileManager(std::move(aFileManager)),
      mFileIds(std::move(aFileIds)),
      mState(State_Initial) {}

void DeleteFilesRunnable::RunImmediately() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_Initial);

  Unused << this->Run();
}

void DeleteFilesRunnable::Open() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_Initial);

  QuotaManager* const quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    Finish();
    return;
  }

  RefPtr<DirectoryLock> directoryLock = quotaManager->CreateDirectoryLock(
      mFileManager->Type(), mFileManager->OriginMetadata(), quota::Client::IDB,
      /* aExclusive */ false);

  mState = State_DirectoryOpenPending;
  directoryLock->Acquire(this);
}

void DeleteFilesRunnable::DoDatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DatabaseWorkOpen);

  if (!mFileManager->Invalidated()) {
    for (int64_t fileId : mFileIds) {
      if (NS_FAILED(mFileManager->SyncDeleteFile(fileId))) {
        NS_WARNING("Failed to delete file!");
      }
    }
  }

  Finish();
}

void DeleteFilesRunnable::Finish() {
  MOZ_ASSERT(mState != State_UnblockingOpen);

  // Must set mState before dispatching otherwise we will race with the main
  // thread.
  mState = State_UnblockingOpen;

  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
}

void DeleteFilesRunnable::UnblockOpen() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_UnblockingOpen);

  mDirectoryLock = nullptr;

  mState = State_Completed;
}

NS_IMPL_ISUPPORTS_INHERITED0(DeleteFilesRunnable, Runnable)

NS_IMETHODIMP
DeleteFilesRunnable::Run() {
  switch (mState) {
    case State_Initial:
      Open();
      break;

    case State_DatabaseWorkOpen:
      DoDatabaseWork();
      break;

    case State_UnblockingOpen:
      UnblockOpen();
      break;

    case State_DirectoryOpenPending:
    default:
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

void DeleteFilesRunnable::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread
  mState = State_DatabaseWorkOpen;

  IDB_TRY(quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL), QM_VOID,
          [this](const nsresult) { Finish(); });
}

void DeleteFilesRunnable::DirectoryLockFailed() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  Finish();
}

void Maintenance::RegisterDatabaseMaintenance(
    DatabaseMaintenance* aDatabaseMaintenance) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabaseMaintenance);
  MOZ_ASSERT(mState == State::BeginDatabaseMaintenance);
  MOZ_ASSERT(
      !mDatabaseMaintenances.Contains(aDatabaseMaintenance->DatabasePath()));

  mDatabaseMaintenances.InsertOrUpdate(aDatabaseMaintenance->DatabasePath(),
                                       aDatabaseMaintenance);
}

void Maintenance::UnregisterDatabaseMaintenance(
    DatabaseMaintenance* aDatabaseMaintenance) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabaseMaintenance);
  MOZ_ASSERT(mState == State::WaitingForDatabaseMaintenancesToComplete);
  MOZ_ASSERT(mDatabaseMaintenances.Get(aDatabaseMaintenance->DatabasePath()));

  mDatabaseMaintenances.Remove(aDatabaseMaintenance->DatabasePath());

  if (mDatabaseMaintenances.Count()) {
    return;
  }

  mState = State::Finishing;
  Finish();
}

void Maintenance::Stringify(nsACString& aResult) const {
  AssertIsOnBackgroundThread();

  aResult.Append("DatabaseMaintenances: "_ns +
                 IntToCString(mDatabaseMaintenances.Count()) + " ("_ns);

  nsTHashtable<nsCStringHashKey> ids;

  for (const auto& entry : mDatabaseMaintenances) {
    MOZ_ASSERT(entry.GetData());

    nsCString id;
    entry.GetData()->Stringify(id);

    ids.PutEntry(id);
  }

  StringifyTableKeys(ids, aResult);

  aResult.Append(")");
}

nsresult Maintenance::Start() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  // Make sure that the IndexedDatabaseManager is running so that we can check
  // for low disk space mode.

  if (IndexedDatabaseManager::Get()) {
    OpenDirectory();
    return NS_OK;
  }

  mState = State::CreateIndexedDatabaseManager;
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));

  return NS_OK;
}

nsresult Maintenance::CreateIndexedDatabaseManager() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::CreateIndexedDatabaseManager);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  IndexedDatabaseManager* const mgr = IndexedDatabaseManager::GetOrCreate();
  if (NS_WARN_IF(!mgr)) {
    return NS_ERROR_FAILURE;
  }

  mState = State::IndexedDatabaseManagerOpen;
  MOZ_ALWAYS_SUCCEEDS(
      mQuotaClient->BackgroundThread()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult Maintenance::OpenDirectory() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::Initial ||
             mState == State::IndexedDatabaseManagerOpen);
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(QuotaManager::Get());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  // Get a shared lock for <profile>/storage/*/*/idb

  RefPtr<DirectoryLock> directoryLock =
      QuotaManager::Get()->CreateDirectoryLockInternal(
          Nullable<PersistenceType>(), OriginScope::FromNull(),
          Nullable<Client::Type>(Client::IDB),
          /* aExclusive */ false);

  mState = State::DirectoryOpenPending;
  directoryLock->Acquire(this);

  return NS_OK;
}

nsresult Maintenance::DirectoryOpen() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(mDirectoryLock);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  mState = State::DirectoryWorkOpen;

  IDB_TRY(quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL),
          NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult Maintenance::DirectoryWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DirectoryWorkOpen);

  // The storage directory is structured like this:
  //
  //   <profile>/storage/<persistence>/<origin>/idb/*.sqlite
  //
  // We have to find all database files that match any persistence type and any
  // origin. We ignore anything out of the ordinary for now.

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  IDB_TRY(quotaManager->EnsureStorageIsInitialized());

  // Since idle maintenance may occur before temporary storage is initialized,
  // make sure it's initialized here (all non-persistent origins need to be
  // cleaned up and quota info needs to be loaded for them).

  // Don't fail whole idle maintenance in case of an error, the persistent
  // repository can still
  // be processed.
  const bool initTemporaryStorageFailed = [&quotaManager] {
    IDB_TRY(quotaManager->EnsureTemporaryStorageIsInitialized(), true);
    return false;
  }();

  const nsCOMPtr<nsIFile> storageDir =
      GetFileForPath(quotaManager->GetStoragePath());
  IDB_TRY(OkIf(storageDir), NS_ERROR_FAILURE);

  {
    IDB_TRY_INSPECT(const bool& exists,
                    MOZ_TO_RESULT_INVOKE(storageDir, Exists));

    // XXX No warning here?
    if (!exists) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  {
    IDB_TRY_INSPECT(const bool& isDirectory,
                    MOZ_TO_RESULT_INVOKE(storageDir, IsDirectory));

    IDB_TRY(OkIf(isDirectory), NS_ERROR_FAILURE);
  }

  // There are currently only 3 persistence types, and we want to iterate them
  // in this order:
  static const PersistenceType kPersistenceTypes[] = {
      PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_DEFAULT,
      PERSISTENCE_TYPE_TEMPORARY};

  static_assert(
      ArrayLength(kPersistenceTypes) == size_t(PERSISTENCE_TYPE_INVALID),
      "Something changed with available persistence types!");

  constexpr auto idbDirName =
      NS_LITERAL_STRING_FROM_CSTRING(IDB_DIRECTORY_NAME);

  for (const PersistenceType persistenceType : kPersistenceTypes) {
    // Loop over "<persistence>" directories.
    if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
        IsAborted()) {
      return NS_ERROR_ABORT;
    }

    const bool persistent = persistenceType == PERSISTENCE_TYPE_PERSISTENT;

    if (!persistent && initTemporaryStorageFailed) {
      // Non-persistent (best effort) repositories can't be processed if
      // temporary storage initialization failed.
      continue;
    }

    // XXX persistenceType == PERSISTENCE_TYPE_PERSISTENT shouldn't be a special
    // case...
    const auto persistenceTypeString =
        persistenceType == PERSISTENCE_TYPE_PERSISTENT
            ? "permanent"_ns
            : PersistenceTypeToString(persistenceType);

    IDB_TRY_INSPECT(
        const auto& persistenceDir,
        CloneFileAndAppend(*storageDir,
                           NS_ConvertASCIItoUTF16(persistenceTypeString)));

    {
      IDB_TRY_INSPECT(const bool& exists,
                      MOZ_TO_RESULT_INVOKE(persistenceDir, Exists));

      if (!exists) {
        continue;
      }

      IDB_TRY_INSPECT(const bool& isDirectory,
                      MOZ_TO_RESULT_INVOKE(persistenceDir, IsDirectory));

      if (NS_WARN_IF(!isDirectory)) {
        continue;
      }
    }

    // Loop over "<origin>/idb" directories.
    IDB_TRY(CollectEachFile(
        *persistenceDir,
        [this, &quotaManager, persistent, persistenceType, &idbDirName](
            const nsCOMPtr<nsIFile>& originDir) -> Result<Ok, nsresult> {
          if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
              IsAborted()) {
            return Err(NS_ERROR_ABORT);
          }

          IDB_TRY_INSPECT(const auto& dirEntryKind,
                          GetDirEntryKind(*originDir));

          switch (dirEntryKind) {
            case nsIFileKind::ExistsAsFile:
              break;

            case nsIFileKind::ExistsAsDirectory: {
              // Get the necessary information about the origin
              // (LoadFullOriginMetadataWithRestore also checks if it's a valid
              // origin).

              IDB_TRY_INSPECT(
                  const auto& metadata,
                  quotaManager->LoadFullOriginMetadataWithRestore(originDir),
                  // Not much we can do here...
                  Ok{});

              // Don't do any maintenance for private browsing databases, which
              // are only temporary.
              if (OriginAttributes::IsPrivateBrowsing(metadata.mOrigin)) {
                return Ok{};
              }

              if (persistent) {
                // We have to check that all persistent origins are cleaned up,
                // but there's no way to do that by one call, we need to
                // initialize (and possibly clean up) them one by one
                // (EnsureTemporaryStorageIsInitialized cleans up only
                // non-persistent origins).

                IDB_TRY_UNWRAP(
                    const DebugOnly<bool> created,
                    quotaManager->EnsurePersistentOriginIsInitialized(metadata)
                        .map([](const auto& res) { return res.second; }),
                    // Not much we can do here...
                    Ok{});

                // We found this origin directory by traversing the repository,
                // so EnsurePersistentOriginIsInitialized shouldn't report that
                // a new directory has been created.
                MOZ_ASSERT(!created);
              }

              IDB_TRY_INSPECT(const auto& idbDir,
                              CloneFileAndAppend(*originDir, idbDirName));

              IDB_TRY_INSPECT(const bool& exists,
                              MOZ_TO_RESULT_INVOKE(idbDir, Exists));

              if (!exists) {
                return Ok{};
              }

              IDB_TRY_INSPECT(const bool& isDirectory,
                              MOZ_TO_RESULT_INVOKE(idbDir, IsDirectory));

              IDB_TRY(OkIf(isDirectory), Ok{});

              nsTArray<nsString> databasePaths;

              // Loop over files in the "idb" directory.
              IDB_TRY(CollectEachFile(
                  *idbDir,
                  [this, &databasePaths](const nsCOMPtr<nsIFile>& idbDirFile)
                      -> Result<Ok, nsresult> {
                    if (NS_WARN_IF(QuotaClient::
                                       IsShuttingDownOnNonBackgroundThread()) ||
                        IsAborted()) {
                      return Err(NS_ERROR_ABORT);
                    }

                    IDB_TRY_UNWRAP(auto idbFilePath,
                                   MOZ_TO_RESULT_INVOKE_TYPED(
                                       nsString, idbDirFile, GetPath));

                    if (!StringEndsWith(idbFilePath, kSQLiteSuffix)) {
                      return Ok{};
                    }

                    IDB_TRY_INSPECT(const auto& dirEntryKind,
                                    GetDirEntryKind(*idbDirFile));

                    switch (dirEntryKind) {
                      case nsIFileKind::ExistsAsDirectory:
                        break;

                      case nsIFileKind::ExistsAsFile:
                        // Found a database.

                        MOZ_ASSERT(!databasePaths.Contains(idbFilePath));

                        databasePaths.AppendElement(std::move(idbFilePath));
                        break;

                      case nsIFileKind::DoesNotExist:
                        // Ignore files that got removed externally while
                        // iterating.
                        break;
                    }

                    return Ok{};
                  }));

              if (!databasePaths.IsEmpty()) {
                mDirectoryInfos.EmplaceBack(persistenceType, metadata,
                                            std::move(databasePaths));
              }

              break;
            }

            case nsIFileKind::DoesNotExist:
              // Ignore files that got removed externally while iterating.
              break;
          }

          return Ok{};
        }));
  }

  mState = State::BeginDatabaseMaintenance;

  MOZ_ALWAYS_SUCCEEDS(
      mQuotaClient->BackgroundThread()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult Maintenance::BeginDatabaseMaintenance() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::BeginDatabaseMaintenance);

  class MOZ_STACK_CLASS Helper final {
   public:
    static bool IsSafeToRunMaintenance(const nsAString& aDatabasePath) {
      if (gFactoryOps) {
        for (uint32_t index = gFactoryOps->Length(); index > 0; index--) {
          CheckedUnsafePtr<FactoryOp>& existingOp = (*gFactoryOps)[index - 1];

          if (!existingOp->DatabaseFilePathIsKnown()) {
            continue;
          }

          if (existingOp->DatabaseFilePath() == aDatabasePath) {
            return false;
          }
        }
      }

      if (gLiveDatabaseHashtable) {
        return std::all_of(
            gLiveDatabaseHashtable->cbegin(), gLiveDatabaseHashtable->cend(),
            [&aDatabasePath](const auto& liveDatabasesEntry) {
              const auto& liveDatabases =
                  liveDatabasesEntry.GetData()->mLiveDatabases;
              return std::all_of(liveDatabases.cbegin(), liveDatabases.cend(),
                                 [&aDatabasePath](const auto& database) {
                                   return database->FilePath() != aDatabasePath;
                                 });
            });
      }

      return true;
    }
  };

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsAborted()) {
    return NS_ERROR_ABORT;
  }

  RefPtr<nsThreadPool> threadPool;

  for (DirectoryInfo& directoryInfo : mDirectoryInfos) {
    RefPtr<DirectoryLock> directoryLock;

    for (const nsString& databasePath : *directoryInfo.mDatabasePaths) {
      if (Helper::IsSafeToRunMaintenance(databasePath)) {
        if (!directoryLock) {
          directoryLock = mDirectoryLock->Specialize(
              directoryInfo.mPersistenceType,
              *directoryInfo.mFullOriginMetadata, Client::IDB);
          MOZ_ASSERT(directoryLock);
        }

        // No key needs to be passed here, because we skip encrypted databases
        // in DoDirectoryWork as long as they are only used in private browsing
        // mode.
        const auto databaseMaintenance = MakeRefPtr<DatabaseMaintenance>(
            this, directoryLock, directoryInfo.mPersistenceType,
            *directoryInfo.mFullOriginMetadata, databasePath, Nothing{});

        if (!threadPool) {
          threadPool = mQuotaClient->GetOrCreateThreadPool();
          MOZ_ASSERT(threadPool);
        }

        MOZ_ALWAYS_SUCCEEDS(
            threadPool->Dispatch(databaseMaintenance, NS_DISPATCH_NORMAL));

        RegisterDatabaseMaintenance(databaseMaintenance);
      }
    }
  }

  mDirectoryInfos.Clear();

  mDirectoryLock = nullptr;

  if (mDatabaseMaintenances.Count()) {
    mState = State::WaitingForDatabaseMaintenancesToComplete;
  } else {
    mState = State::Finishing;
    Finish();
  }

  return NS_OK;
}

void Maintenance::Finish() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(mState == State::Finishing);

  if (NS_FAILED(mResultCode)) {
    nsCString errorName;
    GetErrorName(mResultCode, errorName);

    IDB_WARNING("Maintenance finished with error: %s", errorName.get());
  }

  // It can happen that we are only referenced by mCurrentMaintenance which is
  // cleared in NoteFinishedMaintenance()
  const RefPtr<Maintenance> kungFuDeathGrip = this;

  mQuotaClient->NoteFinishedMaintenance(this);

  mState = State::Complete;
}

NS_IMPL_ISUPPORTS_INHERITED0(Maintenance, Runnable)

NS_IMETHODIMP
Maintenance::Run() {
  MOZ_ASSERT(mState != State::Complete);

  const auto handleError = [this](const nsresult rv) {
    if (mState != State::Finishing) {
      if (NS_SUCCEEDED(mResultCode)) {
        mResultCode = rv;
      }

      // Must set mState before dispatching otherwise we will race with the
      // owning thread.
      mState = State::Finishing;

      if (IsOnBackgroundThread()) {
        Finish();
      } else {
        MOZ_ALWAYS_SUCCEEDS(mQuotaClient->BackgroundThread()->Dispatch(
            this, NS_DISPATCH_NORMAL));
      }
    }
  };

  switch (mState) {
    case State::Initial:
      IDB_TRY(Start(), NS_OK, handleError);
      break;

    case State::CreateIndexedDatabaseManager:
      IDB_TRY(CreateIndexedDatabaseManager(), NS_OK, handleError);
      break;

    case State::IndexedDatabaseManagerOpen:
      IDB_TRY(OpenDirectory(), NS_OK, handleError);
      break;

    case State::DirectoryWorkOpen:
      IDB_TRY(DirectoryWork(), NS_OK, handleError);
      break;

    case State::BeginDatabaseMaintenance:
      IDB_TRY(BeginDatabaseMaintenance(), NS_OK, handleError);
      break;

    case State::Finishing:
      Finish();
      break;

    default:
      MOZ_CRASH("Bad state!");
  }

  return NS_OK;
}

void Maintenance::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  nsresult rv = DirectoryOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    mState = State::Finishing;
    Finish();

    return;
  }
}

void Maintenance::DirectoryLockFailed() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = NS_ERROR_FAILURE;
  }

  mState = State::Finishing;
  Finish();
}

void DatabaseMaintenance::Stringify(nsACString& aResult) const {
  AssertIsOnBackgroundThread();

  aResult.AppendLiteral("Origin:");
  aResult.Append(AnonymizedOriginString(mOriginMetadata.mOrigin));
  aResult.Append(kQuotaGenericDelimiter);

  aResult.AppendLiteral("PersistenceType:");
  aResult.Append(PersistenceTypeToString(mPersistenceType));
  aResult.Append(kQuotaGenericDelimiter);

  aResult.AppendLiteral("Duration:");
  aResult.AppendInt((PR_Now() - mMaintenance->StartTime()) / PR_USEC_PER_MSEC);
}

void DatabaseMaintenance::PerformMaintenanceOnDatabase() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mMaintenance);
  MOZ_ASSERT(mMaintenance->StartTime());
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mDatabasePath.IsEmpty());
  MOZ_ASSERT(!mOriginMetadata.mGroup.IsEmpty());
  MOZ_ASSERT(!mOriginMetadata.mOrigin.IsEmpty());

  class MOZ_STACK_CLASS AutoClose final {
    NotNull<nsCOMPtr<mozIStorageConnection>> mConnection;

   public:
    explicit AutoClose(
        MovingNotNull<nsCOMPtr<mozIStorageConnection>> aConnection)
        : mConnection(std::move(aConnection)) {}

    ~AutoClose() { MOZ_ALWAYS_SUCCEEDS(mConnection->Close()); }
  };

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return;
  }

  const nsCOMPtr<nsIFile> databaseFile = GetFileForPath(mDatabasePath);
  MOZ_ASSERT(databaseFile);

  IDB_TRY_UNWRAP(
      const NotNull<nsCOMPtr<mozIStorageConnection>> connection,
      GetStorageConnection(*databaseFile, mDirectoryLockId,
                           TelemetryIdForFile(databaseFile), mMaybeKey),
      QM_VOID);

  AutoClose autoClose(connection);

  AutoProgressHandler progressHandler(mMaintenance);
  if (NS_WARN_IF(NS_FAILED(progressHandler.Register(connection)))) {
    return;
  }

  bool databaseIsOk;
  nsresult rv = CheckIntegrity(*connection, &databaseIsOk);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_WARN_IF(!databaseIsOk)) {
    // XXX Handle this somehow! Probably need to clear all storage for the
    //     origin. Needs followup.
    MOZ_ASSERT(false, "Database corruption detected!");
    return;
  }

  MaintenanceAction maintenanceAction;
  rv =
      DetermineMaintenanceAction(*connection, databaseFile, &maintenanceAction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  switch (maintenanceAction) {
    case MaintenanceAction::Nothing:
      break;

    case MaintenanceAction::IncrementalVacuum:
      IncrementalVacuum(*connection);
      break;

    case MaintenanceAction::FullVacuum:
      FullVacuum(*connection, databaseFile);
      break;

    default:
      MOZ_CRASH("Unknown MaintenanceAction!");
  }
}

nsresult DatabaseMaintenance::CheckIntegrity(mozIStorageConnection& aConnection,
                                             bool* aOk) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aOk);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return NS_ERROR_ABORT;
  }

  // First do a full integrity_check. Scope statements tightly here because
  // later operations require zero live statements.
  {
    IDB_TRY_INSPECT(const auto& stmt,
                    CreateAndExecuteSingleStepStatement(
                        aConnection, "PRAGMA integrity_check(1);"_ns));

    IDB_TRY_INSPECT(const auto& result,
                    MOZ_TO_RESULT_INVOKE_TYPED(nsString, *stmt, GetString, 0));

    IDB_TRY(OkIf(result.EqualsLiteral("ok")), NS_OK,
            [&aOk](const auto) { *aOk = false; });
  }

  // Now enable and check for foreign key constraints.
  {
    IDB_TRY_INSPECT(const int32_t& foreignKeysWereEnabled,
                    ([&aConnection]() -> Result<int32_t, nsresult> {
                      IDB_TRY_INSPECT(
                          const auto& stmt,
                          CreateAndExecuteSingleStepStatement(
                              aConnection, "PRAGMA foreign_keys;"_ns));

                      IDB_TRY_RETURN(MOZ_TO_RESULT_INVOKE(*stmt, GetInt32, 0));
                    }()));

    if (!foreignKeysWereEnabled) {
      IDB_TRY(aConnection.ExecuteSimpleSQL("PRAGMA foreign_keys = ON;"_ns));
    }

    IDB_TRY_INSPECT(const bool& foreignKeyError,
                    CreateAndExecuteSingleStepStatement<
                        SingleStepResult::ReturnNullIfNoResult>(
                        aConnection, "PRAGMA foreign_key_check;"_ns));

    if (!foreignKeysWereEnabled) {
      IDB_TRY(aConnection.ExecuteSimpleSQL("PRAGMA foreign_keys = OFF;"_ns));
    }

    if (foreignKeyError) {
      *aOk = false;
      return NS_OK;
    }
  }

  *aOk = true;
  return NS_OK;
}

nsresult DatabaseMaintenance::DetermineMaintenanceAction(
    mozIStorageConnection& aConnection, nsIFile* aDatabaseFile,
    MaintenanceAction* aMaintenanceAction) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aDatabaseFile);
  MOZ_ASSERT(aMaintenanceAction);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return NS_ERROR_ABORT;
  }

  IDB_TRY_INSPECT(const int32_t& schemaVersion,
                  MOZ_TO_RESULT_INVOKE(aConnection, GetSchemaVersion));

  // Don't do anything if the schema version is less than 18; before that
  // version no databases had |auto_vacuum == INCREMENTAL| set and we didn't
  // track the values needed for the heuristics below.
  if (schemaVersion < MakeSchemaVersion(18, 0)) {
    *aMaintenanceAction = MaintenanceAction::Nothing;
    return NS_OK;
  }

  // This method shouldn't make any permanent changes to the database, so make
  // sure everything gets rolled back when we leave.
  mozStorageTransaction transaction(&aConnection,
                                    /* aCommitOnComplete */ false);

  IDB_TRY(transaction.Start())

  // Check to see when we last vacuumed this database.
  IDB_TRY_INSPECT(const auto& stmt,
                  CreateAndExecuteSingleStepStatement(
                      aConnection,
                      "SELECT last_vacuum_time, last_vacuum_size "
                      "FROM database;"_ns));

  IDB_TRY_INSPECT(const PRTime& lastVacuumTime,
                  MOZ_TO_RESULT_INVOKE(*stmt, GetInt64, 0));

  IDB_TRY_INSPECT(const int64_t& lastVacuumSize,
                  MOZ_TO_RESULT_INVOKE(*stmt, GetInt64, 1));

  NS_ASSERTION(lastVacuumSize > 0,
               "Thy last vacuum size shall be greater than zero, less than "
               "zero shall thy last vacuum size not be. Zero is right out.");

  const PRTime startTime = mMaintenance->StartTime();

  // This shouldn't really be possible...
  if (NS_WARN_IF(startTime <= lastVacuumTime)) {
    *aMaintenanceAction = MaintenanceAction::Nothing;
    return NS_OK;
  }

  if (startTime - lastVacuumTime < kMinVacuumAge) {
    *aMaintenanceAction = MaintenanceAction::IncrementalVacuum;
    return NS_OK;
  }

  // It has been more than a week since the database was vacuumed, so gather
  // statistics on its usage to see if vacuuming is worthwhile.

  // Create a temporary copy of the dbstat table to speed up the queries that
  // come later.
  IDB_TRY(aConnection.ExecuteSimpleSQL(
      "CREATE VIRTUAL TABLE __stats__ USING dbstat;"
      "CREATE TEMP TABLE __temp_stats__ AS SELECT * FROM __stats__;"_ns));

  {  // Calculate the percentage of the database pages that are not in
     // contiguous order.
    IDB_TRY_INSPECT(
        const auto& stmt,
        CreateAndExecuteSingleStepStatement(
            aConnection,
            "SELECT SUM(__ts1__.pageno != __ts2__.pageno + 1) * 100.0 / "
            "COUNT(*) "
            "FROM __temp_stats__ AS __ts1__, __temp_stats__ AS __ts2__ "
            "WHERE __ts1__.name = __ts2__.name "
            "AND __ts1__.rowid = __ts2__.rowid + 1;"_ns));

    IDB_TRY_INSPECT(const int32_t& percentUnordered,
                    MOZ_TO_RESULT_INVOKE(*stmt, GetInt32, 0));

    MOZ_ASSERT(percentUnordered >= 0);
    MOZ_ASSERT(percentUnordered <= 100);

    if (percentUnordered >= kPercentUnorderedThreshold) {
      *aMaintenanceAction = MaintenanceAction::FullVacuum;
      return NS_OK;
    }
  }

  // Don't try a full vacuum if the file hasn't grown by 10%.
  IDB_TRY_INSPECT(const int64_t& currentFileSize,
                  MOZ_TO_RESULT_INVOKE(aDatabaseFile, GetFileSize));

  if (currentFileSize <= lastVacuumSize ||
      (((currentFileSize - lastVacuumSize) * 100 / currentFileSize) <
       kPercentFileSizeGrowthThreshold)) {
    *aMaintenanceAction = MaintenanceAction::IncrementalVacuum;
    return NS_OK;
  }

  {  // See if there are any free pages that we can reclaim.
    IDB_TRY_INSPECT(const auto& stmt,
                    CreateAndExecuteSingleStepStatement(
                        aConnection, "PRAGMA freelist_count;"_ns));

    IDB_TRY_INSPECT(const int32_t& freelistCount,
                    MOZ_TO_RESULT_INVOKE(*stmt, GetInt32, 0));

    MOZ_ASSERT(freelistCount >= 0);

    // If we have too many free pages then we should try an incremental
    // vacuum. If that causes too much fragmentation then we'll try a full
    // vacuum later.
    if (freelistCount > kMaxFreelistThreshold) {
      *aMaintenanceAction = MaintenanceAction::IncrementalVacuum;
      return NS_OK;
    }
  }

  {  // Calculate the percentage of unused bytes on pages in the database.
    IDB_TRY_INSPECT(
        const auto& stmt,
        CreateAndExecuteSingleStepStatement(
            aConnection,
            "SELECT SUM(unused) * 100.0 / SUM(pgsize) FROM __temp_stats__;"_ns));

    IDB_TRY_INSPECT(const int32_t& percentUnused,
                    MOZ_TO_RESULT_INVOKE(*stmt, GetInt32, 0));

    MOZ_ASSERT(percentUnused >= 0);
    MOZ_ASSERT(percentUnused <= 100);

    *aMaintenanceAction = percentUnused >= kPercentUnusedThreshold
                              ? MaintenanceAction::FullVacuum
                              : MaintenanceAction::IncrementalVacuum;
  }

  return NS_OK;
}

void DatabaseMaintenance::IncrementalVacuum(
    mozIStorageConnection& aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return;
  }

  nsresult rv = aConnection.ExecuteSimpleSQL("PRAGMA incremental_vacuum;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void DatabaseMaintenance::FullVacuum(mozIStorageConnection& aConnection,
                                     nsIFile* aDatabaseFile) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aDatabaseFile);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      mMaintenance->IsAborted()) {
    return;
  }

  IDB_TRY(aConnection.ExecuteSimpleSQL("VACUUM;"_ns), QM_VOID);

  const PRTime vacuumTime = PR_Now();
  MOZ_ASSERT(vacuumTime > 0);

  IDB_TRY_INSPECT(const int64_t& fileSize,
                  MOZ_TO_RESULT_INVOKE(aDatabaseFile, GetFileSize), QM_VOID);

  MOZ_ASSERT(fileSize > 0);

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  IDB_TRY_INSPECT(const auto& stmt,
                  MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageStatement>,
                                             aConnection, CreateStatement,
                                             "UPDATE database "
                                             "SET last_vacuum_time = :time"
                                             ", last_vacuum_size = :size;"_ns),
                  QM_VOID);

  IDB_TRY(stmt->BindInt64ByIndex(0, vacuumTime), QM_VOID);

  IDB_TRY(stmt->BindInt64ByIndex(1, fileSize), QM_VOID);

  IDB_TRY(stmt->Execute(), QM_VOID);
}

void DatabaseMaintenance::RunOnOwningThread() {
  AssertIsOnBackgroundThread();

  mDirectoryLock = nullptr;

  if (mCompleteCallback) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mCompleteCallback.forget()));
  }

  mMaintenance->UnregisterDatabaseMaintenance(this);
}

void DatabaseMaintenance::RunOnConnectionThread() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  PerformMaintenanceOnDatabase();

  MOZ_ALWAYS_SUCCEEDS(
      mMaintenance->BackgroundThread()->Dispatch(this, NS_DISPATCH_NORMAL));
}

NS_IMETHODIMP
DatabaseMaintenance::Run() {
  if (IsOnBackgroundThread()) {
    RunOnOwningThread();
  } else {
    RunOnConnectionThread();
  }

  return NS_OK;
}

nsresult DatabaseMaintenance::AutoProgressHandler::Register(
    NotNull<mozIStorageConnection*> aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  // We want to quickly bail out of any operation if the user becomes active, so
  // use a small granularity here since database performance isn't critical.
  static const int32_t kProgressGranularity = 50;

  nsCOMPtr<mozIStorageProgressHandler> oldHandler;
  nsresult rv = aConnection->SetProgressHandler(kProgressGranularity, this,
                                                getter_AddRefs(oldHandler));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!oldHandler);
  mConnection.init(aConnection);

  return NS_OK;
}

void DatabaseMaintenance::AutoProgressHandler::Unregister() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mConnection);

  nsCOMPtr<mozIStorageProgressHandler> oldHandler;
  nsresult rv =
      mConnection->get()->RemoveProgressHandler(getter_AddRefs(oldHandler));
  Unused << NS_WARN_IF(NS_FAILED(rv));

  MOZ_ASSERT_IF(NS_SUCCEEDED(rv), oldHandler == this);
}

NS_IMETHODIMP_(MozExternalRefCountType)
DatabaseMaintenance::AutoProgressHandler::AddRef() {
  NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);

#ifdef DEBUG
  mDEBUGRefCnt++;
#endif
  return 2;
}

NS_IMETHODIMP_(MozExternalRefCountType)
DatabaseMaintenance::AutoProgressHandler::Release() {
  NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);

#ifdef DEBUG
  mDEBUGRefCnt--;
#endif
  return 1;
}

NS_IMPL_QUERY_INTERFACE(DatabaseMaintenance::AutoProgressHandler,
                        mozIStorageProgressHandler)

NS_IMETHODIMP
DatabaseMaintenance::AutoProgressHandler::OnProgress(
    mozIStorageConnection* aConnection, bool* _retval) {
  NS_ASSERT_OWNINGTHREAD(DatabaseMaintenance::AutoProgressHandler);
  MOZ_ASSERT(*mConnection == aConnection);
  MOZ_ASSERT(_retval);

  *_retval = QuotaClient::IsShuttingDownOnNonBackgroundThread() ||
             mMaintenance->IsAborted();

  return NS_OK;
}

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

// static
nsAutoCString DatabaseOperationBase::MaybeGetBindingClauseForKeyRange(
    const Maybe<SerializedKeyRange>& aOptionalKeyRange,
    const nsACString& aKeyColumnName) {
  return aOptionalKeyRange.isSome()
             ? GetBindingClauseForKeyRange(aOptionalKeyRange.ref(),
                                           aKeyColumnName)
             : nsAutoCString{};
}

// static
nsAutoCString DatabaseOperationBase::GetBindingClauseForKeyRange(
    const SerializedKeyRange& aKeyRange, const nsACString& aKeyColumnName) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!aKeyColumnName.IsEmpty());

  constexpr auto andStr = " AND "_ns;
  constexpr auto spacecolon = " :"_ns;

  nsAutoCString result;
  if (aKeyRange.isOnly()) {
    // Both keys equal.
    result =
        andStr + aKeyColumnName + " ="_ns + spacecolon + kStmtParamNameLowerKey;
  } else {
    if (!aKeyRange.lower().IsUnset()) {
      // Lower key is set.
      result.Append(andStr + aKeyColumnName);
      result.AppendLiteral(" >");
      if (!aKeyRange.lowerOpen()) {
        result.AppendLiteral("=");
      }
      result.Append(spacecolon + kStmtParamNameLowerKey);
    }

    if (!aKeyRange.upper().IsUnset()) {
      // Upper key is set.
      result.Append(andStr + aKeyColumnName);
      result.AppendLiteral(" <");
      if (!aKeyRange.upperOpen()) {
        result.AppendLiteral("=");
      }
      result.Append(spacecolon + kStmtParamNameUpperKey);
    }
  }

  MOZ_ASSERT(!result.IsEmpty());

  return result;
}

// static
uint64_t DatabaseOperationBase::ReinterpretDoubleAsUInt64(double aDouble) {
  // This is a duplicate of the js engine's byte munging in StructuredClone.cpp
  return BitwiseCast<uint64_t>(aDouble);
}

// static
template <typename KeyTransformation>
nsresult DatabaseOperationBase::MaybeBindKeyToStatement(
    const Key& aKey, mozIStorageStatement* const aStatement,
    const nsCString& aParameterName,
    const KeyTransformation& aKeyTransformation) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aStatement);

  if (!aKey.IsUnset()) {
    // XXX This case distinction could be avoided if IDB_TRY_INSPECT would also
    // work with a function not returning a Result<V, E> but simply a V (which
    // is const Key& here) and then assuming it is always a success. Or the
    // transformation could be changed to return Result<const V&, void> but I
    // don't think that Result supports that at the moment.
    if constexpr (std::is_reference_v<
                      std::invoke_result_t<KeyTransformation, Key>>) {
      IDB_TRY(
          aKeyTransformation(aKey).BindToStatement(aStatement, aParameterName));
    } else {
      IDB_TRY_INSPECT(const auto& transformedKey, aKeyTransformation(aKey));
      IDB_TRY(transformedKey.BindToStatement(aStatement, aParameterName));
    }
  }

  return NS_OK;
}

// static
template <typename KeyTransformation>
nsresult DatabaseOperationBase::BindTransformedKeyRangeToStatement(
    const SerializedKeyRange& aKeyRange, mozIStorageStatement* const aStatement,
    const KeyTransformation& aKeyTransformation) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aStatement);

  IDB_TRY(MaybeBindKeyToStatement(aKeyRange.lower(), aStatement,
                                  kStmtParamNameLowerKey, aKeyTransformation));

  if (aKeyRange.isOnly()) {
    return NS_OK;
  }

  IDB_TRY(MaybeBindKeyToStatement(aKeyRange.upper(), aStatement,
                                  kStmtParamNameUpperKey, aKeyTransformation));

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::BindKeyRangeToStatement(
    const SerializedKeyRange& aKeyRange,
    mozIStorageStatement* const aStatement) {
  return BindTransformedKeyRangeToStatement(
      aKeyRange, aStatement, [](const Key& key) -> const auto& { return key; });
}

// static
nsresult DatabaseOperationBase::BindKeyRangeToStatement(
    const SerializedKeyRange& aKeyRange, mozIStorageStatement* const aStatement,
    const nsCString& aLocale) {
  MOZ_ASSERT(!aLocale.IsEmpty());

  return BindTransformedKeyRangeToStatement(
      aKeyRange, aStatement,
      [&aLocale](const Key& key) { return key.ToLocaleAwareKey(aLocale); });
}

// static
void CommonOpenOpHelperBase::AppendConditionClause(
    const nsACString& aColumnName, const nsACString& aStatementParameterName,
    bool aLessThan, bool aEquals, nsCString& aResult) {
  aResult += " AND "_ns + aColumnName + " "_ns;

  if (aLessThan) {
    aResult.Append('<');
  } else {
    aResult.Append('>');
  }

  if (aEquals) {
    aResult.Append('=');
  }

  aResult += " :"_ns + aStatementParameterName;
}

// static
Result<IndexDataValuesAutoArray, nsresult>
DatabaseOperationBase::IndexDataValuesFromUpdateInfos(
    const nsTArray<IndexUpdateInfo>& aUpdateInfos,
    const UniqueIndexTable& aUniqueIndexTable) {
  MOZ_ASSERT_IF(!aUpdateInfos.IsEmpty(), aUniqueIndexTable.Count());

  AUTO_PROFILER_LABEL("DatabaseOperationBase::IndexDataValuesFromUpdateInfos",
                      DOM);

  // XXX We could use TransformIntoNewArray here if it allowed to specify that
  // an AutoArray should be created.
  IndexDataValuesAutoArray indexValues;

  if (NS_WARN_IF(!indexValues.SetCapacity(aUpdateInfos.Length(), fallible))) {
    IDB_REPORT_INTERNAL_ERR();
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  std::transform(aUpdateInfos.cbegin(), aUpdateInfos.cend(),
                 MakeBackInserter(indexValues),
                 [&aUniqueIndexTable](const IndexUpdateInfo& updateInfo) {
                   const IndexOrObjectStoreId& indexId = updateInfo.indexId();

                   bool unique = false;
                   MOZ_ALWAYS_TRUE(aUniqueIndexTable.Get(indexId, &unique));

                   return IndexDataValue{indexId, unique, updateInfo.value(),
                                         updateInfo.localizedValue()};
                 });
  indexValues.Sort();

  return indexValues;
}

// static
nsresult DatabaseOperationBase::InsertIndexTableRows(
    DatabaseConnection* aConnection, const IndexOrObjectStoreId aObjectStoreId,
    const Key& aObjectStoreKey, const nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());

  AUTO_PROFILER_LABEL("DatabaseOperationBase::InsertIndexTableRows", DOM);

  const uint32_t count = aIndexValues.Length();
  if (!count) {
    return NS_OK;
  }

  auto insertUniqueStmt = DatabaseConnection::LazyStatement{
      *aConnection,
      "INSERT INTO unique_index_data "
      "(index_id, value, object_store_id, "
      "object_data_key, value_locale) "
      "VALUES (:"_ns +
          kStmtParamNameIndexId + ", :"_ns + kStmtParamNameValue + ", :"_ns +
          kStmtParamNameObjectStoreId + ", :"_ns + kStmtParamNameObjectDataKey +
          ", :"_ns + kStmtParamNameValueLocale + ");"_ns};
  auto insertStmt = DatabaseConnection::LazyStatement{
      *aConnection,
      "INSERT OR IGNORE INTO index_data "
      "(index_id, value, object_data_key, "
      "object_store_id, value_locale) "
      "VALUES (:"_ns +
          kStmtParamNameIndexId + ", :"_ns + kStmtParamNameValue + ", :"_ns +
          kStmtParamNameObjectDataKey + ", :"_ns + kStmtParamNameObjectStoreId +
          ", :"_ns + kStmtParamNameValueLocale + ");"_ns};

  for (uint32_t index = 0; index < count; index++) {
    const IndexDataValue& info = aIndexValues[index];

    auto& stmt = info.mUnique ? insertUniqueStmt : insertStmt;

    IDB_TRY_INSPECT(const auto& borrowedStmt, stmt.Borrow());

    IDB_TRY(
        borrowedStmt->BindInt64ByName(kStmtParamNameIndexId, info.mIndexId));
    IDB_TRY(
        info.mPosition.BindToStatement(&*borrowedStmt, kStmtParamNameValue));
    IDB_TRY(info.mLocaleAwarePosition.BindToStatement(
        &*borrowedStmt, kStmtParamNameValueLocale));
    IDB_TRY(borrowedStmt->BindInt64ByName(kStmtParamNameObjectStoreId,
                                          aObjectStoreId));
    IDB_TRY(aObjectStoreKey.BindToStatement(&*borrowedStmt,
                                            kStmtParamNameObjectDataKey));

    IDB_TRY(MOZ_TO_RESULT_INVOKE(&*borrowedStmt, Execute)
                .orElse([&info, index,
                         &aIndexValues](nsresult rv) -> Result<Ok, nsresult> {
                  if (rv == NS_ERROR_STORAGE_CONSTRAINT && info.mUnique) {
                    // If we're inserting multiple entries for the same unique
                    // index, then we might have failed to insert due to
                    // colliding with another entry for the same index in which
                    // case we should ignore it.
                    for (int32_t index2 = int32_t(index) - 1;
                         index2 >= 0 &&
                         aIndexValues[index2].mIndexId == info.mIndexId;
                         --index2) {
                      if (info.mPosition == aIndexValues[index2].mPosition) {
                        // We found a key with the same value for the same
                        // index. So we must have had a collision with a value
                        // we just inserted.
                        return Ok{};
                      }
                    }
                  }

                  return Err(rv);
                }));
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::DeleteIndexDataTableRows(
    DatabaseConnection* aConnection, const Key& aObjectStoreKey,
    const nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());

  AUTO_PROFILER_LABEL("DatabaseOperationBase::DeleteIndexDataTableRows", DOM);

  const uint32_t count = aIndexValues.Length();
  if (!count) {
    return NS_OK;
  }

  auto deleteUniqueStmt = DatabaseConnection::LazyStatement{
      *aConnection, "DELETE FROM unique_index_data WHERE index_id = :"_ns +
                        kStmtParamNameIndexId + " AND value = :"_ns +
                        kStmtParamNameValue + ";"_ns};
  auto deleteStmt = DatabaseConnection::LazyStatement{
      *aConnection, "DELETE FROM index_data WHERE index_id = :"_ns +
                        kStmtParamNameIndexId + " AND value = :"_ns +
                        kStmtParamNameValue + " AND object_data_key = :"_ns +
                        kStmtParamNameObjectDataKey + ";"_ns};

  for (uint32_t index = 0; index < count; index++) {
    const IndexDataValue& indexValue = aIndexValues[index];

    auto& stmt = indexValue.mUnique ? deleteUniqueStmt : deleteStmt;

    IDB_TRY_INSPECT(const auto& borrowedStmt, stmt.Borrow());

    IDB_TRY(borrowedStmt->BindInt64ByName(kStmtParamNameIndexId,
                                          indexValue.mIndexId));

    IDB_TRY(indexValue.mPosition.BindToStatement(&*borrowedStmt,
                                                 kStmtParamNameValue));

    if (!indexValue.mUnique) {
      IDB_TRY(aObjectStoreKey.BindToStatement(&*borrowedStmt,
                                              kStmtParamNameObjectDataKey));
    }

    IDB_TRY(borrowedStmt->Execute());
  }

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::DeleteObjectStoreDataTableRowsWithIndexes(
    DatabaseConnection* aConnection, const IndexOrObjectStoreId aObjectStoreId,
    const Maybe<SerializedKeyRange>& aKeyRange) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aObjectStoreId);

#ifdef DEBUG
  {
    IDB_TRY_INSPECT(const bool& hasIndexes,
                    ObjectStoreHasIndexes(*aConnection, aObjectStoreId),
                    QM_PROPAGATE, [](const auto&) { MOZ_ASSERT(false); });
    MOZ_ASSERT(hasIndexes,
               "Don't use this slow method if there are no indexes!");
  }
#endif

  AUTO_PROFILER_LABEL(
      "DatabaseOperationBase::DeleteObjectStoreDataTableRowsWithIndexes", DOM);

  const bool singleRowOnly = aKeyRange.isSome() && aKeyRange.ref().isOnly();

  Key objectStoreKey;
  IDB_TRY_INSPECT(
      const auto& selectStmt,
      ([singleRowOnly, &aConnection, &objectStoreKey, &aKeyRange]()
           -> Result<CachingDatabaseConnection::BorrowedStatement, nsresult> {
        if (singleRowOnly) {
          IDB_TRY_UNWRAP(auto selectStmt,
                         aConnection->BorrowCachedStatement(
                             "SELECT index_data_values "
                             "FROM object_data "
                             "WHERE object_store_id = :"_ns +
                             kStmtParamNameObjectStoreId + " AND key = :"_ns +
                             kStmtParamNameKey + ";"_ns));

          objectStoreKey = aKeyRange.ref().lower();

          IDB_TRY(
              objectStoreKey.BindToStatement(&*selectStmt, kStmtParamNameKey));

          return selectStmt;
        }

        const auto keyRangeClause =
            MaybeGetBindingClauseForKeyRange(aKeyRange, kColumnNameKey);

        IDB_TRY_UNWRAP(
            auto selectStmt,
            aConnection->BorrowCachedStatement(
                "SELECT index_data_values, "_ns + kColumnNameKey +
                " FROM object_data WHERE object_store_id = :"_ns +
                kStmtParamNameObjectStoreId + keyRangeClause + ";"_ns));

        if (aKeyRange.isSome()) {
          IDB_TRY(BindKeyRangeToStatement(aKeyRange.ref(), &*selectStmt));
        }

        return selectStmt;
      }()));

  IDB_TRY(
      selectStmt->BindInt64ByName(kStmtParamNameObjectStoreId, aObjectStoreId));

  DebugOnly<uint32_t> resultCountDEBUG = 0;

  IDB_TRY(CollectWhileHasResult(
      *selectStmt,
      [singleRowOnly, aObjectStoreId, &objectStoreKey, &aConnection,
       &resultCountDEBUG, indexValues = IndexDataValuesAutoArray{},
       deleteStmt = DatabaseConnection::LazyStatement{
           *aConnection,
           "DELETE FROM object_data "
           "WHERE object_store_id = :"_ns +
               kStmtParamNameObjectStoreId + " AND key = :"_ns +
               kStmtParamNameKey +
               ";"_ns}](auto& selectStmt) mutable -> Result<Ok, nsresult> {
        if (!singleRowOnly) {
          IDB_TRY(objectStoreKey.SetFromStatement(&selectStmt, 1));

          indexValues.ClearAndRetainStorage();
        }

        IDB_TRY(ReadCompressedIndexDataValues(selectStmt, 0, indexValues));
        IDB_TRY(
            DeleteIndexDataTableRows(aConnection, objectStoreKey, indexValues));

        IDB_TRY_INSPECT(const auto& borrowedDeleteStmt, deleteStmt.Borrow());

        IDB_TRY(borrowedDeleteStmt->BindInt64ByName(kStmtParamNameObjectStoreId,
                                                    aObjectStoreId));
        IDB_TRY(objectStoreKey.BindToStatement(&*borrowedDeleteStmt,
                                               kStmtParamNameKey));
        IDB_TRY(borrowedDeleteStmt->Execute());

        resultCountDEBUG++;

        return Ok{};
      }));

  MOZ_ASSERT_IF(singleRowOnly, resultCountDEBUG <= 1);

  return NS_OK;
}

// static
nsresult DatabaseOperationBase::UpdateIndexValues(
    DatabaseConnection* aConnection, const IndexOrObjectStoreId aObjectStoreId,
    const Key& aObjectStoreKey, const nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());

  AUTO_PROFILER_LABEL("DatabaseOperationBase::UpdateIndexValues", DOM);

  IDB_TRY_UNWRAP((auto [indexDataValues, indexDataValuesLength]),
                 MakeCompressedIndexDataValues(aIndexValues));

  MOZ_ASSERT(!indexDataValuesLength == !(indexDataValues.get()));

  IDB_TRY(aConnection->ExecuteCachedStatement(
      "UPDATE object_data SET index_data_values = :"_ns +
          kStmtParamNameIndexDataValues + " WHERE object_store_id = :"_ns +
          kStmtParamNameObjectStoreId + " AND key = :"_ns + kStmtParamNameKey +
          ";"_ns,
      [&indexDataValues = indexDataValues,
       indexDataValuesLength = indexDataValuesLength, aObjectStoreId,
       &aObjectStoreKey](
          mozIStorageStatement& updateStmt) -> Result<Ok, nsresult> {
        IDB_TRY(indexDataValues
                    ? updateStmt.BindAdoptedBlobByName(
                          kStmtParamNameIndexDataValues,
                          indexDataValues.release(), indexDataValuesLength)
                    : updateStmt.BindNullByName(kStmtParamNameIndexDataValues));

        IDB_TRY(updateStmt.BindInt64ByName(kStmtParamNameObjectStoreId,
                                           aObjectStoreId));

        IDB_TRY(
            aObjectStoreKey.BindToStatement(&updateStmt, kStmtParamNameKey));

        return Ok{};
      }));

  return NS_OK;
}

// static
Result<bool, nsresult> DatabaseOperationBase::ObjectStoreHasIndexes(
    DatabaseConnection& aConnection,
    const IndexOrObjectStoreId aObjectStoreId) {
  aConnection.AssertIsOnConnectionThread();
  MOZ_ASSERT(aObjectStoreId);

  IDB_TRY_RETURN(aConnection
                     .BorrowAndExecuteSingleStepStatement(
                         "SELECT id "
                         "FROM object_store_index "
                         "WHERE object_store_id = :"_ns +
                             kStmtParamNameObjectStoreId + kOpenLimit + "1;"_ns,
                         [aObjectStoreId](auto& stmt) -> Result<Ok, nsresult> {
                           IDB_TRY(stmt.BindInt64ByName(
                               kStmtParamNameObjectStoreId, aObjectStoreId));
                           return Ok{};
                         })
                     .map(IsSome));
}

NS_IMPL_ISUPPORTS_INHERITED(DatabaseOperationBase, Runnable,
                            mozIStorageProgressHandler)

NS_IMETHODIMP
DatabaseOperationBase::OnProgress(mozIStorageConnection* aConnection,
                                  bool* _retval) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(_retval);

  // This is intentionally racy.
  *_retval = QuotaClient::IsShuttingDownOnNonBackgroundThread() ||
             !OperationMayProceed();
  return NS_OK;
}

DatabaseOperationBase::AutoSetProgressHandler::AutoSetProgressHandler()
    : mConnection(Nothing())
#ifdef DEBUG
      ,
      mDEBUGDatabaseOp(nullptr)
#endif
{
  MOZ_ASSERT(!IsOnBackgroundThread());
}

DatabaseOperationBase::AutoSetProgressHandler::~AutoSetProgressHandler() {
  MOZ_ASSERT(!IsOnBackgroundThread());

  if (mConnection) {
    Unregister();
  }
}

nsresult DatabaseOperationBase::AutoSetProgressHandler::Register(
    mozIStorageConnection& aConnection, DatabaseOperationBase* aDatabaseOp) {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aDatabaseOp);
  MOZ_ASSERT(!mConnection);

  IDB_TRY_UNWRAP(
      const DebugOnly oldProgressHandler,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageProgressHandler>,
                                 aConnection, SetProgressHandler,
                                 kStorageProgressGranularity, aDatabaseOp));

  MOZ_ASSERT(!oldProgressHandler.inspect());

  mConnection = SomeRef(aConnection);
#ifdef DEBUG
  mDEBUGDatabaseOp = aDatabaseOp;
#endif

  return NS_OK;
}

void DatabaseOperationBase::AutoSetProgressHandler::Unregister() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mConnection);

  nsCOMPtr<mozIStorageProgressHandler> oldHandler;
  MOZ_ALWAYS_SUCCEEDS(
      mConnection->RemoveProgressHandler(getter_AddRefs(oldHandler)));
  MOZ_ASSERT(oldHandler == mDEBUGDatabaseOp);

  mConnection = Nothing();
}

MutableFile::MutableFile(nsIFile* aFile, SafeRefPtr<Database> aDatabase,
                         SafeRefPtr<FileInfo> aFileInfo)
    : BackgroundMutableFileParentBase(FILE_HANDLE_STORAGE_IDB, aDatabase->Id(),
                                      IntToString(aFileInfo->Id()), aFile),
      mDatabase(std::move(aDatabase)),
      mFileInfo(std::move(aFileInfo)) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mFileInfo);
}

MutableFile::~MutableFile() { mDatabase->UnregisterMutableFile(this); }

RefPtr<MutableFile> MutableFile::Create(nsIFile* aFile,
                                        SafeRefPtr<Database> aDatabase,
                                        SafeRefPtr<FileInfo> aFileInfo) {
  AssertIsOnBackgroundThread();

  RefPtr<MutableFile> newMutableFile =
      new MutableFile(aFile, aDatabase.clonePtr(), std::move(aFileInfo));

  if (!aDatabase->RegisterMutableFile(newMutableFile)) {
    return nullptr;
  }

  return newMutableFile;
}

void MutableFile::NoteActiveState() {
  AssertIsOnBackgroundThread();

  mDatabase->NoteActiveMutableFile();
}

void MutableFile::NoteInactiveState() {
  AssertIsOnBackgroundThread();

  mDatabase->NoteInactiveMutableFile();
}

PBackgroundParent* MutableFile::GetBackgroundParent() const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  return GetDatabase().GetBackgroundParent();
}

already_AddRefed<nsISupports> MutableFile::CreateStream(bool aReadOnly) {
  AssertIsOnBackgroundThread();

  const PersistenceType persistenceType = mDatabase->Type();
  const auto& originMetadata = mDatabase->OriginMetadata();

  nsCOMPtr<nsISupports> result;

  if (aReadOnly) {
    IDB_TRY_INSPECT(
        const auto& stream,
        CreateFileInputStream(persistenceType, originMetadata, Client::IDB,
                              mFile, -1, -1, nsIFileInputStream::DEFER_OPEN),
        nullptr);
    result = NS_ISUPPORTS_CAST(nsIFileInputStream*, stream.get());
  } else {
    IDB_TRY_INSPECT(
        const auto& stream,
        CreateFileStream(persistenceType, originMetadata, Client::IDB, mFile,
                         -1, -1, nsIFileStream::DEFER_OPEN),
        nullptr);
    result = NS_ISUPPORTS_CAST(nsIFileStream*, stream.get());
  }

  return result.forget();
}

already_AddRefed<BlobImpl> MutableFile::CreateBlobImpl() {
  AssertIsOnBackgroundThread();

  // This doesn't use CreateFileBlobImpl as mutable files cannot be encrypted.
  auto blobImpl = MakeRefPtr<FileBlobImpl>(mFile);
  blobImpl->SetFileId(mFileInfo->Id());

  return blobImpl.forget();
}

PBackgroundFileHandleParent* MutableFile::AllocPBackgroundFileHandleParent(
    const FileMode& aMode) {
  AssertIsOnBackgroundThread();

  // Once a database is closed it must not try to open new file handles.
  if (NS_WARN_IF(mDatabase->IsClosed())) {
    if (!mDatabase->IsInvalidated()) {
      ASSERT_UNLESS_FUZZING();
    }
    return nullptr;
  }

  if (!gFileHandleThreadPool) {
    RefPtr<FileHandleThreadPool> fileHandleThreadPool =
        FileHandleThreadPool::Create();
    if (NS_WARN_IF(!fileHandleThreadPool)) {
      return nullptr;
    }

    gFileHandleThreadPool = fileHandleThreadPool;
  }

  return BackgroundMutableFileParentBase::AllocPBackgroundFileHandleParent(
      aMode);
}

mozilla::ipc::IPCResult MutableFile::RecvPBackgroundFileHandleConstructor(
    PBackgroundFileHandleParent* aActor, const FileMode& aMode) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mDatabase->IsClosed());

  if (NS_WARN_IF(mDatabase->IsInvalidated())) {
    // This is an expected race. We don't want the child to die here, just don't
    // actually do any work.
    return IPC_OK();
  }

  return BackgroundMutableFileParentBase::RecvPBackgroundFileHandleConstructor(
      aActor, aMode);
}

mozilla::ipc::IPCResult MutableFile::RecvGetFileId(int64_t* aFileId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mFileInfo);

  if (NS_WARN_IF(!IndexedDatabaseManager::InTestingMode())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  *aFileId = mFileInfo->Id();
  return IPC_OK();
}

FactoryOp::FactoryOp(SafeRefPtr<Factory> aFactory,
                     RefPtr<ContentParent> aContentParent,
                     const CommonFactoryRequestParams& aCommonParams,
                     bool aDeleting)
    : DatabaseOperationBase(aFactory->GetLoggingInfo()->Id(),
                            aFactory->GetLoggingInfo()->NextRequestSN()),
      mFactory(std::move(aFactory)),
      mContentParent(std::move(aContentParent)),
      mCommonParams(aCommonParams),
      mDirectoryLockId(-1),
      mState(State::Initial),
      mWaitingForPermissionRetry(false),
      mEnforcingQuota(true),
      mDeleting(aDeleting),
      mChromeWriteAccessAllowed(false),
      mFileHandleDisabled(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mFactory);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
}

void FactoryOp::NoteDatabaseBlocked(Database* aDatabase) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(mMaybeBlockedDatabases.Contains(aDatabase));

  // Only send the blocked event if all databases have reported back. If the
  // database was closed then it will have been removed from the array.
  // Otherwise if it was blocked its |mBlocked| flag will be true.
  bool sendBlockedEvent = true;

  for (auto& info : mMaybeBlockedDatabases) {
    if (info == aDatabase) {
      // This database was blocked, mark accordingly.
      info.mBlocked = true;
    } else if (!info.mBlocked) {
      // A database has not yet reported back yet, don't send the event yet.
      sendBlockedEvent = false;
    }
  }

  if (sendBlockedEvent) {
    SendBlockedNotification();
  }
}

void FactoryOp::NoteDatabaseClosed(Database* const aDatabase) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(mMaybeBlockedDatabases.Contains(aDatabase));

  mMaybeBlockedDatabases.RemoveElement(aDatabase);

  if (!mMaybeBlockedDatabases.IsEmpty()) {
    return;
  }

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(mDatabaseId, &info));
  MOZ_ASSERT(info->mWaitingFactoryOp == this);

  if (AreActorsAlive()) {
    // The IPDL strong reference has not yet been released, so we can clear
    // mWaitingFactoryOp immediately.
    info->mWaitingFactoryOp = nullptr;

    WaitForTransactions();
    return;
  }

  // The IPDL strong reference has been released, mWaitingFactoryOp holds the
  // last strong reference to us, so we need to move it to a stack variable
  // instead of clearing it immediately (We could clear it immediately if only
  // the other actor is destroyed, but we don't need to optimize for that, and
  // move it anyway).
  const RefPtr<FactoryOp> waitingFactoryOp = std::move(info->mWaitingFactoryOp);

  IDB_REPORT_INTERNAL_ERR();
  SetFailureCodeIfUnset(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  // We hold a strong ref in waitingFactoryOp, so it's safe to call Run()
  // directly.

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(Run());
}

void FactoryOp::StringifyState(nsACString& aResult) const {
  AssertIsOnOwningThread();

  switch (mState) {
    case State::Initial:
      aResult.AppendLiteral("Initial");
      return;

    case State::PermissionChallenge:
      aResult.AppendLiteral("PermissionChallenge");
      return;

    case State::PermissionRetry:
      aResult.AppendLiteral("PermissionRetry");
      return;

    case State::FinishOpen:
      aResult.AppendLiteral("FinishOpen");
      return;

    case State::QuotaManagerPending:
      aResult.AppendLiteral("QuotaManagerPending");
      return;

    case State::DirectoryOpenPending:
      aResult.AppendLiteral("DirectoryOpenPending");
      return;

    case State::DatabaseOpenPending:
      aResult.AppendLiteral("DatabaseOpenPending");
      return;

    case State::DatabaseWorkOpen:
      aResult.AppendLiteral("DatabaseWorkOpen");
      return;

    case State::BeginVersionChange:
      aResult.AppendLiteral("BeginVersionChange");
      return;

    case State::WaitingForOtherDatabasesToClose:
      aResult.AppendLiteral("WaitingForOtherDatabasesToClose");
      return;

    case State::WaitingForTransactionsToComplete:
      aResult.AppendLiteral("WaitingForTransactionsToComplete");
      return;

    case State::DatabaseWorkVersionChange:
      aResult.AppendLiteral("DatabaseWorkVersionChange");
      return;

    case State::SendingResults:
      aResult.AppendLiteral("SendingResults");
      return;

    case State::Completed:
      aResult.AppendLiteral("Completed");
      return;

    default:
      MOZ_CRASH("Bad state!");
  }
}

void FactoryOp::Stringify(nsACString& aResult) const {
  AssertIsOnOwningThread();

  aResult.AppendLiteral("PersistenceType:");
  aResult.Append(
      PersistenceTypeToString(mCommonParams.metadata().persistenceType()));
  aResult.Append(kQuotaGenericDelimiter);

  aResult.AppendLiteral("Origin:");
  aResult.Append(AnonymizedOriginString(mOriginMetadata.mOrigin));
  aResult.Append(kQuotaGenericDelimiter);

  aResult.AppendLiteral("State:");
  StringifyState(aResult);
}

nsresult FactoryOp::Open() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Initial);

  // Move this to the stack now to ensure that we release it on this thread.
  const RefPtr<ContentParent> contentParent = std::move(mContentParent);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  IDB_TRY_INSPECT(const auto& permission, CheckPermission(contentParent));

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
             permission == PermissionRequestBase::kPermissionDenied ||
             permission == PermissionRequestBase::kPermissionPrompt);

  if (permission == PermissionRequestBase::kPermissionDenied) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  {
    // These services have to be started on the main thread currently.

    IndexedDatabaseManager* mgr;
    if (NS_WARN_IF(!(mgr = IndexedDatabaseManager::GetOrCreate()))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    nsCOMPtr<mozIStorageService> ss;
    if (NS_WARN_IF(!(ss = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID)))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  const DatabaseMetadata& metadata = mCommonParams.metadata();

  QuotaManager::GetStorageId(metadata.persistenceType(),
                             mOriginMetadata.mOrigin, Client::IDB, mDatabaseId);

  mDatabaseId.Append('*');
  mDatabaseId.Append(NS_ConvertUTF16toUTF8(metadata.name()));

  if (permission == PermissionRequestBase::kPermissionPrompt) {
    mState = State::PermissionChallenge;
    MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
    return NS_OK;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed);

  if (mInPrivateBrowsing) {
    const auto lockedPrivateBrowsingInfoHashtable =
        gPrivateBrowsingInfoHashtable->Lock();

    lockedPrivateBrowsingInfoHashtable->LookupOrInsertWith(mDatabaseId, [] {
      IndexedDBCipherStrategy cipherStrategy;

      // XXX Generate key using proper random data, such that we can ensure
      // the use of unique IVs per key by discriminating by database's file
      // id & offset.
      auto keyOrErr = cipherStrategy.GenerateKey();

      // XXX Propagate the error to the caller rather than asserting.
      MOZ_RELEASE_ASSERT(keyOrErr.isOk());

      return keyOrErr.unwrap();
    });
  }

  mState = State::FinishOpen;
  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult FactoryOp::ChallengePermission() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::PermissionChallenge);

  const PrincipalInfo& principalInfo = mCommonParams.principalInfo();
  MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

  if (NS_WARN_IF(!SendPermissionChallenge(principalInfo))) {
    return NS_ERROR_FAILURE;
  }

  mWaitingForPermissionRetry = true;

  return NS_OK;
}

void FactoryOp::PermissionRetry() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::PermissionChallenge);

  mContentParent = BackgroundParent::GetContentParent(Manager()->Manager());

  mState = State::PermissionRetry;

  mWaitingForPermissionRetry = false;

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
}

nsresult FactoryOp::RetryCheckPermission() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::PermissionRetry);
  MOZ_ASSERT(mCommonParams.principalInfo().type() ==
             PrincipalInfo::TContentPrincipalInfo);

  // Move this to the stack now to ensure that we release it on this thread.
  const RefPtr<ContentParent> contentParent = std::move(mContentParent);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  IDB_TRY_INSPECT(const auto& permission, CheckPermission(contentParent));

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
             permission == PermissionRequestBase::kPermissionDenied ||
             permission == PermissionRequestBase::kPermissionPrompt);

  if (permission == PermissionRequestBase::kPermissionDenied ||
      permission == PermissionRequestBase::kPermissionPrompt) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed);

  mState = State::FinishOpen;
  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult FactoryOp::DirectoryOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(gFactoryOps);

  // See if this FactoryOp needs to wait.
  const bool delayed =
      std::any_of(
          gFactoryOps->rbegin(), gFactoryOps->rend(),
          [foundThis = false, &self = *this](const auto& existingOp) mutable {
            if (existingOp == &self) {
              foundThis = true;
              return false;
            }

            if (foundThis && self.MustWaitFor(*existingOp)) {
              // Only one op can be delayed.
              MOZ_ASSERT(!existingOp->mDelayedOp);
              existingOp->mDelayedOp = &self;
              return true;
            }

            return false;
          }) ||
      [&self = *this] {
        QuotaClient* quotaClient = QuotaClient::GetInstance();
        MOZ_ASSERT(quotaClient);

        if (RefPtr<Maintenance> currentMaintenance =
                quotaClient->GetCurrentMaintenance()) {
          if (RefPtr<DatabaseMaintenance> databaseMaintenance =
                  currentMaintenance->GetDatabaseMaintenance(
                      self.mDatabaseFilePath)) {
            databaseMaintenance->WaitForCompletion(&self);
            return true;
          }
        }

        return false;
      }();

  mState = State::DatabaseOpenPending;
  if (!delayed) {
    IDB_TRY(DatabaseOpen());
  }

  return NS_OK;
}

nsresult FactoryOp::SendToIOThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseOpenPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread.
  mState = State::DatabaseWorkOpen;

  IDB_TRY(quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL),
          NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, IDB_REPORT_INTERNAL_ERR_LAMBDA);

  return NS_OK;
}

void FactoryOp::WaitForTransactions() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::BeginVersionChange ||
             mState == State::WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mDatabaseId.IsEmpty());
  MOZ_ASSERT(!IsActorDestroyed());

  mState = State::WaitingForTransactionsToComplete;

  RefPtr<WaitForTransactionsHelper> helper =
      new WaitForTransactionsHelper(mDatabaseId, this);
  helper->WaitForTransactions();
}

void FactoryOp::CleanupMetadata() {
  AssertIsOnOwningThread();

  if (mDelayedOp) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mDelayedOp.forget()));
  }

  MOZ_ASSERT(gFactoryOps);
  gFactoryOps->RemoveElement(this);

  // We might get here even after QuotaManagerOpen failed, so we need to check
  // if we have a quota manager. If we don't, we obviously are not in quota
  // manager shutdown.
  if (auto* const quotaManager = QuotaManager::Get()) {
    quotaManager->MaybeRecordShutdownStep(
        quota::Client::IDB, "An element was removed from gFactoryOps"_ns);
  }

  // Match the IncreaseBusyCount in AllocPBackgroundIDBFactoryRequestParent().
  DecreaseBusyCount();
}

void FactoryOp::FinishSendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(mFactory);

  mState = State::Completed;

  // Make sure to release the factory on this thread.
  mFactory = nullptr;
}

Result<PermissionRequestBase::PermissionValue, nsresult>
FactoryOp::CheckPermission(ContentParent* aContentParent) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Initial || mState == State::PermissionRetry);

  const PrincipalInfo& principalInfo = mCommonParams.principalInfo();
  if (principalInfo.type() != PrincipalInfo::TSystemPrincipalInfo) {
    if (principalInfo.type() != PrincipalInfo::TContentPrincipalInfo) {
      if (aContentParent) {
        // We just want ContentPrincipalInfo or SystemPrincipalInfo.
        aContentParent->KillHard("IndexedDB CheckPermission 0");
      }

      return Err(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    }

    const ContentPrincipalInfo& contentPrincipalInfo =
        principalInfo.get_ContentPrincipalInfo();
    if (contentPrincipalInfo.attrs().mPrivateBrowsingId != 0) {
      if (StaticPrefs::dom_indexedDB_privateBrowsing_enabled()) {
        // XXX Not sure if this should be done from here, it goes beyond
        // checking the permissions.
        mInPrivateBrowsing.Flip();
      } else {
        return Err(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
      }
    }
  }

  mFileHandleDisabled = !Preferences::GetBool(kPrefFileHandleEnabled);

  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  MOZ_ASSERT(principalInfo.type() != PrincipalInfo::TNullPrincipalInfo);

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    MOZ_ASSERT(mState == State::Initial);
    MOZ_ASSERT(persistenceType == PERSISTENCE_TYPE_PERSISTENT);

    if (aContentParent) {
      // Check to make sure that the child process has access to the database it
      // is accessing.
      NS_ConvertUTF16toUTF8 databaseName(mCommonParams.metadata().name());

      const nsAutoCString permissionStringWrite =
          kPermissionStringBase + databaseName + kPermissionWriteSuffix;
      const nsAutoCString permissionStringRead =
          kPermissionStringBase + databaseName + kPermissionReadSuffix;

      bool canWrite = CheckAtLeastOneAppHasPermission(aContentParent,
                                                      permissionStringWrite);

      bool canRead;
      if (canWrite) {
        MOZ_ASSERT(CheckAtLeastOneAppHasPermission(aContentParent,
                                                   permissionStringRead));
        canRead = true;
      } else {
        canRead = CheckAtLeastOneAppHasPermission(aContentParent,
                                                  permissionStringRead);
      }

      // Deleting a database requires write permissions.
      if (mDeleting && !canWrite) {
        aContentParent->KillHard("IndexedDB CheckPermission 2");
        IDB_REPORT_INTERNAL_ERR();
        return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      }

      // Opening or deleting requires read permissions.
      if (!canRead) {
        aContentParent->KillHard("IndexedDB CheckPermission 3");
        IDB_REPORT_INTERNAL_ERR();
        return Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      }

      mChromeWriteAccessAllowed = canWrite;
    } else {
      mChromeWriteAccessAllowed = true;
    }

    if (State::Initial == mState) {
      mOriginMetadata = {QuotaManager::GetInfoForChrome(), persistenceType};

      MOZ_ASSERT(QuotaManager::IsOriginInternal(mOriginMetadata.mOrigin));

      mEnforcingQuota = false;
    }

    return PermissionRequestBase::kPermissionAllowed;
  }

  MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

  IDB_TRY_INSPECT(const auto& principal,
                  PrincipalInfoToPrincipal(principalInfo));

  IDB_TRY_UNWRAP(auto principalMetadata,
                 QuotaManager::GetInfoFromPrincipal(principal));

  IDB_TRY_INSPECT(
      const auto& permission,
      ([persistenceType, &origin = principalMetadata.mOrigin,
        &principal = *principal]()
           -> mozilla::Result<PermissionRequestBase::PermissionValue,
                              nsresult> {
        if (persistenceType == PERSISTENCE_TYPE_PERSISTENT) {
          if (QuotaManager::IsOriginInternal(origin)) {
            return PermissionRequestBase::kPermissionAllowed;
          }
#ifdef IDB_MOBILE
          return Err(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
#else
          return PermissionRequestBase::GetCurrentPermission(principal);
#endif
        }
        return PermissionRequestBase::kPermissionAllowed;
      })());

  if (permission != PermissionRequestBase::kPermissionDenied &&
      State::Initial == mState) {
    mOriginMetadata = {std::move(principalMetadata), persistenceType};

    mEnforcingQuota = persistenceType != PERSISTENCE_TYPE_PERSISTENT;
  }

  return permission;
}

nsresult FactoryOp::SendVersionChangeMessages(
    DatabaseActorInfo* aDatabaseActorInfo, Maybe<Database&> aOpeningDatabase,
    uint64_t aOldVersion, const Maybe<uint64_t>& aNewVersion) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabaseActorInfo);
  MOZ_ASSERT(mState == State::BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(!IsActorDestroyed());

  const uint32_t expectedCount = mDeleting ? 0 : 1;
  const uint32_t liveCount = aDatabaseActorInfo->mLiveDatabases.Length();
  if (liveCount > expectedCount) {
    nsTArray<MaybeBlockedDatabaseInfo> maybeBlockedDatabases;
    for (const auto& database : aDatabaseActorInfo->mLiveDatabases) {
      if ((!aOpeningDatabase || database.get() != &aOpeningDatabase.ref()) &&
          !database->IsClosed() &&
          NS_WARN_IF(!maybeBlockedDatabases.AppendElement(
              SafeRefPtr{database.get(), AcquireStrongRefFromRawPtr{}},
              fallible))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    mMaybeBlockedDatabases = std::move(maybeBlockedDatabases);
  }

  // We don't want to wait forever if we were not able to send the
  // message.
  mMaybeBlockedDatabases.RemoveLastElements(
      mMaybeBlockedDatabases.end() -
      std::remove_if(mMaybeBlockedDatabases.begin(),
                     mMaybeBlockedDatabases.end(),
                     [aOldVersion, &aNewVersion](auto& maybeBlockedDatabase) {
                       return !maybeBlockedDatabase->SendVersionChange(
                           aOldVersion, aNewVersion);
                     }));

  return NS_OK;
}  // namespace indexedDB

// static
bool FactoryOp::CheckAtLeastOneAppHasPermission(
    ContentParent* aContentParent, const nsACString& aPermissionString) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContentParent);
  MOZ_ASSERT(!aPermissionString.IsEmpty());

  return true;
}

nsresult FactoryOp::FinishOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::FinishOpen);
  MOZ_ASSERT(!mContentParent);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (QuotaManager::Get()) {
    IDB_TRY(OpenDirectory());

    return NS_OK;
  }

  mState = State::QuotaManagerPending;
  QuotaManager::GetOrCreate(this);

  return NS_OK;
}

nsresult FactoryOp::QuotaManagerOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::QuotaManagerPending);

  IDB_TRY(OkIf(QuotaManager::Get()), NS_ERROR_FAILURE);

  IDB_TRY(OpenDirectory());

  return NS_OK;
}

nsresult FactoryOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::FinishOpen ||
             mState == State::QuotaManagerPending);
  MOZ_ASSERT(!mOriginMetadata.mOrigin.IsEmpty());
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(QuotaManager::Get());

  const PersistenceType persistenceType =
      mCommonParams.metadata().persistenceType();

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Need to get database file path before opening the directory.
  // XXX: For what reason?
  IDB_TRY_UNWRAP(
      mDatabaseFilePath,
      ([this, quotaManager,
        persistenceType]() -> mozilla::Result<nsString, nsresult> {
        IDB_TRY_INSPECT(const auto& dbFile,
                        quotaManager->GetDirectoryForOrigin(
                            persistenceType, mOriginMetadata.mOrigin));

        IDB_TRY(
            dbFile->Append(NS_LITERAL_STRING_FROM_CSTRING(IDB_DIRECTORY_NAME)));

        IDB_TRY(dbFile->Append(
            GetDatabaseFilenameBase(mCommonParams.metadata().name()) +
            kSQLiteSuffix));

        IDB_TRY_RETURN(MOZ_TO_RESULT_INVOKE_TYPED(nsString, dbFile, GetPath));
      }()));

  RefPtr<DirectoryLock> directoryLock = quotaManager->CreateDirectoryLock(
      persistenceType, mOriginMetadata, Client::IDB,
      /* aExclusive */ false);

  mState = State::DirectoryOpenPending;

  directoryLock->Acquire(this);

  return NS_OK;
}

bool FactoryOp::MustWaitFor(const FactoryOp& aExistingOp) {
  AssertIsOnOwningThread();

  // Things for the same persistence type, the same origin and the same
  // database must wait.
  return aExistingOp.mCommonParams.metadata().persistenceType() ==
             mCommonParams.metadata().persistenceType() &&
         aExistingOp.mOriginMetadata.mOrigin == mOriginMetadata.mOrigin &&
         aExistingOp.mDatabaseId == mDatabaseId;
}

NS_IMPL_ISUPPORTS_INHERITED0(FactoryOp, DatabaseOperationBase)

// Run() assumes that the caller holds a strong reference to the object that
// can't be cleared while Run() is being executed.
// So if you call Run() directly (as opposed to dispatching to an event queue)
// you need to make sure there's such a reference.
// See bug 1356824 for more details.
NS_IMETHODIMP
FactoryOp::Run() {
  const auto handleError = [this](const nsresult rv) {
    if (mState != State::SendingResults) {
      SetFailureCodeIfUnset(rv);

      // Must set mState before dispatching otherwise we will race with the
      // owning thread.
      mState = State::SendingResults;

      if (IsOnOwningThread()) {
        SendResults();
      } else {
        MOZ_ALWAYS_SUCCEEDS(
            mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
      }
    }
  };

  switch (mState) {
    case State::Initial:
      IDB_TRY(Open(), NS_OK, handleError);
      break;

    case State::PermissionChallenge:
      IDB_TRY(ChallengePermission(), NS_OK, handleError);
      break;

    case State::PermissionRetry:
      IDB_TRY(RetryCheckPermission(), NS_OK, handleError);
      break;

    case State::FinishOpen:
      IDB_TRY(FinishOpen(), NS_OK, handleError);
      break;

    case State::QuotaManagerPending:
      IDB_TRY(QuotaManagerOpen(), NS_OK, handleError);
      break;

    case State::DatabaseOpenPending:
      IDB_TRY(DatabaseOpen(), NS_OK, handleError);
      break;

    case State::DatabaseWorkOpen:
      IDB_TRY(DoDatabaseWork(), NS_OK, handleError);
      break;

    case State::BeginVersionChange:
      IDB_TRY(BeginVersionChange(), NS_OK, handleError);
      break;

    case State::WaitingForTransactionsToComplete:
      IDB_TRY(DispatchToWorkThread(), NS_OK, handleError);
      break;

    case State::SendingResults:
      SendResults();
      break;

    default:
      MOZ_CRASH("Bad state!");
  }

  return NS_OK;
}

void FactoryOp::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  MOZ_ASSERT(mDirectoryLock->Id() >= 0);
  mDirectoryLockId = mDirectoryLock->Id();

  IDB_TRY(DirectoryOpen(), QM_VOID, [this](const nsresult rv) {
    SetFailureCodeIfUnset(rv);

    // The caller holds a strong reference to us, no need for a self reference
    // before calling Run().

    mState = State::SendingResults;
    MOZ_ALWAYS_SUCCEEDS(Run());
  });
}

void FactoryOp::DirectoryLockFailed() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  if (!HasFailed()) {
    IDB_REPORT_INTERNAL_ERR();
    SetFailureCode(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  // The caller holds a strong reference to us, no need for a self reference
  // before calling Run().

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(Run());
}

void FactoryOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();

  // Assume ActorDestroy can happen at any time, so we can't probe the current
  // state since mState can be modified on any thread (only one thread at a time
  // based on the state machine).  However we can use mWaitingForPermissionRetry
  // which is only touched on the owning thread.  If mWaitingForPermissionRetry
  // is true, we can also modify mState since we are guaranteed that there are
  // no pending runnables which would probe mState to decide what code needs to
  // run (there shouldn't be any running runnables on other threads either).

  if (mWaitingForPermissionRetry) {
    PermissionRetry();
  }

  // We don't have to handle the case when mWaitingForPermissionRetry is not
  // true since it means that either nothing has been initialized yet, so
  // nothing to cleanup or there are pending runnables that will detect that the
  // actor has been destroyed and cleanup accordingly.
}

mozilla::ipc::IPCResult FactoryOp::RecvPermissionRetry() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!IsActorDestroyed());

  PermissionRetry();

  return IPC_OK();
}

OpenDatabaseOp::OpenDatabaseOp(SafeRefPtr<Factory> aFactory,
                               RefPtr<ContentParent> aContentParent,
                               const CommonFactoryRequestParams& aParams)
    : FactoryOp(std::move(aFactory), std::move(aContentParent), aParams,
                /* aDeleting */ false),
      mMetadata(MakeSafeRefPtr<FullDatabaseMetadata>(aParams.metadata())),
      mRequestedVersion(aParams.metadata().version()),
      mVersionChangeOp(nullptr),
      mTelemetryId(0) {
  if (mContentParent) {
    // This is a little scary but it looks safe to call this off the main thread
    // for now.
    mOptionalContentParentId = Some(mContentParent->ChildID());
  }
}

void OpenDatabaseOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  FactoryOp::ActorDestroy(aWhy);

  if (mVersionChangeOp) {
    mVersionChangeOp->NoteActorDestroyed();
  }
}

nsresult OpenDatabaseOp::DatabaseOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseOpenPending);

  nsresult rv = SendToIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult OpenDatabaseOp::DoDatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DatabaseWorkOpen);

  AUTO_PROFILER_LABEL("OpenDatabaseOp::DoDatabaseWork", DOM);

  IDB_TRY(OkIf(!QuotaClient::IsShuttingDownOnNonBackgroundThread()),
          NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, IDB_REPORT_INTERNAL_ERR_LAMBDA);

  if (!OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const nsString& databaseName = mCommonParams.metadata().name();
  const PersistenceType persistenceType =
      mCommonParams.metadata().persistenceType();

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  IDB_TRY(quotaManager->EnsureStorageIsInitialized());

  IDB_TRY_INSPECT(
      const auto& dbDirectory,
      ([persistenceType, &quotaManager, this]()
           -> mozilla::Result<std::pair<nsCOMPtr<nsIFile>, bool>, nsresult> {
        if (persistenceType == PERSISTENCE_TYPE_PERSISTENT) {
          IDB_TRY_RETURN(quotaManager->EnsurePersistentOriginIsInitialized(
              mOriginMetadata));
        }

        IDB_TRY(quotaManager->EnsureTemporaryStorageIsInitialized());
        IDB_TRY_RETURN(quotaManager->EnsureTemporaryOriginIsInitialized(
            persistenceType, mOriginMetadata));
      }()
                  .map([](const auto& res) { return res.first; })));

  IDB_TRY(
      dbDirectory->Append(NS_LITERAL_STRING_FROM_CSTRING(IDB_DIRECTORY_NAME)));

  {
    IDB_TRY_INSPECT(const bool& exists,
                    MOZ_TO_RESULT_INVOKE(dbDirectory, Exists));

    if (!exists) {
      IDB_TRY(dbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755));
    }
#ifdef DEBUG
    else {
      bool isDirectory;
      MOZ_ASSERT(NS_SUCCEEDED(dbDirectory->IsDirectory(&isDirectory)));
      MOZ_ASSERT(isDirectory);
    }
#endif
  }

  const auto databaseFilenameBase = GetDatabaseFilenameBase(databaseName);

  IDB_TRY_INSPECT(
      const auto& markerFile,
      CloneFileAndAppend(*dbDirectory,
                         kIdbDeletionMarkerFilePrefix + databaseFilenameBase));

  IDB_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(markerFile, Exists));

  if (exists) {
    // Delete the database and directroy since they should be deleted in
    // previous operation.
    // Note: only update usage to the QuotaManager when mEnforcingQuota == true
    IDB_TRY(RemoveDatabaseFilesAndDirectory(
        *dbDirectory, databaseFilenameBase,
        mEnforcingQuota ? quotaManager : nullptr, persistenceType,
        mOriginMetadata, databaseName));
  }

  IDB_TRY_INSPECT(
      const auto& dbFile,
      CloneFileAndAppend(*dbDirectory, databaseFilenameBase + kSQLiteSuffix));

  mTelemetryId = TelemetryIdForFile(dbFile);

#ifdef DEBUG
  {
    IDB_TRY_INSPECT(const auto& databaseFilePath,
                    MOZ_TO_RESULT_INVOKE_TYPED(nsString, dbFile, GetPath));

    MOZ_ASSERT(databaseFilePath == mDatabaseFilePath);
  }
#endif

  IDB_TRY_INSPECT(
      const auto& fmDirectory,
      CloneFileAndAppend(*dbDirectory, databaseFilenameBase +
                                           kFileManagerDirectoryNameSuffix));

  Maybe<const CipherKey> maybeKey;
  if (mInPrivateBrowsing) {
    CipherKey key;

    {
      const auto lockedPrivateBrowsingInfoHashtable =
          gPrivateBrowsingInfoHashtable->Lock();
      MOZ_ALWAYS_TRUE(
          lockedPrivateBrowsingInfoHashtable->Get(mDatabaseId, &key));
    }

    maybeKey.emplace(std::move(key));
  }

  IDB_TRY_UNWRAP(
      NotNull<nsCOMPtr<mozIStorageConnection>> connection,
      CreateStorageConnection(*dbFile, *fmDirectory, databaseName,
                              mOriginMetadata.mOrigin, mDirectoryLockId,
                              mTelemetryId, maybeKey));

  AutoSetProgressHandler asph;
  IDB_TRY(asph.Register(*connection, this));

  IDB_TRY(LoadDatabaseInformation(*connection));

  MOZ_ASSERT(mMetadata->mNextObjectStoreId > mMetadata->mObjectStores.Count());
  MOZ_ASSERT(mMetadata->mNextIndexId > 0);

  // See if we need to do a versionchange transaction

  // Optional version semantics.
  if (!mRequestedVersion) {
    // If the requested version was not specified and the database was created,
    // treat it as if version 1 were requested.
    // Otherwise, treat it as if the current version were requested.
    mRequestedVersion = mMetadata->mCommonMetadata.version() == 0
                            ? 1
                            : mMetadata->mCommonMetadata.version();
  }

  IDB_TRY(OkIf(mMetadata->mCommonMetadata.version() <= mRequestedVersion),
          NS_ERROR_DOM_INDEXEDDB_VERSION_ERR);

  IDB_TRY_UNWRAP(
      mFileManager,
      ([this, persistenceType, &databaseName, &fmDirectory,
        &connection]() -> mozilla::Result<SafeRefPtr<FileManager>, nsresult> {
        IndexedDatabaseManager* const mgr = IndexedDatabaseManager::Get();
        MOZ_ASSERT(mgr);

        SafeRefPtr<FileManager> fileManager = mgr->GetFileManager(
            persistenceType, mOriginMetadata.mOrigin, databaseName);

        if (!fileManager) {
          fileManager = MakeSafeRefPtr<FileManager>(
              persistenceType, mOriginMetadata, databaseName, mEnforcingQuota);

          IDB_TRY(fileManager->Init(fmDirectory, *connection));

          mgr->AddFileManager(fileManager.clonePtr());
        }

        return fileManager;
      }()));

  // Must close connection before dispatching otherwise we might race with the
  // connection thread which needs to open the same database.
  asph.Unregister();

  MOZ_ALWAYS_SUCCEEDS(connection->Close());

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  mState = (mMetadata->mCommonMetadata.version() == mRequestedVersion)
               ? State::SendingResults
               : State::BeginVersionChange;

  IDB_TRY(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult OpenDatabaseOp::LoadDatabaseInformation(
    mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(mMetadata);

  {
    // Load version information.
    IDB_TRY_INSPECT(
        const auto& stmt,
        CreateAndExecuteSingleStepStatement<
            SingleStepResult::ReturnNullIfNoResult>(
            aConnection, "SELECT name, origin, version FROM database"_ns));

    IDB_TRY(OkIf(stmt), NS_ERROR_FILE_CORRUPTED);

    IDB_TRY_INSPECT(const auto& databaseName,
                    MOZ_TO_RESULT_INVOKE_TYPED(nsString, stmt, GetString, 0));

    IDB_TRY(OkIf(mCommonParams.metadata().name() == databaseName),
            NS_ERROR_FILE_CORRUPTED);

    IDB_TRY_INSPECT(const auto& origin, MOZ_TO_RESULT_INVOKE_TYPED(
                                            nsCString, stmt, GetUTF8String, 1));

    // We can't just compare these strings directly. See bug 1339081 comment 69.
    IDB_TRY(OkIf(QuotaManager::AreOriginsEqualOnDisk(mOriginMetadata.mOrigin,
                                                     origin)),
            NS_ERROR_FILE_CORRUPTED);

    IDB_TRY_INSPECT(const int64_t& version,
                    MOZ_TO_RESULT_INVOKE(stmt, GetInt64, 2));

    mMetadata->mCommonMetadata.version() = uint64_t(version);
  }

  ObjectStoreTable& objectStores = mMetadata->mObjectStores;

  IDB_TRY_INSPECT(
      const auto& lastObjectStoreId,
      ([&aConnection,
        &objectStores]() -> mozilla::Result<IndexOrObjectStoreId, nsresult> {
        // Load object store names and ids.
        IDB_TRY_INSPECT(
            const auto& stmt,
            MOZ_TO_RESULT_INVOKE_TYPED(
                nsCOMPtr<mozIStorageStatement>, aConnection, CreateStatement,
                "SELECT id, auto_increment, name, key_path "
                "FROM object_store"_ns));

        IndexOrObjectStoreId lastObjectStoreId = 0;

        IDB_TRY(CollectWhileHasResult(
            *stmt,
            [&lastObjectStoreId, &objectStores,
             usedIds = Maybe<nsTHashtable<nsUint64HashKey>>{},
             usedNames = Maybe<nsTHashtable<nsStringHashKey>>{}](
                auto& stmt) mutable -> mozilla::Result<Ok, nsresult> {
              IDB_TRY_INSPECT(const IndexOrObjectStoreId& objectStoreId,
                              MOZ_TO_RESULT_INVOKE(stmt, GetInt64, 0));

              if (!usedIds) {
                usedIds.emplace();
              }

              IDB_TRY(OkIf(objectStoreId > 0), Err(NS_ERROR_FILE_CORRUPTED));
              IDB_TRY(OkIf(!usedIds.ref().Contains(objectStoreId)),
                      Err(NS_ERROR_FILE_CORRUPTED));

              IDB_TRY(OkIf(usedIds.ref().PutEntry(objectStoreId, fallible)),
                      Err(NS_ERROR_OUT_OF_MEMORY));

              nsString name;
              IDB_TRY(stmt.GetString(2, name));

              if (!usedNames) {
                usedNames.emplace();
              }

              IDB_TRY(OkIf(!usedNames.ref().Contains(name)),
                      Err(NS_ERROR_FILE_CORRUPTED));

              IDB_TRY(OkIf(usedNames.ref().PutEntry(name, fallible)),
                      Err(NS_ERROR_OUT_OF_MEMORY));

              RefPtr<FullObjectStoreMetadata> metadata =
                  new FullObjectStoreMetadata();
              metadata->mCommonMetadata.id() = objectStoreId;
              metadata->mCommonMetadata.name() = name;

              IDB_TRY_INSPECT(const int32_t& columnType,
                              MOZ_TO_RESULT_INVOKE(stmt, GetTypeOfIndex, 3));

              if (columnType == mozIStorageStatement::VALUE_TYPE_NULL) {
                metadata->mCommonMetadata.keyPath() = KeyPath(0);
              } else {
                MOZ_ASSERT(columnType == mozIStorageStatement::VALUE_TYPE_TEXT);

                nsString keyPathSerialization;
                IDB_TRY(stmt.GetString(3, keyPathSerialization));

                metadata->mCommonMetadata.keyPath() =
                    KeyPath::DeserializeFromString(keyPathSerialization);
                IDB_TRY(OkIf(metadata->mCommonMetadata.keyPath().IsValid()),
                        Err(NS_ERROR_FILE_CORRUPTED));
              }

              IDB_TRY_INSPECT(const int64_t& nextAutoIncrementId,
                              MOZ_TO_RESULT_INVOKE(stmt, GetInt64, 1));

              metadata->mCommonMetadata.autoIncrement() = !!nextAutoIncrementId;
              metadata->mNextAutoIncrementId = nextAutoIncrementId;
              metadata->mCommittedAutoIncrementId = nextAutoIncrementId;

              IDB_TRY(OkIf(objectStores.InsertOrUpdate(
                          objectStoreId, std::move(metadata), fallible)),
                      Err(NS_ERROR_OUT_OF_MEMORY));

              lastObjectStoreId = std::max(lastObjectStoreId, objectStoreId);

              return Ok{};
            }));

        return lastObjectStoreId;
      }()));

  IDB_TRY_INSPECT(
      const auto& lastIndexId,
      ([&aConnection,
        &objectStores]() -> mozilla::Result<IndexOrObjectStoreId, nsresult> {
        // Load index information
        IDB_TRY_INSPECT(
            const auto& stmt,
            MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageStatement>,
                                       aConnection, CreateStatement,
                                       "SELECT "
                                       "id, object_store_id, name, key_path, "
                                       "unique_index, multientry, "
                                       "locale, is_auto_locale "
                                       "FROM object_store_index"_ns));

        IndexOrObjectStoreId lastIndexId = 0;

        IDB_TRY(CollectWhileHasResult(
            *stmt,
            [&lastIndexId, &objectStores, &aConnection,
             usedIds = Maybe<nsTHashtable<nsUint64HashKey>>{},
             usedNames = Maybe<nsTHashtable<nsStringHashKey>>{}](
                auto& stmt) mutable -> mozilla::Result<Ok, nsresult> {
              IDB_TRY_INSPECT(const IndexOrObjectStoreId& objectStoreId,
                              MOZ_TO_RESULT_INVOKE(stmt, GetInt64, 1));

              RefPtr<FullObjectStoreMetadata> objectStoreMetadata;
              IDB_TRY(OkIf(objectStores.Get(
                          objectStoreId, getter_AddRefs(objectStoreMetadata))),
                      Err(NS_ERROR_OUT_OF_MEMORY));

              MOZ_ASSERT(objectStoreMetadata->mCommonMetadata.id() ==
                         objectStoreId);

              IndexOrObjectStoreId indexId;
              IDB_TRY(stmt.GetInt64(0, &indexId));

              if (!usedIds) {
                usedIds.emplace();
              }

              IDB_TRY(OkIf(indexId > 0), Err(NS_ERROR_FILE_CORRUPTED));
              IDB_TRY(OkIf(!usedIds.ref().Contains(indexId)),
                      Err(NS_ERROR_FILE_CORRUPTED));

              IDB_TRY(OkIf(usedIds.ref().PutEntry(indexId, fallible)),
                      Err(NS_ERROR_OUT_OF_MEMORY));

              nsString name;
              IDB_TRY(stmt.GetString(2, name));

              const nsAutoString hashName =
                  IntToString(indexId) + u":"_ns + name;

              if (!usedNames) {
                usedNames.emplace();
              }

              IDB_TRY(OkIf(!usedNames.ref().Contains(hashName)),
                      Err(NS_ERROR_FILE_CORRUPTED));

              IDB_TRY(OkIf(usedNames.ref().PutEntry(hashName, fallible)),
                      Err(NS_ERROR_OUT_OF_MEMORY));

              RefPtr<FullIndexMetadata> indexMetadata = new FullIndexMetadata();
              indexMetadata->mCommonMetadata.id() = indexId;
              indexMetadata->mCommonMetadata.name() = name;

#ifdef DEBUG
              {
                int32_t columnType;
                nsresult rv = stmt.GetTypeOfIndex(3, &columnType);
                MOZ_ASSERT(NS_SUCCEEDED(rv));
                MOZ_ASSERT(columnType != mozIStorageStatement::VALUE_TYPE_NULL);
              }
#endif

              nsString keyPathSerialization;
              IDB_TRY(stmt.GetString(3, keyPathSerialization));

              indexMetadata->mCommonMetadata.keyPath() =
                  KeyPath::DeserializeFromString(keyPathSerialization);
              IDB_TRY(OkIf(indexMetadata->mCommonMetadata.keyPath().IsValid()),
                      Err(NS_ERROR_FILE_CORRUPTED));

              int32_t scratch;
              IDB_TRY(stmt.GetInt32(4, &scratch));

              indexMetadata->mCommonMetadata.unique() = !!scratch;

              IDB_TRY(stmt.GetInt32(5, &scratch));

              indexMetadata->mCommonMetadata.multiEntry() = !!scratch;

              const bool localeAware = !stmt.IsNull(6);
              if (localeAware) {
                IDB_TRY(stmt.GetUTF8String(
                    6, indexMetadata->mCommonMetadata.locale()));

                IDB_TRY(stmt.GetInt32(7, &scratch));

                indexMetadata->mCommonMetadata.autoLocale() = !!scratch;

                // Update locale-aware indexes if necessary
                const nsCString& indexedLocale =
                    indexMetadata->mCommonMetadata.locale();
                const bool& isAutoLocale =
                    indexMetadata->mCommonMetadata.autoLocale();
                const nsCString& systemLocale =
                    IndexedDatabaseManager::GetLocale();
                if (!systemLocale.IsEmpty() && isAutoLocale &&
                    !indexedLocale.EqualsASCII(systemLocale.get())) {
                  IDB_TRY(UpdateLocaleAwareIndex(aConnection,
                                                 indexMetadata->mCommonMetadata,
                                                 systemLocale));
                }
              }

              IDB_TRY(OkIf(objectStoreMetadata->mIndexes.InsertOrUpdate(
                          indexId, std::move(indexMetadata), fallible)),
                      Err(NS_ERROR_OUT_OF_MEMORY));

              lastIndexId = std::max(lastIndexId, indexId);

              return Ok{};
            }));

        return lastIndexId;
      }()));

  IDB_TRY(OkIf(lastObjectStoreId != INT64_MAX),
          NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, IDB_REPORT_INTERNAL_ERR_LAMBDA);
  IDB_TRY(OkIf(lastIndexId != INT64_MAX), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR,
          IDB_REPORT_INTERNAL_ERR_LAMBDA);

  mMetadata->mNextObjectStoreId = lastObjectStoreId + 1;
  mMetadata->mNextIndexId = lastIndexId + 1;

  return NS_OK;
}

/* static */
nsresult OpenDatabaseOp::UpdateLocaleAwareIndex(
    mozIStorageConnection& aConnection, const IndexMetadata& aIndexMetadata,
    const nsCString& aLocale) {
  const auto indexTable =
      aIndexMetadata.unique() ? "unique_index_data"_ns : "index_data"_ns;

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  const nsCString readQuery = "SELECT value, object_data_key FROM "_ns +
                              indexTable + " WHERE index_id = :index_id"_ns;

  IDB_TRY_INSPECT(
      const auto& readStmt,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageStatement>, aConnection,
                                 CreateStatement, readQuery));

  IDB_TRY(readStmt->BindInt64ByIndex(0, aIndexMetadata.id()));

  IDB_TRY(CollectWhileHasResult(
      *readStmt,
      [&aConnection, &indexTable, &aIndexMetadata, &aLocale,
       writeStmt = nsCOMPtr<mozIStorageStatement>{}](
          auto& readStmt) mutable -> mozilla::Result<Ok, nsresult> {
        if (!writeStmt) {
          IDB_TRY_UNWRAP(
              writeStmt,
              MOZ_TO_RESULT_INVOKE_TYPED(
                  nsCOMPtr<mozIStorageStatement>, aConnection, CreateStatement,
                  "UPDATE "_ns + indexTable + "SET value_locale = :"_ns +
                      kStmtParamNameValueLocale + " WHERE index_id = :"_ns +
                      kStmtParamNameIndexId + " AND value = :"_ns +
                      kStmtParamNameValue + " AND object_data_key = :"_ns +
                      kStmtParamNameObjectDataKey));
        }

        mozStorageStatementScoper scoper(writeStmt);
        IDB_TRY(writeStmt->BindInt64ByName(kStmtParamNameIndexId,
                                           aIndexMetadata.id()));

        Key oldKey, objectStorePosition;
        IDB_TRY(oldKey.SetFromStatement(&readStmt, 0));
        IDB_TRY(oldKey.BindToStatement(writeStmt, kStmtParamNameValue));

        IDB_TRY_INSPECT(const auto& newSortKey,
                        oldKey.ToLocaleAwareKey(aLocale));

        IDB_TRY(
            newSortKey.BindToStatement(writeStmt, kStmtParamNameValueLocale));
        IDB_TRY(objectStorePosition.SetFromStatement(&readStmt, 1));
        IDB_TRY(objectStorePosition.BindToStatement(
            writeStmt, kStmtParamNameObjectDataKey));

        IDB_TRY(writeStmt->Execute());

        return Ok{};
      }));

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  static constexpr auto metaQuery =
      "UPDATE object_store_index SET "
      "locale = :locale WHERE id = :id"_ns;

  IDB_TRY_INSPECT(
      const auto& metaStmt,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageStatement>, aConnection,
                                 CreateStatement, metaQuery));

  IDB_TRY(metaStmt->BindStringByIndex(0, NS_ConvertASCIItoUTF16(aLocale)));

  IDB_TRY(metaStmt->BindInt64ByIndex(1, aIndexMetadata.id()));

  IDB_TRY(metaStmt->Execute());

  return NS_OK;
}

nsresult OpenDatabaseOp::BeginVersionChange() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(mMetadata->mCommonMetadata.version() <= mRequestedVersion);
  MOZ_ASSERT(!mDatabase);
  MOZ_ASSERT(!mVersionChangeTransaction);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  EnsureDatabaseActor();

  if (mDatabase->IsInvalidated()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  MOZ_ASSERT(!mDatabase->IsClosed());

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(mDatabaseId, &info));

  MOZ_ASSERT(info->mLiveDatabases.Contains(mDatabase.unsafeGetRawPtr()));
  MOZ_ASSERT(!info->mWaitingFactoryOp);
  MOZ_ASSERT(info->mMetadata == mMetadata);

  auto transaction = MakeSafeRefPtr<VersionChangeTransaction>(this);

  if (NS_WARN_IF(!transaction->CopyDatabaseMetadata())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  MOZ_ASSERT(info->mMetadata != mMetadata);
  mMetadata = info->mMetadata.clonePtr();

  const Maybe<uint64_t> newVersion = Some(mRequestedVersion);

  IDB_TRY(SendVersionChangeMessages(info, mDatabase.maybeDeref(),
                                    mMetadata->mCommonMetadata.version(),
                                    newVersion));

  mVersionChangeTransaction = std::move(transaction);

  if (mMaybeBlockedDatabases.IsEmpty()) {
    // We don't need to wait on any databases, just jump to the transaction
    // pool.
    WaitForTransactions();
    return NS_OK;
  }

  // If the actor gets destroyed, mWaitingFactoryOp will hold the last strong
  // reference to us.
  info->mWaitingFactoryOp = this;

  mState = State::WaitingForOtherDatabasesToClose;
  return NS_OK;
}

bool OpenDatabaseOp::AreActorsAlive() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDatabase);

  return !(IsActorDestroyed() || mDatabase->IsActorDestroyed());
}

void OpenDatabaseOp::SendBlockedNotification() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose);

  if (!IsActorDestroyed()) {
    Unused << SendBlocked(mMetadata->mCommonMetadata.version());
  }
}

nsresult OpenDatabaseOp::DispatchToWorkThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForTransactionsToComplete);
  MOZ_ASSERT(mVersionChangeTransaction);
  MOZ_ASSERT(mVersionChangeTransaction->GetMode() ==
             IDBTransaction::Mode::VersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed() || mDatabase->IsInvalidated()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mState = State::DatabaseWorkVersionChange;

  // Intentionally empty.
  nsTArray<nsString> objectStoreNames;

  const int64_t loggingSerialNumber =
      mVersionChangeTransaction->LoggingSerialNumber();
  const nsID& backgroundChildLoggingId =
      mVersionChangeTransaction->GetLoggingInfo()->Id();

  if (NS_WARN_IF(!mDatabase->RegisterTransaction(*mVersionChangeTransaction))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!gConnectionPool) {
    gConnectionPool = new ConnectionPool();
  }

  RefPtr<VersionChangeOp> versionChangeOp = new VersionChangeOp(this);

  uint64_t transactionId = versionChangeOp->StartOnConnectionPool(
      backgroundChildLoggingId, mVersionChangeTransaction->DatabaseId(),
      loggingSerialNumber, objectStoreNames,
      /* aIsWriteTransaction */ true);

  mVersionChangeOp = versionChangeOp;

  mVersionChangeTransaction->NoteActiveRequest();
  mVersionChangeTransaction->Init(transactionId);

  return NS_OK;
}

nsresult OpenDatabaseOp::SendUpgradeNeeded() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseWorkVersionChange);
  MOZ_ASSERT(mVersionChangeTransaction);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(!HasFailed());
  MOZ_ASSERT_IF(!IsActorDestroyed(), mDatabase);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const SafeRefPtr<VersionChangeTransaction> transaction =
      std::move(mVersionChangeTransaction);

  nsresult rv = EnsureDatabaseActorIsAlive();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Transfer ownership to IPDL.
  transaction->SetActorAlive();

  if (!mDatabase->SendPBackgroundIDBVersionChangeTransactionConstructor(
          transaction.unsafeGetRawPtr(), mMetadata->mCommonMetadata.version(),
          mRequestedVersion, mMetadata->mNextObjectStoreId,
          mMetadata->mNextIndexId)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void OpenDatabaseOp::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT_IF(!HasFailed(), !mVersionChangeTransaction);

  DebugOnly<DatabaseActorInfo*> info = nullptr;
  MOZ_ASSERT_IF(
      gLiveDatabaseHashtable && gLiveDatabaseHashtable->Get(mDatabaseId, &info),
      !info->mWaitingFactoryOp);

  if (mVersionChangeTransaction) {
    MOZ_ASSERT(HasFailed());

    mVersionChangeTransaction->Abort(ResultCode(), /* aForce */ true);
    mVersionChangeTransaction = nullptr;
  }

  if (IsActorDestroyed()) {
    SetFailureCodeIfUnset(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  } else {
    FactoryRequestResponse response;

    if (!HasFailed()) {
      // If we just successfully completed a versionchange operation then we
      // need to update the version in our metadata.
      mMetadata->mCommonMetadata.version() = mRequestedVersion;

      nsresult rv = EnsureDatabaseActorIsAlive();
      if (NS_SUCCEEDED(rv)) {
        // We successfully opened a database so use its actor as the success
        // result for this request.

        // XXX OpenDatabaseRequestResponse stores a raw pointer, can this be
        // avoided?
        response =
            OpenDatabaseRequestResponse{mDatabase.unsafeGetRawPtr(), nullptr};
      } else {
        response = ClampResultCode(rv);
#ifdef DEBUG
        SetFailureCode(response.get_nsresult());
#endif
      }
    } else {
#ifdef DEBUG
      // If something failed then our metadata pointer is now bad. No one should
      // ever touch it again though so just null it out in DEBUG builds to make
      // sure we find such cases.
      mMetadata = nullptr;
#endif
      response = ClampResultCode(ResultCode());
    }

    Unused << PBackgroundIDBFactoryRequestParent::Send__delete__(this,
                                                                 response);
  }

  if (mDatabase) {
    MOZ_ASSERT(!mDirectoryLock);

    if (HasFailed()) {
      mDatabase->Invalidate();
    }

    // Make sure to release the database on this thread.
    mDatabase = nullptr;

    CleanupMetadata();
  } else if (mDirectoryLock) {
    // ConnectionClosedCallback will call CleanupMetadata().
    nsCOMPtr<nsIRunnable> callback = NewRunnableMethod(
        "dom::indexedDB::OpenDatabaseOp::ConnectionClosedCallback", this,
        &OpenDatabaseOp::ConnectionClosedCallback);

    RefPtr<WaitForTransactionsHelper> helper =
        new WaitForTransactionsHelper(mDatabaseId, callback);
    helper->WaitForTransactions();
  } else {
    CleanupMetadata();
  }

  FinishSendResults();
}

void OpenDatabaseOp::ConnectionClosedCallback() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(HasFailed());
  MOZ_ASSERT(mDirectoryLock);

  mDirectoryLock = nullptr;

  CleanupMetadata();
}

void OpenDatabaseOp::EnsureDatabaseActor() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::BeginVersionChange ||
             mState == State::DatabaseWorkVersionChange ||
             mState == State::SendingResults);
  MOZ_ASSERT(!HasFailed());
  MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(!IsActorDestroyed());

  if (mDatabase) {
    return;
  }

  MOZ_ASSERT(mMetadata->mDatabaseId.IsEmpty());
  mMetadata->mDatabaseId = mDatabaseId;

  MOZ_ASSERT(mMetadata->mFilePath.IsEmpty());
  mMetadata->mFilePath = mDatabaseFilePath;

  DatabaseActorInfo* info;
  if (gLiveDatabaseHashtable->Get(mDatabaseId, &info)) {
    AssertMetadataConsistency(*info->mMetadata);
    mMetadata = info->mMetadata.clonePtr();
  }

  Maybe<const CipherKey> maybeKey;
  if (mInPrivateBrowsing) {
    CipherKey key;

    {
      const auto lockedPrivateBrowsingInfoHashtable =
          gPrivateBrowsingInfoHashtable->Lock();
      MOZ_ALWAYS_TRUE(
          lockedPrivateBrowsingInfoHashtable->Get(mDatabaseId, &key));
    }

    maybeKey.emplace(std::move(key));
  }

  // XXX Shouldn't Manager() return already_AddRefed when
  // PBackgroundIDBFactoryParent is declared refcounted?
  mDatabase = MakeSafeRefPtr<Database>(
      SafeRefPtr{static_cast<Factory*>(Manager()),
                 AcquireStrongRefFromRawPtr{}},
      mCommonParams.principalInfo(), mOptionalContentParentId, mOriginMetadata,
      mTelemetryId, mMetadata.clonePtr(), mFileManager.clonePtr(),
      std::move(mDirectoryLock), mFileHandleDisabled, mChromeWriteAccessAllowed,
      mInPrivateBrowsing, maybeKey);

  if (info) {
    info->mLiveDatabases.AppendElement(
        WrapNotNullUnchecked(mDatabase.unsafeGetRawPtr()));
  } else {
    // XXX Maybe use LookupOrInsertWith above, to avoid a second lookup here?
    info = gLiveDatabaseHashtable
               ->InsertOrUpdate(
                   mDatabaseId,
                   MakeUnique<DatabaseActorInfo>(
                       mMetadata.clonePtr(),
                       WrapNotNullUnchecked(mDatabase.unsafeGetRawPtr())))
               .get();
  }

  // Balanced in Database::CleanupMetadata().
  IncreaseBusyCount();
}

nsresult OpenDatabaseOp::EnsureDatabaseActorIsAlive() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseWorkVersionChange ||
             mState == State::SendingResults);
  MOZ_ASSERT(!HasFailed());
  MOZ_ASSERT(!IsActorDestroyed());

  EnsureDatabaseActor();

  if (mDatabase->IsActorAlive()) {
    return NS_OK;
  }

  auto* const factory = static_cast<Factory*>(Manager());

  IDB_TRY_INSPECT(const auto& spec, MetadataToSpec());

  // Transfer ownership to IPDL.
  mDatabase->SetActorAlive();

  if (!factory->SendPBackgroundIDBDatabaseConstructor(
          mDatabase.unsafeGetRawPtr(), spec, this)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

Result<DatabaseSpec, nsresult> OpenDatabaseOp::MetadataToSpec() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  DatabaseSpec spec;
  spec.metadata() = mMetadata->mCommonMetadata;

  IDB_TRY_UNWRAP(
      spec.objectStores(),
      TransformIntoNewArrayAbortOnErr(
          mMetadata->mObjectStores,
          [](const auto& objectStoreEntry)
              -> mozilla::Result<ObjectStoreSpec, nsresult> {
            FullObjectStoreMetadata* metadata = objectStoreEntry.GetData();
            MOZ_ASSERT(objectStoreEntry.GetKey());
            MOZ_ASSERT(metadata);

            ObjectStoreSpec objectStoreSpec;
            objectStoreSpec.metadata() = metadata->mCommonMetadata;

            IDB_TRY_UNWRAP(auto indexes,
                           TransformIntoNewArray(
                               metadata->mIndexes,
                               [](const auto& indexEntry) {
                                 FullIndexMetadata* indexMetadata =
                                     indexEntry.GetData();
                                 MOZ_ASSERT(indexEntry.GetKey());
                                 MOZ_ASSERT(indexMetadata);

                                 return indexMetadata->mCommonMetadata;
                               },
                               fallible));

            objectStoreSpec.indexes() = std::move(indexes);

            return objectStoreSpec;
          },
          fallible));

  return spec;
}

#ifdef DEBUG

void OpenDatabaseOp::AssertMetadataConsistency(
    const FullDatabaseMetadata& aMetadata) {
  AssertIsOnBackgroundThread();

  const FullDatabaseMetadata& thisDB = *mMetadata;
  const FullDatabaseMetadata& otherDB = aMetadata;

  MOZ_ASSERT(&thisDB != &otherDB);

  MOZ_ASSERT(thisDB.mCommonMetadata.name() == otherDB.mCommonMetadata.name());
  MOZ_ASSERT(thisDB.mCommonMetadata.version() ==
             otherDB.mCommonMetadata.version());
  MOZ_ASSERT(thisDB.mCommonMetadata.persistenceType() ==
             otherDB.mCommonMetadata.persistenceType());
  MOZ_ASSERT(thisDB.mDatabaseId == otherDB.mDatabaseId);
  MOZ_ASSERT(thisDB.mFilePath == otherDB.mFilePath);

  // |thisDB| reflects the latest objectStore and index ids that have committed
  // to disk. The in-memory metadata |otherDB| keeps track of objectStores and
  // indexes that were created and then removed as well, so the next ids for
  // |otherDB| may be higher than for |thisDB|.
  MOZ_ASSERT(thisDB.mNextObjectStoreId <= otherDB.mNextObjectStoreId);
  MOZ_ASSERT(thisDB.mNextIndexId <= otherDB.mNextIndexId);

  MOZ_ASSERT(thisDB.mObjectStores.Count() == otherDB.mObjectStores.Count());

  for (const auto& objectStoreEntry : thisDB.mObjectStores) {
    FullObjectStoreMetadata* thisObjectStore = objectStoreEntry.GetData();
    MOZ_ASSERT(thisObjectStore);
    MOZ_ASSERT(!thisObjectStore->mDeleted);

    auto otherObjectStore = MatchMetadataNameOrId(
        otherDB.mObjectStores, thisObjectStore->mCommonMetadata.id());
    MOZ_ASSERT(otherObjectStore);

    MOZ_ASSERT(thisObjectStore != &otherObjectStore.ref());

    MOZ_ASSERT(thisObjectStore->mCommonMetadata.id() ==
               otherObjectStore->mCommonMetadata.id());
    MOZ_ASSERT(thisObjectStore->mCommonMetadata.name() ==
               otherObjectStore->mCommonMetadata.name());
    MOZ_ASSERT(thisObjectStore->mCommonMetadata.autoIncrement() ==
               otherObjectStore->mCommonMetadata.autoIncrement());
    MOZ_ASSERT(thisObjectStore->mCommonMetadata.keyPath() ==
               otherObjectStore->mCommonMetadata.keyPath());
    // mNextAutoIncrementId and mCommittedAutoIncrementId may be modified
    // concurrently with this OpenOp, so it is not possible to assert equality
    // here. It's also possible that we've written the new ids to disk but not
    // yet updated the in-memory count.
    MOZ_ASSERT(thisObjectStore->mNextAutoIncrementId <=
               otherObjectStore->mNextAutoIncrementId);
    MOZ_ASSERT(thisObjectStore->mCommittedAutoIncrementId <=
                   otherObjectStore->mCommittedAutoIncrementId ||
               thisObjectStore->mCommittedAutoIncrementId ==
                   otherObjectStore->mNextAutoIncrementId);
    MOZ_ASSERT(!otherObjectStore->mDeleted);

    MOZ_ASSERT(thisObjectStore->mIndexes.Count() ==
               otherObjectStore->mIndexes.Count());

    for (const auto& indexEntry : thisObjectStore->mIndexes) {
      FullIndexMetadata* thisIndex = indexEntry.GetData();
      MOZ_ASSERT(thisIndex);
      MOZ_ASSERT(!thisIndex->mDeleted);

      auto otherIndex = MatchMetadataNameOrId(otherObjectStore->mIndexes,
                                              thisIndex->mCommonMetadata.id());
      MOZ_ASSERT(otherIndex);

      MOZ_ASSERT(thisIndex != &otherIndex.ref());

      MOZ_ASSERT(thisIndex->mCommonMetadata.id() ==
                 otherIndex->mCommonMetadata.id());
      MOZ_ASSERT(thisIndex->mCommonMetadata.name() ==
                 otherIndex->mCommonMetadata.name());
      MOZ_ASSERT(thisIndex->mCommonMetadata.keyPath() ==
                 otherIndex->mCommonMetadata.keyPath());
      MOZ_ASSERT(thisIndex->mCommonMetadata.unique() ==
                 otherIndex->mCommonMetadata.unique());
      MOZ_ASSERT(thisIndex->mCommonMetadata.multiEntry() ==
                 otherIndex->mCommonMetadata.multiEntry());
      MOZ_ASSERT(!otherIndex->mDeleted);
    }
  }
}

#endif  // DEBUG

nsresult OpenDatabaseOp::VersionChangeOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mState == State::DatabaseWorkVersionChange);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  AUTO_PROFILER_LABEL("OpenDatabaseOp::VersionChangeOp::DoDatabaseWork", DOM);

  IDB_LOG_MARK_PARENT_TRANSACTION("Beginning database work", "DB Start",
                                  IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
                                  mTransactionLoggingSerialNumber);

  Transaction().SetActiveOnConnectionThread();

  IDB_TRY(aConnection->BeginWriteTransaction());

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  IDB_TRY(aConnection->ExecuteCachedStatement(
      "UPDATE database SET version = :version;"_ns,
      ([&self = *this](
           mozIStorageStatement& updateStmt) -> mozilla::Result<Ok, nsresult> {
        IDB_TRY(
            updateStmt.BindInt64ByIndex(0, int64_t(self.mRequestedVersion)));

        return Ok{};
      })));

  return NS_OK;
}

nsresult OpenDatabaseOp::VersionChangeOp::SendSuccessResult() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mState == State::DatabaseWorkVersionChange);
  MOZ_ASSERT(mOpenDatabaseOp->mVersionChangeOp == this);

  nsresult rv = mOpenDatabaseOp->SendUpgradeNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool OpenDatabaseOp::VersionChangeOp::SendFailureResult(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mState == State::DatabaseWorkVersionChange);
  MOZ_ASSERT(mOpenDatabaseOp->mVersionChangeOp == this);

  mOpenDatabaseOp->SetFailureCode(aResultCode);
  mOpenDatabaseOp->mState = State::SendingResults;

  MOZ_ALWAYS_SUCCEEDS(mOpenDatabaseOp->Run());

  return false;
}

void OpenDatabaseOp::VersionChangeOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mVersionChangeOp == this);

  mOpenDatabaseOp->mVersionChangeOp = nullptr;
  mOpenDatabaseOp = nullptr;

#ifdef DEBUG
  // A bit hacky but the VersionChangeOp is not generated in response to a
  // child request like most other database operations. Do this to make our
  // assertions happy.
  //
  // XXX: Depending on timing, in most cases, NoteActorDestroyed will not have
  // been destroyed before, but in some cases it has. This should be reworked in
  // a way this hack is not necessary. There are also several similar cases in
  // other *Op classes.
  if (!IsActorDestroyed()) {
    NoteActorDestroyed();
  }
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

void DeleteDatabaseOp::LoadPreviousVersion(nsIFile& aDatabaseFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DatabaseWorkOpen);
  MOZ_ASSERT(!mPreviousVersion);

  AUTO_PROFILER_LABEL("DeleteDatabaseOp::LoadPreviousVersion", DOM);

  nsresult rv;

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  const auto maybeKey = [this]() -> Maybe<const CipherKey> {
    CipherKey key;

    const auto lockedPrivateBrowsingInfoHashtable =
        gPrivateBrowsingInfoHashtable->Lock();
    if (!lockedPrivateBrowsingInfoHashtable->Get(mDatabaseId, &key)) {
      return Nothing{};
    }
    return Some(std::move(key));
  }();

  // Pass -1 as the directoryLockId to disable quota checking, since we might
  // temporarily exceed quota before deleting the database.
  IDB_TRY_INSPECT(const auto& dbFileUrl,
                  GetDatabaseFileURL(aDatabaseFile, -1, maybeKey), QM_VOID);

  IDB_TRY_UNWRAP(const NotNull<nsCOMPtr<mozIStorageConnection>> connection,
                 OpenDatabaseAndHandleBusy(*ss, *dbFileUrl), QM_VOID);

#ifdef DEBUG
  {
    IDB_TRY_INSPECT(const auto& stmt,
                    CreateAndExecuteSingleStepStatement<
                        SingleStepResult::ReturnNullIfNoResult>(
                        *connection, "SELECT name FROM database"_ns),
                    QM_VOID);

    IDB_TRY(OkIf(stmt), QM_VOID);

    nsString databaseName;
    rv = stmt->GetString(0, databaseName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    MOZ_ASSERT(mCommonParams.metadata().name() == databaseName);
  }
#endif

  IDB_TRY_INSPECT(const auto& stmt,
                  CreateAndExecuteSingleStepStatement<
                      SingleStepResult::ReturnNullIfNoResult>(
                      *connection, "SELECT version FROM database"_ns),
                  QM_VOID);

  IDB_TRY(OkIf(stmt), QM_VOID);

  int64_t version;
  rv = stmt->GetInt64(0, &version);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mPreviousVersion = uint64_t(version);
}

nsresult DeleteDatabaseOp::DatabaseOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DatabaseOpenPending);

  // The content parent must be kept alive until SendToIOThread completed.
  // Move this to the stack now to ensure that we release it on this thread.
  const RefPtr<ContentParent> contentParent = std::move(mContentParent);
  Unused << contentParent;  // XXX see Bug 1605075

  nsresult rv = SendToIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DeleteDatabaseOp::DoDatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DatabaseWorkOpen);

  AUTO_PROFILER_LABEL("DeleteDatabaseOp::DoDatabaseWork", DOM);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const nsString& databaseName = mCommonParams.metadata().name();
  const PersistenceType persistenceType =
      mCommonParams.metadata().persistenceType();

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  IDB_TRY_UNWRAP(auto directory, quotaManager->GetDirectoryForOrigin(
                                     persistenceType, mOriginMetadata.mOrigin));

  IDB_TRY(
      directory->Append(NS_LITERAL_STRING_FROM_CSTRING(IDB_DIRECTORY_NAME)));

  IDB_TRY_UNWRAP(mDatabaseDirectoryPath,
                 MOZ_TO_RESULT_INVOKE_TYPED(nsString, directory, GetPath));

  mDatabaseFilenameBase = GetDatabaseFilenameBase(databaseName);

  IDB_TRY_INSPECT(
      const auto& dbFile,
      CloneFileAndAppend(*directory, mDatabaseFilenameBase + kSQLiteSuffix));

#ifdef DEBUG
  {
    IDB_TRY_INSPECT(const auto& databaseFilePath,
                    MOZ_TO_RESULT_INVOKE_TYPED(nsString, dbFile, GetPath));

    MOZ_ASSERT(databaseFilePath == mDatabaseFilePath);
  }
#endif

  IDB_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(dbFile, Exists));

  if (exists) {
    // Parts of this function may fail but that shouldn't prevent us from
    // deleting the file eventually.
    LoadPreviousVersion(*dbFile);

    mState = State::BeginVersionChange;
  } else {
    mState = State::SendingResults;
  }

  IDB_TRY(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult DeleteDatabaseOp::BeginVersionChange() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  DatabaseActorInfo* info;
  if (gLiveDatabaseHashtable->Get(mDatabaseId, &info)) {
    MOZ_ASSERT(!info->mWaitingFactoryOp);

    nsresult rv =
        SendVersionChangeMessages(info, Nothing(), mPreviousVersion, Nothing());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mMaybeBlockedDatabases.IsEmpty()) {
      // If the actor gets destroyed, mWaitingFactoryOp will hold the last
      // strong reference to us.
      info->mWaitingFactoryOp = this;

      mState = State::WaitingForOtherDatabasesToClose;
      return NS_OK;
    }
  }

  // No other databases need to be notified, just make sure that all
  // transactions are complete.
  WaitForTransactions();
  return NS_OK;
}

bool DeleteDatabaseOp::AreActorsAlive() {
  AssertIsOnOwningThread();

  return !IsActorDestroyed();
}

nsresult DeleteDatabaseOp::DispatchToWorkThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForTransactionsToComplete);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mState = State::DatabaseWorkVersionChange;

  RefPtr<VersionChangeOp> versionChangeOp = new VersionChangeOp(this);

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsresult rv = quotaManager->IOThread()->Dispatch(versionChangeOp.forget(),
                                                   NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void DeleteDatabaseOp::SendBlockedNotification() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForOtherDatabasesToClose);

  if (!IsActorDestroyed()) {
    Unused << SendBlocked(mPreviousVersion);
  }
}

void DeleteDatabaseOp::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  DebugOnly<DatabaseActorInfo*> info = nullptr;
  MOZ_ASSERT_IF(
      gLiveDatabaseHashtable && gLiveDatabaseHashtable->Get(mDatabaseId, &info),
      !info->mWaitingFactoryOp);

  if (!IsActorDestroyed()) {
    FactoryRequestResponse response;

    if (!HasFailed()) {
      response = DeleteDatabaseRequestResponse(mPreviousVersion);
    } else {
      response = ClampResultCode(ResultCode());
    }

    Unused << PBackgroundIDBFactoryRequestParent::Send__delete__(this,
                                                                 response);
  }

  mDirectoryLock = nullptr;

  CleanupMetadata();

  FinishSendResults();
}

nsresult DeleteDatabaseOp::VersionChangeOp::RunOnIOThread() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mDeleteDatabaseOp->mState == State::DatabaseWorkVersionChange);

  AUTO_PROFILER_LABEL("DeleteDatabaseOp::VersionChangeOp::RunOnIOThread", DOM);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const PersistenceType& persistenceType =
      mDeleteDatabaseOp->mCommonParams.metadata().persistenceType();

  QuotaManager* quotaManager =
      mDeleteDatabaseOp->mEnforcingQuota ? QuotaManager::Get() : nullptr;

  MOZ_ASSERT_IF(mDeleteDatabaseOp->mEnforcingQuota, quotaManager);

  nsCOMPtr<nsIFile> directory =
      GetFileForPath(mDeleteDatabaseOp->mDatabaseDirectoryPath);
  if (NS_WARN_IF(!directory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult rv = RemoveDatabaseFilesAndDirectory(
      *directory, mDeleteDatabaseOp->mDatabaseFilenameBase, quotaManager,
      persistenceType, mDeleteDatabaseOp->mOriginMetadata,
      mDeleteDatabaseOp->mCommonParams.metadata().name());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void DeleteDatabaseOp::VersionChangeOp::RunOnOwningThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDeleteDatabaseOp->mState == State::DatabaseWorkVersionChange);

  const RefPtr<DeleteDatabaseOp> deleteOp = std::move(mDeleteDatabaseOp);

  if (deleteOp->IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    deleteOp->SetFailureCode(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  } else if (HasFailed()) {
    deleteOp->SetFailureCodeIfUnset(ResultCode());
  } else {
    DatabaseActorInfo* info;

    // Inform all the other databases that they are now invalidated. That
    // should remove the previous metadata from our table.
    if (gLiveDatabaseHashtable->Get(deleteOp->mDatabaseId, &info)) {
      MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());
      MOZ_ASSERT(!info->mWaitingFactoryOp);

      nsTArray<SafeRefPtr<Database>> liveDatabases;
      if (NS_WARN_IF(!liveDatabases.SetCapacity(info->mLiveDatabases.Length(),
                                                fallible))) {
        deleteOp->SetFailureCode(NS_ERROR_OUT_OF_MEMORY);
      } else {
        std::transform(info->mLiveDatabases.cbegin(),
                       info->mLiveDatabases.cend(),
                       MakeBackInserter(liveDatabases),
                       [](const auto& aDatabase) -> SafeRefPtr<Database> {
                         return {aDatabase.get(), AcquireStrongRefFromRawPtr{}};
                       });

#ifdef DEBUG
        // The code below should result in the deletion of |info|. Set to null
        // here to make sure we find invalid uses later.
        info = nullptr;
#endif

        for (const auto& database : liveDatabases) {
          database->Invalidate();
        }

        MOZ_ASSERT(!gLiveDatabaseHashtable->Get(deleteOp->mDatabaseId));
      }
    }
  }

  // We hold a strong ref to the deleteOp, so it's safe to call Run() directly.

  deleteOp->mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(deleteOp->Run());

#ifdef DEBUG
  // A bit hacky but the DeleteDatabaseOp::VersionChangeOp is not really a
  // normal database operation that is tied to an actor. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif
}

nsresult DeleteDatabaseOp::VersionChangeOp::Run() {
  nsresult rv;

  if (IsOnIOThread()) {
    rv = RunOnIOThread();
  } else {
    RunOnOwningThread();
    rv = NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    SetFailureCodeIfUnset(rv);

    MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
  }

  return NS_OK;
}

TransactionDatabaseOperationBase::TransactionDatabaseOperationBase(
    SafeRefPtr<TransactionBase> aTransaction)
    : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                            aTransaction->GetLoggingInfo()->NextRequestSN()),
      mTransaction(WrapNotNull(std::move(aTransaction))),
      mTransactionIsAborted((*mTransaction)->IsAborted()),
      mTransactionLoggingSerialNumber((*mTransaction)->LoggingSerialNumber()) {
  MOZ_ASSERT(LoggingSerialNumber());
}

TransactionDatabaseOperationBase::TransactionDatabaseOperationBase(
    SafeRefPtr<TransactionBase> aTransaction, uint64_t aLoggingSerialNumber)
    : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                            aLoggingSerialNumber),
      mTransaction(WrapNotNull(std::move(aTransaction))),
      mTransactionIsAborted((*mTransaction)->IsAborted()),
      mTransactionLoggingSerialNumber((*mTransaction)->LoggingSerialNumber()) {}

TransactionDatabaseOperationBase::~TransactionDatabaseOperationBase() {
  MOZ_ASSERT(mInternalState == InternalState::Completed);
  MOZ_ASSERT(!mTransaction,
             "TransactionDatabaseOperationBase::Cleanup() was not called by a "
             "subclass!");
}

#ifdef DEBUG

void TransactionDatabaseOperationBase::AssertIsOnConnectionThread() const {
  (*mTransaction)->AssertIsOnConnectionThread();
}

#endif  // DEBUG

uint64_t TransactionDatabaseOperationBase::StartOnConnectionPool(
    const nsID& aBackgroundChildLoggingId, const nsACString& aDatabaseId,
    int64_t aLoggingSerialNumber, const nsTArray<nsString>& aObjectStoreNames,
    bool aIsWriteTransaction) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::Initial);

  // Must set mInternalState before dispatching otherwise we will race with the
  // connection thread.
  mInternalState = InternalState::DatabaseWork;

  return gConnectionPool->Start(aBackgroundChildLoggingId, aDatabaseId,
                                aLoggingSerialNumber, aObjectStoreNames,
                                aIsWriteTransaction, this);
}

void TransactionDatabaseOperationBase::DispatchToConnectionPool() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::Initial);

  Unused << this->Run();
}

void TransactionDatabaseOperationBase::RunOnConnectionThread() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mInternalState == InternalState::DatabaseWork);
  MOZ_ASSERT(!HasFailed());

  AUTO_PROFILER_LABEL("TransactionDatabaseOperationBase::RunOnConnectionThread",
                      DOM);

  // There are several cases where we don't actually have to to any work here.

  if (mTransactionIsAborted || (*mTransaction)->IsInvalidatedOnAnyThread()) {
    // This transaction is already set to be aborted or invalidated.
    SetFailureCode(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  } else if (!OperationMayProceed()) {
    // The operation was canceled in some way, likely because the child process
    // has crashed.
    IDB_REPORT_INTERNAL_ERR();
    OverrideFailureCode(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  } else {
    Database& database = (*mTransaction)->GetMutableDatabase();

    // Here we're actually going to perform the database operation.
    nsresult rv = database.EnsureConnection();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SetFailureCode(rv);
    } else {
      DatabaseConnection* connection = database.GetConnection();
      MOZ_ASSERT(connection);

      auto& storageConnection = connection->MutableStorageConnection();

      AutoSetProgressHandler autoProgress;
      if (mLoggingSerialNumber) {
        rv = autoProgress.Register(storageConnection, this);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          SetFailureCode(rv);
        }
      }

      if (NS_SUCCEEDED(rv)) {
        if (mLoggingSerialNumber) {
          IDB_LOG_MARK_PARENT_TRANSACTION_REQUEST(
              "Beginning database work", "DB Start",
              IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
              mTransactionLoggingSerialNumber, mLoggingSerialNumber);
        }

        rv = DoDatabaseWork(connection);

        if (mLoggingSerialNumber) {
          IDB_LOG_MARK_PARENT_TRANSACTION_REQUEST(
              "Finished database work", "DB End",
              IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
              mTransactionLoggingSerialNumber, mLoggingSerialNumber);
        }

        if (NS_FAILED(rv)) {
          SetFailureCode(rv);
        }
      }
    }
  }

  // Must set mInternalState before dispatching otherwise we will race with the
  // owning thread.
  if (HasPreprocessInfo()) {
    mInternalState = InternalState::SendingPreprocess;
  } else {
    mInternalState = InternalState::SendingResults;
  }

  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
}

bool TransactionDatabaseOperationBase::HasPreprocessInfo() { return false; }

nsresult TransactionDatabaseOperationBase::SendPreprocessInfo() {
  return NS_OK;
}

void TransactionDatabaseOperationBase::NoteContinueReceived() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::WaitingForContinue);

  mWaitingForContinue = false;

  mInternalState = InternalState::SendingResults;

  // This TransactionDatabaseOperationBase can only be held alive by the IPDL.
  // Run() can end up with clearing that last reference. So we need to add
  // a self reference here.
  RefPtr<TransactionDatabaseOperationBase> kungFuDeathGrip = this;

  Unused << this->Run();
}

void TransactionDatabaseOperationBase::SendToConnectionPool() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::Initial);

  // Must set mInternalState before dispatching otherwise we will race with the
  // connection thread.
  mInternalState = InternalState::DatabaseWork;

  gConnectionPool->Dispatch((*mTransaction)->TransactionId(), this);

  (*mTransaction)->NoteActiveRequest();
}

void TransactionDatabaseOperationBase::SendPreprocess() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::SendingPreprocess);

  SendPreprocessInfoOrResults(/* aSendPreprocessInfo */ true);
}

void TransactionDatabaseOperationBase::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::SendingResults);

  SendPreprocessInfoOrResults(/* aSendPreprocessInfo */ false);
}

void TransactionDatabaseOperationBase::SendPreprocessInfoOrResults(
    bool aSendPreprocessInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::SendingPreprocess ||
             mInternalState == InternalState::SendingResults);

  // The flag is raised only when there is no mUpdateRefcountFunction for the
  // executing operation. It assume that is because the previous
  // StartTransactionOp was failed to begin a write transaction and it reported
  // when this operation has already jumped to the Connection thread.
  MOZ_DIAGNOSTIC_ASSERT_IF(mAssumingPreviousOperationFail,
                           (*mTransaction)->IsAborted());

  if (NS_WARN_IF(IsActorDestroyed())) {
    // Normally we wouldn't need to send any notifications if the actor was
    // already destroyed, but this can be a VersionChangeOp which needs to
    // notify its parent operation (OpenDatabaseOp) about the failure.
    // So SendFailureResult needs to be called even when the actor was
    // destroyed.  Normal operations redundantly check if the actor was
    // destroyed in SendSuccessResult and SendFailureResult, therefore it's
    // ok to call it in all cases here.
    if (!HasFailed()) {
      IDB_REPORT_INTERNAL_ERR();
      SetFailureCode(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }
  } else if ((*mTransaction)->IsInvalidated() || (*mTransaction)->IsAborted()) {
    // Aborted transactions always see their requests fail with ABORT_ERR,
    // even if the request succeeded or failed with another error.
    OverrideFailureCode(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  }

  const nsresult rv = [aSendPreprocessInfo, this] {
    if (HasFailed()) {
      return ResultCode();
    }
    if (aSendPreprocessInfo) {
      // This should not release the IPDL reference.
      return SendPreprocessInfo();
    }
    // This may release the IPDL reference.
    return SendSuccessResult();
  }();

  if (NS_FAILED(rv)) {
    SetFailureCodeIfUnset(rv);

    // This should definitely release the IPDL reference.
    if (!SendFailureResult(rv)) {
      // Abort the transaction.
      (*mTransaction)->Abort(rv, /* aForce */ false);
    }
  }

  if (aSendPreprocessInfo && !HasFailed()) {
    mInternalState = InternalState::WaitingForContinue;

    mWaitingForContinue = true;
  } else {
    if (mLoggingSerialNumber) {
      (*mTransaction)->NoteFinishedRequest(mLoggingSerialNumber, ResultCode());
    }

    Cleanup();

    mInternalState = InternalState::Completed;
  }
}

bool TransactionDatabaseOperationBase::Init(TransactionBase& aTransaction) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mInternalState == InternalState::Initial);

  return true;
}

void TransactionDatabaseOperationBase::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInternalState == InternalState::SendingResults);

  mTransaction.destroy();
}

NS_IMETHODIMP
TransactionDatabaseOperationBase::Run() {
  switch (mInternalState) {
    case InternalState::Initial:
      SendToConnectionPool();
      return NS_OK;

    case InternalState::DatabaseWork:
      RunOnConnectionThread();
      return NS_OK;

    case InternalState::SendingPreprocess:
      SendPreprocess();
      return NS_OK;

    case InternalState::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }
}

TransactionBase::CommitOp::CommitOp(SafeRefPtr<TransactionBase> aTransaction,
                                    nsresult aResultCode)
    : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                            aTransaction->GetLoggingInfo()->NextRequestSN()),
      mTransaction(std::move(aTransaction)),
      mResultCode(aResultCode) {
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(LoggingSerialNumber());
}

nsresult TransactionBase::CommitOp::WriteAutoIncrementCounts() {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();
  MOZ_ASSERT(mTransaction->GetMode() == IDBTransaction::Mode::ReadWrite ||
             mTransaction->GetMode() == IDBTransaction::Mode::ReadWriteFlush ||
             mTransaction->GetMode() == IDBTransaction::Mode::Cleanup ||
             mTransaction->GetMode() == IDBTransaction::Mode::VersionChange);

  const nsTArray<RefPtr<FullObjectStoreMetadata>>& metadataArray =
      mTransaction->mModifiedAutoIncrementObjectStoreMetadataArray;

  if (!metadataArray.IsEmpty()) {
    DatabaseConnection* connection =
        mTransaction->GetDatabase().GetConnection();
    MOZ_ASSERT(connection);

    // The parameter names are not used, parameters are bound by index only
    // locally in the same function.
    auto stmt = DatabaseConnection::LazyStatement(
        *connection,
        "UPDATE object_store "
        "SET auto_increment = :auto_increment WHERE id "
        "= :object_store_id;"_ns);
    nsresult rv;

    for (const auto& metadata : metadataArray) {
      MOZ_ASSERT(!metadata->mDeleted);
      MOZ_ASSERT(metadata->mNextAutoIncrementId > 1);

      IDB_TRY_INSPECT(const auto& borrowedStmt, stmt.Borrow());

      rv = borrowedStmt->BindInt64ByIndex(1, metadata->mCommonMetadata.id());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = borrowedStmt->BindInt64ByIndex(0, metadata->mNextAutoIncrementId);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = borrowedStmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

void TransactionBase::CommitOp::CommitOrRollbackAutoIncrementCounts() {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();
  MOZ_ASSERT(mTransaction->GetMode() == IDBTransaction::Mode::ReadWrite ||
             mTransaction->GetMode() == IDBTransaction::Mode::ReadWriteFlush ||
             mTransaction->GetMode() == IDBTransaction::Mode::Cleanup ||
             mTransaction->GetMode() == IDBTransaction::Mode::VersionChange);

  const auto& metadataArray =
      mTransaction->mModifiedAutoIncrementObjectStoreMetadataArray;

  if (!metadataArray.IsEmpty()) {
    bool committed = NS_SUCCEEDED(mResultCode);

    for (const auto& metadata : metadataArray) {
      if (committed) {
        metadata->mCommittedAutoIncrementId = metadata->mNextAutoIncrementId;
      } else {
        metadata->mNextAutoIncrementId = metadata->mCommittedAutoIncrementId;
      }
    }
  }
}

#ifdef DEBUG

void TransactionBase::CommitOp::AssertForeignKeyConsistency(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();
  MOZ_ASSERT(mTransaction->GetMode() != IDBTransaction::Mode::ReadOnly);

  {
    IDB_TRY_INSPECT(
        const auto& pragmaStmt,
        CreateAndExecuteSingleStepStatement(
            aConnection->MutableStorageConnection(), "PRAGMA foreign_keys;"_ns),
        QM_ASSERT_UNREACHABLE_VOID);

    int32_t foreignKeysEnabled;
    MOZ_ALWAYS_SUCCEEDS(pragmaStmt->GetInt32(0, &foreignKeysEnabled));

    MOZ_ASSERT(foreignKeysEnabled,
               "Database doesn't have foreign keys enabled!");
  }

  {
    IDB_TRY_INSPECT(const bool& foreignKeyError,
                    CreateAndExecuteSingleStepStatement<
                        SingleStepResult::ReturnNullIfNoResult>(
                        aConnection->MutableStorageConnection(),
                        "PRAGMA foreign_key_check;"_ns),
                    QM_ASSERT_UNREACHABLE_VOID);

    MOZ_ASSERT(!foreignKeyError, "Database has inconsisistent foreign keys!");
  }
}

#endif  // DEBUG

NS_IMPL_ISUPPORTS_INHERITED0(TransactionBase::CommitOp, DatabaseOperationBase)

NS_IMETHODIMP
TransactionBase::CommitOp::Run() {
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("TransactionBase::CommitOp::Run", DOM);

  IDB_LOG_MARK_PARENT_TRANSACTION_REQUEST(
      "Beginning database work", "DB Start",
      IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
      mTransaction->LoggingSerialNumber(), mLoggingSerialNumber);

  if (mTransaction->GetMode() != IDBTransaction::Mode::ReadOnly &&
      mTransaction->mHasBeenActiveOnConnectionThread) {
    if (DatabaseConnection* connection =
            mTransaction->GetDatabase().GetConnection()) {
      // May be null if the VersionChangeOp was canceled.
      DatabaseConnection::UpdateRefcountFunction* fileRefcountFunction =
          connection->GetUpdateRefcountFunction();

      if (NS_SUCCEEDED(mResultCode)) {
        if (fileRefcountFunction) {
          mResultCode = fileRefcountFunction->WillCommit();
          NS_WARNING_ASSERTION(NS_SUCCEEDED(mResultCode),
                               "WillCommit() failed!");
        }

        if (NS_SUCCEEDED(mResultCode)) {
          mResultCode = WriteAutoIncrementCounts();
          NS_WARNING_ASSERTION(NS_SUCCEEDED(mResultCode),
                               "WriteAutoIncrementCounts() failed!");

          if (NS_SUCCEEDED(mResultCode)) {
            AssertForeignKeyConsistency(connection);

            mResultCode = connection->CommitWriteTransaction();
            NS_WARNING_ASSERTION(NS_SUCCEEDED(mResultCode), "Commit failed!");

            if (NS_SUCCEEDED(mResultCode) &&
                mTransaction->GetMode() ==
                    IDBTransaction::Mode::ReadWriteFlush) {
              mResultCode = connection->Checkpoint();
            }

            if (NS_SUCCEEDED(mResultCode) && fileRefcountFunction) {
              fileRefcountFunction->DidCommit();
            }
          }
        }
      }

      if (NS_FAILED(mResultCode)) {
        if (fileRefcountFunction) {
          fileRefcountFunction->DidAbort();
        }

        connection->RollbackWriteTransaction();
      }

      CommitOrRollbackAutoIncrementCounts();

      connection->FinishWriteTransaction();

      if (mTransaction->GetMode() == IDBTransaction::Mode::Cleanup) {
        connection->DoIdleProcessing(/* aNeedsCheckpoint */ true);

        connection->EnableQuotaChecks();
      }
    }
  }

  IDB_LOG_MARK_PARENT_TRANSACTION_REQUEST(
      "Finished database work", "DB End",
      IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
      mTransaction->LoggingSerialNumber(), mLoggingSerialNumber);

  IDB_LOG_MARK_PARENT_TRANSACTION("Finished database work", "DB End",
                                  IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
                                  mTransaction->LoggingSerialNumber());

  return NS_OK;
}

void TransactionBase::CommitOp::TransactionFinishedBeforeUnblock() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransaction);

  AUTO_PROFILER_LABEL("CommitOp::TransactionFinishedBeforeUnblock", DOM);

  if (!IsActorDestroyed()) {
    mTransaction->UpdateMetadata(mResultCode);
  }
}

void TransactionBase::CommitOp::TransactionFinishedAfterUnblock() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransaction);

  IDB_LOG_MARK_PARENT_TRANSACTION(
      "Finished with result 0x%" PRIx32, "Transaction finished (0x%" PRIx32 ")",
      IDB_LOG_ID_STRING(mTransaction->GetLoggingInfo()->Id()),
      mTransaction->LoggingSerialNumber(), static_cast<uint32_t>(mResultCode));

  mTransaction->SendCompleteNotification(ClampResultCode(mResultCode));

  mTransaction->GetMutableDatabase().UnregisterTransaction(*mTransaction);

  mTransaction = nullptr;

#ifdef DEBUG
  // A bit hacky but the CommitOp is not really a normal database operation
  // that is tied to an actor. Do this to make our assertions happy.
  NoteActorDestroyed();
#endif
}

DatabaseOp::DatabaseOp(SafeRefPtr<Database> aDatabase)
    : DatabaseOperationBase(aDatabase->GetLoggingInfo()->Id(),
                            aDatabase->GetLoggingInfo()->NextRequestSN()),
      mDatabase(std::move(aDatabase)),
      mState(State::Initial) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatabase);
}

nsresult DatabaseOp::SendToIOThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Initial);

  if (!OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  QuotaManager* const quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  // Must set this before dispatching otherwise we will race with the IO thread.
  mState = State::DatabaseWork;

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

NS_IMETHODIMP
DatabaseOp::Run() {
  const auto handleError = [this](const nsresult rv) {
    if (mState != State::SendingResults) {
      SetFailureCodeIfUnset(rv);

      // Must set mState before dispatching otherwise we will race with the
      // owning thread.
      mState = State::SendingResults;

      MOZ_ALWAYS_SUCCEEDS(
          mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  };

  switch (mState) {
    case State::Initial:
      IDB_TRY(SendToIOThread(), NS_OK, handleError);
      break;

    case State::DatabaseWork:
      IDB_TRY(DoDatabaseWork(), NS_OK, handleError);
      break;

    case State::SendingResults:
      SendResults();
      break;

    default:
      MOZ_CRASH("Bad state!");
  }

  return NS_OK;
}

void DatabaseOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();
}

CreateFileOp::CreateFileOp(SafeRefPtr<Database> aDatabase,
                           const DatabaseRequestParams& aParams)
    : DatabaseOp(std::move(aDatabase)),
      mParams(aParams.get_CreateFileParams()) {
  MOZ_ASSERT(aParams.type() == DatabaseRequestParams::TCreateFileParams);
}

Result<RefPtr<MutableFile>, nsresult> CreateFileOp::CreateMutableFile() {
  const nsCOMPtr<nsIFile> file = (*mFileInfo)->GetFileForFileInfo();
  IDB_TRY(OkIf(file), Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
          IDB_REPORT_INTERNAL_ERR_LAMBDA);

  const RefPtr<MutableFile> mutableFile =
      MutableFile::Create(file, mDatabase.clonePtr(), mFileInfo->clonePtr());
  IDB_TRY(OkIf(mutableFile), Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
          IDB_REPORT_INTERNAL_ERR_LAMBDA);

  // Transfer ownership to IPDL.
  mutableFile->SetActorAlive();

  IDB_TRY(OkIf(mDatabase->SendPBackgroundMutableFileConstructor(
              mutableFile, mParams.name(), mParams.type())),
          Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR),
          IDB_REPORT_INTERNAL_ERR_LAMBDA);

  return mutableFile;
}

nsresult CreateFileOp::DoDatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::DatabaseWork);

  AUTO_PROFILER_LABEL("CreateFileOp::DoDatabaseWork", DOM);

  if (NS_WARN_IF(QuotaManager::IsShuttingDown()) || !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  FileManager& fileManager = mDatabase->GetFileManager();

  mFileInfo.init(fileManager.CreateFileInfo());
  if (NS_WARN_IF(!*mFileInfo)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const int64_t fileId = (*mFileInfo)->Id();

  const auto journalDirectory = fileManager.EnsureJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const auto journalFile = fileManager.GetFileForId(journalDirectory, fileId);
  if (NS_WARN_IF(!journalFile)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult rv = journalFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const auto fileDirectory = fileManager.GetDirectory();
  if (NS_WARN_IF(!fileDirectory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const auto file = fileManager.GetFileForId(fileDirectory, fileId);
  if (NS_WARN_IF(!file)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  mState = State::SendingResults;

  rv = mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void CreateFileOp::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (!IsActorDestroyed() && !mDatabase->IsInvalidated()) {
    const auto response = [this]() -> DatabaseRequestResponse {
      if (HasFailed()) {
        return ClampResultCode(ResultCode());
      }

      auto res = [this]() -> DatabaseRequestResponse {
        IDB_TRY_RETURN(
            CreateMutableFile().andThen(
                [](const auto& mutableFile)
                    -> mozilla::Result<CreateFileRequestResponse, nsresult> {
                  // We successfully created a mutable file so use its actor
                  // as the success result for this request.
                  return CreateFileRequestResponse{mutableFile, nullptr};
                }),
            ClampResultCode(tryTempError));
      }();
#ifdef DEBUG
      if (res.type() == DatabaseRequestResponse::Tnsresult) {
        SetFailureCode(res.get_nsresult());
      }
#endif
      return res;
    }();

    Unused << PBackgroundIDBDatabaseRequestParent::Send__delete__(this,
                                                                  response);
  }

  // XXX: "Complete" in CompletedCreateFileOp and State::Completed mean
  // different things, and State::Completed should only be reached after
  // notifying the database. Either should probably be renamed to avoid
  // confusion.
  mDatabase->NoteCompletedCreateFileOp();

  mState = State::Completed;
}

nsresult VersionChangeTransactionOp::SendSuccessResult() {
  AssertIsOnOwningThread();

  // Nothing to send here, the API assumes that this request always succeeds.
  return NS_OK;
}

bool VersionChangeTransactionOp::SendFailureResult(nsresult aResultCode) {
  AssertIsOnOwningThread();

  // The only option here is to cause the transaction to abort.
  return false;
}

void VersionChangeTransactionOp::Cleanup() {
  AssertIsOnOwningThread();

#ifdef DEBUG
  // A bit hacky but the VersionChangeTransactionOp is not generated in response
  // to a child request like most other database operations. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

nsresult CreateObjectStoreOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("CreateObjectStoreOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure that we're not creating an object store with the same name as
    // another that already exists. This should be impossible because we should
    // have thrown an error long before now...
    // The parameter names are not used, parameters are bound by index only
    // locally in the same function.
    IDB_TRY_INSPECT(
        const bool& hasResult,
        aConnection
            ->BorrowAndExecuteSingleStepStatement(
                "SELECT name "
                "FROM object_store "
                "WHERE name = :name;"_ns,
                [&self = *this](auto& stmt) -> Result<Ok, nsresult> {
                  IDB_TRY(stmt.BindStringByIndex(0, self.mMetadata.name()));
                  return Ok{};
                })
            .map(IsSome),
        QM_ASSERT_UNREACHABLE);

    MOZ_ASSERT(!hasResult);
  }
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  IDB_TRY(aConnection->ExecuteCachedStatement(
      "INSERT INTO object_store (id, auto_increment, name, key_path) "
      "VALUES (:id, :auto_increment, :name, :key_path);"_ns,
      [&metadata =
           mMetadata](mozIStorageStatement& stmt) -> Result<Ok, nsresult> {
        IDB_TRY(stmt.BindInt64ByIndex(0, metadata.id()));

        IDB_TRY(stmt.BindInt32ByIndex(1, metadata.autoIncrement() ? 1 : 0));

        IDB_TRY(stmt.BindStringByIndex(2, metadata.name()));

        if (metadata.keyPath().IsValid()) {
          IDB_TRY(stmt.BindStringByIndex(
              3, metadata.keyPath().SerializeToString()));
        } else {
          IDB_TRY(stmt.BindNullByIndex(3));
        }

        return Ok{};
      }));

#ifdef DEBUG
  {
    int64_t id;
    MOZ_ALWAYS_SUCCEEDS(
        aConnection->MutableStorageConnection().GetLastInsertRowID(&id));
    MOZ_ASSERT(mMetadata.id() == id);
  }
#endif

  IDB_TRY(autoSave.Commit());

  return NS_OK;
}

nsresult DeleteObjectStoreOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("DeleteObjectStoreOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure |mIsLastObjectStore| is telling the truth.
    IDB_TRY_INSPECT(
        const auto& stmt,
        aConnection->BorrowCachedStatement("SELECT id FROM object_store;"_ns),
        QM_ASSERT_UNREACHABLE);

    bool foundThisObjectStore = false;
    bool foundOtherObjectStore = false;

    while (true) {
      bool hasResult;
      MOZ_ALWAYS_SUCCEEDS(stmt->ExecuteStep(&hasResult));

      if (!hasResult) {
        break;
      }

      int64_t id;
      MOZ_ALWAYS_SUCCEEDS(stmt->GetInt64(0, &id));

      if (id == mMetadata->mCommonMetadata.id()) {
        foundThisObjectStore = true;
      } else {
        foundOtherObjectStore = true;
      }
    }

    MOZ_ASSERT_IF(mIsLastObjectStore,
                  foundThisObjectStore && !foundOtherObjectStore);
    MOZ_ASSERT_IF(!mIsLastObjectStore,
                  foundThisObjectStore && foundOtherObjectStore);
  }
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  if (mIsLastObjectStore) {
    // We can just delete everything if this is the last object store.
    IDB_TRY(aConnection->ExecuteCachedStatement("DELETE FROM index_data;"_ns));

    IDB_TRY(aConnection->ExecuteCachedStatement(
        "DELETE FROM unique_index_data;"_ns));

    IDB_TRY(aConnection->ExecuteCachedStatement("DELETE FROM object_data;"_ns));

    IDB_TRY(aConnection->ExecuteCachedStatement(
        "DELETE FROM object_store_index;"_ns));

    IDB_TRY(
        aConnection->ExecuteCachedStatement("DELETE FROM object_store;"_ns));
  } else {
    IDB_TRY_INSPECT(
        const bool& hasIndexes,
        ObjectStoreHasIndexes(*aConnection, mMetadata->mCommonMetadata.id()));

    const auto bindObjectStoreIdToFirstParameter =
        [this](mozIStorageStatement& stmt) -> nsresult {
      IDB_TRY(stmt.BindInt64ByIndex(0, mMetadata->mCommonMetadata.id()));

      return NS_OK;
    };

    // The parameter name :object_store_id in the SQL statements below is not
    // used for binding, parameters are bound by index only locally by
    // bindObjectStoreIdToFirstParameter.
    if (hasIndexes) {
      IDB_TRY(DeleteObjectStoreDataTableRowsWithIndexes(
          aConnection, mMetadata->mCommonMetadata.id(), Nothing()));

      // Now clean up the object store index table.
      IDB_TRY(aConnection->ExecuteCachedStatement(
          "DELETE FROM object_store_index "
          "WHERE object_store_id = :object_store_id;"_ns,
          bindObjectStoreIdToFirstParameter));
    } else {
      // We only have to worry about object data if this object store has no
      // indexes.
      IDB_TRY(aConnection->ExecuteCachedStatement(
          "DELETE FROM object_data "
          "WHERE object_store_id = :object_store_id;"_ns,
          bindObjectStoreIdToFirstParameter));
    }

    IDB_TRY(
        aConnection->ExecuteCachedStatement("DELETE FROM object_store "
                                            "WHERE id = :object_store_id;"_ns,
                                            bindObjectStoreIdToFirstParameter));

#ifdef DEBUG
    {
      int32_t deletedRowCount;
      MOZ_ALWAYS_SUCCEEDS(
          aConnection->MutableStorageConnection().GetAffectedRows(
              &deletedRowCount));
      MOZ_ASSERT(deletedRowCount == 1);
    }
#endif
  }

  IDB_TRY(autoSave.Commit());

  if (mMetadata->mCommonMetadata.autoIncrement()) {
    Transaction().ForgetModifiedAutoIncrementObjectStore(mMetadata);
  }

  return NS_OK;
}

nsresult RenameObjectStoreOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("RenameObjectStoreOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure that we're not renaming an object store with the same name as
    // another that already exists. This should be impossible because we should
    // have thrown an error long before now...
    // The parameter names are not used, parameters are bound by index only
    // locally in the same function.
    IDB_TRY_INSPECT(
        const bool& hasResult,
        aConnection
            ->BorrowAndExecuteSingleStepStatement(
                "SELECT name "
                "FROM object_store "
                "WHERE name = :name AND id != :id;"_ns,
                [&self = *this](auto& stmt) -> Result<Ok, nsresult> {
                  IDB_TRY(stmt.BindStringByIndex(0, self.mNewName));

                  IDB_TRY(stmt.BindInt64ByIndex(1, self.mId));
                  return Ok{};
                })
            .map(IsSome),
        QM_ASSERT_UNREACHABLE);

    MOZ_ASSERT(!hasResult);
  }
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  IDB_TRY(aConnection->ExecuteCachedStatement(
      "UPDATE object_store "
      "SET name = :name "
      "WHERE id = :id;"_ns,
      [&self = *this](mozIStorageStatement& stmt) -> nsresult {
        IDB_TRY(stmt.BindStringByIndex(0, self.mNewName));

        IDB_TRY(stmt.BindInt64ByIndex(1, self.mId));

        return NS_OK;
      }));

  IDB_TRY(autoSave.Commit());

  return NS_OK;
}

CreateIndexOp::CreateIndexOp(SafeRefPtr<VersionChangeTransaction> aTransaction,
                             const IndexOrObjectStoreId aObjectStoreId,
                             const IndexMetadata& aMetadata)
    : VersionChangeTransactionOp(std::move(aTransaction)),
      mMetadata(aMetadata),
      mFileManager(Transaction().GetDatabase().GetFileManagerPtr()),
      mDatabaseId(Transaction().DatabaseId()),
      mObjectStoreId(aObjectStoreId) {
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(aMetadata.id());
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT(!mDatabaseId.IsEmpty());
}

nsresult CreateIndexOp::InsertDataFromObjectStore(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mMaybeUniqueIndexTable);

  AUTO_PROFILER_LABEL("CreateIndexOp::InsertDataFromObjectStore", DOM);

  auto& storageConnection = aConnection->MutableStorageConnection();

  RefPtr<UpdateIndexDataValuesFunction> updateFunction =
      new UpdateIndexDataValuesFunction(this, aConnection,
                                        Transaction().GetDatabasePtr());

  constexpr auto updateFunctionName = "update_index_data_values"_ns;

  nsresult rv =
      storageConnection.CreateFunction(updateFunctionName, 4, updateFunction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InsertDataFromObjectStoreInternal(aConnection);

  MOZ_ALWAYS_SUCCEEDS(storageConnection.RemoveFunction(updateFunctionName));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult CreateIndexOp::InsertDataFromObjectStoreInternal(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mMaybeUniqueIndexTable);

  MOZ_ASSERT(aConnection->HasStorageConnection());

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  IDB_TRY(aConnection->ExecuteCachedStatement(
      "UPDATE object_data "
      "SET index_data_values = update_index_data_values "
      "(key, index_data_values, file_ids, data) "
      "WHERE object_store_id = :object_store_id;"_ns,
      [objectStoredId =
           mObjectStoreId](mozIStorageStatement& stmt) -> nsresult {
        IDB_TRY(stmt.BindInt64ByIndex(0, objectStoredId));

        return NS_OK;
      }));

  return NS_OK;
}

bool CreateIndexOp::Init(TransactionBase& aTransaction) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mObjectStoreId);
  MOZ_ASSERT(mMaybeUniqueIndexTable.isNothing());

  const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
      aTransaction.GetMetadataForObjectStoreId(mObjectStoreId);
  MOZ_ASSERT(objectStoreMetadata);

  const uint32_t indexCount = objectStoreMetadata->mIndexes.Count();
  if (!indexCount) {
    return true;
  }

  auto uniqueIndexTable = UniqueIndexTable{indexCount};

  for (const auto& indexEntry : objectStoreMetadata->mIndexes) {
    const FullIndexMetadata* const value = indexEntry.GetData();
    MOZ_ASSERT(!uniqueIndexTable.Contains(value->mCommonMetadata.id()));

    if (NS_WARN_IF(!uniqueIndexTable.InsertOrUpdate(
            value->mCommonMetadata.id(), value->mCommonMetadata.unique(),
            fallible))) {
      IDB_REPORT_INTERNAL_ERR();
      NS_WARNING("out of memory");
      return false;
    }
  }

  uniqueIndexTable.MarkImmutable();

  mMaybeUniqueIndexTable.emplace(std::move(uniqueIndexTable));

  return true;
}

nsresult CreateIndexOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("CreateIndexOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure that we're not creating an index with the same name and object
    // store as another that already exists. This should be impossible because
    // we should have thrown an error long before now...
    // The parameter names are not used, parameters are bound by index only
    // locally in the same function.
    IDB_TRY_INSPECT(
        const bool& hasResult,
        aConnection
            ->BorrowAndExecuteSingleStepStatement(
                "SELECT name "
                "FROM object_store_index "
                "WHERE object_store_id = :object_store_id AND name = :name;"_ns,
                [&self = *this](auto& stmt) -> Result<Ok, nsresult> {
                  IDB_TRY(stmt.BindInt64ByIndex(0, self.mObjectStoreId));
                  IDB_TRY(stmt.BindStringByIndex(1, self.mMetadata.name()));
                  return Ok{};
                })
            .map(IsSome),
        QM_ASSERT_UNREACHABLE);

    MOZ_ASSERT(!hasResult);
  }
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  IDB_TRY(aConnection->ExecuteCachedStatement(
      "INSERT INTO object_store_index (id, name, key_path, unique_index, "
      "multientry, object_store_id, locale, "
      "is_auto_locale) "
      "VALUES (:id, :name, :key_path, :unique, :multientry, "
      ":object_store_id, :locale, :is_auto_locale)"_ns,
      [&metadata = mMetadata,
       objectStoreId = mObjectStoreId](mozIStorageStatement& stmt) -> nsresult {
        IDB_TRY(stmt.BindInt64ByIndex(0, metadata.id()));

        IDB_TRY(stmt.BindStringByIndex(1, metadata.name()));

        IDB_TRY(
            stmt.BindStringByIndex(2, metadata.keyPath().SerializeToString()));

        IDB_TRY(stmt.BindInt32ByIndex(3, metadata.unique() ? 1 : 0));

        IDB_TRY(stmt.BindInt32ByIndex(4, metadata.multiEntry() ? 1 : 0));
        IDB_TRY(stmt.BindInt64ByIndex(5, objectStoreId));

        IDB_TRY(metadata.locale().IsEmpty()
                    ? stmt.BindNullByIndex(6)
                    : stmt.BindUTF8StringByIndex(6, metadata.locale()));

        IDB_TRY(stmt.BindInt32ByIndex(7, metadata.autoLocale()));

        return NS_OK;
      }));

#ifdef DEBUG
  {
    int64_t id;
    MOZ_ALWAYS_SUCCEEDS(
        aConnection->MutableStorageConnection().GetLastInsertRowID(&id));
    MOZ_ASSERT(mMetadata.id() == id);
  }
#endif

  IDB_TRY(InsertDataFromObjectStore(aConnection));

  IDB_TRY(autoSave.Commit());

  return NS_OK;
}

NS_IMPL_ISUPPORTS(CreateIndexOp::UpdateIndexDataValuesFunction,
                  mozIStorageFunction);

NS_IMETHODIMP
CreateIndexOp::UpdateIndexDataValuesFunction::OnFunctionCall(
    mozIStorageValueArray* aValues, nsIVariant** _retval) {
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);
  MOZ_ASSERT(mConnection);
  mConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mOp);
  MOZ_ASSERT(mOp->mFileManager);

  AUTO_PROFILER_LABEL(
      "CreateIndexOp::UpdateIndexDataValuesFunction::OnFunctionCall", DOM);

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 4);  // key, index_data_values, file_ids, data

    int32_t valueType;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(0, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(1, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_NULL ||
               valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(2, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_NULL ||
               valueType == mozIStorageValueArray::VALUE_TYPE_TEXT);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(3, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB ||
               valueType == mozIStorageValueArray::VALUE_TYPE_INTEGER);
  }
#endif

  IDB_TRY_UNWRAP(auto cloneInfo, GetStructuredCloneReadInfoFromValueArray(
                                     aValues,
                                     /* aDataIndex */ 3,
                                     /* aFileIdsIndex */ 2, *mOp->mFileManager,
                                     mDatabase->MaybeKeyRef()));

  const IndexMetadata& metadata = mOp->mMetadata;
  const IndexOrObjectStoreId& objectStoreId = mOp->mObjectStoreId;

  // XXX does this really need a non-const cloneInfo?
  IDB_TRY_INSPECT(const auto& updateInfos,
                  DeserializeIndexValueToUpdateInfos(
                      metadata.id(), metadata.keyPath(), metadata.multiEntry(),
                      metadata.locale(), cloneInfo));

  if (updateInfos.IsEmpty()) {
    // XXX See if we can do this without copying...

    nsCOMPtr<nsIVariant> unmodifiedValue;

    // No changes needed, just return the original value.
    IDB_TRY_INSPECT(const int32_t& valueType,
                    MOZ_TO_RESULT_INVOKE(aValues, GetTypeOfIndex, 1));

    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_NULL ||
               valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    if (valueType == mozIStorageValueArray::VALUE_TYPE_NULL) {
      unmodifiedValue = new storage::NullVariant();
      unmodifiedValue.forget(_retval);
      return NS_OK;
    }

    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    const uint8_t* blobData;
    uint32_t blobDataLength;
    IDB_TRY(aValues->GetSharedBlob(1, &blobDataLength, &blobData));

    const std::pair<uint8_t*, int> copiedBlobDataPair(
        static_cast<uint8_t*>(malloc(blobDataLength)), blobDataLength);

    if (!copiedBlobDataPair.first) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_OUT_OF_MEMORY;
    }

    memcpy(copiedBlobDataPair.first, blobData, blobDataLength);

    unmodifiedValue = new storage::AdoptedBlobVariant(copiedBlobDataPair);
    unmodifiedValue.forget(_retval);

    return NS_OK;
  }

  Key key;
  IDB_TRY(key.SetFromValueArray(aValues, 0));

  IDB_TRY_UNWRAP(auto indexValues, ReadCompressedIndexDataValues(*aValues, 1));

  const bool hadPreviousIndexValues = !indexValues.IsEmpty();

  const uint32_t updateInfoCount = updateInfos.Length();

  IDB_TRY(OkIf(indexValues.SetCapacity(indexValues.Length() + updateInfoCount,
                                       fallible)),
          NS_ERROR_OUT_OF_MEMORY, IDB_REPORT_INTERNAL_ERR_LAMBDA);

  // First construct the full list to update the index_data_values row.
  for (const IndexUpdateInfo& info : updateInfos) {
    MOZ_ALWAYS_TRUE(indexValues.InsertElementSorted(
        IndexDataValue(metadata.id(), metadata.unique(), info.value(),
                       info.localizedValue()),
        fallible));
  }

  IDB_TRY_UNWRAP((auto [indexValuesBlob, indexValuesBlobLength]),
                 MakeCompressedIndexDataValues(indexValues));

  MOZ_ASSERT(!indexValuesBlobLength == !(indexValuesBlob.get()));

  nsCOMPtr<nsIVariant> value;

  if (!indexValuesBlob) {
    value = new storage::NullVariant();

    value.forget(_retval);
    return NS_OK;
  }

  // Now insert the new table rows. We only need to construct a new list if
  // the full list is different.
  if (hadPreviousIndexValues) {
    indexValues.ClearAndRetainStorage();

    MOZ_ASSERT(indexValues.Capacity() >= updateInfoCount);

    for (const IndexUpdateInfo& info : updateInfos) {
      MOZ_ALWAYS_TRUE(indexValues.InsertElementSorted(
          IndexDataValue(metadata.id(), metadata.unique(), info.value(),
                         info.localizedValue()),
          fallible));
    }
  }

  IDB_TRY(InsertIndexTableRows(mConnection, objectStoreId, key, indexValues));

  value = new storage::AdoptedBlobVariant(
      std::pair(indexValuesBlob.release(), indexValuesBlobLength));

  value.forget(_retval);
  return NS_OK;
}

DeleteIndexOp::DeleteIndexOp(SafeRefPtr<VersionChangeTransaction> aTransaction,
                             const IndexOrObjectStoreId aObjectStoreId,
                             const IndexOrObjectStoreId aIndexId,
                             const bool aUnique, const bool aIsLastIndex)
    : VersionChangeTransactionOp(std::move(aTransaction)),
      mObjectStoreId(aObjectStoreId),
      mIndexId(aIndexId),
      mUnique(aUnique),
      mIsLastIndex(aIsLastIndex) {
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(aIndexId);
}

nsresult DeleteIndexOp::RemoveReferencesToIndex(
    DatabaseConnection* aConnection, const Key& aObjectStoreKey,
    nsTArray<IndexDataValue>& aIndexValues) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());
  MOZ_ASSERT_IF(!mIsLastIndex, !aIndexValues.IsEmpty());

  AUTO_PROFILER_LABEL("DeleteIndexOp::RemoveReferencesToIndex", DOM);

  if (mIsLastIndex) {
    // There is no need to parse the previous entry in the index_data_values
    // column if this is the last index. Simply set it to NULL.
    IDB_TRY_INSPECT(const auto& stmt,
                    aConnection->BorrowCachedStatement(
                        "UPDATE object_data "
                        "SET index_data_values = NULL "
                        "WHERE object_store_id = :"_ns +
                        kStmtParamNameObjectStoreId + " AND key = :"_ns +
                        kStmtParamNameKey + ";"_ns));

    IDB_TRY(stmt->BindInt64ByName(kStmtParamNameObjectStoreId, mObjectStoreId));

    IDB_TRY(aObjectStoreKey.BindToStatement(&*stmt, kStmtParamNameKey));

    IDB_TRY(stmt->Execute());

    return NS_OK;
  }

  {
    IndexDataValue search;
    search.mIndexId = mIndexId;

    // Use raw pointers for search to avoid redundant index validity checks.
    // Maybe this should better be encapsulated in nsTArray.
    const auto* const begin = aIndexValues.Elements();
    const auto* const end = aIndexValues.Elements() + aIndexValues.Length();

    const auto indexIdComparator = [](const IndexDataValue& aA,
                                      const IndexDataValue& aB) {
      return aA.mIndexId < aB.mIndexId;
    };

    MOZ_ASSERT(std::is_sorted(begin, end, indexIdComparator));

    const auto [beginRange, endRange] =
        std::equal_range(begin, end, search, indexIdComparator);
    if (beginRange == end) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_FILE_CORRUPTED;
    }

    aIndexValues.RemoveElementsAt(beginRange - begin, endRange - beginRange);
  }

  IDB_TRY(UpdateIndexValues(aConnection, mObjectStoreId, aObjectStoreKey,
                            aIndexValues));

  return NS_OK;
}

nsresult DeleteIndexOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

#ifdef DEBUG
  {
    // Make sure |mIsLastIndex| is telling the truth.
    // The parameter names are not used, parameters are bound by index only
    // locally in the same function.
    IDB_TRY_INSPECT(const auto& stmt,
                    aConnection->BorrowCachedStatement(
                        "SELECT id "
                        "FROM object_store_index "
                        "WHERE object_store_id = :object_store_id;"_ns),
                    QM_ASSERT_UNREACHABLE);

    MOZ_ALWAYS_SUCCEEDS(stmt->BindInt64ByIndex(0, mObjectStoreId));

    bool foundThisIndex = false;
    bool foundOtherIndex = false;

    while (true) {
      bool hasResult;
      MOZ_ALWAYS_SUCCEEDS(stmt->ExecuteStep(&hasResult));

      if (!hasResult) {
        break;
      }

      int64_t id;
      MOZ_ALWAYS_SUCCEEDS(stmt->GetInt64(0, &id));

      if (id == mIndexId) {
        foundThisIndex = true;
      } else {
        foundOtherIndex = true;
      }
    }

    MOZ_ASSERT_IF(mIsLastIndex, foundThisIndex && !foundOtherIndex);
    MOZ_ASSERT_IF(!mIsLastIndex, foundThisIndex && foundOtherIndex);
  }
#endif

  AUTO_PROFILER_LABEL("DeleteIndexOp::DoDatabaseWork", DOM);

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  // mozStorage warns that these statements trigger a sort operation but we
  // don't care because this is a very rare call and we expect it to be slow.
  // The cost of having an index on this field is too high.
  IDB_TRY_INSPECT(
      const auto& selectStmt,
      aConnection->BorrowCachedStatement(
          mUnique
              ? (mIsLastIndex
                     ? "/* do not warn (bug someone else) */ "
                       "SELECT value, object_data_key "
                       "FROM unique_index_data "
                       "WHERE index_id = :"_ns +
                           kStmtParamNameIndexId +
                           " ORDER BY object_data_key ASC;"_ns
                     : "/* do not warn (bug out) */ "
                       "SELECT unique_index_data.value, "
                       "unique_index_data.object_data_key, "
                       "object_data.index_data_values "
                       "FROM unique_index_data "
                       "JOIN object_data "
                       "ON unique_index_data.object_data_key = object_data.key "
                       "WHERE unique_index_data.index_id = :"_ns +
                           kStmtParamNameIndexId +
                           " AND object_data.object_store_id = :"_ns +
                           kStmtParamNameObjectStoreId +
                           " ORDER BY unique_index_data.object_data_key ASC;"_ns)
              : (mIsLastIndex
                     ? "/* do not warn (bug me not) */ "
                       "SELECT value, object_data_key "
                       "FROM index_data "
                       "WHERE index_id = :"_ns +
                           kStmtParamNameIndexId +
                           " AND object_store_id = :"_ns +
                           kStmtParamNameObjectStoreId +
                           " ORDER BY object_data_key ASC;"_ns
                     : "/* do not warn (bug off) */ "
                       "SELECT index_data.value, "
                       "index_data.object_data_key, "
                       "object_data.index_data_values "
                       "FROM index_data "
                       "JOIN object_data "
                       "ON index_data.object_data_key = object_data.key "
                       "WHERE index_data.index_id = :"_ns +
                           kStmtParamNameIndexId +
                           " AND object_data.object_store_id = :"_ns +
                           kStmtParamNameObjectStoreId +
                           " ORDER BY index_data.object_data_key ASC;"_ns)));

  IDB_TRY(selectStmt->BindInt64ByName(kStmtParamNameIndexId, mIndexId));

  if (!mUnique || !mIsLastIndex) {
    IDB_TRY(selectStmt->BindInt64ByName(kStmtParamNameObjectStoreId,
                                        mObjectStoreId));
  }

  Key lastObjectStoreKey;
  IndexDataValuesAutoArray lastIndexValues;

  IDB_TRY(CollectWhileHasResult(
      *selectStmt,
      [this, &aConnection, &lastObjectStoreKey, &lastIndexValues,
       deleteIndexRowStmt =
           DatabaseConnection::LazyStatement{
               *aConnection,
               mUnique
                   ? "DELETE FROM unique_index_data "
                     "WHERE index_id = :"_ns +
                         kStmtParamNameIndexId + " AND value = :"_ns +
                         kStmtParamNameValue + ";"_ns
                   : "DELETE FROM index_data "
                     "WHERE index_id = :"_ns +
                         kStmtParamNameIndexId + " AND value = :"_ns +
                         kStmtParamNameValue + " AND object_data_key = :"_ns +
                         kStmtParamNameObjectDataKey + ";"_ns}](
          auto& selectStmt) mutable -> Result<Ok, nsresult> {
        // We always need the index key to delete the index row.
        Key indexKey;
        IDB_TRY(indexKey.SetFromStatement(&selectStmt, 0));

        IDB_TRY(OkIf(!indexKey.IsUnset()), Err(NS_ERROR_FILE_CORRUPTED),
                IDB_REPORT_INTERNAL_ERR_LAMBDA);

        // Don't call |lastObjectStoreKey.BindToStatement()| directly because we
        // don't want to copy the same key multiple times.
        const uint8_t* objectStoreKeyData;
        uint32_t objectStoreKeyDataLength;
        IDB_TRY(selectStmt.GetSharedBlob(1, &objectStoreKeyDataLength,
                                         &objectStoreKeyData));

        IDB_TRY(OkIf(objectStoreKeyDataLength), Err(NS_ERROR_FILE_CORRUPTED),
                IDB_REPORT_INTERNAL_ERR_LAMBDA);

        const nsDependentCString currentObjectStoreKeyBuffer(
            reinterpret_cast<const char*>(objectStoreKeyData),
            objectStoreKeyDataLength);
        if (currentObjectStoreKeyBuffer != lastObjectStoreKey.GetBuffer()) {
          // We just walked to the next object store key.
          if (!lastObjectStoreKey.IsUnset()) {
            // Before we move on to the next key we need to update the previous
            // key's index_data_values column.
            IDB_TRY(RemoveReferencesToIndex(aConnection, lastObjectStoreKey,
                                            lastIndexValues));
          }

          // Save the object store key.
          lastObjectStoreKey = Key(currentObjectStoreKeyBuffer);

          // And the |index_data_values| row if this isn't the only index.
          if (!mIsLastIndex) {
            lastIndexValues.ClearAndRetainStorage();
            IDB_TRY(
                ReadCompressedIndexDataValues(selectStmt, 2, lastIndexValues));

            IDB_TRY(OkIf(!lastIndexValues.IsEmpty()),
                    Err(NS_ERROR_FILE_CORRUPTED),
                    IDB_REPORT_INTERNAL_ERR_LAMBDA);
          }
        }

        // Now delete the index row.
        {
          IDB_TRY_INSPECT(const auto& borrowedDeleteIndexRowStmt,
                          deleteIndexRowStmt.Borrow());

          IDB_TRY(borrowedDeleteIndexRowStmt->BindInt64ByName(
              kStmtParamNameIndexId, mIndexId));

          IDB_TRY(indexKey.BindToStatement(&*borrowedDeleteIndexRowStmt,
                                           kStmtParamNameValue));

          if (!mUnique) {
            IDB_TRY(lastObjectStoreKey.BindToStatement(
                &*borrowedDeleteIndexRowStmt, kStmtParamNameObjectDataKey));
          }

          IDB_TRY(borrowedDeleteIndexRowStmt->Execute());
        }

        return Ok{};
      }));

  // Take care of the last key.
  if (!lastObjectStoreKey.IsUnset()) {
    MOZ_ASSERT_IF(!mIsLastIndex, !lastIndexValues.IsEmpty());

    IDB_TRY(RemoveReferencesToIndex(aConnection, lastObjectStoreKey,
                                    lastIndexValues));
  }

  IDB_TRY(aConnection->ExecuteCachedStatement(
      "DELETE FROM object_store_index "
      "WHERE id = :index_id;"_ns,
      [indexId = mIndexId](mozIStorageStatement& deleteStmt) -> nsresult {
        IDB_TRY(deleteStmt.BindInt64ByIndex(0, indexId));

        return NS_OK;
      }));

#ifdef DEBUG
  {
    int32_t deletedRowCount;
    MOZ_ALWAYS_SUCCEEDS(aConnection->MutableStorageConnection().GetAffectedRows(
        &deletedRowCount));
    MOZ_ASSERT(deletedRowCount == 1);
  }
#endif

  IDB_TRY(autoSave.Commit());

  return NS_OK;
}

nsresult RenameIndexOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("RenameIndexOp::DoDatabaseWork", DOM);

#ifdef DEBUG
  {
    // Make sure that we're not renaming an index with the same name as another
    // that already exists. This should be impossible because we should have
    // thrown an error long before now...
    // The parameter names are not used, parameters are bound by index only
    // locally in the same function.
    IDB_TRY_INSPECT(
        const bool& hasResult,
        aConnection
            ->BorrowAndExecuteSingleStepStatement(
                "SELECT name "
                "FROM object_store_index "
                "WHERE object_store_id = :object_store_id "
                "AND name = :name "
                "AND id != :id;"_ns,
                [&self = *this](auto& stmt) -> Result<Ok, nsresult> {
                  IDB_TRY(stmt.BindInt64ByIndex(0, self.mObjectStoreId));
                  IDB_TRY(stmt.BindStringByIndex(1, self.mNewName));
                  IDB_TRY(stmt.BindInt64ByIndex(2, self.mIndexId));

                  return Ok{};
                })
            .map(IsSome),
        QM_ASSERT_UNREACHABLE);

    MOZ_ASSERT(!hasResult);
  }
#else
  Unused << mObjectStoreId;
#endif

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  IDB_TRY(aConnection->ExecuteCachedStatement(
      "UPDATE object_store_index "
      "SET name = :name "
      "WHERE id = :id;"_ns,
      [&self = *this](mozIStorageStatement& stmt) -> nsresult {
        IDB_TRY(stmt.BindStringByIndex(0, self.mNewName));

        IDB_TRY(stmt.BindInt64ByIndex(1, self.mIndexId));

        return NS_OK;
      }));

  IDB_TRY(autoSave.Commit());

  return NS_OK;
}

Result<bool, nsresult> NormalTransactionOp::ObjectStoreHasIndexes(
    DatabaseConnection& aConnection, const IndexOrObjectStoreId aObjectStoreId,
    const bool aMayHaveIndexes) {
  aConnection.AssertIsOnConnectionThread();
  MOZ_ASSERT(aObjectStoreId);

  if (Transaction().GetMode() == IDBTransaction::Mode::VersionChange &&
      aMayHaveIndexes) {
    // If this is a version change transaction then mObjectStoreMayHaveIndexes
    // could be wrong (e.g. if a unique index failed to be created due to a
    // constraint error). We have to check on this thread by asking the database
    // directly.
    IDB_TRY_RETURN(DatabaseOperationBase::ObjectStoreHasIndexes(
        aConnection, aObjectStoreId));
  }

#ifdef DEBUG
  IDB_TRY_INSPECT(
      const bool& hasIndexes,
      DatabaseOperationBase::ObjectStoreHasIndexes(aConnection, aObjectStoreId),
      QM_ASSERT_UNREACHABLE);
  MOZ_ASSERT(aMayHaveIndexes == hasIndexes);
#endif

  return aMayHaveIndexes;
}

Result<PreprocessParams, nsresult> NormalTransactionOp::GetPreprocessParams() {
  return PreprocessParams{};
}

nsresult NormalTransactionOp::SendPreprocessInfo() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!IsActorDestroyed());

  IDB_TRY_INSPECT(const auto& params, GetPreprocessParams());

  MOZ_ASSERT(params.type() != PreprocessParams::T__None);

  if (NS_WARN_IF(!PBackgroundIDBRequestParent::SendPreprocess(params))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

nsresult NormalTransactionOp::SendSuccessResult() {
  AssertIsOnOwningThread();

  if (!IsActorDestroyed()) {
    static const size_t kMaxIDBMsgOverhead = 1024 * 1024 * 10;  // 10MB
    const uint32_t maximalSizeFromPref =
        IndexedDatabaseManager::MaxSerializedMsgSize();
    MOZ_ASSERT(maximalSizeFromPref > kMaxIDBMsgOverhead);
    const size_t kMaxMessageSize = maximalSizeFromPref - kMaxIDBMsgOverhead;

    RequestResponse response;
    size_t responseSize = kMaxMessageSize;
    GetResponse(response, &responseSize);

    if (responseSize >= kMaxMessageSize) {
      nsPrintfCString warning(
          "The serialized value is too large"
          " (size=%zu bytes, max=%zu bytes).",
          responseSize, kMaxMessageSize);
      NS_WARNING(warning.get());
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    MOZ_ASSERT(response.type() != RequestResponse::T__None);

    if (response.type() == RequestResponse::Tnsresult) {
      MOZ_ASSERT(NS_FAILED(response.get_nsresult()));

      return response.get_nsresult();
    }

    if (NS_WARN_IF(
            !PBackgroundIDBRequestParent::Send__delete__(this, response))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

#ifdef DEBUG
  mResponseSent = true;
#endif

  return NS_OK;
}

bool NormalTransactionOp::SendFailureResult(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));

  bool result = false;

  if (!IsActorDestroyed()) {
    result = PBackgroundIDBRequestParent::Send__delete__(
        this, ClampResultCode(aResultCode));
  }

#ifdef DEBUG
  mResponseSent = true;
#endif

  return result;
}

void NormalTransactionOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(!IsActorDestroyed(), mResponseSent);

  TransactionDatabaseOperationBase::Cleanup();
}

void NormalTransactionOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();

  // Assume ActorDestroy can happen at any time, so we can't probe the current
  // state since mInternalState can be modified on any thread (only one thread
  // at a time based on the state machine).
  // However we can use mWaitingForContinue which is only touched on the owning
  // thread.  If mWaitingForContinue is true, we can also modify mInternalState
  // since we are guaranteed that there are no pending runnables which would
  // probe mInternalState to decide what code needs to run (there shouldn't be
  // any running runnables on other threads either).

  if (IsWaitingForContinue()) {
    NoteContinueReceived();
  }

  // We don't have to handle the case when mWaitingForContinue is not true since
  // it means that either nothing has been initialized yet, so nothing to
  // cleanup or there are pending runnables that will detect that the actor has
  // been destroyed and cleanup accordingly.
}

mozilla::ipc::IPCResult NormalTransactionOp::RecvContinue(
    const PreprocessResponse& aResponse) {
  AssertIsOnOwningThread();

  switch (aResponse.type()) {
    case PreprocessResponse::Tnsresult:
      SetFailureCode(aResponse.get_nsresult());
      break;

    case PreprocessResponse::TObjectStoreGetPreprocessResponse:
    case PreprocessResponse::TObjectStoreGetAllPreprocessResponse:
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  NoteContinueReceived();

  return IPC_OK();
}

ObjectStoreAddOrPutRequestOp::ObjectStoreAddOrPutRequestOp(
    SafeRefPtr<TransactionBase> aTransaction, RequestParams&& aParams)
    : NormalTransactionOp(std::move(aTransaction)),
      mParams(
          std::move(aParams.type() == RequestParams::TObjectStoreAddParams
                        ? aParams.get_ObjectStoreAddParams().commonParams()
                        : aParams.get_ObjectStorePutParams().commonParams())),
      mOriginMetadata(Transaction().GetDatabase().OriginMetadata()),
      mPersistenceType(Transaction().GetDatabase().Type()),
      mOverwrite(aParams.type() == RequestParams::TObjectStorePutParams),
      mObjectStoreMayHaveIndexes(false) {
  MOZ_ASSERT(aParams.type() == RequestParams::TObjectStoreAddParams ||
             aParams.type() == RequestParams::TObjectStorePutParams);

  mMetadata =
      Transaction().GetMetadataForObjectStoreId(mParams.objectStoreId());
  MOZ_ASSERT(mMetadata);

  mObjectStoreMayHaveIndexes = mMetadata->HasLiveIndexes();

  mDataOverThreshold =
      snappy::MaxCompressedLength(mParams.cloneInfo().data().data.Size()) >
      IndexedDatabaseManager::DataThreshold();
}

nsresult ObjectStoreAddOrPutRequestOp::RemoveOldIndexDataValues(
    DatabaseConnection* aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(mOverwrite);
  MOZ_ASSERT(!mResponse.IsUnset());

#ifdef DEBUG
  {
    IDB_TRY_INSPECT(const bool& hasIndexes,
                    DatabaseOperationBase::ObjectStoreHasIndexes(
                        *aConnection, mParams.objectStoreId()),
                    QM_ASSERT_UNREACHABLE);

    MOZ_ASSERT(hasIndexes,
               "Don't use this slow method if there are no indexes!");
  }
#endif

  IDB_TRY_INSPECT(
      const auto& indexValuesStmt,
      aConnection->BorrowAndExecuteSingleStepStatement(
          "SELECT index_data_values "
          "FROM object_data "
          "WHERE object_store_id = :"_ns +
              kStmtParamNameObjectStoreId + " AND key = :"_ns +
              kStmtParamNameKey + ";"_ns,
          [&self = *this](auto& stmt) -> mozilla::Result<Ok, nsresult> {
            IDB_TRY(stmt.BindInt64ByName(kStmtParamNameObjectStoreId,
                                         self.mParams.objectStoreId()));

            IDB_TRY(self.mResponse.BindToStatement(&stmt, kStmtParamNameKey));

            return Ok{};
          }));

  if (indexValuesStmt) {
    IDB_TRY_INSPECT(const auto& existingIndexValues,
                    ReadCompressedIndexDataValues(**indexValuesStmt, 0));

    IDB_TRY(
        DeleteIndexDataTableRows(aConnection, mResponse, existingIndexValues));
  }

  return NS_OK;
}

bool ObjectStoreAddOrPutRequestOp::Init(TransactionBase& aTransaction) {
  AssertIsOnOwningThread();

  const nsTArray<IndexUpdateInfo>& indexUpdateInfos =
      mParams.indexUpdateInfos();

  if (!indexUpdateInfos.IsEmpty()) {
    mUniqueIndexTable.emplace();

    for (const auto& updateInfo : indexUpdateInfos) {
      RefPtr<FullIndexMetadata> indexMetadata;
      MOZ_ALWAYS_TRUE(mMetadata->mIndexes.Get(updateInfo.indexId(),
                                              getter_AddRefs(indexMetadata)));

      MOZ_ASSERT(!indexMetadata->mDeleted);

      const IndexOrObjectStoreId& indexId = indexMetadata->mCommonMetadata.id();
      const bool& unique = indexMetadata->mCommonMetadata.unique();

      MOZ_ASSERT(indexId == updateInfo.indexId());
      MOZ_ASSERT_IF(!indexMetadata->mCommonMetadata.multiEntry(),
                    !mUniqueIndexTable.ref().Contains(indexId));

      if (NS_WARN_IF(!mUniqueIndexTable.ref().InsertOrUpdate(indexId, unique,
                                                             fallible))) {
        return false;
      }
    }
  } else if (mOverwrite) {
    mUniqueIndexTable.emplace();
  }

  if (mUniqueIndexTable.isSome()) {
    mUniqueIndexTable.ref().MarkImmutable();
  }

  IDB_TRY_UNWRAP(
      mStoredFileInfos,
      TransformIntoNewArray(
          mParams.fileAddInfos(),
          [](const auto& fileAddInfo) {
            MOZ_ASSERT(fileAddInfo.type() == StructuredCloneFileBase::eBlob ||
                       fileAddInfo.type() ==
                           StructuredCloneFileBase::eMutableFile);

            const DatabaseOrMutableFile& file = fileAddInfo.file();

            switch (fileAddInfo.type()) {
              case StructuredCloneFileBase::eBlob: {
                MOZ_ASSERT(
                    file.type() ==
                    DatabaseOrMutableFile::TPBackgroundIDBDatabaseFileParent);

                auto* const fileActor = static_cast<DatabaseFile*>(
                    file.get_PBackgroundIDBDatabaseFileParent());
                MOZ_ASSERT(fileActor);

                return StoredFileInfo::CreateForBlob(
                    fileActor->GetFileInfoPtr(), fileActor);
              }

              case StructuredCloneFileBase::eMutableFile: {
                MOZ_ASSERT(
                    file.type() ==
                    DatabaseOrMutableFile::TPBackgroundMutableFileParent);

                auto mutableFileActor = static_cast<MutableFile*>(
                    file.get_PBackgroundMutableFileParent());
                MOZ_ASSERT(mutableFileActor);

                return StoredFileInfo::CreateForMutableFile(
                    mutableFileActor->GetFileInfoPtr());
              }

              default:
                MOZ_CRASH("Should never get here!");
            }
          },
          fallible),
      false);

  if (mDataOverThreshold) {
    auto fileInfo =
        aTransaction.GetDatabase().GetFileManager().CreateFileInfo();
    if (NS_WARN_IF(!fileInfo)) {
      return false;
    }

    mStoredFileInfos.EmplaceBack(StoredFileInfo::CreateForStructuredClone(
        std::move(fileInfo),
        MakeRefPtr<SCInputStream>(mParams.cloneInfo().data().data)));
  }

  return true;
}

nsresult ObjectStoreAddOrPutRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection->HasStorageConnection());

  AUTO_PROFILER_LABEL("ObjectStoreAddOrPutRequestOp::DoDatabaseWork", DOM);

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  IDB_TRY_INSPECT(const bool& objectStoreHasIndexes,
                  ObjectStoreHasIndexes(*aConnection, mParams.objectStoreId(),
                                        mObjectStoreMayHaveIndexes));

  // This will be the final key we use.
  Key& key = mResponse;
  key = mParams.key();

  const bool keyUnset = key.IsUnset();
  const IndexOrObjectStoreId osid = mParams.objectStoreId();

  // First delete old index_data_values if we're overwriting something and we
  // have indexes.
  if (mOverwrite && !keyUnset && objectStoreHasIndexes) {
    IDB_TRY(RemoveOldIndexDataValues(aConnection));
  }

  int64_t autoIncrementNum = 0;

  {
    // The "|| keyUnset" here is mostly a debugging tool. If a key isn't
    // specified we should never have a collision and so it shouldn't matter
    // if we allow overwrite or not. By not allowing overwrite we raise
    // detectable errors rather than corrupting data.
    const auto optReplaceDirective =
        (!mOverwrite || keyUnset) ? ""_ns : "OR REPLACE "_ns;
    IDB_TRY_INSPECT(const auto& stmt,
                    aConnection->BorrowCachedStatement(
                        "INSERT "_ns + optReplaceDirective +
                        "INTO object_data "
                        "(object_store_id, key, file_ids, data) "
                        "VALUES (:"_ns +
                        kStmtParamNameObjectStoreId + ", :"_ns +
                        kStmtParamNameKey + ", :"_ns + kStmtParamNameFileIds +
                        ", :"_ns + kStmtParamNameData + ");"_ns));

    IDB_TRY(stmt->BindInt64ByName(kStmtParamNameObjectStoreId, osid));

    const SerializedStructuredCloneWriteInfo& cloneInfo = mParams.cloneInfo();
    const JSStructuredCloneData& cloneData = cloneInfo.data().data;
    const size_t cloneDataSize = cloneData.Size();

    MOZ_ASSERT(!keyUnset || mMetadata->mCommonMetadata.autoIncrement(),
               "Should have key unless autoIncrement");

    if (mMetadata->mCommonMetadata.autoIncrement()) {
      if (keyUnset) {
        autoIncrementNum = mMetadata->mNextAutoIncrementId;

        MOZ_ASSERT(autoIncrementNum > 0);

        if (autoIncrementNum > (1LL << 53)) {
          return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
        }

        key.SetFromInteger(autoIncrementNum);
      } else if (key.IsFloat()) {
        double numericKey = key.ToFloat();
        numericKey = std::min(numericKey, double(1LL << 53));
        numericKey = floor(numericKey);
        if (numericKey >= mMetadata->mNextAutoIncrementId) {
          autoIncrementNum = numericKey;
        }
      }

      if (keyUnset && mMetadata->mCommonMetadata.keyPath().IsValid()) {
        const SerializedStructuredCloneWriteInfo& cloneInfo =
            mParams.cloneInfo();
        MOZ_ASSERT(cloneInfo.offsetToKeyProp());
        MOZ_ASSERT(cloneDataSize > sizeof(uint64_t));
        MOZ_ASSERT(cloneInfo.offsetToKeyProp() <=
                   (cloneDataSize - sizeof(uint64_t)));

        // Special case where someone put an object into an autoIncrement'ing
        // objectStore with no key in its keyPath set. We needed to figure out
        // which row id we would get above before we could set that properly.
        uint64_t keyPropValue =
            ReinterpretDoubleAsUInt64(static_cast<double>(autoIncrementNum));

        static const size_t keyPropSize = sizeof(uint64_t);

        char keyPropBuffer[keyPropSize];
        LittleEndian::writeUint64(keyPropBuffer, keyPropValue);

        auto iter = cloneData.Start();
        MOZ_ALWAYS_TRUE(cloneData.Advance(iter, cloneInfo.offsetToKeyProp()));
        MOZ_ALWAYS_TRUE(
            cloneData.UpdateBytes(iter, keyPropBuffer, keyPropSize));
      }
    }

    key.BindToStatement(&*stmt, kStmtParamNameKey);

    if (mDataOverThreshold) {
      // The data we store in the SQLite database is a (signed) 64-bit integer.
      // The flags are left-shifted 32 bits so the max value is 0xFFFFFFFF.
      // The file_ids index occupies the lower 32 bits and its max is
      // 0xFFFFFFFF.
      static const uint32_t kCompressedFlag = (1 << 0);

      uint32_t flags = 0;
      flags |= kCompressedFlag;

      const uint32_t index = mStoredFileInfos.Length() - 1;

      const int64_t data = (uint64_t(flags) << 32) | index;

      IDB_TRY(stmt->BindInt64ByName(kStmtParamNameData, data));
    } else {
      nsCString flatCloneData;
      if (!flatCloneData.SetLength(cloneDataSize, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      {
        auto iter = cloneData.Start();
        MOZ_ALWAYS_TRUE(cloneData.ReadBytes(iter, flatCloneData.BeginWriting(),
                                            cloneDataSize));
      }

      // Compress the bytes before adding into the database.
      const char* const uncompressed = flatCloneData.BeginReading();
      const size_t uncompressedLength = cloneDataSize;

      size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);

      UniqueFreePtr<char> compressed(
          static_cast<char*>(malloc(compressedLength)));
      if (NS_WARN_IF(!compressed)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      snappy::RawCompress(uncompressed, uncompressedLength, compressed.get(),
                          &compressedLength);

      uint8_t* const dataBuffer =
          reinterpret_cast<uint8_t*>(compressed.release());
      const size_t dataBufferLength = compressedLength;

      IDB_TRY(stmt->BindAdoptedBlobByName(kStmtParamNameData, dataBuffer,
                                          dataBufferLength));
    }

    if (!mStoredFileInfos.IsEmpty()) {
      // Moved outside the loop to allow it to be cached when demanded by the
      // first write.  (We may have mStoredFileInfos without any required
      // writes.)
      Maybe<FileHelper> fileHelper;
      nsAutoString fileIds;

      for (auto& storedFileInfo : mStoredFileInfos) {
        MOZ_ASSERT(storedFileInfo.IsValid());

        IDB_TRY_INSPECT(const auto& inputStream,
                        storedFileInfo.GetInputStream());

        if (inputStream) {
          if (fileHelper.isNothing()) {
            fileHelper.emplace(Transaction().GetDatabase().GetFileManagerPtr());
            IDB_TRY(fileHelper->Init(), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR,
                    IDB_REPORT_INTERNAL_ERR_LAMBDA);
          }

          const FileInfo& fileInfo = storedFileInfo.GetFileInfo();

          const auto file = fileHelper->GetFile(fileInfo);
          IDB_TRY(OkIf(file), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR,
                  IDB_REPORT_INTERNAL_ERR_LAMBDA);

          const auto journalFile = fileHelper->GetJournalFile(fileInfo);
          IDB_TRY(OkIf(journalFile), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR,
                  IDB_REPORT_INTERNAL_ERR_LAMBDA);

          IDB_TRY(
              ToResult(fileHelper->CreateFileFromStream(
                           *file, *journalFile, *inputStream,
                           storedFileInfo.ShouldCompress(),
                           Transaction().GetDatabase().MaybeKeyRef()))
                  .mapErr([](const nsresult rv) {
                    if (NS_ERROR_GET_MODULE(rv) !=
                        NS_ERROR_MODULE_DOM_INDEXEDDB) {
                      IDB_REPORT_INTERNAL_ERR();
                      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
                    }
                    return rv;
                  }),
              QM_PROPAGATE,
              ([this, &file = *file, &journalFile = *journalFile](const auto) {
                // Try to remove the file if the copy failed.
                IDB_TRY(
                    Transaction().GetDatabase().GetFileManager().SyncDeleteFile(
                        file, journalFile),
                    QM_VOID);
              }));

          storedFileInfo.NotifyWriteSucceeded();
        }

        if (!fileIds.IsEmpty()) {
          fileIds.Append(' ');
        }
        storedFileInfo.Serialize(fileIds);
      }

      IDB_TRY(stmt->BindStringByName(kStmtParamNameFileIds, fileIds));
    } else {
      IDB_TRY(stmt->BindNullByName(kStmtParamNameFileIds));
    }

    IDB_TRY(stmt->Execute(), QM_PROPAGATE,
            [keyUnset = DebugOnly{keyUnset}](const nsresult rv) {
              if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
                MOZ_ASSERT(!keyUnset, "Generated key had a collision!");
              }
            });
  }

  // Update our indexes if needed.
  if (!mParams.indexUpdateInfos().IsEmpty()) {
    MOZ_ASSERT(mUniqueIndexTable.isSome());

    // Write the index_data_values column.
    IDB_TRY_INSPECT(const auto& indexValues,
                    IndexDataValuesFromUpdateInfos(mParams.indexUpdateInfos(),
                                                   mUniqueIndexTable.ref()));

    IDB_TRY(UpdateIndexValues(aConnection, osid, key, indexValues));

    IDB_TRY(InsertIndexTableRows(aConnection, osid, key, indexValues));
  }

  IDB_TRY(autoSave.Commit());

  if (autoIncrementNum) {
    mMetadata->mNextAutoIncrementId = autoIncrementNum + 1;
    Transaction().NoteModifiedAutoIncrementObjectStore(mMetadata);
  }

  return NS_OK;
}

void ObjectStoreAddOrPutRequestOp::GetResponse(RequestResponse& aResponse,
                                               size_t* aResponseSize) {
  AssertIsOnOwningThread();

  if (mOverwrite) {
    aResponse = ObjectStorePutResponse(mResponse);
    *aResponseSize = mResponse.GetBuffer().Length();
  } else {
    aResponse = ObjectStoreAddResponse(mResponse);
    *aResponseSize = mResponse.GetBuffer().Length();
  }
}

void ObjectStoreAddOrPutRequestOp::Cleanup() {
  AssertIsOnOwningThread();

  mStoredFileInfos.Clear();

  NormalTransactionOp::Cleanup();
}

NS_IMPL_ISUPPORTS(ObjectStoreAddOrPutRequestOp::SCInputStream, nsIInputStream)

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::Close() { return NS_OK; }

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::Available(uint64_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::Read(char* aBuf, uint32_t aCount,
                                                  uint32_t* _retval) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::ReadSegments(
    nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
    uint32_t* _retval) {
  *_retval = 0;

  while (aCount) {
    uint32_t count = std::min(uint32_t(mIter.RemainingInSegment()), aCount);
    if (!count) {
      // We've run out of data in the last segment.
      break;
    }

    uint32_t written;
    nsresult rv =
        aWriter(this, aClosure, mIter.Data(), *_retval, count, &written);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // InputStreams do not propagate errors to caller.
      return NS_OK;
    }

    // Writer should write what we asked it to write.
    MOZ_ASSERT(written == count);

    *_retval += count;
    aCount -= count;

    if (NS_WARN_IF(!mData.Advance(mIter, count))) {
      // InputStreams do not propagate errors to caller.
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
ObjectStoreAddOrPutRequestOp::SCInputStream::IsNonBlocking(bool* _retval) {
  *_retval = false;
  return NS_OK;
}

ObjectStoreGetRequestOp::ObjectStoreGetRequestOp(
    SafeRefPtr<TransactionBase> aTransaction, const RequestParams& aParams,
    bool aGetAll)
    : NormalTransactionOp(std::move(aTransaction)),
      mObjectStoreId(aGetAll
                         ? aParams.get_ObjectStoreGetAllParams().objectStoreId()
                         : aParams.get_ObjectStoreGetParams().objectStoreId()),
      mDatabase(Transaction().GetDatabasePtr()),
      mOptionalKeyRange(
          aGetAll ? aParams.get_ObjectStoreGetAllParams().optionalKeyRange()
                  : Some(aParams.get_ObjectStoreGetParams().keyRange())),
      mBackgroundParent(Transaction().GetBackgroundParent()),
      mPreprocessInfoCount(0),
      mLimit(aGetAll ? aParams.get_ObjectStoreGetAllParams().limit() : 1),
      mGetAll(aGetAll) {
  MOZ_ASSERT(aParams.type() == RequestParams::TObjectStoreGetParams ||
             aParams.type() == RequestParams::TObjectStoreGetAllParams);
  MOZ_ASSERT(mObjectStoreId);
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT_IF(!aGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT(mBackgroundParent);
}

template <typename T>
Result<T, nsresult> ObjectStoreGetRequestOp::ConvertResponse(
    StructuredCloneReadInfoParent&& aInfo) {
  T result;

  static_assert(std::is_same_v<T, SerializedStructuredCloneReadInfo> ||
                std::is_same_v<T, PreprocessInfo>);

  if constexpr (std::is_same_v<T, SerializedStructuredCloneReadInfo>) {
    result.data().data = aInfo.ReleaseData();
    result.hasPreprocessInfo() = aInfo.HasPreprocessInfo();
  }

  IDB_TRY_UNWRAP(
      result.files(),
      SerializeStructuredCloneFiles(mBackgroundParent, mDatabase, aInfo.Files(),
                                    std::is_same_v<T, PreprocessInfo>));

  return result;
}

nsresult ObjectStoreGetRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(!mGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  AUTO_PROFILER_LABEL("ObjectStoreGetRequestOp::DoDatabaseWork", DOM);

  const nsCString query =
      "SELECT file_ids, data "
      "FROM object_data "
      "WHERE object_store_id = :"_ns +
      kStmtParamNameObjectStoreId +
      MaybeGetBindingClauseForKeyRange(mOptionalKeyRange, kColumnNameKey) +
      " ORDER BY key ASC"_ns +
      (mLimit ? kOpenLimit + IntToCString(mLimit) : EmptyCString());

  IDB_TRY_INSPECT(const auto& stmt, aConnection->BorrowCachedStatement(query));

  IDB_TRY(stmt->BindInt64ByName(kStmtParamNameObjectStoreId, mObjectStoreId));

  if (mOptionalKeyRange.isSome()) {
    IDB_TRY(BindKeyRangeToStatement(mOptionalKeyRange.ref(), &*stmt));
  }

  IDB_TRY(CollectWhileHasResult(
      *stmt, [this](auto& stmt) mutable -> mozilla::Result<Ok, nsresult> {
        IDB_TRY_UNWRAP(auto cloneInfo,
                       GetStructuredCloneReadInfoFromStatement(
                           &stmt, 1, 0, mDatabase->GetFileManager(),
                           mDatabase->MaybeKeyRef()));

        if (cloneInfo.HasPreprocessInfo()) {
          mPreprocessInfoCount++;
        }

        IDB_TRY(OkIf(mResponse.EmplaceBack(fallible, std::move(cloneInfo))),
                Err(NS_ERROR_OUT_OF_MEMORY));

        return Ok{};
      }));

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

bool ObjectStoreGetRequestOp::HasPreprocessInfo() {
  return mPreprocessInfoCount > 0;
}

Result<PreprocessParams, nsresult>
ObjectStoreGetRequestOp::GetPreprocessParams() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mResponse.IsEmpty());

  if (mGetAll) {
    auto params = ObjectStoreGetAllPreprocessParams();

    auto& preprocessInfos = params.preprocessInfos();
    if (NS_WARN_IF(
            !preprocessInfos.SetCapacity(mPreprocessInfoCount, fallible))) {
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }

    IDB_TRY(TransformIfAbortOnErr(
        std::make_move_iterator(mResponse.begin()),
        std::make_move_iterator(mResponse.end()),
        MakeBackInserter(preprocessInfos),
        [](const auto& info) { return info.HasPreprocessInfo(); },
        [&self = *this](StructuredCloneReadInfoParent&& info) {
          return self.ConvertResponse<PreprocessInfo>(std::move(info));
        }));

    return PreprocessParams{std::move(params)};
  }

  auto params = ObjectStoreGetPreprocessParams();

  IDB_TRY_UNWRAP(params.preprocessInfo(),
                 ConvertResponse<PreprocessInfo>(std::move(mResponse[0])));

  return PreprocessParams{std::move(params)};
}

void ObjectStoreGetRequestOp::GetResponse(RequestResponse& aResponse,
                                          size_t* aResponseSize) {
  MOZ_ASSERT_IF(mLimit, mResponse.Length() <= mLimit);

  if (mGetAll) {
    aResponse = ObjectStoreGetAllResponse();
    *aResponseSize = 0;

    if (!mResponse.IsEmpty()) {
      IDB_TRY_UNWRAP(
          aResponse.get_ObjectStoreGetAllResponse().cloneInfos(),
          TransformIntoNewArrayAbortOnErr(
              std::make_move_iterator(mResponse.begin()),
              std::make_move_iterator(mResponse.end()),
              [this, &aResponseSize](StructuredCloneReadInfoParent&& info) {
                *aResponseSize += info.Size();
                return ConvertResponse<SerializedStructuredCloneReadInfo>(
                    std::move(info));
              },
              fallible),
          QM_VOID, [&aResponse](const nsresult result) { aResponse = result; });
    }

    return;
  }

  aResponse = ObjectStoreGetResponse();
  *aResponseSize = 0;

  if (!mResponse.IsEmpty()) {
    SerializedStructuredCloneReadInfo& serializedInfo =
        aResponse.get_ObjectStoreGetResponse().cloneInfo();

    *aResponseSize += mResponse[0].Size();
    IDB_TRY_UNWRAP(serializedInfo,
                   ConvertResponse<SerializedStructuredCloneReadInfo>(
                       std::move(mResponse[0])),
                   QM_VOID,
                   [&aResponse](const nsresult result) { aResponse = result; });
  }
}

ObjectStoreGetKeyRequestOp::ObjectStoreGetKeyRequestOp(
    SafeRefPtr<TransactionBase> aTransaction, const RequestParams& aParams,
    bool aGetAll)
    : NormalTransactionOp(std::move(aTransaction)),
      mObjectStoreId(
          aGetAll ? aParams.get_ObjectStoreGetAllKeysParams().objectStoreId()
                  : aParams.get_ObjectStoreGetKeyParams().objectStoreId()),
      mOptionalKeyRange(
          aGetAll ? aParams.get_ObjectStoreGetAllKeysParams().optionalKeyRange()
                  : Some(aParams.get_ObjectStoreGetKeyParams().keyRange())),
      mLimit(aGetAll ? aParams.get_ObjectStoreGetAllKeysParams().limit() : 1),
      mGetAll(aGetAll) {
  MOZ_ASSERT(aParams.type() == RequestParams::TObjectStoreGetKeyParams ||
             aParams.type() == RequestParams::TObjectStoreGetAllKeysParams);
  MOZ_ASSERT(mObjectStoreId);
  MOZ_ASSERT_IF(!aGetAll, mOptionalKeyRange.isSome());
}

nsresult ObjectStoreGetKeyRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("ObjectStoreGetKeyRequestOp::DoDatabaseWork", DOM);

  const nsCString query =
      "SELECT key "
      "FROM object_data "
      "WHERE object_store_id = :"_ns +
      kStmtParamNameObjectStoreId +
      MaybeGetBindingClauseForKeyRange(mOptionalKeyRange, kColumnNameKey) +
      " ORDER BY key ASC"_ns +
      (mLimit ? " LIMIT "_ns + IntToCString(mLimit) : EmptyCString());

  IDB_TRY_INSPECT(const auto& stmt, aConnection->BorrowCachedStatement(query));

  nsresult rv =
      stmt->BindInt64ByName(kStmtParamNameObjectStoreId, mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mOptionalKeyRange.isSome()) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.ref(), &*stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  IDB_TRY(CollectWhileHasResult(
      *stmt, [this](auto& stmt) mutable -> mozilla::Result<Ok, nsresult> {
        Key* const key = mResponse.AppendElement(fallible);
        IDB_TRY(OkIf(key), Err(NS_ERROR_OUT_OF_MEMORY));
        IDB_TRY(key->SetFromStatement(&stmt, 0));

        return Ok{};
      }));

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

void ObjectStoreGetKeyRequestOp::GetResponse(RequestResponse& aResponse,
                                             size_t* aResponseSize) {
  MOZ_ASSERT_IF(mLimit, mResponse.Length() <= mLimit);

  if (mGetAll) {
    aResponse = ObjectStoreGetAllKeysResponse();
    *aResponseSize = std::accumulate(mResponse.begin(), mResponse.end(), 0u,
                                     [](size_t old, const auto& entry) {
                                       return old + entry.GetBuffer().Length();
                                     });

    aResponse.get_ObjectStoreGetAllKeysResponse().keys() = std::move(mResponse);

    return;
  }

  aResponse = ObjectStoreGetKeyResponse();
  *aResponseSize = 0;

  if (!mResponse.IsEmpty()) {
    *aResponseSize = mResponse[0].GetBuffer().Length();
    aResponse.get_ObjectStoreGetKeyResponse().key() = std::move(mResponse[0]);
  }
}

ObjectStoreDeleteRequestOp::ObjectStoreDeleteRequestOp(
    SafeRefPtr<TransactionBase> aTransaction,
    const ObjectStoreDeleteParams& aParams)
    : NormalTransactionOp(std::move(aTransaction)),
      mParams(aParams),
      mObjectStoreMayHaveIndexes(false) {
  AssertIsOnBackgroundThread();

  RefPtr<FullObjectStoreMetadata> metadata =
      Transaction().GetMetadataForObjectStoreId(mParams.objectStoreId());
  MOZ_ASSERT(metadata);

  mObjectStoreMayHaveIndexes = metadata->HasLiveIndexes();
}

nsresult ObjectStoreDeleteRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  AUTO_PROFILER_LABEL("ObjectStoreDeleteRequestOp::DoDatabaseWork", DOM);

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  IDB_TRY_INSPECT(const bool& objectStoreHasIndexes,
                  ObjectStoreHasIndexes(*aConnection, mParams.objectStoreId(),
                                        mObjectStoreMayHaveIndexes));

  if (objectStoreHasIndexes) {
    IDB_TRY(DeleteObjectStoreDataTableRowsWithIndexes(
        aConnection, mParams.objectStoreId(), Some(mParams.keyRange())));
  } else {
    const auto keyRangeClause =
        GetBindingClauseForKeyRange(mParams.keyRange(), kColumnNameKey);

    IDB_TRY(aConnection->ExecuteCachedStatement(
        "DELETE FROM object_data "
        "WHERE object_store_id = :"_ns +
            kStmtParamNameObjectStoreId + keyRangeClause + ";"_ns,
        [&params = mParams](mozIStorageStatement& stmt) -> nsresult {
          IDB_TRY(stmt.BindInt64ByName(kStmtParamNameObjectStoreId,
                                       params.objectStoreId()));

          IDB_TRY(BindKeyRangeToStatement(params.keyRange(), &stmt));

          return NS_OK;
        }));
  }

  IDB_TRY(autoSave.Commit());

  return NS_OK;
}

ObjectStoreClearRequestOp::ObjectStoreClearRequestOp(
    SafeRefPtr<TransactionBase> aTransaction,
    const ObjectStoreClearParams& aParams)
    : NormalTransactionOp(std::move(aTransaction)),
      mParams(aParams),
      mObjectStoreMayHaveIndexes(false) {
  AssertIsOnBackgroundThread();

  RefPtr<FullObjectStoreMetadata> metadata =
      Transaction().GetMetadataForObjectStoreId(mParams.objectStoreId());
  MOZ_ASSERT(metadata);

  mObjectStoreMayHaveIndexes = metadata->HasLiveIndexes();
}

nsresult ObjectStoreClearRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("ObjectStoreClearRequestOp::DoDatabaseWork", DOM);

  DatabaseConnection::AutoSavepoint autoSave;
  IDB_TRY(autoSave.Start(Transaction())
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
              ,
          QM_PROPAGATE, MakeAutoSavepointCleanupHandler(*aConnection)
#endif
  );

  IDB_TRY_INSPECT(const bool& objectStoreHasIndexes,
                  ObjectStoreHasIndexes(*aConnection, mParams.objectStoreId(),
                                        mObjectStoreMayHaveIndexes));

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  IDB_TRY(objectStoreHasIndexes
              ? DeleteObjectStoreDataTableRowsWithIndexes(
                    aConnection, mParams.objectStoreId(), Nothing())
              : aConnection->ExecuteCachedStatement(
                    "DELETE FROM object_data "
                    "WHERE object_store_id = :object_store_id;"_ns,
                    [objectStoreId = mParams.objectStoreId()](
                        mozIStorageStatement& stmt) -> nsresult {
                      IDB_TRY(stmt.BindInt64ByIndex(0, objectStoreId));

                      return NS_OK;
                    }));

  IDB_TRY(autoSave.Commit());

  return NS_OK;
}

nsresult ObjectStoreCountRequestOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("ObjectStoreCountRequestOp::DoDatabaseWork", DOM);

  const auto keyRangeClause = MaybeGetBindingClauseForKeyRange(
      mParams.optionalKeyRange(), kColumnNameKey);

  IDB_TRY_INSPECT(
      const auto& maybeStmt,
      aConnection->BorrowAndExecuteSingleStepStatement(
          "SELECT count(*) "
          "FROM object_data "
          "WHERE object_store_id = :"_ns +
              kStmtParamNameObjectStoreId + keyRangeClause,
          [&params = mParams](auto& stmt) -> mozilla::Result<Ok, nsresult> {
            IDB_TRY(stmt.BindInt64ByName(kStmtParamNameObjectStoreId,
                                         params.objectStoreId()));

            if (params.optionalKeyRange().isSome()) {
              IDB_TRY(BindKeyRangeToStatement(params.optionalKeyRange().ref(),
                                              &stmt));
            }

            return Ok{};
          }));

  IDB_TRY(OkIf(maybeStmt.isSome()), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR,
          [](const auto) {
            // XXX Why do we have an assertion here, but not at most other
            // places using IDB_REPORT_INTERNAL_ERR(_LAMBDA)?
            MOZ_ASSERT(false, "This should never be possible!");
            IDB_REPORT_INTERNAL_ERR();
          });

  const auto& stmt = *maybeStmt;

  const int64_t count = stmt->AsInt64(0);
  IDB_TRY(OkIf(count >= 0), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, [](const auto) {
    // XXX Why do we have an assertion here, but not at most other places using
    // IDB_REPORT_INTERNAL_ERR(_LAMBDA)?
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
  });

  mResponse.count() = count;

  return NS_OK;
}

// static
RefPtr<FullIndexMetadata> IndexRequestOpBase::IndexMetadataForParams(
    const TransactionBase& aTransaction, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetParams ||
             aParams.type() == RequestParams::TIndexGetKeyParams ||
             aParams.type() == RequestParams::TIndexGetAllParams ||
             aParams.type() == RequestParams::TIndexGetAllKeysParams ||
             aParams.type() == RequestParams::TIndexCountParams);

  IndexOrObjectStoreId objectStoreId;
  IndexOrObjectStoreId indexId;

  switch (aParams.type()) {
    case RequestParams::TIndexGetParams: {
      const IndexGetParams& params = aParams.get_IndexGetParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    case RequestParams::TIndexGetKeyParams: {
      const IndexGetKeyParams& params = aParams.get_IndexGetKeyParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    case RequestParams::TIndexGetAllParams: {
      const IndexGetAllParams& params = aParams.get_IndexGetAllParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    case RequestParams::TIndexGetAllKeysParams: {
      const IndexGetAllKeysParams& params = aParams.get_IndexGetAllKeysParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    case RequestParams::TIndexCountParams: {
      const IndexCountParams& params = aParams.get_IndexCountParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  const RefPtr<FullObjectStoreMetadata> objectStoreMetadata =
      aTransaction.GetMetadataForObjectStoreId(objectStoreId);
  MOZ_ASSERT(objectStoreMetadata);

  RefPtr<FullIndexMetadata> indexMetadata =
      aTransaction.GetMetadataForIndexId(objectStoreMetadata, indexId);
  MOZ_ASSERT(indexMetadata);

  return indexMetadata;
}

IndexGetRequestOp::IndexGetRequestOp(SafeRefPtr<TransactionBase> aTransaction,
                                     const RequestParams& aParams, bool aGetAll)
    : IndexRequestOpBase(std::move(aTransaction), aParams),
      mDatabase(Transaction().GetDatabasePtr()),
      mOptionalKeyRange(aGetAll
                            ? aParams.get_IndexGetAllParams().optionalKeyRange()
                            : Some(aParams.get_IndexGetParams().keyRange())),
      mBackgroundParent(Transaction().GetBackgroundParent()),
      mLimit(aGetAll ? aParams.get_IndexGetAllParams().limit() : 1),
      mGetAll(aGetAll) {
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetParams ||
             aParams.type() == RequestParams::TIndexGetAllParams);
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT_IF(!aGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT(mBackgroundParent);
}

nsresult IndexGetRequestOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(!mGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  AUTO_PROFILER_LABEL("IndexGetRequestOp::DoDatabaseWork", DOM);

  const auto indexTable = mMetadata->mCommonMetadata.unique()
                              ? "unique_index_data "_ns
                              : "index_data "_ns;

  IDB_TRY_INSPECT(
      const auto& stmt,
      aConnection->BorrowCachedStatement(
          "SELECT file_ids, data "
          "FROM object_data "
          "INNER JOIN "_ns +
          indexTable +
          "AS index_table "
          "ON object_data.object_store_id = "
          "index_table.object_store_id "
          "AND object_data.key = "
          "index_table.object_data_key "
          "WHERE index_id = :"_ns +
          kStmtParamNameIndexId +
          MaybeGetBindingClauseForKeyRange(mOptionalKeyRange,
                                           kColumnNameValue) +
          (mLimit ? kOpenLimit + IntToCString(mLimit) : EmptyCString())));

  IDB_TRY(stmt->BindInt64ByName(kStmtParamNameIndexId,
                                mMetadata->mCommonMetadata.id()));

  if (mOptionalKeyRange.isSome()) {
    IDB_TRY(BindKeyRangeToStatement(mOptionalKeyRange.ref(), &*stmt));
  }

  IDB_TRY(CollectWhileHasResult(
      *stmt, [this](auto& stmt) mutable -> mozilla::Result<Ok, nsresult> {
        IDB_TRY_UNWRAP(auto cloneInfo,
                       GetStructuredCloneReadInfoFromStatement(
                           &stmt, 1, 0, mDatabase->GetFileManager(),
                           mDatabase->MaybeKeyRef()));

        if (cloneInfo.HasPreprocessInfo()) {
          IDB_WARNING("Preprocessing for indexes not yet implemented!");
          return Err(NS_ERROR_NOT_IMPLEMENTED);
        }

        IDB_TRY(OkIf(mResponse.EmplaceBack(fallible, std::move(cloneInfo))),
                Err(NS_ERROR_OUT_OF_MEMORY));

        return Ok{};
      }));

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

// XXX This is more or less a duplicate of ObjectStoreGetRequestOp::GetResponse
void IndexGetRequestOp::GetResponse(RequestResponse& aResponse,
                                    size_t* aResponseSize) {
  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  auto convertResponse = [this](StructuredCloneReadInfoParent&& info)
      -> mozilla::Result<SerializedStructuredCloneReadInfo, nsresult> {
    SerializedStructuredCloneReadInfo result;

    result.data().data = info.ReleaseData();

    IDB_TRY_UNWRAP(result.files(),
                   SerializeStructuredCloneFiles(mBackgroundParent, mDatabase,
                                                 info.Files(), false));

    return result;
  };

  if (mGetAll) {
    aResponse = IndexGetAllResponse();
    *aResponseSize = 0;

    if (!mResponse.IsEmpty()) {
      IDB_TRY_UNWRAP(
          aResponse.get_IndexGetAllResponse().cloneInfos(),
          TransformIntoNewArrayAbortOnErr(
              std::make_move_iterator(mResponse.begin()),
              std::make_move_iterator(mResponse.end()),
              [convertResponse,
               &aResponseSize](StructuredCloneReadInfoParent&& info) {
                *aResponseSize += info.Size();
                return convertResponse(std::move(info));
              },
              fallible),
          QM_VOID, [&aResponse](const nsresult result) { aResponse = result; });
    }

    return;
  }

  aResponse = IndexGetResponse();
  *aResponseSize = 0;

  if (!mResponse.IsEmpty()) {
    SerializedStructuredCloneReadInfo& serializedInfo =
        aResponse.get_IndexGetResponse().cloneInfo();

    *aResponseSize += mResponse[0].Size();
    IDB_TRY_UNWRAP(serializedInfo, convertResponse(std::move(mResponse[0])),
                   QM_VOID,
                   [&aResponse](const nsresult result) { aResponse = result; });
  }
}

IndexGetKeyRequestOp::IndexGetKeyRequestOp(
    SafeRefPtr<TransactionBase> aTransaction, const RequestParams& aParams,
    bool aGetAll)
    : IndexRequestOpBase(std::move(aTransaction), aParams),
      mOptionalKeyRange(
          aGetAll ? aParams.get_IndexGetAllKeysParams().optionalKeyRange()
                  : Some(aParams.get_IndexGetKeyParams().keyRange())),
      mLimit(aGetAll ? aParams.get_IndexGetAllKeysParams().limit() : 1),
      mGetAll(aGetAll) {
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetKeyParams ||
             aParams.type() == RequestParams::TIndexGetAllKeysParams);
  MOZ_ASSERT_IF(!aGetAll, mOptionalKeyRange.isSome());
}

nsresult IndexGetKeyRequestOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(!mGetAll, mOptionalKeyRange.isSome());
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  AUTO_PROFILER_LABEL("IndexGetKeyRequestOp::DoDatabaseWork", DOM);

  const bool hasKeyRange = mOptionalKeyRange.isSome();

  const auto indexTable = mMetadata->mCommonMetadata.unique()
                              ? "unique_index_data "_ns
                              : "index_data "_ns;

  const nsCString query =
      "SELECT object_data_key "
      "FROM "_ns +
      indexTable + "WHERE index_id = :"_ns + kStmtParamNameIndexId +
      MaybeGetBindingClauseForKeyRange(mOptionalKeyRange, kColumnNameValue) +
      (mLimit ? kOpenLimit + IntToCString(mLimit) : EmptyCString());

  IDB_TRY_INSPECT(const auto& stmt, aConnection->BorrowCachedStatement(query));

  IDB_TRY(stmt->BindInt64ByName(kStmtParamNameIndexId,
                                mMetadata->mCommonMetadata.id()));

  if (hasKeyRange) {
    IDB_TRY(BindKeyRangeToStatement(mOptionalKeyRange.ref(), &*stmt));
  }

  IDB_TRY(CollectWhileHasResult(
      *stmt, [this](auto& stmt) mutable -> mozilla::Result<Ok, nsresult> {
        Key* const key = mResponse.AppendElement(fallible);
        IDB_TRY(OkIf(key), Err(NS_ERROR_OUT_OF_MEMORY));
        IDB_TRY(key->SetFromStatement(&stmt, 0));

        return Ok{};
      }));

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

void IndexGetKeyRequestOp::GetResponse(RequestResponse& aResponse,
                                       size_t* aResponseSize) {
  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  if (mGetAll) {
    aResponse = IndexGetAllKeysResponse();
    *aResponseSize = std::accumulate(mResponse.begin(), mResponse.end(), 0u,
                                     [](size_t old, const auto& entry) {
                                       return old + entry.GetBuffer().Length();
                                     });

    aResponse.get_IndexGetAllKeysResponse().keys() = std::move(mResponse);

    return;
  }

  aResponse = IndexGetKeyResponse();
  *aResponseSize = 0;

  if (!mResponse.IsEmpty()) {
    *aResponseSize = mResponse[0].GetBuffer().Length();
    aResponse.get_IndexGetKeyResponse().key() = std::move(mResponse[0]);
  }
}

nsresult IndexCountRequestOp::DoDatabaseWork(DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();

  AUTO_PROFILER_LABEL("IndexCountRequestOp::DoDatabaseWork", DOM);

  const auto indexTable = mMetadata->mCommonMetadata.unique()
                              ? "unique_index_data "_ns
                              : "index_data "_ns;

  const auto keyRangeClause = MaybeGetBindingClauseForKeyRange(
      mParams.optionalKeyRange(), kColumnNameValue);

  IDB_TRY_INSPECT(
      const auto& maybeStmt,
      aConnection->BorrowAndExecuteSingleStepStatement(
          "SELECT count(*) "
          "FROM "_ns +
              indexTable + "WHERE index_id = :"_ns + kStmtParamNameIndexId +
              keyRangeClause,
          [&self = *this](auto& stmt) -> mozilla::Result<Ok, nsresult> {
            IDB_TRY(stmt.BindInt64ByName(kStmtParamNameIndexId,
                                         self.mMetadata->mCommonMetadata.id()));

            if (self.mParams.optionalKeyRange().isSome()) {
              IDB_TRY(BindKeyRangeToStatement(
                  self.mParams.optionalKeyRange().ref(), &stmt));
            }

            return Ok{};
          }));

  IDB_TRY(OkIf(maybeStmt.isSome()), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR,
          [](const auto) {
            // XXX Why do we have an assertion here, but not at most other
            // places using IDB_REPORT_INTERNAL_ERR(_LAMBDA)?
            MOZ_ASSERT(false, "This should never be possible!");
            IDB_REPORT_INTERNAL_ERR();
          });

  const auto& stmt = *maybeStmt;

  const int64_t count = stmt->AsInt64(0);
  IDB_TRY(OkIf(count >= 0), NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, [](const auto) {
    // XXX Why do we have an assertion here, but not at most other places using
    // IDB_REPORT_INTERNAL_ERR(_LAMBDA)?
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
  });

  mResponse.count() = count;

  return NS_OK;
}

template <IDBCursorType CursorType>
bool Cursor<CursorType>::CursorOpBase::SendFailureResult(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mCurrentlyRunningOp == this);
  MOZ_ASSERT(!mResponseSent);

  if (!IsActorDestroyed()) {
    mResponse = ClampResultCode(aResultCode);

    // This is an expected race when the transaction is invalidated after
    // data is retrieved from database.
    //
    // TODO: There seem to be other cases when mFiles is non-empty here, which
    // have been present before adding cursor preloading, but with cursor
    // preloading they have become more frequent (also during startup). One
    // possible cause with cursor preloading is to be addressed by Bug 1597191.
    NS_WARNING_ASSERTION(
        !mFiles.IsEmpty() && !Transaction().IsInvalidated(),
        "Expected empty mFiles when transaction has not been invalidated");

    // SendResponseInternal will assert when mResponse.type() is
    // CursorResponse::Tnsresult and mFiles is non-empty, so we clear mFiles
    // here.
    mFiles.Clear();

    mCursor->SendResponseInternal(mResponse, mFiles);
  }

#ifdef DEBUG
  mResponseSent = true;
#endif
  return false;
}

template <IDBCursorType CursorType>
void Cursor<CursorType>::CursorOpBase::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT_IF(!IsActorDestroyed(), mResponseSent);

  mCursor = nullptr;

#ifdef DEBUG
  // A bit hacky but the CursorOp request is not generated in response to a
  // child request like most other database operations. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

template <IDBCursorType CursorType>
ResponseSizeOrError
CursorOpBaseHelperBase<CursorType>::PopulateResponseFromStatement(
    mozIStorageStatement* const aStmt, const bool aInitializeResponse,
    Key* const aOptOutSortKey) {
  mOp.Transaction().AssertIsOnConnectionThread();
  MOZ_ASSERT_IF(aInitializeResponse,
                mOp.mResponse.type() == CursorResponse::T__None);
  MOZ_ASSERT_IF(!aInitializeResponse,
                mOp.mResponse.type() != CursorResponse::T__None);
  MOZ_ASSERT_IF(
      mOp.mFiles.IsEmpty() &&
          (mOp.mResponse.type() ==
               CursorResponse::TArrayOfObjectStoreCursorResponse ||
           mOp.mResponse.type() == CursorResponse::TArrayOfIndexCursorResponse),
      aInitializeResponse);

  auto populateResponseHelper = PopulateResponseHelper<CursorType>{mOp};
  auto previousKey = aOptOutSortKey ? std::move(*aOptOutSortKey) : Key{};

  IDB_TRY(populateResponseHelper.GetKeys(aStmt, aOptOutSortKey));

  // aOptOutSortKey must be set iff the cursor is a unique cursor. For unique
  // cursors, we need to skip records with the same key. The SQL queries
  // currently do not filter these out.
  if (aOptOutSortKey && !previousKey.IsUnset() &&
      previousKey == *aOptOutSortKey) {
    return 0;
  }

  IDB_TRY(populateResponseHelper.MaybeGetCloneInfo(aStmt, GetCursor()));

  // CAUTION: It is important that only the part of the function above this
  // comment may fail, and modifications to the data structure (in particular
  // mResponse and mFiles) may only be made below. This is necessary to allow to
  // discard entries that were attempted to be preloaded without causing an
  // inconsistent state.

  if (aInitializeResponse) {
    mOp.mResponse = std::remove_reference_t<decltype(
        populateResponseHelper.GetTypedResponse(&mOp.mResponse))>();
  }

  auto& responses = populateResponseHelper.GetTypedResponse(&mOp.mResponse);
  auto& response = *responses.AppendElement();

  populateResponseHelper.FillKeys(response);
  if constexpr (!CursorTypeTraits<CursorType>::IsKeyOnlyCursor) {
    populateResponseHelper.MaybeFillCloneInfo(response, &mOp.mFiles);
  }

  return populateResponseHelper.GetKeySize(response) +
         populateResponseHelper.MaybeGetCloneInfoSize(response);
}

template <IDBCursorType CursorType>
void CursorOpBaseHelperBase<CursorType>::PopulateExtraResponses(
    mozIStorageStatement* const aStmt, const uint32_t aMaxExtraCount,
    const size_t aInitialResponseSize, const nsCString& aOperation,
    Key* const aOptPreviousSortKey) {
  mOp.AssertIsOnConnectionThread();

  const auto extraCount = [&]() -> uint32_t {
    auto accumulatedResponseSize = aInitialResponseSize;
    uint32_t extraCount = 0;

    do {
      bool hasResult;
      nsresult rv = aStmt->ExecuteStep(&hasResult);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // In case of a failure on one step, do not attempt to execute further
        // steps, but use the results already populated.

        break;
      }

      if (!hasResult) {
        break;
      }

      // PopulateResponseFromStatement does not modify the data in case of
      // failure, so we can just use the results already populated, and discard
      // any remaining entries, and signal overall success. Probably, future
      // attempts to access the same entry will fail as well, but it might never
      // be accessed by the application.
      IDB_TRY_INSPECT(
          const auto& responseSize,
          PopulateResponseFromStatement(aStmt, false, aOptPreviousSortKey),
          extraCount, [](const auto&) {
            // TODO: Maybe disable preloading for this cursor? The problem will
            // probably reoccur on the next attempt, and disabling preloading
            // will reduce latency. However, if some problematic entry will be
            // skipped over, after that it might be fine again. To judge this,
            // the causes for such failures would need to be analyzed more
            // thoroughly. Since this seems to be rare, maybe no further action
            // is necessary at all.
          });

      // Check accumulated size of individual responses and maybe break early.
      accumulatedResponseSize += responseSize;
      if (accumulatedResponseSize > IPC::Channel::kMaximumMessageSize / 2) {
        IDB_LOG_MARK_PARENT_TRANSACTION_REQUEST(
            "PRELOAD: %s: Dropping entries because maximum message size is "
            "exceeded: %" PRIu32 "/%zu bytes",
            "%.0s Dropping too large (%" PRIu32 "/%zu)",
            IDB_LOG_ID_STRING(mOp.mBackgroundChildLoggingId),
            mOp.mTransactionLoggingSerialNumber, mOp.mLoggingSerialNumber,
            aOperation.get(), extraCount, accumulatedResponseSize);

        break;
      }

      // TODO: Do not count entries skipped for unique cursors.
      ++extraCount;
    } while (true);

    return extraCount;
  }();

  IDB_LOG_MARK_PARENT_TRANSACTION_REQUEST(
      "PRELOAD: %s: Number of extra results populated: %" PRIu32 "/%" PRIu32,
      "%.0s Populated (%" PRIu32 "/%" PRIu32 ")",
      IDB_LOG_ID_STRING(mOp.mBackgroundChildLoggingId),
      mOp.mTransactionLoggingSerialNumber, mOp.mLoggingSerialNumber,
      aOperation.get(), extraCount, aMaxExtraCount);
}

template <IDBCursorType CursorType>
void Cursor<CursorType>::SetOptionalKeyRange(
    const Maybe<SerializedKeyRange>& aOptionalKeyRange, bool* const aOpen) {
  MOZ_ASSERT(aOpen);

  Key localeAwareRangeBound;

  if (aOptionalKeyRange.isSome()) {
    const SerializedKeyRange& range = aOptionalKeyRange.ref();

    const bool lowerBound = !IsIncreasingOrder(mDirection);
    *aOpen =
        !range.isOnly() && (lowerBound ? range.lowerOpen() : range.upperOpen());

    const auto& bound =
        (range.isOnly() || lowerBound) ? range.lower() : range.upper();
    if constexpr (IsIndexCursor) {
      if (this->IsLocaleAware()) {
        // XXX Don't we need to propagate the error?
        IDB_TRY_UNWRAP(localeAwareRangeBound,
                       bound.ToLocaleAwareKey(this->mLocale), QM_VOID);
      } else {
        localeAwareRangeBound = bound;
      }
    } else {
      localeAwareRangeBound = bound;
    }
  } else {
    *aOpen = false;
  }

  this->mLocaleAwareRangeBound.init(std::move(localeAwareRangeBound));
}

template <IDBCursorType CursorType>
void ObjectStoreOpenOpHelper<CursorType>::PrepareKeyConditionClauses(
    const nsACString& aDirectionClause, const nsCString& aQueryStart) {
  const bool isIncreasingOrder = IsIncreasingOrder(GetCursor().mDirection);

  nsAutoCString keyRangeClause;
  nsAutoCString continueToKeyRangeClause;
  AppendConditionClause(kStmtParamNameKey, kStmtParamNameCurrentKey,
                        !isIncreasingOrder, false, keyRangeClause);
  AppendConditionClause(kStmtParamNameKey, kStmtParamNameCurrentKey,
                        !isIncreasingOrder, true, continueToKeyRangeClause);

  {
    bool open;
    GetCursor().SetOptionalKeyRange(GetOptionalKeyRange(), &open);

    if (GetOptionalKeyRange().isSome() &&
        !GetCursor().mLocaleAwareRangeBound->IsUnset()) {
      AppendConditionClause(kStmtParamNameKey, kStmtParamNameRangeBound,
                            isIncreasingOrder, !open, keyRangeClause);
      AppendConditionClause(kStmtParamNameKey, kStmtParamNameRangeBound,
                            isIncreasingOrder, !open, continueToKeyRangeClause);
    }
  }

  const nsAutoCString suffix =
      aDirectionClause + kOpenLimit + ":"_ns + kStmtParamNameLimit;

  GetCursor().mContinueQueries.init(
      aQueryStart + keyRangeClause + suffix,
      aQueryStart + continueToKeyRangeClause + suffix);
}

template <IDBCursorType CursorType>
void IndexOpenOpHelper<CursorType>::PrepareIndexKeyConditionClause(
    const nsACString& aDirectionClause,
    const nsLiteralCString& aObjectDataKeyPrefix, nsAutoCString aQueryStart) {
  const bool isIncreasingOrder = IsIncreasingOrder(GetCursor().mDirection);

  {
    bool open;
    GetCursor().SetOptionalKeyRange(GetOptionalKeyRange(), &open);
    if (GetOptionalKeyRange().isSome() &&
        !GetCursor().mLocaleAwareRangeBound->IsUnset()) {
      AppendConditionClause(kColumnNameAliasSortKey, kStmtParamNameRangeBound,
                            isIncreasingOrder, !open, aQueryStart);
    }
  }

  nsCString continueQuery, continueToQuery, continuePrimaryKeyQuery;

  continueToQuery =
      aQueryStart + " AND "_ns +
      GetSortKeyClause(isIncreasingOrder ? ComparisonOperator::GreaterOrEquals
                                         : ComparisonOperator::LessOrEquals,
                       kStmtParamNameCurrentKey);

  switch (GetCursor().mDirection) {
    case IDBCursorDirection::Next:
    case IDBCursorDirection::Prev:
      continueQuery =
          aQueryStart + " AND "_ns +
          GetSortKeyClause(isIncreasingOrder
                               ? ComparisonOperator::GreaterOrEquals
                               : ComparisonOperator::LessOrEquals,
                           kStmtParamNameCurrentKey) +
          " AND ( "_ns +
          GetSortKeyClause(isIncreasingOrder ? ComparisonOperator::GreaterThan
                                             : ComparisonOperator::LessThan,
                           kStmtParamNameCurrentKey) +
          " OR "_ns +
          GetKeyClause(aObjectDataKeyPrefix + "object_data_key"_ns,
                       isIncreasingOrder ? ComparisonOperator::GreaterThan
                                         : ComparisonOperator::LessThan,
                       kStmtParamNameObjectStorePosition) +
          " ) "_ns;

      continuePrimaryKeyQuery =
          aQueryStart +
          " AND ("
          "("_ns +
          GetSortKeyClause(ComparisonOperator::Equals,
                           kStmtParamNameCurrentKey) +
          " AND "_ns +
          GetKeyClause(aObjectDataKeyPrefix + "object_data_key"_ns,
                       isIncreasingOrder ? ComparisonOperator::GreaterOrEquals
                                         : ComparisonOperator::LessOrEquals,
                       kStmtParamNameObjectStorePosition) +
          ") OR "_ns +
          GetSortKeyClause(isIncreasingOrder ? ComparisonOperator::GreaterThan
                                             : ComparisonOperator::LessThan,
                           kStmtParamNameCurrentKey) +
          ")"_ns;
      break;

    case IDBCursorDirection::Nextunique:
    case IDBCursorDirection::Prevunique:
      continueQuery =
          aQueryStart + " AND "_ns +
          GetSortKeyClause(isIncreasingOrder ? ComparisonOperator::GreaterThan
                                             : ComparisonOperator::LessThan,
                           kStmtParamNameCurrentKey);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  const nsAutoCString suffix =
      aDirectionClause + kOpenLimit + ":"_ns + kStmtParamNameLimit;
  continueQuery += suffix;
  continueToQuery += suffix;
  if (!continuePrimaryKeyQuery.IsEmpty()) {
    continuePrimaryKeyQuery += suffix;
  }

  GetCursor().mContinueQueries.init(std::move(continueQuery),
                                    std::move(continueToQuery),
                                    std::move(continuePrimaryKeyQuery));
}

template <IDBCursorType CursorType>
nsresult CommonOpenOpHelper<CursorType>::ProcessStatementSteps(
    mozIStorageStatement* const aStmt) {
  IDB_TRY_INSPECT(const bool& hasResult,
                  MOZ_TO_RESULT_INVOKE(aStmt, ExecuteStep));

  if (!hasResult) {
    SetResponse(void_t{});
    return NS_OK;
  }

  Key previousKey;
  auto* optPreviousKey =
      IsUnique(GetCursor().mDirection) ? &previousKey : nullptr;

  IDB_TRY_INSPECT(const auto& responseSize,
                  PopulateResponseFromStatement(aStmt, true, optPreviousKey));

  // The degree to which extra responses on OpenOp can actually be used depends
  // on the parameters of subsequent ContinueOp operations, see also comment in
  // ContinueOp::DoDatabaseWork.
  //
  // TODO: We should somehow evaluate the effects of this. Maybe use a smaller
  // extra count than for ContinueOp?
  PopulateExtraResponses(aStmt, GetCursor().mMaxExtraCount, responseSize,
                         "OpenOp"_ns, optPreviousKey);

  return NS_OK;
}

nsresult OpenOpHelper<IDBCursorType::ObjectStore>::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(GetCursor().mObjectStoreId);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoObjectStoreDatabaseWork", DOM);

  const bool usingKeyRange = GetOptionalKeyRange().isSome();

  const nsCString queryStart = "SELECT "_ns + kColumnNameKey +
                               ", file_ids, data "
                               "FROM object_data "
                               "WHERE object_store_id = :"_ns +
                               kStmtParamNameId;

  const auto keyRangeClause =
      DatabaseOperationBase::MaybeGetBindingClauseForKeyRange(
          GetOptionalKeyRange(), kColumnNameKey);

  const auto& directionClause = MakeDirectionClause(GetCursor().mDirection);

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  const nsCString firstQuery = queryStart + keyRangeClause + directionClause +
                               kOpenLimit +
                               IntToCString(1 + GetCursor().mMaxExtraCount);

  IDB_TRY_INSPECT(const auto& stmt,
                  aConnection->BorrowCachedStatement(firstQuery));

  IDB_TRY(stmt->BindInt64ByName(kStmtParamNameId, GetCursor().mObjectStoreId));

  if (usingKeyRange) {
    IDB_TRY(DatabaseOperationBase::BindKeyRangeToStatement(
        GetOptionalKeyRange().ref(), &*stmt));
  }

  // Now we need to make the query for ContinueOp.
  PrepareKeyConditionClauses(directionClause, queryStart);

  return ProcessStatementSteps(&*stmt);
}

nsresult OpenOpHelper<IDBCursorType::ObjectStoreKey>::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(GetCursor().mObjectStoreId);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoObjectStoreKeyDatabaseWork", DOM);

  const bool usingKeyRange = GetOptionalKeyRange().isSome();

  const nsCString queryStart = "SELECT "_ns + kColumnNameKey +
                               " FROM object_data "
                               "WHERE object_store_id = :"_ns +
                               kStmtParamNameId;

  const auto keyRangeClause =
      DatabaseOperationBase::MaybeGetBindingClauseForKeyRange(
          GetOptionalKeyRange(), kColumnNameKey);

  const auto& directionClause = MakeDirectionClause(GetCursor().mDirection);

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  const nsCString firstQuery =
      queryStart + keyRangeClause + directionClause + kOpenLimit + "1"_ns;

  IDB_TRY_INSPECT(const auto& stmt,
                  aConnection->BorrowCachedStatement(firstQuery));

  IDB_TRY(stmt->BindInt64ByName(kStmtParamNameId, GetCursor().mObjectStoreId));

  if (usingKeyRange) {
    IDB_TRY(DatabaseOperationBase::BindKeyRangeToStatement(
        GetOptionalKeyRange().ref(), &*stmt));
  }

  // Now we need to make the query to get the next match.
  PrepareKeyConditionClauses(directionClause, queryStart);

  return ProcessStatementSteps(&*stmt);
}

nsresult OpenOpHelper<IDBCursorType::Index>::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(GetCursor().mObjectStoreId);
  MOZ_ASSERT(GetCursor().mIndexId);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoIndexDatabaseWork", DOM);

  const bool usingKeyRange = GetOptionalKeyRange().isSome();

  const auto indexTable =
      GetCursor().mUniqueIndex ? "unique_index_data"_ns : "index_data"_ns;

  // The result of MakeColumnPairSelectionList is stored in a local variable,
  // since inlining it into the next statement causes a crash on some Mac OS X
  // builds (see https://bugzilla.mozilla.org/show_bug.cgi?id=1168606#c110).
  const auto columnPairSelectionList = MakeColumnPairSelectionList(
      "index_table.value"_ns, "index_table.value_locale"_ns,
      kColumnNameAliasSortKey, GetCursor().IsLocaleAware());
  const nsCString sortColumnAlias =
      "SELECT "_ns + columnPairSelectionList + ", "_ns;

  const nsAutoCString queryStart = sortColumnAlias +
                                   "index_table.object_data_key, "
                                   "object_data.file_ids, "
                                   "object_data.data "
                                   "FROM "_ns +
                                   indexTable +
                                   " AS index_table "
                                   "JOIN object_data "
                                   "ON index_table.object_store_id = "
                                   "object_data.object_store_id "
                                   "AND index_table.object_data_key = "
                                   "object_data.key "
                                   "WHERE index_table.index_id = :"_ns +
                                   kStmtParamNameId;

  const auto keyRangeClause =
      DatabaseOperationBase::MaybeGetBindingClauseForKeyRange(
          GetOptionalKeyRange(), kColumnNameAliasSortKey);

  nsAutoCString directionClause = " ORDER BY "_ns + kColumnNameAliasSortKey;

  switch (GetCursor().mDirection) {
    case IDBCursorDirection::Next:
    case IDBCursorDirection::Nextunique:
      directionClause.AppendLiteral(" ASC, index_table.object_data_key ASC");
      break;

    case IDBCursorDirection::Prev:
      directionClause.AppendLiteral(" DESC, index_table.object_data_key DESC");
      break;

    case IDBCursorDirection::Prevunique:
      directionClause.AppendLiteral(" DESC, index_table.object_data_key ASC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  const nsCString firstQuery = queryStart + keyRangeClause + directionClause +
                               kOpenLimit +
                               IntToCString(1 + GetCursor().mMaxExtraCount);

  IDB_TRY_INSPECT(const auto& stmt,
                  aConnection->BorrowCachedStatement(firstQuery));

  IDB_TRY(stmt->BindInt64ByName(kStmtParamNameId, GetCursor().mIndexId));

  if (usingKeyRange) {
    if (GetCursor().IsLocaleAware()) {
      IDB_TRY(DatabaseOperationBase::BindKeyRangeToStatement(
          GetOptionalKeyRange().ref(), &*stmt, GetCursor().mLocale));
    } else {
      IDB_TRY(DatabaseOperationBase::BindKeyRangeToStatement(
          GetOptionalKeyRange().ref(), &*stmt));
    }
  }

  // TODO: At least the last two statements are almost the same in all
  // DoDatabaseWork variants, consider removing this duplication.

  // Now we need to make the query to get the next match.
  PrepareKeyConditionClauses(directionClause, std::move(queryStart));

  return ProcessStatementSteps(&*stmt);
}

nsresult OpenOpHelper<IDBCursorType::IndexKey>::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(GetCursor().mObjectStoreId);
  MOZ_ASSERT(GetCursor().mIndexId);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoIndexKeyDatabaseWork", DOM);

  const bool usingKeyRange = GetOptionalKeyRange().isSome();

  const auto table =
      GetCursor().mUniqueIndex ? "unique_index_data"_ns : "index_data"_ns;

  // The result of MakeColumnPairSelectionList is stored in a local variable,
  // since inlining it into the next statement causes a crash on some Mac OS X
  // builds (see https://bugzilla.mozilla.org/show_bug.cgi?id=1168606#c110).
  const auto columnPairSelectionList = MakeColumnPairSelectionList(
      "value"_ns, "value_locale"_ns, kColumnNameAliasSortKey,
      GetCursor().IsLocaleAware());
  const nsCString sortColumnAlias =
      "SELECT "_ns + columnPairSelectionList + ", "_ns;

  const nsAutoCString queryStart = sortColumnAlias +
                                   "object_data_key "
                                   " FROM "_ns +
                                   table + " WHERE index_id = :"_ns +
                                   kStmtParamNameId;

  const auto keyRangeClause =
      DatabaseOperationBase::MaybeGetBindingClauseForKeyRange(
          GetOptionalKeyRange(), kColumnNameAliasSortKey);

  nsAutoCString directionClause = " ORDER BY "_ns + kColumnNameAliasSortKey;

  switch (GetCursor().mDirection) {
    case IDBCursorDirection::Next:
    case IDBCursorDirection::Nextunique:
      directionClause.AppendLiteral(" ASC, object_data_key ASC");
      break;

    case IDBCursorDirection::Prev:
      directionClause.AppendLiteral(" DESC, object_data_key DESC");
      break;

    case IDBCursorDirection::Prevunique:
      directionClause.AppendLiteral(" DESC, object_data_key ASC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.
  const nsCString firstQuery =
      queryStart + keyRangeClause + directionClause + kOpenLimit + "1"_ns;

  IDB_TRY_INSPECT(const auto& stmt,
                  aConnection->BorrowCachedStatement(firstQuery));

  IDB_TRY(stmt->BindInt64ByName(kStmtParamNameId, GetCursor().mIndexId));

  if (usingKeyRange) {
    if (GetCursor().IsLocaleAware()) {
      IDB_TRY(DatabaseOperationBase::BindKeyRangeToStatement(
          GetOptionalKeyRange().ref(), &*stmt, GetCursor().mLocale));
    } else {
      IDB_TRY(DatabaseOperationBase::BindKeyRangeToStatement(
          GetOptionalKeyRange().ref(), &*stmt));
    }
  }

  // Now we need to make the query to get the next match.
  PrepareKeyConditionClauses(directionClause, std::move(queryStart));

  return ProcessStatementSteps(&*stmt);
}

template <IDBCursorType CursorType>
nsresult Cursor<CursorType>::OpenOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(!mCursor->mContinueQueries);

  AUTO_PROFILER_LABEL("Cursor::OpenOp::DoDatabaseWork", DOM);

  auto helper = OpenOpHelper<CursorType>{*this};
  const auto rv = helper.DoDatabaseWork(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

template <IDBCursorType CursorType>
nsresult Cursor<CursorType>::CursorOpBase::SendSuccessResult() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mCurrentlyRunningOp == this);
  MOZ_ASSERT(mResponse.type() != CursorResponse::T__None);

  if (IsActorDestroyed()) {
    return NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  mCursor->SendResponseInternal(mResponse, mFiles);

#ifdef DEBUG
  mResponseSent = true;
#endif
  return NS_OK;
}

template <IDBCursorType CursorType>
nsresult Cursor<CursorType>::ContinueOp::DoDatabaseWork(
    DatabaseConnection* aConnection) {
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(!mCursor->mContinueQueries->mContinueQuery.IsEmpty());
  MOZ_ASSERT(!mCursor->mContinueQueries->mContinueToQuery.IsEmpty());
  MOZ_ASSERT(!mCurrentPosition.mKey.IsUnset());

  if constexpr (IsIndexCursor) {
    MOZ_ASSERT_IF(
        mCursor->mDirection == IDBCursorDirection::Next ||
            mCursor->mDirection == IDBCursorDirection::Prev,
        !mCursor->mContinueQueries->mContinuePrimaryKeyQuery.IsEmpty());
    MOZ_ASSERT(mCursor->mIndexId);
    MOZ_ASSERT(!mCurrentPosition.mObjectStoreKey.IsUnset());
  }

  AUTO_PROFILER_LABEL("Cursor::ContinueOp::DoDatabaseWork", DOM);

  // We need to pick a query based on whether or not a key was passed to the
  // continue function. If not we'll grab the next item in the database that
  // is greater than (or less than, if we're running a PREV cursor) the current
  // key. If a key was passed we'll grab the next item in the database that is
  // greater than (or less than, if we're running a PREV cursor) or equal to the
  // key that was specified.
  //
  // TODO: The description above is not complete, it does not take account of
  // ContinuePrimaryKey nor Advance.
  //
  // Note: Changing the number or order of SELECT columns in the query will
  // require changes to CursorOpBase::PopulateResponseFromStatement.

  const uint32_t advanceCount =
      mParams.type() == CursorRequestParams::TAdvanceParams
          ? mParams.get_AdvanceParams().count()
          : 1;
  MOZ_ASSERT(advanceCount > 0);

  bool hasContinueKey = false;
  bool hasContinuePrimaryKey = false;

  auto explicitContinueKey = Key{};

  switch (mParams.type()) {
    case CursorRequestParams::TContinueParams:
      if (!mParams.get_ContinueParams().key().IsUnset()) {
        hasContinueKey = true;
        explicitContinueKey = mParams.get_ContinueParams().key();
      }
      break;
    case CursorRequestParams::TContinuePrimaryKeyParams:
      MOZ_ASSERT(!mParams.get_ContinuePrimaryKeyParams().key().IsUnset());
      MOZ_ASSERT(
          !mParams.get_ContinuePrimaryKeyParams().primaryKey().IsUnset());
      MOZ_ASSERT(mCursor->mDirection == IDBCursorDirection::Next ||
                 mCursor->mDirection == IDBCursorDirection::Prev);
      hasContinueKey = true;
      hasContinuePrimaryKey = true;
      explicitContinueKey = mParams.get_ContinuePrimaryKeyParams().key();
      break;
    case CursorRequestParams::TAdvanceParams:
      break;
    default:
      MOZ_CRASH("Should never get here!");
  }

  // TODO: Whether it makes sense to preload depends on the kind of the
  // subsequent operations, not of the current operation. We could assume that
  // the subsequent operations are:
  // - the same as the current operation (with the same parameter values)
  // - as above, except for Advance, where we assume the count will be 1 on the
  // next call
  // - basic operations (Advance with count 1 or Continue-without-key)
  //
  // For now, we implement the second option for now (which correspond to
  // !hasContinueKey).
  //
  // Based on that, we could in both cases either preload for any assumed
  // subsequent operations, or only for the basic operations. For now, we
  // preload only for an assumed basic operation. Other operations would require
  // more work on the client side for invalidation, and may not make any sense
  // at all.
  const uint32_t maxExtraCount = hasContinueKey ? 0 : mCursor->mMaxExtraCount;

  IDB_TRY_INSPECT(const auto& stmt,
                  aConnection->BorrowCachedStatement(
                      mCursor->mContinueQueries->GetContinueQuery(
                          hasContinueKey, hasContinuePrimaryKey)));

  IDB_TRY(stmt->BindUTF8StringByName(
      kStmtParamNameLimit,
      IntToCString(advanceCount + mCursor->mMaxExtraCount)));

  IDB_TRY(stmt->BindInt64ByName(kStmtParamNameId, mCursor->Id()));

  // Bind current key.
  const auto& continueKey =
      hasContinueKey ? explicitContinueKey
                     : mCurrentPosition.GetSortKey(mCursor->IsLocaleAware());
  IDB_TRY(continueKey.BindToStatement(&*stmt, kStmtParamNameCurrentKey));

  // Bind range bound if it is specified.
  if (!mCursor->mLocaleAwareRangeBound->IsUnset()) {
    IDB_TRY(mCursor->mLocaleAwareRangeBound->BindToStatement(
        &*stmt, kStmtParamNameRangeBound));
  }

  // Bind object store position if duplicates are allowed and we're not
  // continuing to a specific key.
  if constexpr (IsIndexCursor) {
    if (!hasContinueKey && (mCursor->mDirection == IDBCursorDirection::Next ||
                            mCursor->mDirection == IDBCursorDirection::Prev)) {
      IDB_TRY(mCurrentPosition.mObjectStoreKey.BindToStatement(
          &*stmt, kStmtParamNameObjectStorePosition));
    } else if (hasContinuePrimaryKey) {
      IDB_TRY(
          mParams.get_ContinuePrimaryKeyParams().primaryKey().BindToStatement(
              &*stmt, kStmtParamNameObjectStorePosition));
    }
  }

  // TODO: Why do we query the records we don't need and skip them here, rather
  // than using a OFFSET clause in the query?
  for (uint32_t index = 0; index < advanceCount; index++) {
    IDB_TRY_INSPECT(const bool& hasResult,
                    MOZ_TO_RESULT_INVOKE(&*stmt, ExecuteStep));

    if (!hasResult) {
      mResponse = void_t();
      return NS_OK;
    }
  }

  Key previousKey;
  auto* const optPreviousKey =
      IsUnique(mCursor->mDirection) ? &previousKey : nullptr;

  auto helper = CursorOpBaseHelperBase<CursorType>{*this};
  IDB_TRY_INSPECT(
      const auto& responseSize,
      helper.PopulateResponseFromStatement(&*stmt, true, optPreviousKey));

  helper.PopulateExtraResponses(&*stmt, maxExtraCount, responseSize,
                                "ContinueOp"_ns, optPreviousKey);

  return NS_OK;
}

Utils::Utils()
#ifdef DEBUG
    : mActorDestroyed(false)
#endif
{
  AssertIsOnBackgroundThread();
}

Utils::~Utils() { MOZ_ASSERT(mActorDestroyed); }

void Utils::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorDestroyed = true;
#endif
}

mozilla::ipc::IPCResult Utils::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundIndexedDBUtilsParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Utils::RecvGetFileReferences(
    const PersistenceType& aPersistenceType, const nsCString& aOrigin,
    const nsString& aDatabaseName, const int64_t& aFileId, int32_t* aRefCnt,
    int32_t* aDBRefCnt, bool* aResult) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aRefCnt);
  MOZ_ASSERT(aDBRefCnt);
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(!mActorDestroyed);

  if (NS_WARN_IF(!IndexedDatabaseManager::Get() || !QuotaManager::Get())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!IndexedDatabaseManager::InTestingMode())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!IsValidPersistenceType(aPersistenceType))) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aOrigin.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aDatabaseName.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aFileId == 0)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv =
      DispatchAndReturnFileReferences(aPersistenceType, aOrigin, aDatabaseName,
                                      aFileId, aRefCnt, aDBRefCnt, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

void PermissionRequestHelper::OnPromptComplete(
    PermissionValue aPermissionValue) {
  MOZ_ASSERT(NS_IsMainThread());

  mResolver(aPermissionValue);
}

#ifdef DEBUG

NS_IMPL_ISUPPORTS(DEBUGThreadSlower, nsIThreadObserver)

NS_IMETHODIMP
DEBUGThreadSlower::OnDispatchedEvent() { MOZ_CRASH("Should never be called!"); }

NS_IMETHODIMP
DEBUGThreadSlower::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                      bool /* aMayWait */) {
  return NS_OK;
}

NS_IMETHODIMP
DEBUGThreadSlower::AfterProcessNextEvent(nsIThreadInternal* /* aThread */,
                                         bool /* aEventWasProcessed */) {
  MOZ_ASSERT(kDEBUGThreadSleepMS);

  MOZ_ALWAYS_TRUE(PR_Sleep(PR_MillisecondsToInterval(kDEBUGThreadSleepMS)) ==
                  PR_SUCCESS);
  return NS_OK;
}

#endif  // DEBUG

nsresult FileHelper::Init() {
  MOZ_ASSERT(!IsOnBackgroundThread());

  auto fileDirectory = mFileManager->GetCheckedDirectory();
  if (NS_WARN_IF(!fileDirectory)) {
    return NS_ERROR_FAILURE;
  }

  auto journalDirectory = mFileManager->EnsureJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    return NS_ERROR_FAILURE;
  }

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(journalDirectory->Exists(&exists)));
  MOZ_ASSERT(exists);

  DebugOnly<bool> isDirectory;
  MOZ_ASSERT(NS_SUCCEEDED(journalDirectory->IsDirectory(&isDirectory)));
  MOZ_ASSERT(isDirectory);

  mFileDirectory.init(WrapNotNullUnchecked(std::move(fileDirectory)));
  mJournalDirectory.init(WrapNotNullUnchecked(std::move(journalDirectory)));

  return NS_OK;
}

nsCOMPtr<nsIFile> FileHelper::GetFile(const FileInfo& aFileInfo) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  return mFileManager->GetFileForId(mFileDirectory->get(), aFileInfo.Id());
}

nsCOMPtr<nsIFile> FileHelper::GetJournalFile(const FileInfo& aFileInfo) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  return mFileManager->GetFileForId(mJournalDirectory->get(), aFileInfo.Id());
}

nsresult FileHelper::CreateFileFromStream(nsIFile& aFile, nsIFile& aJournalFile,
                                          nsIInputStream& aInputStream,
                                          bool aCompress,
                                          const Maybe<CipherKey>& aMaybeKey) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  IDB_TRY_INSPECT(const auto& exists, MOZ_TO_RESULT_INVOKE(aFile, Exists));

  // DOM blobs that are being stored in IDB are cached by calling
  // IDBDatabase::GetOrCreateFileActorForBlob. So if the same DOM blob is stored
  // again under a different key or in a different object store, we just add
  // a new reference instead of creating a new copy (all such stored blobs share
  // the same id).
  // However, it can happen that CreateFileFromStream failed due to quota
  // exceeded error and for some reason the orphaned file couldn't be deleted
  // immediately. Now, if the operation is being repeated, the DOM blob is
  // already cached, so it has the same file id which clashes with the orphaned
  // file. We could do some tricks to restore previous copy loop, but it's safer
  // to just delete the orphaned file and start from scratch.
  // This corner case is partially simulated in test_file_copy_failure.js
  if (exists) {
    IDB_TRY_INSPECT(const auto& isFile, MOZ_TO_RESULT_INVOKE(aFile, IsFile));

    IDB_TRY(OkIf(isFile), NS_ERROR_FAILURE);

    IDB_TRY_INSPECT(const auto& journalExists,
                    MOZ_TO_RESULT_INVOKE(aJournalFile, Exists));

    IDB_TRY(OkIf(journalExists), NS_ERROR_FAILURE);

    IDB_TRY_INSPECT(const auto& journalIsFile,
                    MOZ_TO_RESULT_INVOKE(aJournalFile, IsFile));

    IDB_TRY(OkIf(journalIsFile), NS_ERROR_FAILURE);

    IDB_WARNING("Deleting orphaned file!");

    IDB_TRY(mFileManager->SyncDeleteFile(aFile, aJournalFile));
  }

  // Create a journal file first.
  IDB_TRY(aJournalFile.Create(nsIFile::NORMAL_FILE_TYPE, 0644));

  // Now try to copy the stream.
  IDB_TRY_UNWRAP(auto fileOutputStream,
                 CreateFileOutputStream(mFileManager->Type(),
                                        mFileManager->OriginMetadata(),
                                        Client::IDB, &aFile)
                     .map([](NotNull<RefPtr<FileOutputStream>>&& stream) {
                       return nsCOMPtr<nsIOutputStream>{stream.get()};
                     }));

  AutoTArray<char, kFileCopyBufferSize> buffer;
  const auto actualOutputStream =
      [aCompress, &aMaybeKey, &buffer,
       baseOutputStream =
           std::move(fileOutputStream)]() mutable -> nsCOMPtr<nsIOutputStream> {
    if (aMaybeKey) {
      baseOutputStream =
          MakeRefPtr<EncryptingOutputStream<IndexedDBCipherStrategy>>(
              std::move(baseOutputStream), kEncryptedStreamBlockSize,
              *aMaybeKey);
    }

    if (aCompress) {
      auto snappyOutputStream =
          MakeRefPtr<SnappyCompressOutputStream>(baseOutputStream);

      buffer.SetLength(snappyOutputStream->BlockSize());

      return snappyOutputStream;
    }

    buffer.SetLength(kFileCopyBufferSize);
    return std::move(baseOutputStream);
  }();

  IDB_TRY(SyncCopy(aInputStream, *actualOutputStream, buffer.Elements(),
                   buffer.Length()));

  return NS_OK;
}

class FileHelper::ReadCallback final : public nsIInputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ReadCallback()
      : mMutex("ReadCallback::mMutex"),
        mCondVar(mMutex, "ReadCallback::mCondVar"),
        mInputAvailable(false) {}

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aStream) override {
    mozilla::MutexAutoLock autolock(mMutex);

    mInputAvailable = true;
    mCondVar.Notify();

    return NS_OK;
  }

  nsresult AsyncWait(nsIAsyncInputStream* aStream, uint32_t aBufferSize,
                     nsIEventTarget* aTarget) {
    MOZ_ASSERT(aStream);
    mozilla::MutexAutoLock autolock(mMutex);

    nsresult rv = aStream->AsyncWait(this, 0, aBufferSize, aTarget);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mInputAvailable = false;
    while (!mInputAvailable) {
      mCondVar.Wait();
    }

    return NS_OK;
  }

 private:
  ~ReadCallback() = default;

  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  bool mInputAvailable;
};

NS_IMPL_ADDREF(FileHelper::ReadCallback);
NS_IMPL_RELEASE(FileHelper::ReadCallback);

NS_INTERFACE_MAP_BEGIN(FileHelper::ReadCallback)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStreamCallback)
NS_INTERFACE_MAP_END

nsresult FileHelper::SyncRead(nsIInputStream& aInputStream, char* const aBuffer,
                              const uint32_t aBufferSize,
                              uint32_t* const aRead) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  // Let's try to read, directly.
  nsresult rv = aInputStream.Read(aBuffer, aBufferSize, aRead);
  if (NS_SUCCEEDED(rv) || rv != NS_BASE_STREAM_WOULD_BLOCK) {
    return rv;
  }

  // We need to proceed async.
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(&aInputStream);
  if (!asyncStream) {
    return rv;
  }

  if (!mReadCallback) {
    mReadCallback.init(MakeNotNull<RefPtr<ReadCallback>>());
  }

  // We just need any thread with an event loop for receiving the
  // OnInputStreamReady callback. Let's use the I/O thread.
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  rv = (*mReadCallback)->AsyncWait(asyncStream, aBufferSize, target);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SyncRead(aInputStream, aBuffer, aBufferSize, aRead);
}

nsresult FileHelper::SyncCopy(nsIInputStream& aInputStream,
                              nsIOutputStream& aOutputStream,
                              char* const aBuffer, const uint32_t aBufferSize) {
  MOZ_ASSERT(!IsOnBackgroundThread());

  AUTO_PROFILER_LABEL("FileHelper::SyncCopy", DOM);

  nsresult rv;

  do {
    uint32_t numRead;
    rv = SyncRead(aInputStream, aBuffer, aBufferSize, &numRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (!numRead) {
      break;
    }

    uint32_t numWrite;
    rv = aOutputStream.Write(aBuffer, numRead, &numWrite);
    if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      rv = NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (NS_WARN_IF(numWrite != numRead)) {
      rv = NS_ERROR_FAILURE;
      break;
    }
  } while (true);

  if (NS_SUCCEEDED(rv)) {
    rv = aOutputStream.Flush();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv2 = aOutputStream.Close();
  if (NS_WARN_IF(NS_FAILED(rv2))) {
    return NS_SUCCEEDED(rv) ? rv2 : rv;
  }

  return rv;
}

}  // namespace dom::indexedDB
}  // namespace mozilla

#undef IDB_MOBILE
#undef IDB_DEBUG_LOG
#undef ASSERT_UNLESS_FUZZING
#undef DISABLE_ASSERTS_FOR_FUZZING
