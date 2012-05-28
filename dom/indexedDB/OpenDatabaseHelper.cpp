/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpenDatabaseHelper.h"

#include "nsIFile.h"

#include "mozilla/storage.h"
#include "nsContentUtils.h"
#include "nsEscape.h"
#include "nsThreadUtils.h"
#include "snappy/snappy.h"
#include "test_quota.h"

#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IndexedDatabaseManager.h"

using namespace mozilla;
USING_INDEXEDDB_NAMESPACE

namespace {

// If JS_STRUCTURED_CLONE_VERSION changes then we need to update our major
// schema version.
PR_STATIC_ASSERT(JS_STRUCTURED_CLONE_VERSION == 1);

// Major schema version. Bump for almost everything.
const PRUint32 kMajorSchemaVersion = 12;

// Minor schema version. Should almost always be 0 (maybe bump on release
// branches if we have to).
const PRUint32 kMinorSchemaVersion = 0;

// The schema version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 4 bits so the max value is
// 0xFFFFFFF. The minor version occupies the lower 4 bits and its max is 0xF.
PR_STATIC_ASSERT(kMajorSchemaVersion <= 0xFFFFFFF);
PR_STATIC_ASSERT(kMajorSchemaVersion <= 0xF);

inline
PRInt32
MakeSchemaVersion(PRUint32 aMajorSchemaVersion,
                  PRUint32 aMinorSchemaVersion)
{
  return PRInt32((aMajorSchemaVersion << 4) + aMinorSchemaVersion);
}

const PRInt32 kSQLiteSchemaVersion = PRInt32((kMajorSchemaVersion << 4) +
                                             kMinorSchemaVersion);

inline
PRUint32 GetMajorSchemaVersion(PRInt32 aSchemaVersion)
{
  return PRUint32(aSchemaVersion) >> 4;
}

inline
PRUint32 GetMinorSchemaVersion(PRInt32 aSchemaVersion)
{
  return PRUint32(aSchemaVersion) & 0xF;
}

nsresult
GetDatabaseFilename(const nsAString& aName,
                    nsAString& aDatabaseFilename)
{
  aDatabaseFilename.AppendInt(HashString(aName));

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
  NS_PRECONDITION(!NS_IsMainThread(),
                  "Creating tables on the main thread!");
  NS_PRECONDITION(aDBConn, "Passing a null database connection!");

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
  PRInt32 intVersion;
  PRInt64 dataVersion;

  {
    mozStorageStatementScoper scoper(stmt);

    bool hasResults;
    rv = stmt->ExecuteStep(&hasResults);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(hasResults, NS_ERROR_FAILURE);

    nsString version;
    rv = stmt->GetString(1, version);
    NS_ENSURE_SUCCESS(rv, rv);

    intVersion = version.ToInteger(&rv, 10);
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

    rv = stmt->BindStringParameter(0, name);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt32Parameter(1, intVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt64Parameter(2, dataVersion);
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
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult)
  {
    PRUint32 argc;
    nsresult rv = aArguments->GetNumEntries(&argc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (argc != 1) {
      NS_WARNING("Don't call me with the wrong number of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    PRInt32 type;
    rv = aArguments->GetTypeOfIndex(0, &type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (type != mozIStorageStatement::VALUE_TYPE_BLOB) {
      NS_WARNING("Don't call me with the wrong type of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    const PRUint8* uncompressed;
    PRUint32 uncompressedLength;
    rv = aArguments->GetSharedBlob(0, &uncompressedLength, &uncompressed);
    NS_ENSURE_SUCCESS(rv, rv);

    size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);
    nsAutoArrayPtr<char> compressed(new char[compressedLength]);

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

NS_IMPL_ISUPPORTS1(CompressDataBlobsFunction, mozIStorageFunction)

nsresult
UpgradeSchemaFrom8To9_0(mozIStorageConnection* aConnection)
{
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
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult)
  {
    PRUint32 argc;
    nsresult rv = aArguments->GetNumEntries(&argc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (argc != 1) {
      NS_WARNING("Don't call me with the wrong number of arguments!");
      return NS_ERROR_UNEXPECTED;
    }

    PRInt32 type;
    rv = aArguments->GetTypeOfIndex(0, &type);
    NS_ENSURE_SUCCESS(rv, rv);

    Key key;
    if (type == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      PRInt64 intKey;
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

NS_IMPL_ISUPPORTS1(EncodeKeysFunction, mozIStorageFunction)

nsresult
UpgradeSchemaFrom11_0To12_0(mozIStorageConnection* aConnection)
{
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

class VersionChangeEventsRunnable;

class SetVersionHelper : public AsyncConnectionHelper,
                         public IDBTransactionListener
{
  friend class VersionChangeEventsRunnable;
public:
  SetVersionHelper(IDBTransaction* aTransaction,
                   IDBOpenDBRequest* aRequest,
                   OpenDatabaseHelper* aHelper,
                   PRUint64 aRequestedVersion,
                   PRUint64 aCurrentVersion)
  : AsyncConnectionHelper(aTransaction, aRequest),
    mOpenRequest(aRequest), mOpenHelper(aHelper),
    mRequestedVersion(aRequestedVersion),
    mCurrentVersion(aCurrentVersion)
  {
    mTransaction->SetTransactionListener(this);
  }

  NS_DECL_ISUPPORTS_INHERITED

  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

protected:
  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult Init();

  // SetVersionHelper never fires an error event at the request.  It hands that
  // responsibility back to the OpenDatabaseHelper
  void OnError() { }

  // Need an upgradeneeded event here.
  already_AddRefed<nsDOMEvent> CreateSuccessEvent();

  nsresult NotifyTransactionComplete(IDBTransaction* aTransaction);

  PRUint64 RequestedVersion() const
  {
    return mRequestedVersion;
  }

private:
  // In-params
  nsRefPtr<IDBOpenDBRequest> mOpenRequest;
  nsRefPtr<OpenDatabaseHelper> mOpenHelper;
  PRUint64 mRequestedVersion;
  PRUint64 mCurrentVersion;
};

class DeleteDatabaseHelper : public AsyncConnectionHelper
{
  friend class VersionChangeEventsRunnable;
public:
  DeleteDatabaseHelper(IDBOpenDBRequest* aRequest,
                       OpenDatabaseHelper* aHelper,
                       PRUint64 aCurrentVersion,
                       const nsAString& aName,
                       const nsACString& aASCIIOrigin)
  : AsyncConnectionHelper(static_cast<IDBDatabase*>(nsnull), aRequest),
    mOpenHelper(aHelper), mOpenRequest(aRequest),
    mCurrentVersion(aCurrentVersion), mName(aName),
    mASCIIOrigin(aASCIIOrigin)
  { }

  nsresult GetSuccessResult(JSContext* aCx,
                            jsval* aVal);

  void ReleaseMainThreadObjects()
  {
    mOpenHelper = nsnull;
    mOpenRequest = nsnull;

    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

protected:
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

  PRUint64 RequestedVersion() const
  {
    return 0;
  }
private:
  // In-params
  nsRefPtr<OpenDatabaseHelper> mOpenHelper;
  nsRefPtr<IDBOpenDBRequest> mOpenRequest;
  PRUint64 mCurrentVersion;
  nsString mName;
  nsCString mASCIIOrigin;
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
                            nsTArray<nsRefPtr<IDBDatabase> >& aWaitingDatabases,
                            PRInt64 aOldVersion,
                            PRInt64 aNewVersion)
  : mRequestingDatabase(aRequestingDatabase),
    mRequest(aRequest),
    mOldVersion(aOldVersion),
    mNewVersion(aNewVersion)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(aRequestingDatabase, "Null pointer!");
    NS_ASSERTION(aRequest, "Null pointer!");

    if (!mWaitingDatabases.SwapElements(aWaitingDatabases)) {
      NS_ERROR("This should never fail!");
    }
  }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    // Fire version change events at all of the databases that are not already
    // closed. Also kick bfcached documents out of bfcache.
    PRUint32 count = mWaitingDatabases.Length();
    for (PRUint32 index = 0; index < count; index++) {
      nsRefPtr<IDBDatabase>& database = mWaitingDatabases[index];

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

      // Otherwise fire a versionchange event.
      nsRefPtr<nsDOMEvent> event = 
        IDBVersionChangeEvent::Create(mOldVersion, mNewVersion);
      NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

      bool dummy;
      database->DispatchEvent(event, &dummy);
    }

    // Now check to see if any didn't close. If there are some running still
    // then fire the blocked event.
    for (PRUint32 index = 0; index < count; index++) {
      if (!mWaitingDatabases[index]->IsClosed()) {
        nsRefPtr<nsDOMEvent> event =
          IDBVersionChangeEvent::CreateBlocked(mOldVersion, mNewVersion);
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
  void QueueVersionChange(nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
                          void* aClosure);
private:
  nsRefPtr<IDBDatabase> mRequestingDatabase;
  nsRefPtr<IDBOpenDBRequest> mRequest;
  nsTArray<nsRefPtr<IDBDatabase> > mWaitingDatabases;
  PRInt64 mOldVersion;
  PRInt64 mNewVersion;
};

} // anonymous namespace

NS_IMPL_THREADSAFE_ISUPPORTS1(OpenDatabaseHelper, nsIRunnable);

nsresult
OpenDatabaseHelper::Init()
{
  nsCString str(mASCIIOrigin);
  str.Append("*");
  str.Append(NS_ConvertUTF16toUTF8(mName));

  nsCOMPtr<nsIAtom> atom = do_GetAtom(str);
  NS_ENSURE_TRUE(atom, NS_ERROR_FAILURE);

  atom.swap(mDatabaseId);
  return NS_OK;
}

nsresult
OpenDatabaseHelper::Dispatch(nsIEventTarget* aTarget)
{
  NS_ASSERTION(mState == eCreated, "We've already been dispatched?");
  mState = eDBWork;

  return aTarget->Dispatch(this, NS_DISPATCH_NORMAL);
}

nsresult
OpenDatabaseHelper::RunImmediately()
{
  NS_ASSERTION(mState == eCreated, "We've already been dispatched?");
  NS_ASSERTION(NS_FAILED(mResultCode),
               "Should only be short-circuiting if we failed!");
  NS_ASSERTION(NS_IsMainThread(), "All hell is about to break lose!");

  mState = eFiringEvents;
  return this->Run();
}

nsresult
OpenDatabaseHelper::DoDatabaseWork()
{
#ifdef DEBUG
  {
    bool correctThread;
    NS_ASSERTION(NS_SUCCEEDED(IndexedDatabaseManager::Get()->IOThread()->
                              IsOnCurrentThread(&correctThread)) &&
                 correctThread,
                 "Running on the wrong thread!");
  }
#endif

  mState = eFiringEvents; // In case we fail somewhere along the line.

  if (IndexedDatabaseManager::IsShuttingDown()) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  NS_ASSERTION(mOpenDBRequest, "This should never be null!");

  // This will be null for non-window contexts.
  nsPIDOMWindow* window = mOpenDBRequest->GetOwner();

  AutoEnterWindow autoWindow(window);

  nsCOMPtr<nsIFile> dbDirectory;

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  nsresult rv = mgr->EnsureOriginIsInitialized(mASCIIOrigin,
                                               getter_AddRefs(dbDirectory));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsAutoString filename;
  rv = GetDatabaseFilename(mName, filename);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  
  nsCOMPtr<nsIFile> dbFile;
  rv = dbDirectory->Clone(getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbFile->GetPath(mDatabaseFilePath);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIFile> fileManagerDirectory;
  rv = dbDirectory->Clone(getter_AddRefs(fileManagerDirectory));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = fileManagerDirectory->Append(filename);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateDatabaseConnection(mName, dbFile, fileManagerDirectory,
                                getter_AddRefs(connection));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = IDBFactory::LoadDatabaseInformation(connection, mDatabaseId,
                                           &mCurrentVersion, mObjectStores);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (mForDeletion) {
    mState = eDeletePending;
    return NS_OK;
  }

  for (PRUint32 i = 0; i < mObjectStores.Length(); i++) {
    nsRefPtr<ObjectStoreInfo>& objectStoreInfo = mObjectStores[i];
    for (PRUint32 j = 0; j < objectStoreInfo->indexes.Length(); j++) {
      IndexInfo& indexInfo = objectStoreInfo->indexes[j];
      mLastIndexId = NS_MAX(indexInfo.id, mLastIndexId);
    }
    mLastObjectStoreId = NS_MAX(objectStoreInfo->id, mLastObjectStoreId);
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

  mFileManager = mgr->GetOrCreateFileManager(mASCIIOrigin, mName);
  NS_ENSURE_TRUE(mFileManager, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!mFileManager->Inited()) {
    rv = mFileManager->Init(fileManagerDirectory, connection);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  if (!mFileManager->Loaded()) {
    rv = mFileManager->Load(connection);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  return NS_OK;
}

// static
nsresult
OpenDatabaseHelper::CreateDatabaseConnection(
                                        const nsAString& aName,
                                        nsIFile* aDBFile,
                                        nsIFile* aFileManagerDirectory,
                                        mozIStorageConnection** aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  NS_NAMED_LITERAL_CSTRING(quotaVFSName, "quota");

  nsCOMPtr<mozIStorageServiceQuotaManagement> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(ss, NS_ERROR_FAILURE);

  nsCOMPtr<mozIStorageConnection> connection;
  nsresult rv = ss->OpenDatabaseWithVFS(aDBFile, quotaVFSName,
                                        getter_AddRefs(connection));
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

    bool exists;
    rv = aFileManagerDirectory->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      bool isDirectory;
      rv = aFileManagerDirectory->IsDirectory(&isDirectory);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(isDirectory, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      rv = aFileManagerDirectory->Remove(true);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = ss->OpenDatabaseWithVFS(aDBFile, quotaVFSName,
                                 getter_AddRefs(connection));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->EnableModule(NS_LITERAL_CSTRING("filesystem"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check to make sure that the database schema is correct.
  PRInt32 schemaVersion;
  rv = connection->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  // Unknown schema will fail origin initialization too
  if (!schemaVersion && aName.IsVoid()) {
    NS_WARNING("Unable to open IndexedDB database, schema is not set!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (schemaVersion > kSQLiteSchemaVersion) {
    NS_WARNING("Unable to open IndexedDB database, schema is too high!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  bool vacuumNeeded = false;

  if (schemaVersion != kSQLiteSchemaVersion) {
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
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), aName);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }
    else  {
      // This logic needs to change next time we change the schema!
      PR_STATIC_ASSERT(kSQLiteSchemaVersion == PRInt32((12 << 4) + 0));

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
        else {
          NS_WARNING("Unable to open IndexedDB database, no upgrade path is "
                     "available!");
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }
        NS_ENSURE_SUCCESS(rv, rv);

        rv = connection->GetSchemaVersion(&schemaVersion);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      NS_ASSERTION(schemaVersion == kSQLiteSchemaVersion, "Huh?!");
    }

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (vacuumNeeded) {
    rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "VACUUM;"
    ));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Turn on foreign key constraints.
  rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA foreign_keys = ON;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

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

  nsTArray<nsString> storesToOpen;
  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(mDatabase, storesToOpen,
                           IDBTransaction::VERSION_CHANGE, true);
  NS_ENSURE_TRUE(transaction, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<SetVersionHelper> helper =
    new SetVersionHelper(transaction, mOpenDBRequest, this, mRequestedVersion,
                         mCurrentVersion);

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  rv = mgr->AcquireExclusiveAccess(mDatabase, helper,
            &VersionChangeEventsRunnable::QueueVersionChange<SetVersionHelper>,
                                   helper);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

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
                             mASCIIOrigin);

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  rv = mgr->AcquireExclusiveAccess(mDatabase, helper,
        &VersionChangeEventsRunnable::QueueVersionChange<DeleteDatabaseHelper>,
                                   helper);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

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
        // Allow transaction creation/other version change transactions to proceed
        // before we fire events.  Other version changes will be postd to the end
        // of the event loop, and will be behind whatever the page does in
        // its error/success event handlers.
        mDatabase->ExitSetVersionTransaction();

        mState = eFiringEvents;
        break;
      }

      case eDeleteCompleted: {
        // Destroy the database now (we should have the only ref).
        mDatabase = nsnull;

        DatabaseInfo::Remove(mDatabaseId);

        IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
        NS_ASSERTION(mgr, "This should never fail!");

        mgr->InvalidateFileManager(mASCIIOrigin, mName);

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

    if (NS_FAILED(mResultCode)) {
      DispatchErrorEvent();
    } else {
      DispatchSuccessEvent();
    }

    IndexedDatabaseManager* manager = IndexedDatabaseManager::Get();
    NS_ASSERTION(manager, "This should never be null!");

    manager->AllowNextSynchronizedOp(mASCIIOrigin, mDatabaseId);

    ReleaseMainThreadObjects();

    return NS_OK;
  }

  // If we're on the DB thread, do that
  NS_ASSERTION(mState == eDBWork, "Why are we here?");
  mResultCode = DoDatabaseWork();
  NS_ASSERTION(mState != eDBWork, "We should be doing something else now.");

  return NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
}

nsresult
OpenDatabaseHelper::EnsureSuccessResult()
{
  nsRefPtr<DatabaseInfo> dbInfo;
  if (DatabaseInfo::Get(mDatabaseId, getter_AddRefs(dbInfo))) {

#ifdef DEBUG
    {
      NS_ASSERTION(dbInfo->name == mName &&
                   dbInfo->version == mCurrentVersion &&
                   dbInfo->id == mDatabaseId &&
                   dbInfo->filePath == mDatabaseFilePath,
                   "Metadata mismatch!");

      PRUint32 objectStoreCount = mObjectStores.Length();
      for (PRUint32 index = 0; index < objectStoreCount; index++) {
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

        PRUint32 indexCount = info->indexes.Length();
        for (PRUint32 indexIndex = 0; indexIndex < indexCount; indexIndex++) {
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
    newInfo->origin = mASCIIOrigin;
    newInfo->id = mDatabaseId;
    newInfo->filePath = mDatabaseFilePath;

    if (!DatabaseInfo::Put(newInfo)) {
      NS_ERROR("Failed to add to hash!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    newInfo.swap(dbInfo);

    nsresult rv = IDBFactory::SetDatabaseMetadata(dbInfo, mCurrentVersion,
                                                  mObjectStores);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(mObjectStores.IsEmpty(), "Should have swapped!");
  }

  dbInfo->nextObjectStoreId = mLastObjectStoreId + 1;
  dbInfo->nextIndexId = mLastIndexId + 1;

  nsRefPtr<IDBDatabase> database =
    IDBDatabase::Create(mOpenDBRequest,
                        dbInfo.forget(),
                        mASCIIOrigin,
                        mFileManager);
  if (!database) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  NS_ASSERTION(!mDatabase, "Shouldn't have a database yet!");
  mDatabase.swap(database);

  return NS_OK;
}

nsresult
OpenDatabaseHelper::GetSuccessResult(JSContext* aCx,
                                     jsval* aVal)
{
  // Be careful not to load the database twice.
  if (!mDatabase) {
    nsresult rv = EnsureSuccessResult();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return WrapNative(aCx, NS_ISUPPORTS_CAST(nsIDOMEventTarget*, mDatabase),
                    aVal);
}

nsresult
OpenDatabaseHelper::NotifySetVersionFinished()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread");
  NS_ASSERTION(mState = eSetVersionPending, "How did we get here?");

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
  nsRefPtr<nsDOMEvent> event =
    CreateGenericEvent(NS_LITERAL_STRING(SUCCESS_EVT_STR),
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
  nsRefPtr<nsDOMEvent> event =
    CreateGenericEvent(NS_LITERAL_STRING(ERROR_EVT_STR),
                       eDoesBubble, eCancelable);
  if (!event) {
    NS_ERROR("Failed to create event!");
    return;
  }

  nsCOMPtr<nsIDOMDOMError> error;
  DebugOnly<nsresult> rv =
    mOpenDBRequest->GetError(getter_AddRefs(error));
  NS_ASSERTION(NS_SUCCEEDED(rv), "This shouldn't be failing at this point!");
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

  mOpenDBRequest = nsnull;
  mDatabase = nsnull;
  mDatabaseId = nsnull;

  HelperBase::ReleaseMainThreadObjects();
}

NS_IMPL_ISUPPORTS_INHERITED0(SetVersionHelper, AsyncConnectionHelper);

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
  NS_ASSERTION(aConnection, "Passing a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE database "
    "SET version = :version"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("version"),
                             mRequestedVersion);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
  }

  return NS_OK;
}

nsresult
SetVersionHelper::GetSuccessResult(JSContext* aCx,
                                   jsval* aVal)
{
  DatabaseInfo* info = mDatabase->Info();
  info->version = mRequestedVersion;

  NS_ASSERTION(mTransaction, "Better have a transaction!");

  mOpenRequest->SetTransaction(mTransaction);

  return WrapNative(aCx, NS_ISUPPORTS_CAST(nsIDOMEventTarget*, mDatabase),
                    aVal);
}

// static
template <class T>
void
VersionChangeEventsRunnable::QueueVersionChange(
                                  nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
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

already_AddRefed<nsDOMEvent>
SetVersionHelper::CreateSuccessEvent()
{
  NS_ASSERTION(mCurrentVersion < mRequestedVersion, "Huh?");

  return IDBVersionChangeEvent::CreateUpgradeNeeded(mCurrentVersion,
                                                    mRequestedVersion);
}

nsresult
SetVersionHelper::NotifyTransactionComplete(IDBTransaction* aTransaction)
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
    mOpenHelper->SetError(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  }

  mOpenRequest->SetTransaction(nsnull);
  mOpenRequest = nsnull;

  rv = mOpenHelper->NotifySetVersionFinished();
  mOpenHelper = nsnull;

  return rv;
}

nsresult
DeleteDatabaseHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!aConnection, "How did we get a connection here?");

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never fail!");

  nsCOMPtr<nsIFile> directory;
  nsresult rv = mgr->GetDirectoryForOrigin(mASCIIOrigin,
                                           getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(directory, "What?");

  nsAutoString filename;
  rv = GetDatabaseFilename(mName, filename);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIFile> dbFile;
  rv = directory->Clone(getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = dbFile->Append(filename + NS_LITERAL_STRING(".sqlite"));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  bool exists = false;
  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  int rc;

  if (exists) {
    nsString dbFilePath;
    rv = dbFile->GetPath(dbFilePath);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rc = sqlite3_quota_remove(NS_ConvertUTF16toUTF8(dbFilePath).get());
    if (rc != SQLITE_OK) {
      NS_WARNING("Failed to delete db file!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  nsCOMPtr<nsIFile> fileManagerDirectory;
  rv = directory->Clone(getter_AddRefs(fileManagerDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = fileManagerDirectory->Append(filename);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = fileManagerDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (exists) {
    bool isDirectory;
    rv = fileManagerDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    nsString fileManagerDirectoryPath;
    rv = fileManagerDirectory->GetPath(fileManagerDirectoryPath);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rc = sqlite3_quota_remove(
      NS_ConvertUTF16toUTF8(fileManagerDirectoryPath).get());
    if (rc != SQLITE_OK) {
      NS_WARNING("Failed to delete file directory!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    rv = fileManagerDirectory->Remove(true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
DeleteDatabaseHelper::GetSuccessResult(JSContext* aCx, jsval* aVal)
{
  return NS_OK;
}

nsresult
DeleteDatabaseHelper::Init()
{
  // Note that there's no need to block the database here, since the page
  // never gets to touch it, and all other databases must be closed.

  return NS_OK;
}
