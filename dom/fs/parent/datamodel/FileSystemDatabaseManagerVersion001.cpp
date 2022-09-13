/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDatabaseManagerVersion001.h"

#include "FileSystemFileManager.h"
#include "ResultStatement.h"
#include "mozStorageHelper.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom {

using FileSystemEntries = nsTArray<fs::FileSystemEntryMetadata>;

namespace fs::data {

namespace {

Result<bool, QMResult> ApplyEntryExistsQuery(
    const FileSystemConnection& aConnection, const nsACString& aQuery,
    const FileSystemChildMetadata& aHandle) {
  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, aQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("parent"_ns, aHandle.parentId())));
  QM_TRY(QM_TO_RESULT(stmt.BindNameByName("name"_ns, aHandle.childName())));

  return stmt.YesOrNoQuery();
}

Result<bool, QMResult> IsDirectoryEmpty(FileSystemConnection& mConnection,
                                        const EntryId& aEntryId) {
  const nsLiteralCString isDirEmptyQuery =
      "SELECT EXISTS ("
      "SELECT 1 FROM Entries WHERE parent = :parent "
      ");"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, isDirEmptyQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("parent"_ns, aEntryId)));
  QM_TRY_UNWRAP(bool childrenExist, stmt.YesOrNoQuery());

  return !childrenExist;
}

// TODO: Running two aggregations simultaneously leads to
// undefined outcomes and should be prevented.
// However, appending new values while
// one aggreration is ongoing should work if the aggregation is only
// done up to a row which is fixed in the beginning.
nsresult AggregateUsages(FileSystemConnection& mConnection) {
  bool rollbackOnScopeExit{false};  // We roll back unless explicitly committed
  mozStorageTransaction transaction(
      mConnection.get(), rollbackOnScopeExit,
      mozIStorageConnection::TRANSACTION_IMMEDIATE);

  QM_TRY(MOZ_TO_RESULT(mConnection->ExecuteSimpleSQL(
      "INSERT INTO Usages "
      "( usage, aggregated ) "
      "SELECT SUM(usage) OVER (ROWS UNBOUNDED PRECEDING), TRUE "
      "FROM Usages "
      "WHERE aggregated = FALSE "
      ";"_ns)));

  QM_TRY(
      MOZ_TO_RESULT(mConnection->ExecuteSimpleSQL("DELETE FROM Usages "
                                                  "WHERE aggregated = FALSE "
                                                  ";"_ns)));

  QM_TRY(MOZ_TO_RESULT(
      mConnection->ExecuteSimpleSQL("UPDATE Usages "
                                    "SET aggregated = NOT aggregated "
                                    ";"_ns)));

  return transaction.Commit();
}

Result<bool, QMResult> DoesDirectoryExist(
    const FileSystemConnection& mConnection,
    const FileSystemChildMetadata& aHandle) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  const nsCString existsQuery =
      "SELECT EXISTS "
      "(SELECT 1 FROM Directories JOIN Entries USING (handle) "
      "WHERE Directories.name = :name AND Entries.parent = :parent )"
      ";"_ns;

  QM_TRY_UNWRAP(bool exists,
                ApplyEntryExistsQuery(mConnection, existsQuery, aHandle));

  return exists;
}

Result<Path, QMResult> ResolveReversedPath(
    const FileSystemConnection& aConnection,
    const FileSystemEntryPair& aEndpoints) {
  const nsCString pathQuery =
      "WITH RECURSIVE followPath(handle, parent) AS ( "
      "SELECT handle, parent "
      "FROM Entries "
      "WHERE handle=:entryId "
      "UNION "
      "SELECT Entries.handle, Entries.parent FROM followPath, Entries "
      "WHERE followPath.parent=Entries.handle ) "
      "SELECT COALESCE(Directories.name, Files.name), handle "
      "FROM followPath "
      "LEFT JOIN Directories USING(handle) "
      "LEFT JOIN Files USING(handle);"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, pathQuery));
  QM_TRY(
      QM_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEndpoints.childId())));
  QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());

  Path pathResult;
  while (moreResults) {
    QM_TRY_UNWRAP(Name entryName, stmt.GetNameByColumn(/* Column */ 0u));
    QM_TRY_UNWRAP(EntryId entryId, stmt.GetEntryIdByColumn(/* Column */ 1u));

    if (aEndpoints.parentId() == entryId) {
      return pathResult;
    }
    pathResult.AppendElement(entryName);

    QM_TRY_UNWRAP(moreResults, stmt.ExecuteStep());
  }

  return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
}

Result<bool, QMResult> DoesFileExist(const FileSystemConnection& mConnection,
                                     const FileSystemChildMetadata& aHandle) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  const nsCString existsQuery =
      "SELECT EXISTS "
      "(SELECT 1 FROM Files JOIN Entries USING (handle) "
      "WHERE Files.name = :name AND Entries.parent = :parent )"
      ";"_ns;

  QM_TRY_UNWRAP(bool exists,
                ApplyEntryExistsQuery(mConnection, existsQuery, aHandle));

  return exists;
}

nsresult GetFileAttributes(const FileSystemConnection& aConnection,
                           const EntryId& aEntryId,
                           TimeStamp& aLastModifiedMilliSeconds,
                           ContentType& aType) {
  const nsLiteralCString getFileLocation =
      "SELECT type FROM Files INNER JOIN Entries USING(handle) WHERE handle = :entryId;"_ns;

  // TODO: bug 1790391: Request this from filemanager who makes the system call
  aLastModifiedMilliSeconds = 0;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, getFileLocation));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));
  QM_TRY_UNWRAP(bool hasEntries, stmt.ExecuteStep());

  // Type is an optional attribute
  if (!hasEntries || stmt.IsNullByColumn(/* Column */ 0u)) {
    return NS_OK;
  }

  QM_TRY_UNWRAP(aType, stmt.GetContentTypeByColumn(/* Column */ 0u));

  return NS_OK;
}

nsresult GetEntries(const FileSystemConnection& aConnection,
                    const nsACString& aUnboundStmt, const EntryId& aParent,
                    PageNumber aPage, bool aDirectory,
                    FileSystemEntries& aEntries) {
  // The entries inside a directory are sent to the child process in batches
  // of pageSize items. Large value ensures that iteration is less often delayed
  // by IPC messaging and querying the database.
  // TODO: The current value 1024 is not optimized.
  // TODO: Value "pageSize" is shared with the iterator implementation and
  // should be defined in a common place.
  const int32_t pageSize = 1024;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, aUnboundStmt));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("parent"_ns, aParent)));
  QM_TRY(QM_TO_RESULT(stmt.BindPageNumberByName("pageSize"_ns, pageSize)));
  QM_TRY(QM_TO_RESULT(
      stmt.BindPageNumberByName("pageOffset"_ns, aPage * pageSize)));

  QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());

  while (moreResults) {
    QM_TRY_UNWRAP(EntryId entryId, stmt.GetEntryIdByColumn(/* Column */ 0u));
    QM_TRY_UNWRAP(Name entryName, stmt.GetNameByColumn(/* Column */ 1u));

    FileSystemEntryMetadata metadata(entryId, entryName, aDirectory);
    aEntries.AppendElement(metadata);

    QM_TRY_UNWRAP(moreResults, stmt.ExecuteStep());
  }

  return NS_OK;
}

Result<EntryId, QMResult> GetUniqueEntryId(
    const FileSystemConnection& aConnection,
    const FileSystemChildMetadata& aHandle) {
  const nsCString existsQuery =
      "SELECT EXISTS "
      "(SELECT 1 FROM Entries "
      "WHERE handle = :handle )"
      ";"_ns;

  FileSystemChildMetadata generatorInput = aHandle;

  const size_t maxRounds = 1024u;

  for (size_t hangGuard = 0u; hangGuard < maxRounds; ++hangGuard) {
    QM_TRY_UNWRAP(EntryId entryId, fs::data::GetEntryHandle(generatorInput));
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, existsQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));

    QM_TRY_UNWRAP(bool alreadyInUse, stmt.YesOrNoQuery());

    if (!alreadyInUse) {
      return entryId;
    }

    generatorInput.parentId() = entryId;
  }

  return Err(QMResult(NS_ERROR_UNEXPECTED));
}

Result<EntryId, QMResult> FindEntryId(const FileSystemConnection& aConnection,
                                      const FileSystemChildMetadata& aHandle,
                                      bool isFile) {
  const nsCString aDirectoryQuery =
      "SELECT Entries.handle FROM Directories JOIN Entries USING (handle) "
      "WHERE Directories.name = :name AND Entries.parent = :parent "
      ";"_ns;

  const nsCString aFileQuery =
      "SELECT Entries.handle FROM Files JOIN Entries USING (handle) "
      "WHERE Files.name = :name AND Entries.parent = :parent "
      ";"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection,
                                        isFile ? aFileQuery : aDirectoryQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("parent"_ns, aHandle.parentId())));
  QM_TRY(QM_TO_RESULT(stmt.BindNameByName("name"_ns, aHandle.childName())));
  QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());

  if (!moreResults) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  QM_TRY_UNWRAP(EntryId entryId, stmt.GetEntryIdByColumn(/* Column */ 0u));

  return entryId;
}

}  // namespace

Result<Usage, QMResult> FileSystemDatabaseManagerVersion001::GetUsage() const {
  const nsLiteralCString sumUsagesQuery =
      "SELECT sum(deltas) FROM Usages WHERE aggregated = FALSE;"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, sumUsagesQuery));
  QM_TRY_UNWRAP(Usage total, stmt.GetUsageByColumn(/* Column */ 0u));

  return total;
}

nsresult FileSystemDatabaseManagerVersion001::UpdateUsage(int64_t aDelta) {
  const nsLiteralCString addUsageQuery =
      "INSERT INTO Usages "
      "( usage ) "
      "VALUES "
      "( :usage ) "
      ";"_ns;

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, addUsageQuery));
    QM_TRY(MOZ_TO_RESULT(stmt.BindUsageByName("usage"_ns, aDelta)));
    QM_TRY(MOZ_TO_RESULT(stmt.Execute()));
  }

  // Try again later, no harm done;
  QM_WARNONLY_TRY(MOZ_TO_RESULT(AggregateUsages(mConnection)));

  return NS_OK;
}

Result<EntryId, QMResult> FileSystemDatabaseManagerVersion001::GetParentEntryId(
    const EntryId& aEntry) const {
  MOZ_ASSERT(!aEntry.IsEmpty());

  const nsCString parentQuery =
      "SELECT parent FROM Entries "
      "WHERE handle = :entryId;"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, parentQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntry)));
  QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());
  if (!moreResults) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }
  QM_TRY_UNWRAP(EntryId parentId, stmt.GetEntryIdByColumn(/* Column */ 0u));

  return parentId;
}

Result<EntryId, QMResult>
FileSystemDatabaseManagerVersion001::GetOrCreateDirectory(
    const FileSystemChildMetadata& aHandle, bool aCreate) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  const auto& name = aHandle.childName();
  // Belt and suspenders: check here as well as in child.
  if (!IsValidName(name)) {
    return Err(QMResult(NS_ERROR_DOM_TYPE_MISMATCH_ERR));
  }
  MOZ_ASSERT(!name.IsVoid() && !name.IsEmpty());

  bool exists = true;
  QM_TRY_UNWRAP(exists, DoesFileExist(mConnection, aHandle));

  // By spec, we don't allow a file and a directory
  // to have the same name and parent
  if (exists) {
    return Err(QMResult(NS_ERROR_DOM_TYPE_MISMATCH_ERR));
  }

  QM_TRY_UNWRAP(exists, DoesDirectoryExist(mConnection, aHandle));

  // exists as directory
  if (exists) {
    return FindEntryId(mConnection, aHandle, false);
  }

  if (!aCreate) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  const nsLiteralCString insertEntryQuery =
      "INSERT OR IGNORE INTO Entries "
      "( handle, parent ) "
      "VALUES "
      "( :handle, :parent ) "
      ";"_ns;

  const nsLiteralCString insertDirectoryQuery =
      "INSERT OR IGNORE INTO Directories "
      "( handle, name ) "
      "VALUES "
      "( :handle, :name ) "
      ";"_ns;

  QM_TRY_UNWRAP(EntryId entryId, GetUniqueEntryId(mConnection, aHandle));
  MOZ_ASSERT(!entryId.IsEmpty());

  mozStorageTransaction transaction(
      mConnection.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);
  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, insertEntryQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(
        QM_TO_RESULT(stmt.BindEntryIdByName("parent"_ns, aHandle.parentId())));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, insertDirectoryQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(QM_TO_RESULT(stmt.BindNameByName("name"_ns, name)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(UpdateUsage(name.Length())));

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  QM_TRY_UNWRAP(DebugOnly<bool> doesItExistNow,
                DoesDirectoryExist(mConnection, aHandle));
  MOZ_ASSERT(doesItExistNow);

  return entryId;
}

Result<EntryId, QMResult> FileSystemDatabaseManagerVersion001::GetOrCreateFile(
    const FileSystemChildMetadata& aHandle, bool aCreate) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  const auto& name = aHandle.childName();
  // Belt and suspenders: check here as well as in child.
  if (!IsValidName(name)) {
    return Err(QMResult(NS_ERROR_DOM_TYPE_MISMATCH_ERR));
  }
  MOZ_ASSERT(!name.IsVoid() && !name.IsEmpty());

  QM_TRY_UNWRAP(bool exists, DoesDirectoryExist(mConnection, aHandle));

  // By spec, we don't allow a file and a directory
  // to have the same name and parent
  if (exists) {
    return Err(QMResult(NS_ERROR_DOM_TYPE_MISMATCH_ERR));
  }

  QM_TRY_UNWRAP(exists, DoesFileExist(mConnection, aHandle));

  if (exists) {
    return FindEntryId(mConnection, aHandle, true);
  }

  if (!aCreate) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  const nsLiteralCString insertEntryQuery =
      "INSERT INTO Entries "
      "( handle, parent ) "
      "VALUES "
      "( :handle, :parent ) "
      ";"_ns;

  const nsLiteralCString insertFileQuery =
      "INSERT INTO Files "
      "( handle, name ) "
      "VALUES "
      "( :handle, :name ) "
      ";"_ns;

  QM_TRY_UNWRAP(EntryId entryId, GetUniqueEntryId(mConnection, aHandle));
  MOZ_ASSERT(!entryId.IsEmpty());

  mozStorageTransaction transaction(
      mConnection.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);
  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, insertEntryQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(
        QM_TO_RESULT(stmt.BindEntryIdByName("parent"_ns, aHandle.parentId())));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, insertFileQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(QM_TO_RESULT(stmt.BindNameByName("name"_ns, name)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(UpdateUsage(name.Length())));

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  return entryId;
}

Result<FileSystemDirectoryListing, QMResult>
FileSystemDatabaseManagerVersion001::GetDirectoryEntries(
    const EntryId& aParent, PageNumber aPage) const {
  // TODO: Offset is reported to have bad performance - see Bug 1780386.
  const nsCString directoriesQuery =
      "SELECT Dirs.handle, Dirs.name "
      "FROM Directories AS Dirs "
      "INNER JOIN ( "
      "SELECT handle "
      "FROM Entries "
      "WHERE parent = :parent "
      "LIMIT :pageSize "
      "OFFSET :pageOffset ) "
      "AS Ents "
      "ON Dirs.handle = Ents.handle "
      ";"_ns;
  const nsCString filesQuery =
      "SELECT Files.handle, Files.name "
      "FROM Files "
      "INNER JOIN ( "
      "SELECT handle "
      "FROM Entries "
      "WHERE parent = :parent "
      "LIMIT :pageSize "
      "OFFSET :pageOffset ) "
      "AS Ents "
      "ON Files.handle = Ents.handle "
      ";"_ns;

  FileSystemDirectoryListing entries;
  QM_TRY(
      QM_TO_RESULT(GetEntries(mConnection, directoriesQuery, aParent, aPage,
                              /* aDirectory */ true, entries.directories())));

  QM_TRY(QM_TO_RESULT(GetEntries(mConnection, filesQuery, aParent, aPage,
                                 /* aDirectory */ false, entries.files())));

  return entries;
}

nsresult FileSystemDatabaseManagerVersion001::GetFile(
    const FileSystemEntryPair& aEndpoints, nsString& aType,
    TimeStamp& lastModifiedMilliSeconds, nsTArray<Name>& aPath,
    nsCOMPtr<nsIFile>& aFile) const {
  MOZ_ASSERT(!aEndpoints.parentId().IsEmpty());
  MOZ_ASSERT(!aEndpoints.childId().IsEmpty());

  const auto& entryId = aEndpoints.childId();

  QM_TRY(MOZ_TO_RESULT(GetFileAttributes(mConnection, entryId,
                                         lastModifiedMilliSeconds, aType)));

  QM_TRY_UNWRAP(aPath, ResolveReversedPath(mConnection, aEndpoints));

  aPath.Reverse();

  QM_TRY_UNWRAP(aFile,
                mFileManager->GetOrCreateFile(entryId));  // Timestamp from here

  return NS_OK;
}

Result<bool, QMResult> FileSystemDatabaseManagerVersion001::RemoveDirectory(
    const FileSystemChildMetadata& aHandle, bool aRecursive) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  DebugOnly<Name> name = aHandle.childName();
  MOZ_ASSERT(!name.inspect().IsVoid() && !name.inspect().IsEmpty());

  QM_TRY_UNWRAP(bool exists, DoesDirectoryExist(mConnection, aHandle));

  if (!exists) {
    return false;
  }

  // At this point, entry exists and is a directory.
  QM_TRY_UNWRAP(EntryId entryId, FindEntryId(mConnection, aHandle, false));
  MOZ_ASSERT(!entryId.IsEmpty());

  QM_TRY_UNWRAP(bool isEmpty, IsDirectoryEmpty(mConnection, entryId));

  if (!aRecursive && !isEmpty) {
    return Err(QMResult(
        NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR));  // Is this correct error?
  }
  // If it's empty or we can delete recursively, deleting the handle will
  // cascade

  const nsLiteralCString descendantsQuery =
      "WITH RECURSIVE traceChildren(handle, parent) AS ( "
      "SELECT handle, parent "
      "FROM Entries "
      "WHERE handle=:handle "
      "UNION "
      "SELECT Entries.handle, Entries.parent FROM traceChildren, Entries "
      "WHERE traceChildren.handle=Entries.parent ) "
      "SELECT handle "
      "FROM traceChildren INNER JOIN Files "
      "USING(handle) "
      ";"_ns;

  const nsLiteralCString deleteEntryQuery =
      "DELETE FROM Entries "
      "WHERE handle = :handle "
      ";"_ns;

  mozStorageTransaction transaction(
      mConnection.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  nsTArray<EntryId> descendants;
  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, descendantsQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());

    while (moreResults) {
      QM_TRY_UNWRAP(EntryId entryId, stmt.GetEntryIdByColumn(/* Column */ 0u));

      descendants.AppendElement(entryId);

      QM_TRY_UNWRAP(moreResults, stmt.ExecuteStep());
    }
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, deleteEntryQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(
      UpdateUsage(static_cast<int64_t>(aHandle.childName().Length()))));

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  for (const auto& child : descendants) {
    QM_WARNONLY_TRY(MOZ_TO_RESULT(mFileManager->RemoveFile(child)));
  }

  return true;
}

Result<bool, QMResult> FileSystemDatabaseManagerVersion001::RemoveFile(
    const FileSystemChildMetadata& aHandle) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  DebugOnly<Name> name = aHandle.childName();
  MOZ_ASSERT(!name.inspect().IsVoid() && !name.inspect().IsEmpty());

  // Make it more evident that we won't remove directories
  QM_TRY_UNWRAP(bool exists, DoesFileExist(mConnection, aHandle));

  if (!exists) {
    return false;
  }
  // At this point, entry exists and is a file

  const nsLiteralCString deleteEntryQuery =
      "DELETE FROM Entries "
      "WHERE handle = :handle "
      ";"_ns;

  QM_TRY_UNWRAP(EntryId entryId, FindEntryId(mConnection, aHandle, true));
  MOZ_ASSERT(!entryId.IsEmpty());

  mozStorageTransaction transaction(
      mConnection.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, deleteEntryQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(
      UpdateUsage(static_cast<int64_t>(aHandle.childName().Length()))));

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  QM_WARNONLY_TRY(MOZ_TO_RESULT(mFileManager->RemoveFile(entryId)));

  return true;
}

Result<bool, QMResult> FileSystemDatabaseManagerVersion001::MoveEntry(
    const FileSystemChildMetadata& aHandle,
    const FileSystemChildMetadata& aNewDesignation) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  if (aHandle.childName().IsEmpty() || aNewDesignation.childName().IsEmpty()) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  DebugOnly<Name> name = aHandle.childName();
  MOZ_ASSERT(!name.inspect().IsVoid());

  // Verify the source exists
  bool isFile = false;
  QM_TRY_UNWRAP(bool exists, DoesFileExist(mConnection, aHandle));
  if (!exists) {
    QM_TRY_UNWRAP(exists, DoesDirectoryExist(mConnection, aHandle));
    if (!exists) {
      return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
    }
  } else {
    isFile = true;
  }

  QM_TRY_UNWRAP(EntryId entryId, FindEntryId(mConnection, aHandle, isFile));
#if 0
  // Enable once file lock code lands with the SyncAccessHandle patch
  // At this point, entry exists
  if (isFile && !CanUseFile(entryId, AccessMode::EXCLUSIVE)) {
    LOG(("Trying to move in-use file"));
    return Err(QMResult(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR));
  }
#endif

  const nsLiteralCString updateEntryParentQuery =
      "UPDATE Entries "
      "SET parent = :parent "
      "WHERE handle = :handle "
      ";"_ns;

  const nsLiteralCString updateFileNameQuery =
      "UPDATE Files "
      "SET name = :name "
      "WHERE handle = :handle "
      ";"_ns;

  const nsLiteralCString updateDirectoryNameQuery =
      "UPDATE Directories "
      "SET name = :name "
      "WHERE handle = :handle "
      ";"_ns;

  mozStorageTransaction transaction(
      mConnection.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, updateEntryParentQuery));
    QM_TRY(QM_TO_RESULT(
        stmt.BindEntryIdByName("parent"_ns, aNewDesignation.parentId())));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  if (isFile) {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, updateFileNameQuery));
    QM_TRY(QM_TO_RESULT(
        stmt.BindNameByName("name"_ns, aNewDesignation.childName())));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  } else {
    QM_TRY_UNWRAP(
        ResultStatement stmt,
        ResultStatement::Create(mConnection, updateDirectoryNameQuery));
    QM_TRY(QM_TO_RESULT(
        stmt.BindNameByName("name"_ns, aNewDesignation.childName())));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, entryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  const auto usageDelta =
      static_cast<int64_t>(aNewDesignation.childName().Length()) -
      static_cast<int64_t>(aHandle.childName().Length());
  QM_TRY(QM_TO_RESULT(UpdateUsage(usageDelta)));

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  return true;
}

Result<Path, QMResult> FileSystemDatabaseManagerVersion001::Resolve(
    const FileSystemEntryPair& aEndpoints) const {
  QM_TRY_UNWRAP(Path path, ResolveReversedPath(mConnection, aEndpoints));

  path.Reverse();
  return path;
}

void FileSystemDatabaseManagerVersion001::Close() { mConnection->Close(); }

}  // namespace fs::data

}  // namespace mozilla::dom
