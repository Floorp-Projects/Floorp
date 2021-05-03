/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DBSchema.h"

// local includes
#include "ActorsParentCommon.h"
#include "IndexedDBCommon.h"

// global includes
#include "ErrorList.h"
#include "js/StructuredClone.h"
#include "mozIStorageConnection.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/ProfilerLabels.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsLiteralString.h"
#include "nsString.h"

namespace mozilla::dom::indexedDB {

using quota::AssertIsOnIOThread;

// If JS_STRUCTURED_CLONE_VERSION changes then we need to update our major
// schema version.
static_assert(JS_STRUCTURED_CLONE_VERSION == 8,
              "Need to update the major schema version.");

nsresult CreateFileTables(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("CreateFileTables", DOM);

  constexpr nsLiteralCString commands[] = {
      // Table `file`
      "CREATE TABLE file ("
      "id INTEGER PRIMARY KEY, "
      "refcount INTEGER NOT NULL"
      ");"_ns,
      "CREATE TRIGGER object_data_insert_trigger "
      "AFTER INSERT ON object_data "
      "FOR EACH ROW "
      "WHEN NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(NULL, NEW.file_ids); "
      "END;"_ns,
      "CREATE TRIGGER object_data_update_trigger "
      "AFTER UPDATE OF file_ids ON object_data "
      "FOR EACH ROW "
      "WHEN OLD.file_ids IS NOT NULL OR NEW.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NEW.file_ids); "
      "END;"_ns,
      "CREATE TRIGGER object_data_delete_trigger "
      "AFTER DELETE ON object_data "
      "FOR EACH ROW WHEN OLD.file_ids IS NOT NULL "
      "BEGIN "
      "SELECT update_refcount(OLD.file_ids, NULL); "
      "END;"_ns,
      "CREATE TRIGGER file_update_trigger "
      "AFTER UPDATE ON file "
      "FOR EACH ROW WHEN NEW.refcount = 0 "
      "BEGIN "
      "DELETE FROM file WHERE id = OLD.id; "
      "END;"_ns};

  QM_TRY(ExecuteSimpleSQLSequence(aConnection, commands));

  return NS_OK;
}

nsresult CreateTables(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("CreateTables", DOM);

  constexpr nsLiteralCString commands[] = {
      // Table `database`

      // There are two reasons for having the origin column.
      // First, we can ensure that we don't have collisions in the origin hash
      // we
      // use for the path because when we open the db we can make sure that the
      // origins exactly match. Second, chrome code crawling through the idb
      // directory can figure out the origin of every db without having to
      // reverse-engineer our hash scheme.
      "CREATE TABLE database"
      "( name TEXT PRIMARY KEY"
      ", origin TEXT NOT NULL"
      ", version INTEGER NOT NULL DEFAULT 0"
      ", last_vacuum_time INTEGER NOT NULL DEFAULT 0"
      ", last_analyze_time INTEGER NOT NULL DEFAULT 0"
      ", last_vacuum_size INTEGER NOT NULL DEFAULT 0"
      ") WITHOUT ROWID;"_ns,
      // Table `object_store`
      "CREATE TABLE object_store"
      "( id INTEGER PRIMARY KEY"
      ", auto_increment INTEGER NOT NULL DEFAULT 0"
      ", name TEXT NOT NULL"
      ", key_path TEXT"
      ");"_ns,
      // Table `object_store_index`
      "CREATE TABLE object_store_index"
      "( id INTEGER PRIMARY KEY"
      ", object_store_id INTEGER NOT NULL"
      ", name TEXT NOT NULL"
      ", key_path TEXT NOT NULL"
      ", unique_index INTEGER NOT NULL"
      ", multientry INTEGER NOT NULL"
      ", locale TEXT"
      ", is_auto_locale BOOLEAN NOT NULL"
      ", FOREIGN KEY (object_store_id) "
      "REFERENCES object_store(id) "
      ");"_ns,
      // Table `object_data`
      "CREATE TABLE object_data"
      "( object_store_id INTEGER NOT NULL"
      ", key BLOB NOT NULL"
      ", index_data_values BLOB DEFAULT NULL"
      ", file_ids TEXT"
      ", data BLOB NOT NULL"
      ", PRIMARY KEY (object_store_id, key)"
      ", FOREIGN KEY (object_store_id) "
      "REFERENCES object_store(id) "
      ") WITHOUT ROWID;"_ns,
      // Table `index_data`
      "CREATE TABLE index_data"
      "( index_id INTEGER NOT NULL"
      ", value BLOB NOT NULL"
      ", object_data_key BLOB NOT NULL"
      ", object_store_id INTEGER NOT NULL"
      ", value_locale BLOB"
      ", PRIMARY KEY (index_id, value, object_data_key)"
      ", FOREIGN KEY (index_id) "
      "REFERENCES object_store_index(id) "
      ", FOREIGN KEY (object_store_id, object_data_key) "
      "REFERENCES object_data(object_store_id, key) "
      ") WITHOUT ROWID;"_ns,
      "CREATE INDEX index_data_value_locale_index "
      "ON index_data (index_id, value_locale, object_data_key, value) "
      "WHERE value_locale IS NOT NULL;"_ns,
      // Table `unique_index_data`
      "CREATE TABLE unique_index_data"
      "( index_id INTEGER NOT NULL"
      ", value BLOB NOT NULL"
      ", object_store_id INTEGER NOT NULL"
      ", object_data_key BLOB NOT NULL"
      ", value_locale BLOB"
      ", PRIMARY KEY (index_id, value)"
      ", FOREIGN KEY (index_id) "
      "REFERENCES object_store_index(id) "
      ", FOREIGN KEY (object_store_id, object_data_key) "
      "REFERENCES object_data(object_store_id, key) "
      ") WITHOUT ROWID;"_ns,
      "CREATE INDEX unique_index_data_value_locale_index "
      "ON unique_index_data (index_id, value_locale, object_data_key, value) "
      "WHERE value_locale IS NOT NULL;"_ns};

  QM_TRY(ExecuteSimpleSQLSequence(aConnection, commands));

  QM_TRY(CreateFileTables(aConnection));

  QM_TRY(aConnection.SetSchemaVersion(kSQLiteSchemaVersion));

  return NS_OK;
}

}  // namespace mozilla::dom::indexedDB
