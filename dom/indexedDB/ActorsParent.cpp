/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include <algorithm>
#include "FileInfo.h"
#include "FileManager.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IndexedDatabase.h"
#include "IndexedDatabaseInlines.h"
#include "IndexedDatabaseManager.h"
#include "js/StructuredClone.h"
#include "js/Value.h"
#include "jsapi.h"
#include "KeyPath.h"
#include "mozilla/Attributes.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Endian.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/storage.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBCursorParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseFileParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryRequestParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBRequestParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBTransactionParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBVersionChangeTransactionParent.h"
#include "mozilla/dom/indexedDB/PIndexedDBPermissionRequestParent.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PBackground.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsEscape.h"
#include "nsHashKeys.h"
#include "nsNetUtil.h"
#include "nsIAppsService.h"
#include "nsIEventTarget.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsInterfaceHashtable.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOfflineStorage.h"
#include "nsIOutputStream.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsISupportsPriority.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCID.h"
#include "PermissionRequestBase.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"
#include "snappy/snappy.h"
#include "TransactionThreadPool.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

namespace {

class Cursor;
class Database;
struct DatabaseActorInfo;
class DatabaseLoggingInfo;
class DatabaseFile;
class DatabaseOfflineStorage;
class Factory;
class OpenDatabaseOp;
class TransactionBase;
class VersionChangeTransaction;

/*******************************************************************************
 * Constants
 ******************************************************************************/

// If JS_STRUCTURED_CLONE_VERSION changes then we need to update our major
// schema version.
static_assert(JS_STRUCTURED_CLONE_VERSION == 5,
              "Need to update the major schema version.");

// Major schema version. Bump for almost everything.
const uint32_t kMajorSchemaVersion = 17;

// Minor schema version. Should almost always be 0 (maybe bump on release
// branches if we have to).
const uint32_t kMinorSchemaVersion = 0;

// The schema version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 4 bits so the max value is
// 0xFFFFFFF. The minor version occupies the lower 4 bits and its max is 0xF.
static_assert(kMajorSchemaVersion <= 0xFFFFFFF,
              "Major version needs to fit in 28 bits.");
static_assert(kMinorSchemaVersion <= 0xF,
              "Minor version needs to fit in 4 bits.");

const int32_t kSQLiteSchemaVersion =
  int32_t((kMajorSchemaVersion << 4) + kMinorSchemaVersion);

const int32_t kStorageProgressGranularity = 1000;

const char kSavepointClause[] = "SAVEPOINT sp;";

const uint32_t kFileCopyBufferSize = 32768;

const char kJournalDirectoryName[] = "journals";

const char kFileManagerDirectoryNameSuffix[] = ".files";

const char kPrefIndexedDBEnabled[] = "dom.indexedDB.enabled";

#define IDB_PREFIX "indexedDB"

#ifdef MOZ_CHILD_PERMISSIONS
const char kPermissionString[] = IDB_PREFIX;
#endif // MOZ_CHILD_PERMISSIONS

const char kPermissionStringChromeBase[] = IDB_PREFIX "-chrome-";
const char kPermissionStringChromeReadSuffix[] = "-read";
const char kPermissionStringChromeWriteSuffix[] = "-write";

#undef IDB_PREFIX

enum AppId {
  kNoAppId = nsIScriptSecurityManager::NO_APP_ID,
  kUnknownAppId = nsIScriptSecurityManager::UNKNOWN_APP_ID
};

#ifdef DEBUG

const int32_t kDEBUGThreadPriority = nsISupportsPriority::PRIORITY_NORMAL;
const uint32_t kDEBUGThreadSleepMS = 0;

#endif

/*******************************************************************************
 * Metadata classes
 ******************************************************************************/

struct FullIndexMetadata
{
  IndexMetadata mCommonMetadata;

  bool mDeleted;

public:
  FullIndexMetadata()
    : mCommonMetadata(0, nsString(), KeyPath(0), false, false)
    , mDeleted(false)
  {
    // This can happen either on the QuotaManager IO thread or on a
    // versionchange transaction thread. These threads can never race so this is
    // totally safe.
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FullIndexMetadata)

private:
  ~FullIndexMetadata()
  { }
};

typedef nsRefPtrHashtable<nsUint64HashKey, FullIndexMetadata> IndexTable;

struct FullObjectStoreMetadata
{
  ObjectStoreMetadata mCommonMetadata;
  IndexTable mIndexes;

  // These two members are only ever touched on a transaction thread!
  int64_t mNextAutoIncrementId;
  int64_t mComittedAutoIncrementId;

  bool mDeleted;

public:
  FullObjectStoreMetadata()
    : mCommonMetadata(0, nsString(), KeyPath(0), false)
    , mNextAutoIncrementId(0)
    , mComittedAutoIncrementId(0)
    , mDeleted(false)
  {
    // This can happen either on the QuotaManager IO thread or on a
    // versionchange transaction thread. These threads can never race so this is
    // totally safe.
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FullObjectStoreMetadata);

private:
  ~FullObjectStoreMetadata()
  { }
};

typedef nsRefPtrHashtable<nsUint64HashKey, FullObjectStoreMetadata>
  ObjectStoreTable;

struct FullDatabaseMetadata
{
  DatabaseMetadata mCommonMetadata;
  nsCString mDatabaseId;
  nsString mFilePath;
  ObjectStoreTable mObjectStores;

  int64_t mNextObjectStoreId;
  int64_t mNextIndexId;

public:
  explicit FullDatabaseMetadata(const DatabaseMetadata& aCommonMetadata)
    : mCommonMetadata(aCommonMetadata)
    , mNextObjectStoreId(0)
    , mNextIndexId(0)
  {
    AssertIsOnBackgroundThread();
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FullDatabaseMetadata)

  already_AddRefed<FullDatabaseMetadata>
  Duplicate() const;

private:
  ~FullDatabaseMetadata()
  { }
};

template <class MetadataType>
class MOZ_STACK_CLASS MetadataNameOrIdMatcher MOZ_FINAL
{
  typedef MetadataNameOrIdMatcher<MetadataType> SelfType;

  const int64_t mId;
  const nsString mName;
  nsRefPtr<MetadataType> mMetadata;
  bool mCheckName;

public:
  template <class Enumerable>
  static MetadataType*
  Match(const Enumerable& aEnumerable, uint64_t aId, const nsAString& aName)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aId);

    SelfType closure(aId, aName);
    aEnumerable.EnumerateRead(Enumerate, &closure);

    return closure.mMetadata;
  }

  template <class Enumerable>
  static MetadataType*
  Match(const Enumerable& aEnumerable, uint64_t aId)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aId);

    SelfType closure(aId);
    aEnumerable.EnumerateRead(Enumerate, &closure);

    return closure.mMetadata;
  }

private:
  MetadataNameOrIdMatcher(const int64_t& aId, const nsAString& aName)
    : mId(aId)
    , mName(PromiseFlatString(aName))
    , mMetadata(nullptr)
    , mCheckName(true)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aId);
  }

  explicit MetadataNameOrIdMatcher(const int64_t& aId)
    : mId(aId)
    , mMetadata(nullptr)
    , mCheckName(false)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aId);
  }

  static PLDHashOperator
  Enumerate(const uint64_t& aKey, MetadataType* aValue, void* aClosure)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aKey);
    MOZ_ASSERT(aValue);
    MOZ_ASSERT(aClosure);

    auto* closure = static_cast<SelfType*>(aClosure);

    if (!aValue->mDeleted &&
        (closure->mId == aValue->mCommonMetadata.id() ||
         (closure->mCheckName &&
          closure->mName == aValue->mCommonMetadata.name()))) {
      closure->mMetadata = aValue;
      return PL_DHASH_STOP;
    }

    return PL_DHASH_NEXT;
  }
};

/*******************************************************************************
 * SQLite functions
 ******************************************************************************/

int32_t
MakeSchemaVersion(uint32_t aMajorSchemaVersion,
                  uint32_t aMinorSchemaVersion)
{
  return int32_t((aMajorSchemaVersion << 4) + aMinorSchemaVersion);
}

uint32_t
HashName(const nsAString& aName)
{
  struct Helper
  {
    static uint32_t
    RotateBitsLeft32(uint32_t aValue, uint8_t aBits)
    {
      MOZ_ASSERT(aBits < 32);
      return (aValue << aBits) | (aValue >> (32 - aBits));
    }
  };

  static const uint32_t kGoldenRatioU32 = 0x9e3779b9u;

  const char16_t* str = aName.BeginReading();
  size_t length = aName.Length();

  uint32_t hash = 0;
  for (size_t i = 0; i < length; i++) {
    hash = kGoldenRatioU32 * (Helper::RotateBitsLeft32(hash, 5) ^ str[i]);
  }

  return hash;
}

nsresult
ClampResultCode(nsresult aResultCode)
{
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
      nsPrintfCString message("Converting non-IndexedDB error code (0x%X) to "
                              "NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR",
                              aResultCode);
      NS_WARNING(message.get());
#else
      ;
#endif
  }

  IDB_REPORT_INTERNAL_ERR();
  return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
}

void
GetDatabaseFilename(const nsAString& aName,
                    nsAutoString& aDatabaseFilename)
{
  MOZ_ASSERT(aDatabaseFilename.IsEmpty());

  aDatabaseFilename.AppendInt(HashName(aName));

  nsCString escapedName;
  if (!NS_Escape(NS_ConvertUTF16toUTF8(aName), escapedName, url_XPAlphas)) {
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

  aDatabaseFilename.AppendASCII(substring.get(), substring.Length());
}

nsresult
CreateFileTables(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "CreateFileTables",
                 js::ProfileEntry::Category::STORAGE);

  // Table `file`
  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE file ("
      "id INTEGER PRIMARY KEY, "
      "refcount INTEGER NOT NULL"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_insert_trigger "
    "AFTER INSERT ON object_data "
    "FOR EACH ROW "
    "WHEN NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
    "END;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_update_trigger "
    "AFTER UPDATE OF file_ids ON object_data "
    "FOR EACH ROW "
    "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
    "END;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_delete_trigger "
    "AFTER DELETE ON object_data "
    "FOR EACH ROW WHEN OLD.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NULL); "
    "END;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER file_update_trigger "
    "AFTER UPDATE ON file "
    "FOR EACH ROW WHEN NEW.refcount = 0 "
    "BEGIN "
      "DELETE FROM file WHERE id = OLD.id; "
    "END;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
CreateTables(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "CreateTables",
                 js::ProfileEntry::Category::STORAGE);

  // Table `database`
  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE database ("
      "name TEXT NOT NULL, "
      "version INTEGER NOT NULL DEFAULT 0"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `object_store`
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store ("
      "id INTEGER PRIMARY KEY, "
      "auto_increment INTEGER NOT NULL DEFAULT 0, "
      "name TEXT NOT NULL, "
      "key_path TEXT, "
      "UNIQUE (name)"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `object_data`
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_data ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "key_value BLOB DEFAULT NULL, "
      "file_ids TEXT, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, key_value), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `index`
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store_index ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "key_path TEXT NOT NULL, "
      "unique_index INTEGER NOT NULL, "
      "multientry INTEGER NOT NULL, "
      "UNIQUE (object_store_id, name), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `index_data`
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE index_data ("
      "index_id INTEGER NOT NULL, "
      "value BLOB NOT NULL, "
      "object_data_key BLOB NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE, "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Need this to make cascading deletes from object_data and object_store fast.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX index_data_object_data_id_index "
    "ON index_data (object_data_id);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `unique_index_data`
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE unique_index_data ("
      "index_id INTEGER NOT NULL, "
      "value BLOB NOT NULL, "
      "object_data_key BLOB NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "UNIQUE (index_id, value), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Need this to make cascading deletes from object_data and object_store fast.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX unique_index_data_object_data_id_index "
    "ON unique_index_data (object_data_id);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateFileTables(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(kSQLiteSchemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom4To5(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom4To5",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv;

  // All we changed is the type of the version column, so lets try to
  // convert that to an integer, and if we fail, set it to 0.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, version, dataVersion "
    "FROM database"
  ), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString name;
  int32_t intVersion;
  int64_t dataVersion;

  {
    mozStorageStatementScoper scoper(stmt);

    bool hasResults;
    rv = stmt->ExecuteStep(&hasResults);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (NS_WARN_IF(!hasResults)) {
      return NS_ERROR_FAILURE;
    }

    nsString version;
    rv = stmt->GetString(1, version);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    intVersion = version.ToInteger(&rv);
    if (NS_FAILED(rv)) {
      intVersion = 0;
    }

    rv = stmt->GetString(0, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->GetInt64(2, &dataVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE database"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE database ("
      "name TEXT NOT NULL, "
      "version INTEGER NOT NULL DEFAULT 0, "
      "dataVersion INTEGER NOT NULL"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO database (name, version, dataVersion) "
    "VALUES (:name, :version, :dataVersion)"
  ), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  {
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindStringParameter(0, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt32Parameter(1, intVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64Parameter(2, dataVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aConnection->SetSchemaVersion(5);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom5To6(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom5To6",
                 js::ProfileEntry::Category::STORAGE);

  // First, drop all the indexes we're no longer going to use.
  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX key_index;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX ai_key_index;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX value_index;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX ai_value_index;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now, reorder the columns of object_data to put the blob data last. We do
  // this by copying into a temporary table, dropping the original, then copying
  // back into a newly created table.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id, "
      "key_value, "
      "data "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, key_value, data "
      "FROM object_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_data ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "key_value DEFAULT NULL, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, key_value), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_data "
      "SELECT id, object_store_id, key_value, data "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We need to add a unique constraint to our ai_object_data table. Copy all
  // the data out of it using a temporary table as before.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id, "
      "data "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, data "
      "FROM ai_object_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_object_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE ai_object_data ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "object_store_id INTEGER NOT NULL, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO ai_object_data "
      "SELECT id, object_store_id, data "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "object_data_key NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE, "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX index_data_object_data_id_index "
    "ON index_data (object_data_id);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the unique_index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM unique_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE unique_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE unique_index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "object_data_key NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "UNIQUE (index_id, value), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO unique_index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX unique_index_data_object_data_id_index "
    "ON unique_index_data (object_data_id);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the ai_index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "ai_object_data_id "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, value, ai_object_data_id "
      "FROM ai_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE ai_index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "ai_object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, ai_object_data_id), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE, "
      "FOREIGN KEY (ai_object_data_id) REFERENCES ai_object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO ai_index_data "
      "SELECT index_id, value, ai_object_data_id "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX ai_index_data_ai_object_data_id_index "
    "ON ai_index_data (ai_object_data_id);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the ai_unique_index_data table. We're reordering the columns as well
  // as changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "ai_object_data_id "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, value, ai_object_data_id "
      "FROM ai_unique_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_unique_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE ai_unique_index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "ai_object_data_id INTEGER NOT NULL, "
      "UNIQUE (index_id, value), "
      "PRIMARY KEY (index_id, value, ai_object_data_id), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE, "
      "FOREIGN KEY (ai_object_data_id) REFERENCES ai_object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO ai_unique_index_data "
      "SELECT index_id, value, ai_object_data_id "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX ai_unique_index_data_ai_object_data_id_index "
    "ON ai_unique_index_data (ai_object_data_id);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(6);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom6To7(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom6To7",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id, "
      "name, "
      "key_path, "
      "auto_increment"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, name, key_path, auto_increment "
      "FROM object_store;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_store;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store ("
      "id INTEGER PRIMARY KEY, "
      "auto_increment INTEGER NOT NULL DEFAULT 0, "
      "name TEXT NOT NULL, "
      "key_path TEXT, "
      "UNIQUE (name)"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_store "
      "SELECT id, auto_increment, name, nullif(key_path, '') "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(7);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom7To8(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom7To8",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id, "
      "object_store_id, "
      "name, "
      "key_path, "
      "unique_index, "
      "object_store_autoincrement"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, object_store_autoincrement "
      "FROM object_store_index;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_store_index;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store_index ("
      "id INTEGER, "
      "object_store_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "key_path TEXT NOT NULL, "
      "unique_index INTEGER NOT NULL, "
      "multientry INTEGER NOT NULL, "
      "object_store_autoincrement INTERGER NOT NULL, "
      "PRIMARY KEY (id), "
      "UNIQUE (object_store_id, name), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_store_index "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, 0, object_store_autoincrement "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(8);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class CompressDataBlobsFunction MOZ_FINAL
  : public mozIStorageFunction
{
public:
  NS_DECL_ISUPPORTS

private:
  ~CompressDataBlobsFunction()
  { }

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aArguments);
    MOZ_ASSERT(aResult);

    PROFILER_LABEL("IndexedDB",
                   "CompressDataBlobsFunction::OnFunctionCall",
                   js::ProfileEntry::Category::STORAGE);

    uint32_t argc;
    nsresult rv = aArguments->GetNumEntries(&argc);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (argc != 1) {
      NS_WARNING("Don't call me with the wrong number of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    int32_t type;
    rv = aArguments->GetTypeOfIndex(0, &type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (type != mozIStorageStatement::VALUE_TYPE_BLOB) {
      NS_WARNING("Don't call me with the wrong type of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    const uint8_t* uncompressed;
    uint32_t uncompressedLength;
    rv = aArguments->GetSharedBlob(0, &uncompressedLength, &uncompressed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);
    nsAutoArrayPtr<char> compressed(new (fallible) char[compressedLength]);
    if (NS_WARN_IF(!compressed)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    snappy::RawCompress(reinterpret_cast<const char*>(uncompressed),
                        uncompressedLength, compressed.get(),
                        &compressedLength);

    std::pair<const void *, int> data(static_cast<void*>(compressed.get()),
                                      int(compressedLength));

    // XXX This copies the buffer again... There doesn't appear to be any way to
    //     preallocate space and write directly to a BlobVariant at the moment.
    nsCOMPtr<nsIVariant> result = new mozilla::storage::BlobVariant(data);

    result.forget(aResult);
    return NS_OK;
  }
};

nsresult
UpgradeSchemaFrom8To9_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom8To9_0",
                 js::ProfileEntry::Category::STORAGE);

  // We no longer use the dataVersion column.
  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE database SET dataVersion = 0;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageFunction> compressor = new CompressDataBlobsFunction();

  NS_NAMED_LITERAL_CSTRING(compressorName, "compress");

  rv = aConnection->CreateFunction(compressorName, 1, compressor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Turn off foreign key constraints before we do anything here.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE object_data SET data = compress(data);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE ai_object_data SET data = compress(data);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->RemoveFunction(compressorName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(9, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom9_0To10_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom9_0To10_0",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE object_data ADD COLUMN file_ids TEXT;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE ai_object_data ADD COLUMN file_ids TEXT;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateFileTables(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(10, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom10_0To11_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom10_0To11_0",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id, "
      "object_store_id, "
      "name, "
      "key_path, "
      "unique_index, "
      "multientry"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, multientry "
      "FROM object_store_index;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_store_index;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store_index ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "key_path TEXT NOT NULL, "
      "unique_index INTEGER NOT NULL, "
      "multientry INTEGER NOT NULL, "
      "UNIQUE (object_store_id, name), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_store_index "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, multientry "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TRIGGER object_data_insert_trigger;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_data (object_store_id, key_value, data, file_ids) "
      "SELECT object_store_id, id, data, file_ids "
      "FROM ai_object_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_insert_trigger "
    "AFTER INSERT ON object_data "
    "FOR EACH ROW "
    "WHEN NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
    "END;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO index_data (index_id, value, object_data_key, object_data_id) "
      "SELECT ai_index_data.index_id, ai_index_data.value, ai_index_data.ai_object_data_id, object_data.id "
      "FROM ai_index_data "
      "INNER JOIN object_store_index ON "
        "object_store_index.id = ai_index_data.index_id "
      "INNER JOIN object_data ON "
        "object_data.object_store_id = object_store_index.object_store_id AND "
        "object_data.key_value = ai_index_data.ai_object_data_id;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO unique_index_data (index_id, value, object_data_key, object_data_id) "
      "SELECT ai_unique_index_data.index_id, ai_unique_index_data.value, ai_unique_index_data.ai_object_data_id, object_data.id "
      "FROM ai_unique_index_data "
      "INNER JOIN object_store_index ON "
        "object_store_index.id = ai_unique_index_data.index_id "
      "INNER JOIN object_data ON "
        "object_data.object_store_id = object_store_index.object_store_id AND "
        "object_data.key_value = ai_unique_index_data.ai_object_data_id;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE object_store "
      "SET auto_increment = (SELECT max(id) FROM ai_object_data) + 1 "
      "WHERE auto_increment;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_unique_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_object_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(11, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class EncodeKeysFunction MOZ_FINAL
  : public mozIStorageFunction
{
public:
  NS_DECL_ISUPPORTS

private:
  ~EncodeKeysFunction()
  { }

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aArguments);
    MOZ_ASSERT(aResult);

    PROFILER_LABEL("IndexedDB",
                   "EncodeKeysFunction::OnFunctionCall",
                   js::ProfileEntry::Category::STORAGE);

    uint32_t argc;
    nsresult rv = aArguments->GetNumEntries(&argc);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (argc != 1) {
      NS_WARNING("Don't call me with the wrong number of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    int32_t type;
    rv = aArguments->GetTypeOfIndex(0, &type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    Key key;
    if (type == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      int64_t intKey;
      aArguments->GetInt64(0, &intKey);
      key.SetFromInteger(intKey);
    } else if (type == mozIStorageStatement::VALUE_TYPE_TEXT) {
      nsString stringKey;
      aArguments->GetString(0, stringKey);
      key.SetFromString(stringKey);
    } else {
      NS_WARNING("Don't call me with the wrong type of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    const nsCString& buffer = key.GetBuffer();

    std::pair<const void *, int> data(static_cast<const void*>(buffer.get()),
                                      int(buffer.Length()));

    nsCOMPtr<nsIVariant> result = new mozilla::storage::BlobVariant(data);

    result.forget(aResult);
    return NS_OK;
  }
};

nsresult
UpgradeSchemaFrom11_0To12_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom11_0To12_0",
                 js::ProfileEntry::Category::STORAGE);

  NS_NAMED_LITERAL_CSTRING(encoderName, "encode");

  nsCOMPtr<mozIStorageFunction> encoder = new EncodeKeysFunction();

  nsresult rv = aConnection->CreateFunction(encoderName, 1, encoder);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id, "
      "key_value, "
      "data, "
      "file_ids "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, encode(key_value), data, file_ids "
      "FROM object_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_data ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "key_value BLOB DEFAULT NULL, "
      "file_ids TEXT, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, key_value), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_data "
      "SELECT id, object_store_id, key_value, file_ids, data "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_insert_trigger "
    "AFTER INSERT ON object_data "
    "FOR EACH ROW "
    "WHEN NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
    "END;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_update_trigger "
    "AFTER UPDATE OF file_ids ON object_data "
    "FOR EACH ROW "
    "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
    "END;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_delete_trigger "
    "AFTER DELETE ON object_data "
    "FOR EACH ROW WHEN OLD.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NULL); "
    "END;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, encode(value), encode(object_data_key), object_data_id "
      "FROM index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE index_data ("
      "index_id INTEGER NOT NULL, "
      "value BLOB NOT NULL, "
      "object_data_key BLOB NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE, "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX index_data_object_data_id_index "
    "ON index_data (object_data_id);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, encode(value), encode(object_data_key), object_data_id "
      "FROM unique_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE unique_index_data;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE unique_index_data ("
      "index_id INTEGER NOT NULL, "
      "value BLOB NOT NULL, "
      "object_data_key BLOB NOT NULL, "
      "object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, object_data_key), "
      "UNIQUE (index_id, value), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
        "CASCADE "
      "FOREIGN KEY (object_data_id) REFERENCES object_data(id) ON DELETE "
        "CASCADE"
    ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO unique_index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX unique_index_data_object_data_id_index "
    "ON unique_index_data (object_data_id);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->RemoveFunction(encoderName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(12, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom12_0To13_0(mozIStorageConnection* aConnection,
                            bool* aVacuumNeeded)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "UpgradeSchemaFrom12_0To13_0",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv;

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
  int32_t defaultPageSize;
  rv = aConnection->GetDefaultPageSize(&defaultPageSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Enable auto_vacuum mode and update the page size to the platform default.
  nsAutoCString upgradeQuery("PRAGMA auto_vacuum = FULL; PRAGMA page_size = ");
  upgradeQuery.AppendInt(defaultPageSize);

  rv = aConnection->ExecuteSimpleSQL(upgradeQuery);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aVacuumNeeded = true;
#endif

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(13, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom13_0To14_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The only change between 13 and 14 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(14, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom14_0To15_0(mozIStorageConnection* aConnection)
{
  // The only change between 14 and 15 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(15, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom15_0To16_0(mozIStorageConnection* aConnection)
{
  // The only change between 15 and 16 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(16, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeSchemaFrom16_0To17_0(mozIStorageConnection* aConnection)
{
  // The only change between 16 and 17 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(17, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
GetDatabaseFileURL(nsIFile* aDatabaseFile,
                   PersistenceType aPersistenceType,
                   const nsACString& aGroup,
                   const nsACString& aOrigin,
                   nsIFileURL** aResult)
{
  MOZ_ASSERT(aDatabaseFile);
  MOZ_ASSERT(aResult);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewFileURI(getter_AddRefs(uri), aDatabaseFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(uri);
  MOZ_ASSERT(fileUrl);

  nsAutoCString type;
  PersistenceTypeToText(aPersistenceType, type);

  rv = fileUrl->SetQuery(NS_LITERAL_CSTRING("persistenceType=") + type +
                         NS_LITERAL_CSTRING("&group=") + aGroup +
                         NS_LITERAL_CSTRING("&origin=") + aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  fileUrl.forget(aResult);
  return NS_OK;
}

nsresult
SetDefaultPragmas(mozIStorageConnection* aConnection)
{
  MOZ_ASSERT(aConnection);

  static const char query[] =
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
    // Switch the journaling mode to TRUNCATE to avoid changing the directory
    // structure at the conclusion of every transaction for devices with slower
    // file systems.
    "PRAGMA journal_mode = TRUNCATE; "
#endif
    // We use foreign keys in lots of places.
    "PRAGMA foreign_keys = ON; "
    // The "INSERT OR REPLACE" statement doesn't fire the update trigger,
    // instead it fires only the insert trigger. This confuses the update
    // refcount function. This behavior changes with enabled recursive triggers,
    // so the statement fires the delete trigger first and then the insert
    // trigger.
    "PRAGMA recursive_triggers = ON;"
    // We don't need SQLite's table locks because we manage transaction ordering
    // ourselves and we know we will never allow a write transaction to modify
    // an object store that a read transaction is in the process of using.
    "PRAGMA read_uncommitted = TRUE;"
    // No more PRAGMAs.
    ;

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(query));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (IndexedDatabaseManager::FullSynchronous()) {
    rv = aConnection->ExecuteSimpleSQL(
                             NS_LITERAL_CSTRING("PRAGMA synchronous = FULL;"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
CreateDatabaseConnection(nsIFile* aDBFile,
                         nsIFile* aFMDirectory,
                         const nsAString& aName,
                         PersistenceType aPersistenceType,
                         const nsACString& aGroup,
                         const nsACString& aOrigin,
                         mozIStorageConnection** aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDBFile);
  MOZ_ASSERT(aFMDirectory);
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "CreateDatabaseConnection",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv;
  bool exists;

  if (IndexedDatabaseManager::InLowDiskSpaceMode()) {
    rv = aDBFile->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!exists) {
      NS_WARNING("Refusing to create database because disk space is low!");
      return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    }
  }

  nsCOMPtr<nsIFileURL> dbFileUrl;
  rv = GetDatabaseFileURL(aDBFile, aPersistenceType, aGroup, aOrigin,
                          getter_AddRefs(dbFileUrl));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // If we're just opening the database during origin initialization, then
    // we don't want to erase any files. The failure here will fail origin
    // initialization too.
    if (aName.IsVoid()) {
      return rv;
    }

    // Nuke the database file.
    rv = aDBFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aFMDirectory->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (exists) {
      bool isDirectory;
      rv = aFMDirectory->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (NS_WARN_IF(!isDirectory)) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      rv = aFMDirectory->Remove(true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(connection));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetDefaultPragmas(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = connection->EnableModule(NS_LITERAL_CSTRING("filesystem"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Check to make sure that the database schema is correct.
  int32_t schemaVersion;
  rv = connection->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Unknown schema will fail origin initialization too.
  if (!schemaVersion && aName.IsVoid()) {
    IDB_WARNING("Unable to open IndexedDB database, schema is not set!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (schemaVersion > kSQLiteSchemaVersion) {
    IDB_WARNING("Unable to open IndexedDB database, schema is too high!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  bool vacuumNeeded = false;

  if (schemaVersion != kSQLiteSchemaVersion) {
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
    if (!schemaVersion) {
      // Have to do this before opening a transaction.
      rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        // Turn on auto_vacuum mode to reclaim disk space on mobile devices.
        "PRAGMA auto_vacuum = FULL; "
      ));
      if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
        // mozstorage translates SQLITE_FULL to NS_ERROR_FILE_NO_DEVICE_SPACE,
        // which we know better as NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR.
        rv = NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
#endif

    mozStorageTransaction transaction(connection, false,
                                  mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (!schemaVersion) {
      // Brand new file, initialize our tables.
      rv = CreateTables(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)));
      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);

      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = connection->CreateStatement(NS_LITERAL_CSTRING(
        "INSERT INTO database (name) "
        "VALUES (:name)"
      ), getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), aName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else  {
      // This logic needs to change next time we change the schema!
      static_assert(kSQLiteSchemaVersion == int32_t((17 << 4) + 0),
                    "Upgrade function needed due to schema version increase.");

      while (schemaVersion != kSQLiteSchemaVersion) {
        if (schemaVersion == 4) {
          rv = UpgradeSchemaFrom4To5(connection);
        } else if (schemaVersion == 5) {
          rv = UpgradeSchemaFrom5To6(connection);
        } else if (schemaVersion == 6) {
          rv = UpgradeSchemaFrom6To7(connection);
        } else if (schemaVersion == 7) {
          rv = UpgradeSchemaFrom7To8(connection);
        } else if (schemaVersion == 8) {
          rv = UpgradeSchemaFrom8To9_0(connection);
          vacuumNeeded = true;
        } else if (schemaVersion == MakeSchemaVersion(9, 0)) {
          rv = UpgradeSchemaFrom9_0To10_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(10, 0)) {
          rv = UpgradeSchemaFrom10_0To11_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(11, 0)) {
          rv = UpgradeSchemaFrom11_0To12_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(12, 0)) {
          rv = UpgradeSchemaFrom12_0To13_0(connection, &vacuumNeeded);
        } else if (schemaVersion == MakeSchemaVersion(13, 0)) {
          rv = UpgradeSchemaFrom13_0To14_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(14, 0)) {
          rv = UpgradeSchemaFrom14_0To15_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(15, 0)) {
          rv = UpgradeSchemaFrom15_0To16_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(16, 0)) {
          rv = UpgradeSchemaFrom16_0To17_0(connection);
        } else {
          IDB_WARNING("Unable to open IndexedDB database, no upgrade path is "
                      "available!");
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = connection->GetSchemaVersion(&schemaVersion);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);
    }

    rv = transaction.Commit();
    if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      // mozstorage translates SQLITE_FULL to NS_ERROR_FILE_NO_DEVICE_SPACE,
      // which we know better as NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR.
      rv = NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (vacuumNeeded) {
    rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM;"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  connection.forget(aConnection);
  return NS_OK;
}

already_AddRefed<nsIFile>
GetFileForPath(const nsAString& aPath)
{
  MOZ_ASSERT(!aPath.IsEmpty());

  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  if (NS_WARN_IF(!file)) {
    return nullptr;
  }

  if (NS_WARN_IF(NS_FAILED(file->InitWithPath(aPath)))) {
    return nullptr;
  }

  return file.forget();
}

nsresult
GetDatabaseConnection(const nsAString& aDatabaseFilePath,
                      PersistenceType aPersistenceType,
                      const nsACString& aGroup,
                      const nsACString& aOrigin,
                      mozIStorageConnection** aConnection)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!aDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(StringEndsWith(aDatabaseFilePath, NS_LITERAL_STRING(".sqlite")));
  MOZ_ASSERT(aConnection);

  PROFILER_LABEL("IndexedDB",
                 "GetDatabaseConnection",
                 js::ProfileEntry::Category::STORAGE);

  nsCOMPtr<nsIFile> dbFile = GetFileForPath(aDatabaseFilePath);
  if (NS_WARN_IF(!dbFile)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  bool exists;
  nsresult rv = dbFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!exists)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFileURL> dbFileUrl;
  rv = GetDatabaseFileURL(dbFile, aPersistenceType, aGroup, aOrigin,
                          getter_AddRefs(dbFileUrl));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetDefaultPragmas(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class DatabaseOperationBase
  : public nsRunnable
  , public mozIStorageProgressHandler
{
protected:
  class AutoSetProgressHandler;

  typedef nsDataHashtable<nsUint64HashKey, bool> UniqueIndexTable;

  nsCOMPtr<nsIEventTarget> mOwningThread;
  const nsID mBackgroundChildLoggingId;
  const uint64_t mLoggingSerialNumber;
  nsresult mResultCode;

private:
  Atomic<bool> mOperationMayProceed;
  bool mActorDestroyed;

public:
  NS_DECL_ISUPPORTS_INHERITED

  void
  AssertIsOnOwningThread() const
  {
    AssertIsOnBackgroundThread();

#ifdef DEBUG
    MOZ_ASSERT(mOwningThread);
    bool current;
    MOZ_ASSERT(NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)));
    MOZ_ASSERT(current);
#endif
  }

  void
  NoteActorDestroyed()
  {
    AssertIsOnOwningThread();

    mActorDestroyed = true;
    mOperationMayProceed = false;
  }

  bool
  IsActorDestroyed() const
  {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

  // May be called on any thread.
  bool
  OperationMayProceed() const
  {
    return mOperationMayProceed;
  }

  const nsID&
  BackgroundChildLoggingId() const
  {
    return mBackgroundChildLoggingId;
  }

  uint64_t
  LoggingSerialNumber() const
  {
    return mLoggingSerialNumber;
  }

  nsresult
  ResultCode() const
  {
    return mResultCode;
  }

  void
  SetFailureCode(nsresult aErrorCode)
  {
    MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    mResultCode = aErrorCode;
  }

protected:
  DatabaseOperationBase(const nsID& aBackgroundChildLoggingId,
                        uint64_t aLoggingSerialNumber)
    : mOwningThread(NS_GetCurrentThread())
    , mBackgroundChildLoggingId(aBackgroundChildLoggingId)
    , mLoggingSerialNumber(aLoggingSerialNumber)
    , mResultCode(NS_OK)
    , mOperationMayProceed(true)
    , mActorDestroyed(false)
  {
    AssertIsOnOwningThread();
  }

  virtual
  ~DatabaseOperationBase()
  {
    MOZ_ASSERT(mActorDestroyed);
  }

  static void
  GetBindingClauseForKeyRange(const SerializedKeyRange& aKeyRange,
                              const nsACString& aKeyColumnName,
                              nsAutoCString& aBindingClause);

  static uint64_t
  ReinterpretDoubleAsUInt64(double aDouble);

  static nsresult
  GetStructuredCloneReadInfoFromStatement(mozIStorageStatement* aStatement,
                                          uint32_t aDataIndex,
                                          uint32_t aFileIdsIndex,
                                          FileManager* aFileManager,
                                          StructuredCloneReadInfo* aInfo);

  static nsresult
  BindKeyRangeToStatement(const SerializedKeyRange& aKeyRange,
                          mozIStorageStatement* aStatement);

  static void
  AppendConditionClause(const nsACString& aColumnName,
                        const nsACString& aArgName,
                        bool aLessThan,
                        bool aEquals,
                        nsAutoCString& aResult);

  static nsresult
  UpdateIndexes(TransactionBase* aTransaction,
                const UniqueIndexTable& aUniqueIndexTable,
                const Key& aObjectStoreKey,
                bool aOverwrite,
                int64_t aObjectDataId,
                const nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

private:
  // Not to be overridden by subclasses.
  NS_DECL_MOZISTORAGEPROGRESSHANDLER
};

class MOZ_STACK_CLASS DatabaseOperationBase::AutoSetProgressHandler MOZ_FINAL
{
  mozIStorageConnection* mConnection;
  DebugOnly<DatabaseOperationBase*> mDEBUGDatabaseOp;

public:
  AutoSetProgressHandler()
    : mConnection(nullptr)
    , mDEBUGDatabaseOp(nullptr)
  { }

  ~AutoSetProgressHandler();

  nsresult
  Register(DatabaseOperationBase* aDatabaseOp,
           const nsCOMPtr<mozIStorageConnection>& aConnection);
};

class TransactionDatabaseOperationBase
  : public DatabaseOperationBase
{
  nsRefPtr<TransactionBase> mTransaction;
  const int64_t mTransactionLoggingSerialNumber;
  const bool mTransactionIsAborted;

public:
  void
  AssertIsOnTransactionThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  void
  DispatchToTransactionThreadPool();

  // May be overridden by subclasses if they need to perform work on the
  // background thread before being dispatched. Returning false will kill the
  // child actors and prevent dispatch.
  virtual bool
  Init(TransactionBase* aTransaction);

  // This callback will be called on the background thread before releasing the
  // final reference to this request object. Subclasses may perform any
  // additional cleanup here but must always call the base class implementation.
  virtual void
  Cleanup();

protected:
  explicit
  TransactionDatabaseOperationBase(TransactionBase* aTransaction);

  TransactionDatabaseOperationBase(TransactionBase* aTransaction,
                                   uint64_t aLoggingSerialNumber);

  virtual
  ~TransactionDatabaseOperationBase();

  // Must be overridden in subclasses. Called on the target thread to allow the
  // subclass to perform necessary database or file operations. A successful
  // return value will trigger a SendSuccessResult callback on the background
  // thread while a failure value will trigger a SendFailureResult callback.
  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) = 0;

  // Must be overridden in subclasses. Called on the background thread to allow
  // the subclass to serialize its results and send them to the child actor. A
  // failed return value will trigger a SendFailureResult callback.
  virtual nsresult
  SendSuccessResult() = 0;

  // Must be overridden in subclasses. Called on the background thread to allow
  // the subclass to send its failure code. Returning false will cause the
  // transaction to be aborted with aResultCode. Returning true will not cause
  // the transaction to be aborted.
  virtual bool
  SendFailureResult(nsresult aResultCode) = 0;

private:
  void
  RunOnTransactionThread();

  void
  RunOnOwningThread();

  // Not to be overridden by subclasses.
  NS_DECL_NSIRUNNABLE
};

class Factory MOZ_FINAL
  : public PBackgroundIDBFactoryParent
{
  // Counts the number of "live" Factory instances that have not yet had
  // ActorDestroy called.
  static uint64_t sFactoryInstanceCount;

  nsRefPtr<DatabaseLoggingInfo> mLoggingInfo;

  DebugOnly<bool> mActorDestroyed;

public:
  static already_AddRefed<Factory>
  Create(const LoggingInfo& aLoggingInfo);

  DatabaseLoggingInfo*
  GetLoggingInfo() const
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo);

    return mLoggingInfo;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::Factory)

private:
  // Only constructed in Create().
  explicit
  Factory(already_AddRefed<DatabaseLoggingInfo> aLoggingInfo);

  // Reference counted.
  ~Factory();

  // IPDL methods are only called by IPDL.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvDeleteMe() MOZ_OVERRIDE;

  virtual bool
  RecvIncrementLoggingRequestSerialNumber() MOZ_OVERRIDE;

  virtual PBackgroundIDBFactoryRequestParent*
  AllocPBackgroundIDBFactoryRequestParent(const FactoryRequestParams& aParams)
                                          MOZ_OVERRIDE;

  virtual bool
  RecvPBackgroundIDBFactoryRequestConstructor(
                                     PBackgroundIDBFactoryRequestParent* aActor,
                                     const FactoryRequestParams& aParams)
                                     MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBFactoryRequestParent(
                                     PBackgroundIDBFactoryRequestParent* aActor)
                                     MOZ_OVERRIDE;

  virtual PBackgroundIDBDatabaseParent*
  AllocPBackgroundIDBDatabaseParent(
                                   const DatabaseSpec& aSpec,
                                   PBackgroundIDBFactoryRequestParent* aRequest)
                                   MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBDatabaseParent(PBackgroundIDBDatabaseParent* aActor)
                                      MOZ_OVERRIDE;
};

class Database MOZ_FINAL
  : public PBackgroundIDBDatabaseParent
{
  friend class VersionChangeTransaction;

  nsRefPtr<Factory> mFactory;
  nsRefPtr<FullDatabaseMetadata> mMetadata;
  nsRefPtr<FileManager> mFileManager;
  nsRefPtr<DatabaseOfflineStorage> mOfflineStorage;
  nsTHashtable<nsPtrHashKey<TransactionBase>> mTransactions;
  const PrincipalInfo mPrincipalInfo;
  const nsCString mGroup;
  const nsCString mOrigin;
  const nsCString mId;
  const nsString mFilePath;
  const PersistenceType mPersistenceType;
  const bool mChromeWriteAccessAllowed;
  bool mClosed;
  bool mInvalidated;
  bool mActorWasAlive;
  bool mActorDestroyed;
  bool mMetadataCleanedUp;

public:
  // Created by OpenDatabaseOp.
  Database(Factory* aFactory,
           const PrincipalInfo& aPrincipalInfo,
           const nsACString& aGroup,
           const nsACString& aOrigin,
           FullDatabaseMetadata* aMetadata,
           FileManager* aFileManager,
           already_AddRefed<DatabaseOfflineStorage> aOfflineStorage,
           bool aChromeWriteAccessAllowed);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::Database)

  void
  Invalidate();

  const PrincipalInfo&
  GetPrincipalInfo() const
  {
    return mPrincipalInfo;
  }

  const nsCString&
  Group() const
  {
    return mGroup;
  }

  const nsCString&
  Origin() const
  {
    return mOrigin;
  }

  const nsCString&
  Id() const
  {
    return mId;
  }

  PersistenceType
  Type() const
  {
    return mPersistenceType;
  }

  const nsString&
  FilePath() const
  {
    return mFilePath;
  }

  FileManager*
  GetFileManager() const
  {
    return mFileManager;
  }

  FullDatabaseMetadata*
  Metadata() const
  {
    MOZ_ASSERT(mMetadata);
    return mMetadata;
  }

  PBackgroundParent*
  GetBackgroundParent() const
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!IsActorDestroyed());

    return Manager()->Manager();
  }

  DatabaseLoggingInfo*
  GetLoggingInfo() const
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mFactory);

    return mFactory->GetLoggingInfo();
  }

  bool
  RegisterTransaction(TransactionBase* aTransaction);

  void
  UnregisterTransaction(TransactionBase* aTransaction);

  void
  SetActorAlive();

  bool
  IsActorAlive() const
  {
    AssertIsOnBackgroundThread();

    return mActorWasAlive && !mActorDestroyed;
  }

  bool
  IsActorDestroyed() const
  {
    AssertIsOnBackgroundThread();

    return mActorWasAlive && mActorDestroyed;
  }

  bool
  IsClosed() const
  {
    AssertIsOnBackgroundThread();

    return mClosed;
  }

  bool
  IsInvalidated() const
  {
    AssertIsOnBackgroundThread();

    return mInvalidated;
  }

private:
  // Reference counted.
  ~Database()
  {
    MOZ_ASSERT(mClosed);
    MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);
  }

  bool
  CloseInternal();

  void
  CleanupMetadata();

  // IPDL methods are only called by IPDL.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PBackgroundIDBDatabaseFileParent*
  AllocPBackgroundIDBDatabaseFileParent(PBlobParent* aBlobParent)
                                        MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBDatabaseFileParent(
                                       PBackgroundIDBDatabaseFileParent* aActor)
                                       MOZ_OVERRIDE;

  virtual PBackgroundIDBTransactionParent*
  AllocPBackgroundIDBTransactionParent(
                                    const nsTArray<nsString>& aObjectStoreNames,
                                    const Mode& aMode)
                                    MOZ_OVERRIDE;

  virtual bool
  RecvPBackgroundIDBTransactionConstructor(
                                    PBackgroundIDBTransactionParent* aActor,
                                    InfallibleTArray<nsString>&& aObjectStoreNames,
                                    const Mode& aMode)
                                    MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBTransactionParent(
                                        PBackgroundIDBTransactionParent* aActor)
                                        MOZ_OVERRIDE;

  virtual PBackgroundIDBVersionChangeTransactionParent*
  AllocPBackgroundIDBVersionChangeTransactionParent(
                                              const uint64_t& aCurrentVersion,
                                              const uint64_t& aRequestedVersion,
                                              const int64_t& aNextObjectStoreId,
                                              const int64_t& aNextIndexId)
                                              MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBVersionChangeTransactionParent(
                           PBackgroundIDBVersionChangeTransactionParent* aActor)
                           MOZ_OVERRIDE;

  virtual bool
  RecvDeleteMe() MOZ_OVERRIDE;

  virtual bool
  RecvBlocked() MOZ_OVERRIDE;

  virtual bool
  RecvClose() MOZ_OVERRIDE;
};

class DatabaseFile MOZ_FINAL
  : public PBackgroundIDBDatabaseFileParent
{
  friend class Database;

  nsRefPtr<FileImpl> mBlobImpl;
  nsRefPtr<FileInfo> mFileInfo;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::DatabaseFile);

  FileInfo*
  GetFileInfo() const
  {
    AssertIsOnBackgroundThread();

    return mFileInfo;
  }

  already_AddRefed<nsIInputStream>
  GetInputStream() const
  {
    AssertIsOnBackgroundThread();

    nsCOMPtr<nsIInputStream> inputStream;
    if (mBlobImpl) {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
        mBlobImpl->GetInternalStream(getter_AddRefs(inputStream))));
    }

    return inputStream.forget();
  }

  void
  ClearInputStream()
  {
    AssertIsOnBackgroundThread();

    mBlobImpl = nullptr;
  }

private:
  // Called when sending to the child.
  explicit DatabaseFile(FileInfo* aFileInfo)
    : mFileInfo(aFileInfo)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aFileInfo);
  }

  // Called when receiving from the child.
  DatabaseFile(FileImpl* aBlobImpl, FileInfo* aFileInfo)
    : mBlobImpl(aBlobImpl)
    , mFileInfo(aFileInfo)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aBlobImpl);
    MOZ_ASSERT(aFileInfo);
  }

  ~DatabaseFile()
  { }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE
  {
    AssertIsOnBackgroundThread();

    mBlobImpl = nullptr;
    mFileInfo = nullptr;
  }
};

class TransactionBase
{
  friend class Cursor;

  class CommitOp;
  class UpdateRefcountFunction;

public:
  class AutoSavepoint;
  class CachedStatement;

protected:
  typedef IDBTransaction::Mode Mode;

private:
  nsRefPtr<Database> mDatabase;
  nsCOMPtr<mozIStorageConnection> mConnection;
  nsRefPtr<UpdateRefcountFunction> mUpdateFileRefcountFunction;
  nsInterfaceHashtable<nsCStringHashKey, mozIStorageStatement>
    mCachedStatements;
  nsTArray<nsRefPtr<FullObjectStoreMetadata>>
    mModifiedAutoIncrementObjectStoreMetadataArray;
  const uint64_t mTransactionId;
  const nsCString mDatabaseId;
  const int64_t mLoggingSerialNumber;
  uint64_t mActiveRequestCount;
  Atomic<bool> mInvalidatedOnAnyThread;
  Mode mMode;
  bool mHasBeenActive;
  bool mActorDestroyed;
  bool mInvalidated;

protected:
  nsresult mResultCode;
  bool mCommitOrAbortReceived;
  bool mCommittedOrAborted;
  bool mForceAborted;

private:
  DebugOnly<PRThread*> mTransactionThread;
  DebugOnly<uint32_t> mSavepointCount;

public:
  void
  AssertIsOnTransactionThread() const
  {
    MOZ_ASSERT(mTransactionThread);
    MOZ_ASSERT(PR_GetCurrentThread() == mTransactionThread);
  }

  bool
  IsActorDestroyed() const
  {
    AssertIsOnBackgroundThread();

    return mActorDestroyed;
  }

  // Must be called on the background thread.
  bool
  IsInvalidated() const
  {
    MOZ_ASSERT(IsOnBackgroundThread(), "Use IsInvalidatedOnAnyThread()");
    MOZ_ASSERT_IF(mInvalidated, NS_FAILED(mResultCode));

    return mInvalidated;
  }

  // May be called on any thread, but is more expensive than IsInvalidated().
  bool
  IsInvalidatedOnAnyThread() const
  {
    return mInvalidatedOnAnyThread;
  }

  void
  SetActive()
  {
    AssertIsOnBackgroundThread();

    mHasBeenActive = true;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(
    mozilla::dom::indexedDB::TransactionBase)

  nsresult
  GetCachedStatement(const nsACString& aQuery,
                     CachedStatement* aCachedStatement);

  template<int N>
  nsresult
  GetCachedStatement(const char (&aQuery)[N],
                     CachedStatement* aCachedStatement)
  {
    AssertIsOnTransactionThread();
    MOZ_ASSERT(aCachedStatement);

    return GetCachedStatement(NS_LITERAL_CSTRING(aQuery), aCachedStatement);
  }

  nsresult
  EnsureConnection();

  void
  Abort(nsresult aResultCode, bool aForce);

  mozIStorageConnection*
  Connection() const
  {
    AssertIsOnTransactionThread();
    MOZ_ASSERT(mConnection);

    return mConnection;
  }

  uint64_t
  TransactionId() const
  {
    return mTransactionId;
  }

  const nsCString&
  DatabaseId() const
  {
    return mDatabaseId;
  }

  Mode
  GetMode() const
  {
    return mMode;
  }

  Database*
  GetDatabase() const
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatabase);

    return mDatabase;
  }

  DatabaseLoggingInfo*
  GetLoggingInfo() const
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatabase);

    return mDatabase->GetLoggingInfo();
  }

  int64_t
  LoggingSerialNumber() const
  {
    return mLoggingSerialNumber;
  }

  bool
  IsAborted() const
  {
    AssertIsOnBackgroundThread();

    return NS_FAILED(mResultCode);
  }

  already_AddRefed<FullObjectStoreMetadata>
  GetMetadataForObjectStoreId(int64_t aObjectStoreId) const;

  already_AddRefed<FullIndexMetadata>
  GetMetadataForIndexId(FullObjectStoreMetadata* const aObjectStoreMetadata,
                        int64_t aIndexId) const;

  PBackgroundParent*
  GetBackgroundParent() const
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!IsActorDestroyed());

    return GetDatabase()->GetBackgroundParent();
  }

  void
  NoteModifiedAutoIncrementObjectStore(FullObjectStoreMetadata* aMetadata);

  void
  ForgetModifiedAutoIncrementObjectStore(FullObjectStoreMetadata* aMetadata);

  nsresult
  StartSavepoint();

  nsresult
  ReleaseSavepoint();

  nsresult
  RollbackSavepoint();

  void
  NoteActiveRequest();

  void
  NoteFinishedRequest();

  void
  Invalidate();

protected:
  TransactionBase(Database* aDatabase, Mode aMode);

  virtual
  ~TransactionBase();

  void
  NoteActorDestroyed()
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!mActorDestroyed);

    mActorDestroyed = true;
  }

#ifdef DEBUG
  // Only called by VersionChangeTransaction.
  void
  FakeActorDestroyed()
  {
    mActorDestroyed = true;
  }
#endif

  bool
  RecvCommit();

  bool
  RecvAbort(nsresult aResultCode);

  void
  MaybeCommitOrAbort()
  {
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

  PBackgroundIDBRequestParent*
  AllocRequest(const RequestParams& aParams, bool aTrustParams);

  bool
  StartRequest(PBackgroundIDBRequestParent* aActor);

  bool
  DeallocRequest(PBackgroundIDBRequestParent* aActor);

  PBackgroundIDBCursorParent*
  AllocCursor(const OpenCursorParams& aParams, bool aTrustParams);

  bool
  StartCursor(PBackgroundIDBCursorParent* aActor,
              const OpenCursorParams& aParams);

  bool
  DeallocCursor(PBackgroundIDBCursorParent* aActor);

  virtual void
  UpdateMetadata(nsresult aResult)
  { }

  virtual bool
  SendCompleteNotification(nsresult aResult) = 0;

private:
  // Only called by CommitOp.
  void
  ReleaseTransactionThreadObjects();

  // Only called by CommitOp.
  void
  ReleaseBackgroundThreadObjects();

  bool
  VerifyRequestParams(const RequestParams& aParams) const;

  bool
  VerifyRequestParams(const OpenCursorParams& aParams) const;

  bool
  VerifyRequestParams(const CursorRequestParams& aParams) const;

  bool
  VerifyRequestParams(const SerializedKeyRange& aKeyRange) const;

  bool
  VerifyRequestParams(const ObjectStoreAddPutParams& aParams) const;

  bool
  VerifyRequestParams(const OptionalKeyRange& aKeyRange) const;

  void
  CommitOrAbort();
};

class TransactionBase::CommitOp MOZ_FINAL
  : public DatabaseOperationBase
  , public TransactionThreadPool::FinishCallback
{
  friend class TransactionBase;

  nsRefPtr<TransactionBase> mTransaction;
  nsresult mResultCode;

private:
  CommitOp(TransactionBase* aTransaction, nsresult aResultCode);

  ~CommitOp()
  { }

  // Writes new autoIncrement counts to database.
  nsresult
  WriteAutoIncrementCounts();

  // Updates counts after a database activity has finished.
  void
  CommitOrRollbackAutoIncrementCounts();

  NS_DECL_NSIRUNNABLE

  virtual void
  TransactionFinishedBeforeUnblock() MOZ_OVERRIDE;

  virtual void
  TransactionFinishedAfterUnblock() MOZ_OVERRIDE;

public:
  void
  AssertIsOnTransactionThread() const
  {
    MOZ_ASSERT(mTransaction);
    mTransaction->AssertIsOnTransactionThread();
  }

  NS_DECL_ISUPPORTS_INHERITED
};

class TransactionBase::UpdateRefcountFunction MOZ_FINAL
  : public mozIStorageFunction
{
  class FileInfoEntry
  {
    friend class UpdateRefcountFunction;

    nsRefPtr<FileInfo> mFileInfo;
    int32_t mDelta;
    int32_t mSavepointDelta;

  public:
    explicit FileInfoEntry(FileInfo* aFileInfo)
      : mFileInfo(aFileInfo)
      , mDelta(0)
      , mSavepointDelta(0)
    { }
  };

  enum UpdateType
  {
    eIncrement,
    eDecrement
  };

  class DatabaseUpdateFunction
  {
    nsCOMPtr<mozIStorageConnection> mConnection;
    nsCOMPtr<mozIStorageStatement> mUpdateStatement;
    nsCOMPtr<mozIStorageStatement> mSelectStatement;
    nsCOMPtr<mozIStorageStatement> mInsertStatement;

    UpdateRefcountFunction* mFunction;

    nsresult mErrorCode;

  public:
    DatabaseUpdateFunction(mozIStorageConnection* aConnection,
                           UpdateRefcountFunction* aFunction)
      : mConnection(aConnection)
      , mFunction(aFunction)
      , mErrorCode(NS_OK)
    { }

    bool
    Update(int64_t aId, int32_t aDelta);

    nsresult
    ErrorCode() const
    {
      return mErrorCode;
    }

  private:
    nsresult
    UpdateInternal(int64_t aId, int32_t aDelta);
  };

  FileManager* mFileManager;
  nsClassHashtable<nsUint64HashKey, FileInfoEntry> mFileInfoEntries;
  nsDataHashtable<nsUint64HashKey, FileInfoEntry*> mSavepointEntriesIndex;

  nsTArray<int64_t> mJournalsToCreateBeforeCommit;
  nsTArray<int64_t> mJournalsToRemoveAfterCommit;
  nsTArray<int64_t> mJournalsToRemoveAfterAbort;

  bool mInSavepoint;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  explicit UpdateRefcountFunction(FileManager* aFileManager)
    : mFileManager(aFileManager)
    , mInSavepoint(false)
  { }

  void
  ClearFileInfoEntries()
  {
    mFileInfoEntries.Clear();
  }

  nsresult
  WillCommit(mozIStorageConnection* aConnection);

  void
  DidCommit();

  void
  DidAbort();

  void
  StartSavepoint();

  void
  ReleaseSavepoint();

  void
  RollbackSavepoint();

private:
  ~UpdateRefcountFunction()
  { }

  nsresult
  ProcessValue(mozIStorageValueArray* aValues,
               int32_t aIndex,
               UpdateType aUpdateType);

  nsresult
  CreateJournals();

  nsresult
  RemoveJournals(const nsTArray<int64_t>& aJournals);

  static PLDHashOperator
  DatabaseUpdateCallback(const uint64_t& aKey,
                         FileInfoEntry* aValue,
                         void* aUserArg);

  static PLDHashOperator
  FileInfoUpdateCallback(const uint64_t& aKey,
                         FileInfoEntry* aValue,
                         void* aUserArg);
};

class MOZ_STACK_CLASS TransactionBase::AutoSavepoint MOZ_FINAL
{
  TransactionBase* mTransaction;

public:
  AutoSavepoint()
    : mTransaction(nullptr)
  { }

  ~AutoSavepoint();

  nsresult
  Start(TransactionBase* aTransaction);

  nsresult
  Commit();
};

class TransactionBase::CachedStatement MOZ_FINAL
{
  friend class TransactionBase;

  nsCOMPtr<mozIStorageStatement> mStatement;
  Maybe<mozStorageStatementScoper> mScoper;

public:
  CachedStatement()
  { }

  ~CachedStatement()
  { }

  operator mozIStorageStatement*()
  {
    return mStatement;
  }

  mozIStorageStatement*
  operator->() MOZ_NO_ADDREF_RELEASE_ON_RETURN
  {
    MOZ_ASSERT(mStatement);
    return mStatement;
  }

  void
  Reset()
  {
    MOZ_ASSERT_IF(mStatement, mScoper);

    if (mStatement) {
      mScoper.reset();
      mScoper.emplace(mStatement);
    }
  }

private:
  // Only called by TransactionBase.
  void
  Assign(already_AddRefed<mozIStorageStatement> aStatement)
  {
    mScoper.reset();

    mStatement = aStatement;

    if (mStatement) {
      mScoper.emplace(mStatement);
    }
  }

  // No funny business allowed.
  CachedStatement(const CachedStatement&) = delete;
  CachedStatement& operator=(const CachedStatement&) = delete;
};

class NormalTransaction MOZ_FINAL
  : public TransactionBase
  , public PBackgroundIDBTransactionParent
{
  friend class Database;

  nsTArray<nsRefPtr<FullObjectStoreMetadata>> mObjectStores;

private:
  // This constructor is only called by Database.
  NormalTransaction(Database* aDatabase,
                    TransactionBase::Mode aMode,
                    nsTArray<nsRefPtr<FullObjectStoreMetadata>>& aObjectStores);

  // Reference counted.
  ~NormalTransaction()
  { }

  bool
  IsSameProcessActor();

  // Only called by TransactionBase.
  virtual bool
  SendCompleteNotification(nsresult aResult) MOZ_OVERRIDE;

  // IPDL methods are only called by IPDL.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvDeleteMe() MOZ_OVERRIDE;

  virtual bool
  RecvCommit() MOZ_OVERRIDE;

  virtual bool
  RecvAbort(const nsresult& aResultCode) MOZ_OVERRIDE;

  virtual PBackgroundIDBRequestParent*
  AllocPBackgroundIDBRequestParent(const RequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  RecvPBackgroundIDBRequestConstructor(PBackgroundIDBRequestParent* aActor,
                                       const RequestParams& aParams)
                                       MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBRequestParent(PBackgroundIDBRequestParent* aActor)
                                     MOZ_OVERRIDE;

  virtual PBackgroundIDBCursorParent*
  AllocPBackgroundIDBCursorParent(const OpenCursorParams& aParams) MOZ_OVERRIDE;

  virtual bool
  RecvPBackgroundIDBCursorConstructor(PBackgroundIDBCursorParent* aActor,
                                      const OpenCursorParams& aParams)
                                      MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBCursorParent(PBackgroundIDBCursorParent* aActor)
                                    MOZ_OVERRIDE;
};

class VersionChangeTransaction MOZ_FINAL
  : public TransactionBase
  , public PBackgroundIDBVersionChangeTransactionParent
{
  friend class OpenDatabaseOp;

  nsRefPtr<OpenDatabaseOp> mOpenDatabaseOp;
  nsRefPtr<FullDatabaseMetadata> mOldMetadata;

  bool mActorWasAlive;

private:
  // Only called by OpenDatabaseOp.
  explicit VersionChangeTransaction(OpenDatabaseOp* aOpenDatabaseOp);

  // Reference counted.
  ~VersionChangeTransaction();

  bool
  IsSameProcessActor();

  // Only called by OpenDatabaseOp.
  bool
  CopyDatabaseMetadata();

  void
  SetActorAlive();

  // Only called by TransactionBase.
  virtual void
  UpdateMetadata(nsresult aResult) MOZ_OVERRIDE;

  // Only called by TransactionBase.
  virtual bool
  SendCompleteNotification(nsresult aResult) MOZ_OVERRIDE;

  // IPDL methods are only called by IPDL.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvDeleteMe() MOZ_OVERRIDE;

  virtual bool
  RecvCommit() MOZ_OVERRIDE;

  virtual bool
  RecvAbort(const nsresult& aResultCode) MOZ_OVERRIDE;

  virtual bool
  RecvCreateObjectStore(const ObjectStoreMetadata& aMetadata) MOZ_OVERRIDE;

  virtual bool
  RecvDeleteObjectStore(const int64_t& aObjectStoreId) MOZ_OVERRIDE;

  virtual bool
  RecvCreateIndex(const int64_t& aObjectStoreId,
                  const IndexMetadata& aMetadata) MOZ_OVERRIDE;

  virtual bool
  RecvDeleteIndex(const int64_t& aObjectStoreId,
                  const int64_t& aIndexId) MOZ_OVERRIDE;

  virtual PBackgroundIDBRequestParent*
  AllocPBackgroundIDBRequestParent(const RequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  RecvPBackgroundIDBRequestConstructor(PBackgroundIDBRequestParent* aActor,
                                       const RequestParams& aParams)
                                       MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBRequestParent(PBackgroundIDBRequestParent* aActor)
                                     MOZ_OVERRIDE;

  virtual PBackgroundIDBCursorParent*
  AllocPBackgroundIDBCursorParent(const OpenCursorParams& aParams) MOZ_OVERRIDE;

  virtual bool
  RecvPBackgroundIDBCursorConstructor(PBackgroundIDBCursorParent* aActor,
                                      const OpenCursorParams& aParams)
                                      MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBCursorParent(PBackgroundIDBCursorParent* aActor)
                                    MOZ_OVERRIDE;
};

class FactoryOp
  : public DatabaseOperationBase
  , public PBackgroundIDBFactoryRequestParent
{
public:
  struct MaybeBlockedDatabaseInfo;

protected:
  enum State
  {
    // Just created on the PBackground thread, dispatched to the main thread.
    // Next step is State_OpenPending.
    State_Initial,

    // Waiting for open allowed on the main thread. The next step is either
    // State_SendingResults if permission is denied,
    // State_PermissionChallenge if the permission is unknown, or
    // State_DatabaseWorkOpen if permission is granted.
    State_OpenPending,

    // Sending a permission challenge message to the child on the PBackground
    // thread. Next step is State_PermissionRetryReady.
    State_PermissionChallenge,

    // Retrying permission check after a challenge on the main thread. Next step
    // is either State_SendingResults if permission is denied or
    // State_DatabaseWorkOpen if permission is granted.
    State_PermissionRetry,

    // Waiting to do/doing work on the QuotaManager IO thread. Its next step is
    // either State_BeginVersionChange if the requested version doesn't match
    // the existing database version or State_SendingResults if the versions
    // match.
    State_DatabaseWorkOpen,

    // Starting a version change transaction or deleting a database on the
    // PBackground thread. We need to notify other databases that a version
    // change is about to happen, and maybe tell the request that a version
    // change has been blocked. If databases are notified then the next step is
    // State_WaitingForOtherDatabasesToClose. Otherwise the next step is
    // State_DispatchToWorkThread.
    State_BeginVersionChange,

    // Waiting for other databases to close on the PBackground thread. This
    // state may persist until all databases are closed. The next state is
    // State_WaitingForTransactionsToComplete.
    State_WaitingForOtherDatabasesToClose,

    // Waiting for all transactions that could interfere with this operation to
    // complete on the PBackground thread. Next state is
    // State_DatabaseWorkVersionChange.
    State_WaitingForTransactionsToComplete,

    // Waiting to do/doing work on the "work thread". This involves waiting for
    // the VersionChangeOp (OpenDatabaseOp and DeleteDatabaseOp each have a
    // different implementation) to do its work. Eventually the state will
    // transition to State_SendingResults.
    State_DatabaseWorkVersionChange,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // UnblockingQuotaManager.
    State_SendingResults,

    // Notifying the QuotaManager that it can proceed to the next operation on
    // the main thread. Next step is Completed.
    State_UnblockingQuotaManager,

    // All done.
    State_Completed
  };

  // Must be released on the background thread!
  nsRefPtr<Factory> mFactory;

  // Must be released on the main thread!
  nsRefPtr<ContentParent> mContentParent;

  nsTArray<MaybeBlockedDatabaseInfo> mMaybeBlockedDatabases;

  const CommonFactoryRequestParams mCommonParams;
  nsCString mGroup;
  nsCString mOrigin;
  nsCString mDatabaseId;
  State mState;
  bool mIsApp;
  bool mEnforcingQuota;
  const bool mDeleting;
  bool mBlockedQuotaManager;
  bool mChromeWriteAccessAllowed;

public:
  void
  NoteDatabaseBlocked(Database* aDatabase);

  virtual void
  NoteDatabaseClosed(Database* aDatabase) = 0;

#ifdef DEBUG
  bool
  HasBlockedDatabases() const
  {
    return !mMaybeBlockedDatabases.IsEmpty();
  }
#endif

protected:
  FactoryOp(Factory* aFactory,
            already_AddRefed<ContentParent> aContentParent,
            const CommonFactoryRequestParams& aCommonParams,
            bool aDeleting);

  virtual
  ~FactoryOp()
  {
    // Normally this would be out-of-line since it is a virtual function but
    // MSVC 2010 fails to link for some reason if it is not inlined here...
    MOZ_ASSERT_IF(OperationMayProceed(),
                  mState == State_Initial || mState == State_Completed);
  }

  nsresult
  Open();

  nsresult
  ChallengePermission();

  nsresult
  RetryCheckPermission();

  nsresult
  SendToIOThread();

  void
  WaitForTransactions();

  void
  FinishSendResults();

  void
  UnblockQuotaManager();

  nsresult
  SendVersionChangeMessages(DatabaseActorInfo* aDatabaseActorInfo,
                            Database* aOpeningDatabase,
                            uint64_t aOldVersion,
                            const NullableVersion& aNewVersion);

  // Methods that subclasses must implement.
  virtual nsresult
  QuotaManagerOpen() = 0;

  virtual nsresult
  DoDatabaseWork() = 0;

  virtual nsresult
  BeginVersionChange() = 0;

  virtual nsresult
  DispatchToWorkThread() = 0;

  virtual void
  SendResults() = 0;

  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() MOZ_FINAL;

  // IPDL methods.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPermissionRetry() MOZ_OVERRIDE;

  virtual void
  SendBlockedNotification() = 0;

private:
  nsresult
  CheckPermission(ContentParent* aContentParent,
                  PermissionRequestBase::PermissionValue* aPermission);

  static bool
  CheckAtLeastOneAppHasPermission(ContentParent* aContentParent,
                                  const nsACString& aPermissionString);

  nsresult
  FinishOpen();
};

struct FactoryOp::MaybeBlockedDatabaseInfo MOZ_FINAL
{
  nsRefPtr<Database> mDatabase;
  bool mBlocked;

  MOZ_IMPLICIT MaybeBlockedDatabaseInfo(Database* aDatabase)
    : mDatabase(aDatabase)
    , mBlocked(false)
  {
    MOZ_ASSERT(aDatabase);

    MOZ_COUNT_CTOR(FactoryOp::MaybeBlockedDatabaseInfo);
  }

  ~MaybeBlockedDatabaseInfo()
  {
    MOZ_COUNT_DTOR(FactoryOp::MaybeBlockedDatabaseInfo);
  }

  bool
  operator==(const MaybeBlockedDatabaseInfo& aOther) const
  {
    return mDatabase == aOther.mDatabase;
  }

  bool
  operator<(const MaybeBlockedDatabaseInfo& aOther) const
  {
    return mDatabase < aOther.mDatabase;
  }

  Database*
  operator->() MOZ_NO_ADDREF_RELEASE_ON_RETURN
  {
    return mDatabase;
  }
};

class OpenDatabaseOp MOZ_FINAL
  : public FactoryOp
{
  friend class Database;
  friend class VersionChangeTransaction;

  class VersionChangeOp;

  const OptionalContentId mOptionalContentParentId;

  nsRefPtr<FullDatabaseMetadata> mMetadata;

  uint64_t mRequestedVersion;
  nsString mDatabaseFilePath;
  nsRefPtr<FileManager> mFileManager;

  nsRefPtr<Database> mDatabase;
  nsRefPtr<VersionChangeTransaction> mVersionChangeTransaction;

  nsRefPtr<DatabaseOfflineStorage> mOfflineStorage;

public:
  OpenDatabaseOp(Factory* aFactory,
                 already_AddRefed<ContentParent> aContentParent,
                 const CommonFactoryRequestParams& aParams);

  bool
  IsOtherProcessActor() const
  {
    MOZ_ASSERT(mOptionalContentParentId.type() != OptionalContentId::T__None);

    return mOptionalContentParentId.type() ==
             OptionalContentId::TContentParentId;
  }

private:
  ~OpenDatabaseOp()
  { }

  nsresult
  LoadDatabaseInformation(mozIStorageConnection* aConnection);

  nsresult
  SendUpgradeNeeded();

  void
  EnsureDatabaseActor();

  nsresult
  EnsureDatabaseActorIsAlive();

  void
  MetadataToSpec(DatabaseSpec& aSpec);

  void
  AssertMetadataConsistency(const FullDatabaseMetadata* aMetadata)
#ifdef DEBUG
  ;
#else
  { }
#endif

  virtual nsresult
  QuotaManagerOpen() MOZ_OVERRIDE;

  virtual nsresult
  DoDatabaseWork() MOZ_OVERRIDE;

  virtual nsresult
  BeginVersionChange() MOZ_OVERRIDE;

  virtual void
  NoteDatabaseClosed(Database* aDatabase) MOZ_OVERRIDE;

  virtual void
  SendBlockedNotification() MOZ_OVERRIDE;

  virtual nsresult
  DispatchToWorkThread() MOZ_OVERRIDE;

  virtual void
  SendResults() MOZ_OVERRIDE;
};

class OpenDatabaseOp::VersionChangeOp MOZ_FINAL
  : public TransactionDatabaseOperationBase
{
  friend class OpenDatabaseOp;

  nsRefPtr<OpenDatabaseOp> mOpenDatabaseOp;
  const uint64_t mRequestedVersion;
  uint64_t mPreviousVersion;

private:
  explicit
  VersionChangeOp(OpenDatabaseOp* aOpenDatabaseOp)
    : TransactionDatabaseOperationBase(
                                     aOpenDatabaseOp->mVersionChangeTransaction,
                                     aOpenDatabaseOp->LoggingSerialNumber())
    , mOpenDatabaseOp(aOpenDatabaseOp)
    , mRequestedVersion(aOpenDatabaseOp->mRequestedVersion)
    , mPreviousVersion(aOpenDatabaseOp->mMetadata->mCommonMetadata.version())
  {
    MOZ_ASSERT(aOpenDatabaseOp);
    MOZ_ASSERT(mRequestedVersion);
  }

  ~VersionChangeOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual nsresult
  SendSuccessResult() MOZ_OVERRIDE;

  virtual bool
  SendFailureResult(nsresult aResultCode) MOZ_OVERRIDE;

  virtual void
  Cleanup() MOZ_OVERRIDE;
};

class DeleteDatabaseOp MOZ_FINAL
  : public FactoryOp
{
  class VersionChangeOp;

  nsString mDatabaseDirectoryPath;
  nsString mDatabaseFilenameBase;
  uint64_t mPreviousVersion;

public:
  DeleteDatabaseOp(Factory* aFactory,
                   already_AddRefed<ContentParent> aContentParent,
                   const CommonFactoryRequestParams& aParams)
    : FactoryOp(aFactory, Move(aContentParent), aParams, /* aDeleting */ true)
    , mPreviousVersion(0)
  { }

private:
  ~DeleteDatabaseOp()
  { }

  void
  LoadPreviousVersion(nsIFile* aDatabaseFile);

  virtual nsresult
  QuotaManagerOpen() MOZ_OVERRIDE;

  virtual nsresult
  DoDatabaseWork() MOZ_OVERRIDE;

  virtual nsresult
  BeginVersionChange() MOZ_OVERRIDE;

  virtual void
  NoteDatabaseClosed(Database* aDatabase) MOZ_OVERRIDE;

  virtual void
  SendBlockedNotification() MOZ_OVERRIDE;

  virtual nsresult
  DispatchToWorkThread() MOZ_OVERRIDE;

  virtual void
  SendResults() MOZ_OVERRIDE;
};

class DeleteDatabaseOp::VersionChangeOp MOZ_FINAL
  : public DatabaseOperationBase
{
  friend class DeleteDatabaseOp;

  nsRefPtr<DeleteDatabaseOp> mDeleteDatabaseOp;

private:
  explicit
  VersionChangeOp(DeleteDatabaseOp* aDeleteDatabaseOp)
    : DatabaseOperationBase(aDeleteDatabaseOp->BackgroundChildLoggingId(),
                            aDeleteDatabaseOp->LoggingSerialNumber())
    , mDeleteDatabaseOp(aDeleteDatabaseOp)
  {
    MOZ_ASSERT(aDeleteDatabaseOp);
    MOZ_ASSERT(!aDeleteDatabaseOp->mDatabaseDirectoryPath.IsEmpty());
  }

  ~VersionChangeOp()
  { }

  // XXX This should be much simpler when the QuotaManager lives on the
  //     PBackground thread.
  nsresult
  RunOnMainThread();

  nsresult
  RunOnIOThread();

  void
  RunOnOwningThread();

  NS_DECL_NSIRUNNABLE
};

class VersionChangeTransactionOp
  : public TransactionDatabaseOperationBase
{
public:
  virtual void
  Cleanup() MOZ_OVERRIDE;

protected:
  explicit VersionChangeTransactionOp(VersionChangeTransaction* aTransaction)
    : TransactionDatabaseOperationBase(aTransaction)
  { }

  virtual
  ~VersionChangeTransactionOp()
  { }

private:
  virtual nsresult
  SendSuccessResult() MOZ_OVERRIDE;

  virtual bool
  SendFailureResult(nsresult aResultCode) MOZ_OVERRIDE;
};

class CreateObjectStoreOp MOZ_FINAL
  : public VersionChangeTransactionOp
{
  friend class VersionChangeTransaction;

  const ObjectStoreMetadata mMetadata;

private:
  // Only created by VersionChangeTransaction.
  CreateObjectStoreOp(VersionChangeTransaction* aTransaction,
                      const ObjectStoreMetadata& aMetadata)
    : VersionChangeTransactionOp(aTransaction)
    , mMetadata(aMetadata)
  {
    MOZ_ASSERT(aMetadata.id());
  }

  ~CreateObjectStoreOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;
};

class DeleteObjectStoreOp MOZ_FINAL
  : public VersionChangeTransactionOp
{
  friend class VersionChangeTransaction;

  const nsRefPtr<FullObjectStoreMetadata> mMetadata;

private:
  // Only created by VersionChangeTransaction.
  DeleteObjectStoreOp(VersionChangeTransaction* aTransaction,
                      FullObjectStoreMetadata* const aMetadata)
    : VersionChangeTransactionOp(aTransaction)
    , mMetadata(aMetadata)
  {
    MOZ_ASSERT(aMetadata->mCommonMetadata.id());
  }

  ~DeleteObjectStoreOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;
};

class CreateIndexOp MOZ_FINAL
  : public VersionChangeTransactionOp
{
  friend class VersionChangeTransaction;

  class ThreadLocalJSRuntime;
  friend class ThreadLocalJSRuntime;

  static const unsigned int kBadThreadLocalIndex =
    static_cast<unsigned int>(-1);

  static unsigned int sThreadLocalIndex;

  const IndexMetadata mMetadata;
  Maybe<UniqueIndexTable> mMaybeUniqueIndexTable;
  nsRefPtr<FileManager> mFileManager;
  const nsCString mDatabaseId;
  const uint64_t mObjectStoreId;

private:
  // Only created by VersionChangeTransaction.
  CreateIndexOp(VersionChangeTransaction* aTransaction,
                const int64_t aObjectStoreId,
                const IndexMetadata& aMetadata);

  ~CreateIndexOp()
  { }

  static void
  InitThreadLocals();

  nsresult
  InsertDataFromObjectStore(TransactionBase* aTransaction);

  virtual bool
  Init(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;
};

class CreateIndexOp::ThreadLocalJSRuntime MOZ_FINAL
{
  friend class CreateIndexOp;
  friend class nsAutoPtr<ThreadLocalJSRuntime>;

  static const JSClass kGlobalClass;
  static const uint32_t kRuntimeHeapSize = 768 * 1024;

  JSRuntime* mRuntime;
  JSContext* mContext;
  JSObject* mGlobal;

public:
  static ThreadLocalJSRuntime*
  GetOrCreate();

  JSContext*
  Context() const
  {
    return mContext;
  }

  JSObject*
  Global() const
  {
    return mGlobal;
  }

private:
  ThreadLocalJSRuntime()
    : mRuntime(nullptr)
    , mContext(nullptr)
    , mGlobal(nullptr)
  {
    MOZ_COUNT_CTOR(CreateIndexOp::ThreadLocalJSRuntime);
  }

  ~ThreadLocalJSRuntime()
  {
    MOZ_COUNT_DTOR(CreateIndexOp::ThreadLocalJSRuntime);

    if (mContext) {
      JS_DestroyContext(mContext);
    }

    if (mRuntime) {
      JS_DestroyRuntime(mRuntime);
    }
  }

  bool
  Init();
};

class DeleteIndexOp MOZ_FINAL
  : public VersionChangeTransactionOp
{
  friend class VersionChangeTransaction;

  const int64_t mIndexId;

private:
  // Only created by VersionChangeTransaction.
  DeleteIndexOp(VersionChangeTransaction* aTransaction,
                const int64_t aIndexId)
    : VersionChangeTransactionOp(aTransaction)
    , mIndexId(aIndexId)
  {
    MOZ_ASSERT(aIndexId);
  }

  ~DeleteIndexOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;
};

class NormalTransactionOp
  : public TransactionDatabaseOperationBase
  , public PBackgroundIDBRequestParent
{
  DebugOnly<bool> mResponseSent;

public:
  virtual void
  Cleanup() MOZ_OVERRIDE;

protected:
  explicit NormalTransactionOp(TransactionBase* aTransaction)
    : TransactionDatabaseOperationBase(aTransaction)
    , mResponseSent(false)
  { }

  virtual
  ~NormalTransactionOp()
  { }

  // Subclasses use this override to set the IPDL response value.
  virtual void
  GetResponse(RequestResponse& aResponse) = 0;

private:
  virtual nsresult
  SendSuccessResult() MOZ_OVERRIDE;

  virtual bool
  SendFailureResult(nsresult aResultCode) MOZ_OVERRIDE;

  // IPDL methods.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
};

class ObjectStoreAddOrPutRequestOp MOZ_FINAL
  : public NormalTransactionOp
{
  friend class TransactionBase;

  typedef mozilla::dom::quota::PersistenceType PersistenceType;

  struct StoredFileInfo;

  const ObjectStoreAddPutParams mParams;
  Maybe<UniqueIndexTable> mUniqueIndexTable;

  // This must be non-const so that we can update the mNextAutoIncrementId field
  // if we are modifying an autoIncrement objectStore.
  nsRefPtr<FullObjectStoreMetadata> mMetadata;

  FallibleTArray<StoredFileInfo> mStoredFileInfos;

  nsRefPtr<FileManager> mFileManager;

  Key mResponse;
  const nsCString mGroup;
  const nsCString mOrigin;
  const PersistenceType mPersistenceType;
  const bool mOverwrite;

private:
  // Only created by TransactionBase.
  ObjectStoreAddOrPutRequestOp(TransactionBase* aTransaction,
                               const RequestParams& aParams);

  ~ObjectStoreAddOrPutRequestOp()
  { }

  nsresult
  CopyFileData(nsIInputStream* aInputStream, nsIOutputStream* aOutputStream);

  virtual bool
  Init(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE;

  virtual void
  Cleanup() MOZ_OVERRIDE;
};

struct ObjectStoreAddOrPutRequestOp::StoredFileInfo MOZ_FINAL
{
  nsRefPtr<DatabaseFile> mFileActor;
  nsRefPtr<FileInfo> mFileInfo;
  nsCOMPtr<nsIInputStream> mInputStream;
  bool mCopiedSuccessfully;

  StoredFileInfo()
    : mCopiedSuccessfully(false)
  {
    AssertIsOnBackgroundThread();

    MOZ_COUNT_CTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo);
  }

  ~StoredFileInfo()
  {
    AssertIsOnBackgroundThread();

    MOZ_COUNT_DTOR(ObjectStoreAddOrPutRequestOp::StoredFileInfo);
  }
};

class ObjectStoreGetRequestOp MOZ_FINAL
  : public NormalTransactionOp
{
  friend class TransactionBase;

  const uint32_t mObjectStoreId;
  nsRefPtr<FileManager> mFileManager;
  const OptionalKeyRange mOptionalKeyRange;
  AutoFallibleTArray<StructuredCloneReadInfo, 1> mResponse;
  PBackgroundParent* mBackgroundParent;
  const uint32_t mLimit;
  const bool mGetAll;

private:
  // Only created by TransactionBase.
  ObjectStoreGetRequestOp(TransactionBase* aTransaction,
                          const RequestParams& aParams,
                          bool aGetAll);

  ~ObjectStoreGetRequestOp()
  { }

  nsresult
  ConvertResponse(uint32_t aIndex,
                  SerializedStructuredCloneReadInfo& aSerializedInfo);

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE;
};

class ObjectStoreGetAllKeysRequestOp MOZ_FINAL
  : public NormalTransactionOp
{
  friend class TransactionBase;

  const ObjectStoreGetAllKeysParams mParams;
  FallibleTArray<Key> mResponse;

private:
  // Only created by TransactionBase.
  ObjectStoreGetAllKeysRequestOp(TransactionBase* aTransaction,
                                 const ObjectStoreGetAllKeysParams& aParams)
    : NormalTransactionOp(aTransaction)
    , mParams(aParams)
  { }

  ~ObjectStoreGetAllKeysRequestOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE;
};

class ObjectStoreDeleteRequestOp MOZ_FINAL
  : public NormalTransactionOp
{
  friend class TransactionBase;

  const ObjectStoreDeleteParams mParams;
  ObjectStoreDeleteResponse mResponse;

private:
  ObjectStoreDeleteRequestOp(TransactionBase* aTransaction,
                             const ObjectStoreDeleteParams& aParams)
    : NormalTransactionOp(aTransaction)
    , mParams(aParams)
  { }

  ~ObjectStoreDeleteRequestOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE
  {
    aResponse = Move(mResponse);
  }
};

class ObjectStoreClearRequestOp MOZ_FINAL
  : public NormalTransactionOp
{
  friend class TransactionBase;

  const ObjectStoreClearParams mParams;
  ObjectStoreClearResponse mResponse;

private:
  ObjectStoreClearRequestOp(TransactionBase* aTransaction,
                            const ObjectStoreClearParams& aParams)
    : NormalTransactionOp(aTransaction)
    , mParams(aParams)
  { }

  ~ObjectStoreClearRequestOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE
  {
    aResponse = Move(mResponse);
  }
};

class ObjectStoreCountRequestOp MOZ_FINAL
  : public NormalTransactionOp
{
  friend class TransactionBase;

  const ObjectStoreCountParams mParams;
  ObjectStoreCountResponse mResponse;

private:
  ObjectStoreCountRequestOp(TransactionBase* aTransaction,
                            const ObjectStoreCountParams& aParams)
    : NormalTransactionOp(aTransaction)
    , mParams(aParams)
  { }

  ~ObjectStoreCountRequestOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE
  {
    aResponse = Move(mResponse);
  }
};

class IndexRequestOpBase
  : public NormalTransactionOp
{
protected:
  const nsRefPtr<FullIndexMetadata> mMetadata;

protected:
  IndexRequestOpBase(TransactionBase* aTransaction,
                     const RequestParams& aParams)
    : NormalTransactionOp(aTransaction)
    , mMetadata(IndexMetadataForParams(aTransaction, aParams))
  { }

  virtual
  ~IndexRequestOpBase()
  { }

private:
  static already_AddRefed<FullIndexMetadata>
  IndexMetadataForParams(TransactionBase* aTransaction,
                         const RequestParams& aParams);
};

class IndexGetRequestOp MOZ_FINAL
  : public IndexRequestOpBase
{
  friend class TransactionBase;

  nsRefPtr<FileManager> mFileManager;
  const OptionalKeyRange mOptionalKeyRange;
  AutoFallibleTArray<StructuredCloneReadInfo, 1> mResponse;
  PBackgroundParent* mBackgroundParent;
  const uint32_t mLimit;
  const bool mGetAll;

private:
  // Only created by TransactionBase.
  IndexGetRequestOp(TransactionBase* aTransaction,
                    const RequestParams& aParams,
                    bool aGetAll);

  ~IndexGetRequestOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE;
};

class IndexGetKeyRequestOp MOZ_FINAL
  : public IndexRequestOpBase
{
  friend class TransactionBase;

  const OptionalKeyRange mOptionalKeyRange;
  AutoFallibleTArray<Key, 1> mResponse;
  const uint32_t mLimit;
  const bool mGetAll;

private:
  // Only created by TransactionBase.
  IndexGetKeyRequestOp(TransactionBase* aTransaction,
                       const RequestParams& aParams,
                       bool aGetAll);

  ~IndexGetKeyRequestOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE;
};

class IndexCountRequestOp MOZ_FINAL
  : public IndexRequestOpBase
{
  friend class TransactionBase;

  const IndexCountParams mParams;
  IndexCountResponse mResponse;

private:
  // Only created by TransactionBase.
  IndexCountRequestOp(TransactionBase* aTransaction,
                      const RequestParams& aParams)
    : IndexRequestOpBase(aTransaction, aParams)
    , mParams(aParams.get_IndexCountParams())
  { }

  ~IndexCountRequestOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual void
  GetResponse(RequestResponse& aResponse) MOZ_OVERRIDE
  {
    aResponse = Move(mResponse);
  }
};

class Cursor MOZ_FINAL :
    public PBackgroundIDBCursorParent
{
  friend class TransactionBase;

  class ContinueOp;
  class CursorOpBase;
  class OpenOp;

public:
  typedef OpenCursorParams::Type Type;

private:
  nsRefPtr<TransactionBase> mTransaction;
  nsRefPtr<FileManager> mFileManager;
  PBackgroundParent* mBackgroundParent;

  const int64_t mObjectStoreId;
  const int64_t mIndexId;

  nsCString mContinueQuery;
  nsCString mContinueToQuery;

  Key mKey;
  Key mObjectKey;
  Key mRangeKey;

  CursorOpBase* mCurrentlyRunningOp;

  const Type mType;
  const Direction mDirection;

  bool mUniqueIndex;
  bool mActorDestroyed;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::indexedDB::Cursor)

private:
  // Only created by TransactionBase.
  Cursor(TransactionBase* aTransaction,
         Type aType,
         int64_t aObjectStoreId,
         int64_t aIndexId,
         Direction aDirection);

  // Reference counted.
  ~Cursor()
  {
    MOZ_ASSERT(mActorDestroyed);
  }

  // Only called by TransactionBase.
  bool
  Start(const OpenCursorParams& aParams);

  void
  SendResponseInternal(CursorResponse& aResponse,
                       const nsTArray<StructuredCloneFile>& aFiles);

  // Must call SendResponseInternal!
  bool
  SendResponse(const CursorResponse& aResponse) = delete;

  // IPDL methods.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvDeleteMe() MOZ_OVERRIDE;

  virtual bool
  RecvContinue(const CursorRequestParams& aParams) MOZ_OVERRIDE;
};

class Cursor::CursorOpBase
  : public TransactionDatabaseOperationBase
{
protected:
  nsRefPtr<Cursor> mCursor;
  FallibleTArray<StructuredCloneFile> mFiles;

  CursorResponse mResponse;

  DebugOnly<bool> mResponseSent;

protected:
  explicit CursorOpBase(Cursor* aCursor)
    : TransactionDatabaseOperationBase(aCursor->mTransaction)
    , mCursor(aCursor)
    , mResponseSent(false)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aCursor);
  }

  virtual
  ~CursorOpBase()
  { }

  virtual bool
  SendFailureResult(nsresult aResultCode) MOZ_OVERRIDE;

  virtual void
  Cleanup() MOZ_OVERRIDE;
};

class Cursor::OpenOp MOZ_FINAL
  : public Cursor::CursorOpBase
{
  friend class Cursor;

  const OptionalKeyRange mOptionalKeyRange;

private:
  // Only created by Cursor.
  OpenOp(Cursor* aCursor,
         const OptionalKeyRange& aOptionalKeyRange)
    : CursorOpBase(aCursor)
    , mOptionalKeyRange(aOptionalKeyRange)
  { }

  // Reference counted.
  ~OpenOp()
  { }

  void
  GetRangeKeyInfo(bool aLowerBound, Key* aKey, bool* aOpen);

  nsresult
  DoObjectStoreDatabaseWork(TransactionBase* aTransaction);

  nsresult
  DoObjectStoreKeyDatabaseWork(TransactionBase* aTransaction);

  nsresult
  DoIndexDatabaseWork(TransactionBase* aTransaction);

  nsresult
  DoIndexKeyDatabaseWork(TransactionBase* aTransaction);

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual nsresult
  SendSuccessResult() MOZ_OVERRIDE;
};

class Cursor::ContinueOp MOZ_FINAL
  : public Cursor::CursorOpBase
{
  friend class Cursor;

  const CursorRequestParams mParams;

private:
  // Only created by Cursor.
  ContinueOp(Cursor* aCursor, const CursorRequestParams& aParams)
    : CursorOpBase(aCursor)
    , mParams(aParams)
  {
    MOZ_ASSERT(aParams.type() != CursorRequestParams::T__None);
  }

  // Reference counted.
  ~ContinueOp()
  { }

  virtual nsresult
  DoDatabaseWork(TransactionBase* aTransaction) MOZ_OVERRIDE;

  virtual nsresult
  SendSuccessResult() MOZ_OVERRIDE;
};

class PermissionRequestHelper MOZ_FINAL
  : public PermissionRequestBase
  , public PIndexedDBPermissionRequestParent
{
  bool mActorDestroyed;

public:
  PermissionRequestHelper(nsPIDOMWindow* aWindow,
                          nsIPrincipal* aPrincipal)
    : PermissionRequestBase(aWindow, aPrincipal)
    , mActorDestroyed(false)
  { }

protected:
  ~PermissionRequestHelper()
  { }

private:
  virtual void
  OnPromptComplete(PermissionValue aPermissionValue) MOZ_OVERRIDE;

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

struct DatabaseActorInfo MOZ_FINAL
{
  friend class nsAutoPtr<DatabaseActorInfo>;

  nsRefPtr<FullDatabaseMetadata> mMetadata;
  nsTArray<Database*> mLiveDatabases;
  nsRefPtr<FactoryOp> mWaitingFactoryOp;

  DatabaseActorInfo(FullDatabaseMetadata* aMetadata,
                    Database* aDatabase)
    : mMetadata(aMetadata)
  {
    MOZ_ASSERT(aDatabase);

    MOZ_COUNT_CTOR(DatabaseActorInfo);

    mLiveDatabases.AppendElement(aDatabase);
  }

private:
  ~DatabaseActorInfo()
  {
    MOZ_ASSERT(mLiveDatabases.IsEmpty());
    MOZ_ASSERT(!mWaitingFactoryOp ||
               !mWaitingFactoryOp->HasBlockedDatabases());

    MOZ_COUNT_DTOR(DatabaseActorInfo);
  }
};

class DatabaseLoggingInfo MOZ_FINAL
{
#ifdef DEBUG
  // Just for potential warnings.
  friend class Factory;
#endif

  LoggingInfo mLoggingInfo;

public:
  explicit DatabaseLoggingInfo(const LoggingInfo& aLoggingInfo)
    : mLoggingInfo(aLoggingInfo)
  {
    AssertIsOnBackgroundThread();
  }

  const nsID&
  Id() const
  {
    AssertIsOnBackgroundThread();

    return mLoggingInfo.backgroundChildLoggingId();
  }

  int64_t
  NextTransactionSN(IDBTransaction::Mode aMode)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo.nextTransactionSerialNumber() < INT64_MAX);
    MOZ_ASSERT(mLoggingInfo.nextVersionChangeTransactionSerialNumber() >
                 INT64_MIN);

    if (aMode == IDBTransaction::VERSION_CHANGE) {
      return mLoggingInfo.nextVersionChangeTransactionSerialNumber()--;
    }

    return mLoggingInfo.nextTransactionSerialNumber()++;
  }

  uint64_t
  NextRequestSN()
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mLoggingInfo.nextRequestSerialNumber() < UINT64_MAX);

    return mLoggingInfo.nextRequestSerialNumber()++;
  }

  NS_INLINE_DECL_REFCOUNTING(DatabaseLoggingInfo)

private:
  ~DatabaseLoggingInfo();
};

class NonMainThreadHackBlobImpl MOZ_FINAL
  : public FileImplFile
{
public:
  NonMainThreadHackBlobImpl(nsIFile* aFile, FileInfo* aFileInfo)
    : FileImplFile(aFile, aFileInfo)
  {
    // Getting the content type is not currently supported off the main thread.
    // This isn't a problem here because:
    //
    //   1. The real content type is stored in the structured clone data and
    //      that's all that the DOM will see. This blob's data will be updated
    //      during RecvSetMysteryBlobInfo().
    //   2. The nsExternalHelperAppService guesses the content type based only
    //      on the file extension. Our stored files have no extension so the
    //      current code path fails and sets the content type to the empty
    //      string.
    //
    // So, this is a hack to keep the nsExternalHelperAppService out of the
    // picture entirely. Eventually we should probably fix this some other way.
    mContentType.Truncate();
  }

private:
  ~NonMainThreadHackBlobImpl()
  { }
};

class QuotaClient MOZ_FINAL
  : public mozilla::dom::quota::Client
{
  class ShutdownTransactionThreadPoolRunnable;
  friend class ShutdownTransactionThreadPoolRunnable;

  class WaitForTransactionsRunnable;
  friend class WaitForTransactionsRunnable;

  static QuotaClient* sInstance;

  nsCOMPtr<nsIEventTarget> mBackgroundThread;
  nsRefPtr<ShutdownTransactionThreadPoolRunnable> mShutdownRunnable;

  bool mShutdownRequested;

public:
  QuotaClient();

  static QuotaClient*
  GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sInstance;
  }

  static bool
  IsShuttingDownOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (sInstance) {
      return sInstance->mShutdownRequested;
    }

    return QuotaManager::IsShuttingDown();
  }

  static bool
  IsShuttingDownOnNonMainThread()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    return QuotaManager::IsShuttingDown();
  }

  bool
  IsShuttingDown() const
  {
    MOZ_ASSERT(NS_IsMainThread());

    return mShutdownRequested;
  }

  void
  NoteBackgroundThread(nsIEventTarget* aBackgroundThread);

  NS_INLINE_DECL_REFCOUNTING(QuotaClient, MOZ_OVERRIDE)

  virtual mozilla::dom::quota::Client::Type
  GetType() MOZ_OVERRIDE;

  virtual nsresult
  InitOrigin(PersistenceType aPersistenceType,
             const nsACString& aGroup,
             const nsACString& aOrigin,
             UsageInfo* aUsageInfo) MOZ_OVERRIDE;

  virtual nsresult
  GetUsageForOrigin(PersistenceType aPersistenceType,
                    const nsACString& aGroup,
                    const nsACString& aOrigin,
                    UsageInfo* aUsageInfo) MOZ_OVERRIDE;

  virtual void
  OnOriginClearCompleted(PersistenceType aPersistenceType,
                         const nsACString& aOrigin)
                         MOZ_OVERRIDE;

  virtual void
  ReleaseIOThreadObjects() MOZ_OVERRIDE;

  virtual bool
  IsFileServiceUtilized() MOZ_OVERRIDE;

  virtual bool
  IsTransactionServiceActivated() MOZ_OVERRIDE;

  virtual void
  WaitForStoragesToComplete(nsTArray<nsIOfflineStorage*>& aStorages,
                            nsIRunnable* aCallback) MOZ_OVERRIDE;

  virtual void
  ShutdownTransactionService() MOZ_OVERRIDE;

private:
  ~QuotaClient();

  nsresult
  GetDirectory(PersistenceType aPersistenceType,
               const nsACString& aOrigin,
               nsIFile** aDirectory);

  nsresult
  GetUsageForDirectoryInternal(nsIFile* aDirectory,
                               UsageInfo* aUsageInfo,
                               bool aDatabaseFiles);
};

class QuotaClient::ShutdownTransactionThreadPoolRunnable MOZ_FINAL
  : public nsRunnable
{
  nsRefPtr<QuotaClient> mQuotaClient;
  bool mHasRequestedShutDown;

public:

  explicit ShutdownTransactionThreadPoolRunnable(QuotaClient* aQuotaClient)
    : mQuotaClient(aQuotaClient)
    , mHasRequestedShutDown(false)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aQuotaClient);
    MOZ_ASSERT(QuotaClient::GetInstance() == aQuotaClient);
    MOZ_ASSERT(aQuotaClient->mShutdownRequested);
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~ShutdownTransactionThreadPoolRunnable()
  {
    MOZ_ASSERT(!mQuotaClient);
  }

  NS_DECL_NSIRUNNABLE
};

class QuotaClient::WaitForTransactionsRunnable MOZ_FINAL
  : public nsRunnable
{
  nsRefPtr<QuotaClient> mQuotaClient;
  nsTArray<nsCString> mDatabaseIds;
  nsCOMPtr<nsIRunnable> mCallback;

  enum
  {
    State_Initial = 0,
    State_WaitingForTransactions,
    State_CallingCallback,
    State_Complete
  } mState;

public:
  WaitForTransactionsRunnable(QuotaClient* aQuotaClient,
                              nsTArray<nsCString>& aDatabaseIds,
                              nsIRunnable* aCallback)
    : mQuotaClient(aQuotaClient)
    , mCallback(aCallback)
    , mState(State_Initial)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aQuotaClient);
    MOZ_ASSERT(QuotaClient::GetInstance() == aQuotaClient);
    MOZ_ASSERT(!aDatabaseIds.IsEmpty());
    MOZ_ASSERT(aCallback);

    mDatabaseIds.SwapElements(aDatabaseIds);
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~WaitForTransactionsRunnable()
  {
    MOZ_ASSERT(!mQuotaClient);
    MOZ_ASSERT(!mCallback);
    MOZ_ASSERT(mState = State_Complete);
  }

  void
  MaybeWait();

  void
  SendToMainThread();

  void
  CallCallback();

  NS_DECL_NSIRUNNABLE
};

class DatabaseOfflineStorage MOZ_FINAL
  : public nsIOfflineStorage
{
  // Must be released on the main thread!
  nsRefPtr<QuotaClient> mStrongQuotaClient;

  // Only used on the main thread.
  QuotaClient* mWeakQuotaClient;

  // Only used on the background thread.
  Database* mDatabase;

  const OptionalContentId mOptionalContentParentId;
  const nsCString mOrigin;
  const nsCString mId;
  nsCOMPtr<nsIEventTarget> mOwningThread;

  bool mClosedOnMainThread;
  bool mClosedOnOwningThread;
  bool mInvalidatedOnMainThread;
  bool mInvalidatedOnOwningThread;

  DebugOnly<bool> mRegisteredWithQuotaManager;

public:
  DatabaseOfflineStorage(QuotaClient* aQuotaClient,
                         const OptionalContentId& aOptionalContentParentId,
                         const nsACString& aGroup,
                         const nsACString& aOrigin,
                         const nsACString& aId,
                         PersistenceType aPersistenceType,
                         nsIEventTarget* aOwningThread);

  static void
  UnregisterOnOwningThread(
                      already_AddRefed<DatabaseOfflineStorage> aOfflineStorage);

  void
  SetDatabase(Database* aDatabase)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aDatabase);
    MOZ_ASSERT(!mDatabase);

    mDatabase = aDatabase;
  }

  nsIEventTarget*
  OwningThread() const
  {
    return mOwningThread;
  }

  void
  NoteRegisteredWithQuotaManager()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mRegisteredWithQuotaManager);

    mRegisteredWithQuotaManager = true;
  }

  void
  CloseOnOwningThread();

  void
  AssertInvalidatedOnMainThread() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mInvalidatedOnMainThread);
  }

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  ~DatabaseOfflineStorage()
  {
    MOZ_ASSERT(!mDatabase);
    MOZ_ASSERT(!mRegisteredWithQuotaManager);
  }

  void
  CloseOnMainThread();

  void
  InvalidateOnMainThread();

  void
  InvalidateOnOwningThread();

  void
  UnregisterOnMainThread();

  NS_DECL_NSIOFFLINESTORAGE
};

#ifdef DEBUG

class DEBUGThreadSlower MOZ_FINAL
  : public nsIThreadObserver
{
public:
  DEBUGThreadSlower()
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(kDEBUGThreadSleepMS);
  }

  NS_DECL_ISUPPORTS

private:
  ~DEBUGThreadSlower()
  {
    AssertIsOnBackgroundThread();
  }

  NS_DECL_NSITHREADOBSERVER
};

#endif // DEBUG

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

bool
TokenizerIgnoreNothing(char16_t /* aChar */)
{
  return false;
}

nsresult
ConvertFileIdsToArray(const nsAString& aFileIds,
                      nsTArray<int64_t>& aResult)
{
  nsCharSeparatedTokenizerTemplate<TokenizerIgnoreNothing>
    tokenizer(aFileIds, ' ');

  nsAutoString token;
  nsresult rv;

  while (tokenizer.hasMoreTokens()) {
    token = tokenizer.nextToken();
    MOZ_ASSERT(!token.IsEmpty());

    int32_t id = token.ToInteger(&rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aResult.AppendElement(id);
  }

  return NS_OK;
}

bool
GetDatabaseBaseFilename(const nsAString& aFilename,
                        nsDependentSubstring& aDatabaseBaseFilename)
{
  MOZ_ASSERT(!aFilename.IsEmpty());
  MOZ_ASSERT(aDatabaseBaseFilename.IsEmpty());

  NS_NAMED_LITERAL_STRING(sqlite, ".sqlite");

  if (!StringEndsWith(aFilename, sqlite) ||
      aFilename.Length() == sqlite.Length()) {
    return false;
  }

  MOZ_ASSERT(aFilename.Length() > sqlite.Length());

  aDatabaseBaseFilename.Rebind(aFilename,
                               0,
                               aFilename.Length() - sqlite.Length());
  return true;
}

nsresult
ConvertBlobsToActors(PBackgroundParent* aBackgroundActor,
                     FileManager* aFileManager,
                     const nsTArray<StructuredCloneFile>& aFiles,
                     FallibleTArray<PBlobParent*>& aActors,
                     FallibleTArray<intptr_t>& aFileInfos)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(aFileManager);
  MOZ_ASSERT(aActors.IsEmpty());
  MOZ_ASSERT(aFileInfos.IsEmpty());

  if (aFiles.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> directory = aFileManager->GetDirectory();
  if (NS_WARN_IF(!directory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(directory->Exists(&exists)));
  MOZ_ASSERT(exists);

  DebugOnly<bool> isDirectory;
  MOZ_ASSERT(NS_SUCCEEDED(directory->IsDirectory(&isDirectory)));
  MOZ_ASSERT(isDirectory);

  const uint32_t count = aFiles.Length();

  if (NS_WARN_IF(!aActors.SetCapacity(count))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const bool collectFileInfos =
    !BackgroundParent::IsOtherProcessActor(aBackgroundActor);

  if (collectFileInfos && NS_WARN_IF(!aFileInfos.SetCapacity(count))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t index = 0; index < count; index++) {
    const StructuredCloneFile& file = aFiles[index];

    const int64_t fileId = file.mFileInfo->Id();
    MOZ_ASSERT(fileId > 0);

    nsCOMPtr<nsIFile> nativeFile =
      aFileManager->GetFileForId(directory, fileId);
    if (NS_WARN_IF(!nativeFile)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    MOZ_ASSERT(NS_SUCCEEDED(nativeFile->Exists(&exists)));
    MOZ_ASSERT(exists);

    DebugOnly<bool> isFile;
    MOZ_ASSERT(NS_SUCCEEDED(nativeFile->IsFile(&isFile)));
    MOZ_ASSERT(isFile);

    nsRefPtr<FileImpl> impl =
      new NonMainThreadHackBlobImpl(nativeFile, file.mFileInfo);

    PBlobParent* actor =
      BackgroundParent::GetOrCreateActorForBlobImpl(aBackgroundActor, impl);
    if (!actor) {
      // This can only fail if the child has crashed.
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    MOZ_ALWAYS_TRUE(aActors.AppendElement(actor));

    if (collectFileInfos) {
      nsRefPtr<FileInfo> fileInfo = file.mFileInfo;

      // Transfer a reference to the receiver.
      auto transferedFileInfo =
        reinterpret_cast<intptr_t>(fileInfo.forget().take());
      MOZ_ALWAYS_TRUE(aFileInfos.AppendElement(transferedFileInfo));
    }
  }

  return NS_OK;
}

/*******************************************************************************
 * Globals
 ******************************************************************************/

// Maps a database id to information about live database actors.
typedef nsClassHashtable<nsCStringHashKey, DatabaseActorInfo>
        DatabaseActorHashtable;

StaticAutoPtr<DatabaseActorHashtable> gLiveDatabaseHashtable;

StaticRefPtr<nsRunnable> gStartTransactionRunnable;

StaticRefPtr<TransactionThreadPool> gTransactionThreadPool;

typedef nsDataHashtable<nsIDHashKey, DatabaseLoggingInfo*>
        DatabaseLoggingInfoHashtable;

StaticAutoPtr<DatabaseLoggingInfoHashtable> gLoggingInfoHashtable;

#ifdef DEBUG

StaticRefPtr<DEBUGThreadSlower> gDEBUGThreadSlower;

#endif // DEBUG

} // anonymous namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

PBackgroundIDBFactoryParent*
AllocPBackgroundIDBFactoryParent(const LoggingInfo& aLoggingInfo)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread())) {
    return nullptr;
  }

  nsRefPtr<Factory> actor = Factory::Create(aLoggingInfo);
  MOZ_ASSERT(actor);

  return actor.forget().take();
}

bool
RecvPBackgroundIDBFactoryConstructor(PBackgroundIDBFactoryParent* aActor,
                                     const LoggingInfo& /* aLoggingInfo */)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnNonMainThread());

  return true;
}

bool
DeallocPBackgroundIDBFactoryParent(PBackgroundIDBFactoryParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  nsRefPtr<Factory> actor = dont_AddRef(static_cast<Factory*>(aActor));
  return true;
}

PIndexedDBPermissionRequestParent*
AllocPIndexedDBPermissionRequestParent(nsPIDOMWindow* aWindow,
                                       nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<PermissionRequestHelper> actor =
    new PermissionRequestHelper(aWindow, aPrincipal);
  return actor.forget().take();
}

bool
RecvPIndexedDBPermissionRequestConstructor(
                                      PIndexedDBPermissionRequestParent* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  auto* actor = static_cast<PermissionRequestHelper*>(aActor);

  PermissionRequestBase::PermissionValue permission;
  nsresult rv = actor->PromptIfNeeded(&permission);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (permission != PermissionRequestBase::kPermissionPrompt) {
    unused <<
      PIndexedDBPermissionRequestParent::Send__delete__(actor, permission);
  }

  return true;
}

bool
DeallocPIndexedDBPermissionRequestParent(
                                      PIndexedDBPermissionRequestParent* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  nsRefPtr<PermissionRequestHelper> actor =
    dont_AddRef(static_cast<PermissionRequestHelper*>(aActor));
  return true;
}

already_AddRefed<mozilla::dom::quota::Client>
CreateQuotaClient()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<QuotaClient> client = new QuotaClient();
  return client.forget();
}

/*******************************************************************************
 * Metadata classes
 ******************************************************************************/

already_AddRefed<FullDatabaseMetadata>
FullDatabaseMetadata::Duplicate() const
{
  AssertIsOnBackgroundThread();

  class MOZ_STACK_CLASS IndexClosure MOZ_FINAL
  {
    FullObjectStoreMetadata& mNew;

  public:
    explicit IndexClosure(FullObjectStoreMetadata& aNew)
      : mNew(aNew)
    { }

    static PLDHashOperator
    Copy(const uint64_t& aKey, FullIndexMetadata* aValue, void* aClosure)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);
      MOZ_ASSERT(aClosure);

      auto* closure = static_cast<IndexClosure*>(aClosure);

      nsRefPtr<FullIndexMetadata> newMetadata = new FullIndexMetadata();

      newMetadata->mCommonMetadata = aValue->mCommonMetadata;

      if (NS_WARN_IF(!closure->mNew.mIndexes.Put(aKey, newMetadata,
                                                 fallible))) {
        return PL_DHASH_STOP;
      }

      return PL_DHASH_NEXT;
    }
  };

  class MOZ_STACK_CLASS ObjectStoreClosure MOZ_FINAL
  {
    FullDatabaseMetadata& mNew;

  public:
    explicit ObjectStoreClosure(FullDatabaseMetadata& aNew)
      : mNew(aNew)
    { }

    static PLDHashOperator
    Copy(const uint64_t& aKey, FullObjectStoreMetadata* aValue, void* aClosure)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);
      MOZ_ASSERT(aClosure);

      auto* objClosure = static_cast<ObjectStoreClosure*>(aClosure);

      nsRefPtr<FullObjectStoreMetadata> newMetadata =
        new FullObjectStoreMetadata();

      newMetadata->mCommonMetadata = aValue->mCommonMetadata;
      newMetadata->mNextAutoIncrementId = aValue->mNextAutoIncrementId;
      newMetadata->mComittedAutoIncrementId = aValue->mComittedAutoIncrementId;

      IndexClosure idxClosure(*newMetadata);
      aValue->mIndexes.EnumerateRead(IndexClosure::Copy, &idxClosure);

      if (NS_WARN_IF(aValue->mIndexes.Count() !=
                     newMetadata->mIndexes.Count())) {
        return PL_DHASH_STOP;
      }

      if (NS_WARN_IF(!objClosure->mNew.mObjectStores.Put(aKey, newMetadata,
                                                         fallible))) {
        return PL_DHASH_STOP;
      }

      return PL_DHASH_NEXT;
    }
  };

  // FullDatabaseMetadata contains two hash tables of pointers that we need to
  // duplicate so we can't just use the copy constructor.
  nsRefPtr<FullDatabaseMetadata> newMetadata =
    new FullDatabaseMetadata(mCommonMetadata);

  newMetadata->mDatabaseId = mDatabaseId;
  newMetadata->mFilePath = mFilePath;
  newMetadata->mNextObjectStoreId = mNextObjectStoreId;
  newMetadata->mNextIndexId = mNextIndexId;

  ObjectStoreClosure closure(*newMetadata);
  mObjectStores.EnumerateRead(ObjectStoreClosure::Copy, &closure);

  if (NS_WARN_IF(mObjectStores.Count() !=
                 newMetadata->mObjectStores.Count())) {
    return nullptr;
  }

  return newMetadata.forget();
}

DatabaseLoggingInfo::~DatabaseLoggingInfo()
{
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

uint64_t Factory::sFactoryInstanceCount = 0;

Factory::Factory(already_AddRefed<DatabaseLoggingInfo> aLoggingInfo)
  : mLoggingInfo(Move(aLoggingInfo))
  , mActorDestroyed(false)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnNonMainThread());
}

Factory::~Factory()
{
  MOZ_ASSERT(mActorDestroyed);
}

// static
already_AddRefed<Factory>
Factory::Create(const LoggingInfo& aLoggingInfo)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnNonMainThread());

  // If this is the first instance then we need to do some initialization.
  if (!sFactoryInstanceCount) {
    if (!gTransactionThreadPool) {
      nsRefPtr<TransactionThreadPool> threadPool =
        TransactionThreadPool::Create();
      if (NS_WARN_IF(!threadPool)) {
        return nullptr;
      }

      gTransactionThreadPool = threadPool;
    }

    MOZ_ASSERT(!gLiveDatabaseHashtable);
    gLiveDatabaseHashtable = new DatabaseActorHashtable();

    MOZ_ASSERT(!gStartTransactionRunnable);
    gStartTransactionRunnable = new nsRunnable();

    MOZ_ASSERT(!gLoggingInfoHashtable);
    gLoggingInfoHashtable = new DatabaseLoggingInfoHashtable();

#ifdef DEBUG
    if (kDEBUGThreadPriority != nsISupportsPriority::PRIORITY_NORMAL) {
      NS_WARNING("PBackground thread debugging enabled, priority has been "
                 "modified!");
      nsCOMPtr<nsISupportsPriority> thread =
        do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(thread->SetPriority(kDEBUGThreadPriority)));
    }

    if (kDEBUGThreadSleepMS) {
      NS_WARNING("PBackground thread debugging enabled, sleeping after every "
                 "event!");
      nsCOMPtr<nsIThreadInternal> thread =
        do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      gDEBUGThreadSlower = new DEBUGThreadSlower();

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(thread->AddObserver(gDEBUGThreadSlower)));
    }
#endif // DEBUG
  }

  nsRefPtr<DatabaseLoggingInfo> loggingInfo =
    gLoggingInfoHashtable->Get(aLoggingInfo.backgroundChildLoggingId());
  if (loggingInfo) {
    MOZ_ASSERT(aLoggingInfo.backgroundChildLoggingId() == loggingInfo->Id());
#if !DISABLE_ASSERTS_FOR_FUZZING
    NS_WARN_IF_FALSE(aLoggingInfo.nextTransactionSerialNumber() ==
                       loggingInfo->mLoggingInfo.nextTransactionSerialNumber(),
                     "NextTransactionSerialNumber doesn't match!");
    NS_WARN_IF_FALSE(aLoggingInfo.nextVersionChangeTransactionSerialNumber() ==
                       loggingInfo->mLoggingInfo.
                         nextVersionChangeTransactionSerialNumber(),
                     "NextVersionChangeTransactionSerialNumber doesn't match!");
    NS_WARN_IF_FALSE(aLoggingInfo.nextRequestSerialNumber() ==
                       loggingInfo->mLoggingInfo.nextRequestSerialNumber(),
                     "NextRequestSerialNumber doesn't match!");
#endif // !DISABLE_ASSERTS_FOR_FUZZING
  } else {
    loggingInfo = new DatabaseLoggingInfo(aLoggingInfo);
    gLoggingInfoHashtable->Put(aLoggingInfo.backgroundChildLoggingId(),
                               loggingInfo);
  }

  nsRefPtr<Factory> actor = new Factory(loggingInfo.forget());

  sFactoryInstanceCount++;

  return actor.forget();
}

void
Factory::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  // Clean up if there are no more instances.
  if (!(--sFactoryInstanceCount)) {
    MOZ_ASSERT(gLoggingInfoHashtable);
    gLoggingInfoHashtable = nullptr;

    MOZ_ASSERT(gStartTransactionRunnable);
    gStartTransactionRunnable = nullptr;

    MOZ_ASSERT(gLiveDatabaseHashtable);
    MOZ_ASSERT(!gLiveDatabaseHashtable->Count());
    gLiveDatabaseHashtable = nullptr;

#ifdef DEBUG
    if (kDEBUGThreadPriority != nsISupportsPriority::PRIORITY_NORMAL) {
      nsCOMPtr<nsISupportsPriority> thread =
        do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
        thread->SetPriority(nsISupportsPriority::PRIORITY_NORMAL)));
    }

    if (kDEBUGThreadSleepMS) {
      MOZ_ASSERT(gDEBUGThreadSlower);

      nsCOMPtr<nsIThreadInternal> thread =
        do_QueryInterface(NS_GetCurrentThread());
      MOZ_ASSERT(thread);

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(thread->RemoveObserver(gDEBUGThreadSlower)));

      gDEBUGThreadSlower = nullptr;
    }
#endif // DEBUG
  }
}

bool
Factory::RecvDeleteMe()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  return PBackgroundIDBFactoryParent::Send__delete__(this);
}

bool
Factory::RecvIncrementLoggingRequestSerialNumber()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mLoggingInfo);

  mLoggingInfo->NextRequestSN();
  return true;
}

PBackgroundIDBFactoryRequestParent*
Factory::AllocPBackgroundIDBFactoryRequestParent(
                                            const FactoryRequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != FactoryRequestParams::T__None);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread())) {
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
  if (NS_WARN_IF(metadata.persistenceType() != PERSISTENCE_TYPE_PERSISTENT &&
                 metadata.persistenceType() != PERSISTENCE_TYPE_TEMPORARY &&
                 metadata.persistenceType() != PERSISTENCE_TYPE_DEFAULT)) {
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

  nsRefPtr<ContentParent> contentParent =
    BackgroundParent::GetContentParent(Manager());

  nsRefPtr<FactoryOp> actor;
  if (aParams.type() == FactoryRequestParams::TOpenDatabaseRequestParams) {
    actor = new OpenDatabaseOp(this,
                               contentParent.forget(),
                               *commonParams);
  } else {
    actor = new DeleteDatabaseOp(this, contentParent.forget(), *commonParams);
  }

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool
Factory::RecvPBackgroundIDBFactoryRequestConstructor(
                                     PBackgroundIDBFactoryRequestParent* aActor,
                                     const FactoryRequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != FactoryRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnNonMainThread());

  auto* op = static_cast<FactoryOp*>(aActor);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(op)));
  return true;
}

bool
Factory::DeallocPBackgroundIDBFactoryRequestParent(
                                     PBackgroundIDBFactoryRequestParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  nsRefPtr<FactoryOp> op = dont_AddRef(static_cast<FactoryOp*>(aActor));
  return true;
}

PBackgroundIDBDatabaseParent*
Factory::AllocPBackgroundIDBDatabaseParent(
                                   const DatabaseSpec& aSpec,
                                   PBackgroundIDBFactoryRequestParent* aRequest)
{
  MOZ_CRASH("PBackgroundIDBDatabaseParent actors should be constructed "
            "manually!");
}

bool
Factory::DeallocPBackgroundIDBDatabaseParent(
                                           PBackgroundIDBDatabaseParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  nsRefPtr<Database> database = dont_AddRef(static_cast<Database*>(aActor));
  return true;
}

/*******************************************************************************
 * Database
 ******************************************************************************/

Database::Database(Factory* aFactory,
                   const PrincipalInfo& aPrincipalInfo,
                   const nsACString& aGroup,
                   const nsACString& aOrigin,
                   FullDatabaseMetadata* aMetadata,
                   FileManager* aFileManager,
                   already_AddRefed<DatabaseOfflineStorage> aOfflineStorage,
                   bool aChromeWriteAccessAllowed)
  : mFactory(aFactory)
  , mMetadata(aMetadata)
  , mFileManager(aFileManager)
  , mOfflineStorage(Move(aOfflineStorage))
  , mPrincipalInfo(aPrincipalInfo)
  , mGroup(aGroup)
  , mOrigin(aOrigin)
  , mId(aMetadata->mDatabaseId)
  , mFilePath(aMetadata->mFilePath)
  , mPersistenceType(aMetadata->mCommonMetadata.persistenceType())
  , mChromeWriteAccessAllowed(aChromeWriteAccessAllowed)
  , mClosed(false)
  , mInvalidated(false)
  , mActorWasAlive(false)
  , mActorDestroyed(false)
  , mMetadataCleanedUp(false)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(aMetadata);
  MOZ_ASSERT(aFileManager);
  MOZ_ASSERT_IF(aChromeWriteAccessAllowed,
                aPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo);

  mOfflineStorage->SetDatabase(this);
}

void
Database::Invalidate()
{
  AssertIsOnBackgroundThread();

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static bool
    InvalidateTransactions(nsTHashtable<nsPtrHashKey<TransactionBase>>& aTable)
    {
      AssertIsOnBackgroundThread();

      const uint32_t count = aTable.Count();
      if (!count) {
        return true;
      }

      FallibleTArray<nsRefPtr<TransactionBase>> transactions;
      if (NS_WARN_IF(!transactions.SetCapacity(count))) {
        return false;
      }

      aTable.EnumerateEntries(Collect, &transactions);

      if (NS_WARN_IF(transactions.Length() != count)) {
        return false;
      }

      if (count) {
        IDB_REPORT_INTERNAL_ERR();

        for (uint32_t index = 0; index < count; index++) {
          nsRefPtr<TransactionBase> transaction = transactions[index].forget();
          MOZ_ASSERT(transaction);

          transaction->Invalidate();
        }
      }

      return true;
    }

  private:
    static PLDHashOperator
    Collect(nsPtrHashKey<TransactionBase>* aEntry, void* aUserData)
    {
      AssertIsOnBackgroundThread();
      MOZ_ASSERT(aUserData);

      auto* array =
        static_cast<FallibleTArray<nsRefPtr<TransactionBase>>*>(aUserData);

      if (NS_WARN_IF(!array->AppendElement(aEntry->GetKey()))) {
        return PL_DHASH_STOP;
      }

      return PL_DHASH_NEXT;
    }
  };

  if (mInvalidated) {
    return;
  }

  mInvalidated = true;

  if (mActorWasAlive && !mActorDestroyed) {
    unused << SendInvalidate();
  }

  if (!Helper::InvalidateTransactions(mTransactions)) {
    NS_WARNING("Failed to abort all transactions!");
  }

  MOZ_ALWAYS_TRUE(CloseInternal());

  CleanupMetadata();
}

bool
Database::RegisterTransaction(TransactionBase* aTransaction)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(!mTransactions.GetEntry(aTransaction));
  MOZ_ASSERT(mOfflineStorage);

  if (NS_WARN_IF(!mTransactions.PutEntry(aTransaction, fallible))) {
    return false;
  }

  return true;
}

void
Database::UnregisterTransaction(TransactionBase* aTransaction)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(mTransactions.GetEntry(aTransaction));

  mTransactions.RemoveEntry(aTransaction);

  if (mOfflineStorage && !mTransactions.Count() && IsClosed()) {
    DatabaseOfflineStorage::UnregisterOnOwningThread(
      mOfflineStorage.forget());
    CleanupMetadata();
  }
}

void
Database::SetActorAlive()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!mActorDestroyed);

  mActorWasAlive = true;

  // This reference will be absorbed by IPDL and released when the actor is
  // destroyed.
  AddRef();
}

bool
Database::CloseInternal()
{
  AssertIsOnBackgroundThread();

  if (mClosed) {
    if (NS_WARN_IF(!IsInvalidated())) {
      // Kill misbehaving child for sending the close message twice.
      return false;
    }

    // Ignore harmless race when we just invalidated the database.
    return true;
  }

  mClosed = true;

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));

  MOZ_ASSERT(info->mLiveDatabases.Contains(this));

  if (info->mWaitingFactoryOp) {
    info->mWaitingFactoryOp->NoteDatabaseClosed(this);
  }

  if (mOfflineStorage) {
    mOfflineStorage->CloseOnOwningThread();

    if (!mTransactions.Count()) {
      DatabaseOfflineStorage::UnregisterOnOwningThread(
        mOfflineStorage.forget());
      CleanupMetadata();
    }
  }

  return true;
}

void
Database::CleanupMetadata()
{
  AssertIsOnBackgroundThread();

  if (!mMetadataCleanedUp) {
    mMetadataCleanedUp = true;

    DatabaseActorInfo* info;
    MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));
    MOZ_ALWAYS_TRUE(info->mLiveDatabases.RemoveElement(this));

    if (info->mLiveDatabases.IsEmpty()) {
      MOZ_ASSERT(!info->mWaitingFactoryOp ||
                 !info->mWaitingFactoryOp->HasBlockedDatabases());
      gLiveDatabaseHashtable->Remove(Id());
    }
  }
}

void
Database::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!IsInvalidated()) {
    Invalidate();
  }
}

PBackgroundIDBDatabaseFileParent*
Database::AllocPBackgroundIDBDatabaseFileParent(PBlobParent* aBlobParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBlobParent);

  nsRefPtr<FileImpl> blobImpl =
    static_cast<BlobParent*>(aBlobParent)->GetBlobImpl();
  MOZ_ASSERT(blobImpl);

  nsRefPtr<DatabaseFile> actor;

  if (nsRefPtr<FileInfo> fileInfo = blobImpl->GetFileInfo(mFileManager)) {
    // This blob was previously shared with the child.
    actor = new DatabaseFile(fileInfo);
  } else {
    // This is a blob we haven't seen before.
    fileInfo = mFileManager->GetNewFileInfo();
    MOZ_ASSERT(fileInfo);

    actor = new DatabaseFile(blobImpl, fileInfo);
  }

  MOZ_ASSERT(actor);

  return actor.forget().take();
}

bool
Database::DeallocPBackgroundIDBDatabaseFileParent(
                                       PBackgroundIDBDatabaseFileParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  nsRefPtr<DatabaseFile> actor =
    dont_AddRef(static_cast<DatabaseFile*>(aActor));
  return true;
}

PBackgroundIDBTransactionParent*
Database::AllocPBackgroundIDBTransactionParent(
                                    const nsTArray<nsString>& aObjectStoreNames,
                                    const Mode& aMode)
{
  AssertIsOnBackgroundThread();

  class MOZ_STACK_CLASS Closure MOZ_FINAL
  {
    const nsString& mName;
    FallibleTArray<nsRefPtr<FullObjectStoreMetadata>>& mObjectStores;

  public:
    Closure(const nsString& aName,
            FallibleTArray<nsRefPtr<FullObjectStoreMetadata>>& aObjectStores)
      : mName(aName)
      , mObjectStores(aObjectStores)
    { }

    static PLDHashOperator
    Find(const uint64_t& aKey,
         FullObjectStoreMetadata* aValue,
         void* aClosure)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);
      MOZ_ASSERT(aClosure);

      auto* closure = static_cast<Closure*>(aClosure);

      if (closure->mName == aValue->mCommonMetadata.name() &&
          !aValue->mDeleted) {
        MOZ_ALWAYS_TRUE(closure->mObjectStores.AppendElement(aValue));
        return PL_DHASH_STOP;
      }

      return PL_DHASH_NEXT;
    }
  };

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

  if (NS_WARN_IF(aMode != IDBTransaction::READ_ONLY &&
                 aMode != IDBTransaction::READ_WRITE)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // If this is a readwrite transaction to a chrome database make sure the child
  // has write access.
  if (NS_WARN_IF(aMode == IDBTransaction::READ_WRITE &&
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

  FallibleTArray<nsRefPtr<FullObjectStoreMetadata>> fallibleObjectStores;
  if (NS_WARN_IF(!fallibleObjectStores.SetCapacity(nameCount))) {
    return nullptr;
  }

  for (uint32_t nameIndex = 0; nameIndex < nameCount; nameIndex++) {
    const nsString& name = aObjectStoreNames[nameIndex];

    if (nameIndex) {
      // Make sure that this name is sorted properly and not a duplicate.
      if (NS_WARN_IF(name <= aObjectStoreNames[nameIndex - 1])) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }
    }

    const uint32_t oldLength = fallibleObjectStores.Length();

    Closure closure(name, fallibleObjectStores);
    objectStores.EnumerateRead(Closure::Find, &closure);

    if (NS_WARN_IF((oldLength + 1) != fallibleObjectStores.Length())) {
      return nullptr;
    }
  }

  nsTArray<nsRefPtr<FullObjectStoreMetadata>> infallibleObjectStores;
  infallibleObjectStores.SwapElements(fallibleObjectStores);

  nsRefPtr<NormalTransaction> transaction =
    new NormalTransaction(this, aMode, infallibleObjectStores);

  MOZ_ASSERT(infallibleObjectStores.IsEmpty());

  return transaction.forget().take();
}

bool
Database::RecvPBackgroundIDBTransactionConstructor(
                                    PBackgroundIDBTransactionParent* aActor,
                                    InfallibleTArray<nsString>&& aObjectStoreNames,
                                    const Mode& aMode)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!aObjectStoreNames.IsEmpty());
  MOZ_ASSERT(aMode == IDBTransaction::READ_ONLY ||
             aMode == IDBTransaction::READ_WRITE);
  MOZ_ASSERT(!mClosed);

  if (IsInvalidated()) {
    // This is an expected race. We don't want the child to die here, just don't
    // actually do any work.
    return true;
  }

  auto* transaction = static_cast<NormalTransaction*>(aActor);

  // Add a placeholder for this transaction immediately.
  gTransactionThreadPool->Start(transaction->TransactionId(),
                                mMetadata->mDatabaseId,
                                aObjectStoreNames,
                                aMode,
                                GetLoggingInfo()->Id(),
                                transaction->LoggingSerialNumber(),
                                gStartTransactionRunnable);

  transaction->SetActive();

  if (NS_WARN_IF(!RegisterTransaction(transaction))) {
    IDB_REPORT_INTERNAL_ERR();
    transaction->Abort(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, /* aForce */ false);
    return true;
  }

  return true;
}

bool
Database::DeallocPBackgroundIDBTransactionParent(
                                        PBackgroundIDBTransactionParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  nsRefPtr<NormalTransaction> transaction =
    dont_AddRef(static_cast<NormalTransaction*>(aActor));
  return true;
}

PBackgroundIDBVersionChangeTransactionParent*
Database::AllocPBackgroundIDBVersionChangeTransactionParent(
                                              const uint64_t& aCurrentVersion,
                                              const uint64_t& aRequestedVersion,
                                              const int64_t& aNextObjectStoreId,
                                              const int64_t& aNextIndexId)
{
  MOZ_CRASH("PBackgroundIDBVersionChangeTransactionParent actors should be "
            "constructed manually!");
}

bool
Database::DeallocPBackgroundIDBVersionChangeTransactionParent(
                           PBackgroundIDBVersionChangeTransactionParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  nsRefPtr<VersionChangeTransaction> transaction =
    dont_AddRef(static_cast<VersionChangeTransaction*>(aActor));
  return true;
}

bool
Database::RecvDeleteMe()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  return PBackgroundIDBDatabaseParent::Send__delete__(this);
}

bool
Database::RecvBlocked()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mClosed)) {
    return false;
  }

  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(Id(), &info));

  MOZ_ASSERT(info->mLiveDatabases.Contains(this));
  MOZ_ASSERT(info->mWaitingFactoryOp);

  info->mWaitingFactoryOp->NoteDatabaseBlocked(this);

  return true;
}

bool
Database::RecvClose()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!CloseInternal())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  return true;
}

/*******************************************************************************
 * TransactionBase
 ******************************************************************************/

TransactionBase::TransactionBase(Database* aDatabase, Mode aMode)
  : mDatabase(aDatabase)
  , mTransactionId(gTransactionThreadPool->NextTransactionId())
  , mDatabaseId(aDatabase->Id())
  , mLoggingSerialNumber(aDatabase->GetLoggingInfo()->NextTransactionSN(aMode))
  , mActiveRequestCount(0)
  , mInvalidatedOnAnyThread(false)
  , mMode(aMode)
  , mHasBeenActive(false)
  , mActorDestroyed(false)
  , mInvalidated(false)
  , mResultCode(NS_OK)
  , mCommitOrAbortReceived(false)
  , mCommittedOrAborted(false)
  , mForceAborted(false)
  , mTransactionThread(nullptr)
  , mSavepointCount(0)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mTransactionId);
  MOZ_ASSERT(mLoggingSerialNumber);
}

TransactionBase::~TransactionBase()
{
  MOZ_ASSERT(!mSavepointCount);
  MOZ_ASSERT(!mActiveRequestCount);
  MOZ_ASSERT(mActorDestroyed);
  MOZ_ASSERT_IF(mHasBeenActive, mCommittedOrAborted);
}

nsresult
TransactionBase::EnsureConnection()
{
#ifdef DEBUG
  MOZ_ASSERT(!IsOnBackgroundThread());
  if (!mTransactionThread) {
    mTransactionThread = PR_GetCurrentThread();
  }
#endif

  AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "TransactionBase::EnsureConnection",
                 js::ProfileEntry::Category::STORAGE);

  if (!mConnection) {
    nsCOMPtr<mozIStorageConnection> connection;
    nsresult rv =
      GetDatabaseConnection(mDatabase->FilePath(), mDatabase->Type(),
                            mDatabase->Group(), mDatabase->Origin(),
                            getter_AddRefs(connection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsRefPtr<UpdateRefcountFunction> function;
    nsCString beginTransaction;

    if (mMode == IDBTransaction::READ_ONLY) {
      beginTransaction.AssignLiteral("BEGIN TRANSACTION;");
    } else {
      function = new UpdateRefcountFunction(mDatabase->GetFileManager());

      rv = connection->CreateFunction(NS_LITERAL_CSTRING("update_refcount"), 2,
                                      function);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      beginTransaction.AssignLiteral("BEGIN IMMEDIATE TRANSACTION;");
    }

    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(beginTransaction, getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    function.swap(mUpdateFileRefcountFunction);
    connection.swap(mConnection);
  }

  return NS_OK;
}

void
TransactionBase::Abort(nsresult aResultCode, bool aForce)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));

  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = aResultCode;
  }

  if (aForce) {
    mForceAborted = true;
  }

  MaybeCommitOrAbort();
}

bool
TransactionBase::RecvCommit()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  mCommitOrAbortReceived = true;

  MaybeCommitOrAbort();
  return true;
}

bool
TransactionBase::RecvAbort(nsresult aResultCode)
{
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

  mCommitOrAbortReceived = true;

  Abort(aResultCode, /* aForce */ false);
  return true;
}

void
TransactionBase::CommitOrAbort()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mCommittedOrAborted);

  mCommittedOrAborted = true;

  if (!mHasBeenActive) {
    return;
  }

  nsRefPtr<CommitOp> commitOp =
    new CommitOp(this, ClampResultCode(mResultCode));

  gTransactionThreadPool->Dispatch(TransactionId(),
                                   DatabaseId(),
                                   commitOp,
                                   /* aFinish */ true,
                                   /* aFinishCallback */ commitOp);
}

already_AddRefed<FullObjectStoreMetadata>
TransactionBase::GetMetadataForObjectStoreId(int64_t aObjectStoreId) const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aObjectStoreId);

  if (!aObjectStoreId) {
    return nullptr;
  }

  nsRefPtr<FullObjectStoreMetadata> metadata;
  if (!mDatabase->Metadata()->mObjectStores.Get(aObjectStoreId,
                                                getter_AddRefs(metadata))) {
    return nullptr;
  }

  MOZ_ASSERT(metadata->mCommonMetadata.id() == aObjectStoreId);
  MOZ_ASSERT(!metadata->mDeleted);

  return metadata.forget();
}

already_AddRefed<FullIndexMetadata>
TransactionBase::GetMetadataForIndexId(
                            FullObjectStoreMetadata* const aObjectStoreMetadata,
                            int64_t aIndexId) const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aIndexId);

  if (!aIndexId) {
    return nullptr;
  }

  nsRefPtr<FullIndexMetadata> metadata;
  if (!aObjectStoreMetadata->mIndexes.Get(aIndexId, getter_AddRefs(metadata))) {
    return nullptr;
  }

  MOZ_ASSERT(metadata->mCommonMetadata.id() == aIndexId);
  MOZ_ASSERT(!metadata->mDeleted);

  return metadata.forget();
}

void
TransactionBase::NoteModifiedAutoIncrementObjectStore(
                                             FullObjectStoreMetadata* aMetadata)
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(aMetadata);

  if (!mModifiedAutoIncrementObjectStoreMetadataArray.Contains(aMetadata)) {
    mModifiedAutoIncrementObjectStoreMetadataArray.AppendElement(aMetadata);
  }
}

void
TransactionBase::ForgetModifiedAutoIncrementObjectStore(
                                             FullObjectStoreMetadata* aMetadata)
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(aMetadata);

  mModifiedAutoIncrementObjectStoreMetadataArray.RemoveElement(aMetadata);
}

bool
TransactionBase::VerifyRequestParams(const RequestParams& aParams) const
{
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
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
      if (NS_WARN_IF(mMode != IDBTransaction::READ_WRITE &&
                     mMode != IDBTransaction::VERSION_CHANGE)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const ObjectStoreDeleteParams& params =
        aParams.get_ObjectStoreDeleteParams();
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
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
      if (NS_WARN_IF(mMode != IDBTransaction::READ_WRITE &&
                     mMode != IDBTransaction::VERSION_CHANGE)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const ObjectStoreClearParams& params =
        aParams.get_ObjectStoreClearParams();
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const nsRefPtr<FullIndexMetadata> indexMetadata =
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const nsRefPtr<FullIndexMetadata> indexMetadata =
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const nsRefPtr<FullIndexMetadata> indexMetadata =
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const nsRefPtr<FullIndexMetadata> indexMetadata =
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
      const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      const nsRefPtr<FullIndexMetadata> indexMetadata =
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

bool
TransactionBase::VerifyRequestParams(const OpenCursorParams& aParams) const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  switch (aParams.type()) {
    case OpenCursorParams::TObjectStoreOpenCursorParams: {
      const ObjectStoreOpenCursorParams& params =
        aParams.get_ObjectStoreOpenCursorParams();
      nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
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

    case OpenCursorParams::TObjectStoreOpenKeyCursorParams: {
      const ObjectStoreOpenKeyCursorParams& params =
        aParams.get_ObjectStoreOpenKeyCursorParams();
      nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
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

    case OpenCursorParams::TIndexOpenCursorParams: {
      const IndexOpenCursorParams& params = aParams.get_IndexOpenCursorParams();
      nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      nsRefPtr<FullIndexMetadata> indexMetadata =
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

    case OpenCursorParams::TIndexOpenKeyCursorParams: {
      const IndexOpenKeyCursorParams& params =
        aParams.get_IndexOpenKeyCursorParams();
      nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
        GetMetadataForObjectStoreId(params.objectStoreId());
      if (NS_WARN_IF(!objectStoreMetadata)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      nsRefPtr<FullIndexMetadata> indexMetadata =
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

bool
TransactionBase::VerifyRequestParams(const CursorRequestParams& aParams) const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != CursorRequestParams::T__None);

  switch (aParams.type()) {
    case CursorRequestParams::TContinueParams:
      break;

    case CursorRequestParams::TAdvanceParams:
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

bool
TransactionBase::VerifyRequestParams(const SerializedKeyRange& aParams) const
{
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

bool
TransactionBase::VerifyRequestParams(const ObjectStoreAddPutParams& aParams)
                                     const
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mMode != IDBTransaction::READ_WRITE &&
                 mMode != IDBTransaction::VERSION_CHANGE)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<FullObjectStoreMetadata> objMetadata =
    GetMetadataForObjectStoreId(aParams.objectStoreId());
  if (NS_WARN_IF(!objMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(aParams.cloneInfo().data().IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (objMetadata->mCommonMetadata.autoIncrement() &&
      objMetadata->mCommonMetadata.keyPath().IsValid() &&
      aParams.key().IsUnset()) {
    const SerializedStructuredCloneWriteInfo cloneInfo = aParams.cloneInfo();

    if (NS_WARN_IF(!cloneInfo.offsetToKeyProp())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(cloneInfo.data().Length() < sizeof(uint64_t))) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(cloneInfo.offsetToKeyProp() >
                   (cloneInfo.data().Length() - sizeof(uint64_t)))) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  } else if (NS_WARN_IF(aParams.cloneInfo().offsetToKeyProp())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const nsTArray<IndexUpdateInfo>& updates = aParams.indexUpdateInfos();

  for (uint32_t index = 0; index < updates.Length(); index++) {
    nsRefPtr<FullIndexMetadata> indexMetadata =
      GetMetadataForIndexId(objMetadata, updates[index].indexId());
    if (NS_WARN_IF(!indexMetadata)) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }

    if (NS_WARN_IF(updates[index].value().IsUnset())) {
      ASSERT_UNLESS_FUZZING();
      return false;
    }
  }

  const nsTArray<DatabaseFileOrMutableFileId>& files = aParams.files();

  for (uint32_t index = 0; index < files.Length(); index++) {
    const DatabaseFileOrMutableFileId& fileOrFileId = files[index];

    MOZ_ASSERT(fileOrFileId.type() != DatabaseFileOrMutableFileId::T__None);

    switch (fileOrFileId.type()) {
      case DatabaseFileOrMutableFileId::TPBackgroundIDBDatabaseFileChild:
        ASSERT_UNLESS_FUZZING();
        return false;

      case DatabaseFileOrMutableFileId::TPBackgroundIDBDatabaseFileParent:
        if (NS_WARN_IF(!fileOrFileId.get_PBackgroundIDBDatabaseFileParent())) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
        break;

      case DatabaseFileOrMutableFileId::Tint64_t:
        if (NS_WARN_IF(fileOrFileId.get_int64_t() <= 0)) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
        break;

      default:
        MOZ_CRASH("Should never get here!");
    }
  }

  return true;
}

bool
TransactionBase::VerifyRequestParams(const OptionalKeyRange& aParams) const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != OptionalKeyRange::T__None);

  switch (aParams.type()) {
    case OptionalKeyRange::TSerializedKeyRange:
      if (NS_WARN_IF(!VerifyRequestParams(aParams.get_SerializedKeyRange()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }
      break;

    case OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

nsresult
TransactionBase::StartSavepoint()
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(IDBTransaction::READ_WRITE == mMode ||
             IDBTransaction::VERSION_CHANGE == mMode);

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(kSavepointClause, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mUpdateFileRefcountFunction->StartSavepoint();

  mSavepointCount++;

  return NS_OK;
}

nsresult
TransactionBase::ReleaseSavepoint()
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(IDBTransaction::READ_WRITE == mMode ||
             IDBTransaction::VERSION_CHANGE == mMode);
  MOZ_ASSERT(mSavepointCount);

  mSavepointCount--;

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(
    NS_LITERAL_CSTRING("RELEASE ") + NS_LITERAL_CSTRING(kSavepointClause),
    &stmt);
  if (NS_SUCCEEDED(rv)) {
    rv = stmt->Execute();
    if (NS_SUCCEEDED(rv)) {
      mUpdateFileRefcountFunction->ReleaseSavepoint();
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    mUpdateFileRefcountFunction->RollbackSavepoint();
  }

  return rv;
}

nsresult
TransactionBase::RollbackSavepoint()
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(IDBTransaction::READ_WRITE == mMode ||
             IDBTransaction::VERSION_CHANGE == mMode);
  MOZ_ASSERT(mSavepointCount);

  mSavepointCount--;

  mUpdateFileRefcountFunction->RollbackSavepoint();

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(
    NS_LITERAL_CSTRING("ROLLBACK TO ") + NS_LITERAL_CSTRING(kSavepointClause),
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This may fail if SQLite already rolled back the savepoint so ignore any
  // errors.
  unused << stmt->Execute();

  return NS_OK;
}

void
TransactionBase::NoteActiveRequest()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveRequestCount < UINT64_MAX);

  mActiveRequestCount++;
}

void
TransactionBase::NoteFinishedRequest()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveRequestCount);

  mActiveRequestCount--;

  MaybeCommitOrAbort();
}

void
TransactionBase::Invalidate()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mInvalidated == mInvalidatedOnAnyThread);

  if (!mInvalidated) {
    mInvalidated = true;
    mInvalidatedOnAnyThread = true;

    Abort(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, /* aForce */ true);
  }
}

PBackgroundIDBRequestParent*
TransactionBase::AllocRequest(const RequestParams& aParams, bool aTrustParams)
{
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

  nsRefPtr<NormalTransactionOp> actor;

  switch (aParams.type()) {
    case RequestParams::TObjectStoreAddParams:
    case RequestParams::TObjectStorePutParams:
      actor = new ObjectStoreAddOrPutRequestOp(this, aParams);
      break;

    case RequestParams::TObjectStoreGetParams:
      actor =
        new ObjectStoreGetRequestOp(this, aParams, /* aGetAll */ false);
      break;

    case RequestParams::TObjectStoreGetAllParams:
      actor =
        new ObjectStoreGetRequestOp(this, aParams, /* aGetAll */ true);
      break;

    case RequestParams::TObjectStoreGetAllKeysParams:
      actor =
        new ObjectStoreGetAllKeysRequestOp(this,
                                     aParams.get_ObjectStoreGetAllKeysParams());
      break;

    case RequestParams::TObjectStoreDeleteParams:
      actor =
        new ObjectStoreDeleteRequestOp(this,
                                       aParams.get_ObjectStoreDeleteParams());
      break;

    case RequestParams::TObjectStoreClearParams:
      actor =
        new ObjectStoreClearRequestOp(this,
                                      aParams.get_ObjectStoreClearParams());
      break;

    case RequestParams::TObjectStoreCountParams:
      actor =
        new ObjectStoreCountRequestOp(this,
                                      aParams.get_ObjectStoreCountParams());
      break;

    case RequestParams::TIndexGetParams:
      actor = new IndexGetRequestOp(this, aParams, /* aGetAll */ false);
      break;

    case RequestParams::TIndexGetKeyParams:
      actor = new IndexGetKeyRequestOp(this, aParams, /* aGetAll */ false);
      break;

    case RequestParams::TIndexGetAllParams:
      actor = new IndexGetRequestOp(this, aParams, /* aGetAll */ true);
      break;

    case RequestParams::TIndexGetAllKeysParams:
      actor = new IndexGetKeyRequestOp(this, aParams, /* aGetAll */ true);
      break;

    case RequestParams::TIndexCountParams:
      actor = new IndexCountRequestOp(this, aParams);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool
TransactionBase::StartRequest(PBackgroundIDBRequestParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  auto* op = static_cast<NormalTransactionOp*>(aActor);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return false;
  }

  op->DispatchToTransactionThreadPool();
  return true;
}

bool
TransactionBase::DeallocRequest(PBackgroundIDBRequestParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  nsRefPtr<NormalTransactionOp> actor =
    dont_AddRef(static_cast<NormalTransactionOp*>(aActor));
  return true;
}

PBackgroundIDBCursorParent*
TransactionBase::AllocCursor(const OpenCursorParams& aParams, bool aTrustParams)
{
  AssertIsOnBackgroundThread();

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

  OpenCursorParams::Type type = aParams.type();
  MOZ_ASSERT(type != OpenCursorParams::T__None);

  int64_t objectStoreId;
  int64_t indexId;
  Cursor::Direction direction;

  switch(type) {
    case OpenCursorParams::TObjectStoreOpenCursorParams: {
      const auto& params = aParams.get_ObjectStoreOpenCursorParams();
      objectStoreId = params.objectStoreId();
      indexId = 0;
      direction = params.direction();
      break;
    }

    case OpenCursorParams::TObjectStoreOpenKeyCursorParams: {
      const auto& params = aParams.get_ObjectStoreOpenKeyCursorParams();
      objectStoreId = params.objectStoreId();
      indexId = 0;
      direction = params.direction();
      break;
    }

    case OpenCursorParams::TIndexOpenCursorParams: {
      const auto& params = aParams.get_IndexOpenCursorParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      direction = params.direction();
      break;
    }

    case OpenCursorParams::TIndexOpenKeyCursorParams: {
      const auto& params = aParams.get_IndexOpenKeyCursorParams();
      objectStoreId = params.objectStoreId();
      indexId = params.indexId();
      direction = params.direction();
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  nsRefPtr<Cursor> actor =
    new Cursor(this, type, objectStoreId, indexId, direction);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool
TransactionBase::StartCursor(PBackgroundIDBCursorParent* aActor,
                             const OpenCursorParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  auto* op = static_cast<Cursor*>(aActor);

  if (NS_WARN_IF(!op->Start(aParams))) {
    return false;
  }

  return true;
}

bool
TransactionBase::DeallocCursor(PBackgroundIDBCursorParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  nsRefPtr<Cursor> actor = dont_AddRef(static_cast<Cursor*>(aActor));
  return true;
}

nsresult
TransactionBase::GetCachedStatement(const nsACString& aQuery,
                                    CachedStatement* aCachedStatement)
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(!aQuery.IsEmpty());
  MOZ_ASSERT(aCachedStatement);
  MOZ_ASSERT(mConnection);

  nsCOMPtr<mozIStorageStatement> stmt;

  if (!mCachedStatements.Get(aQuery, getter_AddRefs(stmt))) {
    nsresult rv = mConnection->CreateStatement(aQuery, getter_AddRefs(stmt));
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      nsCString msg;
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mConnection->GetLastErrorString(msg)));

      nsAutoCString error =
        NS_LITERAL_CSTRING("The statement '") + aQuery +
        NS_LITERAL_CSTRING("' failed to compile with the error message '") +
        msg + NS_LITERAL_CSTRING("'.");

      NS_WARNING(error.get());
#endif
      return rv;
    }

    mCachedStatements.Put(aQuery, stmt);
  }

  aCachedStatement->Assign(stmt.forget());
  return NS_OK;
}

void
TransactionBase::ReleaseTransactionThreadObjects()
{
  AssertIsOnTransactionThread();

  mCachedStatements.Clear();

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mConnection->Close()));
  mConnection = nullptr;
}

void
TransactionBase::ReleaseBackgroundThreadObjects()
{
  AssertIsOnBackgroundThread();

  if (mUpdateFileRefcountFunction) {
    mUpdateFileRefcountFunction->ClearFileInfoEntries();
    mUpdateFileRefcountFunction = nullptr;
  }
}

/*******************************************************************************
 * NormalTransaction
 ******************************************************************************/

NormalTransaction::NormalTransaction(
                     Database* aDatabase,
                     TransactionBase::Mode aMode,
                     nsTArray<nsRefPtr<FullObjectStoreMetadata>>& aObjectStores)
  : TransactionBase(aDatabase, aMode)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!aObjectStores.IsEmpty());

  mObjectStores.SwapElements(aObjectStores);
}

bool
NormalTransaction::IsSameProcessActor()
{
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager()->Manager()->Manager();
  MOZ_ASSERT(actor);

  return !BackgroundParent::IsOtherProcessActor(actor);
}

bool
NormalTransaction::SendCompleteNotification(nsresult aResult)
{
  AssertIsOnBackgroundThread();

  return IsActorDestroyed() || !NS_WARN_IF(!SendComplete(aResult));
}

void
NormalTransaction::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  if (!mCommittedOrAborted) {
    if (NS_SUCCEEDED(mResultCode)) {
      IDB_REPORT_INTERNAL_ERR();
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    mForceAborted = true;

    MaybeCommitOrAbort();
  }

  NoteActorDestroyed();
}

bool
NormalTransaction::RecvDeleteMe()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  return PBackgroundIDBTransactionParent::Send__delete__(this);
}

bool
NormalTransaction::RecvCommit()
{
  AssertIsOnBackgroundThread();

  return TransactionBase::RecvCommit();
}

bool
NormalTransaction::RecvAbort(const nsresult& aResultCode)
{
  AssertIsOnBackgroundThread();

  return TransactionBase::RecvAbort(aResultCode);
}

PBackgroundIDBRequestParent*
NormalTransaction::AllocPBackgroundIDBRequestParent(
                                                   const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  return AllocRequest(aParams, IsSameProcessActor());
}

bool
NormalTransaction::RecvPBackgroundIDBRequestConstructor(
                                            PBackgroundIDBRequestParent* aActor,
                                            const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  return StartRequest(aActor);
}

bool
NormalTransaction::DeallocPBackgroundIDBRequestParent(
                                            PBackgroundIDBRequestParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocRequest(aActor);
}

PBackgroundIDBCursorParent*
NormalTransaction::AllocPBackgroundIDBCursorParent(
                                                const OpenCursorParams& aParams)
{
  AssertIsOnBackgroundThread();

  return AllocCursor(aParams, IsSameProcessActor());
}

bool
NormalTransaction::RecvPBackgroundIDBCursorConstructor(
                                             PBackgroundIDBCursorParent* aActor,
                                             const OpenCursorParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  return StartCursor(aActor, aParams);
}

bool
NormalTransaction::DeallocPBackgroundIDBCursorParent(
                                             PBackgroundIDBCursorParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocCursor(aActor);
}

/*******************************************************************************
 * VersionChangeTransaction
 ******************************************************************************/

VersionChangeTransaction::VersionChangeTransaction(
                                                OpenDatabaseOp* aOpenDatabaseOp)
  : TransactionBase(aOpenDatabaseOp->mDatabase, IDBTransaction::VERSION_CHANGE)
  , mOpenDatabaseOp(aOpenDatabaseOp)
  , mActorWasAlive(false)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aOpenDatabaseOp);
}

VersionChangeTransaction::~VersionChangeTransaction()
{
#ifdef DEBUG
  // Silence the base class' destructor assertion if we never made this actor
  // live.
  FakeActorDestroyed();
#endif
}

bool
VersionChangeTransaction::IsSameProcessActor()
{
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager()->Manager()->Manager();
  MOZ_ASSERT(actor);

  return !BackgroundParent::IsOtherProcessActor(actor);
}

void
VersionChangeTransaction::SetActorAlive()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!IsActorDestroyed());

  mActorWasAlive = true;

  // This reference will be absorbed by IPDL and released when the actor is
  // destroyed.
  AddRef();
}

bool
VersionChangeTransaction::CopyDatabaseMetadata()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mOldMetadata);

  const nsRefPtr<FullDatabaseMetadata> origMetadata =
    GetDatabase()->Metadata();
  MOZ_ASSERT(origMetadata);

  nsRefPtr<FullDatabaseMetadata> newMetadata = origMetadata->Duplicate();
  if (NS_WARN_IF(!newMetadata)) {
    return false;
  }

  // Replace the live metadata with the new mutable copy.
  DatabaseActorInfo* info;
  MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(origMetadata->mDatabaseId,
                                              &info));
  MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());
  MOZ_ASSERT(info->mMetadata == origMetadata);

  mOldMetadata = info->mMetadata.forget();
  info->mMetadata.swap(newMetadata);

  // Replace metadata pointers for all live databases.
  for (uint32_t count = info->mLiveDatabases.Length(), index = 0;
       index < count;
       index++) {
    info->mLiveDatabases[index]->mMetadata = info->mMetadata;
  }

  return true;
}

void
VersionChangeTransaction::UpdateMetadata(nsresult aResult)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(GetDatabase());
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mDatabase);
  MOZ_ASSERT(!mOpenDatabaseOp->mDatabaseId.IsEmpty());

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static PLDHashOperator
    Enumerate(const uint64_t& aKey,
              nsRefPtr<FullObjectStoreMetadata>& aValue,
              void* /* aClosure */)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);

      if (aValue->mDeleted) {
        return PL_DHASH_REMOVE;
      }

      aValue->mIndexes.Enumerate(Enumerate, nullptr);
#ifdef DEBUG
      aValue->mIndexes.MarkImmutable();
#endif

      return PL_DHASH_NEXT;
    }

  private:
    static PLDHashOperator
    Enumerate(const uint64_t& aKey,
              nsRefPtr<FullIndexMetadata>& aValue,
              void* /* aClosure */)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);

      if (aValue->mDeleted) {
        return PL_DHASH_REMOVE;
      }

      return PL_DHASH_NEXT;
    }
  };

  if (IsActorDestroyed()) {
    return;
  }

  nsRefPtr<FullDatabaseMetadata> oldMetadata;
  mOldMetadata.swap(oldMetadata);

  DatabaseActorInfo* info;
  if (!gLiveDatabaseHashtable->Get(oldMetadata->mDatabaseId, &info)) {
    return;
  }

  MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());

  if (NS_SUCCEEDED(aResult)) {
    // Remove all deleted objectStores and indexes, then mark immutable.
    info->mMetadata->mObjectStores.Enumerate(Helper::Enumerate, nullptr);
#ifdef DEBUG
    info->mMetadata->mObjectStores.MarkImmutable();
#endif
  } else {
    // Replace metadata pointers for all live databases.
    info->mMetadata = oldMetadata.forget();

    for (uint32_t count = info->mLiveDatabases.Length(), index = 0;
         index < count;
         index++) {
      info->mLiveDatabases[index]->mMetadata = info->mMetadata;
    }
  }
}

bool
VersionChangeTransaction::SendCompleteNotification(nsresult aResult)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOpenDatabaseOp);

  nsRefPtr<OpenDatabaseOp> openDatabaseOp;
  mOpenDatabaseOp.swap(openDatabaseOp);

  if (NS_FAILED(aResult) && NS_SUCCEEDED(openDatabaseOp->mResultCode)) {
    openDatabaseOp->mResultCode = aResult;
  }

  openDatabaseOp->mState = OpenDatabaseOp::State_SendingResults;

  bool result = IsActorDestroyed() || !NS_WARN_IF(!SendComplete(aResult));

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(openDatabaseOp->Run()));

  return result;
}

void
VersionChangeTransaction::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  if (!mCommittedOrAborted) {
    if (NS_SUCCEEDED(mResultCode)) {
      IDB_REPORT_INTERNAL_ERR();
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    mForceAborted = true;

    MaybeCommitOrAbort();
  }

  NoteActorDestroyed();
}

bool
VersionChangeTransaction::RecvDeleteMe()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  return PBackgroundIDBVersionChangeTransactionParent::Send__delete__(this);
}

bool
VersionChangeTransaction::RecvCommit()
{
  AssertIsOnBackgroundThread();

  return TransactionBase::RecvCommit();
}

bool
VersionChangeTransaction::RecvAbort(const nsresult& aResultCode)
{
  AssertIsOnBackgroundThread();

  return TransactionBase::RecvAbort(aResultCode);
}

bool
VersionChangeTransaction::RecvCreateObjectStore(
                                           const ObjectStoreMetadata& aMetadata)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aMetadata.id())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const nsRefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);

  if (NS_WARN_IF(aMetadata.id() != dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  auto* foundMetadata =
    MetadataNameOrIdMatcher<FullObjectStoreMetadata>::Match(
      dbMetadata->mObjectStores, aMetadata.id(), aMetadata.name());

  if (NS_WARN_IF(foundMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<FullObjectStoreMetadata> newMetadata = new FullObjectStoreMetadata();
  newMetadata->mCommonMetadata = aMetadata;
  newMetadata->mNextAutoIncrementId = aMetadata.autoIncrement() ? 1 : 0;
  newMetadata->mComittedAutoIncrementId = newMetadata->mNextAutoIncrementId;

  if (NS_WARN_IF(!dbMetadata->mObjectStores.Put(aMetadata.id(), newMetadata,
                                                fallible))) {
    return false;
  }

  dbMetadata->mNextObjectStoreId++;

  nsRefPtr<CreateObjectStoreOp> op = new CreateObjectStoreOp(this, aMetadata);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return false;
  }

  op->DispatchToTransactionThreadPool();

  return true;
}

bool
VersionChangeTransaction::RecvDeleteObjectStore(const int64_t& aObjectStoreId)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const nsRefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);
  MOZ_ASSERT(dbMetadata->mNextObjectStoreId > 0);

  if (NS_WARN_IF(aObjectStoreId >= dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<FullObjectStoreMetadata> foundMetadata =
    GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  foundMetadata->mDeleted = true;

  nsRefPtr<DeleteObjectStoreOp> op =
    new DeleteObjectStoreOp(this, foundMetadata);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return false;
  }

  op->DispatchToTransactionThreadPool();

  return true;
}

bool
VersionChangeTransaction::RecvCreateIndex(const int64_t& aObjectStoreId,
                                          const IndexMetadata& aMetadata)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(!aMetadata.id())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const nsRefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);

  if (NS_WARN_IF(aMetadata.id() != dbMetadata->mNextIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<FullObjectStoreMetadata> foundObjectStoreMetadata =
    GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundObjectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<FullIndexMetadata> foundIndexMetadata =
    MetadataNameOrIdMatcher<FullIndexMetadata>::Match(
      foundObjectStoreMetadata->mIndexes, aMetadata.id(), aMetadata.name());

  if (NS_WARN_IF(foundIndexMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<FullIndexMetadata> newMetadata = new FullIndexMetadata();
  newMetadata->mCommonMetadata = aMetadata;

  if (NS_WARN_IF(!foundObjectStoreMetadata->mIndexes.Put(aMetadata.id(),
                                                         newMetadata,
                                                         fallible))) {
    return false;
  }

  dbMetadata->mNextIndexId++;

  nsRefPtr<CreateIndexOp> op =
    new CreateIndexOp(this, aObjectStoreId, aMetadata);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return false;
  }

  op->DispatchToTransactionThreadPool();

  return true;
}

bool
VersionChangeTransaction::RecvDeleteIndex(const int64_t& aObjectStoreId,
                                          const int64_t& aIndexId)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!aObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(!aIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const nsRefPtr<FullDatabaseMetadata> dbMetadata = GetDatabase()->Metadata();
  MOZ_ASSERT(dbMetadata);
  MOZ_ASSERT(dbMetadata->mNextObjectStoreId > 0);
  MOZ_ASSERT(dbMetadata->mNextIndexId > 0);

  if (NS_WARN_IF(aObjectStoreId >= dbMetadata->mNextObjectStoreId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(aIndexId >= dbMetadata->mNextIndexId)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<FullObjectStoreMetadata> foundObjectStoreMetadata =
    GetMetadataForObjectStoreId(aObjectStoreId);

  if (NS_WARN_IF(!foundObjectStoreMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<FullIndexMetadata> foundIndexMetadata =
    GetMetadataForIndexId(foundObjectStoreMetadata, aIndexId);

  if (NS_WARN_IF(!foundIndexMetadata)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  foundIndexMetadata->mDeleted = true;

  nsRefPtr<DeleteIndexOp> op = new DeleteIndexOp(this, aIndexId);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return false;
  }

  op->DispatchToTransactionThreadPool();

  return true;
}

PBackgroundIDBRequestParent*
VersionChangeTransaction::AllocPBackgroundIDBRequestParent(
                                                   const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  return AllocRequest(aParams, IsSameProcessActor());
}

bool
VersionChangeTransaction::RecvPBackgroundIDBRequestConstructor(
                                            PBackgroundIDBRequestParent* aActor,
                                            const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  return StartRequest(aActor);
}

bool
VersionChangeTransaction::DeallocPBackgroundIDBRequestParent(
                                            PBackgroundIDBRequestParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocRequest(aActor);
}

PBackgroundIDBCursorParent*
VersionChangeTransaction::AllocPBackgroundIDBCursorParent(
                                                const OpenCursorParams& aParams)
{
  AssertIsOnBackgroundThread();

  return AllocCursor(aParams, IsSameProcessActor());
}

bool
VersionChangeTransaction::RecvPBackgroundIDBCursorConstructor(
                                             PBackgroundIDBCursorParent* aActor,
                                             const OpenCursorParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  return StartCursor(aActor, aParams);
}

bool
VersionChangeTransaction::DeallocPBackgroundIDBCursorParent(
                                             PBackgroundIDBCursorParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  return DeallocCursor(aActor);
}

/*******************************************************************************
 * Cursor
 ******************************************************************************/

Cursor::Cursor(TransactionBase* aTransaction,
               Type aType,
               int64_t aObjectStoreId,
               int64_t aIndexId,
               Direction aDirection)
  : mTransaction(aTransaction)
  , mBackgroundParent(nullptr)
  , mObjectStoreId(aObjectStoreId)
  , mIndexId(aIndexId)
  , mCurrentlyRunningOp(nullptr)
  , mType(aType)
  , mDirection(aDirection)
  , mUniqueIndex(false)
  , mActorDestroyed(false)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(aType != OpenCursorParams::T__None);
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT_IF(aType == OpenCursorParams::TIndexOpenCursorParams ||
                  aType == OpenCursorParams::TIndexOpenKeyCursorParams,
                aIndexId);

  if (mType == OpenCursorParams::TObjectStoreOpenCursorParams ||
      mType == OpenCursorParams::TIndexOpenCursorParams) {
    mFileManager = aTransaction->GetDatabase()->GetFileManager();
    MOZ_ASSERT(mFileManager);

    mBackgroundParent = aTransaction->GetBackgroundParent();
    MOZ_ASSERT(mBackgroundParent);
  }

  if (aIndexId) {
    MOZ_ASSERT(aType == OpenCursorParams::TIndexOpenCursorParams ||
                 aType == OpenCursorParams::TIndexOpenKeyCursorParams);

    nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
      aTransaction->GetMetadataForObjectStoreId(aObjectStoreId);
    MOZ_ASSERT(objectStoreMetadata);

    nsRefPtr<FullIndexMetadata> indexMetadata =
      aTransaction->GetMetadataForIndexId(objectStoreMetadata, aIndexId);
    MOZ_ASSERT(indexMetadata);

    mUniqueIndex = indexMetadata->mCommonMetadata.unique();
  }

  static_assert(OpenCursorParams::T__None == 0 &&
                  OpenCursorParams::T__Last == 4,
                "Lots of code here assumes only four types of cursors!");
}

bool
Cursor::Start(const OpenCursorParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() == mType);
  MOZ_ASSERT(!mActorDestroyed);

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  const OptionalKeyRange& optionalKeyRange =
    mType == OpenCursorParams::TObjectStoreOpenCursorParams ?
      aParams.get_ObjectStoreOpenCursorParams().optionalKeyRange() :
    mType == OpenCursorParams::TObjectStoreOpenKeyCursorParams ?
      aParams.get_ObjectStoreOpenKeyCursorParams().optionalKeyRange() :
    mType == OpenCursorParams::TIndexOpenCursorParams ?
      aParams.get_IndexOpenCursorParams().optionalKeyRange() :
      aParams.get_IndexOpenKeyCursorParams().optionalKeyRange();

  if (mTransaction->IsInvalidated()) {
    return true;
  }

  nsRefPtr<OpenOp> openOp = new OpenOp(this, optionalKeyRange);

  if (NS_WARN_IF(!openOp->Init(mTransaction))) {
    openOp->Cleanup();
    return false;
  }

  openOp->DispatchToTransactionThreadPool();
  mCurrentlyRunningOp = openOp;

  return true;
}

void
Cursor::SendResponseInternal(CursorResponse& aResponse,
                             const nsTArray<StructuredCloneFile>& aFiles)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aResponse.type() != CursorResponse::T__None);
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tnsresult,
                NS_FAILED(aResponse.get_nsresult()));
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tnsresult,
                NS_ERROR_GET_MODULE(aResponse.get_nsresult()) ==
                  NS_ERROR_MODULE_DOM_INDEXEDDB);
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tvoid_t, mKey.IsUnset());
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tvoid_t,
                mRangeKey.IsUnset());
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tvoid_t,
                mObjectKey.IsUnset());
  MOZ_ASSERT_IF(aResponse.type() == CursorResponse::Tnsresult ||
                aResponse.type() == CursorResponse::Tvoid_t ||
                aResponse.type() ==
                  CursorResponse::TObjectStoreKeyCursorResponse ||
                aResponse.type() == CursorResponse::TIndexKeyCursorResponse,
                aFiles.IsEmpty());
  MOZ_ASSERT(!mActorDestroyed);
  MOZ_ASSERT(mCurrentlyRunningOp);

  if (!aFiles.IsEmpty()) {
    MOZ_ASSERT(aResponse.type() == CursorResponse::TObjectStoreCursorResponse ||
               aResponse.type() == CursorResponse::TIndexCursorResponse);
    MOZ_ASSERT(mFileManager);
    MOZ_ASSERT(mBackgroundParent);

    FallibleTArray<PBlobParent*> actors;
    FallibleTArray<intptr_t> fileInfos;
    nsresult rv = ConvertBlobsToActors(mBackgroundParent,
                                       mFileManager,
                                       aFiles,
                                       actors,
                                       fileInfos);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResponse = ClampResultCode(rv);
    } else {
      SerializedStructuredCloneReadInfo* serializedInfo = nullptr;
      switch (aResponse.type()) {
        case CursorResponse::TObjectStoreCursorResponse:
          serializedInfo =
            &aResponse.get_ObjectStoreCursorResponse().cloneInfo();
          break;

        case CursorResponse::TIndexCursorResponse:
          serializedInfo = &aResponse.get_IndexCursorResponse().cloneInfo();
          break;

        default:
          MOZ_CRASH("Should never get here!");
      }

      MOZ_ASSERT(serializedInfo);
      MOZ_ASSERT(serializedInfo->blobsParent().IsEmpty());
      MOZ_ASSERT(serializedInfo->fileInfos().IsEmpty());

      serializedInfo->blobsParent().SwapElements(actors);
      serializedInfo->fileInfos().SwapElements(fileInfos);
    }
  }

  // Work around the deleted function by casting to the base class.
  auto* base = static_cast<PBackgroundIDBCursorParent*>(this);
  if (!base->SendResponse(aResponse)) {
    NS_WARNING("Failed to send response!");
  }

  mCurrentlyRunningOp = nullptr;
}

void
Cursor::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (mCurrentlyRunningOp) {
    mCurrentlyRunningOp->NoteActorDestroyed();
  }

  mBackgroundParent = nullptr;
}

bool
Cursor::RecvDeleteMe()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  return PBackgroundIDBCursorParent::Send__delete__(this);
}

bool
Cursor::RecvContinue(const CursorRequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != CursorRequestParams::T__None);
  MOZ_ASSERT(!mActorDestroyed);

  if (NS_WARN_IF(mCurrentlyRunningOp)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (NS_WARN_IF(mTransaction->mCommitOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (aParams.type() == CursorRequestParams::TContinueParams) {
    const Key& key = aParams.get_ContinueParams().key();
    if (!key.IsUnset()) {
      switch (mDirection) {
        case IDBCursor::NEXT:
        case IDBCursor::NEXT_UNIQUE:
          if (NS_WARN_IF(key <= mKey)) {
            ASSERT_UNLESS_FUZZING();
            return false;
          }
          break;

        case IDBCursor::PREV:
        case IDBCursor::PREV_UNIQUE:
          if (NS_WARN_IF(key >= mKey)) {
            ASSERT_UNLESS_FUZZING();
            return false;
          }
          break;

        default:
          MOZ_CRASH("Should never get here!");
      }
    }
  }

  if (mTransaction->IsInvalidated()) {
    return true;
  }

  nsRefPtr<ContinueOp> continueOp = new ContinueOp(this, aParams);
  if (NS_WARN_IF(!continueOp->Init(mTransaction))) {
    continueOp->Cleanup();
    return false;
  }

  continueOp->DispatchToTransactionThreadPool();
  mCurrentlyRunningOp = continueOp;

  return true;
}

/*******************************************************************************
 * FileManager
 ******************************************************************************/

FileManager::FileManager(PersistenceType aPersistenceType,
                         const nsACString& aGroup,
                         const nsACString& aOrigin,
                         const nsAString& aDatabaseName,
                         bool aEnforcingQuota)
  : mPersistenceType(aPersistenceType)
  , mGroup(aGroup)
  , mOrigin(aOrigin)
  , mDatabaseName(aDatabaseName)
  , mLastFileId(0)
  , mEnforcingQuota(aEnforcingQuota)
  , mInvalidated(false)
{ }

FileManager::~FileManager()
{ }

nsresult
FileManager::Init(nsIFile* aDirectory,
                  mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aConnection);

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isDirectory;
    rv = aDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aDirectory->GetPath(mDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> journalDirectory;
  rv = aDirectory->Clone(getter_AddRefs(journalDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_ConvertASCIItoUTF16 dirName(NS_LITERAL_CSTRING(kJournalDirectoryName));
  rv = journalDirectory->Append(dirName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = journalDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isDirectory;
    rv = journalDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      return NS_ERROR_FAILURE;
    }
  }

  rv = journalDirectory->GetPath(mJournalDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, refcount "
    "FROM file"
  ), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    int64_t id;
    rv = stmt->GetInt64(0, &id);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int32_t refcount;
    rv = stmt->GetInt32(1, &refcount);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(refcount > 0);

    nsRefPtr<FileInfo> fileInfo = FileInfo::Create(this, id);
    fileInfo->mDBRefCnt = static_cast<nsrefcnt>(refcount);

    mFileInfos.Put(id, fileInfo);

    mLastFileId = std::max(id, mLastFileId);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
FileManager::Invalidate()
{
  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static PLDHashOperator
    ClearDBRefs(const uint64_t& aKey, FileInfo*& aValue, void* aUserArg)
    {
      MOZ_ASSERT(aValue);

      if (aValue->LockedClearDBRefs()) {
        return PL_DHASH_NEXT;
      }

      return PL_DHASH_REMOVE;
    }
  };

  if (IndexedDatabaseManager::IsClosed()) {
    MOZ_ASSERT(false, "Shouldn't be called after shutdown!");
    return NS_ERROR_UNEXPECTED;
  }

  MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

  MOZ_ASSERT(!mInvalidated);
  mInvalidated = true;

  mFileInfos.Enumerate(Helper::ClearDBRefs, nullptr);

  return NS_OK;
}

already_AddRefed<nsIFile>
FileManager::GetDirectory()
{
  return GetFileForPath(mDirectoryPath);
}

already_AddRefed<nsIFile>
FileManager::GetJournalDirectory()
{
  return GetFileForPath(mJournalDirectoryPath);
}

already_AddRefed<nsIFile>
FileManager::EnsureJournalDirectory()
{
  // This can happen on the IO or on a transaction thread.
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIFile> journalDirectory = GetFileForPath(mJournalDirectoryPath);
  if (NS_WARN_IF(!journalDirectory)) {
    return nullptr;
  }

  bool exists;
  nsresult rv = journalDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  if (exists) {
    bool isDirectory;
    rv = journalDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    if (NS_WARN_IF(!isDirectory)) {
      return nullptr;
    }
  } else {
    rv = journalDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  return journalDirectory.forget();
}

already_AddRefed<FileInfo>
FileManager::GetFileInfo(int64_t aId)
{
  if (IndexedDatabaseManager::IsClosed()) {
    MOZ_ASSERT(false, "Shouldn't be called after shutdown!");
    return nullptr;
  }

  FileInfo* fileInfo;
  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());
    fileInfo = mFileInfos.Get(aId);
  }

  nsRefPtr<FileInfo> result = fileInfo;
  return result.forget();
}

already_AddRefed<FileInfo>
FileManager::GetNewFileInfo()
{
  MOZ_ASSERT(!IndexedDatabaseManager::IsClosed());

  FileInfo* fileInfo;
  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

    int64_t id = mLastFileId + 1;

    fileInfo = FileInfo::Create(this, id);

    mFileInfos.Put(id, fileInfo);

    mLastFileId = id;
  }

  nsRefPtr<FileInfo> result = fileInfo;
  return result.forget();
}

// static
already_AddRefed<nsIFile>
FileManager::GetFileForId(nsIFile* aDirectory, int64_t aId)
{
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aId > 0);

  nsAutoString id;
  id.AppendInt(aId);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  rv = file->Append(id);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return file.forget();
}

// static
nsresult
FileManager::InitDirectory(nsIFile* aDirectory,
                           nsIFile* aDatabaseFile,
                           PersistenceType aPersistenceType,
                           const nsACString& aGroup,
                           const nsACString& aOrigin)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aDatabaseFile);

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

  bool isDirectory;
  rv = aDirectory->IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!isDirectory)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> journalDirectory;
  rv = aDirectory->Clone(getter_AddRefs(journalDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_ConvertASCIItoUTF16 dirName(NS_LITERAL_CSTRING(kJournalDirectoryName));
  rv = journalDirectory->Append(dirName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = journalDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = journalDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = journalDirectory->GetDirectoryEntries(getter_AddRefs(entries));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool hasElements;
    rv = entries->HasMoreElements(&hasElements);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (hasElements) {
      nsCOMPtr<mozIStorageConnection> connection;
      rv = CreateDatabaseConnection(aDatabaseFile,
                                    aDirectory,
                                    NullString(),
                                    aPersistenceType,
                                    aGroup,
                                    aOrigin,
                                    getter_AddRefs(connection));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mozStorageTransaction transaction(connection, false);

      rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE VIRTUAL TABLE fs USING filesystem;"
      ));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<mozIStorageStatement> stmt;
      rv = connection->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT name, (name IN (SELECT id FROM file)) "
        "FROM fs "
        "WHERE path = :path"
      ), getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsString path;
      rv = journalDirectory->GetPath(path);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("path"), path);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      bool hasResult;
      while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
        nsString name;
        rv = stmt->GetString(0, name);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        int32_t flag = stmt->AsInt32(1);

        if (!flag) {
          nsCOMPtr<nsIFile> file;
          rv = aDirectory->Clone(getter_AddRefs(file));
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          rv = file->Append(name);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          if (NS_FAILED(file->Remove(false))) {
            NS_WARNING("Failed to remove orphaned file!");
          }
        }

        nsCOMPtr<nsIFile> journalFile;
        rv = journalDirectory->Clone(getter_AddRefs(journalFile));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = journalFile->Append(name);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (NS_FAILED(journalFile->Remove(false))) {
          NS_WARNING("Failed to remove journal file!");
        }
      }

      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP TABLE fs;"
      ));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      transaction.Commit();
    }
  }

  return NS_OK;
}

// static
nsresult
FileManager::GetUsage(nsIFile* aDirectory, uint64_t* aUsage)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aUsage);

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    *aUsage = 0;
    return NS_OK;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t usage = 0;

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    MOZ_ASSERT(file);

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (leafName.EqualsLiteral(kJournalDirectoryName)) {
      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    quota::IncrementUsage(&usage, uint64_t(fileSize));
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aUsage = usage;
  return NS_OK;
}

/*******************************************************************************
 * QuotaClient
 ******************************************************************************/

QuotaClient* QuotaClient::sInstance = nullptr;

QuotaClient::QuotaClient()
  : mShutdownRequested(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sInstance, "We expect this to be a singleton!");

  sInstance = this;
}

QuotaClient::~QuotaClient()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance == this, "We expect this to be a singleton!");

  sInstance = nullptr;
}

void
QuotaClient::NoteBackgroundThread(nsIEventTarget* aBackgroundThread)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundThread);
  MOZ_ASSERT(!mShutdownRequested);

  mBackgroundThread = aBackgroundThread;
}

mozilla::dom::quota::Client::Type
QuotaClient::GetType()
{
  return QuotaClient::IDB;
}

struct FileManagerInitInfo
{
  nsCOMPtr<nsIFile> mDirectory;
  nsCOMPtr<nsIFile> mDatabaseFile;
};

nsresult
QuotaClient::InitOrigin(PersistenceType aPersistenceType,
                        const nsACString& aGroup,
                        const nsACString& aOrigin,
                        UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> directory;
  nsresult rv =
    GetDirectory(aPersistenceType, aOrigin, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We need to see if there are any files in the directory already. If they
  // are database files then we need to cleanup stored files (if it's needed)
  // and also get the usage.

  nsAutoTArray<nsString, 20> subdirsToProcess;
  nsTArray<nsCOMPtr<nsIFile>> unknownFiles;
  nsTHashtable<nsStringHashKey> validSubdirs(20);
  nsAutoTArray<FileManagerInitInfo, 20> initInfos;

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const NS_ConvertASCIItoUTF16 filesSuffix(kFileManagerDirectoryNameSuffix);

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
         hasMore &&
         (!aUsageInfo || !aUsageInfo->Canceled())) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    MOZ_ASSERT(file);

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }


    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (isDirectory) {
      if (!StringEndsWith(leafName, filesSuffix) ||
          !validSubdirs.GetEntry(leafName)) {
        subdirsToProcess.AppendElement(leafName);
      }
      continue;
    }

    // Skip SQLite and Desktop Service Store (.DS_Store) files.
    // Desktop Service Store file is only used on Mac OS X, but the profile
    // can be shared across different operating systems, so we check it on
    // all platforms.
    if (StringEndsWith(leafName, NS_LITERAL_STRING(".sqlite-journal")) ||
        leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
      continue;
    }

    nsDependentSubstring dbBaseFilename;
    if (!GetDatabaseBaseFilename(leafName, dbBaseFilename)) {
      unknownFiles.AppendElement(file);
      continue;
    }

    nsString fmDirectoryBaseName = dbBaseFilename + filesSuffix;

    nsCOMPtr<nsIFile> fmDirectory;
    rv = directory->Clone(getter_AddRefs(fmDirectory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = fmDirectory->Append(fmDirectoryBaseName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    FileManagerInitInfo* initInfo = initInfos.AppendElement();
    initInfo->mDirectory.swap(fmDirectory);
    initInfo->mDatabaseFile.swap(file);

    validSubdirs.PutEntry(fmDirectoryBaseName);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (uint32_t count = subdirsToProcess.Length(), i = 0; i < count; i++) {
    const nsString& subdirName = subdirsToProcess[i];

    // If the directory has the correct suffix then it must exist in
    // validSubdirs.
    if (StringEndsWith(subdirName, filesSuffix)) {
      if (NS_WARN_IF(!validSubdirs.GetEntry(subdirName))) {
        return NS_ERROR_UNEXPECTED;
      }

      continue;
    }

    // The directory didn't have the right suffix but we might need to rename
    // it. Check to see if we have a database that references this directory.
    nsString subdirNameWithSuffix = subdirName + filesSuffix;
    if (!validSubdirs.GetEntry(subdirNameWithSuffix)) {
      // Windows doesn't allow a directory to end with a dot ('.'), so we have
      // to check that possibility here too.
      // We do this on all platforms, because the origin directory may have
      // been created on Windows and now accessed on different OS.
      subdirNameWithSuffix = subdirName + NS_LITERAL_STRING(".") + filesSuffix;
      if (NS_WARN_IF(!validSubdirs.GetEntry(subdirNameWithSuffix))) {
        return NS_ERROR_UNEXPECTED;
      }
    }

    // We do have a database that uses this directory so we should rename it
    // now. However, first check to make sure that we're not overwriting
    // something else.
    nsCOMPtr<nsIFile> subdir;
    rv = directory->Clone(getter_AddRefs(subdir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = subdir->Append(subdirNameWithSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool exists;
    rv = subdir->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (exists) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    rv = directory->Clone(getter_AddRefs(subdir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = subdir->Append(subdirName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    DebugOnly<bool> isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(subdir->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory);

    rv = subdir->RenameTo(nullptr, subdirNameWithSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  for (uint32_t count = initInfos.Length(), i = 0; i < count; i++) {
    FileManagerInitInfo& initInfo = initInfos[i];
    MOZ_ASSERT(initInfo.mDirectory);
    MOZ_ASSERT(initInfo.mDatabaseFile);

    rv = FileManager::InitDirectory(initInfo.mDirectory,
                                    initInfo.mDatabaseFile,
                                    aPersistenceType,
                                    aGroup,
                                    aOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (aUsageInfo && !aUsageInfo->Canceled()) {
      int64_t fileSize;
      rv = initInfo.mDatabaseFile->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(fileSize >= 0);

      aUsageInfo->AppendToDatabaseUsage(uint64_t(fileSize));

      uint64_t usage;
      rv = FileManager::GetUsage(initInfo.mDirectory, &usage);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      aUsageInfo->AppendToFileUsage(usage);
    }
  }

  // We have to do this after file manager initialization.
  for (uint32_t count = unknownFiles.Length(), i = 0; i < count; i++) {
    nsCOMPtr<nsIFile>& unknownFile = unknownFiles[i];

    // Some temporary SQLite files could disappear during file manager
    // initialization, so we have to check if the unknown file still exists.
    bool exists;
    rv = unknownFile->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (exists) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

nsresult
QuotaClient::GetUsageForOrigin(PersistenceType aPersistenceType,
                               const nsACString& aGroup,
                               const nsACString& aOrigin,
                               UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aUsageInfo);

  nsCOMPtr<nsIFile> directory;
  nsresult rv =
    GetDirectory(aPersistenceType, aOrigin, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = GetUsageForDirectoryInternal(directory, aUsageInfo, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
QuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                    const nsACString& aOrigin)
{
  AssertIsOnIOThread();

  if (IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get()) {
    mgr->InvalidateFileManagers(aPersistenceType, aOrigin);
  }
}

void
QuotaClient::ReleaseIOThreadObjects()
{
  AssertIsOnIOThread();

  if (IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get()) {
    mgr->InvalidateAllFileManagers();
  }
}

bool
QuotaClient::IsFileServiceUtilized()
{
  MOZ_ASSERT(NS_IsMainThread());

  return true;
}

bool
QuotaClient::IsTransactionServiceActivated()
{
  MOZ_ASSERT(NS_IsMainThread());

  return true;
}

void
QuotaClient::WaitForStoragesToComplete(nsTArray<nsIOfflineStorage*>& aStorages,
                                       nsIRunnable* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aStorages.IsEmpty());
  MOZ_ASSERT(aCallback);

  nsCOMPtr<nsIEventTarget> backgroundThread;
  nsTArray<nsCString> databaseIds;

  for (uint32_t count = aStorages.Length(), index = 0; index < count; index++) {
    nsIOfflineStorage* storage = aStorages[index];
    MOZ_ASSERT(storage);
    MOZ_ASSERT(storage->GetClient() == this);

    const nsACString& databaseId = storage->Id();

    if (!databaseIds.Contains(databaseId)) {
      databaseIds.AppendElement(databaseId);

      if (!backgroundThread) {
        backgroundThread =
          static_cast<DatabaseOfflineStorage*>(storage)->OwningThread();
        MOZ_ASSERT(backgroundThread);
      }
#ifdef DEBUG
      else {
        MOZ_ASSERT(backgroundThread ==
                     static_cast<DatabaseOfflineStorage*>(storage)->
                       OwningThread());
      }
#endif
    }
  }

  if (databaseIds.IsEmpty()) {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(aCallback)));
    return;
  }

  MOZ_ASSERT(backgroundThread);

  nsCOMPtr<nsIRunnable> runnable =
    new WaitForTransactionsRunnable(this, databaseIds, aCallback);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    backgroundThread->Dispatch(runnable, NS_DISPATCH_NORMAL)));
}

void
QuotaClient::ShutdownTransactionService()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mShutdownRunnable);
  MOZ_ASSERT(!mShutdownRequested);

  mShutdownRequested = true;

  if (mBackgroundThread) {
    nsRefPtr<ShutdownTransactionThreadPoolRunnable> runnable =
      new ShutdownTransactionThreadPoolRunnable(this);

    if (NS_FAILED(mBackgroundThread->Dispatch(runnable, NS_DISPATCH_NORMAL))) {
      // This can happen if the thread has shut down already.
      return;
    }

    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

    mShutdownRunnable.swap(runnable);

    while (mShutdownRunnable) {
      MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(currentThread));
    }
  }
}

nsresult
QuotaClient::GetDirectory(PersistenceType aPersistenceType,
                          const nsACString& aOrigin, nsIFile** aDirectory)
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsCOMPtr<nsIFile> directory;
  nsresult rv = quotaManager->GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                                    getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(directory);

  rv = directory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  directory.forget(aDirectory);
  return NS_OK;
}

nsresult
QuotaClient::GetUsageForDirectoryInternal(nsIFile* aDirectory,
                                          UsageInfo* aUsageInfo,
                                          bool aDatabaseFiles)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aUsageInfo);

  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!entries) {
    return NS_OK;
  }

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
         hasMore &&
         !aUsageInfo->Canceled()) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    MOZ_ASSERT(file);

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (isDirectory) {
      if (aDatabaseFiles) {
        rv = GetUsageForDirectoryInternal(file, aUsageInfo, false);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        nsString leafName;
        rv = file->GetLeafName(leafName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (!leafName.EqualsLiteral(kJournalDirectoryName)) {
          NS_WARNING("Unknown directory found!");
        }
      }

      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(fileSize >= 0);

    if (aDatabaseFiles) {
      aUsageInfo->AppendToDatabaseUsage(uint64_t(fileSize));
    } else {
      aUsageInfo->AppendToFileUsage(uint64_t(fileSize));
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}


void
QuotaClient::
WaitForTransactionsRunnable::MaybeWait()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_Initial);
  MOZ_ASSERT(mQuotaClient);

  nsRefPtr<TransactionThreadPool> threadPool = gTransactionThreadPool.get();
  if (threadPool) {
    mState = State_WaitingForTransactions;

    threadPool->WaitForDatabasesToComplete(mDatabaseIds, this);

    MOZ_ASSERT(mDatabaseIds.IsEmpty());
    return;
  }

  mDatabaseIds.Clear();

  SendToMainThread();
}

void
QuotaClient::
WaitForTransactionsRunnable::SendToMainThread()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mState == State_Initial || mState == State_WaitingForTransactions);
  MOZ_ASSERT(mDatabaseIds.IsEmpty());

  mState = State_CallingCallback;

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));
}

void
QuotaClient::
WaitForTransactionsRunnable::CallCallback()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_CallingCallback);
  MOZ_ASSERT(mDatabaseIds.IsEmpty());

  nsRefPtr<QuotaClient> quotaClient;
  mQuotaClient.swap(quotaClient);

  nsCOMPtr<nsIRunnable> callback;
  mCallback.swap(callback);

  callback->Run();

  mState = State_Complete;
}

NS_IMPL_ISUPPORTS_INHERITED0(QuotaClient::WaitForTransactionsRunnable,
                             nsRunnable)

NS_IMETHODIMP
QuotaClient::
WaitForTransactionsRunnable::Run()
{
  MOZ_ASSERT(mState != State_Complete);
  MOZ_ASSERT(mCallback);

  switch (mState) {
    case State_Initial:
      MaybeWait();
      break;

    case State_WaitingForTransactions:
      SendToMainThread();
      break;

    case State_CallingCallback:
      CallCallback();
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(QuotaClient::ShutdownTransactionThreadPoolRunnable,
                             nsRunnable)

NS_IMETHODIMP
QuotaClient::
ShutdownTransactionThreadPoolRunnable::Run()
{
  if (NS_IsMainThread()) {
    MOZ_ASSERT(mHasRequestedShutDown);
    MOZ_ASSERT(QuotaClient::GetInstance() == mQuotaClient);
    MOZ_ASSERT(mQuotaClient->mShutdownRunnable == this);

    mQuotaClient->mShutdownRunnable = nullptr;
    mQuotaClient = nullptr;

    return NS_OK;
  }

  AssertIsOnBackgroundThread();

  if (!mHasRequestedShutDown) {
    mHasRequestedShutDown = true;

    nsRefPtr<TransactionThreadPool> threadPool = gTransactionThreadPool.get();
    if (threadPool) {
      threadPool->Shutdown();

      gTransactionThreadPool = nullptr;
    }

  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));

  return NS_OK;
}

/*******************************************************************************
 * DatabaseOfflineStorage
 ******************************************************************************/

DatabaseOfflineStorage::DatabaseOfflineStorage(
                              QuotaClient* aQuotaClient,
                              const OptionalContentId& aOptionalContentParentId,
                              const nsACString& aGroup,
                              const nsACString& aOrigin,
                              const nsACString& aId,
                              PersistenceType aPersistenceType,
                              nsIEventTarget* aOwningThread)
  : mStrongQuotaClient(aQuotaClient)
  , mWeakQuotaClient(aQuotaClient)
  , mDatabase(nullptr)
  , mOptionalContentParentId(aOptionalContentParentId)
  , mOrigin(aOrigin)
  , mId(aId)
  , mOwningThread(aOwningThread)
  , mClosedOnMainThread(false)
  , mClosedOnOwningThread(false)
  , mInvalidatedOnMainThread(false)
  , mInvalidatedOnOwningThread(false)
  , mRegisteredWithQuotaManager(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aQuotaClient);
  MOZ_ASSERT(aOwningThread);

  DebugOnly<bool> current;
  MOZ_ASSERT(NS_SUCCEEDED(aOwningThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(!current);

  mGroup = aGroup;
  mPersistenceType = aPersistenceType;
}

// static
void
DatabaseOfflineStorage::UnregisterOnOwningThread(
                       already_AddRefed<DatabaseOfflineStorage> aOfflineStorage)
{
  AssertIsOnBackgroundThread();

  nsRefPtr<DatabaseOfflineStorage> offlineStorage = Move(aOfflineStorage);
  MOZ_ASSERT(offlineStorage);
  MOZ_ASSERT(offlineStorage->mClosedOnOwningThread);

  offlineStorage->mDatabase = nullptr;

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(offlineStorage.get(),
                         &DatabaseOfflineStorage::UnregisterOnMainThread);
  MOZ_ASSERT(runnable);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
}

void
DatabaseOfflineStorage::CloseOnOwningThread()
{
  AssertIsOnBackgroundThread();

  if (mClosedOnOwningThread) {
    return;
  }

  mClosedOnOwningThread = true;

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &DatabaseOfflineStorage::CloseOnMainThread);
  MOZ_ASSERT(runnable);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
}

void
DatabaseOfflineStorage::CloseOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mClosedOnMainThread) {
    return;
  }

  mClosedOnMainThread = true;
}

void
DatabaseOfflineStorage::InvalidateOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mInvalidatedOnMainThread) {
    return;
  }

  mInvalidatedOnMainThread = true;

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this,
                         &DatabaseOfflineStorage::InvalidateOnOwningThread);
  MOZ_ASSERT(runnable);

  nsCOMPtr<nsIEventTarget> owningThread = mOwningThread;
  MOZ_ASSERT(owningThread);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(owningThread->Dispatch(runnable,
                                                      NS_DISPATCH_NORMAL)));
}

void
DatabaseOfflineStorage::InvalidateOnOwningThread()
{
  AssertIsOnBackgroundThread();

  if (mInvalidatedOnOwningThread) {
    return;
  }

  mInvalidatedOnOwningThread = true;

  if (nsRefPtr<Database> database = mDatabase) {
    mDatabase = nullptr;

    database->Invalidate();
  }
}

void
DatabaseOfflineStorage::UnregisterOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwningThread);
  MOZ_ASSERT(mRegisteredWithQuotaManager);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  quotaManager->UnregisterStorage(this);
  mRegisteredWithQuotaManager = false;

  mStrongQuotaClient = nullptr;

  mOwningThread = nullptr;
}

NS_IMPL_ISUPPORTS(DatabaseOfflineStorage, nsIOfflineStorage)

NS_IMETHODIMP_(const nsACString&)
DatabaseOfflineStorage::Id()
{
  return mId;
}

NS_IMETHODIMP_(Client*)
DatabaseOfflineStorage::GetClient()
{
  MOZ_ASSERT(NS_IsMainThread());

  return mWeakQuotaClient;
}

NS_IMETHODIMP_(bool)
DatabaseOfflineStorage::IsOwnedByProcess(ContentParent* aOwner)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aOwner);

  return mOptionalContentParentId.type() ==
           OptionalContentId::TContentParentId &&
         mOptionalContentParentId.get_ContentParentId() == aOwner->ChildID();
}

NS_IMETHODIMP_(const nsACString&)
DatabaseOfflineStorage::Origin()
{
  return mOrigin;
}

NS_IMETHODIMP_(nsresult)
DatabaseOfflineStorage::Close()
{
  MOZ_ASSERT(NS_IsMainThread());

  InvalidateOnMainThread();
  return NS_OK;
}

NS_IMETHODIMP_(void)
DatabaseOfflineStorage::Invalidate()
{
  MOZ_ASSERT(NS_IsMainThread());

  InvalidateOnMainThread();
}

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

NS_IMPL_ISUPPORTS(CompressDataBlobsFunction, mozIStorageFunction)
NS_IMPL_ISUPPORTS(EncodeKeysFunction, mozIStorageFunction)

// static
void
DatabaseOperationBase::GetBindingClauseForKeyRange(
                                            const SerializedKeyRange& aKeyRange,
                                            const nsACString& aKeyColumnName,
                                            nsAutoCString& aBindingClause)
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(!aKeyColumnName.IsEmpty());

  NS_NAMED_LITERAL_CSTRING(andStr, " AND ");
  NS_NAMED_LITERAL_CSTRING(spacecolon, " :");
  NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

  if (aKeyRange.isOnly()) {
    // Both keys equal.
    aBindingClause = andStr + aKeyColumnName + NS_LITERAL_CSTRING(" =") +
                     spacecolon + lowerKey;
    return;
  }

  aBindingClause.Truncate();

  if (!aKeyRange.lower().IsUnset()) {
    // Lower key is set.
    aBindingClause.Append(andStr + aKeyColumnName);
    aBindingClause.AppendLiteral(" >");
    if (!aKeyRange.lowerOpen()) {
      aBindingClause.AppendLiteral("=");
    }
    aBindingClause.Append(spacecolon + lowerKey);
  }

  if (!aKeyRange.upper().IsUnset()) {
    // Upper key is set.
    aBindingClause.Append(andStr + aKeyColumnName);
    aBindingClause.AppendLiteral(" <");
    if (!aKeyRange.upperOpen()) {
      aBindingClause.AppendLiteral("=");
    }
    aBindingClause.Append(spacecolon + NS_LITERAL_CSTRING("upper_key"));
  }

  MOZ_ASSERT(!aBindingClause.IsEmpty());
}

// static
uint64_t
DatabaseOperationBase::ReinterpretDoubleAsUInt64(double aDouble)
{
  // This is a duplicate of the js engine's byte munging in StructuredClone.cpp
  union {
    double d;
    uint64_t u;
  } pun;
  pun.d = aDouble;
  return pun.u;
}

// static
nsresult
DatabaseOperationBase::GetStructuredCloneReadInfoFromStatement(
                                               mozIStorageStatement* aStatement,
                                               uint32_t aDataIndex,
                                               uint32_t aFileIdsIndex,
                                               FileManager* aFileManager,
                                               StructuredCloneReadInfo* aInfo)
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aStatement);
  MOZ_ASSERT(aFileManager);

  PROFILER_LABEL("IndexedDB",
                 "DatabaseOperationBase::"
                 "GetStructuredCloneReadInfoFromStatement",
                 js::ProfileEntry::Category::STORAGE);

#ifdef DEBUG
  {
    int32_t type;
    MOZ_ASSERT(NS_SUCCEEDED(aStatement->GetTypeOfIndex(aDataIndex, &type)));
    MOZ_ASSERT(type == mozIStorageStatement::VALUE_TYPE_BLOB);
  }
#endif

  const uint8_t* blobData;
  uint32_t blobDataLength;
  nsresult rv =
    aStatement->GetSharedBlob(aDataIndex, &blobDataLength, &blobData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const char* compressed = reinterpret_cast<const char*>(blobData);
  size_t compressedLength = size_t(blobDataLength);

  size_t uncompressedLength;
  if (NS_WARN_IF(!snappy::GetUncompressedLength(compressed, compressedLength,
                                                &uncompressedLength))) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  FallibleTArray<uint8_t> uncompressed;
  if (NS_WARN_IF(!uncompressed.SetLength(uncompressedLength))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char* uncompressedBuffer = reinterpret_cast<char*>(uncompressed.Elements());

  if (NS_WARN_IF(!snappy::RawUncompress(compressed, compressedLength,
                                        uncompressedBuffer))) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  aInfo->mData.SwapElements(uncompressed);

  bool isNull;
  rv = aStatement->GetIsNull(aFileIdsIndex, &isNull);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isNull) {
    nsString ids;
    rv = aStatement->GetString(aFileIdsIndex, ids);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoTArray<int64_t, 10> array;
    rv = ConvertFileIdsToArray(ids, array);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    for (uint32_t count = array.Length(), index = 0; index < count; index++) {
      MOZ_ASSERT(array[index] > 0);

      nsRefPtr<FileInfo> fileInfo = aFileManager->GetFileInfo(array[index]);
      MOZ_ASSERT(fileInfo);

      StructuredCloneFile* file = aInfo->mFiles.AppendElement();
      file->mFileInfo.swap(fileInfo);
    }
  }

  return NS_OK;
}

// static
nsresult
DatabaseOperationBase::BindKeyRangeToStatement(
                                            const SerializedKeyRange& aKeyRange,
                                            mozIStorageStatement* aStatement)
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aStatement);

  NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

  if (aKeyRange.isOnly()) {
    return aKeyRange.lower().BindToStatement(aStatement, lowerKey);
  }

  nsresult rv;

  if (!aKeyRange.lower().IsUnset()) {
    rv = aKeyRange.lower().BindToStatement(aStatement, lowerKey);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (!aKeyRange.upper().IsUnset()) {
    rv = aKeyRange.upper().BindToStatement(aStatement,
                                           NS_LITERAL_CSTRING("upper_key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

// static
void
DatabaseOperationBase::AppendConditionClause(const nsACString& aColumnName,
                                             const nsACString& aArgName,
                                             bool aLessThan,
                                             bool aEquals,
                                             nsAutoCString& aResult)
{
  aResult += NS_LITERAL_CSTRING(" AND ") + aColumnName +
             NS_LITERAL_CSTRING(" ");

  if (aLessThan) {
    aResult.Append('<');
  }
  else {
    aResult.Append('>');
  }

  if (aEquals) {
    aResult.Append('=');
  }

  aResult += NS_LITERAL_CSTRING(" :") + aArgName;
}

// static
nsresult
DatabaseOperationBase::UpdateIndexes(
                              TransactionBase* aTransaction,
                              const UniqueIndexTable& aUniqueIndexTable,
                              const Key& aObjectStoreKey,
                              bool aOverwrite,
                              int64_t aObjectDataId,
                              const nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(!aObjectStoreKey.IsUnset());

  PROFILER_LABEL("IndexedDB",
                 "DatabaseOperationBase::UpdateIndexes",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv;
  NS_NAMED_LITERAL_CSTRING(objectDataId, "object_data_id");

  if (aOverwrite) {
    TransactionBase::CachedStatement stmt;
    rv = aTransaction->GetCachedStatement(
        "DELETE FROM unique_index_data "
        "WHERE object_data_id = :object_data_id; "
        "DELETE FROM index_data "
        "WHERE object_data_id = :object_data_id",
        &stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByName(objectDataId, aObjectDataId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Avoid lots of hash lookups for objectStores with lots of indexes by lazily
  // holding the necessary statements on the stack outside the loop.
  TransactionBase::CachedStatement insertUniqueStmt;
  TransactionBase::CachedStatement insertStmt;

  for (uint32_t idxCount = aUpdateInfoArray.Length(), idxIndex = 0;
       idxIndex < idxCount;
       idxIndex++) {
    const IndexUpdateInfo& updateInfo = aUpdateInfoArray[idxIndex];

    bool unique;
    MOZ_ALWAYS_TRUE(aUniqueIndexTable.Get(updateInfo.indexId(), &unique));

    TransactionBase::CachedStatement& stmt =
      unique ? insertUniqueStmt : insertStmt;

    if (stmt) {
      stmt.Reset();
    } else if (unique) {
      rv = aTransaction->GetCachedStatement(
        "INSERT INTO unique_index_data "
          "(index_id, object_data_id, object_data_key, value) "
        "VALUES (:index_id, :object_data_id, :object_data_key, :value)",
        &stmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      rv = aTransaction->GetCachedStatement(
        "INSERT OR IGNORE INTO index_data ("
          "index_id, object_data_id, object_data_key, value) "
        "VALUES (:index_id, :object_data_id, :object_data_key, :value)",
        &stmt);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                               updateInfo.indexId());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByName(objectDataId, aObjectDataId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aObjectStoreKey.BindToStatement(stmt,
                                         NS_LITERAL_CSTRING("object_data_key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = updateInfo.value().BindToStatement(stmt, NS_LITERAL_CSTRING("value"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (rv == NS_ERROR_STORAGE_CONSTRAINT && unique) {
      // If we're inserting multiple entries for the same unique index, then
      // we might have failed to insert due to colliding with another entry for
      // the same index in which case we should ignore it.
      for (int32_t index = int32_t(idxIndex) - 1;
           index >= 0 &&
           aUpdateInfoArray[index].indexId() == updateInfo.indexId();
           --index) {
        if (updateInfo.value() == aUpdateInfoArray[index].value()) {
          // We found a key with the same value for the same index. So we
          // must have had a collision with a value we just inserted.
          rv = NS_OK;
          break;
        }
      }
    }

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(DatabaseOperationBase,
                            nsRunnable,
                            mozIStorageProgressHandler)

NS_IMETHODIMP
DatabaseOperationBase::OnProgress(mozIStorageConnection* aConnection,
                                  bool* _retval)
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(_retval);

  // This is intentionally racy.
  *_retval = !OperationMayProceed();
  return NS_OK;
}

DatabaseOperationBase::
AutoSetProgressHandler::~AutoSetProgressHandler()
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT_IF(mConnection, mDEBUGDatabaseOp);

  if (mConnection) {
    nsCOMPtr<mozIStorageProgressHandler> oldHandler;
    nsresult rv =
      mConnection->RemoveProgressHandler(getter_AddRefs(oldHandler));
    if (NS_SUCCEEDED(rv)) {
      MOZ_ASSERT(SameCOMIdentity(oldHandler,
                                 static_cast<nsIRunnable*>(mDEBUGDatabaseOp)));
    } else {
      NS_WARNING("Failed to remove progress handler!");
    }
  }
}

nsresult
DatabaseOperationBase::
AutoSetProgressHandler::Register(
                             DatabaseOperationBase* aDatabaseOp,
                             const nsCOMPtr<mozIStorageConnection>& aConnection)
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aDatabaseOp);
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!mConnection);
  MOZ_ASSERT(!mDEBUGDatabaseOp);

  nsCOMPtr<mozIStorageProgressHandler> oldProgressHandler;

  nsresult rv =
    aConnection->SetProgressHandler(kStorageProgressGranularity, aDatabaseOp,
                                    getter_AddRefs(oldProgressHandler));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!oldProgressHandler);

  mConnection = aConnection;
  mDEBUGDatabaseOp = aDatabaseOp;

  return NS_OK;
}

FactoryOp::FactoryOp(Factory* aFactory,
                     already_AddRefed<ContentParent> aContentParent,
                     const CommonFactoryRequestParams& aCommonParams,
                     bool aDeleting)
  : DatabaseOperationBase(aFactory->GetLoggingInfo()->Id(),
                          aFactory->GetLoggingInfo()->NextRequestSN())
  , mFactory(aFactory)
  , mContentParent(Move(aContentParent))
  , mCommonParams(aCommonParams)
  , mState(State_Initial)
  , mIsApp(false)
  , mEnforcingQuota(true)
  , mDeleting(aDeleting)
  , mBlockedQuotaManager(false)
  , mChromeWriteAccessAllowed(false)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnNonMainThread());
}

nsresult
FactoryOp::Open()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_Initial);

  // Swap this to the stack now to ensure that we release it on this thread.
  nsRefPtr<ContentParent> contentParent;
  mContentParent.swap(contentParent);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnMainThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  PermissionRequestBase::PermissionValue permission;
  nsresult rv = CheckPermission(contentParent, &permission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
             permission == PermissionRequestBase::kPermissionDenied ||
             permission == PermissionRequestBase::kPermissionPrompt);

  if (permission == PermissionRequestBase::kPermissionDenied) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  {
    // These services have to be started on the main thread currently.
    if (NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate())) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    nsCOMPtr<mozIStorageService> ss;
    if (NS_WARN_IF(!(ss = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID)))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (NS_WARN_IF(!QuotaManager::GetOrCreate())) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  const DatabaseMetadata& metadata = mCommonParams.metadata();

  QuotaManager::GetStorageId(metadata.persistenceType(),
                             mOrigin,
                             Client::IDB,
                             metadata.name(),
                             mDatabaseId);

  if (permission == PermissionRequestBase::kPermissionPrompt) {
    mState = State_PermissionChallenge;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mOwningThread->Dispatch(this,
                                                         NS_DISPATCH_NORMAL)));
    return NS_OK;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed);

  rv = FinishOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
FactoryOp::ChallengePermission()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_PermissionChallenge);

  const PrincipalInfo& principalInfo = mCommonParams.principalInfo();
  MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

  if (NS_WARN_IF(!SendPermissionChallenge(principalInfo))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
FactoryOp::RetryCheckPermission()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_PermissionRetry);
  MOZ_ASSERT(mCommonParams.principalInfo().type() ==
               PrincipalInfo::TContentPrincipalInfo);

  // Swap this to the stack now to ensure that we release it on this thread.
  nsRefPtr<ContentParent> contentParent;
  mContentParent.swap(contentParent);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnMainThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  PermissionRequestBase::PermissionValue permission;
  nsresult rv = CheckPermission(contentParent, &permission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed ||
             permission == PermissionRequestBase::kPermissionDenied ||
             permission == PermissionRequestBase::kPermissionPrompt);

  if (permission == PermissionRequestBase::kPermissionDenied ||
      permission == PermissionRequestBase::kPermissionPrompt) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  MOZ_ASSERT(permission == PermissionRequestBase::kPermissionAllowed);

  rv = FinishOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
FactoryOp::SendToIOThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_OpenPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnMainThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread.
  mState = State_DatabaseWorkOpen;

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void
FactoryOp::WaitForTransactions()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_BeginVersionChange ||
             mState == State_WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mDatabaseId.IsEmpty());
  MOZ_ASSERT(!IsActorDestroyed());

  nsTArray<nsCString> databaseIds;
  databaseIds.AppendElement(mDatabaseId);

  nsRefPtr<TransactionThreadPool> threadPool = gTransactionThreadPool.get();
  MOZ_ASSERT(threadPool);

  // WaitForDatabasesToComplete() will run this op immediately if there are no
  // transactions blocking it, so be sure to set the next state here before
  // calling it.
  mState = State_WaitingForTransactionsToComplete;

  threadPool->WaitForDatabasesToComplete(databaseIds, this);
  return;
}

void
FactoryOp::FinishSendResults()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_SendingResults);
  MOZ_ASSERT(mFactory);

  // Make sure to release the factory on this thread.
  nsRefPtr<Factory> factory;
  mFactory.swap(factory);

  if (mBlockedQuotaManager) {
    // Must set mState before dispatching otherwise we will race with the main
    // thread.
    mState = State_UnblockingQuotaManager;

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));
  } else {
    mState = State_Completed;
  }
}

void
FactoryOp::UnblockQuotaManager()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_UnblockingQuotaManager);

  Nullable<PersistenceType> persistenceType(
    const_cast<PersistenceType&>(mCommonParams.metadata().persistenceType()));

  if (QuotaManager* quotaManager = QuotaManager::Get()) {
    quotaManager->
      AllowNextSynchronizedOp(OriginOrPatternString::FromOrigin(mOrigin),
                              persistenceType,
                              mDatabaseId);
  } else {
    NS_WARNING("QuotaManager went away before we could unblock it!");
  }

  mState = State_Completed;
}

nsresult
FactoryOp::CheckPermission(ContentParent* aContentParent,
                           PermissionRequestBase::PermissionValue* aPermission)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_Initial || mState == State_PermissionRetry);

  const PrincipalInfo& principalInfo = mCommonParams.principalInfo();
  if (principalInfo.type() != PrincipalInfo::TSystemPrincipalInfo &&
      NS_WARN_IF(!Preferences::GetBool(kPrefIndexedDBEnabled, false))) {
    if (aContentParent) {
      // The DOM in the other process should have kept us from receiving any
      // indexedDB messages so assume that the child is misbehaving.
      aContentParent->KillHard("IndexedDB CheckPermission 1");
    }
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  if (NS_WARN_IF(mCommonParams.privateBrowsingMode())) {
    // XXX This is only temporary.
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  MOZ_ASSERT(principalInfo.type() != PrincipalInfo::TNullPrincipalInfo);

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    MOZ_ASSERT(mState == State_Initial);
    MOZ_ASSERT(persistenceType == PERSISTENCE_TYPE_PERSISTENT);

    if (aContentParent) {
      // Check to make sure that the child process has access to the database it
      // is accessing.
      NS_NAMED_LITERAL_CSTRING(permissionStringBase,
                               kPermissionStringChromeBase);
      NS_ConvertUTF16toUTF8 databaseName(mCommonParams.metadata().name());
      NS_NAMED_LITERAL_CSTRING(readSuffix, kPermissionStringChromeReadSuffix);
      NS_NAMED_LITERAL_CSTRING(writeSuffix, kPermissionStringChromeWriteSuffix);

      const nsAutoCString permissionStringWrite =
        permissionStringBase + databaseName + writeSuffix;
      const nsAutoCString permissionStringRead =
        permissionStringBase + databaseName + readSuffix;

      bool canWrite =
        CheckAtLeastOneAppHasPermission(aContentParent, permissionStringWrite);

      bool canRead;
      if (canWrite) {
        MOZ_ASSERT(CheckAtLeastOneAppHasPermission(aContentParent,
                                                   permissionStringRead));
        canRead = true;
      } else {
        canRead =
          CheckAtLeastOneAppHasPermission(aContentParent, permissionStringRead);
      }

      // Deleting a database requires write permissions.
      if (mDeleting && !canWrite) {
        aContentParent->KillHard("IndexedDB CheckPermission 2");
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      // Opening or deleting requires read permissions.
      if (!canRead) {
        aContentParent->KillHard("IndexedDB CheckPermission 3");
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      mChromeWriteAccessAllowed = canWrite;
    } else {
      mChromeWriteAccessAllowed = true;
    }

    if (State_Initial == mState) {
      QuotaManager::GetInfoForChrome(&mGroup, &mOrigin, &mIsApp);

      MOZ_ASSERT(!QuotaManager::IsFirstPromptRequired(persistenceType, mOrigin,
                                                      mIsApp));

      mEnforcingQuota =
        QuotaManager::IsQuotaEnforced(persistenceType, mOrigin, mIsApp);
    }

    *aPermission = PermissionRequestBase::kPermissionAllowed;
    return NS_OK;
  }

  MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(principalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString group;
  nsCString origin;
  bool isApp;
  rv = QuotaManager::GetInfoFromPrincipal(principal, &group, &origin, &isApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
  if (persistenceType == PERSISTENCE_TYPE_PERSISTENT &&
      !QuotaManager::IsOriginWhitelistedForPersistentStorage(origin) &&
      !isApp) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }
#endif

  PermissionRequestBase::PermissionValue permission;

  if (QuotaManager::IsFirstPromptRequired(persistenceType, origin, isApp)) {
#ifdef MOZ_CHILD_PERMISSIONS
    if (aContentParent) {
      if (NS_WARN_IF(!AssertAppPrincipal(aContentParent, principal))) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      uint32_t intPermission =
        mozilla::CheckPermission(aContentParent, principal, kPermissionString);

      permission =
        PermissionRequestBase::PermissionValueForIntPermission(intPermission);
    } else
#endif // MOZ_CHILD_PERMISSIONS
    {
      rv = PermissionRequestBase::GetCurrentPermission(principal, &permission);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  } else {
    permission = PermissionRequestBase::kPermissionAllowed;
  }

  if (permission != PermissionRequestBase::kPermissionDenied &&
      State_Initial == mState) {
    mGroup = group;
    mOrigin = origin;
    mIsApp = isApp;

    mEnforcingQuota =
      QuotaManager::IsQuotaEnforced(persistenceType, mOrigin, mIsApp);
  }

  *aPermission = permission;
  return NS_OK;
}

nsresult
FactoryOp::SendVersionChangeMessages(DatabaseActorInfo* aDatabaseActorInfo,
                                     Database* aOpeningDatabase,
                                     uint64_t aOldVersion,
                                     const NullableVersion& aNewVersion)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabaseActorInfo);
  MOZ_ASSERT(mState == State_BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(!IsActorDestroyed());

  const uint32_t expectedCount = mDeleting ? 0 : 1;
  const uint32_t liveCount = aDatabaseActorInfo->mLiveDatabases.Length();
  if (liveCount > expectedCount) {
    FallibleTArray<MaybeBlockedDatabaseInfo> maybeBlockedDatabases;
    for (uint32_t index = 0; index < liveCount; index++) {
      Database* database = aDatabaseActorInfo->mLiveDatabases[index];
      if ((!aOpeningDatabase || database != aOpeningDatabase) &&
          !database->IsClosed() &&
          NS_WARN_IF(!maybeBlockedDatabases.AppendElement(database))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    if (!maybeBlockedDatabases.IsEmpty()) {
      mMaybeBlockedDatabases.SwapElements(maybeBlockedDatabases);
    }
  }

  if (!mMaybeBlockedDatabases.IsEmpty()) {
    for (uint32_t count = mMaybeBlockedDatabases.Length(), index = 0;
         index < count;
         /* incremented conditionally */) {
      if (mMaybeBlockedDatabases[index]->SendVersionChange(aOldVersion,
                                                           aNewVersion)) {
        index++;
      } else {
        // We don't want to wait forever if we were not able to send the
        // message.
        mMaybeBlockedDatabases.RemoveElementAt(index);
        count--;
      }
    }
  }

  return NS_OK;
}

// static
bool
FactoryOp::CheckAtLeastOneAppHasPermission(ContentParent* aContentParent,
                                           const nsACString& aPermissionString)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContentParent);
  MOZ_ASSERT(!aPermissionString.IsEmpty());

#ifdef MOZ_CHILD_PERMISSIONS
  const nsTArray<PBrowserParent*>& browsers =
    aContentParent->ManagedPBrowserParent();

  if (!browsers.IsEmpty()) {
    nsCOMPtr<nsIAppsService> appsService =
      do_GetService(APPS_SERVICE_CONTRACTID);
    if (NS_WARN_IF(!appsService)) {
      return false;
    }

    nsCOMPtr<nsIIOService> ioService = do_GetIOService();
    if (NS_WARN_IF(!ioService)) {
      return false;
    }

    nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (NS_WARN_IF(!secMan)) {
      return false;
    }

    nsCOMPtr<nsIPermissionManager> permMan =
      mozilla::services::GetPermissionManager();
    if (NS_WARN_IF(!permMan)) {
      return false;
    }

    const nsPromiseFlatCString permissionString =
      PromiseFlatCString(aPermissionString);

    for (uint32_t index = 0, count = browsers.Length();
         index < count;
         index++) {
      uint32_t appId =
        TabParent::GetFrom(browsers[index])->OwnOrContainingAppId();
      MOZ_ASSERT(kUnknownAppId != appId && kNoAppId != appId);

      nsCOMPtr<mozIApplication> app;
      nsresult rv = appsService->GetAppByLocalId(appId, getter_AddRefs(app));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      nsString origin;
      rv = app->GetOrigin(origin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), origin, nullptr, nullptr, ioService);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      nsCOMPtr<nsIPrincipal> principal;
      rv = secMan->GetAppCodebasePrincipal(uri, appId, false,
                                           getter_AddRefs(principal));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      uint32_t permission;
      rv = permMan->TestExactPermissionFromPrincipal(principal,
                                                     permissionString.get(),
                                                     &permission);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      if (permission == nsIPermissionManager::ALLOW_ACTION) {
        return true;
      }
    }
  }

  return false;
#else
  return true;
#endif // MOZ_CHILD_PERMISSIONS
}

nsresult
FactoryOp::FinishOpen()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_Initial || mState == State_PermissionRetry);
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDatabaseId.IsEmpty());
  MOZ_ASSERT(!mBlockedQuotaManager);
  MOZ_ASSERT(!mContentParent);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnMainThread());

  QuotaManager* quotaManager = QuotaManager::GetOrCreate();
  if (NS_WARN_IF(!quotaManager)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  nsresult rv =
    quotaManager->
      WaitForOpenAllowed(OriginOrPatternString::FromOrigin(mOrigin),
                         Nullable<PersistenceType>(persistenceType),
                         mDatabaseId,
                         this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mBlockedQuotaManager = true;

  mState = State_OpenPending;
  return NS_OK;
}

void
FactoryOp::NoteDatabaseBlocked(Database* aDatabase)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(mMaybeBlockedDatabases.Contains(aDatabase));

  // Only send the blocked event if all databases have reported back. If the
  // database was closed then it will have been removed from the array.
  // Otherwise if it was blocked its |mBlocked| flag will be true.
  bool sendBlockedEvent = true;

  for (uint32_t count = mMaybeBlockedDatabases.Length(), index = 0;
       index < count;
       index++) {
    MaybeBlockedDatabaseInfo& info = mMaybeBlockedDatabases[index];
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

NS_IMETHODIMP
FactoryOp::Run()
{
  nsresult rv;

  switch (mState) {
    case State_Initial:
      rv = Open();
      break;

    case State_OpenPending:
      rv = QuotaManagerOpen();
      break;

    case State_PermissionChallenge:
      rv = ChallengePermission();
      break;

    case State_PermissionRetry:
      rv = RetryCheckPermission();
      break;

    case State_DatabaseWorkOpen:
      rv = DoDatabaseWork();
      break;

    case State_BeginVersionChange:
      rv = BeginVersionChange();
      break;

    case State_WaitingForTransactionsToComplete:
      rv = DispatchToWorkThread();
      break;

    case State_SendingResults:
      SendResults();
      return NS_OK;

    case State_UnblockingQuotaManager:
      UnblockQuotaManager();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State_SendingResults) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State_SendingResults;

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mOwningThread->Dispatch(this,
                                                         NS_DISPATCH_NORMAL)));
  }

  return NS_OK;
}

void
FactoryOp::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  NoteActorDestroyed();
}

bool
FactoryOp::RecvPermissionRetry()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!IsActorDestroyed());
  MOZ_ASSERT(mState == State_PermissionChallenge);

  mContentParent = BackgroundParent::GetContentParent(Manager()->Manager());

  mState = State_PermissionRetry;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));

  return true;
}

OpenDatabaseOp::OpenDatabaseOp(Factory* aFactory,
                               already_AddRefed<ContentParent> aContentParent,
                               const CommonFactoryRequestParams& aParams)
  : FactoryOp(aFactory, Move(aContentParent), aParams, /* aDeleting */ false)
  , mMetadata(new FullDatabaseMetadata(aParams.metadata()))
  , mRequestedVersion(aParams.metadata().version())
{
  auto& optionalContentParentId =
    const_cast<OptionalContentId&>(mOptionalContentParentId);

  if (mContentParent) {
    // This is a little scary but it looks safe to call this off the main thread
    // for now.
    optionalContentParentId = mContentParent->ChildID();
  } else {
    optionalContentParentId = void_t();
  }
}

nsresult
OpenDatabaseOp::QuotaManagerOpen()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_OpenPending);
  MOZ_ASSERT(!mOfflineStorage);

  QuotaClient* quotaClient = QuotaClient::GetInstance();
  if (NS_WARN_IF(!quotaClient) ||
      NS_WARN_IF(quotaClient->IsShuttingDown())) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsRefPtr<DatabaseOfflineStorage> offlineStorage =
    new DatabaseOfflineStorage(quotaClient,
                               mOptionalContentParentId,
                               mGroup,
                               mOrigin,
                               mDatabaseId,
                               mCommonParams.metadata().persistenceType(),
                               mOwningThread);

  if (NS_WARN_IF(!quotaManager->RegisterStorage(offlineStorage))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  offlineStorage->NoteRegisteredWithQuotaManager();

  quotaClient->NoteBackgroundThread(mOwningThread);

  mOfflineStorage.swap(offlineStorage);

  nsresult rv = SendToIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
OpenDatabaseOp::DoDatabaseWork()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DatabaseWorkOpen);

  PROFILER_LABEL("IndexedDB",
                 "OpenDatabaseOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const nsString& databaseName = mCommonParams.metadata().name();
  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> dbDirectory;

  nsresult rv =
    quotaManager->EnsureOriginIsInitialized(persistenceType,
                                            mGroup,
                                            mOrigin,
                                            mIsApp,
                                            getter_AddRefs(dbDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbDirectory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = dbDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    rv = dbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#ifdef DEBUG
  else {
    bool isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(dbDirectory->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory);
  }
#endif

  nsAutoString filename;
  GetDatabaseFilename(databaseName, filename);

  nsCOMPtr<nsIFile> dbFile;
  rv = dbDirectory->Clone(getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->GetPath(mDatabaseFilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> fmDirectory;
  rv = dbDirectory->Clone(getter_AddRefs(fmDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const NS_ConvertASCIItoUTF16 filesSuffix(kFileManagerDirectoryNameSuffix);

  rv = fmDirectory->Append(filename + filesSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateDatabaseConnection(dbFile,
                                fmDirectory,
                                databaseName,
                                persistenceType,
                                mGroup,
                                mOrigin,
                                getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoSetProgressHandler asph;
  rv = asph.Register(this, connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = LoadDatabaseInformation(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(mMetadata->mNextObjectStoreId > mMetadata->mObjectStores.Count());
  MOZ_ASSERT(mMetadata->mNextIndexId > 0);

  // See if we need to do a versionchange transaction

  // Optional version semantics.
  if (!mRequestedVersion) {
    // If the requested version was not specified and the database was created,
    // treat it as if version 1 were requested.
    if (mMetadata->mCommonMetadata.version() == 0) {
      mRequestedVersion = 1;
    } else {
      // Otherwise, treat it as if the current version were requested.
      mRequestedVersion = mMetadata->mCommonMetadata.version();
    }
  }

  if (NS_WARN_IF(mMetadata->mCommonMetadata.version() > mRequestedVersion)) {
    return NS_ERROR_DOM_INDEXEDDB_VERSION_ERR;
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  MOZ_ASSERT(mgr);

  nsRefPtr<FileManager> fileManager =
    mgr->GetFileManager(persistenceType, mOrigin, databaseName);
  if (!fileManager) {
    fileManager = new FileManager(persistenceType,
                                  mGroup,
                                  mOrigin,
                                  databaseName,
                                  mEnforcingQuota);

    rv = fileManager->Init(fmDirectory, connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mgr->AddFileManager(fileManager);
  }

  mFileManager = fileManager.forget();

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  mState = (mMetadata->mCommonMetadata.version() == mRequestedVersion) ?
           State_SendingResults :
           State_BeginVersionChange;

  rv = mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
OpenDatabaseOp::LoadDatabaseInformation(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(mMetadata);

  // Load version information.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, version "
    "FROM database"
  ), getter_AddRefs(stmt));
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

  nsString databaseName;
  rv = stmt->GetString(0, databaseName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(mCommonParams.metadata().name() != databaseName)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  int64_t version;
  rv = stmt->GetInt64(1, &version);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mMetadata->mCommonMetadata.version() = uint64_t(version);

  ObjectStoreTable& objectStores = mMetadata->mObjectStores;

  // Load object store names and ids.
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, auto_increment, name, key_path "
    "FROM object_store"
  ), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Maybe<nsTHashtable<nsUint64HashKey>> usedIds;
  Maybe<nsTHashtable<nsStringHashKey>> usedNames;

  int64_t lastObjectStoreId = 0;

  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    int64_t objectStoreId;
    rv = stmt->GetInt64(0, &objectStoreId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!usedIds) {
      usedIds.emplace();
    }

    if (NS_WARN_IF(objectStoreId <= 0) ||
        NS_WARN_IF(usedIds.ref().Contains(objectStoreId))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (NS_WARN_IF(!usedIds.ref().PutEntry(objectStoreId, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsString name;
    rv = stmt->GetString(2, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!usedNames) {
      usedNames.emplace();
    }

    if (NS_WARN_IF(usedNames.ref().Contains(name))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (NS_WARN_IF(!usedNames.ref().PutEntry(name, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsRefPtr<FullObjectStoreMetadata> metadata = new FullObjectStoreMetadata();
    metadata->mCommonMetadata.id() = objectStoreId;
    metadata->mCommonMetadata.name() = name;

    int32_t columnType;
    rv = stmt->GetTypeOfIndex(3, &columnType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (columnType == mozIStorageStatement::VALUE_TYPE_NULL) {
      metadata->mCommonMetadata.keyPath() = KeyPath(0);
    } else {
      MOZ_ASSERT(columnType == mozIStorageStatement::VALUE_TYPE_TEXT);

      nsString keyPathSerialization;
      rv = stmt->GetString(3, keyPathSerialization);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      metadata->mCommonMetadata.keyPath() =
        KeyPath::DeserializeFromString(keyPathSerialization);
      if (NS_WARN_IF(!metadata->mCommonMetadata.keyPath().IsValid())) {
        return NS_ERROR_FILE_CORRUPTED;
      }
    }

    int64_t nextAutoIncrementId;
    rv = stmt->GetInt64(1, &nextAutoIncrementId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    metadata->mCommonMetadata.autoIncrement() = !!nextAutoIncrementId;
    metadata->mNextAutoIncrementId = nextAutoIncrementId;
    metadata->mComittedAutoIncrementId = nextAutoIncrementId;

    if (NS_WARN_IF(!objectStores.Put(objectStoreId, metadata, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    lastObjectStoreId = std::max(lastObjectStoreId, objectStoreId);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  usedIds.reset();
  usedNames.reset();

  // Load index information
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, object_store_id, name, key_path, unique_index, multientry "
    "FROM object_store_index"
  ), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t lastIndexId = 0;

  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    int64_t objectStoreId;
    rv = stmt->GetInt64(1, &objectStoreId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata;
    if (NS_WARN_IF(!objectStores.Get(objectStoreId,
                                     getter_AddRefs(objectStoreMetadata)))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    MOZ_ASSERT(objectStoreMetadata->mCommonMetadata.id() == objectStoreId);

    int64_t indexId;
    rv = stmt->GetInt64(0, &indexId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!usedIds) {
      usedIds.emplace();
    }

    if (NS_WARN_IF(indexId <= 0) ||
        NS_WARN_IF(usedIds.ref().Contains(indexId))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (NS_WARN_IF(!usedIds.ref().PutEntry(indexId, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsString name;
    rv = stmt->GetString(2, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoString hashName;
    hashName.AppendInt(indexId);
    hashName.Append(':');
    hashName.Append(name);

    if (!usedNames) {
      usedNames.emplace();
    }

    if (NS_WARN_IF(usedNames.ref().Contains(hashName))) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    if (NS_WARN_IF(!usedNames.ref().PutEntry(hashName, fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsRefPtr<FullIndexMetadata> indexMetadata = new FullIndexMetadata();
    indexMetadata->mCommonMetadata.id() = indexId;
    indexMetadata->mCommonMetadata.name() = name;

#ifdef DEBUG
    {
      int32_t columnType;
      rv = stmt->GetTypeOfIndex(3, &columnType);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      MOZ_ASSERT(columnType != mozIStorageStatement::VALUE_TYPE_NULL);
    }
#endif

    nsString keyPathSerialization;
    rv = stmt->GetString(3, keyPathSerialization);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    indexMetadata->mCommonMetadata.keyPath() =
      KeyPath::DeserializeFromString(keyPathSerialization);
    if (NS_WARN_IF(!indexMetadata->mCommonMetadata.keyPath().IsValid())) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    int32_t scratch;
    rv = stmt->GetInt32(4, &scratch);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    indexMetadata->mCommonMetadata.unique() = !!scratch;

    rv = stmt->GetInt32(5, &scratch);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    indexMetadata->mCommonMetadata.multiEntry() = !!scratch;

    if (NS_WARN_IF(!objectStoreMetadata->mIndexes.Put(indexId, indexMetadata,
                                                      fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    lastIndexId = std::max(lastIndexId, indexId);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(lastObjectStoreId == INT64_MAX) ||
      NS_WARN_IF(lastIndexId == INT64_MAX)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mMetadata->mNextObjectStoreId = lastObjectStoreId + 1;
  mMetadata->mNextIndexId = lastIndexId + 1;

  return NS_OK;
}

nsresult
OpenDatabaseOp::BeginVersionChange()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(mMetadata->mCommonMetadata.version() <= mRequestedVersion);
  MOZ_ASSERT(!mDatabase);
  MOZ_ASSERT(!mVersionChangeTransaction);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread()) ||
      !OperationMayProceed() ||
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

  MOZ_ASSERT(info->mLiveDatabases.Contains(mDatabase));
  MOZ_ASSERT(!info->mWaitingFactoryOp);
  MOZ_ASSERT(info->mMetadata == mMetadata);

  nsRefPtr<VersionChangeTransaction> transaction =
    new VersionChangeTransaction(this);

  if (NS_WARN_IF(!transaction->CopyDatabaseMetadata())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  MOZ_ASSERT(info->mMetadata != mMetadata);
  mMetadata = info->mMetadata;

  NullableVersion newVersion = mRequestedVersion;

  nsresult rv =
    SendVersionChangeMessages(info,
                              mDatabase,
                              mMetadata->mCommonMetadata.version(),
                              newVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mVersionChangeTransaction.swap(transaction);

  if (mMaybeBlockedDatabases.IsEmpty()) {
    // We don't need to wait on any databases, just jump to the transaction
    // pool.
    WaitForTransactions();
    return NS_OK;
  }

  info->mWaitingFactoryOp = this;

  mState = State_WaitingForOtherDatabasesToClose;
  return NS_OK;
}

void
OpenDatabaseOp::NoteDatabaseClosed(Database* aDatabase)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mMaybeBlockedDatabases.IsEmpty());

  bool actorDestroyed = IsActorDestroyed() || mDatabase->IsActorDestroyed();

  nsresult rv;
  if (actorDestroyed) {
    IDB_REPORT_INTERNAL_ERR();
    rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  } else {
    rv = NS_OK;
  }

  if (mMaybeBlockedDatabases.RemoveElement(aDatabase) &&
      mMaybeBlockedDatabases.IsEmpty()) {
    if (actorDestroyed) {
      DatabaseActorInfo* info;
      MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(mDatabaseId, &info));
      MOZ_ASSERT(info->mWaitingFactoryOp == this);
      info->mWaitingFactoryOp = nullptr;
    } else {
      WaitForTransactions();
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    mState = State_SendingResults;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(Run()));
  }
}

void
OpenDatabaseOp::SendBlockedNotification()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_WaitingForOtherDatabasesToClose);

  if (!IsActorDestroyed()) {
    unused << SendBlocked(mMetadata->mCommonMetadata.version());
  }
}

nsresult
OpenDatabaseOp::DispatchToWorkThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_WaitingForTransactionsToComplete);
  MOZ_ASSERT(mVersionChangeTransaction);
  MOZ_ASSERT(mVersionChangeTransaction->GetMode() ==
               IDBTransaction::VERSION_CHANGE);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mState = State_DatabaseWorkVersionChange;

  // Intentionally empty.
  nsTArray<nsString> objectStoreNames;

  const int64_t loggingSerialNumber =
    mVersionChangeTransaction->LoggingSerialNumber();
  const nsID& backgroundChildLoggingId =
    mVersionChangeTransaction->GetLoggingInfo()->Id();

  nsRefPtr<VersionChangeOp> versionChangeOp = new VersionChangeOp(this);

  gTransactionThreadPool->Start(mVersionChangeTransaction->TransactionId(),
                                mVersionChangeTransaction->DatabaseId(),
                                objectStoreNames,
                                mVersionChangeTransaction->GetMode(),
                                backgroundChildLoggingId,
                                loggingSerialNumber,
                                versionChangeOp);

  mVersionChangeTransaction->SetActive();

  mVersionChangeTransaction->NoteActiveRequest();

  if (NS_WARN_IF(!mDatabase->RegisterTransaction(mVersionChangeTransaction))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
OpenDatabaseOp::SendUpgradeNeeded()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_DatabaseWorkVersionChange);
  MOZ_ASSERT(mVersionChangeTransaction);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
  MOZ_ASSERT_IF(!IsActorDestroyed(), mDatabase);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread()) ||
      !OperationMayProceed() ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsRefPtr<VersionChangeTransaction> transaction;
  mVersionChangeTransaction.swap(transaction);

  nsresult rv = EnsureDatabaseActorIsAlive();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Transfer ownership to IPDL.
  transaction->SetActorAlive();

  if (!mDatabase->SendPBackgroundIDBVersionChangeTransactionConstructor(
                                           transaction,
                                           mMetadata->mCommonMetadata.version(),
                                           mRequestedVersion,
                                           mMetadata->mNextObjectStoreId,
                                           mMetadata->mNextIndexId)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void
OpenDatabaseOp::SendResults()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_SendingResults);
  MOZ_ASSERT_IF(NS_SUCCEEDED(mResultCode), mMaybeBlockedDatabases.IsEmpty());
  MOZ_ASSERT_IF(NS_SUCCEEDED(mResultCode), !mVersionChangeTransaction);

  mMaybeBlockedDatabases.Clear();

  // Only needed if we're being called from within NoteDatabaseDone() since this
  // OpenDatabaseOp is only held alive by the gLiveDatabaseHashtable.
  nsRefPtr<OpenDatabaseOp> kungFuDeathGrip;

  DatabaseActorInfo* info;
  if (gLiveDatabaseHashtable &&
      gLiveDatabaseHashtable->Get(mDatabaseId, &info) &&
      info->mWaitingFactoryOp) {
    MOZ_ASSERT(info->mWaitingFactoryOp == this);
    kungFuDeathGrip =
      static_cast<OpenDatabaseOp*>(info->mWaitingFactoryOp.get());
    info->mWaitingFactoryOp = nullptr;
  }

  if (mVersionChangeTransaction) {
    MOZ_ASSERT(NS_FAILED(mResultCode));

    mVersionChangeTransaction->Abort(mResultCode, /* aForce */ true);
    mVersionChangeTransaction = nullptr;
  }

  if (!IsActorDestroyed()) {
    FactoryRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      // If we just successfully completed a versionchange operation then we
      // need to update the version in our metadata.
      mMetadata->mCommonMetadata.version() = mRequestedVersion;

      nsresult rv = EnsureDatabaseActorIsAlive();
      if (NS_SUCCEEDED(rv)) {
        // We successfully opened a database so use its actor as the success
        // result for this request.
        OpenDatabaseRequestResponse openResponse;
        openResponse.databaseParent() = mDatabase;
        response = openResponse;
      } else {
        response = ClampResultCode(rv);
#ifdef DEBUG
        mResultCode = response.get_nsresult();
#endif
      }
    } else {
#ifdef DEBUG
      // If something failed then our metadata pointer is now bad. No one should
      // ever touch it again though so just null it out in DEBUG builds to make
      // sure we find such cases.
      mMetadata = nullptr;
#endif
      response = ClampResultCode(mResultCode);
    }

    unused <<
      PBackgroundIDBFactoryRequestParent::Send__delete__(this, response);
  }

  if (NS_FAILED(mResultCode) && mOfflineStorage) {
    mOfflineStorage->CloseOnOwningThread();
    DatabaseOfflineStorage::UnregisterOnOwningThread(mOfflineStorage.forget());
  }

  // Make sure to release the database on this thread.
  nsRefPtr<Database> database;
  mDatabase.swap(database);

  FinishSendResults();
}

void
OpenDatabaseOp::EnsureDatabaseActor()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_BeginVersionChange ||
             mState == State_DatabaseWorkVersionChange ||
             mState == State_SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
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
    AssertMetadataConsistency(info->mMetadata);
    mMetadata = info->mMetadata;
  }

  auto factory = static_cast<Factory*>(Manager());

  mDatabase = new Database(factory,
                           mCommonParams.principalInfo(),
                           mGroup,
                           mOrigin,
                           mMetadata,
                           mFileManager,
                           mOfflineStorage.forget(),
                           mChromeWriteAccessAllowed);

  if (info) {
    info->mLiveDatabases.AppendElement(mDatabase);
  } else {
    info = new DatabaseActorInfo(mMetadata, mDatabase);
    gLiveDatabaseHashtable->Put(mDatabaseId, info);
  }
}

nsresult
OpenDatabaseOp::EnsureDatabaseActorIsAlive()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_DatabaseWorkVersionChange ||
             mState == State_SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
  MOZ_ASSERT(!IsActorDestroyed());

  EnsureDatabaseActor();

  if (mDatabase->IsActorAlive()) {
    return NS_OK;
  }

  auto factory = static_cast<Factory*>(Manager());

  DatabaseSpec spec;
  MetadataToSpec(spec);

  // Transfer ownership to IPDL.
  mDatabase->SetActorAlive();

  if (!factory->SendPBackgroundIDBDatabaseConstructor(mDatabase, spec, this)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

void
OpenDatabaseOp::MetadataToSpec(DatabaseSpec& aSpec)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mMetadata);

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
    DatabaseSpec& mSpec;
    ObjectStoreSpec* mCurrentObjectStoreSpec;

  public:
    static void
    CopyToSpec(const FullDatabaseMetadata* aMetadata, DatabaseSpec& aSpec)
    {
      AssertIsOnBackgroundThread();
      MOZ_ASSERT(aMetadata);

      aSpec.metadata() = aMetadata->mCommonMetadata;

      Helper helper(aSpec);
      aMetadata->mObjectStores.EnumerateRead(Enumerate, &helper);
    }

  private:
    explicit Helper(DatabaseSpec& aSpec)
      : mSpec(aSpec)
      , mCurrentObjectStoreSpec(nullptr)
    { }

    static PLDHashOperator
    Enumerate(const uint64_t& aKey,
              FullObjectStoreMetadata* aValue,
              void* aClosure)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);
      MOZ_ASSERT(aClosure);

      auto* helper = static_cast<Helper*>(aClosure);

      MOZ_ASSERT(!helper->mCurrentObjectStoreSpec);

      // XXX This should really be fallible...
      ObjectStoreSpec* objectStoreSpec =
        helper->mSpec.objectStores().AppendElement();
      objectStoreSpec->metadata() = aValue->mCommonMetadata;

      AutoRestore<ObjectStoreSpec*> ar(helper->mCurrentObjectStoreSpec);
      helper->mCurrentObjectStoreSpec = objectStoreSpec;

      aValue->mIndexes.EnumerateRead(Enumerate, helper);

      return PL_DHASH_NEXT;
    }

    static PLDHashOperator
    Enumerate(const uint64_t& aKey, FullIndexMetadata* aValue, void* aClosure)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);
      MOZ_ASSERT(aClosure);

      auto* helper = static_cast<Helper*>(aClosure);

      MOZ_ASSERT(helper->mCurrentObjectStoreSpec);

      // XXX This should really be fallible...
      IndexMetadata* metadata =
        helper->mCurrentObjectStoreSpec->indexes().AppendElement();
      *metadata = aValue->mCommonMetadata;

      return PL_DHASH_NEXT;
    }
  };

  Helper::CopyToSpec(mMetadata, aSpec);
}


#ifdef DEBUG

void
OpenDatabaseOp::AssertMetadataConsistency(const FullDatabaseMetadata* aMetadata)
{
  AssertIsOnBackgroundThread();

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
    const ObjectStoreTable& mOtherObjectStores;
    IndexTable* mCurrentOtherIndexTable;

  public:
    static void
    AssertConsistent(const ObjectStoreTable& aThisObjectStores,
                     const ObjectStoreTable& aOtherObjectStores)
    {
      Helper helper(aOtherObjectStores);
      aThisObjectStores.EnumerateRead(Enumerate, &helper);
    }

  private:
    explicit Helper(const ObjectStoreTable& aOtherObjectStores)
      : mOtherObjectStores(aOtherObjectStores)
      , mCurrentOtherIndexTable(nullptr)
    { }

    static PLDHashOperator
    Enumerate(const uint64_t& /* aKey */,
              FullObjectStoreMetadata* aThisObjectStore,
              void* aClosure)
    {
      MOZ_ASSERT(aThisObjectStore);
      MOZ_ASSERT(!aThisObjectStore->mDeleted);
      MOZ_ASSERT(aClosure);

      auto* helper = static_cast<Helper*>(aClosure);

      MOZ_ASSERT(!helper->mCurrentOtherIndexTable);

      auto* otherObjectStore =
        MetadataNameOrIdMatcher<FullObjectStoreMetadata>::Match(
          helper->mOtherObjectStores, aThisObjectStore->mCommonMetadata.id());
      MOZ_ASSERT(otherObjectStore);

      MOZ_ASSERT(aThisObjectStore != otherObjectStore);

      MOZ_ASSERT(aThisObjectStore->mCommonMetadata.id() ==
                   otherObjectStore->mCommonMetadata.id());
      MOZ_ASSERT(aThisObjectStore->mCommonMetadata.name() ==
                   otherObjectStore->mCommonMetadata.name());
      MOZ_ASSERT(aThisObjectStore->mCommonMetadata.autoIncrement() ==
                   otherObjectStore->mCommonMetadata.autoIncrement());
      MOZ_ASSERT(aThisObjectStore->mCommonMetadata.keyPath() ==
                   otherObjectStore->mCommonMetadata.keyPath());
      // mNextAutoIncrementId and mComittedAutoIncrementId may be modified
      // concurrently with this OpenOp, so it is not possible to assert equality
      // here.
      MOZ_ASSERT(aThisObjectStore->mNextAutoIncrementId <=
                   otherObjectStore->mNextAutoIncrementId);
      MOZ_ASSERT(aThisObjectStore->mComittedAutoIncrementId <=
                   otherObjectStore->mComittedAutoIncrementId);
      MOZ_ASSERT(!otherObjectStore->mDeleted);

      MOZ_ASSERT(aThisObjectStore->mIndexes.Count() ==
                   otherObjectStore->mIndexes.Count());

      AutoRestore<IndexTable*> ar(helper->mCurrentOtherIndexTable);
      helper->mCurrentOtherIndexTable = &otherObjectStore->mIndexes;

      aThisObjectStore->mIndexes.EnumerateRead(Enumerate, helper);

      return PL_DHASH_NEXT;
    }

    static PLDHashOperator
    Enumerate(const uint64_t& /* aKey */,
              FullIndexMetadata* aThisIndex,
              void* aClosure)
    {
      MOZ_ASSERT(aThisIndex);
      MOZ_ASSERT(!aThisIndex->mDeleted);
      MOZ_ASSERT(aClosure);

      auto* helper = static_cast<Helper*>(aClosure);

      MOZ_ASSERT(helper->mCurrentOtherIndexTable);

      auto* otherIndex =
        MetadataNameOrIdMatcher<FullIndexMetadata>::Match(
          *helper->mCurrentOtherIndexTable, aThisIndex->mCommonMetadata.id());
      MOZ_ASSERT(otherIndex);

      MOZ_ASSERT(aThisIndex != otherIndex);

      MOZ_ASSERT(aThisIndex->mCommonMetadata.id() ==
                   otherIndex->mCommonMetadata.id());
      MOZ_ASSERT(aThisIndex->mCommonMetadata.name() ==
                   otherIndex->mCommonMetadata.name());
      MOZ_ASSERT(aThisIndex->mCommonMetadata.keyPath() ==
                   otherIndex->mCommonMetadata.keyPath());
      MOZ_ASSERT(aThisIndex->mCommonMetadata.unique() ==
                   otherIndex->mCommonMetadata.unique());
      MOZ_ASSERT(aThisIndex->mCommonMetadata.multiEntry() ==
                   otherIndex->mCommonMetadata.multiEntry());
      MOZ_ASSERT(!otherIndex->mDeleted);

      return PL_DHASH_NEXT;
    }
  };

  const FullDatabaseMetadata* thisDB = mMetadata;
  const FullDatabaseMetadata* otherDB = aMetadata;

  MOZ_ASSERT(thisDB);
  MOZ_ASSERT(otherDB);
  MOZ_ASSERT(thisDB != otherDB);

  MOZ_ASSERT(thisDB->mCommonMetadata.name() == otherDB->mCommonMetadata.name());
  MOZ_ASSERT(thisDB->mCommonMetadata.version() ==
               otherDB->mCommonMetadata.version());
  MOZ_ASSERT(thisDB->mCommonMetadata.persistenceType() ==
               otherDB->mCommonMetadata.persistenceType());
  MOZ_ASSERT(thisDB->mDatabaseId == otherDB->mDatabaseId);
  MOZ_ASSERT(thisDB->mFilePath == otherDB->mFilePath);

  // |thisDB| reflects the latest objectStore and index ids that have committed
  // to disk. The in-memory metadata |otherDB| keeps track of objectStores and
  // indexes that were created and then removed as well, so the next ids for
  // |otherDB| may be higher than for |thisDB|.
  MOZ_ASSERT(thisDB->mNextObjectStoreId <= otherDB->mNextObjectStoreId);
  MOZ_ASSERT(thisDB->mNextIndexId <= otherDB->mNextIndexId);

  MOZ_ASSERT(thisDB->mObjectStores.Count() == otherDB->mObjectStores.Count());

  Helper::AssertConsistent(thisDB->mObjectStores, otherDB->mObjectStores);
}

#endif // DEBUG

nsresult
OpenDatabaseOp::
VersionChangeOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(mOpenDatabaseOp->mState == State_DatabaseWorkVersionChange);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  PROFILER_LABEL("IndexedDB",
                 "OpenDatabaseOp::VersionChangeOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  mozIStorageConnection* connection = aTransaction->Connection();
  MOZ_ASSERT(connection);

  TransactionBase::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(
    NS_LITERAL_CSTRING("UPDATE database "
                       "SET version = :version"),
    getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("version"),
                             int64_t(mRequestedVersion));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
OpenDatabaseOp::
VersionChangeOp::SendSuccessResult()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mState == State_DatabaseWorkVersionChange);

  nsresult rv = mOpenDatabaseOp->SendUpgradeNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool
OpenDatabaseOp::
VersionChangeOp::SendFailureResult(nsresult aResultCode)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpenDatabaseOp);
  MOZ_ASSERT(mOpenDatabaseOp->mState == State_DatabaseWorkVersionChange);

  mOpenDatabaseOp->SetFailureCode(aResultCode);
  mOpenDatabaseOp->mState = State_SendingResults;

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mOpenDatabaseOp->Run()));

  return false;
}

void
OpenDatabaseOp::
VersionChangeOp::Cleanup()
{
  AssertIsOnOwningThread();

  mOpenDatabaseOp = nullptr;

#ifdef DEBUG
  // A bit hacky but the VersionChangeOp is not generated in response to a
  // child request like most other database operations. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

void
DeleteDatabaseOp::LoadPreviousVersion(nsIFile* aDatabaseFile)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDatabaseFile);
  MOZ_ASSERT(mState == State_DatabaseWorkOpen);
  MOZ_ASSERT(!mPreviousVersion);

  PROFILER_LABEL("IndexedDB",
                 "DeleteDatabaseOp::LoadPreviousVersion",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv;

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabase(aDatabaseFile, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

#ifdef DEBUG
  {
    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT name "
      "FROM database"
    ), getter_AddRefs(stmt));
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "CreateStatement failed!");

    if (NS_SUCCEEDED(rv)) {
      bool hasResult;
      rv = stmt->ExecuteStep(&hasResult);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "ExecuteStep failed!");

      if (NS_SUCCEEDED(rv)) {
        nsString databaseName;
        rv = stmt->GetString(0, databaseName);
        NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "GetString failed!");

        if (NS_SUCCEEDED(rv)) {
          NS_WARN_IF_FALSE(mCommonParams.metadata().name() == databaseName,
                           "Database names don't match!");
        }
      }
    }
  }
#endif

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT version "
    "FROM database"
  ), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_WARN_IF(!hasResult)) {
    return;
  }

  int64_t version;
  rv = stmt->GetInt64(0, &version);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mPreviousVersion = uint64_t(version);
}

nsresult
DeleteDatabaseOp::QuotaManagerOpen()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_OpenPending);

  // Swap this to the stack now to ensure that we release it on this thread.
  nsRefPtr<ContentParent> contentParent;
  mContentParent.swap(contentParent);

  nsresult rv = SendToIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
DeleteDatabaseOp::DoDatabaseWork()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DatabaseWorkOpen);

  PROFILER_LABEL("IndexedDB",
                 "DeleteDatabaseOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const nsString& databaseName = mCommonParams.metadata().name();
  PersistenceType persistenceType = mCommonParams.metadata().persistenceType();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = quotaManager->GetDirectoryForOrigin(persistenceType,
                                                    mOrigin,
                                                    getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directory->GetPath(mDatabaseDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString filename;
  GetDatabaseFilename(databaseName, filename);

  mDatabaseFilenameBase = filename;

  nsCOMPtr<nsIFile> dbFile;
  rv = directory->Clone(getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = dbFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    // Parts of this function may fail but that shouldn't prevent us from
    // deleting the file eventually.
    LoadPreviousVersion(dbFile);

    mState = State_BeginVersionChange;
  } else {
    mState = State_SendingResults;
  }

  rv = mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
DeleteDatabaseOp::BeginVersionChange()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_BeginVersionChange);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread()) ||
      !OperationMayProceed() ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  DatabaseActorInfo* info;
  if (gLiveDatabaseHashtable->Get(mDatabaseId, &info)) {
    MOZ_ASSERT(!info->mWaitingFactoryOp);

    NullableVersion newVersion = null_t();

    nsresult rv =
      SendVersionChangeMessages(info, nullptr, mPreviousVersion, newVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mMaybeBlockedDatabases.IsEmpty()) {
      info->mWaitingFactoryOp = this;

      mState = State_WaitingForOtherDatabasesToClose;
      return NS_OK;
    }
  }

  // No other databases need to be notified, just make sure that all
  // transactions are complete.
  WaitForTransactions();
  return NS_OK;
}

nsresult
DeleteDatabaseOp::DispatchToWorkThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_WaitingForTransactionsToComplete);
  MOZ_ASSERT(mMaybeBlockedDatabases.IsEmpty());

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread()) ||
      !OperationMayProceed() ||
      IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mState = State_DatabaseWorkVersionChange;

  nsRefPtr<VersionChangeOp> versionChangeOp = new VersionChangeOp(this);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(versionChangeOp)));

  return NS_OK;
}

void
DeleteDatabaseOp::NoteDatabaseClosed(Database* aDatabase)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_WaitingForOtherDatabasesToClose);
  MOZ_ASSERT(!mMaybeBlockedDatabases.IsEmpty());

  bool actorDestroyed = IsActorDestroyed();

  nsresult rv;
  if (actorDestroyed) {
    IDB_REPORT_INTERNAL_ERR();
    rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  } else {
    rv = NS_OK;
  }

  if (mMaybeBlockedDatabases.RemoveElement(aDatabase) &&
      mMaybeBlockedDatabases.IsEmpty()) {
    if (actorDestroyed) {
      DatabaseActorInfo* info;
      MOZ_ALWAYS_TRUE(gLiveDatabaseHashtable->Get(mDatabaseId, &info));
      MOZ_ASSERT(info->mWaitingFactoryOp == this);
      info->mWaitingFactoryOp = nullptr;
    } else {
      WaitForTransactions();
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    mState = State_SendingResults;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(Run()));
  }
}

void
DeleteDatabaseOp::SendBlockedNotification()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_WaitingForOtherDatabasesToClose);

  if (!IsActorDestroyed()) {
    unused << SendBlocked(0);
  }
}

void
DeleteDatabaseOp::SendResults()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_SendingResults);

  if (!IsActorDestroyed()) {
    FactoryRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      response = DeleteDatabaseRequestResponse(mPreviousVersion);
    } else {
      response = ClampResultCode(mResultCode);
    }

    unused <<
      PBackgroundIDBFactoryRequestParent::Send__delete__(this, response);
  }

  FinishSendResults();
}

nsresult
DeleteDatabaseOp::
VersionChangeOp::RunOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDeleteDatabaseOp->mState == State_DatabaseWorkVersionChange);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnMainThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

nsresult
DeleteDatabaseOp::
VersionChangeOp::RunOnIOThread()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mDeleteDatabaseOp->mState == State_DatabaseWorkVersionChange);

  PROFILER_LABEL("IndexedDB",
                 "DeleteDatabaseOp::VersionChangeOp::RunOnIOThread",
                 js::ProfileEntry::Category::STORAGE);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonMainThread()) ||
      !OperationMayProceed()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFile> directory =
    GetFileForPath(mDeleteDatabaseOp->mDatabaseDirectoryPath);
  if (NS_WARN_IF(!directory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = directory->Clone(getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->Append(mDeleteDatabaseOp->mDatabaseFilenameBase +
                      NS_LITERAL_STRING(".sqlite"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = dbFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const nsString& databaseName =
    mDeleteDatabaseOp->mCommonParams.metadata().name();
  PersistenceType persistenceType =
    mDeleteDatabaseOp->mCommonParams.metadata().persistenceType();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  if (exists) {
    int64_t fileSize;

    if (mDeleteDatabaseOp->mEnforcingQuota) {
      rv = dbFile->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = dbFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (mDeleteDatabaseOp->mEnforcingQuota) {
      quotaManager->DecreaseUsageForOrigin(persistenceType,
                                           mDeleteDatabaseOp->mGroup,
                                           mDeleteDatabaseOp->mOrigin,
                                           fileSize);
    }
  }

  nsCOMPtr<nsIFile> dbJournalFile;
  rv = directory->Clone(getter_AddRefs(dbJournalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbJournalFile->Append(mDeleteDatabaseOp->mDatabaseFilenameBase +
                             NS_LITERAL_STRING(".sqlite-journal"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbJournalFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = dbJournalFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIFile> fmDirectory;
  rv = directory->Clone(getter_AddRefs(fmDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const NS_ConvertASCIItoUTF16 filesSuffix(kFileManagerDirectoryNameSuffix);

  rv = fmDirectory->Append(mDeleteDatabaseOp->mDatabaseFilenameBase +
                           filesSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = fmDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isDirectory;
    rv = fmDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!isDirectory)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    uint64_t usage = 0;

    if (mDeleteDatabaseOp->mEnforcingQuota) {
      rv = FileManager::GetUsage(fmDirectory, &usage);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = fmDirectory->Remove(true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (mDeleteDatabaseOp->mEnforcingQuota) {
      quotaManager->DecreaseUsageForOrigin(persistenceType,
                                           mDeleteDatabaseOp->mGroup,
                                           mDeleteDatabaseOp->mOrigin,
                                           usage);
    }
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  MOZ_ASSERT(mgr);

  mgr->InvalidateFileManager(persistenceType,
                             mDeleteDatabaseOp->mOrigin,
                             databaseName);

  rv = mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
DeleteDatabaseOp::
VersionChangeOp::RunOnOwningThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDeleteDatabaseOp->mState == State_DatabaseWorkVersionChange);

  nsRefPtr<DeleteDatabaseOp> deleteOp;
  mDeleteDatabaseOp.swap(deleteOp);

  if (deleteOp->IsActorDestroyed()) {
    IDB_REPORT_INTERNAL_ERR();
    deleteOp->SetFailureCode(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  } else {
    DatabaseActorInfo* info;
    if (gLiveDatabaseHashtable->Get(deleteOp->mDatabaseId, &info) &&
        info->mWaitingFactoryOp) {
      MOZ_ASSERT(info->mWaitingFactoryOp == deleteOp);
      info->mWaitingFactoryOp = nullptr;
    }

    if (NS_FAILED(mResultCode)) {
      if (NS_SUCCEEDED(deleteOp->ResultCode())) {
        deleteOp->SetFailureCode(mResultCode);
      }
    } else {
      // Inform all the other databases that they are now invalidated. That
      // should remove the previous metadata from our table.
      if (info) {
        MOZ_ASSERT(!info->mLiveDatabases.IsEmpty());

        FallibleTArray<Database*> liveDatabases;
        if (NS_WARN_IF(!liveDatabases.AppendElements(info->mLiveDatabases))) {
          deleteOp->SetFailureCode(NS_ERROR_OUT_OF_MEMORY);
        } else {
#ifdef DEBUG
          // The code below should result in the deletion of |info|. Set to null
          // here to make sure we find invalid uses later.
          info = nullptr;
#endif
          for (uint32_t count = liveDatabases.Length(), index = 0;
               index < count;
               index++) {
            nsRefPtr<Database> database = liveDatabases[index];
            database->Invalidate();
          }

          MOZ_ASSERT(!gLiveDatabaseHashtable->Get(deleteOp->mDatabaseId));
        }
      }
    }
  }

  deleteOp->mState = State_SendingResults;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(deleteOp->Run()));

#ifdef DEBUG
  // A bit hacky but the DeleteDatabaseOp::VersionChangeOp is not really a
  // normal database operation that is tied to an actor. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif
}

nsresult
DeleteDatabaseOp::
VersionChangeOp::Run()
{
  nsresult rv;

  if (NS_IsMainThread()) {
    rv = RunOnMainThread();
  } else if (!IsOnBackgroundThread()) {
    rv = RunOnIOThread();
  } else {
    RunOnOwningThread();
    rv = NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mOwningThread->Dispatch(this,
                                                         NS_DISPATCH_NORMAL)));
  }

  return NS_OK;
}

TransactionDatabaseOperationBase::TransactionDatabaseOperationBase(
                                                  TransactionBase* aTransaction)
  : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                          aTransaction->GetLoggingInfo()->NextRequestSN())
  , mTransaction(aTransaction)
  , mTransactionLoggingSerialNumber(aTransaction->LoggingSerialNumber())
  , mTransactionIsAborted(aTransaction->IsAborted())
{
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(LoggingSerialNumber());
}

TransactionDatabaseOperationBase::TransactionDatabaseOperationBase(
                                                  TransactionBase* aTransaction,
                                                  uint64_t aLoggingSerialNumber)
  : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                          aLoggingSerialNumber)
  , mTransaction(aTransaction)
  , mTransactionLoggingSerialNumber(aTransaction->LoggingSerialNumber())
  , mTransactionIsAborted(aTransaction->IsAborted())
{
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(LoggingSerialNumber());
}

TransactionDatabaseOperationBase::~TransactionDatabaseOperationBase()
{
  MOZ_ASSERT(!mTransaction,
             "TransactionDatabaseOperationBase::Cleanup() was not called by a "
             "subclass!");
}

#ifdef DEBUG

void
TransactionDatabaseOperationBase::AssertIsOnTransactionThread() const
{
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnTransactionThread();
}

#endif // DEBUG

void
TransactionDatabaseOperationBase::DispatchToTransactionThreadPool()
{
  AssertIsOnOwningThread();

  gTransactionThreadPool->Dispatch(mTransaction->TransactionId(),
                                   mTransaction->DatabaseId(),
                                   this,
                                   /* aFinish */ false,
                                   /* aFinishCallback */ nullptr);

  mTransaction->NoteActiveRequest();
}

void
TransactionDatabaseOperationBase::RunOnTransactionThread()
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));

  PROFILER_LABEL("IndexedDB",
                 "TransactionDatabaseOperationBase::RunOnTransactionThread",
                 js::ProfileEntry::Category::STORAGE);

  // There are several cases where we don't actually have to to any work here.

  if (mTransactionIsAborted) {
    // This transaction is already set to be aborted.
    mResultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  } else if (mTransaction->IsInvalidatedOnAnyThread()) {
    // This transaction is being invalidated.
    mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  } else if (!OperationMayProceed()) {
    // The operation was canceled in some way, likely because the child process
    // has crashed.
    IDB_REPORT_INTERNAL_ERR();
    mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  } else {
    // Here we're actually going to perform the database operation.
    nsresult rv = mTransaction->EnsureConnection();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mResultCode = rv;
    } else {
      mTransaction->AssertIsOnTransactionThread();

      AutoSetProgressHandler autoProgress;
      rv = autoProgress.Register(this, mTransaction->Connection());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mResultCode = rv;
      } else {
        IDB_LOG_MARK("IndexedDB %s: Parent Transaction[%lld] Request[%llu]: "
                       "Beginning database work",
                     "IndexedDB %s: P T[%lld] R[%llu]: DB Start",
                     IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
                     mTransactionLoggingSerialNumber,
                     mLoggingSerialNumber);

        rv = DoDatabaseWork(mTransaction);

        IDB_LOG_MARK("IndexedDB %s: Parent Transaction[%lld] Request[%llu]: "
                       "Finished database work",
                     "IndexedDB %s: P T[%lld] R[%llu]: DB End",
                     IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
                     mTransactionLoggingSerialNumber,
                     mLoggingSerialNumber);

        if (NS_FAILED(rv)) {
          mResultCode = rv;
        }
      }
    }
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mOwningThread->Dispatch(this,
                                                       NS_DISPATCH_NORMAL)));
}

void
TransactionDatabaseOperationBase::RunOnOwningThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  if (NS_WARN_IF(IsActorDestroyed())) {
    // Don't send any notifications if the actor was destroyed already.
    if (NS_SUCCEEDED(mResultCode)) {
      IDB_REPORT_INTERNAL_ERR();
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  } else {
    if (mTransaction->IsInvalidated()) {
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    } else if (mTransaction->IsAborted()) {
      // Aborted transactions always see their requests fail with ABORT_ERR,
      // even if the request succeeded or failed with another error.
      mResultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
    } else if (NS_SUCCEEDED(mResultCode)) {
      // This may release the IPDL reference.
      mResultCode = SendSuccessResult();
    }

    if (NS_FAILED(mResultCode)) {
      // This should definitely release the IPDL reference.
      if (!SendFailureResult(mResultCode)) {
        // Abort the transaction.
        mTransaction->Abort(mResultCode, /* aForce */ false);
      }
    }
  }

  mTransaction->NoteFinishedRequest();

  Cleanup();
}

bool
TransactionDatabaseOperationBase::Init(TransactionBase* aTransaction)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);

  return true;
}

void
TransactionDatabaseOperationBase::Cleanup()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTransaction);

  mTransaction = nullptr;
}

NS_IMETHODIMP
TransactionDatabaseOperationBase::Run()
{
  MOZ_ASSERT(mTransaction);

  if (IsOnBackgroundThread()) {
    RunOnOwningThread();
  } else {
    RunOnTransactionThread();
  }

  return NS_OK;
}

TransactionBase::

CommitOp::CommitOp(TransactionBase* aTransaction, nsresult aResultCode)
  : DatabaseOperationBase(aTransaction->GetLoggingInfo()->Id(),
                          aTransaction->GetLoggingInfo()->NextRequestSN())
  , mTransaction(aTransaction)
  , mResultCode(aResultCode)
{
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(LoggingSerialNumber());
}

nsresult
TransactionBase::
CommitOp::WriteAutoIncrementCounts()
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(mTransaction);

  const nsTArray<nsRefPtr<FullObjectStoreMetadata>>& metadataArray =
    mTransaction->mModifiedAutoIncrementObjectStoreMetadataArray;

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv;

  if (!metadataArray.IsEmpty()) {
    NS_NAMED_LITERAL_CSTRING(osid, "osid");
    NS_NAMED_LITERAL_CSTRING(ai, "ai");

    for (uint32_t count = metadataArray.Length(), index = 0;
         index < count;
         index++) {
      const nsRefPtr<FullObjectStoreMetadata>& metadata = metadataArray[index];
      MOZ_ASSERT(!metadata->mDeleted);
      MOZ_ASSERT(metadata->mNextAutoIncrementId > 1);

      if (stmt) {
        MOZ_ALWAYS_TRUE(NS_SUCCEEDED(stmt->Reset()));
      } else {
        rv = mTransaction->mConnection->CreateStatement(
          NS_LITERAL_CSTRING("UPDATE object_store "
                             "SET auto_increment = :") + ai +
          NS_LITERAL_CSTRING(" WHERE id = :") + osid +
          NS_LITERAL_CSTRING(";"),
          getter_AddRefs(stmt));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      rv = stmt->BindInt64ByName(osid, metadata->mCommonMetadata.id());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindInt64ByName(ai, metadata->mNextAutoIncrementId);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

void
TransactionBase::
CommitOp::CommitOrRollbackAutoIncrementCounts()
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(mTransaction);

  nsTArray<nsRefPtr<FullObjectStoreMetadata>>& metadataArray =
    mTransaction->mModifiedAutoIncrementObjectStoreMetadataArray;

  if (!metadataArray.IsEmpty()) {
    bool committed = NS_SUCCEEDED(mResultCode);

    for (uint32_t count = metadataArray.Length(), index = 0;
         index < count;
         index++) {
      nsRefPtr<FullObjectStoreMetadata>& metadata = metadataArray[index];

      if (committed) {
        metadata->mComittedAutoIncrementId = metadata->mNextAutoIncrementId;
      } else {
        metadata->mNextAutoIncrementId = metadata->mComittedAutoIncrementId;
      }
    }
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(TransactionBase::CommitOp, nsRunnable)

NS_IMETHODIMP
TransactionBase::
CommitOp::Run()
{
  MOZ_ASSERT(mTransaction);

  PROFILER_LABEL("IndexedDB",
                 "CommitOp::Run",
                 js::ProfileEntry::Category::STORAGE);

  nsCOMPtr<mozIStorageConnection>& connection = mTransaction->mConnection;

  if (!connection) {
    return NS_OK;
  }

  AssertIsOnTransactionThread();

  IDB_LOG_MARK("IndexedDB %s: Parent Transaction[%lld] Request[%llu]: "
                 "Beginning database work",
               "IndexedDB %s: P T[%lld] R[%llu]: DB Start",
               IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
               mTransaction->LoggingSerialNumber(),
               mLoggingSerialNumber);

  if (NS_SUCCEEDED(mResultCode) && mTransaction->mUpdateFileRefcountFunction) {
    mResultCode = mTransaction->
      mUpdateFileRefcountFunction->WillCommit(connection);
  }

  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = WriteAutoIncrementCounts();
  }

  if (NS_SUCCEEDED(mResultCode)) {
    NS_NAMED_LITERAL_CSTRING(commit, "COMMIT TRANSACTION");
    mResultCode = connection->ExecuteSimpleSQL(commit);

    if (NS_SUCCEEDED(mResultCode)) {
      if (mTransaction->mUpdateFileRefcountFunction) {
        mTransaction->mUpdateFileRefcountFunction->DidCommit();
      }
    }
  }

  if (NS_FAILED(mResultCode)) {
    if (mTransaction->mUpdateFileRefcountFunction) {
      mTransaction->mUpdateFileRefcountFunction->DidAbort();
    }

    // This may fail if SQLite already rolled back the transaction so ignore any
    // errors.
    unused <<
      connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("ROLLBACK TRANSACTION"));
  }

  CommitOrRollbackAutoIncrementCounts();

  if (mTransaction->mUpdateFileRefcountFunction) {
    NS_NAMED_LITERAL_CSTRING(functionName, "update_refcount");
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(connection->RemoveFunction(functionName)));
  }

  mTransaction->ReleaseTransactionThreadObjects();

  IDB_LOG_MARK("IndexedDB %s: Parent Transaction[%lld] Request[%llu]: "
                 "Finished database work",
               "IndexedDB %s: P T[%lld] R[%llu]: DB End",
               IDB_LOG_ID_STRING(mBackgroundChildLoggingId),
               mTransaction->LoggingSerialNumber(),
               mLoggingSerialNumber);

  return NS_OK;
}

void
TransactionBase::
CommitOp::TransactionFinishedBeforeUnblock()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransaction);

  PROFILER_LABEL("IndexedDB",
                 "CommitOp::TransactionFinishedBeforeUnblock",
                 js::ProfileEntry::Category::STORAGE);

  if (!IsActorDestroyed()) {
    mTransaction->UpdateMetadata(mResultCode);
  }
}

void
TransactionBase::
CommitOp::TransactionFinishedAfterUnblock()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransaction);

  if (!mTransaction->IsActorDestroyed()) {
    IDB_LOG_MARK("IndexedDB %s: Parent Transaction[%lld]: "
                   "Finished with result 0x%x",
                 "IndexedDB %s: P T[%lld]: Transaction finished (0x%x)",
                 IDB_LOG_ID_STRING(mTransaction->GetLoggingInfo()->Id()),
                 mTransaction->LoggingSerialNumber(),
                 mResultCode);
  }

  mTransaction->ReleaseBackgroundThreadObjects();

  mTransaction->SendCompleteNotification(ClampResultCode(mResultCode));

  mTransaction->GetDatabase()->UnregisterTransaction(mTransaction);

  mTransaction = nullptr;

#ifdef DEBUG
  // A bit hacky but the CommitOp is not really a normal database operation
  // that is tied to an actor. Do this to make our assertions happy.
  NoteActorDestroyed();
#endif
}

NS_IMPL_ISUPPORTS(TransactionBase::UpdateRefcountFunction, mozIStorageFunction)

NS_IMETHODIMP
TransactionBase::
UpdateRefcountFunction::OnFunctionCall(mozIStorageValueArray* aValues,
                                       nsIVariant** _retval)
{
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);

  uint32_t numEntries;
  nsresult rv = aValues->GetNumEntries(&numEntries);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(numEntries == 2);

#ifdef DEBUG
  {
    int32_t type1 = mozIStorageValueArray::VALUE_TYPE_NULL;
    MOZ_ASSERT(NS_SUCCEEDED(aValues->GetTypeOfIndex(0, &type1)));

    int32_t type2 = mozIStorageValueArray::VALUE_TYPE_NULL;
    MOZ_ASSERT(NS_SUCCEEDED(aValues->GetTypeOfIndex(1, &type2)));

    MOZ_ASSERT(!(type1 == mozIStorageValueArray::VALUE_TYPE_NULL &&
                 type2 == mozIStorageValueArray::VALUE_TYPE_NULL));
  }
#endif

  rv = ProcessValue(aValues, 0, eDecrement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = ProcessValue(aValues, 1, eIncrement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
TransactionBase::
UpdateRefcountFunction::WillCommit(mozIStorageConnection* aConnection)
{
  MOZ_ASSERT(aConnection);

  DatabaseUpdateFunction function(aConnection, this);

  mFileInfoEntries.EnumerateRead(DatabaseUpdateCallback, &function);

  nsresult rv = function.ErrorCode();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateJournals();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
TransactionBase::
UpdateRefcountFunction::DidCommit()
{
  mFileInfoEntries.EnumerateRead(FileInfoUpdateCallback, nullptr);

  if (NS_FAILED(RemoveJournals(mJournalsToRemoveAfterCommit))) {
    NS_WARNING("RemoveJournals failed!");
  }
}

void
TransactionBase::
UpdateRefcountFunction::DidAbort()
{
  if (NS_FAILED(RemoveJournals(mJournalsToRemoveAfterAbort))) {
    NS_WARNING("RemoveJournals failed!");
  }
}

void
TransactionBase::
UpdateRefcountFunction::StartSavepoint()
{
  MOZ_ASSERT(!mInSavepoint);
  MOZ_ASSERT(!mSavepointEntriesIndex.Count());

  mInSavepoint = true;
}

void
TransactionBase::
UpdateRefcountFunction::ReleaseSavepoint()
{
  MOZ_ASSERT(mInSavepoint);

  mSavepointEntriesIndex.Clear();
  mInSavepoint = false;
}

void
TransactionBase::
UpdateRefcountFunction::RollbackSavepoint()
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(mInSavepoint);

  struct Helper
  {
    static PLDHashOperator
    Rollback(const uint64_t& aKey, FileInfoEntry* aValue, void* /* aUserArg */)
    {
      MOZ_ASSERT(!IsOnBackgroundThread());
      MOZ_ASSERT(aValue);

      aValue->mDelta -= aValue->mSavepointDelta;
      return PL_DHASH_NEXT;
    }
  };

  mSavepointEntriesIndex.EnumerateRead(Helper::Rollback, nullptr);

  mInSavepoint = false;
  mSavepointEntriesIndex.Clear();
}

nsresult
TransactionBase::
UpdateRefcountFunction::ProcessValue(mozIStorageValueArray* aValues,
                                     int32_t aIndex,
                                     UpdateType aUpdateType)
{
  MOZ_ASSERT(aValues);

  int32_t type;
  nsresult rv = aValues->GetTypeOfIndex(aIndex, &type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (type == mozIStorageValueArray::VALUE_TYPE_NULL) {
    return NS_OK;
  }

  nsString ids;
  rv = aValues->GetString(aIndex, ids);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<int64_t> fileIds;
  rv = ConvertFileIdsToArray(ids, fileIds);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (uint32_t i = 0; i < fileIds.Length(); i++) {
    int64_t id = fileIds.ElementAt(i);

    FileInfoEntry* entry;
    if (!mFileInfoEntries.Get(id, &entry)) {
      nsRefPtr<FileInfo> fileInfo = mFileManager->GetFileInfo(id);
      MOZ_ASSERT(fileInfo);

      entry = new FileInfoEntry(fileInfo);
      mFileInfoEntries.Put(id, entry);
    }

    if (mInSavepoint) {
      mSavepointEntriesIndex.Put(id, entry);
    }

    switch (aUpdateType) {
      case eIncrement:
        entry->mDelta++;
        if (mInSavepoint) {
          entry->mSavepointDelta++;
        }
        break;
      case eDecrement:
        entry->mDelta--;
        if (mInSavepoint) {
          entry->mSavepointDelta--;
        }
        break;
      default:
        MOZ_CRASH("Unknown update type!");
    }
  }

  return NS_OK;
}

nsresult
TransactionBase::
UpdateRefcountFunction::CreateJournals()
{
  nsCOMPtr<nsIFile> journalDirectory = mFileManager->GetJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < mJournalsToCreateBeforeCommit.Length(); i++) {
    int64_t id = mJournalsToCreateBeforeCommit[i];

    nsCOMPtr<nsIFile> file =
      mFileManager->GetFileForId(journalDirectory, id);
    if (NS_WARN_IF(!file)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mJournalsToRemoveAfterAbort.AppendElement(id);
  }

  return NS_OK;
}

nsresult
TransactionBase::
UpdateRefcountFunction::RemoveJournals(const nsTArray<int64_t>& aJournals)
{
  nsCOMPtr<nsIFile> journalDirectory = mFileManager->GetJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t index = 0; index < aJournals.Length(); index++) {
    nsCOMPtr<nsIFile> file =
      mFileManager->GetFileForId(journalDirectory, aJournals[index]);
    if (NS_WARN_IF(!file)) {
      return NS_ERROR_FAILURE;
    }

    if (NS_FAILED(file->Remove(false))) {
      NS_WARNING("Failed to removed journal!");
    }
  }

  return NS_OK;
}

PLDHashOperator
TransactionBase::
UpdateRefcountFunction::DatabaseUpdateCallback(const uint64_t& aKey,
                                               FileInfoEntry* aValue,
                                               void* aUserArg)
{
  MOZ_ASSERT(aValue);
  MOZ_ASSERT(aUserArg);

  if (!aValue->mDelta) {
    return PL_DHASH_NEXT;
  }

  auto function = static_cast<DatabaseUpdateFunction*>(aUserArg);

  if (!function->Update(aKey, aValue->mDelta)) {
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

PLDHashOperator
TransactionBase::
UpdateRefcountFunction::FileInfoUpdateCallback(const uint64_t& aKey,
                                               FileInfoEntry* aValue,
                                               void* aUserArg)
{
  MOZ_ASSERT(aValue);

  if (aValue->mDelta) {
    aValue->mFileInfo->UpdateDBRefs(aValue->mDelta);
  }

  return PL_DHASH_NEXT;
}

bool
TransactionBase::UpdateRefcountFunction::
DatabaseUpdateFunction::Update(int64_t aId,
                               int32_t aDelta)
{
  nsresult rv = UpdateInternal(aId, aDelta);
  if (NS_FAILED(rv)) {
    mErrorCode = rv;
    return false;
  }

  return true;
}

nsresult
TransactionBase::UpdateRefcountFunction::
DatabaseUpdateFunction::UpdateInternal(int64_t aId,
                                       int32_t aDelta)
{
  nsresult rv;

  if (!mUpdateStatement) {
    rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE file "
      "SET refcount = refcount + :delta "
      "WHERE id = :id"
    ), getter_AddRefs(mUpdateStatement));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mozStorageStatementScoper updateScoper(mUpdateStatement);

  rv = mUpdateStatement->BindInt32ByName(NS_LITERAL_CSTRING("delta"), aDelta);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mUpdateStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mUpdateStatement->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t rows;
  rv = mConnection->GetAffectedRows(&rows);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (rows > 0) {
    if (!mSelectStatement) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT id "
        "FROM file "
        "WHERE id = :id"
      ), getter_AddRefs(mSelectStatement));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    mozStorageStatementScoper selectScoper(mSelectStatement);

    rv = mSelectStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool hasResult;
    rv = mSelectStatement->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!hasResult) {
      // Don't have to create the journal here, we can create all at once,
      // just before commit
      mFunction->mJournalsToCreateBeforeCommit.AppendElement(aId);
    }

    return NS_OK;
  }

  if (!mInsertStatement) {
    rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO file (id, refcount) "
      "VALUES(:id, :delta)"
    ), getter_AddRefs(mInsertStatement));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mozStorageStatementScoper insertScoper(mInsertStatement);

  rv = mInsertStatement->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mInsertStatement->BindInt32ByName(NS_LITERAL_CSTRING("delta"), aDelta);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mInsertStatement->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mFunction->mJournalsToRemoveAfterCommit.AppendElement(aId);
  return NS_OK;
}

TransactionBase::
AutoSavepoint::~AutoSavepoint()
{
  if (mTransaction) {
    mTransaction->AssertIsOnTransactionThread();
    MOZ_ASSERT(mTransaction->GetMode() == IDBTransaction::READ_WRITE ||
               mTransaction->GetMode() == IDBTransaction::VERSION_CHANGE);

    if (NS_FAILED(mTransaction->RollbackSavepoint())) {
      NS_WARNING("Failed to rollback savepoint!");
    }
  }
}

nsresult
TransactionBase::
AutoSavepoint::Start(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(aTransaction->GetMode() == IDBTransaction::READ_WRITE ||
             aTransaction->GetMode() == IDBTransaction::VERSION_CHANGE);
  MOZ_ASSERT(!mTransaction);

  nsresult rv = aTransaction->StartSavepoint();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mTransaction = aTransaction;

  return NS_OK;
}

nsresult
TransactionBase::
AutoSavepoint::Commit()
{
  MOZ_ASSERT(mTransaction);
  mTransaction->AssertIsOnTransactionThread();

  nsresult rv = mTransaction->ReleaseSavepoint();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mTransaction = nullptr;

  return NS_OK;
}

nsresult
VersionChangeTransactionOp::SendSuccessResult()
{
  AssertIsOnOwningThread();

  // Nothing to send here, the API assumes that this request always succeeds.
  return NS_OK;
}

bool
VersionChangeTransactionOp::SendFailureResult(nsresult aResultCode)
{
  AssertIsOnOwningThread();

  // The only option here is to cause the transaction to abort.
  return false;
}

void
VersionChangeTransactionOp::Cleanup()
{
  AssertIsOnOwningThread();

#ifdef DEBUG
  // A bit hacky but the VersionChangeTransactionOp is not generated in response
  // to a child request like most other database operations. Do this to make our
  // assertions happy.
  NoteActorDestroyed();
#endif

  TransactionDatabaseOperationBase::Cleanup();
}

nsresult
CreateObjectStoreOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "CreateObjectStoreOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  if (NS_WARN_IF(IndexedDatabaseManager::InLowDiskSpaceMode())) {
    return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
  }

  TransactionBase::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  TransactionBase::CachedStatement stmt;
  rv = aTransaction->GetCachedStatement(
    "INSERT INTO object_store (id, auto_increment, name, key_path) "
    "VALUES (:id, :auto_increment, :name, :key_path)",
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mMetadata.autoIncrement() ? 1 : 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mMetadata.name());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_NAMED_LITERAL_CSTRING(keyPath, "key_path");

  if (mMetadata.keyPath().IsValid()) {
    nsAutoString keyPathSerialization;
    mMetadata.keyPath().SerializeToString(keyPathSerialization);

    rv = stmt->BindStringByName(keyPath, keyPathSerialization);
  } else {
    rv = stmt->BindNullByName(keyPath);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  {
    int64_t id;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      aTransaction->Connection()->GetLastInsertRowID(&id)));
    MOZ_ASSERT(mMetadata.id() == id);
  }
#endif

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
DeleteObjectStoreOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "DeleteObjectStoreOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  TransactionBase::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  TransactionBase::CachedStatement stmt;
  rv = aTransaction->GetCachedStatement(
    "DELETE FROM object_store "
    "WHERE id = :id",
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                             mMetadata->mCommonMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mMetadata->mCommonMetadata.autoIncrement()) {
    aTransaction->ForgetModifiedAutoIncrementObjectStore(mMetadata);
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

CreateIndexOp::CreateIndexOp(VersionChangeTransaction* aTransaction,
                             const int64_t aObjectStoreId,
                             const IndexMetadata& aMetadata)
  : VersionChangeTransactionOp(aTransaction)
  , mMetadata(aMetadata)
  , mFileManager(aTransaction->GetDatabase()->GetFileManager())
  , mDatabaseId(aTransaction->DatabaseId())
  , mObjectStoreId(aObjectStoreId)
{
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(aMetadata.id());
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT(!mDatabaseId.IsEmpty());

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static void
    CopyUniqueValues(const IndexTable& aIndexes,
                     Maybe<UniqueIndexTable>& aMaybeUniqueIndexTable)
    {
      aMaybeUniqueIndexTable.emplace();

      const uint32_t indexCount = aIndexes.Count();
      MOZ_ASSERT(indexCount);

      aIndexes.EnumerateRead(Enumerate, aMaybeUniqueIndexTable.ptr());

      if (NS_WARN_IF(aMaybeUniqueIndexTable.ref().Count() != indexCount)) {
        aMaybeUniqueIndexTable.reset();
        return;
      }

#ifdef DEBUG
      aMaybeUniqueIndexTable.ref().MarkImmutable();
#endif
    }

  private:
    static PLDHashOperator
    Enumerate(const uint64_t& aKey, FullIndexMetadata* aValue, void* aClosure)
    {
      auto* uniqueIndexTable = static_cast<UniqueIndexTable*>(aClosure);
      MOZ_ASSERT(uniqueIndexTable);
      MOZ_ASSERT(!uniqueIndexTable->Get(aValue->mCommonMetadata.id()));

      if (NS_WARN_IF(!uniqueIndexTable->Put(aValue->mCommonMetadata.id(),
                                            aValue->mCommonMetadata.unique(),
                                            fallible))) {
        return PL_DHASH_STOP;
      }

      return PL_DHASH_NEXT;
    }
  };

  InitThreadLocals();

  const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata = 
    aTransaction->GetMetadataForObjectStoreId(aObjectStoreId);
  MOZ_ASSERT(objectStoreMetadata);

  Helper::CopyUniqueValues(objectStoreMetadata->mIndexes,
                           mMaybeUniqueIndexTable);
}

unsigned int CreateIndexOp::sThreadLocalIndex = kBadThreadLocalIndex;

// static
void
CreateIndexOp::InitThreadLocals()
{
  AssertIsOnBackgroundThread();

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static void
    Destroy(void* aThreadLocal)
    {
      delete static_cast<ThreadLocalJSRuntime*>(aThreadLocal);
    }
  };

  if (sThreadLocalIndex == kBadThreadLocalIndex) {
    if (NS_WARN_IF(PR_SUCCESS !=
                     PR_NewThreadPrivateIndex(&sThreadLocalIndex,
                                              &Helper::Destroy))) {
      return;
    }
  }

  MOZ_ASSERT(sThreadLocalIndex != kBadThreadLocalIndex);
}

nsresult
CreateIndexOp::InsertDataFromObjectStore(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(!IndexedDatabaseManager::InLowDiskSpaceMode());
  MOZ_ASSERT(mMaybeUniqueIndexTable);

  PROFILER_LABEL("IndexedDB",
                 "CreateIndexOp::InsertDataFromObjectStore",
                 js::ProfileEntry::Category::STORAGE);

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(
    "SELECT id, data, file_ids, key_value "
    "FROM object_data "
    "WHERE object_store_id = :osid",
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    // Bail early if we have no data to avoid creating the runtime below.
    return NS_OK;
  }

  ThreadLocalJSRuntime* runtime = ThreadLocalJSRuntime::GetOrCreate();
  if (NS_WARN_IF(!runtime)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  JSContext* cx = runtime->Context();
  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, runtime->Global());

  do {
    StructuredCloneReadInfo cloneInfo;
    rv = GetStructuredCloneReadInfoFromStatement(stmt, 1, 2, mFileManager,
                                                 &cloneInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    JS::Rooted<JS::Value> clone(cx);
    if (NS_WARN_IF(!IDBObjectStore::DeserializeIndexValue(cx, cloneInfo,
                                                          &clone))) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }

    nsTArray<IndexUpdateInfo> updateInfo;
    rv = IDBObjectStore::AppendIndexUpdateInfo(mMetadata.id(),
                                               mMetadata.keyPath(),
                                               mMetadata.unique(),
                                               mMetadata.multiEntry(),
                                               cx,
                                               clone,
                                               updateInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int64_t objectDataId = stmt->AsInt64(0);

    Key key;
    rv = key.SetFromStatement(stmt, 3);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = UpdateIndexes(aTransaction,
                       mMaybeUniqueIndexTable.ref(),
                       key,
                       false,
                       objectDataId,
                       updateInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&hasResult)) && hasResult);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool
CreateIndexOp::Init(TransactionBase* aTransaction)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);

  if (NS_WARN_IF(!mMaybeUniqueIndexTable) ||
      NS_WARN_IF(sThreadLocalIndex == kBadThreadLocalIndex)) {
    return false;
  }

  return true;
}

nsresult
CreateIndexOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "CreateIndexOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  if (NS_WARN_IF(IndexedDatabaseManager::InLowDiskSpaceMode())) {
    return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
  }

  TransactionBase::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  TransactionBase::CachedStatement stmt;
  rv = aTransaction->GetCachedStatement(
    "INSERT INTO object_store_index (id, name, key_path, unique_index, "
                                    "multientry, object_store_id) "
    "VALUES (:id, :name, :key_path, :unique, :multientry, :osid)",
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mMetadata.name());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString keyPathSerialization;
  mMetadata.keyPath().SerializeToString(keyPathSerialization);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"),
                              keyPathSerialization);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("unique"),
                             mMetadata.unique() ? 1 : 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("multientry"),
                             mMetadata.multiEntry() ? 1 : 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  {
    int64_t id;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      aTransaction->Connection()->GetLastInsertRowID(&id)));
    MOZ_ASSERT(mMetadata.id() == id);
  }
#endif

  rv = InsertDataFromObjectStore(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

const JSClass CreateIndexOp::ThreadLocalJSRuntime::kGlobalClass = {
  "IndexedDBTransactionThreadGlobal",
  JSCLASS_GLOBAL_FLAGS,
  /* addProperty */ nullptr,
  /* delProperty */ nullptr,
  /* getProperty */ nullptr,
  /* setProperty */ nullptr,
  /* enumerate */ nullptr,
  /* resolve */ nullptr,
  /* convert */ nullptr,
  /* finalize */ nullptr,
  /* call */ nullptr,
  /* hasInstance */ nullptr,
  /* construct */ nullptr,
  /* trace */ JS_GlobalObjectTraceHook
};

// static
auto
CreateIndexOp::
ThreadLocalJSRuntime::GetOrCreate() -> ThreadLocalJSRuntime*
{
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(CreateIndexOp::kBadThreadLocalIndex !=
             CreateIndexOp::sThreadLocalIndex);

  auto* runtime = static_cast<ThreadLocalJSRuntime*>(
    PR_GetThreadPrivate(CreateIndexOp::sThreadLocalIndex));
  if (runtime) {
    return runtime;
  }

  nsAutoPtr<ThreadLocalJSRuntime> newRuntime(new ThreadLocalJSRuntime());

  if (NS_WARN_IF(!newRuntime->Init())) {
    return nullptr;
  }

  DebugOnly<PRStatus> status =
    PR_SetThreadPrivate(CreateIndexOp::sThreadLocalIndex, newRuntime);
  MOZ_ASSERT(status == PR_SUCCESS);

  return newRuntime.forget();
}

bool
CreateIndexOp::
ThreadLocalJSRuntime::Init()
{
  MOZ_ASSERT(!IsOnBackgroundThread());

  mRuntime = JS_NewRuntime(kRuntimeHeapSize);
  if (NS_WARN_IF(!mRuntime)) {
    return false;
  }

  // Not setting this will cause JS_CHECK_RECURSION to report false positives.
  JS_SetNativeStackQuota(mRuntime, 128 * sizeof(size_t) * 1024); 

  mContext = JS_NewContext(mRuntime, 0);
  if (NS_WARN_IF(!mContext)) {
    return false;
  }

  JSAutoRequest ar(mContext);

  mGlobal = JS_NewGlobalObject(mContext, &kGlobalClass, nullptr,
                               JS::FireOnNewGlobalHook);
  if (NS_WARN_IF(!mGlobal)) {
    return false;
  }

  return true;
}

nsresult
DeleteIndexOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "DeleteIndexOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  TransactionBase::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  TransactionBase::CachedStatement stmt;
  rv = aTransaction->GetCachedStatement(
    "DELETE FROM object_store_index "
    "WHERE id = :id ",
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mIndexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
NormalTransactionOp::SendSuccessResult()
{
  AssertIsOnOwningThread();

  if (!IsActorDestroyed()) {
    RequestResponse response;
    GetResponse(response);

    MOZ_ASSERT(response.type() != RequestResponse::T__None);

    if (response.type() == RequestResponse::Tnsresult) {
      MOZ_ASSERT(NS_FAILED(response.get_nsresult()));

      return response.get_nsresult();
    }

    if (NS_WARN_IF(!PBackgroundIDBRequestParent::Send__delete__(this,
                                                                response))) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  mResponseSent = true;

  return NS_OK;
}

bool
NormalTransactionOp::SendFailureResult(nsresult aResultCode)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));

  bool result = false;

  if (!IsActorDestroyed()) {
    result =
      PBackgroundIDBRequestParent::Send__delete__(this,
                                                  ClampResultCode(aResultCode));
  }

  mResponseSent = true;

  return result;
}

void
NormalTransactionOp::Cleanup()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(!IsActorDestroyed(), mResponseSent);

  TransactionDatabaseOperationBase::Cleanup();
}

void
NormalTransactionOp::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

ObjectStoreAddOrPutRequestOp::ObjectStoreAddOrPutRequestOp(
                                                  TransactionBase* aTransaction,
                                                  const RequestParams& aParams)
  : NormalTransactionOp(aTransaction)
  , mParams(aParams.type() == RequestParams::TObjectStoreAddParams ?
              aParams.get_ObjectStoreAddParams().commonParams() :
              aParams.get_ObjectStorePutParams().commonParams())
  , mGroup(aTransaction->GetDatabase()->Group())
  , mOrigin(aTransaction->GetDatabase()->Origin())
  , mPersistenceType(aTransaction->GetDatabase()->Type())
  , mOverwrite(aParams.type() == RequestParams::TObjectStorePutParams)
{
  MOZ_ASSERT(aParams.type() == RequestParams::TObjectStoreAddParams ||
             aParams.type() == RequestParams::TObjectStorePutParams);

  mMetadata =
    aTransaction->GetMetadataForObjectStoreId(mParams.objectStoreId());
  MOZ_ASSERT(mMetadata);
}

nsresult
ObjectStoreAddOrPutRequestOp::CopyFileData(nsIInputStream* aInputStream,
                                           nsIOutputStream* aOutputStream)
{
  AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "ObjectStoreAddOrPutRequestOp::CopyFileData",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv;

  do {
    char copyBuffer[kFileCopyBufferSize];

    uint32_t numRead;
    rv = aInputStream->Read(copyBuffer, sizeof(copyBuffer), &numRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    if (!numRead) {
      break;
    }

    uint32_t numWrite;
    rv = aOutputStream->Write(copyBuffer, numRead, &numWrite);
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
    rv = aOutputStream->Flush();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv2 = aOutputStream->Close();
  if (NS_WARN_IF(NS_FAILED(rv2))) {
    return NS_SUCCEEDED(rv) ? rv2 : rv;
  }

  return rv;
}

bool
ObjectStoreAddOrPutRequestOp::Init(TransactionBase* aTransaction)
{
  AssertIsOnOwningThread();

  const nsTArray<IndexUpdateInfo>& indexUpdateInfos =
    mParams.indexUpdateInfos();

  if (!indexUpdateInfos.IsEmpty()) {
    const uint32_t count = indexUpdateInfos.Length();

    mUniqueIndexTable.emplace();

    for (uint32_t index = 0; index < count; index++) {
      const IndexUpdateInfo& updateInfo = indexUpdateInfos[index];

      nsRefPtr<FullIndexMetadata> indexMetadata;
      MOZ_ALWAYS_TRUE(mMetadata->mIndexes.Get(updateInfo.indexId(),
                                              getter_AddRefs(indexMetadata)));

      MOZ_ASSERT(!indexMetadata->mDeleted);

      const int64_t& indexId = indexMetadata->mCommonMetadata.id();
      const bool& unique = indexMetadata->mCommonMetadata.unique();

      MOZ_ASSERT(indexId == updateInfo.indexId());
      MOZ_ASSERT_IF(!indexMetadata->mCommonMetadata.multiEntry(),
                    !mUniqueIndexTable.ref().Get(indexId));

      if (NS_WARN_IF(!mUniqueIndexTable.ref().Put(indexId, unique, fallible))) {
        return false;
      }
    }
  } else if (mOverwrite) {
    // Kinda lame...
    mUniqueIndexTable.emplace();
  }

#ifdef DEBUG
  if (mUniqueIndexTable) {
    mUniqueIndexTable.ref().MarkImmutable();
  }
#endif

  const nsTArray<DatabaseFileOrMutableFileId>& files = mParams.files();

  if (!files.IsEmpty()) {
    const uint32_t count = files.Length();

    if (NS_WARN_IF(!mStoredFileInfos.SetCapacity(count))) {
      return false;
    }

    nsRefPtr<FileManager> fileManager =
      aTransaction->GetDatabase()->GetFileManager();
    MOZ_ASSERT(fileManager);

    for (uint32_t index = 0; index < count; index++) {
      const DatabaseFileOrMutableFileId& fileOrFileId = files[index];
      MOZ_ASSERT(fileOrFileId.type() ==
                   DatabaseFileOrMutableFileId::
                     TPBackgroundIDBDatabaseFileParent ||
                 fileOrFileId.type() == DatabaseFileOrMutableFileId::Tint64_t);

      StoredFileInfo* storedFileInfo = mStoredFileInfos.AppendElement();
      MOZ_ASSERT(storedFileInfo);

      switch (fileOrFileId.type()) {
        case DatabaseFileOrMutableFileId::TPBackgroundIDBDatabaseFileParent: {
          storedFileInfo->mFileActor =
            static_cast<DatabaseFile*>(
              fileOrFileId.get_PBackgroundIDBDatabaseFileParent());
          MOZ_ASSERT(storedFileInfo->mFileActor);

          storedFileInfo->mFileInfo = storedFileInfo->mFileActor->GetFileInfo();
          MOZ_ASSERT(storedFileInfo->mFileInfo);

          storedFileInfo->mInputStream =
            storedFileInfo->mFileActor->GetInputStream();
          if (storedFileInfo->mInputStream && !mFileManager) {
            mFileManager = fileManager;
          }
          break;
        }

        case DatabaseFileOrMutableFileId::Tint64_t:
          storedFileInfo->mFileInfo =
            fileManager->GetFileInfo(fileOrFileId.get_int64_t());
          MOZ_ASSERT(storedFileInfo->mFileInfo);
          break;

        case DatabaseFileOrMutableFileId::TPBackgroundIDBDatabaseFileChild:
        default:
          MOZ_CRASH("Should never get here!");
      }
    }
  }

  return true;
}

nsresult
ObjectStoreAddOrPutRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT_IF(mFileManager, !mStoredFileInfos.IsEmpty());

  PROFILER_LABEL("IndexedDB",
                 "ObjectStoreAddOrPutRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  if (NS_WARN_IF(IndexedDatabaseManager::InLowDiskSpaceMode())) {
    return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
  }

  TransactionBase::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This will be the final key we use.
  Key& key = mResponse;
  key = mParams.key();

  const bool keyUnset = key.IsUnset();
  const int64_t osid = mParams.objectStoreId();
  const KeyPath& keyPath = mMetadata->mCommonMetadata.keyPath();

  // The "|| keyUnset" here is mostly a debugging tool. If a key isn't
  // specified we should never have a collision and so it shouldn't matter
  // if we allow overwrite or not. By not allowing overwrite we raise
  // detectable errors rather than corrupting data.
  TransactionBase::CachedStatement stmt;
  if (!mOverwrite || keyUnset) {
    rv = aTransaction->GetCachedStatement(
      "INSERT INTO object_data (object_store_id, key_value, data, file_ids) "
      "VALUES (:osid, :key_value, :data, :file_ids)",
      &stmt);
  } else {
    rv = aTransaction->GetCachedStatement(
      "INSERT OR REPLACE INTO object_data (object_store_id, key_value, data, "
                                          "file_ids) "
      "VALUES (:osid, :key_value, :data, :file_ids)",
    &stmt);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), osid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!keyUnset || mMetadata->mCommonMetadata.autoIncrement(),
             "Should have key unless autoIncrement");

  int64_t autoIncrementNum = 0;

  if (mMetadata->mCommonMetadata.autoIncrement()) {
    if (keyUnset) {
      autoIncrementNum = mMetadata->mNextAutoIncrementId;

      MOZ_ASSERT(autoIncrementNum > 0);

      if (autoIncrementNum > (1LL << 53)) {
        IDB_REPORT_INTERNAL_ERR();
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      key.SetFromInteger(autoIncrementNum);
    } else if (key.IsFloat() &&
               key.ToFloat() >= mMetadata->mNextAutoIncrementId) {
      autoIncrementNum = floor(key.ToFloat());
    }

    if (keyUnset && keyPath.IsValid()) {
      const SerializedStructuredCloneWriteInfo& cloneInfo = mParams.cloneInfo();
      MOZ_ASSERT(cloneInfo.offsetToKeyProp());
      MOZ_ASSERT(cloneInfo.data().Length() > sizeof(uint64_t));
      MOZ_ASSERT(cloneInfo.offsetToKeyProp() <=
                 (cloneInfo.data().Length() - sizeof(uint64_t)));

      // Special case where someone put an object into an autoIncrement'ing
      // objectStore with no key in its keyPath set. We needed to figure out
      // which row id we would get above before we could set that properly.
      uint8_t* keyPropPointer =
        const_cast<uint8_t*>(cloneInfo.data().Elements() +
                             cloneInfo.offsetToKeyProp());
      uint64_t keyPropValue =
        ReinterpretDoubleAsUInt64(static_cast<double>(autoIncrementNum));

      LittleEndian::writeUint64(keyPropPointer, keyPropValue);
    }
  }

  key.BindToStatement(stmt, NS_LITERAL_CSTRING("key_value"));

  // Compress the bytes before adding into the database.
  const char* uncompressed =
    reinterpret_cast<const char*>(mParams.cloneInfo().data().Elements());
  size_t uncompressedLength = mParams.cloneInfo().data().Length();

  // We don't have a smart pointer class that calls moz_free, so we need to
  // manage | compressed | manually.
  {
    size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);

    // moz_malloc is equivalent to NS_Alloc, which we use because mozStorage
    // expects to be able to free the adopted pointer with NS_Free.
    char* compressed = static_cast<char*>(moz_malloc(compressedLength));
    if (NS_WARN_IF(!compressed)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    snappy::RawCompress(uncompressed, uncompressedLength, compressed,
                        &compressedLength);

    uint8_t* dataBuffer = reinterpret_cast<uint8_t*>(compressed);
    size_t dataBufferLength = compressedLength;

    // If this call succeeds, | compressed | is now owned by the statement, and
    // we are no longer responsible for it.
    rv = stmt->BindAdoptedBlobByName(NS_LITERAL_CSTRING("data"), dataBuffer,
                                     dataBufferLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      moz_free(compressed);
      return rv;
    }
  }

  nsCOMPtr<nsIFile> fileDirectory;
  nsCOMPtr<nsIFile> journalDirectory;

  if (mFileManager) {
    fileDirectory = mFileManager->GetDirectory();
    if (NS_WARN_IF(!fileDirectory)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    journalDirectory = mFileManager->EnsureJournalDirectory();
    if (NS_WARN_IF(!journalDirectory)) {
      IDB_REPORT_INTERNAL_ERR();
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    DebugOnly<bool> exists;
    MOZ_ASSERT(NS_SUCCEEDED(fileDirectory->Exists(&exists)));
    MOZ_ASSERT(exists);

    DebugOnly<bool> isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(fileDirectory->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory);

    MOZ_ASSERT(NS_SUCCEEDED(journalDirectory->Exists(&exists)));
    MOZ_ASSERT(exists);

    MOZ_ASSERT(NS_SUCCEEDED(journalDirectory->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory);
  }

  if (!mStoredFileInfos.IsEmpty()) {
    nsAutoString fileIds;

    for (uint32_t count = mStoredFileInfos.Length(), index = 0;
         index < count;
         index++) {
      StoredFileInfo& storedFileInfo = mStoredFileInfos[index];
      MOZ_ASSERT(storedFileInfo.mFileInfo);

      const int64_t id = storedFileInfo.mFileInfo->Id();

      nsCOMPtr<nsIInputStream> inputStream;
      storedFileInfo.mInputStream.swap(inputStream);

      if (inputStream) {
        MOZ_ASSERT(fileDirectory);
        MOZ_ASSERT(journalDirectory);

        nsCOMPtr<nsIFile> diskFile =
          mFileManager->GetFileForId(fileDirectory, id);
        if (NS_WARN_IF(!diskFile)) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        bool exists;
        rv = diskFile->Exists(&exists);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        if (exists) {
          bool isFile;
          rv = diskFile->IsFile(&isFile);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          if (NS_WARN_IF(!isFile)) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          uint64_t inputStreamSize;
          rv = inputStream->Available(&inputStreamSize);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          int64_t fileSize;
          rv = diskFile->GetFileSize(&fileSize);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          if (NS_WARN_IF(fileSize < 0)) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          if (NS_WARN_IF(uint64_t(fileSize) != inputStreamSize)) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }
        } else {
          // Create a journal file first.
          nsCOMPtr<nsIFile> journalFile =
            mFileManager->GetFileForId(journalDirectory, id);
          if (NS_WARN_IF(!journalFile)) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          rv = journalFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          // Now try to copy the stream.
          nsRefPtr<FileOutputStream> outputStream =
            FileOutputStream::Create(mPersistenceType,
                                     mGroup,
                                     mOrigin,
                                     diskFile);
          if (NS_WARN_IF(!outputStream)) {
            IDB_REPORT_INTERNAL_ERR();
            return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }

          rv = CopyFileData(inputStream, outputStream);
          if (NS_FAILED(rv) &&
              NS_ERROR_GET_MODULE(rv) != NS_ERROR_MODULE_DOM_INDEXEDDB) {
            IDB_REPORT_INTERNAL_ERR();
            rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            // Try to remove the file if the copy failed.
            nsresult rv2;
            int64_t fileSize;

            if (mFileManager->EnforcingQuota()) {
              rv2 = diskFile->GetFileSize(&fileSize);
              if (NS_WARN_IF(NS_FAILED(rv2))) {
                return rv;
              }
            }

            rv2 = diskFile->Remove(false);
            if (NS_WARN_IF(NS_FAILED(rv2))) {
              return rv;
            }

            if (mFileManager->EnforcingQuota()) {
              QuotaManager* quotaManager = QuotaManager::Get();
              MOZ_ASSERT(quotaManager);

              quotaManager->DecreaseUsageForOrigin(mFileManager->Type(),
                                                   mFileManager->Group(),
                                                   mFileManager->Origin(),
                                                   fileSize);
            }

            return rv;
          }

          storedFileInfo.mCopiedSuccessfully = true;
        }
      }

      if (index) {
        fileIds.Append(' ');
      }
      fileIds.AppendInt(id);
    }

    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("file_ids"), fileIds);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("file_ids"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = stmt->Execute();
  if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
    MOZ_ASSERT(!keyUnset, "Generated key had a collision!");
    return rv;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t objectDataId;
  rv = aTransaction->Connection()->GetLastInsertRowID(&objectDataId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update our indexes if needed.
  if (mOverwrite || !mParams.indexUpdateInfos().IsEmpty()) {
    MOZ_ASSERT(mUniqueIndexTable);

    rv = UpdateIndexes(aTransaction,
                       mUniqueIndexTable.ref(),
                       key,
                       mOverwrite,
                       objectDataId,
                       mParams.indexUpdateInfos());
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (autoIncrementNum) {
    mMetadata->mNextAutoIncrementId = autoIncrementNum + 1;
    aTransaction->NoteModifiedAutoIncrementObjectStore(mMetadata);
  }

  return NS_OK;
}

void
ObjectStoreAddOrPutRequestOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  if (mOverwrite) {
    aResponse = ObjectStorePutResponse(mResponse);
  } else {
    aResponse = ObjectStoreAddResponse(mResponse);
  }
}

void
ObjectStoreAddOrPutRequestOp::Cleanup()
{
  AssertIsOnOwningThread();

  if (!mStoredFileInfos.IsEmpty()) {
    for (uint32_t count = mStoredFileInfos.Length(), index = 0;
         index < count;
         index++) {
      StoredFileInfo& storedFileInfo = mStoredFileInfos[index];
      nsRefPtr<DatabaseFile>& fileActor = storedFileInfo.mFileActor;

      MOZ_ASSERT_IF(!fileActor, !storedFileInfo.mCopiedSuccessfully);

      if (fileActor && storedFileInfo.mCopiedSuccessfully) {
        fileActor->ClearInputStream();
      }
    }

    mStoredFileInfos.Clear();
  }

  NormalTransactionOp::Cleanup();
}

ObjectStoreGetRequestOp::ObjectStoreGetRequestOp(TransactionBase* aTransaction,
                                                 const RequestParams& aParams,
                                                 bool aGetAll)
  : NormalTransactionOp(aTransaction)
  , mObjectStoreId(aGetAll ?
                     aParams.get_ObjectStoreGetAllParams().objectStoreId() :
                     aParams.get_ObjectStoreGetParams().objectStoreId())
  , mFileManager(aTransaction->GetDatabase()->GetFileManager())
  , mOptionalKeyRange(aGetAll ?
                        aParams.get_ObjectStoreGetAllParams()
                               .optionalKeyRange() :
                        OptionalKeyRange(aParams.get_ObjectStoreGetParams()
                                                .keyRange()))
  , mBackgroundParent(aTransaction->GetBackgroundParent())
  , mLimit(aGetAll ? aParams.get_ObjectStoreGetAllParams().limit() : 1)
  , mGetAll(aGetAll)
{
  MOZ_ASSERT(aParams.type() == RequestParams::TObjectStoreGetParams ||
             aParams.type() == RequestParams::TObjectStoreGetAllParams);
  MOZ_ASSERT(mObjectStoreId);
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT_IF(!aGetAll,
                mOptionalKeyRange.type() ==
                  OptionalKeyRange::TSerializedKeyRange);
  MOZ_ASSERT(mBackgroundParent);
}

nsresult
ObjectStoreGetRequestOp::ConvertResponse(
                             uint32_t aIndex,
                             SerializedStructuredCloneReadInfo& aSerializedInfo)
{
  MOZ_ASSERT(aIndex < mResponse.Length());

  StructuredCloneReadInfo& info = mResponse[aIndex];

  info.mData.SwapElements(aSerializedInfo.data());

  FallibleTArray<PBlobParent*> blobs;
  FallibleTArray<intptr_t> fileInfos;
  nsresult rv = ConvertBlobsToActors(mBackgroundParent,
                                     mFileManager,
                                     info.mFiles,
                                     blobs,
                                     fileInfos);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(aSerializedInfo.blobsParent().IsEmpty());
  MOZ_ASSERT(aSerializedInfo.fileInfos().IsEmpty());

  aSerializedInfo.blobsParent().SwapElements(blobs);
  aSerializedInfo.fileInfos().SwapElements(fileInfos);

  return NS_OK;
}

nsresult
ObjectStoreGetRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT_IF(!mGetAll,
                mOptionalKeyRange.type() ==
                  OptionalKeyRange::TSerializedKeyRange);
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  PROFILER_LABEL("IndexedDB",
                 "ObjectStoreGetRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool hasKeyRange =
    mOptionalKeyRange.type() == OptionalKeyRange::TSerializedKeyRange;

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.get_SerializedKeyRange(),
                                NS_LITERAL_CSTRING("key_value"),
                                keyRangeClause);
  }

  nsCString limitClause;
  if (mLimit) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query =
    NS_LITERAL_CSTRING("SELECT data, file_ids "
                       "FROM object_data "
                       "WHERE object_store_id = :osid") +
    keyRangeClause +
    NS_LITERAL_CSTRING(" ORDER BY key_value ASC") +
    limitClause;

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.get_SerializedKeyRange(),
                                 stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    StructuredCloneReadInfo* cloneInfo = mResponse.AppendElement();
    if (NS_WARN_IF(!cloneInfo)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = GetStructuredCloneReadInfoFromStatement(stmt, 0, 1, mFileManager,
                                                 cloneInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

void
ObjectStoreGetRequestOp::GetResponse(RequestResponse& aResponse)
{
  MOZ_ASSERT_IF(mLimit, mResponse.Length() <= mLimit);

  if (mGetAll) {
    aResponse = ObjectStoreGetAllResponse();

    if (!mResponse.IsEmpty()) {
      FallibleTArray<SerializedStructuredCloneReadInfo> fallibleCloneInfos;
      if (NS_WARN_IF(!fallibleCloneInfos.SetLength(mResponse.Length()))) {
        aResponse = NS_ERROR_OUT_OF_MEMORY;
        return;
      }

      for (uint32_t count = mResponse.Length(), index = 0;
           index < count;
           index++) {
        nsresult rv = ConvertResponse(index, fallibleCloneInfos[index]);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          aResponse = rv;
          return;
        }
      }

      nsTArray<SerializedStructuredCloneReadInfo>& cloneInfos =
        aResponse.get_ObjectStoreGetAllResponse().cloneInfos();

      fallibleCloneInfos.SwapElements(cloneInfos);
    }

    return;
  }

  aResponse = ObjectStoreGetResponse();

  if (!mResponse.IsEmpty()) {
    SerializedStructuredCloneReadInfo& serializedInfo =
      aResponse.get_ObjectStoreGetResponse().cloneInfo();

    nsresult rv = ConvertResponse(0, serializedInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResponse = rv;
    }
  }
}

nsresult
ObjectStoreGetAllKeysRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "ObjectStoreGetAllKeysRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool hasKeyRange =
    mParams.optionalKeyRange().type() == OptionalKeyRange::TSerializedKeyRange;

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(
      mParams.optionalKeyRange().get_SerializedKeyRange(),
      NS_LITERAL_CSTRING("key_value"),
      keyRangeClause);
  }

  nsAutoCString limitClause;
  if (uint32_t limit = mParams.limit()) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(limit);
  }

  nsCString query =
    NS_LITERAL_CSTRING("SELECT key_value "
                       "FROM object_data "
                       "WHERE object_store_id = :osid") +
    keyRangeClause +
    NS_LITERAL_CSTRING(" ORDER BY key_value ASC") +
    limitClause;

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                             mParams.objectStoreId());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(
      mParams.optionalKeyRange().get_SerializedKeyRange(),
      stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    Key* key = mResponse.AppendElement();
    if (NS_WARN_IF(!key)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = key->SetFromStatement(stmt, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
ObjectStoreGetAllKeysRequestOp::GetResponse(RequestResponse& aResponse)
{
  aResponse = ObjectStoreGetAllKeysResponse();

  if (!mResponse.IsEmpty()) {
    nsTArray<Key>& response =
      aResponse.get_ObjectStoreGetAllKeysResponse().keys();
    mResponse.SwapElements(response);
  }
}

nsresult
ObjectStoreDeleteRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "ObjectStoreDeleteRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  TransactionBase::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString keyRangeClause;
  GetBindingClauseForKeyRange(mParams.keyRange(),
                              NS_LITERAL_CSTRING("key_value"),
                              keyRangeClause);

  nsCString query =
    NS_LITERAL_CSTRING("DELETE FROM object_data "
                       "WHERE object_store_id = :osid") +
    keyRangeClause;

  TransactionBase::CachedStatement stmt;
  rv = aTransaction->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                             mParams.objectStoreId());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = BindKeyRangeToStatement(mParams.keyRange(), stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
ObjectStoreClearRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "ObjectStoreClearRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  TransactionBase::AutoSavepoint autoSave;
  nsresult rv = autoSave.Start(aTransaction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  TransactionBase::CachedStatement stmt;
  rv = aTransaction->GetCachedStatement(
    "DELETE FROM object_data "
    "WHERE object_store_id = :osid",
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                             mParams.objectStoreId());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoSave.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
ObjectStoreCountRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "ObjectStoreCountRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool hasKeyRange =
    mParams.optionalKeyRange().type() == OptionalKeyRange::TSerializedKeyRange;

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(
      mParams.optionalKeyRange().get_SerializedKeyRange(),
      NS_LITERAL_CSTRING("key_value"),
      keyRangeClause);
  }

  nsCString query =
    NS_LITERAL_CSTRING("SELECT count(*) "
                       "FROM object_data "
                       "WHERE object_store_id = :osid") +
    keyRangeClause;

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"),
                             mParams.objectStoreId());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(
      mParams.optionalKeyRange().get_SerializedKeyRange(),
      stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  int64_t count = stmt->AsInt64(0);
  if (NS_WARN_IF(count < 0)) {
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mResponse.count() = count;

  return NS_OK;
}

// static
already_AddRefed<FullIndexMetadata>
IndexRequestOpBase::IndexMetadataForParams(TransactionBase* aTransaction,
                                           const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetParams ||
             aParams.type() == RequestParams::TIndexGetKeyParams ||
             aParams.type() == RequestParams::TIndexGetAllParams ||
             aParams.type() == RequestParams::TIndexGetAllKeysParams ||
             aParams.type() == RequestParams::TIndexCountParams);

  uint64_t objectStoreId;
  uint64_t indexId;

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

  const nsRefPtr<FullObjectStoreMetadata> objectStoreMetadata =
    aTransaction->GetMetadataForObjectStoreId(objectStoreId);
  MOZ_ASSERT(objectStoreMetadata);

  nsRefPtr<FullIndexMetadata> indexMetadata =
    aTransaction->GetMetadataForIndexId(objectStoreMetadata, indexId);
  MOZ_ASSERT(indexMetadata);

  return indexMetadata.forget();
}

IndexGetRequestOp::IndexGetRequestOp(TransactionBase* aTransaction,
                                     const RequestParams& aParams,
                                     bool aGetAll)
  : IndexRequestOpBase(aTransaction, aParams)
  , mFileManager(aTransaction->GetDatabase()->GetFileManager())
  , mOptionalKeyRange(aGetAll ?
                        aParams.get_IndexGetAllParams().optionalKeyRange() :
                        OptionalKeyRange(aParams.get_IndexGetParams()
                                                .keyRange()))
  , mBackgroundParent(aTransaction->GetBackgroundParent())
  , mLimit(aGetAll ? aParams.get_IndexGetAllParams().limit() : 1)
  , mGetAll(aGetAll)
{
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetParams ||
             aParams.type() == RequestParams::TIndexGetAllParams);
  MOZ_ASSERT(mFileManager);
  MOZ_ASSERT_IF(!aGetAll,
                mOptionalKeyRange.type() ==
                  OptionalKeyRange::TSerializedKeyRange);
  MOZ_ASSERT(mBackgroundParent);
}

nsresult
IndexGetRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT_IF(!mGetAll,
                mOptionalKeyRange.type() ==
                  OptionalKeyRange::TSerializedKeyRange);
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  PROFILER_LABEL("IndexedDB",
                 "IndexGetRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool hasKeyRange =
    mOptionalKeyRange.type() == OptionalKeyRange::TSerializedKeyRange;

  nsCString indexTable;
  if (mMetadata->mCommonMetadata.unique()) {
    indexTable.AssignLiteral("unique_index_data ");
  }
  else {
    indexTable.AssignLiteral("index_data ");
  }

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.get_SerializedKeyRange(),
                                NS_LITERAL_CSTRING("value"),
                                keyRangeClause);
  }

  nsCString limitClause;
  if (mLimit) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query =
    NS_LITERAL_CSTRING("SELECT data, file_ids "
                       "FROM object_data "
                       "INNER JOIN ") +
    indexTable +
    NS_LITERAL_CSTRING("AS index_table "
                       "ON object_data.id = index_table.object_data_id "
                       "WHERE index_id = :index_id") +
    keyRangeClause +
    limitClause;

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                             mMetadata->mCommonMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.get_SerializedKeyRange(),
                                 stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    StructuredCloneReadInfo* cloneInfo = mResponse.AppendElement();
    if (NS_WARN_IF(!cloneInfo)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = GetStructuredCloneReadInfoFromStatement(stmt, 0, 1, mFileManager,
                                                 cloneInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

void
IndexGetRequestOp::GetResponse(RequestResponse& aResponse)
{
  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  if (mGetAll) {
    aResponse = IndexGetAllResponse();

    if (!mResponse.IsEmpty()) {
      FallibleTArray<SerializedStructuredCloneReadInfo> fallibleCloneInfos;
      if (NS_WARN_IF(!fallibleCloneInfos.SetLength(mResponse.Length()))) {
        aResponse = NS_ERROR_OUT_OF_MEMORY;
        return;
      }

      for (uint32_t count = mResponse.Length(), index = 0;
           index < count;
           index++) {
        StructuredCloneReadInfo& info = mResponse[index];

        SerializedStructuredCloneReadInfo& serializedInfo =
          fallibleCloneInfos[index];

        info.mData.SwapElements(serializedInfo.data());

        FallibleTArray<PBlobParent*> blobs;
        FallibleTArray<intptr_t> fileInfos;
        nsresult rv = ConvertBlobsToActors(mBackgroundParent,
                                           mFileManager,
                                           info.mFiles,
                                           blobs,
                                           fileInfos);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          aResponse = rv;
          return;
        }

        MOZ_ASSERT(serializedInfo.blobsParent().IsEmpty());
        MOZ_ASSERT(serializedInfo.fileInfos().IsEmpty());

        serializedInfo.blobsParent().SwapElements(blobs);
        serializedInfo.fileInfos().SwapElements(fileInfos);
      }

      nsTArray<SerializedStructuredCloneReadInfo>& cloneInfos =
        aResponse.get_IndexGetAllResponse().cloneInfos();

      fallibleCloneInfos.SwapElements(cloneInfos);
    }

    return;
  }

  aResponse = IndexGetResponse();

  if (!mResponse.IsEmpty()) {
    StructuredCloneReadInfo& info = mResponse[0];

    SerializedStructuredCloneReadInfo& serializedInfo =
      aResponse.get_IndexGetResponse().cloneInfo();

    info.mData.SwapElements(serializedInfo.data());

    FallibleTArray<PBlobParent*> blobs;
    FallibleTArray<intptr_t> fileInfos;
    nsresult rv =
      ConvertBlobsToActors(mBackgroundParent,
                           mFileManager,
                           info.mFiles,
                           blobs,
                           fileInfos);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResponse = rv;
      return;
    }

    MOZ_ASSERT(serializedInfo.blobsParent().IsEmpty());
    MOZ_ASSERT(serializedInfo.fileInfos().IsEmpty());

    serializedInfo.blobsParent().SwapElements(blobs);
    serializedInfo.fileInfos().SwapElements(fileInfos);
  }
}

IndexGetKeyRequestOp::IndexGetKeyRequestOp(TransactionBase* aTransaction,
                                           const RequestParams& aParams,
                                           bool aGetAll)
  : IndexRequestOpBase(aTransaction, aParams)
  , mOptionalKeyRange(aGetAll ?
                        aParams.get_IndexGetAllKeysParams().optionalKeyRange() :
                        OptionalKeyRange(aParams.get_IndexGetKeyParams()
                                                .keyRange()))
  , mLimit(aGetAll ? aParams.get_IndexGetAllKeysParams().limit() : 1)
  , mGetAll(aGetAll)
{
  MOZ_ASSERT(aParams.type() == RequestParams::TIndexGetKeyParams ||
             aParams.type() == RequestParams::TIndexGetAllKeysParams);
  MOZ_ASSERT_IF(!aGetAll,
                mOptionalKeyRange.type() ==
                  OptionalKeyRange::TSerializedKeyRange);
}

nsresult
IndexGetKeyRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT_IF(!mGetAll,
                mOptionalKeyRange.type() ==
                  OptionalKeyRange::TSerializedKeyRange);
  MOZ_ASSERT_IF(!mGetAll, mLimit == 1);

  PROFILER_LABEL("IndexedDB",
                 "IndexGetKeyRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool hasKeyRange =
    mOptionalKeyRange.type() == OptionalKeyRange::TSerializedKeyRange;

  nsCString indexTable;
  if (mMetadata->mCommonMetadata.unique()) {
    indexTable.AssignLiteral("unique_index_data ");
  }
  else {
    indexTable.AssignLiteral("index_data ");
  }

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.get_SerializedKeyRange(),
                                NS_LITERAL_CSTRING("value"),
                                keyRangeClause);
  }

  nsCString limitClause;
  if (mLimit) {
    limitClause.AssignLiteral(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query =
    NS_LITERAL_CSTRING("SELECT object_data_key "
                       "FROM ") +
    indexTable +
    NS_LITERAL_CSTRING("WHERE index_id = :index_id") +
    keyRangeClause +
    limitClause;

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                             mMetadata->mCommonMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.get_SerializedKeyRange(),
                                 stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    Key* key = mResponse.AppendElement();
    if (NS_WARN_IF(!key)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = key->SetFromStatement(stmt, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  return NS_OK;
}

void
IndexGetKeyRequestOp::GetResponse(RequestResponse& aResponse)
{
  MOZ_ASSERT_IF(!mGetAll, mResponse.Length() <= 1);

  if (mGetAll) {
    aResponse = IndexGetAllKeysResponse();

    if (!mResponse.IsEmpty()) {
      mResponse.SwapElements(aResponse.get_IndexGetAllKeysResponse().keys());
    }

    return;
  }

  aResponse = IndexGetKeyResponse();

  if (!mResponse.IsEmpty()) {
    aResponse.get_IndexGetKeyResponse().key() = Move(mResponse[0]);
  }
}

nsresult
IndexCountRequestOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();

  PROFILER_LABEL("IndexedDB",
                 "IndexCountRequestOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool hasKeyRange =
    mParams.optionalKeyRange().type() == OptionalKeyRange::TSerializedKeyRange;

  nsCString indexTable;
  if (mMetadata->mCommonMetadata.unique()) {
    indexTable.AssignLiteral("unique_index_data ");
  }
  else {
    indexTable.AssignLiteral("index_data ");
  }

  nsAutoCString keyRangeClause;
  if (hasKeyRange) {
    GetBindingClauseForKeyRange(
      mParams.optionalKeyRange().get_SerializedKeyRange(),
      NS_LITERAL_CSTRING("value"),
      keyRangeClause);
  }

  nsCString query =
    NS_LITERAL_CSTRING("SELECT count(*) "
                       "FROM ") +
    indexTable +
    NS_LITERAL_CSTRING("WHERE index_id = :index_id") +
    keyRangeClause;

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                             mMetadata->mCommonMetadata.id());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasKeyRange) {
    rv = BindKeyRangeToStatement(
      mParams.optionalKeyRange().get_SerializedKeyRange(),
      stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  int64_t count = stmt->AsInt64(0);
  if (NS_WARN_IF(count < 0)) {
    MOZ_ASSERT(false, "This should never be possible!");
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  mResponse.count() = count;

  return NS_OK;
}

bool
Cursor::
CursorOpBase::SendFailureResult(nsresult aResultCode)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mCurrentlyRunningOp == this);
  MOZ_ASSERT(!mResponseSent);

  if (!IsActorDestroyed()) {
    mResponse = ClampResultCode(aResultCode);

    mCursor->SendResponseInternal(mResponse, mFiles);
  }

  mResponseSent = true;
  return false;
}

void
Cursor::
CursorOpBase::Cleanup()
{
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

void
Cursor::
OpenOp::GetRangeKeyInfo(bool aLowerBound, Key* aKey, bool* aOpen)
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(aKey);
  MOZ_ASSERT(aKey->IsUnset());
  MOZ_ASSERT(aOpen);
  if (mOptionalKeyRange.type() == OptionalKeyRange::TSerializedKeyRange) {
    const SerializedKeyRange& range =
      mOptionalKeyRange.get_SerializedKeyRange();
    if (range.isOnly()) {
      *aKey = range.lower();
      *aOpen = false;
    } else {
      *aKey = aLowerBound ? range.lower() : range.upper();
      *aOpen = aLowerBound ? range.lowerOpen() : range.upperOpen();
    }
  } else {
    *aOpen = false;
  }
}

nsresult
Cursor::
OpenOp::DoObjectStoreDatabaseWork(TransactionBase* aTransaction)
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mType == OpenCursorParams::TObjectStoreOpenCursorParams);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(mCursor->mFileManager);

  PROFILER_LABEL("IndexedDB",
                 "Cursor::OpenOp::DoObjectStoreDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool usingKeyRange =
    mOptionalKeyRange.type() == OptionalKeyRange::TSerializedKeyRange;

  NS_NAMED_LITERAL_CSTRING(keyValue, "key_value");
  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(openLimit, " LIMIT ");

  nsCString queryStart =
    NS_LITERAL_CSTRING("SELECT ") +
    keyValue +
    NS_LITERAL_CSTRING(", data, file_ids "
                       "FROM object_data "
                       "WHERE object_store_id = :") +
    id;

  nsAutoCString keyRangeClause;
  if (usingKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.get_SerializedKeyRange(),
                                keyValue,
                                keyRangeClause);
  }

  nsAutoCString directionClause = NS_LITERAL_CSTRING(" ORDER BY ") + keyValue;
  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AppendLiteral(" ASC");
      break;

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE:
      directionClause.AppendLiteral(" DESC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  nsCString firstQuery =
    queryStart +
    keyRangeClause +
    directionClause +
    openLimit +
    NS_LITERAL_CSTRING("1");

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(firstQuery, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(id, mCursor->mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (usingKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.get_SerializedKeyRange(),
                                 stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    mResponse = void_t();
    return NS_OK;
  }

  rv = mCursor->mKey.SetFromStatement(stmt, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  StructuredCloneReadInfo cloneInfo;
  rv = GetStructuredCloneReadInfoFromStatement(stmt,
                                               1,
                                               2,
                                               mCursor->mFileManager,
                                               &cloneInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now we need to make the query to get the next match.
  keyRangeClause.Truncate();
  nsAutoCString continueToKeyRangeClause;

  NS_NAMED_LITERAL_CSTRING(currentKey, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      AppendConditionClause(keyValue, currentKey, false, false,
                            keyRangeClause);
      AppendConditionClause(keyValue, currentKey, false, true,
                            continueToKeyRangeClause);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(keyValue, rangeKey, true, !open, keyRangeClause);
        AppendConditionClause(keyValue, rangeKey, true, !open,
                              continueToKeyRangeClause);
        mCursor->mRangeKey = upper;
      }
      break;
    }

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      AppendConditionClause(keyValue, currentKey, true, false, keyRangeClause);
      AppendConditionClause(keyValue, currentKey, true, true,
                           continueToKeyRangeClause);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(keyValue, rangeKey, false, !open, keyRangeClause);
        AppendConditionClause(keyValue, rangeKey, false, !open,
                              continueToKeyRangeClause);
        mCursor->mRangeKey = lower;
      }
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  mCursor->mContinueQuery =
    queryStart +
    keyRangeClause +
    directionClause +
    openLimit;

  mCursor->mContinueToQuery =
    queryStart +
    continueToKeyRangeClause +
    directionClause +
    openLimit;

  mResponse = ObjectStoreCursorResponse();

  auto& response = mResponse.get_ObjectStoreCursorResponse();
  response.cloneInfo().data().SwapElements(cloneInfo.mData);
  response.key() = mCursor->mKey;

  mFiles.SwapElements(cloneInfo.mFiles);

  return NS_OK;
}

nsresult
Cursor::
OpenOp::DoObjectStoreKeyDatabaseWork(TransactionBase* aTransaction)
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mType ==
               OpenCursorParams::TObjectStoreOpenKeyCursorParams);
  MOZ_ASSERT(mCursor->mObjectStoreId);

  PROFILER_LABEL("IndexedDB",
                 "Cursor::OpenOp::DoObjectStoreKeyDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool usingKeyRange =
    mOptionalKeyRange.type() == OptionalKeyRange::TSerializedKeyRange;

  NS_NAMED_LITERAL_CSTRING(keyValue, "key_value");
  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(openLimit, " LIMIT ");

  nsCString queryStart =
    NS_LITERAL_CSTRING("SELECT ") +
    keyValue +
    NS_LITERAL_CSTRING(" FROM object_data "
                       "WHERE object_store_id = :") +
    id;

  nsAutoCString keyRangeClause;
  if (usingKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.get_SerializedKeyRange(),
                                keyValue,
                                keyRangeClause);
  }

  nsAutoCString directionClause = NS_LITERAL_CSTRING(" ORDER BY ") + keyValue;
  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AppendLiteral(" ASC");
      break;

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE:
      directionClause.AppendLiteral(" DESC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  nsCString firstQuery =
    queryStart +
    keyRangeClause +
    directionClause +
    openLimit +
    NS_LITERAL_CSTRING("1");

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(firstQuery, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(id, mCursor->mObjectStoreId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (usingKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.get_SerializedKeyRange(),
                                 stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    mResponse = void_t();
    return NS_OK;
  }

  rv = mCursor->mKey.SetFromStatement(stmt, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now we need to make the query to get the next match.
  keyRangeClause.Truncate();
  nsAutoCString continueToKeyRangeClause;

  NS_NAMED_LITERAL_CSTRING(currentKey, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      AppendConditionClause(keyValue, currentKey, false, false,
                            keyRangeClause);
      AppendConditionClause(keyValue, currentKey, false, true,
                            continueToKeyRangeClause);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(keyValue, rangeKey, true, !open, keyRangeClause);
        AppendConditionClause(keyValue, rangeKey, true, !open,
                              continueToKeyRangeClause);
        mCursor->mRangeKey = upper;
      }
      break;
    }

    case IDBCursor::PREV:
    case IDBCursor::PREV_UNIQUE: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      AppendConditionClause(keyValue, currentKey, true, false, keyRangeClause);
      AppendConditionClause(keyValue, currentKey, true, true,
                            continueToKeyRangeClause);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(keyValue, rangeKey, false, !open, keyRangeClause);
        AppendConditionClause(keyValue, rangeKey, false, !open,
                              continueToKeyRangeClause);
        mCursor->mRangeKey = lower;
      }
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  mCursor->mContinueQuery =
    queryStart +
    keyRangeClause +
    directionClause +
    openLimit;
  mCursor->mContinueToQuery =
    queryStart +
    continueToKeyRangeClause +
    directionClause +
    openLimit;

  mResponse = ObjectStoreKeyCursorResponse(mCursor->mKey);

  return NS_OK;
}

nsresult
Cursor::
OpenOp::DoIndexDatabaseWork(TransactionBase* aTransaction)
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mType == OpenCursorParams::TIndexOpenCursorParams);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(mCursor->mIndexId);

  PROFILER_LABEL("IndexedDB",
                 "Cursor::OpenOp::DoIndexDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool usingKeyRange =
    mOptionalKeyRange.type() == OptionalKeyRange::TSerializedKeyRange;

  nsCString indexTable = mCursor->mUniqueIndex ?
    NS_LITERAL_CSTRING("unique_index_data") :
    NS_LITERAL_CSTRING("index_data");

  NS_NAMED_LITERAL_CSTRING(value, "index_table.value");
  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(openLimit, " LIMIT ");

  nsAutoCString keyRangeClause;
  if (usingKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.get_SerializedKeyRange(),
                                value,
                                keyRangeClause);
  }

  nsAutoCString directionClause =
    NS_LITERAL_CSTRING(" ORDER BY ") +
    value;

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AppendLiteral(" ASC, index_table.object_data_key ASC");
      break;

    case IDBCursor::PREV:
      directionClause.AppendLiteral(" DESC, index_table.object_data_key DESC");
      break;

    case IDBCursor::PREV_UNIQUE:
      directionClause.AppendLiteral(" DESC, index_table.object_data_key ASC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  nsAutoCString queryStart =
    NS_LITERAL_CSTRING("SELECT index_table.value, "
                              "index_table.object_data_key, "
                              "object_data.data, "
                              "object_data.file_ids "
                       "FROM ") +
    indexTable +
    NS_LITERAL_CSTRING(" AS index_table "
                       "JOIN object_data "
                       "ON index_table.object_data_id = object_data.id "
                       "WHERE index_table.index_id = :") +
    id;

  nsCString firstQuery =
    queryStart +
    keyRangeClause +
    directionClause +
    openLimit +
    NS_LITERAL_CSTRING("1");

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(firstQuery, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(id, mCursor->mIndexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (usingKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.get_SerializedKeyRange(),
                                 stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    mResponse = void_t();
    return NS_OK;
  }

  rv = mCursor->mKey.SetFromStatement(stmt, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mCursor->mObjectKey.SetFromStatement(stmt, 1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  StructuredCloneReadInfo cloneInfo;
  rv = GetStructuredCloneReadInfoFromStatement(stmt,
                                               2,
                                               3,
                                               mCursor->mFileManager,
                                               &cloneInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now we need to make the query to get the next match.
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !open, queryStart);
        mCursor->mRangeKey = upper;
      }
      mCursor->mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value >= :current_key "
                            "AND ( index_table.value > :current_key OR "
                                  "index_table.object_data_key > :object_key ) "
                          ) +
        directionClause +
        openLimit;
      mCursor->mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value >= :current_key") +
        directionClause +
        openLimit;
      break;
    }

    case IDBCursor::NEXT_UNIQUE: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !open, queryStart);
        mCursor->mRangeKey = upper;
      }
      mCursor->mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value > :current_key") +
        directionClause +
        openLimit;
      mCursor->mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value >= :current_key") +
        directionClause +
        openLimit;
      break;
    }

    case IDBCursor::PREV: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !open, queryStart);
        mCursor->mRangeKey = lower;
      }
      mCursor->mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value <= :current_key "
                            "AND ( index_table.value < :current_key OR "
                                  "index_table.object_data_key < :object_key ) "
                          ) +
        directionClause +
        openLimit;
      mCursor->mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value <= :current_key") +
        directionClause +
        openLimit;
      break;
    }

    case IDBCursor::PREV_UNIQUE: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !open, queryStart);
        mCursor->mRangeKey = lower;
      }
      mCursor->mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value < :current_key") +
        directionClause +
        openLimit;
      mCursor->mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND index_table.value <= :current_key") +
        directionClause +
        openLimit;
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  mResponse = IndexCursorResponse();

  auto& response = mResponse.get_IndexCursorResponse();
  response.cloneInfo().data().SwapElements(cloneInfo.mData);
  response.key() = mCursor->mKey;
  response.objectKey() = mCursor->mObjectKey;

  mFiles.SwapElements(cloneInfo.mFiles);

  return NS_OK;
}

nsresult
Cursor::
OpenOp::DoIndexKeyDatabaseWork(TransactionBase* aTransaction)
{
  AssertIsOnTransactionThread();
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mType == OpenCursorParams::TIndexOpenKeyCursorParams);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(mCursor->mIndexId);

  PROFILER_LABEL("IndexedDB",
                 "Cursor::OpenOp::DoIndexKeyDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  const bool usingKeyRange =
    mOptionalKeyRange.type() == OptionalKeyRange::TSerializedKeyRange;

  nsCString table = mCursor->mUniqueIndex ?
    NS_LITERAL_CSTRING("unique_index_data") :
    NS_LITERAL_CSTRING("index_data");

  NS_NAMED_LITERAL_CSTRING(value, "value");
  NS_NAMED_LITERAL_CSTRING(id, "id");
  NS_NAMED_LITERAL_CSTRING(openLimit, " LIMIT ");

  nsAutoCString keyRangeClause;
  if (usingKeyRange) {
    GetBindingClauseForKeyRange(mOptionalKeyRange.get_SerializedKeyRange(),
                                value,
                                keyRangeClause);
  }

  nsAutoCString directionClause =
    NS_LITERAL_CSTRING(" ORDER BY ") +
    value;

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT:
    case IDBCursor::NEXT_UNIQUE:
      directionClause.AppendLiteral(" ASC, object_data_key ASC");
      break;

    case IDBCursor::PREV:
      directionClause.AppendLiteral(" DESC, object_data_key DESC");
      break;

    case IDBCursor::PREV_UNIQUE:
      directionClause.AppendLiteral(" DESC, object_data_key ASC");
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  nsAutoCString queryStart =
    NS_LITERAL_CSTRING("SELECT value, object_data_key "
                       "FROM ") +
    table +
    NS_LITERAL_CSTRING(" WHERE index_id = :") +
    id;

  nsCString firstQuery =
    queryStart +
    keyRangeClause +
    directionClause +
    openLimit +
    NS_LITERAL_CSTRING("1");

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(firstQuery, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(id, mCursor->mIndexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (usingKeyRange) {
    rv = BindKeyRangeToStatement(mOptionalKeyRange.get_SerializedKeyRange(),
                                 stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!hasResult) {
    mResponse = void_t();
    return NS_OK;
  }

  rv = mCursor->mKey.SetFromStatement(stmt, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mCursor->mObjectKey.SetFromStatement(stmt, 1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now we need to make the query to get the next match.
  NS_NAMED_LITERAL_CSTRING(rangeKey, "range_key");

  switch (mCursor->mDirection) {
    case IDBCursor::NEXT: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !open, queryStart);
        mCursor->mRangeKey = upper;
      }
      mCursor->mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value >= :current_key "
                            "AND ( value > :current_key OR "
                                  "object_data_key > :object_key )") +
        directionClause +
        openLimit;
      mCursor->mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value >= :current_key ") +
        directionClause +
        openLimit;
      break;
    }

    case IDBCursor::NEXT_UNIQUE: {
      Key upper;
      bool open;
      GetRangeKeyInfo(false, &upper, &open);
      if (usingKeyRange && !upper.IsUnset()) {
        AppendConditionClause(value, rangeKey, true, !open, queryStart);
        mCursor->mRangeKey = upper;
      }
      mCursor->mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value > :current_key") +
        directionClause +
        openLimit;
      mCursor->mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value >= :current_key") +
        directionClause +
        openLimit;
      break;
    }

    case IDBCursor::PREV: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !open, queryStart);
        mCursor->mRangeKey = lower;
      }

      mCursor->mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value <= :current_key "
                            "AND ( value < :current_key OR "
                                  "object_data_key < :object_key )") +
        directionClause +
        openLimit;
      mCursor->mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value <= :current_key ") +
        directionClause +
        openLimit;
      break;
    }

    case IDBCursor::PREV_UNIQUE: {
      Key lower;
      bool open;
      GetRangeKeyInfo(true, &lower, &open);
      if (usingKeyRange && !lower.IsUnset()) {
        AppendConditionClause(value, rangeKey, false, !open, queryStart);
        mCursor->mRangeKey = lower;
      }
      mCursor->mContinueQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value < :current_key") +
        directionClause +
        openLimit;
      mCursor->mContinueToQuery =
        queryStart +
        NS_LITERAL_CSTRING(" AND value <= :current_key") +
        directionClause +
        openLimit;
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  mResponse = IndexKeyCursorResponse();

  auto& response = mResponse.get_IndexKeyCursorResponse();
  response.key() = mCursor->mKey;
  response.objectKey() = mCursor->mObjectKey;

  return NS_OK;
}

nsresult
Cursor::
OpenOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mContinueQuery.IsEmpty());
  MOZ_ASSERT(mCursor->mContinueToQuery.IsEmpty());
  MOZ_ASSERT(mCursor->mKey.IsUnset());
  MOZ_ASSERT(mCursor->mRangeKey.IsUnset());

  PROFILER_LABEL("IndexedDB",
                 "Cursor::OpenOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  nsresult rv;

  switch (mCursor->mType) {
    case OpenCursorParams::TObjectStoreOpenCursorParams:
      rv = DoObjectStoreDatabaseWork(aTransaction);
      break;

    case OpenCursorParams::TObjectStoreOpenKeyCursorParams:
      rv = DoObjectStoreKeyDatabaseWork(aTransaction);
      break;

    case OpenCursorParams::TIndexOpenCursorParams:
      rv = DoIndexDatabaseWork(aTransaction);
      break;

    case OpenCursorParams::TIndexOpenKeyCursorParams:
      rv = DoIndexKeyDatabaseWork(aTransaction);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
Cursor::
OpenOp::SendSuccessResult()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mCurrentlyRunningOp == this);
  MOZ_ASSERT(mResponse.type() != CursorResponse::T__None);
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mRangeKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mObjectKey.IsUnset());

  if (IsActorDestroyed()) {
    return NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  mCursor->SendResponseInternal(mResponse, mFiles);

  mResponseSent = true;
  return NS_OK;
}

nsresult
Cursor::
ContinueOp::DoDatabaseWork(TransactionBase* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnTransactionThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mObjectStoreId);
  MOZ_ASSERT(!mCursor->mContinueQuery.IsEmpty());
  MOZ_ASSERT(!mCursor->mContinueToQuery.IsEmpty());
  MOZ_ASSERT(!mCursor->mKey.IsUnset());

  const bool isIndex =
    mCursor->mType == OpenCursorParams::TIndexOpenCursorParams ||
    mCursor->mType == OpenCursorParams::TIndexOpenKeyCursorParams;

  MOZ_ASSERT_IF(isIndex, mCursor->mIndexId);
  MOZ_ASSERT_IF(isIndex, !mCursor->mObjectKey.IsUnset());

  PROFILER_LABEL("IndexedDB",
                 "Cursor::ContinueOp::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  // We need to pick a query based on whether or not a key was passed to the
  // continue function. If not we'll grab the the next item in the database that
  // is greater than (or less than, if we're running a PREV cursor) the current
  // key. If a key was passed we'll grab the next item in the database that is
  // greater than (or less than, if we're running a PREV cursor) or equal to the
  // key that was specified.

  nsAutoCString countString;
  nsCString query;

  bool hasContinueKey = false;
  uint32_t advanceCount;

  if (mParams.type() == CursorRequestParams::TContinueParams) {
    // Always go to the next result.
    advanceCount = 1;
    countString.AppendLiteral("1");

    if (mParams.get_ContinueParams().key().IsUnset()) {
      query = mCursor->mContinueQuery + countString;
      hasContinueKey = false;
    } else {
      query = mCursor->mContinueToQuery + countString;
      hasContinueKey = true;
    }
  } else {
    advanceCount = mParams.get_AdvanceParams().count();
    countString.AppendInt(advanceCount);

    query = mCursor->mContinueQuery + countString;
    hasContinueKey = false;
  }

  NS_NAMED_LITERAL_CSTRING(currentKeyName, "current_key");
  NS_NAMED_LITERAL_CSTRING(rangeKeyName, "range_key");
  NS_NAMED_LITERAL_CSTRING(objectKeyName, "object_key");

  const Key& currentKey =
    hasContinueKey ? mParams.get_ContinueParams().key() : mCursor->mKey;

  const bool usingRangeKey = !mCursor->mRangeKey.IsUnset();

  TransactionBase::CachedStatement stmt;
  nsresult rv = aTransaction->GetCachedStatement(query, &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t id = isIndex ? mCursor->mIndexId : mCursor->mObjectStoreId;

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), id);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Bind current key.
  rv = currentKey.BindToStatement(stmt, currentKeyName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Bind range key if it is specified.
  if (usingRangeKey) {
    rv = mCursor->mRangeKey.BindToStatement(stmt, rangeKeyName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Bind object key if duplicates are allowed and we're not continuing to a
  // specific key.
  if (isIndex &&
      !hasContinueKey &&
      (mCursor->mDirection == IDBCursor::NEXT ||
       mCursor->mDirection == IDBCursor::PREV)) {
    rv = mCursor->mObjectKey.BindToStatement(stmt, objectKeyName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool hasResult;
  for (uint32_t index = 0; index < advanceCount; index++) {
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!hasResult) {
      break;
    }
  }

  if (!hasResult) {
    mCursor->mKey.Unset();
    mCursor->mRangeKey.Unset();
    mCursor->mObjectKey.Unset();
    mResponse = void_t();
    return NS_OK;
  }

  switch (mCursor->mType) {
    case OpenCursorParams::TObjectStoreOpenCursorParams: {
      rv = mCursor->mKey.SetFromStatement(stmt, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      StructuredCloneReadInfo cloneInfo;
      rv = GetStructuredCloneReadInfoFromStatement(stmt,
                                                   1,
                                                   2,
                                                   mCursor->mFileManager,
                                                   &cloneInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mResponse = ObjectStoreCursorResponse();

      auto& response = mResponse.get_ObjectStoreCursorResponse();
      response.cloneInfo().data().SwapElements(cloneInfo.mData);
      response.key() = mCursor->mKey;

      mFiles.SwapElements(cloneInfo.mFiles);

      break;
    }

    case OpenCursorParams::TObjectStoreOpenKeyCursorParams: {
      rv = mCursor->mKey.SetFromStatement(stmt, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mResponse = ObjectStoreKeyCursorResponse(mCursor->mKey);

      break;
    }

    case OpenCursorParams::TIndexOpenCursorParams: {
      rv = mCursor->mKey.SetFromStatement(stmt, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = mCursor->mObjectKey.SetFromStatement(stmt, 1);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      StructuredCloneReadInfo cloneInfo;
      rv = GetStructuredCloneReadInfoFromStatement(stmt,
                                                   2,
                                                   3,
                                                   mCursor->mFileManager,
                                                   &cloneInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mResponse = IndexCursorResponse();

      auto& response = mResponse.get_IndexCursorResponse();
      response.cloneInfo().data().SwapElements(cloneInfo.mData);
      response.key() = mCursor->mKey;
      response.objectKey() = mCursor->mObjectKey;

      mFiles.SwapElements(cloneInfo.mFiles);

      break;
    }

    case OpenCursorParams::TIndexOpenKeyCursorParams: {
      rv = mCursor->mKey.SetFromStatement(stmt, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = mCursor->mObjectKey.SetFromStatement(stmt, 1);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mResponse = IndexKeyCursorResponse(mCursor->mKey, mCursor->mObjectKey);

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

nsresult
Cursor::
ContinueOp::SendSuccessResult()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCursor);
  MOZ_ASSERT(mCursor->mCurrentlyRunningOp == this);
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mRangeKey.IsUnset());
  MOZ_ASSERT_IF(mResponse.type() == CursorResponse::Tvoid_t,
                mCursor->mObjectKey.IsUnset());

  if (IsActorDestroyed()) {
    return NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  }

  mCursor->SendResponseInternal(mResponse, mFiles);

  mResponseSent = true;
  return NS_OK;
}

void
PermissionRequestHelper::OnPromptComplete(PermissionValue aPermissionValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mActorDestroyed) {
    unused <<
      PIndexedDBPermissionRequestParent::Send__delete__(this, aPermissionValue);
  }
}

void
PermissionRequestHelper::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;
}

#ifdef DEBUG

NS_IMPL_ISUPPORTS(DEBUGThreadSlower, nsIThreadObserver)

NS_IMETHODIMP
DEBUGThreadSlower::OnDispatchedEvent(nsIThreadInternal* /* aThread */)
{
  MOZ_CRASH("Should never be called!");
}

NS_IMETHODIMP
DEBUGThreadSlower::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                      bool /* aMayWait */,
                                      uint32_t /* aRecursionDepth */)
{
  return NS_OK;
}

NS_IMETHODIMP
DEBUGThreadSlower::AfterProcessNextEvent(nsIThreadInternal* /* aThread */,
                                         uint32_t /* aRecursionDepth */,
                                         bool /* aEventWasProcessed */)
{
  MOZ_ASSERT(kDEBUGThreadSleepMS);

  MOZ_ALWAYS_TRUE(PR_Sleep(PR_MillisecondsToInterval(kDEBUGThreadSleepMS)) ==
                    PR_SUCCESS);
  return NS_OK;
}

#endif // DEBUG

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
