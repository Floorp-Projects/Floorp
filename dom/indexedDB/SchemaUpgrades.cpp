/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SchemaUpgrades.h"

// local includes
#include "ActorsParentCommon.h"
#include "DatabaseFileInfo.h"
#include "DatabaseFileManager.h"
#include "DBSchema.h"
#include "IndexedDatabase.h"
#include "IndexedDatabaseInlines.h"
#include "IndexedDBCommon.h"
#include "ReportInternalError.h"

// global includes
#include <stdlib.h>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include <utility>
#include "ErrorList.h"
#include "MainThreadUtils.h"
#include "SafeRefPtr.h"
#include "js/RootingAPI.h"
#include "js/StructuredClone.h"
#include "js/Value.h"
#include "jsapi.h"
#include "mozIStorageConnection.h"
#include "mozIStorageFunction.h"
#include "mozIStorageStatement.h"
#include "mozIStorageValueArray.h"
#include "mozStorageHelper.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Monitor.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/indexedDB/IDBResult.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/quota/Assertions.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/fallible.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/mozalloc.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/storage/Variant.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupports.h"
#include "nsIVariant.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTLiteralString.h"
#include "nsTStringRepr.h"
#include "nsThreadUtils.h"
#include "nscore.h"
#include "snappy/snappy.h"

struct JSContext;
class JSObject;

#if defined(MOZ_WIDGET_ANDROID)
#  define IDB_MOBILE
#endif

namespace mozilla::dom::indexedDB {

using mozilla::ipc::IsOnBackgroundThread;
using quota::AssertIsOnIOThread;
using quota::PERSISTENCE_TYPE_INVALID;

namespace {

nsresult UpgradeSchemaFrom4To5(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom4To5", DOM);

  nsresult rv;

  // All we changed is the type of the version column, so lets try to
  // convert that to an integer, and if we fail, set it to 0.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection.CreateStatement(
      "SELECT name, version, dataVersion "
      "FROM database"_ns,
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString name;
  int32_t intVersion;
  int64_t dataVersion;

  {
    mozStorageStatementScoper scoper(stmt);

    QM_TRY_INSPECT(const bool& hasResults,
                   MOZ_TO_RESULT_INVOKE_MEMBER(stmt, ExecuteStep));

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

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE database"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TABLE database ("
      "name TEXT NOT NULL, "
      "version INTEGER NOT NULL DEFAULT 0, "
      "dataVersion INTEGER NOT NULL"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  rv = aConnection.CreateStatement(
      "INSERT INTO database (name, version, dataVersion) "
      "VALUES (:name, :version, :dataVersion)"_ns,
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  {
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindStringByIndex(0, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt32ByIndex(1, intVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByIndex(2, dataVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aConnection.SetSchemaVersion(5);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom5To6(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom5To6", DOM);

  // First, drop all the indexes we're no longer going to use.
  nsresult rv = aConnection.ExecuteSimpleSQL("DROP INDEX key_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP INDEX ai_key_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP INDEX value_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP INDEX ai_value_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now, reorder the columns of object_data to put the blob data last. We do
  // this by copying into a temporary table, dropping the original, then copying
  // back into a newly created table.
  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id, "
      "key_value, "
      "data "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, key_value, data "
      "FROM object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TABLE object_data ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "key_value DEFAULT NULL, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, key_value), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
      "CASCADE"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO object_data "
      "SELECT id, object_store_id, key_value, data "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We need to add a unique constraint to our ai_object_data table. Copy all
  // the data out of it using a temporary table as before.
  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id, "
      "data "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, data "
      "FROM ai_object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE ai_object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TABLE ai_object_data ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "object_store_id INTEGER NOT NULL, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, id), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
      "CASCADE"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO ai_object_data "
      "SELECT id, object_store_id, data "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
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
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT OR IGNORE INTO index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE INDEX index_data_object_data_id_index "
      "ON index_data (object_data_id);"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the unique_index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
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
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO unique_index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE INDEX unique_index_data_object_data_id_index "
      "ON unique_index_data (object_data_id);"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the ai_index_data table. We're reordering the columns as well as
  // changing the primary key from being a simple id to being a composite.
  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "ai_object_data_id "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, value, ai_object_data_id "
      "FROM ai_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE ai_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TABLE ai_index_data ("
      "index_id INTEGER NOT NULL, "
      "value NOT NULL, "
      "ai_object_data_id INTEGER NOT NULL, "
      "PRIMARY KEY (index_id, value, ai_object_data_id), "
      "FOREIGN KEY (index_id) REFERENCES object_store_index(id) ON DELETE "
      "CASCADE, "
      "FOREIGN KEY (ai_object_data_id) REFERENCES ai_object_data(id) ON DELETE "
      "CASCADE"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT OR IGNORE INTO ai_index_data "
      "SELECT index_id, value, ai_object_data_id "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE INDEX ai_index_data_ai_object_data_id_index "
      "ON ai_index_data (ai_object_data_id);"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fix up the ai_unique_index_data table. We're reordering the columns as well
  // as changing the primary key from being a simple id to being a composite.
  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "ai_object_data_id "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, value, ai_object_data_id "
      "FROM ai_unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE ai_unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
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
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO ai_unique_index_data "
      "SELECT index_id, value, ai_object_data_id "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE INDEX ai_unique_index_data_ai_object_data_id_index "
      "ON ai_unique_index_data (ai_object_data_id);"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(6);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom6To7(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom6To7", DOM);

  nsresult rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "id, "
      "name, "
      "key_path, "
      "auto_increment"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT id, name, key_path, auto_increment "
      "FROM object_store;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE object_store;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TABLE object_store ("
      "id INTEGER PRIMARY KEY, "
      "auto_increment INTEGER NOT NULL DEFAULT 0, "
      "name TEXT NOT NULL, "
      "key_path TEXT, "
      "UNIQUE (name)"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO object_store "
      "SELECT id, auto_increment, name, nullif(key_path, '') "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(7);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom7To8(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom7To8", DOM);

  nsresult rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "id, "
      "object_store_id, "
      "name, "
      "key_path, "
      "unique_index, "
      "object_store_autoincrement"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, object_store_autoincrement "
      "FROM object_store_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE object_store_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
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
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO object_store_index "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, 0, object_store_autoincrement "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(8);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class CompressDataBlobsFunction final : public mozIStorageFunction {
 public:
  NS_DECL_ISUPPORTS

 private:
  ~CompressDataBlobsFunction() = default;

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override {
    MOZ_ASSERT(aArguments);
    MOZ_ASSERT(aResult);

    AUTO_PROFILER_LABEL("CompressDataBlobsFunction::OnFunctionCall", DOM);

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
    UniqueFreePtr<uint8_t> compressed(
        static_cast<uint8_t*>(malloc(compressedLength)));
    if (NS_WARN_IF(!compressed)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    snappy::RawCompress(
        reinterpret_cast<const char*>(uncompressed), uncompressedLength,
        reinterpret_cast<char*>(compressed.get()), &compressedLength);

    std::pair<uint8_t*, int> data(compressed.release(), int(compressedLength));

    nsCOMPtr<nsIVariant> result =
        new mozilla::storage::AdoptedBlobVariant(data);

    result.forget(aResult);
    return NS_OK;
  }
};

nsresult UpgradeSchemaFrom8To9_0(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom8To9_0", DOM);

  // We no longer use the dataVersion column.
  nsresult rv =
      aConnection.ExecuteSimpleSQL("UPDATE database SET dataVersion = 0;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageFunction> compressor = new CompressDataBlobsFunction();

  constexpr auto compressorName = "compress"_ns;

  rv = aConnection.CreateFunction(compressorName, 1, compressor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Turn off foreign key constraints before we do anything here.
  rv = aConnection.ExecuteSimpleSQL(
      "UPDATE object_data SET data = compress(data);"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "UPDATE ai_object_data SET data = compress(data);"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.RemoveFunction(compressorName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(9, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom9_0To10_0(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom9_0To10_0", DOM);

  nsresult rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE object_data ADD COLUMN file_ids TEXT;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE ai_object_data ADD COLUMN file_ids TEXT;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateFileTables(aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(10, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom10_0To11_0(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom10_0To11_0", DOM);

  nsresult rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "id, "
      "object_store_id, "
      "name, "
      "key_path, "
      "unique_index, "
      "multientry"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, multientry "
      "FROM object_store_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE object_store_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
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
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO object_store_index "
      "SELECT id, object_store_id, name, key_path, "
      "unique_index, multientry "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "DROP TRIGGER object_data_insert_trigger;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO object_data (object_store_id, key_value, data, file_ids) "
      "SELECT object_store_id, id, data, file_ids "
      "FROM ai_object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TRIGGER object_data_insert_trigger "
      "AFTER INSERT ON object_data "
      "FOR EACH ROW "
      "WHEN NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO index_data (index_id, value, object_data_key, "
      "object_data_id) "
      "SELECT ai_index_data.index_id, ai_index_data.value, "
      "ai_index_data.ai_object_data_id, object_data.id "
      "FROM ai_index_data "
      "INNER JOIN object_store_index ON "
      "object_store_index.id = ai_index_data.index_id "
      "INNER JOIN object_data ON "
      "object_data.object_store_id = object_store_index.object_store_id AND "
      "object_data.key_value = ai_index_data.ai_object_data_id;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO unique_index_data (index_id, value, object_data_key, "
      "object_data_id) "
      "SELECT ai_unique_index_data.index_id, ai_unique_index_data.value, "
      "ai_unique_index_data.ai_object_data_id, object_data.id "
      "FROM ai_unique_index_data "
      "INNER JOIN object_store_index ON "
      "object_store_index.id = ai_unique_index_data.index_id "
      "INNER JOIN object_data ON "
      "object_data.object_store_id = object_store_index.object_store_id AND "
      "object_data.key_value = ai_unique_index_data.ai_object_data_id;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "UPDATE object_store "
      "SET auto_increment = (SELECT max(id) FROM ai_object_data) + 1 "
      "WHERE auto_increment;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE ai_unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE ai_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE ai_object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(11, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class EncodeKeysFunction final : public mozIStorageFunction {
 public:
  NS_DECL_ISUPPORTS

 private:
  ~EncodeKeysFunction() = default;

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override {
    MOZ_ASSERT(aArguments);
    MOZ_ASSERT(aResult);

    AUTO_PROFILER_LABEL("EncodeKeysFunction::OnFunctionCall", DOM);

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

    QM_TRY_INSPECT(
        const auto& key, ([type, aArguments]() -> Result<Key, nsresult> {
          switch (type) {
            case mozIStorageStatement::VALUE_TYPE_INTEGER: {
              int64_t intKey;
              aArguments->GetInt64(0, &intKey);

              Key key;
              QM_TRY(key.SetFromInteger(intKey));

              return key;
            }
            case mozIStorageStatement::VALUE_TYPE_TEXT: {
              nsString stringKey;
              aArguments->GetString(0, stringKey);

              Key key;
              QM_TRY(key.SetFromString(stringKey));

              return key;
            }
            default:
              NS_WARNING("Don't call me with the wrong type of arguments!");
              return Err(NS_ERROR_UNEXPECTED);
          }
        }()));

    const nsCString& buffer = key.GetBuffer();

    std::pair<const void*, int> data(static_cast<const void*>(buffer.get()),
                                     int(buffer.Length()));

    nsCOMPtr<nsIVariant> result = new mozilla::storage::BlobVariant(data);

    result.forget(aResult);
    return NS_OK;
  }
};

nsresult UpgradeSchemaFrom11_0To12_0(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom11_0To12_0", DOM);

  constexpr auto encoderName = "encode"_ns;

  nsCOMPtr<mozIStorageFunction> encoder = new EncodeKeysFunction();

  nsresult rv = aConnection.CreateFunction(encoderName, 1, encoder);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id, "
      "key_value, "
      "data, "
      "file_ids "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT id, object_store_id, encode(key_value), data, file_ids "
      "FROM object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TABLE object_data ("
      "id INTEGER PRIMARY KEY, "
      "object_store_id INTEGER NOT NULL, "
      "key_value BLOB DEFAULT NULL, "
      "file_ids TEXT, "
      "data BLOB NOT NULL, "
      "UNIQUE (object_store_id, key_value), "
      "FOREIGN KEY (object_store_id) REFERENCES object_store(id) ON DELETE "
      "CASCADE"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO object_data "
      "SELECT id, object_store_id, key_value, file_ids, data "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TRIGGER object_data_insert_trigger "
      "AFTER INSERT ON object_data "
      "FOR EACH ROW "
      "WHEN NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TRIGGER object_data_update_trigger "
      "AFTER UPDATE OF file_ids ON object_data "
      "FOR EACH ROW "
      "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TRIGGER object_data_delete_trigger "
      "AFTER DELETE ON object_data "
      "FOR EACH ROW WHEN OLD.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NULL); "
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, encode(value), encode(object_data_key), object_data_id "
      "FROM index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
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
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE INDEX index_data_object_data_id_index "
      "ON index_data (object_data_id);"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TABLE temp_upgrade ("
      "index_id, "
      "value, "
      "object_data_key, "
      "object_data_id "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO temp_upgrade "
      "SELECT index_id, encode(value), encode(object_data_key), object_data_id "
      "FROM unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
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
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO unique_index_data "
      "SELECT index_id, value, object_data_key, object_data_id "
      "FROM temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE temp_upgrade;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE INDEX unique_index_data_object_data_id_index "
      "ON unique_index_data (object_data_id);"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.RemoveFunction(encoderName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(12, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom12_0To13_0(mozIStorageConnection& aConnection,
                                     bool* aVacuumNeeded) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom12_0To13_0", DOM);

  nsresult rv;

#ifdef IDB_MOBILE
  int32_t defaultPageSize;
  rv = aConnection.GetDefaultPageSize(&defaultPageSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Enable auto_vacuum mode and update the page size to the platform default.
  nsAutoCString upgradeQuery("PRAGMA auto_vacuum = FULL; PRAGMA page_size = ");
  upgradeQuery.AppendInt(defaultPageSize);

  rv = aConnection.ExecuteSimpleSQL(upgradeQuery);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aVacuumNeeded = true;
#endif

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(13, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom13_0To14_0(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  // The only change between 13 and 14 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection.SetSchemaVersion(MakeSchemaVersion(14, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom14_0To15_0(mozIStorageConnection& aConnection) {
  // The only change between 14 and 15 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection.SetSchemaVersion(MakeSchemaVersion(15, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom15_0To16_0(mozIStorageConnection& aConnection) {
  // The only change between 15 and 16 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection.SetSchemaVersion(MakeSchemaVersion(16, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom16_0To17_0(mozIStorageConnection& aConnection) {
  // The only change between 16 and 17 was a different structured
  // clone format, but it's backwards-compatible.
  nsresult rv = aConnection.SetSchemaVersion(MakeSchemaVersion(17, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class UpgradeSchemaFrom17_0To18_0Helper final {
  class InsertIndexDataValuesFunction;
  class UpgradeKeyFunction;

 public:
  static nsresult DoUpgrade(mozIStorageConnection& aConnection,
                            const nsACString& aOrigin);

 private:
  static nsresult DoUpgradeInternal(mozIStorageConnection& aConnection,
                                    const nsACString& aOrigin);

  UpgradeSchemaFrom17_0To18_0Helper() = delete;
  ~UpgradeSchemaFrom17_0To18_0Helper() = delete;
};

class UpgradeSchemaFrom17_0To18_0Helper::InsertIndexDataValuesFunction final
    : public mozIStorageFunction {
 public:
  InsertIndexDataValuesFunction() = default;

  NS_DECL_ISUPPORTS

 private:
  ~InsertIndexDataValuesFunction() = default;

  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(
    UpgradeSchemaFrom17_0To18_0Helper::InsertIndexDataValuesFunction,
    mozIStorageFunction);

NS_IMETHODIMP
UpgradeSchemaFrom17_0To18_0Helper::InsertIndexDataValuesFunction::
    OnFunctionCall(mozIStorageValueArray* aValues, nsIVariant** _retval) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 4);

    int32_t valueType;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(0, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_NULL ||
               valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(1, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_INTEGER);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(2, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_INTEGER);

    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(3, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);
  }
#endif

  // Read out the previous value. It may be NULL, in which case we'll just end
  // up with an empty array.
  QM_TRY_UNWRAP(auto indexValues, ReadCompressedIndexDataValues(*aValues, 0));

  IndexOrObjectStoreId indexId;
  nsresult rv = aValues->GetInt64(1, &indexId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t unique;
  rv = aValues->GetInt32(2, &unique);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Key value;
  rv = value.SetFromValueArray(aValues, 3);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the array with the new addition.
  if (NS_WARN_IF(!indexValues.InsertElementSorted(
          IndexDataValue(indexId, !!unique, value), fallible))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Compress the array.
  QM_TRY_UNWRAP((auto [indexValuesBlob, indexValuesBlobLength]),
                MakeCompressedIndexDataValues(indexValues));

  // The compressed blob is the result of this function.
  nsCOMPtr<nsIVariant> result = new storage::AdoptedBlobVariant(
      std::pair(indexValuesBlob.release(), indexValuesBlobLength));

  result.forget(_retval);
  return NS_OK;
}

class UpgradeSchemaFrom17_0To18_0Helper::UpgradeKeyFunction final
    : public mozIStorageFunction {
 public:
  UpgradeKeyFunction() = default;

  static nsresult CopyAndUpgradeKeyBuffer(const uint8_t* aSource,
                                          const uint8_t* aSourceEnd,
                                          uint8_t* aDestination) {
    return CopyAndUpgradeKeyBufferInternal(aSource, aSourceEnd, aDestination,
                                           0 /* aTagOffset */,
                                           0 /* aRecursionDepth */);
  }

  NS_DECL_ISUPPORTS

 private:
  ~UpgradeKeyFunction() = default;

  static nsresult CopyAndUpgradeKeyBufferInternal(const uint8_t*& aSource,
                                                  const uint8_t* aSourceEnd,
                                                  uint8_t*& aDestination,
                                                  uint8_t aTagOffset,
                                                  uint8_t aRecursionDepth);

  static uint32_t AdjustedSize(uint32_t aMaxSize, const uint8_t* aSource,
                               const uint8_t* aSourceEnd) {
    MOZ_ASSERT(aMaxSize);
    MOZ_ASSERT(aSource);
    MOZ_ASSERT(aSourceEnd);
    MOZ_ASSERT(aSource <= aSourceEnd);

    return std::min(aMaxSize, uint32_t(aSourceEnd - aSource));
  }

  NS_DECL_MOZISTORAGEFUNCTION
};

// static
nsresult UpgradeSchemaFrom17_0To18_0Helper::UpgradeKeyFunction::
    CopyAndUpgradeKeyBufferInternal(const uint8_t*& aSource,
                                    const uint8_t* aSourceEnd,
                                    uint8_t*& aDestination, uint8_t aTagOffset,
                                    uint8_t aRecursionDepth) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aSource);
  MOZ_ASSERT(*aSource);
  MOZ_ASSERT(aSourceEnd);
  MOZ_ASSERT(aSource < aSourceEnd);
  MOZ_ASSERT(aDestination);
  MOZ_ASSERT(aTagOffset <= Key::kMaxArrayCollapse);

  static constexpr uint8_t kOldNumberTag = 0x1;
  static constexpr uint8_t kOldDateTag = 0x2;
  static constexpr uint8_t kOldStringTag = 0x3;
  static constexpr uint8_t kOldArrayTag = 0x4;
  static constexpr uint8_t kOldMaxType = kOldArrayTag;

  if (NS_WARN_IF(aRecursionDepth > Key::kMaxRecursionDepth)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  const uint8_t sourceTag = *aSource - (aTagOffset * kOldMaxType);
  MOZ_ASSERT(sourceTag);

  if (NS_WARN_IF(sourceTag > kOldMaxType * Key::kMaxArrayCollapse)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  if (sourceTag == kOldNumberTag || sourceTag == kOldDateTag) {
    // Write the new tag.
    *aDestination++ = (sourceTag == kOldNumberTag ? Key::eFloat : Key::eDate) +
                      (aTagOffset * Key::eMaxType);
    aSource++;

    // Numbers and Dates are encoded as 64-bit integers, but trailing 0
    // bytes have been removed.
    const uint32_t byteCount =
        AdjustedSize(sizeof(uint64_t), aSource, aSourceEnd);

    aDestination = std::copy(aSource, aSource + byteCount, aDestination);
    aSource += byteCount;

    return NS_OK;
  }

  if (sourceTag == kOldStringTag) {
    // Write the new tag.
    *aDestination++ = Key::eString + (aTagOffset * Key::eMaxType);
    aSource++;

    while (aSource < aSourceEnd) {
      const uint8_t byte = *aSource++;
      *aDestination++ = byte;

      if (!byte) {
        // Just copied the terminator.
        break;
      }

      // Maybe copy one or two extra bytes if the byte is tagged and we have
      // enough source space.
      if (byte & 0x80) {
        const uint32_t byteCount =
            AdjustedSize((byte & 0x40) ? 2 : 1, aSource, aSourceEnd);

        aDestination = std::copy(aSource, aSource + byteCount, aDestination);
        aSource += byteCount;
      }
    }

    return NS_OK;
  }

  if (NS_WARN_IF(sourceTag < kOldArrayTag)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_FILE_CORRUPTED;
  }

  aTagOffset++;

  if (aTagOffset == Key::kMaxArrayCollapse) {
    MOZ_ASSERT(sourceTag == kOldArrayTag);

    *aDestination++ = (aTagOffset * Key::eMaxType);
    aSource++;

    aTagOffset = 0;
  }

  while (aSource < aSourceEnd &&
         (*aSource - (aTagOffset * kOldMaxType)) != Key::eTerminator) {
    nsresult rv = CopyAndUpgradeKeyBufferInternal(
        aSource, aSourceEnd, aDestination, aTagOffset, aRecursionDepth + 1);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aTagOffset = 0;
  }

  if (aSource < aSourceEnd) {
    MOZ_ASSERT((*aSource - (aTagOffset * kOldMaxType)) == Key::eTerminator);
    *aDestination++ = Key::eTerminator + (aTagOffset * Key::eMaxType);
    aSource++;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(UpgradeSchemaFrom17_0To18_0Helper::UpgradeKeyFunction,
                  mozIStorageFunction);

NS_IMETHODIMP
UpgradeSchemaFrom17_0To18_0Helper::UpgradeKeyFunction::OnFunctionCall(
    mozIStorageValueArray* aValues, nsIVariant** _retval) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());
  MOZ_ASSERT(aValues);
  MOZ_ASSERT(_retval);

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 1);

    int32_t valueType;
    MOZ_ALWAYS_SUCCEEDS(aValues->GetTypeOfIndex(0, &valueType));
    MOZ_ASSERT(valueType == mozIStorageValueArray::VALUE_TYPE_BLOB);
  }
#endif

  // Dig the old key out of the values.
  const uint8_t* blobData;
  uint32_t blobDataLength;
  nsresult rv = aValues->GetSharedBlob(0, &blobDataLength, &blobData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Upgrading the key doesn't change the amount of space needed to hold it.
  UniqueFreePtr<uint8_t> upgradedBlobData(
      static_cast<uint8_t*>(malloc(blobDataLength)));
  if (NS_WARN_IF(!upgradedBlobData)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = CopyAndUpgradeKeyBuffer(blobData, blobData + blobDataLength,
                               upgradedBlobData.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The upgraded key is the result of this function.
  std::pair<uint8_t*, int> data(upgradedBlobData.release(),
                                int(blobDataLength));

  nsCOMPtr<nsIVariant> result = new mozilla::storage::AdoptedBlobVariant(data);

  result.forget(_retval);
  return NS_OK;
}

// static
nsresult UpgradeSchemaFrom17_0To18_0Helper::DoUpgrade(
    mozIStorageConnection& aConnection, const nsACString& aOrigin) {
  MOZ_ASSERT(!aOrigin.IsEmpty());

  // Register the |upgrade_key| function.
  RefPtr<UpgradeKeyFunction> updateFunction = new UpgradeKeyFunction();

  constexpr auto upgradeKeyFunctionName = "upgrade_key"_ns;

  nsresult rv =
      aConnection.CreateFunction(upgradeKeyFunctionName, 1, updateFunction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Register the |insert_idv| function.
  RefPtr<InsertIndexDataValuesFunction> insertIDVFunction =
      new InsertIndexDataValuesFunction();

  constexpr auto insertIDVFunctionName = "insert_idv"_ns;

  rv = aConnection.CreateFunction(insertIDVFunctionName, 4, insertIDVFunction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_ALWAYS_SUCCEEDS(aConnection.RemoveFunction(upgradeKeyFunctionName));
    return rv;
  }

  rv = DoUpgradeInternal(aConnection, aOrigin);

  MOZ_ALWAYS_SUCCEEDS(aConnection.RemoveFunction(upgradeKeyFunctionName));
  MOZ_ALWAYS_SUCCEEDS(aConnection.RemoveFunction(insertIDVFunctionName));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
nsresult UpgradeSchemaFrom17_0To18_0Helper::DoUpgradeInternal(
    mozIStorageConnection& aConnection, const nsACString& aOrigin) {
  MOZ_ASSERT(!aOrigin.IsEmpty());

  nsresult rv = aConnection.ExecuteSimpleSQL(
      // Drop these triggers to avoid unnecessary work during the upgrade
      // process.
      "DROP TRIGGER object_data_insert_trigger;"
      "DROP TRIGGER object_data_update_trigger;"
      "DROP TRIGGER object_data_delete_trigger;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // Drop these indexes before we do anything else to free disk space.
      "DROP INDEX index_data_object_data_id_index;"
      "DROP INDEX unique_index_data_object_data_id_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Create the new tables and triggers first.

  rv = aConnection.ExecuteSimpleSQL(
      // This will eventually become the |database| table.
      "CREATE TABLE database_upgrade "
      "( name TEXT PRIMARY KEY"
      ", origin TEXT NOT NULL"
      ", version INTEGER NOT NULL DEFAULT 0"
      ", last_vacuum_time INTEGER NOT NULL DEFAULT 0"
      ", last_analyze_time INTEGER NOT NULL DEFAULT 0"
      ", last_vacuum_size INTEGER NOT NULL DEFAULT 0"
      ") WITHOUT ROWID;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // This will eventually become the |object_store| table.
      "CREATE TABLE object_store_upgrade"
      "( id INTEGER PRIMARY KEY"
      ", auto_increment INTEGER NOT NULL DEFAULT 0"
      ", name TEXT NOT NULL"
      ", key_path TEXT"
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // This will eventually become the |object_store_index| table.
      "CREATE TABLE object_store_index_upgrade"
      "( id INTEGER PRIMARY KEY"
      ", object_store_id INTEGER NOT NULL"
      ", name TEXT NOT NULL"
      ", key_path TEXT NOT NULL"
      ", unique_index INTEGER NOT NULL"
      ", multientry INTEGER NOT NULL"
      ", FOREIGN KEY (object_store_id) "
      "REFERENCES object_store(id) "
      ");"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // This will eventually become the |object_data| table.
      "CREATE TABLE object_data_upgrade"
      "( object_store_id INTEGER NOT NULL"
      ", key BLOB NOT NULL"
      ", index_data_values BLOB DEFAULT NULL"
      ", file_ids TEXT"
      ", data BLOB NOT NULL"
      ", PRIMARY KEY (object_store_id, key)"
      ", FOREIGN KEY (object_store_id) "
      "REFERENCES object_store(id) "
      ") WITHOUT ROWID;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // This will eventually become the |index_data| table.
      "CREATE TABLE index_data_upgrade"
      "( index_id INTEGER NOT NULL"
      ", value BLOB NOT NULL"
      ", object_data_key BLOB NOT NULL"
      ", object_store_id INTEGER NOT NULL"
      ", PRIMARY KEY (index_id, value, object_data_key)"
      ", FOREIGN KEY (index_id) "
      "REFERENCES object_store_index(id) "
      ", FOREIGN KEY (object_store_id, object_data_key) "
      "REFERENCES object_data(object_store_id, key) "
      ") WITHOUT ROWID;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // This will eventually become the |unique_index_data| table.
      "CREATE TABLE unique_index_data_upgrade"
      "( index_id INTEGER NOT NULL"
      ", value BLOB NOT NULL"
      ", object_store_id INTEGER NOT NULL"
      ", object_data_key BLOB NOT NULL"
      ", PRIMARY KEY (index_id, value)"
      ", FOREIGN KEY (index_id) "
      "REFERENCES object_store_index(id) "
      ", FOREIGN KEY (object_store_id, object_data_key) "
      "REFERENCES object_data(object_store_id, key) "
      ") WITHOUT ROWID;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // Temporarily store |index_data_values| that we build during the upgrade
      // of the index tables. We will later move this to the |object_data|
      // table.
      "CREATE TEMPORARY TABLE temp_index_data_values "
      "( object_store_id INTEGER NOT NULL"
      ", key BLOB NOT NULL"
      ", index_data_values BLOB DEFAULT NULL"
      ", PRIMARY KEY (object_store_id, key)"
      ") WITHOUT ROWID;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // These two triggers help build the |index_data_values| blobs. The nested
      // SELECT statements help us achieve an "INSERT OR UPDATE"-like behavior.
      "CREATE TEMPORARY TRIGGER unique_index_data_upgrade_insert_trigger "
      "AFTER INSERT ON unique_index_data_upgrade "
      "BEGIN "
      "INSERT OR REPLACE INTO temp_index_data_values "
      "VALUES "
      "( NEW.object_store_id"
      ", NEW.object_data_key"
      ", insert_idv("
      "( SELECT index_data_values "
      "FROM temp_index_data_values "
      "WHERE object_store_id = NEW.object_store_id "
      "AND key = NEW.object_data_key "
      "), NEW.index_id"
      ", 1" /* unique */
      ", NEW.value"
      ")"
      ");"
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TEMPORARY TRIGGER index_data_upgrade_insert_trigger "
      "AFTER INSERT ON index_data_upgrade "
      "BEGIN "
      "INSERT OR REPLACE INTO temp_index_data_values "
      "VALUES "
      "( NEW.object_store_id"
      ", NEW.object_data_key"
      ", insert_idv("
      "("
      "SELECT index_data_values "
      "FROM temp_index_data_values "
      "WHERE object_store_id = NEW.object_store_id "
      "AND key = NEW.object_data_key "
      "), NEW.index_id"
      ", 0" /* not unique */
      ", NEW.value"
      ")"
      ");"
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |unique_index_data| table to change the column order, remove the
  // ON DELETE CASCADE clauses, and to apply the WITHOUT ROWID optimization.
  rv = aConnection.ExecuteSimpleSQL(
      // Insert all the data.
      "INSERT INTO unique_index_data_upgrade "
      "SELECT "
      "unique_index_data.index_id, "
      "upgrade_key(unique_index_data.value), "
      "object_data.object_store_id, "
      "upgrade_key(unique_index_data.object_data_key) "
      "FROM unique_index_data "
      "JOIN object_data "
      "ON unique_index_data.object_data_id = object_data.id;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // The trigger is no longer needed.
      "DROP TRIGGER unique_index_data_upgrade_insert_trigger;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // The old table is no longer needed.
      "DROP TABLE unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // Rename the table.
      "ALTER TABLE unique_index_data_upgrade "
      "RENAME TO unique_index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |index_data| table to change the column order, remove the ON
  // DELETE CASCADE clauses, and to apply the WITHOUT ROWID optimization.
  rv = aConnection.ExecuteSimpleSQL(
      // Insert all the data.
      "INSERT INTO index_data_upgrade "
      "SELECT "
      "index_data.index_id, "
      "upgrade_key(index_data.value), "
      "upgrade_key(index_data.object_data_key), "
      "object_data.object_store_id "
      "FROM index_data "
      "JOIN object_data "
      "ON index_data.object_data_id = object_data.id;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // The trigger is no longer needed.
      "DROP TRIGGER index_data_upgrade_insert_trigger;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // The old table is no longer needed.
      "DROP TABLE index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // Rename the table.
      "ALTER TABLE index_data_upgrade "
      "RENAME TO index_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |object_data| table to add the |index_data_values| column,
  // remove the ON DELETE CASCADE clause, and apply the WITHOUT ROWID
  // optimization.
  rv = aConnection.ExecuteSimpleSQL(
      // Insert all the data.
      "INSERT INTO object_data_upgrade "
      "SELECT "
      "object_data.object_store_id, "
      "upgrade_key(object_data.key_value), "
      "temp_index_data_values.index_data_values, "
      "object_data.file_ids, "
      "object_data.data "
      "FROM object_data "
      "LEFT JOIN temp_index_data_values "
      "ON object_data.object_store_id = "
      "temp_index_data_values.object_store_id "
      "AND upgrade_key(object_data.key_value) = "
      "temp_index_data_values.key;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // The temporary table is no longer needed.
      "DROP TABLE temp_index_data_values;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // The old table is no longer needed.
      "DROP TABLE object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      // Rename the table.
      "ALTER TABLE object_data_upgrade "
      "RENAME TO object_data;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |object_store_index| table to remove the UNIQUE constraint and
  // the ON DELETE CASCADE clause.
  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO object_store_index_upgrade "
      "SELECT * "
      "FROM object_store_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE object_store_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE object_store_index_upgrade "
      "RENAME TO object_store_index;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |object_store| table to remove the UNIQUE constraint.
  rv = aConnection.ExecuteSimpleSQL(
      "INSERT INTO object_store_upgrade "
      "SELECT * "
      "FROM object_store;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE object_store;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE object_store_upgrade "
      "RENAME TO object_store;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update the |database| table to include the origin, vacuum information, and
  // apply the WITHOUT ROWID optimization.
  nsCOMPtr<mozIStorageStatement> stmt;

  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  rv = aConnection.CreateStatement(
      "INSERT INTO database_upgrade "
      "SELECT name, :origin, version, 0, 0, 0 "
      "FROM database;"_ns,
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindUTF8StringByIndex(0, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL("DROP TABLE database;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE database_upgrade "
      "RENAME TO database;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  {
    // Make sure there's only one entry in the |database| table.
    QM_TRY_INSPECT(const auto& stmt,
                   quota::CreateAndExecuteSingleStepStatement(
                       aConnection, "SELECT COUNT(*) FROM database;"_ns),
                   QM_ASSERT_UNREACHABLE);

    int64_t count;
    MOZ_ASSERT(NS_SUCCEEDED(stmt->GetInt64(0, &count)));

    MOZ_ASSERT(count == 1);
  }
#endif

  // Recreate file table triggers.
  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TRIGGER object_data_insert_trigger "
      "AFTER INSERT ON object_data "
      "WHEN NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids);"
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TRIGGER object_data_update_trigger "
      "AFTER UPDATE OF file_ids ON object_data "
      "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids);"
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TRIGGER object_data_delete_trigger "
      "AFTER DELETE ON object_data "
      "WHEN OLD.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NULL);"
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Finally, turn on auto_vacuum mode. We use full auto_vacuum mode to reclaim
  // disk space on mobile devices (at the cost of some COMMIT speed), and
  // incremental auto_vacuum mode on desktop builds.
  rv = aConnection.ExecuteSimpleSQL(
#ifdef IDB_MOBILE
      "PRAGMA auto_vacuum = FULL;"_ns
#else
      "PRAGMA auto_vacuum = INCREMENTAL;"_ns
#endif
  );
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(18, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom17_0To18_0(mozIStorageConnection& aConnection,
                                     const nsACString& aOrigin) {
  MOZ_ASSERT(!aOrigin.IsEmpty());

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom17_0To18_0", DOM);

  return UpgradeSchemaFrom17_0To18_0Helper::DoUpgrade(aConnection, aOrigin);
}

nsresult UpgradeSchemaFrom18_0To19_0(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  nsresult rv;
  AUTO_PROFILER_LABEL("UpgradeSchemaFrom18_0To19_0", DOM);

  rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE object_store_index "
      "ADD COLUMN locale TEXT;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE object_store_index "
      "ADD COLUMN is_auto_locale BOOLEAN;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE index_data "
      "ADD COLUMN value_locale BLOB;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "ALTER TABLE unique_index_data "
      "ADD COLUMN value_locale BLOB;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE INDEX index_data_value_locale_index "
      "ON index_data (index_id, value_locale, object_data_key, value) "
      "WHERE value_locale IS NOT NULL;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "CREATE INDEX unique_index_data_value_locale_index "
      "ON unique_index_data (index_id, value_locale, object_data_key, value) "
      "WHERE value_locale IS NOT NULL;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(19, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class UpgradeFileIdsFunction final : public mozIStorageFunction {
  SafeRefPtr<DatabaseFileManager> mFileManager;

 public:
  UpgradeFileIdsFunction() { AssertIsOnIOThread(); }

  nsresult Init(nsIFile* aFMDirectory, mozIStorageConnection& aConnection);

  NS_DECL_ISUPPORTS

 private:
  ~UpgradeFileIdsFunction() {
    AssertIsOnIOThread();

    if (mFileManager) {
      mFileManager->Invalidate();
    }
  }

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override;
};

nsresult UpgradeSchemaFrom19_0To20_0(nsIFile* aFMDirectory,
                                     mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom19_0To20_0", DOM);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection.CreateStatement(
      "SELECT count(*) "
      "FROM object_data "
      "WHERE file_ids IS NOT NULL"_ns,
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t count;

  {
    mozStorageStatementScoper scoper(stmt);

    QM_TRY_INSPECT(const bool& hasResult,
                   MOZ_TO_RESULT_INVOKE_MEMBER(stmt, ExecuteStep));

    if (NS_WARN_IF(!hasResult)) {
      MOZ_ASSERT(false, "This should never be possible!");
      return NS_ERROR_FAILURE;
    }

    count = stmt->AsInt64(0);
    if (NS_WARN_IF(count < 0)) {
      MOZ_ASSERT(false, "This should never be possible!");
      return NS_ERROR_FAILURE;
    }
  }

  if (count == 0) {
    // Nothing to upgrade.
    rv = aConnection.SetSchemaVersion(MakeSchemaVersion(20, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  RefPtr<UpgradeFileIdsFunction> function = new UpgradeFileIdsFunction();

  rv = function->Init(aFMDirectory, aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  constexpr auto functionName = "upgrade"_ns;

  rv = aConnection.CreateFunction(functionName, 2, function);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Disable update trigger.
  rv = aConnection.ExecuteSimpleSQL(
      "DROP TRIGGER object_data_update_trigger;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "UPDATE object_data "
      "SET file_ids = upgrade(file_ids, data) "
      "WHERE file_ids IS NOT NULL;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Enable update trigger.
  rv = aConnection.ExecuteSimpleSQL(
      "CREATE TRIGGER object_data_update_trigger "
      "AFTER UPDATE OF file_ids ON object_data "
      "FOR EACH ROW "
      "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
      "END;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.RemoveFunction(functionName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(20, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class UpgradeIndexDataValuesFunction final : public mozIStorageFunction {
 public:
  UpgradeIndexDataValuesFunction() { AssertIsOnIOThread(); }

  NS_DECL_ISUPPORTS

 private:
  ~UpgradeIndexDataValuesFunction() { AssertIsOnIOThread(); }

  using IndexDataValuesArray = IndexDataValuesAutoArray;
  Result<IndexDataValuesArray, nsresult> ReadOldCompressedIDVFromBlob(
      Span<const uint8_t> aBlobData);

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override;
};

NS_IMPL_ISUPPORTS(UpgradeIndexDataValuesFunction, mozIStorageFunction)

Result<UpgradeIndexDataValuesFunction::IndexDataValuesArray, nsresult>
UpgradeIndexDataValuesFunction::ReadOldCompressedIDVFromBlob(
    const Span<const uint8_t> aBlobData) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!IsOnBackgroundThread());

  IndexOrObjectStoreId indexId;
  bool unique;
  bool nextIndexIdAlreadyRead = false;

  IndexDataValuesArray result;
  for (auto remainder = aBlobData; !remainder.IsEmpty();) {
    if (!nextIndexIdAlreadyRead) {
      QM_TRY_UNWRAP((std::tie(indexId, unique, remainder)),
                    ReadCompressedIndexId(remainder));
    }
    nextIndexIdAlreadyRead = false;

    if (NS_WARN_IF(remainder.IsEmpty())) {
      IDB_REPORT_INTERNAL_ERR();
      return Err(NS_ERROR_FILE_CORRUPTED);
    }

    // Read key buffer length.
    QM_TRY_INSPECT(
        (const auto& [keyBufferLength, remainderAfterKeyBufferLength]),
        ReadCompressedNumber(remainder));

    if (NS_WARN_IF(remainderAfterKeyBufferLength.IsEmpty()) ||
        NS_WARN_IF(keyBufferLength > uint64_t(UINT32_MAX)) ||
        NS_WARN_IF(keyBufferLength > remainderAfterKeyBufferLength.Length())) {
      IDB_REPORT_INTERNAL_ERR();
      return Err(NS_ERROR_FILE_CORRUPTED);
    }

    const auto [keyBuffer, remainderAfterKeyBuffer] =
        remainderAfterKeyBufferLength.SplitAt(keyBufferLength);
    if (NS_WARN_IF(!result.EmplaceBack(fallible, indexId, unique,
                                       Key{nsCString{AsChars(keyBuffer)}}))) {
      IDB_REPORT_INTERNAL_ERR();
      return Err(NS_ERROR_OUT_OF_MEMORY);
    }

    remainder = remainderAfterKeyBuffer;
    if (!remainder.IsEmpty()) {
      // Read either a sort key buffer length or an index id.
      QM_TRY_INSPECT((const auto& [maybeIndexId, remainderAfterIndexId]),
                     ReadCompressedNumber(remainder));

      // Locale-aware indexes haven't been around long enough to have any users,
      // we can safely assume all sort key buffer lengths will be zero.
      // XXX This duplicates logic from ReadCompressedIndexId.
      if (maybeIndexId != 0) {
        unique = maybeIndexId % 2 == 1;
        indexId = maybeIndexId / 2;
        nextIndexIdAlreadyRead = true;
      }

      remainder = remainderAfterIndexId;
    }
  }
  result.Sort();

  return result;
}

NS_IMETHODIMP
UpgradeIndexDataValuesFunction::OnFunctionCall(
    mozIStorageValueArray* aArguments, nsIVariant** aResult) {
  MOZ_ASSERT(aArguments);
  MOZ_ASSERT(aResult);

  AUTO_PROFILER_LABEL("UpgradeIndexDataValuesFunction::OnFunctionCall", DOM);

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

  const uint8_t* oldBlob;
  uint32_t oldBlobLength;
  rv = aArguments->GetSharedBlob(0, &oldBlobLength, &oldBlob);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  QM_TRY_INSPECT(const auto& oldIdv,
                 ReadOldCompressedIDVFromBlob(Span(oldBlob, oldBlobLength)));

  QM_TRY_UNWRAP((auto [newIdv, newIdvLength]),
                MakeCompressedIndexDataValues(oldIdv));

  nsCOMPtr<nsIVariant> result = new storage::AdoptedBlobVariant(
      std::pair(newIdv.release(), newIdvLength));

  result.forget(aResult);
  return NS_OK;
}

nsresult UpgradeSchemaFrom20_0To21_0(mozIStorageConnection& aConnection) {
  // This should have been part of the 18 to 19 upgrade, where we changed the
  // layout of the index_data_values blobs but didn't upgrade the existing data.
  // See bug 1202788.

  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom20_0To21_0", DOM);

  RefPtr<UpgradeIndexDataValuesFunction> function =
      new UpgradeIndexDataValuesFunction();

  constexpr auto functionName = "upgrade_idv"_ns;

  nsresult rv = aConnection.CreateFunction(functionName, 1, function);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "UPDATE object_data "
      "SET index_data_values = upgrade_idv(index_data_values) "
      "WHERE index_data_values IS NOT NULL;"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.RemoveFunction(functionName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(21, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom21_0To22_0(mozIStorageConnection& aConnection) {
  // The only change between 21 and 22 was a different structured clone format,
  // but it's backwards-compatible.
  nsresult rv = aConnection.SetSchemaVersion(MakeSchemaVersion(22, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom22_0To23_0(mozIStorageConnection& aConnection,
                                     const nsACString& aOrigin) {
  AssertIsOnIOThread();

  MOZ_ASSERT(!aOrigin.IsEmpty());

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom22_0To23_0", DOM);

  nsCOMPtr<mozIStorageStatement> stmt;
  // The parameter names are not used, parameters are bound by index only
  // locally in the same function.
  nsresult rv = aConnection.CreateStatement(
      "UPDATE database SET origin = :origin;"_ns, getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindUTF8StringByIndex(0, aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(23, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom23_0To24_0(mozIStorageConnection& aConnection) {
  // The only change between 23 and 24 was a different structured clone format,
  // but it's backwards-compatible.
  nsresult rv = aConnection.SetSchemaVersion(MakeSchemaVersion(24, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom24_0To25_0(mozIStorageConnection& aConnection) {
  // The changes between 24 and 25 were an upgraded snappy library, a different
  // structured clone format and a different file_ds format. But everything is
  // backwards-compatible.
  nsresult rv = aConnection.SetSchemaVersion(MakeSchemaVersion(25, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class StripObsoleteOriginAttributesFunction final : public mozIStorageFunction {
 public:
  NS_DECL_ISUPPORTS

 private:
  ~StripObsoleteOriginAttributesFunction() = default;

  NS_IMETHOD
  OnFunctionCall(mozIStorageValueArray* aArguments,
                 nsIVariant** aResult) override {
    MOZ_ASSERT(aArguments);
    MOZ_ASSERT(aResult);

    AUTO_PROFILER_LABEL("StripObsoleteOriginAttributesFunction::OnFunctionCall",
                        DOM);

#ifdef DEBUG
    {
      uint32_t argCount;
      MOZ_ALWAYS_SUCCEEDS(aArguments->GetNumEntries(&argCount));
      MOZ_ASSERT(argCount == 1);

      int32_t type;
      MOZ_ALWAYS_SUCCEEDS(aArguments->GetTypeOfIndex(0, &type));
      MOZ_ASSERT(type == mozIStorageValueArray::VALUE_TYPE_TEXT);
    }
#endif

    nsCString origin;
    nsresult rv = aArguments->GetUTF8String(0, origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Deserialize and re-serialize to automatically drop any obsolete origin
    // attributes.
    OriginAttributes oa;

    nsCString originNoSuffix;
    bool ok = oa.PopulateFromOrigin(origin, originNoSuffix);
    if (NS_WARN_IF(!ok)) {
      return NS_ERROR_FAILURE;
    }

    nsCString suffix;
    oa.CreateSuffix(suffix);

    nsCOMPtr<nsIVariant> result =
        new mozilla::storage::UTF8TextVariant(originNoSuffix + suffix);

    result.forget(aResult);
    return NS_OK;
  }
};

nsresult UpgradeSchemaFrom25_0To26_0(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("UpgradeSchemaFrom25_0To26_0", DOM);

  constexpr auto functionName = "strip_obsolete_attributes"_ns;

  nsCOMPtr<mozIStorageFunction> stripObsoleteAttributes =
      new StripObsoleteOriginAttributesFunction();

  nsresult rv = aConnection.CreateFunction(functionName,
                                           /* aNumArguments */ 1,
                                           stripObsoleteAttributes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.ExecuteSimpleSQL(
      "UPDATE DATABASE "
      "SET origin = strip_obsolete_attributes(origin) "
      "WHERE origin LIKE '%^%';"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.RemoveFunction(functionName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection.SetSchemaVersion(MakeSchemaVersion(26, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(CompressDataBlobsFunction, mozIStorageFunction)
NS_IMPL_ISUPPORTS(EncodeKeysFunction, mozIStorageFunction)
NS_IMPL_ISUPPORTS(StripObsoleteOriginAttributesFunction, mozIStorageFunction);

class DeserializeUpgradeValueHelper final : public Runnable {
 public:
  explicit DeserializeUpgradeValueHelper(
      StructuredCloneReadInfoParent& aCloneReadInfo)
      : Runnable("DeserializeUpgradeValueHelper"),
        mMonitor("DeserializeUpgradeValueHelper::mMonitor"),
        mCloneReadInfo(aCloneReadInfo),
        mStatus(NS_ERROR_FAILURE) {}

  nsresult DispatchAndWait(nsAString& aFileIds) {
    // We don't need to go to the main-thread and use the sandbox.
    if (!mCloneReadInfo.Data().Size()) {
      PopulateFileIds(aFileIds);
      return NS_OK;
    }

    // The operation will continue on the main-thread.

    MOZ_ASSERT(!(mCloneReadInfo.Data().Size() % sizeof(uint64_t)));

    MonitorAutoLock lock(mMonitor);

    RefPtr<Runnable> self = this;
    const nsresult rv = SchedulerGroup::Dispatch(self.forget());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    lock.Wait();

    if (NS_FAILED(mStatus)) {
      return mStatus;
    }

    PopulateFileIds(aFileIds);
    return NS_OK;
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();

    JS::Rooted<JSObject*> global(cx, GetSandbox(cx));
    if (NS_WARN_IF(!global)) {
      OperationCompleted(NS_ERROR_FAILURE);
      return NS_OK;
    }

    const JSAutoRealm ar(cx, global);

    JS::Rooted<JS::Value> value(cx);
    const nsresult rv = DeserializeUpgradeValue(cx, &value);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      OperationCompleted(rv);
      return NS_OK;
    }

    OperationCompleted(NS_OK);
    return NS_OK;
  }

 private:
  nsresult DeserializeUpgradeValue(JSContext* aCx,
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

  void PopulateFileIds(nsAString& aFileIds) {
    for (uint32_t count = mCloneReadInfo.Files().Length(), index = 0;
         index < count; index++) {
      const StructuredCloneFileParent& file = mCloneReadInfo.Files()[index];

      const int64_t id = file.FileInfo().Id();

      if (index) {
        aFileIds.Append(' ');
      }
      aFileIds.AppendInt(file.Type() == StructuredCloneFileBase::eBlob ? id
                                                                       : -id);
    }
  }

  void OperationCompleted(nsresult aStatus) {
    mStatus = aStatus;

    MonitorAutoLock lock(mMonitor);
    lock.Notify();
  }

  Monitor mMonitor MOZ_UNANNOTATED;
  StructuredCloneReadInfoParent& mCloneReadInfo;
  nsresult mStatus;
};

nsresult DeserializeUpgradeValueToFileIds(
    StructuredCloneReadInfoParent& aCloneReadInfo, nsAString& aFileIds) {
  MOZ_ASSERT(!NS_IsMainThread());

  const RefPtr<DeserializeUpgradeValueHelper> helper =
      new DeserializeUpgradeValueHelper(aCloneReadInfo);
  return helper->DispatchAndWait(aFileIds);
}

nsresult UpgradeFileIdsFunction::Init(nsIFile* aFMDirectory,
                                      mozIStorageConnection& aConnection) {
  // This DatabaseFileManager doesn't need real origin info, etc. The only
  // purpose is to store file ids without adding more complexity or code
  // duplication.
  auto fileManager = MakeSafeRefPtr<DatabaseFileManager>(
      PERSISTENCE_TYPE_INVALID, quota::OriginMetadata{},
      /* aDatabaseName */ u""_ns, /* aDatabaseID */ ""_ns,
      /* aDatabaseFilePath */ u""_ns, /* aEnforcingQuota */ false,
      /* aIsInPrivateBrowsingMode */ false);

  nsresult rv =
      fileManager->Init(aFMDirectory, /* aDatabaseVersion */ 0, aConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mFileManager = std::move(fileManager);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(UpgradeFileIdsFunction, mozIStorageFunction)

NS_IMETHODIMP
UpgradeFileIdsFunction::OnFunctionCall(mozIStorageValueArray* aArguments,
                                       nsIVariant** aResult) {
  MOZ_ASSERT(aArguments);
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(mFileManager);

  AUTO_PROFILER_LABEL("UpgradeFileIdsFunction::OnFunctionCall", DOM);

  uint32_t argc;
  nsresult rv = aArguments->GetNumEntries(&argc);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (argc != 2) {
    NS_WARNING("Don't call me with the wrong number of arguments!");
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY_UNWRAP(auto cloneInfo, GetStructuredCloneReadInfoFromValueArray(
                                    aArguments, 1, 0, *mFileManager));

  nsAutoString fileIds;
  // XXX does this really need non-const cloneInfo?
  rv = DeserializeUpgradeValueToFileIds(cloneInfo, fileIds);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  nsCOMPtr<nsIVariant> result = new mozilla::storage::TextVariant(fileIds);

  result.forget(aResult);
  return NS_OK;
}

}  // namespace

Result<bool, nsresult> MaybeUpgradeSchema(mozIStorageConnection& aConnection,
                                          const int32_t aSchemaVersion,
                                          nsIFile& aFMDirectory,
                                          const nsACString& aOrigin) {
  bool vacuumNeeded = false;
  int32_t schemaVersion = aSchemaVersion;

  // This logic needs to change next time we change the schema!
  static_assert(kSQLiteSchemaVersion == int32_t((26 << 4) + 0),
                "Upgrade function needed due to schema version increase.");

  while (schemaVersion != kSQLiteSchemaVersion) {
    switch (schemaVersion) {
      case 4:
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom4To5(aConnection)));
        break;
      case 5:
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom5To6(aConnection)));
        break;
      case 6:
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom6To7(aConnection)));
        break;
      case 7:
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom7To8(aConnection)));
        break;
      case 8:
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom8To9_0(aConnection)));
        vacuumNeeded = true;
        break;
      case MakeSchemaVersion(9, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom9_0To10_0(aConnection)));
        break;
      case MakeSchemaVersion(10, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom10_0To11_0(aConnection)));
        break;
      case MakeSchemaVersion(11, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom11_0To12_0(aConnection)));
        break;
      case MakeSchemaVersion(12, 0):
        QM_TRY(MOZ_TO_RESULT(
            UpgradeSchemaFrom12_0To13_0(aConnection, &vacuumNeeded)));
        break;
      case MakeSchemaVersion(13, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom13_0To14_0(aConnection)));
        break;
      case MakeSchemaVersion(14, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom14_0To15_0(aConnection)));
        break;
      case MakeSchemaVersion(15, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom15_0To16_0(aConnection)));
        break;
      case MakeSchemaVersion(16, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom16_0To17_0(aConnection)));
        break;
      case MakeSchemaVersion(17, 0):
        QM_TRY(
            MOZ_TO_RESULT(UpgradeSchemaFrom17_0To18_0(aConnection, aOrigin)));
        vacuumNeeded = true;
        break;
      case MakeSchemaVersion(18, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom18_0To19_0(aConnection)));
        break;
      case MakeSchemaVersion(19, 0):
        QM_TRY(MOZ_TO_RESULT(
            UpgradeSchemaFrom19_0To20_0(&aFMDirectory, aConnection)));
        break;
      case MakeSchemaVersion(20, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom20_0To21_0(aConnection)));
        break;
      case MakeSchemaVersion(21, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom21_0To22_0(aConnection)));
        break;
      case MakeSchemaVersion(22, 0):
        QM_TRY(
            MOZ_TO_RESULT(UpgradeSchemaFrom22_0To23_0(aConnection, aOrigin)));
        break;
      case MakeSchemaVersion(23, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom23_0To24_0(aConnection)));
        break;
      case MakeSchemaVersion(24, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom24_0To25_0(aConnection)));
        break;
      case MakeSchemaVersion(25, 0):
        QM_TRY(MOZ_TO_RESULT(UpgradeSchemaFrom25_0To26_0(aConnection)));
        break;
      default:
        QM_FAIL(Err(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR), []() {
          IDB_WARNING(
              "Unable to open IndexedDB database, no upgrade path is "
              "available!");
        });
    }

    QM_TRY_UNWRAP(schemaVersion,
                  MOZ_TO_RESULT_INVOKE_MEMBER(aConnection, GetSchemaVersion));
  }

  MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);

  return vacuumNeeded;
}

}  // namespace mozilla::dom::indexedDB

#undef IDB_MOBILE
