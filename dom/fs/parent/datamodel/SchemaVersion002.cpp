/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SchemaVersion002.h"

#include "ResultStatement.h"
#include "StartedTransaction.h"
#include "fs/FileSystemConstants.h"
#include "mozStorageHelper.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::fs {

namespace {

nsresult CreateFileIds(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "CREATE TABLE IF NOT EXISTS FileIds ( "
      "fileId BLOB PRIMARY KEY, "
      "handle BLOB, "
      "FOREIGN KEY (handle) "
      "REFERENCES Files (handle) "
      "ON DELETE SET NULL ) "
      ";"_ns);
}

nsresult CreateMainFiles(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "CREATE TABLE IF NOT EXISTS MainFiles ( "
      "handle BLOB UNIQUE, "
      "fileId BLOB UNIQUE, "
      "FOREIGN KEY (handle) REFERENCES Files (handle) "
      "ON DELETE CASCADE, "
      "FOREIGN KEY (fileId) REFERENCES FileIds (fileId) "
      "ON DELETE SET NULL ) "
      ";"_ns);
}

nsresult PopulateFileIds(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "INSERT OR IGNORE INTO FileIds ( fileId, handle ) "
      "SELECT handle, handle FROM Files "
      ";"_ns);
}

nsresult PopulateMainFiles(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "INSERT OR IGNORE INTO MainFiles ( fileId, handle ) "
      "SELECT handle, handle FROM Files "
      ";"_ns);
}

nsresult ClearFileIds(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL("DELETE FROM FileIds;"_ns);
}

nsresult ClearMainFiles(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL("DELETE FROM MainFiles;"_ns);
}

nsresult ConnectUsagesToFileIds(ResultConnection& aConn) {
  QM_TRY(
      MOZ_TO_RESULT(aConn->ExecuteSimpleSQL("PRAGMA foreign_keys = OFF;"_ns)));

  auto turnForeignKeysBackOn = MakeScopeExit([&aConn]() {
    QM_WARNONLY_TRY(
        MOZ_TO_RESULT(aConn->ExecuteSimpleSQL("PRAGMA foreign_keys = ON;"_ns)));
  });

  QM_TRY_UNWRAP(auto transaction, StartedTransaction::Create(aConn));

  QM_TRY(MOZ_TO_RESULT(
      aConn->ExecuteSimpleSQL("DROP TABLE IF EXISTS migrateUsages ;"_ns)));

  QM_TRY(MOZ_TO_RESULT(aConn->ExecuteSimpleSQL(
      "CREATE TABLE migrateUsages ( "
      "handle BLOB PRIMARY KEY, "
      "usage INTEGER NOT NULL DEFAULT 0, "
      "tracked BOOLEAN NOT NULL DEFAULT 0 CHECK (tracked IN (0, 1)), "
      "CONSTRAINT handles_are_fileIds "
      "FOREIGN KEY (handle) "
      "REFERENCES FileIds (fileId) "
      "ON DELETE CASCADE ) "
      ";"_ns)));

  QM_TRY(MOZ_TO_RESULT(aConn->ExecuteSimpleSQL(
      "INSERT INTO migrateUsages ( handle, usage, tracked ) "
      "SELECT handle, usage, tracked FROM Usages ;"_ns)));

  QM_TRY(MOZ_TO_RESULT(aConn->ExecuteSimpleSQL("DROP TABLE Usages;"_ns)));

  QM_TRY(MOZ_TO_RESULT(aConn->ExecuteSimpleSQL(
      "ALTER TABLE migrateUsages RENAME TO Usages;"_ns)));

  QM_TRY(
      MOZ_TO_RESULT(aConn->ExecuteSimpleSQL("PRAGMA foreign_key_check;"_ns)));

  QM_TRY(MOZ_TO_RESULT(transaction.Commit()));

  return NS_OK;
}

nsresult CreateEntryNamesView(ResultConnection& aConn) {
  return aConn->ExecuteSimpleSQL(
      "CREATE VIEW IF NOT EXISTS EntryNames AS "
      "SELECT isFile, handle, parent, name FROM Entries INNER JOIN ( "
      "SELECT 1 AS isFile, handle, name FROM Files UNION "
      "SELECT 0, handle, name FROM Directories ) "
      "USING (handle) "
      ";"_ns);
}

}  // namespace

Result<DatabaseVersion, QMResult> SchemaVersion002::InitializeConnection(
    ResultConnection& aConn, const Origin& aOrigin) {
  QM_TRY_UNWRAP(const bool wasEmpty, CheckIfEmpty(aConn));

  DatabaseVersion currentVersion = 0;

  if (wasEmpty) {
    QM_TRY(QM_TO_RESULT(SetEncoding(aConn)));
  } else {
    QM_TRY(QM_TO_RESULT(aConn->GetSchemaVersion(&currentVersion)));
  }

  if (currentVersion < sVersion) {
    MOZ_ASSERT_IF(0 != currentVersion, 1 == currentVersion);

    QM_TRY_UNWRAP(auto transaction, StartedTransaction::Create(aConn));

    if (0 == currentVersion) {
      QM_TRY(QM_TO_RESULT(SchemaVersion001::CreateTables(aConn, aOrigin)));
    }

    QM_TRY(QM_TO_RESULT(CreateFileIds(aConn)));

    if (!wasEmpty) {
      QM_TRY(QM_TO_RESULT(PopulateFileIds(aConn)));
    }

    QM_TRY(QM_TO_RESULT(ConnectUsagesToFileIds(aConn)));

    QM_TRY(QM_TO_RESULT(CreateMainFiles(aConn)));
    if (!wasEmpty) {
      QM_TRY(QM_TO_RESULT(PopulateMainFiles(aConn)));
    }

    QM_TRY(QM_TO_RESULT(CreateEntryNamesView(aConn)));

    QM_TRY(QM_TO_RESULT(aConn->SetSchemaVersion(sVersion)));

    QM_TRY(QM_TO_RESULT(transaction.Commit()));

    if (!wasEmpty) {
      QM_TRY(QM_TO_RESULT(aConn->ExecuteSimpleSQL("VACUUM;"_ns)));
    }
  }

  auto UsagesTableRefsFilesTable = [&aConn]() -> Result<bool, QMResult> {
    const nsLiteralCString query =
        "SELECT pragma_foreign_key_list.'table'=='Files' "
        "FROM pragma_foreign_key_list('Usages');"_ns;

    QM_TRY_UNWRAP(ResultStatement stmt, ResultStatement::Create(aConn, query));

    return stmt.YesOrNoQuery();
  };

  QM_TRY_UNWRAP(auto usagesTableRefsFilesTable, UsagesTableRefsFilesTable());

  if (usagesTableRefsFilesTable) {
    QM_TRY_UNWRAP(auto transaction, StartedTransaction::Create(aConn));

    QM_TRY(QM_TO_RESULT(ClearFileIds(aConn)));
    QM_TRY(QM_TO_RESULT(PopulateFileIds(aConn)));
    QM_TRY(QM_TO_RESULT(ConnectUsagesToFileIds(aConn)));
    QM_TRY(QM_TO_RESULT(ClearMainFiles(aConn)));
    QM_TRY(QM_TO_RESULT(PopulateMainFiles(aConn)));

    QM_TRY(QM_TO_RESULT(transaction.Commit()));

    QM_TRY(QM_TO_RESULT(aConn->ExecuteSimpleSQL("VACUUM;"_ns)));

    QM_TRY_UNWRAP(usagesTableRefsFilesTable, UsagesTableRefsFilesTable());
    MOZ_ASSERT(!usagesTableRefsFilesTable);
  }

  QM_TRY(QM_TO_RESULT(aConn->ExecuteSimpleSQL("PRAGMA foreign_keys = ON;"_ns)));

  QM_TRY(QM_TO_RESULT(aConn->GetSchemaVersion(&currentVersion)));

  return currentVersion;
}

}  // namespace mozilla::dom::fs
