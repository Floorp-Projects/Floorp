/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SchemaVersion002.h"

#include "FileSystemFileManager.h"
#include "FileSystemHashSource.h"
#include "FileSystemHashStorageFunction.h"
#include "ResultStatement.h"
#include "StartedTransaction.h"
#include "fs/FileSystemConstants.h"
#include "mozStorageHelper.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsID.h"

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

Result<Ok, QMResult> ClearInvalidFileIds(
    ResultConnection& aConn, data::FileSystemFileManager& aFileManager) {
  // We cant't just clear all file ids because if a file was accessed using
  // writable file stream a new file id was created which is not the same as
  // entry id.

  // Get all file ids first.
  QM_TRY_INSPECT(
      const auto& allFileIds,
      ([&aConn]() -> Result<nsTArray<FileId>, QMResult> {
        const nsLiteralCString allFileIdsQuery =
            "SELECT fileId FROM FileIds;"_ns;

        QM_TRY_UNWRAP(ResultStatement stmt,
                      ResultStatement::Create(aConn, allFileIdsQuery));

        nsTArray<FileId> fileIds;

        while (true) {
          QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());
          if (!moreResults) {
            break;
          }

          QM_TRY_UNWRAP(FileId fileId, stmt.GetFileIdByColumn(/* Column */ 0u));

          fileIds.AppendElement(fileId);
        }

        return std::move(fileIds);
      }()));

  // Filter out file ids which have non-zero-sized files on disk.
  QM_TRY_INSPECT(const auto& invalidFileIds,
                 ([&aFileManager](const nsTArray<FileId>& aFileIds)
                      -> Result<nsTArray<FileId>, QMResult> {
                   nsTArray<FileId> fileIds;

                   for (const auto& fileId : aFileIds) {
                     QM_TRY_UNWRAP(auto file, aFileManager.GetFile(fileId));

                     QM_TRY_INSPECT(const bool& exists,
                                    QM_TO_RESULT_INVOKE_MEMBER(file, Exists));

                     if (exists) {
                       QM_TRY_INSPECT(
                           const int64_t& fileSize,
                           QM_TO_RESULT_INVOKE_MEMBER(file, GetFileSize));

                       if (fileSize != 0) {
                         continue;
                       }

                       QM_TRY(QM_TO_RESULT(file->Remove(false)));
                     }

                     fileIds.AppendElement(fileId);
                   }

                   return std::move(fileIds);
                 }(allFileIds)));

  // Finally, clear invalid file ids.
  QM_TRY(([&aConn](const nsTArray<FileId>& aFileIds) -> Result<Ok, QMResult> {
    for (const auto& fileId : aFileIds) {
      const nsLiteralCString clearFileIdsQuery =
          "DELETE FROM FileIds "
          "WHERE fileId = :fileId "
          ";"_ns;

      QM_TRY_UNWRAP(ResultStatement stmt,
                    ResultStatement::Create(aConn, clearFileIdsQuery));

      QM_TRY(QM_TO_RESULT(stmt.BindFileIdByName("fileId"_ns, fileId)));

      QM_TRY(QM_TO_RESULT(stmt.Execute()));
    }

    return Ok{};
  }(invalidFileIds)));

  return Ok{};
}

Result<Ok, QMResult> ClearInvalidMainFiles(
    ResultConnection& aConn, data::FileSystemFileManager& aFileManager) {
  // We cant't just clear all main files because if a file was accessed using
  // writable file stream a new main file was created which is not the same as
  // entry id.

  // Get all main files first.
  QM_TRY_INSPECT(
      const auto& allMainFiles,
      ([&aConn]() -> Result<nsTArray<std::pair<EntryId, FileId>>, QMResult> {
        const nsLiteralCString allMainFilesQuery =
            "SELECT handle, fileId FROM MainFiles;"_ns;

        QM_TRY_UNWRAP(ResultStatement stmt,
                      ResultStatement::Create(aConn, allMainFilesQuery));

        nsTArray<std::pair<EntryId, FileId>> mainFiles;

        while (true) {
          QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());
          if (!moreResults) {
            break;
          }

          QM_TRY_UNWRAP(EntryId entryId,
                        stmt.GetEntryIdByColumn(/* Column */ 0u));
          QM_TRY_UNWRAP(FileId fileId, stmt.GetFileIdByColumn(/* Column */ 1u));

          mainFiles.AppendElement(std::pair<EntryId, FileId>(entryId, fileId));
        }

        return std::move(mainFiles);
      }()));

  // Filter out main files which have non-zero-sized files on disk.
  QM_TRY_INSPECT(
      const auto& invalidMainFiles,
      ([&aFileManager](const nsTArray<std::pair<EntryId, FileId>>& aMainFiles)
           -> Result<nsTArray<std::pair<EntryId, FileId>>, QMResult> {
        nsTArray<std::pair<EntryId, FileId>> mainFiles;

        for (const auto& mainFile : aMainFiles) {
          QM_TRY_UNWRAP(auto file, aFileManager.GetFile(mainFile.second));

          QM_TRY_INSPECT(const bool& exists,
                         QM_TO_RESULT_INVOKE_MEMBER(file, Exists));

          if (exists) {
            QM_TRY_INSPECT(const int64_t& fileSize,
                           QM_TO_RESULT_INVOKE_MEMBER(file, GetFileSize));

            if (fileSize != 0) {
              continue;
            }

            QM_TRY(QM_TO_RESULT(file->Remove(false)));
          }

          mainFiles.AppendElement(mainFile);
        }

        return std::move(mainFiles);
      }(allMainFiles)));

  // Finally, clear invalid main files.
  QM_TRY(([&aConn](const nsTArray<std::pair<EntryId, FileId>>& aMainFiles)
              -> Result<Ok, QMResult> {
    for (const auto& mainFile : aMainFiles) {
      const nsLiteralCString clearMainFilesQuery =
          "DELETE FROM MainFiles "
          "WHERE handle = :entryId AND fileId = :fileId "
          ";"_ns;

      QM_TRY_UNWRAP(ResultStatement stmt,
                    ResultStatement::Create(aConn, clearMainFilesQuery));

      QM_TRY(
          QM_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, mainFile.first)));
      QM_TRY(QM_TO_RESULT(stmt.BindFileIdByName("fileId"_ns, mainFile.second)));

      QM_TRY(QM_TO_RESULT(stmt.Execute()));
    }

    return Ok{};
  }(invalidMainFiles)));

  return Ok{};
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

nsresult FixEntryIds(const ResultConnection& aConnection,
                     const EntryId& aRootEntry) {
  const nsLiteralCString calculateHashesQuery =
      "CREATE TEMPORARY TABLE EntryMigrationTable AS "
      "WITH RECURSIVE "
      "rehashMap( depth, isFile, handle, parent, name, hash ) AS ( "
      "SELECT 0, isFile, handle, parent, name, hashEntry( :rootEntry, name ) "
      "FROM EntryNames WHERE parent = :rootEntry UNION SELECT "
      "1 + depth, EntryNames.isFile, EntryNames.handle, EntryNames.parent, "
      "EntryNames.name, hashEntry( rehashMap.hash, EntryNames.name ) "
      "FROM rehashMap, EntryNames WHERE rehashMap.handle = EntryNames.parent ) "
      "SELECT depth, isFile, handle, parent, name, hash FROM rehashMap "
      ";"_ns;

  const nsLiteralCString createIndexByDepthQuery =
      "CREATE INDEX indexOnDepth ON EntryMigrationTable ( depth ); "_ns;

  // To avoid constraint violation, new entries are inserted under a temporary
  // parent.

  const nsLiteralCString insertTemporaryParentEntry =
      "INSERT INTO Entries ( handle, parent ) "
      "VALUES ( :tempParent, :rootEntry ) ;"_ns;

  const nsLiteralCString flagTemporaryParentAsDir =
      "INSERT INTO Directories ( handle, name ) "
      "VALUES ( :tempParent, 'temp' ) ;"_ns;

  const nsLiteralCString insertNewEntriesQuery =
      "INSERT INTO Entries ( handle, parent ) "
      "SELECT hash, :tempParent FROM EntryMigrationTable WHERE hash != handle "
      ";"_ns;

  const nsLiteralCString insertNewDirectoriesQuery =
      "INSERT INTO Directories ( handle, name ) "
      "SELECT hash, name FROM EntryMigrationTable "
      "WHERE isFile = 0 AND hash != handle "
      "ORDER BY depth "
      ";"_ns;

  const nsLiteralCString insertNewFilesQuery =
      "INSERT INTO Files ( handle, type, name ) "
      "SELECT EntryMigrationTable.hash, Files.type, EntryMigrationTable.name "
      "FROM EntryMigrationTable INNER JOIN Files USING (handle) "
      "WHERE EntryMigrationTable.isFile = 1 AND hash != handle "
      ";"_ns;

  const nsLiteralCString updateFileMappingsQuery =
      "UPDATE FileIds SET handle = hash "
      "FROM ( SELECT handle, hash FROM EntryMigrationTable WHERE hash != "
      "handle ) "
      "AS replacement WHERE FileIds.handle = replacement.handle "
      ";"_ns;

  const nsLiteralCString updateMainFilesQuery =
      "UPDATE MainFiles SET handle = hash "
      "FROM ( SELECT handle, hash FROM EntryMigrationTable WHERE hash != "
      "handle ) "
      "AS replacement WHERE MainFiles.handle = replacement.handle "
      ";"_ns;

  // Now fix the parents.
  const nsLiteralCString updateEntryMappingsQuery =
      "UPDATE Entries SET parent = hash "
      "FROM ( SELECT Lhs.hash AS handle, Rhs.hash AS hash, Lhs.depth AS depth "
      "FROM EntryMigrationTable AS Lhs "
      "INNER JOIN EntryMigrationTable AS Rhs "
      "ON Rhs.handle = Lhs.parent ORDER BY depth ) AS replacement "
      "WHERE Entries.handle = replacement.handle "
      "AND Entries.parent = :tempParent "
      ";"_ns;

  const nsLiteralCString cleanupOldEntriesQuery =
      "DELETE FROM Entries WHERE handle IN "
      "( SELECT handle FROM EntryMigrationTable WHERE hash != handle ) "
      ";"_ns;

  const nsLiteralCString cleanupTemporaryParent =
      "DELETE FROM Entries WHERE handle = :tempParent ;"_ns;

  const nsLiteralCString dropIndexByDepthQuery =
      "DROP INDEX indexOnDepth ; "_ns;

  // Index is automatically deleted
  const nsLiteralCString cleanupTemporaries =
      "DROP TABLE EntryMigrationTable ;"_ns;

  EntryId tempParent(nsCString(nsID::GenerateUUID().ToString().get()));

  nsCOMPtr<mozIStorageFunction> rehashFunction =
      new data::FileSystemHashStorageFunction();
  QM_TRY(MOZ_TO_RESULT(aConnection->CreateFunction("hashEntry"_ns,
                                                   /* number of arguments */ 2,
                                                   rehashFunction)));
  auto finallyRemoveFunction = MakeScopeExit([&aConnection]() {
    QM_WARNONLY_TRY(MOZ_TO_RESULT(aConnection->RemoveFunction("hashEntry"_ns)));
  });

  // We need this to make sure the old entries get removed
  QM_TRY(MOZ_TO_RESULT(
      aConnection->ExecuteSimpleSQL("PRAGMA foreign_keys = ON;"_ns)));

  QM_TRY_UNWRAP(auto transaction, StartedTransaction::Create(aConnection));

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, calculateHashesQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("rootEntry"_ns, aRootEntry)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(aConnection->ExecuteSimpleSQL(createIndexByDepthQuery)));

  {
    QM_TRY_UNWRAP(
        ResultStatement stmt,
        ResultStatement::Create(aConnection, insertTemporaryParentEntry));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("tempParent"_ns, tempParent)));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("rootEntry"_ns, aRootEntry)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(
        ResultStatement stmt,
        ResultStatement::Create(aConnection, flagTemporaryParentAsDir));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("tempParent"_ns, tempParent)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, insertNewEntriesQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("tempParent"_ns, tempParent)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(
        ResultStatement stmt,
        ResultStatement::Create(aConnection, insertNewDirectoriesQuery));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, insertNewFilesQuery));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(
        ResultStatement stmt,
        ResultStatement::Create(aConnection, updateFileMappingsQuery));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, updateMainFilesQuery));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(
        ResultStatement stmt,
        ResultStatement::Create(aConnection, updateEntryMappingsQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("tempParent"_ns, tempParent)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, cleanupOldEntriesQuery));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, cleanupTemporaryParent));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("tempParent"_ns, tempParent)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(aConnection->ExecuteSimpleSQL(dropIndexByDepthQuery)));

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, cleanupTemporaries));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  QM_WARNONLY_TRY(QM_TO_RESULT(aConnection->ExecuteSimpleSQL("VACUUM;"_ns)));

  return NS_OK;
}

}  // namespace

Result<DatabaseVersion, QMResult> SchemaVersion002::InitializeConnection(
    ResultConnection& aConn, data::FileSystemFileManager& aFileManager,
    const Origin& aOrigin) {
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

  // The upgrade from version 1 to version 2 was buggy, so we have to check if
  // the Usages table still references the Files table which is a sign that
  // the upgrade wasn't complete. This extra query has only negligible perf
  // impact. See bug 1847989.
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

    // The buggy upgrade didn't call PopulateFileIds, ConnectUsagesToFileIds
    // and PopulateMainFiles was completely missing. Since invalid file ids
    // and main files could be inserted when the profile was broken, we need
    // to clear them before populating.
    QM_TRY(ClearInvalidFileIds(aConn, aFileManager));
    QM_TRY(QM_TO_RESULT(PopulateFileIds(aConn)));
    QM_TRY(QM_TO_RESULT(ConnectUsagesToFileIds(aConn)));
    QM_TRY(ClearInvalidMainFiles(aConn, aFileManager));
    QM_TRY(QM_TO_RESULT(PopulateMainFiles(aConn)));

    QM_TRY(QM_TO_RESULT(transaction.Commit()));

    QM_TRY(QM_TO_RESULT(aConn->ExecuteSimpleSQL("VACUUM;"_ns)));

    QM_TRY_UNWRAP(usagesTableRefsFilesTable, UsagesTableRefsFilesTable());
    MOZ_ASSERT(!usagesTableRefsFilesTable);
  }

  // In schema version 001, entryId was unique but not necessarily related to
  // a path. For schema 002, we have to fix all entryIds to be derived from
  // the underlying path.
  auto OneTimeRehashingDone = [&aConn]() -> Result<bool, QMResult> {
    const nsLiteralCString query =
        "SELECT EXISTS (SELECT 1 FROM sqlite_master "
        "WHERE type='table' AND name='RehashedFrom001to002' ) ;"_ns;

    QM_TRY_UNWRAP(ResultStatement stmt, ResultStatement::Create(aConn, query));

    return stmt.YesOrNoQuery();
  };

  QM_TRY_UNWRAP(auto oneTimeRehashingDone, OneTimeRehashingDone());

  if (!oneTimeRehashingDone) {
    const nsLiteralCString findRootEntry =
        "SELECT handle FROM Entries WHERE parent IS NULL ;"_ns;

    EntryId rootId;
    {
      QM_TRY_UNWRAP(ResultStatement stmt,
                    ResultStatement::Create(aConn, findRootEntry));

      QM_TRY_UNWRAP(DebugOnly<bool> moreResults, stmt.ExecuteStep());
      MOZ_ASSERT(moreResults);

      QM_TRY_UNWRAP(rootId, stmt.GetEntryIdByColumn(/* Column */ 0u));
    }

    MOZ_ASSERT(!rootId.IsEmpty());

    QM_TRY(QM_TO_RESULT(FixEntryIds(aConn, rootId)));

    QM_TRY(QM_TO_RESULT(aConn->ExecuteSimpleSQL(
        "CREATE TABLE RehashedFrom001to002 (id INTEGER PRIMARY KEY);"_ns)));

    QM_TRY_UNWRAP(DebugOnly<bool> isDoneNow, OneTimeRehashingDone());
    MOZ_ASSERT(isDoneNow);
  }

  QM_TRY(QM_TO_RESULT(aConn->ExecuteSimpleSQL("PRAGMA foreign_keys = ON;"_ns)));

  QM_TRY(QM_TO_RESULT(aConn->GetSchemaVersion(&currentVersion)));

  return currentVersion;
}

}  // namespace mozilla::dom::fs
