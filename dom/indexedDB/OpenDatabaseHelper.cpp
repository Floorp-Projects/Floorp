/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "OpenDatabaseHelper.h"

#include "nsIBFCacheEntry.h"
#include "nsIFile.h"

#include <algorithm>
#include "mozilla/dom/quota/AcquireListener.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/storage.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "snappy/snappy.h"

#include "Client.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IndexedDatabaseManager.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

using namespace mozilla;
using namespace mozilla::dom;
USING_INDEXEDDB_NAMESPACE
USING_QUOTA_NAMESPACE

namespace {

// If JS_STRUCTURED_CLONE_VERSION changes then we need to update our major
// schema version.
static_assert(JS_STRUCTURED_CLONE_VERSION == 3,
              "Need to update the major schema version.");

// Major schema version. Bump for almost everything.
const uint32_t kMajorSchemaVersion = 15;

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

inline
int32_t
MakeSchemaVersion(uint32_t aMajorSchemaVersion,
                  uint32_t aMinorSchemaVersion)
{
  return int32_t((aMajorSchemaVersion << 4) + aMinorSchemaVersion);
}

const int32_t kSQLiteSchemaVersion = int32_t((kMajorSchemaVersion << 4) +
                                             kMinorSchemaVersion);

const uint32_t kGoldenRatioU32 = 0x9E3779B9U;

inline
uint32_t
RotateBitsLeft32(uint32_t value, uint8_t bits)
{
  MOZ_ASSERT(bits < 32);
  return (value << bits) | (value >> (32 - bits));
}

inline
uint32_t
HashName(const nsAString& aName)
{
  const char16_t* str = aName.BeginReading();
  size_t length = aName.Length();

  uint32_t hash = 0;
  for (size_t i = 0; i < length; i++) {
    hash = kGoldenRatioU32 * (RotateBitsLeft32(hash, 5) ^ str[i]);
  }

  return hash;
}

nsresult
GetDatabaseFilename(const nsAString& aName,
                    nsAString& aDatabaseFilename)
{
  aDatabaseFilename.AppendInt(HashName(aName));

  nsCString escapedName;
  if (!NS_Escape(NS_ConvertUTF16toUTF8(aName), escapedName, url_XPAlphas)) {
    NS_WARNING("Can't escape database name!");
    return NS_ERROR_UNEXPECTED;
  }

  const char* forwardIter = escapedName.BeginReading();
  const char* backwardIter = escapedName.EndReading() - 1;

  nsCString substring;
  while (forwardIter <= backwardIter && substring.Length() < 21) {
    if (substring.Length() % 2) {
      substring.Append(*backwardIter--);
    }
    else {
      substring.Append(*forwardIter++);
    }
  }

  aDatabaseFilename.Append(NS_ConvertASCIItoUTF16(substring));

  return NS_OK;
}

nsresult
CreateFileTables(mozIStorageConnection* aDBConn)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "CreateFileTables",
    js::ProfileEntry::Category::STORAGE);

  // Table `file`
  nsresult rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE file ("
      "id INTEGER PRIMARY KEY, "
      "refcount INTEGER NOT NULL"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_insert_trigger "
    "AFTER INSERT ON object_data "
    "FOR EACH ROW "
    "WHEN NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
    "END;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_update_trigger "
    "AFTER UPDATE OF file_ids ON object_data "
    "FOR EACH ROW "
    "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
    "END;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_delete_trigger "
    "AFTER DELETE ON object_data "
    "FOR EACH ROW WHEN OLD.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NULL); "
    "END;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER file_update_trigger "
    "AFTER UPDATE ON file "
    "FOR EACH ROW WHEN NEW.refcount = 0 "
    "BEGIN "
      "DELETE FROM file WHERE id = OLD.id; "
    "END;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CreateTables(mozIStorageConnection* aDBConn)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(aDBConn, "Passing a null database connection!");

  PROFILER_LABEL("OpenDatabaseHelper", "CreateTables",
    js::ProfileEntry::Category::STORAGE);

  // Table `database`
  nsresult rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE database ("
      "name TEXT NOT NULL, "
      "version INTEGER NOT NULL DEFAULT 0"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `object_store`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store ("
      "id INTEGER PRIMARY KEY, "
      "auto_increment INTEGER NOT NULL DEFAULT 0, "
      "name TEXT NOT NULL, "
      "key_path TEXT, "
      "UNIQUE (name)"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `object_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `index`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `index_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
  NS_ENSURE_SUCCESS(rv, rv);

  // Need this to make cascading deletes from object_data and object_store fast.
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX index_data_object_data_id_index "
    "ON index_data (object_data_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table `unique_index_data`
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
  NS_ENSURE_SUCCESS(rv, rv);

  // Need this to make cascading deletes from object_data and object_store fast.
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX unique_index_data_object_data_id_index "
    "ON unique_index_data (object_data_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CreateFileTables(aDBConn);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->SetSchemaVersion(kSQLiteSchemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom4To5(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom4To5",
    js::ProfileEntry::Category::STORAGE);

  nsresult rv;

  // All we changed is the type of the version column, so lets try to
  // convert that to an integer, and if we fail, set it to 0.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, version, dataVersion "
    "FROM database"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString name;
  int32_t intVersion;
  int64_t dataVersion;

  {
    mozStorageStatementScoper scoper(stmt);

    bool hasResults;
    rv = stmt->ExecuteStep(&hasResults);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(hasResults, NS_ERROR_FAILURE);

    nsString version;
    rv = stmt->GetString(1, version);
    NS_ENSURE_SUCCESS(rv, rv);

    intVersion = version.ToInteger(&rv);
    if (NS_FAILED(rv)) {
      intVersion = 0;
    }

    rv = stmt->GetString(0, name);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->GetInt64(2, &dataVersion);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE database"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE database ("
      "name TEXT NOT NULL, "
      "version INTEGER NOT NULL DEFAULT 0, "
      "dataVersion INTEGER NOT NULL"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO database (name, version, dataVersion) "
    "VALUES (:name, :version, :dataVersion)"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  {
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), name);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("version"), intVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("dataVersion"), dataVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aConnection->SetSchemaVersion(5);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom5To6(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom5To6",
    js::ProfileEntry::Category::STORAGE);

  // First, drop all the indexes we're no longer going to use.
  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX key_index;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX ai_key_index;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX value_index;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX ai_value_index;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, key_value, data "
      "FROM object_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_data "
      "SELECT id, object_store_id, key_value, data "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to add a unique constraint to our ai_object_data table. Copy all
  // the data out of it using a temporary table as before.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id, "
      "data "
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, data "
      "FROM ai_object_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_object_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO ai_object_data "
      "SELECT id, object_store_id, data "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX index_data_object_data_id_index "
    "ON index_data (object_data_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM unique_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE unique_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO unique_index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX unique_index_data_object_data_id_index "
    "ON unique_index_data (object_data_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Fix up the ai_index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "ai_object_data_id "
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, value, ai_object_data_id "
      "FROM ai_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO ai_index_data "
      "SELECT index_id, value, ai_object_data_id "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX ai_index_data_ai_object_data_id_index "
    "ON ai_index_data (ai_object_data_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Fix up the ai_unique_index_data table. We're reordering the columns as well
  // as changing the primary key from being a simple id to being a composite.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "ai_object_data_id "
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, value, ai_object_data_id "
      "FROM ai_unique_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_unique_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO ai_unique_index_data "
      "SELECT index_id, value, ai_object_data_id "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX ai_unique_index_data_ai_object_data_id_index "
    "ON ai_unique_index_data (ai_object_data_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->SetSchemaVersion(6);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom6To7(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom6To7",
    js::ProfileEntry::Category::STORAGE);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id, "
      "name, "
      "key_path, "
      "auto_increment"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, name, key_path, auto_increment "
      "FROM object_store;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_store;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE object_store ("
      "id INTEGER PRIMARY KEY, "
      "auto_increment INTEGER NOT NULL DEFAULT 0, "
      "name TEXT NOT NULL, "
      "key_path TEXT, "
      "UNIQUE (name)"
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_store "
      "SELECT id, auto_increment, name, nullif(key_path, '') "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->SetSchemaVersion(7);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom7To8(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom7To8",
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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, object_store_autoincrement "
      "FROM object_store_index;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_store_index;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_store_index "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, 0, object_store_autoincrement "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->SetSchemaVersion(8);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

class CompressDataBlobsFunction MOZ_FINAL : public mozIStorageFunction
{
  ~CompressDataBlobsFunction() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult)
  {
    PROFILER_LABEL("CompressDataBlobsFunction", "OnFunctionCall",
      js::ProfileEntry::Category::STORAGE);

    uint32_t argc;
    nsresult rv = aArguments->GetNumEntries(&argc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (argc != 1) {
      NS_WARNING("Don't call me with the wrong number of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    int32_t type;
    rv = aArguments->GetTypeOfIndex(0, &type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (type != mozIStorageStatement::VALUE_TYPE_BLOB) {
      NS_WARNING("Don't call me with the wrong type of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    const uint8_t* uncompressed;
    uint32_t uncompressedLength;
    rv = aArguments->GetSharedBlob(0, &uncompressedLength, &uncompressed);
    NS_ENSURE_SUCCESS(rv, rv);

    size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);
    char* compressed = (char*)moz_malloc(compressedLength);
    NS_ENSURE_TRUE(compressed, NS_ERROR_OUT_OF_MEMORY);

    snappy::RawCompress(reinterpret_cast<const char*>(uncompressed),
                        uncompressedLength, compressed, &compressedLength);

    std::pair<uint8_t *, int> data((uint8_t*)compressed,
                                   int(compressedLength));
    // The variant takes ownership of | compressed |.
    nsCOMPtr<nsIVariant> result = new mozilla::storage::AdoptedBlobVariant(data);

    result.forget(aResult);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(CompressDataBlobsFunction, mozIStorageFunction)

nsresult
UpgradeSchemaFrom8To9_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom8To9_0",
    js::ProfileEntry::Category::STORAGE);

  // We no longer use the dataVersion column.
  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE database SET dataVersion = 0;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageFunction> compressor = new CompressDataBlobsFunction();

  NS_NAMED_LITERAL_CSTRING(compressorName, "compress");

  rv = aConnection->CreateFunction(compressorName, 1, compressor);
  NS_ENSURE_SUCCESS(rv, rv);

  // Turn off foreign key constraints before we do anything here.
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE object_data SET data = compress(data);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE ai_object_data SET data = compress(data);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->RemoveFunction(compressorName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(9, 0));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom9_0To10_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom9_0To10_0",
    js::ProfileEntry::Category::STORAGE);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE object_data ADD COLUMN file_ids TEXT;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE ai_object_data ADD COLUMN file_ids TEXT;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CreateFileTables(aConnection);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(10, 0));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom10_0To11_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom10_0To11_0",
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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, multientry "
      "FROM object_store_index;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_store_index;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_store_index "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, multientry "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TRIGGER object_data_insert_trigger;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_data (object_store_id, key_value, data, file_ids) "
      "SELECT object_store_id, id, data, file_ids "
      "FROM ai_object_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_insert_trigger "
    "AFTER INSERT ON object_data "
    "FOR EACH ROW "
    "WHEN NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
    "END;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE object_store "
      "SET auto_increment = (SELECT max(id) FROM ai_object_data) + 1 "
      "WHERE auto_increment;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_unique_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE ai_object_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(11, 0));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

class EncodeKeysFunction MOZ_FINAL : public mozIStorageFunction
{
  ~EncodeKeysFunction() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult)
  {
    PROFILER_LABEL("EncodeKeysFunction", "OnFunctionCall",
      js::ProfileEntry::Category::STORAGE);

    uint32_t argc;
    nsresult rv = aArguments->GetNumEntries(&argc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (argc != 1) {
      NS_WARNING("Don't call me with the wrong number of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    int32_t type;
    rv = aArguments->GetTypeOfIndex(0, &type);
    NS_ENSURE_SUCCESS(rv, rv);

    Key key;
    if (type == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      int64_t intKey;
      aArguments->GetInt64(0, &intKey);
      key.SetFromInteger(intKey);
    }
    else if (type == mozIStorageStatement::VALUE_TYPE_TEXT) {
      nsString stringKey;
      aArguments->GetString(0, stringKey);
      key.SetFromString(stringKey);
    }
    else {
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

NS_IMPL_ISUPPORTS(EncodeKeysFunction, mozIStorageFunction)

nsresult
UpgradeSchemaFrom11_0To12_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom11_0To12_0",
    js::ProfileEntry::Category::STORAGE);

  NS_NAMED_LITERAL_CSTRING(encoderName, "encode");

  nsCOMPtr<mozIStorageFunction> encoder = new EncodeKeysFunction();

  nsresult rv = aConnection->CreateFunction(encoderName, 1, encoder);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id, "
      "key_value, "
      "data, "
      "file_ids "
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, encode(key_value), data, file_ids "
      "FROM object_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE object_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO object_data "
      "SELECT id, object_store_id, key_value, file_ids, data "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_insert_trigger "
    "AFTER INSERT ON object_data "
    "FOR EACH ROW "
    "WHEN NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
    "END;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_update_trigger "
    "AFTER UPDATE OF file_ids ON object_data "
    "FOR EACH ROW "
    "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
    "END;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TRIGGER object_data_delete_trigger "
    "AFTER DELETE ON object_data "
    "FOR EACH ROW WHEN OLD.file_ids IS NOT NULL "
    "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NULL); "
    "END;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, encode(value), encode(object_data_key), object_data_id "
      "FROM index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX index_data_object_data_id_index "
    "ON index_data (object_data_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
    ");"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO temp_upgrade "
      "SELECT index_id, encode(value), encode(object_data_key), object_data_id "
      "FROM unique_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE unique_index_data;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO unique_index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE temp_upgrade;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX unique_index_data_object_data_id_index "
    "ON unique_index_data (object_data_id);"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->RemoveFunction(encoderName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(12, 0));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom12_0To13_0(mozIStorageConnection* aConnection,
                            bool* aVacuumNeeded)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "UpgradeSchemaFrom12_0To13_0",
    js::ProfileEntry::Category::STORAGE);

  nsresult rv;

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
  int32_t defaultPageSize;
  rv = aConnection->GetDefaultPageSize(&defaultPageSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Enable auto_vacuum mode and update the page size to the platform default.
  nsAutoCString upgradeQuery("PRAGMA auto_vacuum = FULL; PRAGMA page_size = ");
  upgradeQuery.AppendInt(defaultPageSize);

  rv = aConnection->ExecuteSimpleSQL(upgradeQuery);
  NS_ENSURE_SUCCESS(rv, rv);

  *aVacuumNeeded = true;
#endif

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(13, 0));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom13_0To14_0(mozIStorageConnection* aConnection)
{
  // The only change between 13 and 14 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(14, 0));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
UpgradeSchemaFrom14_0To15_0(mozIStorageConnection* aConnection)
{
  // The only change between 14 and 15 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(15, 0));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

class VersionChangeEventsRunnable;

class SetVersionHelper : public AsyncConnectionHelper,
                         public IDBTransactionListener,
                         public AcquireListener
{
  friend class VersionChangeEventsRunnable;

public:
  SetVersionHelper(IDBTransaction* aTransaction,
                   IDBOpenDBRequest* aRequest,
                   OpenDatabaseHelper* aHelper,
                   uint64_t aRequestedVersion,
                   uint64_t aCurrentVersion)
  : AsyncConnectionHelper(aTransaction, aRequest),
    mOpenRequest(aRequest), mOpenHelper(aHelper),
    mRequestedVersion(aRequestedVersion),
    mCurrentVersion(aCurrentVersion)
  {
    mTransaction->SetTransactionListener(this);
  }

  NS_DECL_ISUPPORTS_INHERITED

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual nsresult
  OnExclusiveAccessAcquired() MOZ_OVERRIDE;

protected:
  virtual ~SetVersionHelper() {}

  virtual nsresult Init() MOZ_OVERRIDE;

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  // SetVersionHelper never fires an error event at the request.  It hands that
  // responsibility back to the OpenDatabaseHelper
  virtual void OnError() MOZ_OVERRIDE
  { }

  // Need an upgradeneeded event here.
  virtual already_AddRefed<nsIDOMEvent> CreateSuccessEvent(
    mozilla::dom::EventTarget* aOwner) MOZ_OVERRIDE;

  virtual nsresult NotifyTransactionPreComplete(IDBTransaction* aTransaction)
                                                MOZ_OVERRIDE;
  virtual nsresult NotifyTransactionPostComplete(IDBTransaction* aTransaction)
                                                 MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE
  {
    return Success_NotSent;
  }

  virtual nsresult UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
                                            MOZ_OVERRIDE
  {
    MOZ_CRASH("Should never get here!");
  }

  uint64_t RequestedVersion() const
  {
    return mRequestedVersion;
  }

private:
  // In-params
  nsRefPtr<IDBOpenDBRequest> mOpenRequest;
  nsRefPtr<OpenDatabaseHelper> mOpenHelper;
  uint64_t mRequestedVersion;
  uint64_t mCurrentVersion;
};

class DeleteDatabaseHelper : public AsyncConnectionHelper,
                             public AcquireListener
{
  friend class VersionChangeEventsRunnable;
public:
  DeleteDatabaseHelper(IDBOpenDBRequest* aRequest,
                       OpenDatabaseHelper* aHelper,
                       uint64_t aCurrentVersion,
                       const nsAString& aName,
                       const nsACString& aGroup,
                       const nsACString& aASCIIOrigin,
                       PersistenceType aPersistenceType)
  : AsyncConnectionHelper(static_cast<IDBDatabase*>(nullptr), aRequest),
    mOpenHelper(aHelper), mOpenRequest(aRequest),
    mCurrentVersion(aCurrentVersion), mName(aName),
    mGroup(aGroup), mASCIIOrigin(aASCIIOrigin),
    mPersistenceType(aPersistenceType)
  { }

  NS_DECL_ISUPPORTS_INHERITED

  nsresult GetSuccessResult(JSContext* aCx,
                            JS::MutableHandle<JS::Value> aVal);

  void ReleaseMainThreadObjects()
  {
    mOpenHelper = nullptr;
    mOpenRequest = nullptr;

    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

  virtual nsresult
  OnExclusiveAccessAcquired() MOZ_OVERRIDE;

protected:
  virtual ~DeleteDatabaseHelper() {}

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult Init();

  // DeleteDatabaseHelper never fires events at the request.  It hands that
  // responsibility back to the OpenDatabaseHelper
  void OnError()
  {
    mOpenHelper->NotifyDeleteFinished();
  }

  nsresult OnSuccess()
  {
    return mOpenHelper->NotifyDeleteFinished();
  }

  uint64_t RequestedVersion() const
  {
    return 0;
  }

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE
  {
    return Success_NotSent;
  }

  virtual nsresult UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
                                            MOZ_OVERRIDE
  {
    MOZ_CRASH("Should never get here!");
  }

private:
  // In-params
  nsRefPtr<OpenDatabaseHelper> mOpenHelper;
  nsRefPtr<IDBOpenDBRequest> mOpenRequest;
  uint64_t mCurrentVersion;
  nsString mName;
  nsCString mGroup;
  nsCString mASCIIOrigin;
  PersistenceType mPersistenceType;
};

// Responsible for firing "versionchange" events at all live and non-closed
// databases, and for firing a "blocked" event at the requesting database if any
// databases fail to close.
class VersionChangeEventsRunnable : public nsRunnable
{
public:
  VersionChangeEventsRunnable(
                      IDBDatabase* aRequestingDatabase,
                      IDBOpenDBRequest* aRequest,
                      nsTArray<nsCOMPtr<nsIOfflineStorage> >& aWaitingDatabases,
                      int64_t aOldVersion,
                      int64_t aNewVersion)
  : mRequestingDatabase(aRequestingDatabase),
    mRequest(aRequest),
    mOldVersion(aOldVersion),
    mNewVersion(aNewVersion)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(aRequestingDatabase, "Null pointer!");
    NS_ASSERTION(aRequest, "Null pointer!");

    mWaitingDatabases.SwapElements(aWaitingDatabases);
  }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    PROFILER_MAIN_THREAD_LABEL("OpenDatabaseHelper", "VersionChangeEventsRunnable::Run",
      js::ProfileEntry::Category::STORAGE);

    // Fire version change events at all of the databases that are not already
    // closed. Also kick bfcached documents out of bfcache.
    uint32_t count = mWaitingDatabases.Length();
    for (uint32_t index = 0; index < count; index++) {
      IDBDatabase* database =
        IDBDatabase::FromStorage(mWaitingDatabases[index]);
      NS_ASSERTION(database, "This shouldn't be null!");

      if (database->IsClosed()) {
        continue;
      }

      // First check if the document the IDBDatabase is part of is bfcached.
      nsCOMPtr<nsIDocument> ownerDoc = database->GetOwnerDocument();
      nsIBFCacheEntry* bfCacheEntry;
      if (ownerDoc && (bfCacheEntry = ownerDoc->GetBFCacheEntry())) {
        bfCacheEntry->RemoveFromBFCacheSync();
        NS_ASSERTION(database->IsClosed(),
                     "Kicking doc out of bfcache should have closed database");
        continue;
      }

      // Next check if it's in the process of being bfcached.
      nsPIDOMWindow* owner = database->GetOwner();
      if (owner && owner->IsFrozen()) {
        // We can't kick the document out of the bfcache because it's not yet
        // fully in the bfcache.  Instead we'll abort everything for the window
        // and mark it as not-bfcacheable.
        QuotaManager* quotaManager = QuotaManager::Get();
        NS_ASSERTION(quotaManager, "Huh?");
        quotaManager->AbortCloseStoragesForWindow(owner);

        NS_ASSERTION(database->IsClosed(),
                   "AbortCloseStoragesForWindow should have closed database");
        ownerDoc->DisallowBFCaching();
        continue;
      }

      // Otherwise fire a versionchange event.
      nsRefPtr<Event> event = 
        IDBVersionChangeEvent::Create(database, mOldVersion, mNewVersion);
      NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

      bool dummy;
      database->DispatchEvent(event, &dummy);
    }

    // Now check to see if any didn't close. If there are some running still
    // then fire the blocked event.
    for (uint32_t index = 0; index < count; index++) {
      if (!mWaitingDatabases[index]->IsClosed()) {
        nsRefPtr<Event> event =
          IDBVersionChangeEvent::CreateBlocked(mRequest,
                                               mOldVersion, mNewVersion);
        NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

        bool dummy;
        mRequest->DispatchEvent(event, &dummy);

        break;
      }
    }

    return NS_OK;
  }

  template <class T>
  static
  void QueueVersionChange(nsTArray<nsCOMPtr<nsIOfflineStorage> >& aDatabases,
                          void* aClosure);
private:
  nsRefPtr<IDBDatabase> mRequestingDatabase;
  nsRefPtr<IDBOpenDBRequest> mRequest;
  nsTArray<nsCOMPtr<nsIOfflineStorage> > mWaitingDatabases;
  int64_t mOldVersion;
  int64_t mNewVersion;
};

} // anonymous namespace

NS_IMPL_ISUPPORTS(OpenDatabaseHelper, nsIRunnable)

nsresult
OpenDatabaseHelper::Init()
{
  QuotaManager::GetStorageId(mPersistenceType, mASCIIOrigin, Client::IDB,
                             mName, mDatabaseId);
  MOZ_ASSERT(!mDatabaseId.IsEmpty());

  return NS_OK;
}

nsresult
OpenDatabaseHelper::WaitForOpenAllowed()
{
  NS_ASSERTION(mState == eCreated, "We've already been dispatched?");
  NS_ASSERTION(NS_IsMainThread(), "All hell is about to break lose!");

  mState = eOpenPending;

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never be null!");

  return quotaManager->
    WaitForOpenAllowed(OriginOrPatternString::FromOrigin(mASCIIOrigin),
                       Nullable<PersistenceType>(mPersistenceType), mDatabaseId,
                       this);
}

nsresult
OpenDatabaseHelper::Dispatch(nsIEventTarget* aTarget)
{
  NS_ASSERTION(mState == eCreated || mState == eOpenPending,
               "We've already been dispatched?");

  mState = eDBWork;

  return aTarget->Dispatch(this, NS_DISPATCH_NORMAL);
}

nsresult
OpenDatabaseHelper::DispatchToIOThread()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never be null!");

  return Dispatch(quotaManager->IOThread());
}

nsresult
OpenDatabaseHelper::RunImmediately()
{
  NS_ASSERTION(mState == eCreated || mState == eOpenPending,
               "We've already been dispatched?");
  NS_ASSERTION(NS_FAILED(mResultCode),
               "Should only be short-circuiting if we failed!");
  NS_ASSERTION(NS_IsMainThread(), "All hell is about to break lose!");

  mState = eFiringEvents;

  return this->Run();
}

nsresult
OpenDatabaseHelper::DoDatabaseWork()
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "DoDatabaseWork",
    js::ProfileEntry::Category::STORAGE);

  mState = eFiringEvents; // In case we fail somewhere along the line.

  if (QuotaManager::IsShuttingDown()) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  NS_ASSERTION(mOpenDBRequest, "This should never be null!");

  // This will be null for non-window contexts.
  nsPIDOMWindow* window = mOpenDBRequest->GetOwner();

  AutoEnterWindow autoWindow(window);

  nsCOMPtr<nsIFile> dbDirectory;

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never be null!");

  nsresult rv =
    quotaManager->EnsureOriginIsInitialized(mPersistenceType, mGroup,
                                            mASCIIOrigin, mTrackingQuota,
                                            getter_AddRefs(dbDirectory));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbDirectory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  bool exists;
  rv = dbDirectory->Exists(&exists);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!exists) {
    rv = dbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
#ifdef DEBUG
  else {
    bool isDirectory;
    NS_ASSERTION(NS_SUCCEEDED(dbDirectory->IsDirectory(&isDirectory)) &&
                isDirectory, "Should have caught this earlier!");
  }
#endif

  nsAutoString filename;
  rv = GetDatabaseFilename(mName, filename);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIFile> dbFile;
  rv = dbDirectory->Clone(getter_AddRefs(dbFile));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbFile->GetPath(mDatabaseFilePath);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIFile> fmDirectory;
  rv = dbDirectory->Clone(getter_AddRefs(fmDirectory));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = fmDirectory->Append(filename);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateDatabaseConnection(dbFile, fmDirectory, mName, mPersistenceType,
                                mGroup, mASCIIOrigin,
                                getter_AddRefs(connection));
  if (NS_FAILED(rv) &&
      NS_ERROR_GET_MODULE(rv) != NS_ERROR_MODULE_DOM_INDEXEDDB) {
    IDB_REPORT_INTERNAL_ERR();
    rv = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = IDBFactory::LoadDatabaseInformation(connection, mDatabaseId,
                                           &mCurrentVersion, mObjectStores);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mForDeletion) {
    mState = eDeletePending;
    return NS_OK;
  }

  for (uint32_t i = 0; i < mObjectStores.Length(); i++) {
    nsRefPtr<ObjectStoreInfo>& objectStoreInfo = mObjectStores[i];
    for (uint32_t j = 0; j < objectStoreInfo->indexes.Length(); j++) {
      IndexInfo& indexInfo = objectStoreInfo->indexes[j];
      mLastIndexId = std::max(indexInfo.id, mLastIndexId);
    }
    mLastObjectStoreId = std::max(objectStoreInfo->id, mLastObjectStoreId);
  }

  // See if we need to do a VERSION_CHANGE transaction

  // Optional version semantics.
  if (!mRequestedVersion) {
    // If the requested version was not specified and the database was created,
    // treat it as if version 1 were requested.
    if (mCurrentVersion == 0) {
      mRequestedVersion = 1;
    }
    else {
      // Otherwise, treat it as if the current version were requested.
      mRequestedVersion = mCurrentVersion;
    }
  }

  if (mCurrentVersion > mRequestedVersion) {
    return NS_ERROR_DOM_INDEXEDDB_VERSION_ERR;
  }

  if (mCurrentVersion != mRequestedVersion) {
    mState = eSetVersionPending;
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  nsRefPtr<FileManager> fileManager =
    mgr->GetFileManager(mPersistenceType, mASCIIOrigin, mName);
  if (!fileManager) {
    fileManager = new FileManager(mPersistenceType, mGroup, mASCIIOrigin,
                                  mPrivilege, mName);

    rv = fileManager->Init(fmDirectory, connection);
    IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    mgr->AddFileManager(fileManager);
  }

  mFileManager = fileManager.forget();

  return NS_OK;
}

// static
nsresult
OpenDatabaseHelper::CreateDatabaseConnection(
                                        nsIFile* aDBFile,
                                        nsIFile* aFMDirectory,
                                        const nsAString& aName,
                                        PersistenceType aPersistenceType,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin,
                                        mozIStorageConnection** aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("OpenDatabaseHelper", "CreateDatabaseConnection",
    js::ProfileEntry::Category::STORAGE);

  nsresult rv;
  bool exists;

  if (IndexedDatabaseManager::InLowDiskSpaceMode()) {
    rv = aDBFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!exists) {
      NS_WARNING("Refusing to create database because disk space is low!");
      return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    }
  }

  nsCOMPtr<nsIFileURL> dbFileUrl =
    IDBFactory::GetDatabaseFileURL(aDBFile, aPersistenceType, aGroup, aOrigin);
  NS_ENSURE_TRUE(dbFileUrl, NS_ERROR_FAILURE);

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(ss, NS_ERROR_FAILURE);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // If we're just opening the database during origin initialization, then
    // we don't want to erase any files. The failure here will fail origin
    // initialization too.
    if (aName.IsVoid()) {
      return rv;
    }

    // Nuke the database file.  The web services can recreate their data.
    rv = aDBFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aFMDirectory->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      bool isDirectory;
      rv = aFMDirectory->IsDirectory(&isDirectory);
      NS_ENSURE_SUCCESS(rv, rv);
      IDB_ENSURE_TRUE(isDirectory, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      rv = aFMDirectory->Remove(true);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(connection));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = IDBFactory::SetDefaultPragmas(connection);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->EnableModule(NS_LITERAL_CSTRING("filesystem"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check to make sure that the database schema is correct.
  int32_t schemaVersion;
  rv = connection->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  // Unknown schema will fail origin initialization too
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
      NS_ENSURE_SUCCESS(rv, rv);
    }
#endif

    mozStorageTransaction transaction(connection, false,
                                  mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (!schemaVersion) {
      // Brand new file, initialize our tables.
      rv = CreateTables(connection);
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ASSERTION(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)) &&
                   schemaVersion == kSQLiteSchemaVersion,
                   "CreateTables set a bad schema version!");

      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = connection->CreateStatement(NS_LITERAL_CSTRING(
        "INSERT INTO database (name) "
        "VALUES (:name)"
      ), getter_AddRefs(stmt));
      IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), aName);
      IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      rv = stmt->Execute();
      IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }
    else  {
      // This logic needs to change next time we change the schema!
      static_assert(kSQLiteSchemaVersion == int32_t((15 << 4) + 0),
                    "Need upgrade code from schema version increase.");

      while (schemaVersion != kSQLiteSchemaVersion) {
        if (schemaVersion == 4) {
          rv = UpgradeSchemaFrom4To5(connection);
        }
        else if (schemaVersion == 5) {
          rv = UpgradeSchemaFrom5To6(connection);
        }
        else if (schemaVersion == 6) {
          rv = UpgradeSchemaFrom6To7(connection);
        }
        else if (schemaVersion == 7) {
          rv = UpgradeSchemaFrom7To8(connection);
        }
        else if (schemaVersion == 8) {
          rv = UpgradeSchemaFrom8To9_0(connection);
          vacuumNeeded = true;
        }
        else if (schemaVersion == MakeSchemaVersion(9, 0)) {
          rv = UpgradeSchemaFrom9_0To10_0(connection);
        }
        else if (schemaVersion == MakeSchemaVersion(10, 0)) {
          rv = UpgradeSchemaFrom10_0To11_0(connection);
        }
        else if (schemaVersion == MakeSchemaVersion(11, 0)) {
          rv = UpgradeSchemaFrom11_0To12_0(connection);
        }
        else if (schemaVersion == MakeSchemaVersion(12, 0)) {
          rv = UpgradeSchemaFrom12_0To13_0(connection, &vacuumNeeded);
        }
        else if (schemaVersion == MakeSchemaVersion(13, 0)) {
          rv = UpgradeSchemaFrom13_0To14_0(connection);
        }
        else if (schemaVersion == MakeSchemaVersion(14, 0)) {
          rv = UpgradeSchemaFrom14_0To15_0(connection);
        }
        else {
          NS_WARNING("Unable to open IndexedDB database, no upgrade path is "
                     "available!");
          IDB_REPORT_INTERNAL_ERR();
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }
        NS_ENSURE_SUCCESS(rv, rv);

        rv = connection->GetSchemaVersion(&schemaVersion);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      NS_ASSERTION(schemaVersion == kSQLiteSchemaVersion, "Huh?!");
    }

    rv = transaction.Commit();
    if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      // mozstorage translates SQLITE_FULL to NS_ERROR_FILE_NO_DEVICE_SPACE,
      // which we know better as NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR.
      rv = NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (vacuumNeeded) {
    rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM;"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult
OpenDatabaseHelper::StartSetVersion()
{
  NS_ASSERTION(mState == eSetVersionPending, "Why are we here?");

  // In case we fail, fire error events
  mState = eFiringEvents;

  nsresult rv = EnsureSuccessResult();
  NS_ENSURE_SUCCESS(rv, rv);

  Sequence<nsString> storesToOpen;
  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(mDatabase, storesToOpen,
                           IDBTransaction::VERSION_CHANGE, true);
  IDB_ENSURE_TRUE(transaction, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<SetVersionHelper> helper =
    new SetVersionHelper(transaction, mOpenDBRequest, this, mRequestedVersion,
                         mCurrentVersion);

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never be null!");

  rv = quotaManager->AcquireExclusiveAccess(
             mDatabase, mDatabase->Origin(),
             Nullable<PersistenceType>(mDatabase->Type()), helper,
             &VersionChangeEventsRunnable::QueueVersionChange<SetVersionHelper>,
             helper);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  // The SetVersionHelper is responsible for dispatching us back to the
  // main thread again and changing the state to eSetVersionCompleted.
  mState = eSetVersionPending;
  return NS_OK;
}

nsresult
OpenDatabaseHelper::StartDelete()
{
  NS_ASSERTION(mState == eDeletePending, "Why are we here?");

  // In case we fail, fire error events
  mState = eFiringEvents;

  nsresult rv = EnsureSuccessResult();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<DeleteDatabaseHelper> helper =
    new DeleteDatabaseHelper(mOpenDBRequest, this, mCurrentVersion, mName,
                             mGroup, mASCIIOrigin, mPersistenceType);

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never be null!");

  rv = quotaManager->AcquireExclusiveAccess(
         mDatabase, mDatabase->Origin(),
         Nullable<PersistenceType>(mDatabase->Type()), helper,
         &VersionChangeEventsRunnable::QueueVersionChange<DeleteDatabaseHelper>,
         helper);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  // The DeleteDatabaseHelper is responsible for dispatching us back to the
  // main thread again and changing the state to eDeleteCompleted.
  mState = eDeletePending;
  return NS_OK;
}

NS_IMETHODIMP
OpenDatabaseHelper::Run()
{
  NS_ASSERTION(mState != eCreated, "Dispatch was not called?!?");

  if (NS_IsMainThread()) {
    PROFILER_MAIN_THREAD_LABEL("OpenDatabaseHelper", "Run",
      js::ProfileEntry::Category::STORAGE);

    if (mState == eOpenPending) {
      if (NS_FAILED(mResultCode)) {
        return RunImmediately();
      }

      return DispatchToIOThread();
    }

    // If we need to queue up a SetVersionHelper, do that here.
    if (mState == eSetVersionPending) {
      nsresult rv = StartSetVersion();

      if (NS_SUCCEEDED(rv)) {
        return rv;
      }

      SetError(rv);
      // fall through and run the default error processing
    }
    else if (mState == eDeletePending) {
      nsresult rv = StartDelete();

      if (NS_SUCCEEDED(rv)) {
        return rv;
      }

      SetError(rv);
      // fall through and run the default error processing
    }

    // We've done whatever work we need to do on the DB thread, and any
    // SetVersion/DeleteDatabase stuff is done by now.
    NS_ASSERTION(mState == eFiringEvents ||
                 mState == eSetVersionCompleted ||
                 mState == eDeleteCompleted, "Why are we here?");

    switch (mState) {
      case eSetVersionCompleted: {
        mState = eFiringEvents;
        break;
      }

      case eDeleteCompleted: {
        // Destroy the database now (we should have the only ref).
        mDatabase = nullptr;

        DatabaseInfo::Remove(mDatabaseId);

        mState = eFiringEvents;
        break;
      }

      case eFiringEvents: {
        // Notify the request that we're done, but only if we didn't just
        // finish a [SetVersion/DeleteDatabase]Helper.  In that case, the
        // helper tells the request that it is done, and we avoid calling
        // NotifyHelperCompleted twice.

        nsresult rv = mOpenDBRequest->NotifyHelperCompleted(this);
        if (NS_SUCCEEDED(mResultCode) && NS_FAILED(rv)) {
          mResultCode = rv;
        }
        break;
      }

      default:
        NS_NOTREACHED("Shouldn't get here!");
    }

    NS_ASSERTION(mState == eFiringEvents, "Why are we here?");

    IDB_PROFILER_MARK("IndexedDB Request %llu: Running main thread "
                      "response (rv = %lu)",
                      "IDBRequest[%llu] MT Done",
                      mRequest->GetSerialNumber(), mResultCode);

    if (NS_FAILED(mResultCode)) {
      DispatchErrorEvent();
    } else {
      DispatchSuccessEvent();
    }

    QuotaManager* quotaManager = QuotaManager::Get();
    NS_ASSERTION(quotaManager, "This should never be null!");

    quotaManager->
      AllowNextSynchronizedOp(OriginOrPatternString::FromOrigin(mASCIIOrigin),
                              Nullable<PersistenceType>(mPersistenceType),
                              mDatabaseId);

    ReleaseMainThreadObjects();

    return NS_OK;
  }

  PROFILER_LABEL("OpenDatabaseHelper", "Run",
    js::ProfileEntry::Category::STORAGE);

  // We're on the DB thread.
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  IDB_PROFILER_MARK("IndexedDB Request %llu: Beginning database work",
                    "IDBRequest[%llu] DT Start", mRequest->GetSerialNumber());

  NS_ASSERTION(mState == eDBWork, "Why are we here?");
  mResultCode = DoDatabaseWork();
  NS_ASSERTION(mState != eDBWork, "We should be doing something else now.");

  IDB_PROFILER_MARK("IndexedDB Request %llu: Finished database work (rv = %lu)",
                    "IDBRequest[%llu] DT Done", mRequest->GetSerialNumber(),
                    mResultCode);

  return NS_DispatchToMainThread(this);
}

nsresult
OpenDatabaseHelper::EnsureSuccessResult()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PROFILER_MAIN_THREAD_LABEL("OpenDatabaseHelper", "EnsureSuccessResult",
    js::ProfileEntry::Category::STORAGE);

  nsRefPtr<DatabaseInfo> dbInfo;
  if (DatabaseInfo::Get(mDatabaseId, getter_AddRefs(dbInfo))) {

#ifdef DEBUG
    {
      NS_ASSERTION(dbInfo->name == mName &&
                   dbInfo->version == mCurrentVersion &&
                   dbInfo->persistenceType == mPersistenceType &&
                   dbInfo->id == mDatabaseId &&
                   dbInfo->filePath == mDatabaseFilePath,
                   "Metadata mismatch!");

      uint32_t objectStoreCount = mObjectStores.Length();
      for (uint32_t index = 0; index < objectStoreCount; index++) {
        nsRefPtr<ObjectStoreInfo>& info = mObjectStores[index];

        ObjectStoreInfo* otherInfo = dbInfo->GetObjectStore(info->name);
        NS_ASSERTION(otherInfo, "ObjectStore not known!");

        NS_ASSERTION(info->name == otherInfo->name &&
                     info->id == otherInfo->id &&
                     info->keyPath == otherInfo->keyPath,
                     "Metadata mismatch!");
        NS_ASSERTION(dbInfo->ContainsStoreName(info->name),
                     "Object store names out of date!");
        NS_ASSERTION(info->indexes.Length() == otherInfo->indexes.Length(),
                     "Bad index length!");

        uint32_t indexCount = info->indexes.Length();
        for (uint32_t indexIndex = 0; indexIndex < indexCount; indexIndex++) {
          const IndexInfo& indexInfo = info->indexes[indexIndex];
          const IndexInfo& otherIndexInfo = otherInfo->indexes[indexIndex];
          NS_ASSERTION(indexInfo.id == otherIndexInfo.id,
                       "Bad index id!");
          NS_ASSERTION(indexInfo.name == otherIndexInfo.name,
                       "Bad index name!");
          NS_ASSERTION(indexInfo.keyPath == otherIndexInfo.keyPath,
                       "Bad index keyPath!");
          NS_ASSERTION(indexInfo.unique == otherIndexInfo.unique,
                       "Bad index unique value!");
        }
      }
    }
#endif

  }
  else {
    nsRefPtr<DatabaseInfo> newInfo(new DatabaseInfo());

    newInfo->name = mName;
    newInfo->group = mGroup;
    newInfo->origin = mASCIIOrigin;
    newInfo->persistenceType = mPersistenceType;
    newInfo->id = mDatabaseId;
    newInfo->filePath = mDatabaseFilePath;

    if (!DatabaseInfo::Put(newInfo)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    newInfo.swap(dbInfo);

    nsresult rv = IDBFactory::SetDatabaseMetadata(dbInfo, mCurrentVersion,
                                                  mObjectStores);
    IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(mObjectStores.IsEmpty(), "Should have swapped!");
  }

  dbInfo->nextObjectStoreId = mLastObjectStoreId + 1;
  dbInfo->nextIndexId = mLastIndexId + 1;

  nsRefPtr<IDBDatabase> database =
    IDBDatabase::Create(mOpenDBRequest, mOpenDBRequest->Factory(),
                        dbInfo.forget(), mASCIIOrigin, mFileManager,
                        mContentParent);
  if (!database) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  NS_ASSERTION(!mDatabase, "Shouldn't have a database yet!");
  mDatabase.swap(database);

  return NS_OK;
}

nsresult
OpenDatabaseHelper::GetSuccessResult(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aVal)
{
  // Be careful not to load the database twice.
  if (!mDatabase) {
    nsresult rv = EnsureSuccessResult();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return WrapNative(aCx, NS_ISUPPORTS_CAST(EventTarget*, mDatabase),
                    aVal);
}

nsresult
OpenDatabaseHelper::NotifySetVersionFinished()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread");
  NS_ASSERTION(mState = eSetVersionPending, "How did we get here?");

  // Allow transaction creation to proceed.
  mDatabase->ExitSetVersionTransaction();

  mState = eSetVersionCompleted;

  // Dispatch ourself back to the main thread
  return NS_DispatchToCurrentThread(this);
}

nsresult
OpenDatabaseHelper::NotifyDeleteFinished()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread");
  NS_ASSERTION(mState == eDeletePending, "How did we get here?");

  mState = eDeleteCompleted;
  
  // Dispatch ourself back to the main thread
  return NS_DispatchToCurrentThread(this);
}

void
OpenDatabaseHelper::BlockDatabase()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mDatabase, "This is going bad fast.");

  mDatabase->EnterSetVersionTransaction();
}

void
OpenDatabaseHelper::DispatchSuccessEvent()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PROFILER_MAIN_THREAD_LABEL("OpenDatabaseHelper", "DispatchSuccessEvent",
    js::ProfileEntry::Category::STORAGE);

  nsRefPtr<nsIDOMEvent> event =
    CreateGenericEvent(mOpenDBRequest, NS_LITERAL_STRING(SUCCESS_EVT_STR),
                       eDoesNotBubble, eNotCancelable);
  if (!event) {
    NS_ERROR("Failed to create event!");
    return;
  }

  bool dummy;
  mOpenDBRequest->DispatchEvent(event, &dummy);
}

void
OpenDatabaseHelper::DispatchErrorEvent()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PROFILER_MAIN_THREAD_LABEL("OpenDatabaseHelper", "DispatchErrorEvent",
    js::ProfileEntry::Category::STORAGE);

  nsRefPtr<nsIDOMEvent> event =
    CreateGenericEvent(mOpenDBRequest, NS_LITERAL_STRING(ERROR_EVT_STR),
                       eDoesBubble, eCancelable);
  if (!event) {
    NS_ERROR("Failed to create event!");
    return;
  }

  ErrorResult rv;
  nsRefPtr<DOMError> error = mOpenDBRequest->GetError(rv);

  NS_ASSERTION(!rv.Failed(), "This shouldn't be failing at this point!");
  if (!error) {
    mOpenDBRequest->SetError(mResultCode);
  }

  bool dummy;
  mOpenDBRequest->DispatchEvent(event, &dummy);
}

void
OpenDatabaseHelper::ReleaseMainThreadObjects()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mOpenDBRequest = nullptr;
  mDatabase = nullptr;

  HelperBase::ReleaseMainThreadObjects();
}

NS_IMPL_ISUPPORTS_INHERITED0(SetVersionHelper, AsyncConnectionHelper)

nsresult
SetVersionHelper::Init()
{
  // Block transaction creation until we are done.
  mOpenHelper->BlockDatabase();

  return NS_OK;
}

nsresult
SetVersionHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(aConnection, "Passing a null connection!");

  PROFILER_LABEL("SetVersionHelper", "DoDatabaseWork",
    js::ProfileEntry::Category::STORAGE);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE database "
    "SET version = :version"
  ), getter_AddRefs(stmt));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("version"),
                             mRequestedVersion);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
  }

  return NS_OK;
}

nsresult
SetVersionHelper::GetSuccessResult(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aVal)
{
  DatabaseInfo* info = mDatabase->Info();
  info->version = mRequestedVersion;

  NS_ASSERTION(mTransaction, "Better have a transaction!");

  mOpenRequest->SetTransaction(mTransaction);

  return WrapNative(aCx, NS_ISUPPORTS_CAST(EventTarget*, mDatabase),
                    aVal);
}

nsresult
SetVersionHelper::OnExclusiveAccessAcquired()
{
  nsresult rv = DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
template <class T>
void
VersionChangeEventsRunnable::QueueVersionChange(
                             nsTArray<nsCOMPtr<nsIOfflineStorage> >& aDatabases,
                             void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aDatabases.IsEmpty(), "Why are we here?");

  T* closure = static_cast<T*>(aClosure);

  nsRefPtr<VersionChangeEventsRunnable> eventsRunnable =
    new VersionChangeEventsRunnable(closure->mOpenHelper->Database(),
                                    closure->mOpenRequest,
                                    aDatabases,
                                    closure->mCurrentVersion,
                                    closure->RequestedVersion());

  NS_DispatchToCurrentThread(eventsRunnable);
}

already_AddRefed<nsIDOMEvent>
SetVersionHelper::CreateSuccessEvent(mozilla::dom::EventTarget* aOwner)
{
  NS_ASSERTION(mCurrentVersion < mRequestedVersion, "Huh?");

  return IDBVersionChangeEvent::CreateUpgradeNeeded(aOwner,
                                                    mCurrentVersion,
                                                    mRequestedVersion);
}

nsresult
SetVersionHelper::NotifyTransactionPreComplete(IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "This is unexpected.");
  NS_ASSERTION(mOpenRequest, "Why don't we have a request?");

  return mOpenHelper->NotifySetVersionFinished();
}

nsresult
SetVersionHelper::NotifyTransactionPostComplete(IDBTransaction* aTransaction)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "This is unexpected.");
  NS_ASSERTION(mOpenRequest, "Why don't we have a request?");

  // If we hit an error, the OpenDatabaseHelper needs to get that error too.
  nsresult rv = GetResultCode();
  if (NS_FAILED(rv)) {
    mOpenHelper->SetError(rv);
  }

  // If the transaction was aborted, we should throw an error message.
  if (aTransaction->IsAborted()) {
    mOpenHelper->SetError(aTransaction->GetAbortCode());
  }

  mOpenRequest->SetTransaction(nullptr);
  mOpenRequest = nullptr;

  mOpenHelper = nullptr;

  return rv;
}

NS_IMPL_ISUPPORTS_INHERITED0(DeleteDatabaseHelper, AsyncConnectionHelper);

nsresult
DeleteDatabaseHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(!aConnection, "How did we get a connection here?");

  PROFILER_LABEL("DeleteDatabaseHelper", "DoDatabaseWork",
    js::ProfileEntry::Category::STORAGE);

  const StoragePrivilege& privilege = mOpenHelper->Privilege();

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsCOMPtr<nsIFile> directory;
  nsresult rv = quotaManager->GetDirectoryForOrigin(mPersistenceType,
                                                    mASCIIOrigin,
                                                    getter_AddRefs(directory));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(directory, "What?");

  rv = directory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsAutoString filename;
  rv = GetDatabaseFilename(mName, filename);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIFile> dbFile;
  rv = directory->Clone(getter_AddRefs(dbFile));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  bool exists = false;
  rv = dbFile->Exists(&exists);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (exists) {
    int64_t fileSize;

    if (privilege != Chrome) {
      rv = dbFile->GetFileSize(&fileSize);
      IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }

    rv = dbFile->Remove(false);
    IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (privilege != Chrome) {
      QuotaManager* quotaManager = QuotaManager::Get();
      NS_ASSERTION(quotaManager, "Shouldn't be null!");

      quotaManager->DecreaseUsageForOrigin(mPersistenceType, mGroup,
                                           mASCIIOrigin, fileSize);
    }
  }

  nsCOMPtr<nsIFile> dbJournalFile;
  rv = directory->Clone(getter_AddRefs(dbJournalFile));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbJournalFile->Append(filename + NS_LITERAL_STRING(".sqlite-journal"));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbJournalFile->Exists(&exists);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (exists) {
    rv = dbJournalFile->Remove(false);
    IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  nsCOMPtr<nsIFile> fmDirectory;
  rv = directory->Clone(getter_AddRefs(fmDirectory));
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = fmDirectory->Append(filename);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = fmDirectory->Exists(&exists);
  IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (exists) {
    bool isDirectory;
    rv = fmDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    IDB_ENSURE_TRUE(isDirectory, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    uint64_t usage = 0;

    if (privilege != Chrome) {
      rv = FileManager::GetUsage(fmDirectory, &usage);
      IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }

    rv = fmDirectory->Remove(true);
    IDB_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (privilege != Chrome) {
      QuotaManager* quotaManager = QuotaManager::Get();
      NS_ASSERTION(quotaManager, "Shouldn't be null!");

      quotaManager->DecreaseUsageForOrigin(mPersistenceType, mGroup,
                                           mASCIIOrigin, usage);
    }
  }

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never fail!");

  mgr->InvalidateFileManager(mPersistenceType, mASCIIOrigin, mName);

  return NS_OK;
}

nsresult
DeleteDatabaseHelper::GetSuccessResult(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  return NS_OK;
}

nsresult
DeleteDatabaseHelper::OnExclusiveAccessAcquired()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "We should definitely have a manager here");

  nsresult rv = Dispatch(quotaManager->IOThread());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
DeleteDatabaseHelper::Init()
{
  // Note that there's no need to block the database here, since the page
  // never gets to touch it, and all other databases must be closed.

  return NS_OK;
}
