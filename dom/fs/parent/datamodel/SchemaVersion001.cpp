/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SchemaVersion001.h"

#include "FileSystemHashSource.h"
#include "ResultStatement.h"
#include "fs/FileSystemConstants.h"
#include "mozStorageHelper.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::fs {

namespace {

nsresult SetEncoding(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(R"(PRAGMA encoding = "UTF-16";)"_ns);
}

nsresult CreateEntries(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "CREATE TABLE IF NOT EXISTS Entries ( "
      "handle BLOB PRIMARY KEY, "  // Generated from parent + name, unique
      "parent BLOB, "              // Not null due to constraint
      "CONSTRAINT parent_is_a_directory "
      "FOREIGN KEY (parent) "
      "REFERENCES Directories (handle) "
      "ON DELETE CASCADE ) "
      ";"_ns);
}

nsresult CreateDirectories(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "CREATE TABLE IF NOT EXISTS Directories ( "
      "handle BLOB PRIMARY KEY, "
      "name BLOB NOT NULL, "
      "CONSTRAINT directories_are_entries "
      "FOREIGN KEY (handle) "
      "REFERENCES Entries (handle) "
      "ON DELETE CASCADE ) "
      ";"_ns);
}

nsresult CreateFiles(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "CREATE TABLE IF NOT EXISTS Files ( "
      "handle BLOB PRIMARY KEY, "
      "type TEXT, "
      "name BLOB NOT NULL, "
      "CONSTRAINT files_are_entries "
      "FOREIGN KEY (handle) "
      "REFERENCES Entries (handle) "
      "ON DELETE CASCADE ) "
      ";"_ns);
}

class KeepForeignKeysOffUntilScopeExit final {
 public:
  explicit KeepForeignKeysOffUntilScopeExit(const ResultConnection& aConn)
      : mConn(aConn) {}

  static Result<KeepForeignKeysOffUntilScopeExit, QMResult> Create(
      const ResultConnection& aConn) {
    QM_TRY(
        QM_TO_RESULT(aConn->ExecuteSimpleSQL("PRAGMA foreign_keys = OFF;"_ns)));
    KeepForeignKeysOffUntilScopeExit result(aConn);
    return result;
  }

  ~KeepForeignKeysOffUntilScopeExit() {
    auto maskResult = [this]() -> Result<Ok, nsresult> {
      QM_TRY(MOZ_TO_RESULT(
          mConn->ExecuteSimpleSQL("PRAGMA foreign_keys = ON;"_ns)));

      return Ok{};
    };
    QM_WARNONLY_TRY(maskResult());
  }

 private:
  ResultConnection mConn;
};

nsresult CreateRootEntry(ResultConnection& aConn, const Origin& aOrigin) {
  KeepForeignKeysOffUntilScopeExit foreignKeysGuard(aConn);

  const nsLiteralCString createRootQuery =
      "INSERT OR IGNORE INTO Entries "
      "( handle, parent ) "
      "VALUES ( :handle, NULL );"_ns;

  const nsLiteralCString flagRootAsDirectoryQuery =
      "INSERT OR IGNORE INTO Directories "
      "( handle, name ) "
      "VALUES ( :handle, :name );"_ns;

  QM_TRY_UNWRAP(EntryId rootId,
                data::FileSystemHashSource::GenerateHash(aOrigin, kRootName));

  mozStorageTransaction transaction(
      aConn.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConn, createRootQuery));
    QM_TRY(MOZ_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, rootId)));
    QM_TRY(MOZ_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConn, flagRootAsDirectoryQuery));
    QM_TRY(MOZ_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, rootId)));
    QM_TRY(MOZ_TO_RESULT(stmt.BindNameByName("name"_ns, kRootName)));
    QM_TRY(MOZ_TO_RESULT(stmt.Execute()));
  }

  return transaction.Commit();
}

nsresult CreateUsages(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "CREATE TABLE IF NOT EXISTS Usages ( "
      "usage INTEGER NOT NULL, "
      "aggregated BOOLEAN DEFAULT FALSE "
      ") "
      ";"_ns);
}

Result<bool, QMResult> CheckIfEmpty(ResultConnection& aConn) {
  const nsLiteralCString areThereTablesQuery =
      "SELECT EXISTS ("
      "SELECT 1 FROM sqlite_master "
      ");"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConn, areThereTablesQuery));

  return stmt.YesOrNoQuery();
};

Result<DatabaseVersion, QMResult> GetDatabaseVersion(ResultConnection& aConn) {
  const nsLiteralCString getUserVersionQuery = "PRAGMA USER_VERSION;"_ns;
  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConn, getUserVersionQuery));

  return stmt.GetDatabaseVersion();
}

nsresult SetDatabaseVersion(ResultConnection& aConn) {
  // Unfortunately bind does not work.
  nsCString setUserVersionQuery = "PRAGMA USER_VERSION = "_ns;
  setUserVersionQuery.AppendInt(SchemaVersion001::sVersion);
  setUserVersionQuery.Append(" ;"_ns);

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConn, setUserVersionQuery));
  QM_TRY(MOZ_TO_RESULT(stmt.Execute()));

  return NS_OK;
}

}  // namespace

Result<DatabaseVersion, QMResult> SchemaVersion001::InitializeConnection(
    ResultConnection& aConn, const Origin& aOrigin) {
  QM_TRY_UNWRAP(bool isEmpty, CheckIfEmpty(aConn));

  DatabaseVersion previous = 0;

  if (isEmpty) {
    QM_TRY(QM_TO_RESULT(SetEncoding(aConn)));
  } else {
    QM_TRY_UNWRAP(previous, GetDatabaseVersion(aConn));
  }

  if (previous < sVersion) {
    QM_TRY(QM_TO_RESULT(CreateEntries(aConn)));
    QM_TRY(QM_TO_RESULT(CreateDirectories(aConn)));
    QM_TRY(QM_TO_RESULT(CreateFiles(aConn)));
    QM_TRY(QM_TO_RESULT(CreateUsages(aConn)));
    QM_TRY(QM_TO_RESULT(CreateRootEntry(aConn, aOrigin)));
    QM_TRY(QM_TO_RESULT(SetDatabaseVersion(aConn)));
  }

  return GetDatabaseVersion(aConn);
}

}  // namespace mozilla::dom::fs
