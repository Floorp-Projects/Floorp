/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDatabaseManagerVersion002.h"

#include "ErrorList.h"
#include "FileSystemContentTypeGuess.h"
#include "FileSystemDataManager.h"
#include "FileSystemFileManager.h"
#include "FileSystemHashSource.h"
#include "FileSystemHashStorageFunction.h"
#include "FileSystemParentTypes.h"
#include "ResultStatement.h"
#include "mozStorageHelper.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::fs::data {

namespace {

Result<FileId, QMResult> GetFileId002(const FileSystemConnection& aConnection,
                                      const EntryId& aEntryId) {
  const nsLiteralCString fileIdQuery =
      "SELECT fileId FROM FileIds WHERE handle = :entryId ;"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, fileIdQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));
  QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());

  if (!moreResults) {
    return FileId(aEntryId);
  }

  // XXX: Introduce GetFileIdByColumn
  QM_TRY_UNWRAP(EntryId fileId, stmt.GetEntryIdByColumn(/* Column */ 0u));

  return FileId(fileId);
}

Result<bool, QMResult> DoesFileIdExist(const FileSystemConnection& aConnection,
                                       const FileId& aFileId) {
  MOZ_ASSERT(!aFileId.IsEmpty());

  const nsLiteralCString existsQuery =
      "SELECT EXISTS "
      "(SELECT 1 FROM FileIds WHERE fileId = :handle ) "
      ";"_ns;

  QM_TRY_RETURN(
      ApplyEntryExistsQuery(aConnection, existsQuery, aFileId.Value()));
}

nsresult RehashFile(const FileSystemConnection& aConnection,
                    const EntryId& aEntryId,
                    const FileSystemChildMetadata& aNewDesignation,
                    const ContentType& aNewType) {
  QM_TRY_INSPECT(const EntryId& newId,
                 FileSystemHashSource::GenerateHash(
                     aNewDesignation.parentId(), aNewDesignation.childName()));

  // The destination should be empty at this point: either we exited because
  // overwrite was not desired, or the existing content was removed.
  const nsLiteralCString insertNewEntryQuery =
      "INSERT INTO Entries ( handle, parent ) "
      "VALUES ( :newId, :newParent ) "
      ";"_ns;

  const nsLiteralCString insertNewFileQuery =
      "INSERT INTO Files ( handle, type, name ) "
      "VALUES ( :newId, :type, :newName ) "
      ";"_ns;

  const nsLiteralCString updateFileMappingsQuery =
      "UPDATE FileIds SET handle = :newId WHERE handle = :handle ;"_ns;

  const nsLiteralCString cleanupOldEntryQuery =
      "DELETE FROM Entries WHERE handle = :handle ;"_ns;

  mozStorageTransaction transaction(
      aConnection.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  QM_TRY(QM_TO_RESULT(transaction.Start()));

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, insertNewEntryQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("newId"_ns, newId)));
    QM_TRY(QM_TO_RESULT(
        stmt.BindEntryIdByName("newParent"_ns, aNewDesignation.parentId())));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, insertNewFileQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("newId"_ns, newId)));
    QM_TRY(QM_TO_RESULT(stmt.BindContentTypeByName("type"_ns, aNewType)));
    QM_TRY(QM_TO_RESULT(
        stmt.BindNameByName("newName"_ns, aNewDesignation.childName())));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(
        ResultStatement stmt,
        ResultStatement::Create(aConnection, updateFileMappingsQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("newId"_ns, newId)));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, aEntryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, cleanupOldEntryQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, aEntryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  return NS_OK;
}

nsresult RehashDirectory(const FileSystemConnection& aConnection,
                         const EntryId& aEntryId,
                         const FileSystemChildMetadata& aNewDesignation) {
  // This name won't match up with the entryId for the old path but
  // it will be removed at the end
  const nsLiteralCString updateNameQuery =
      "UPDATE Directories SET name = :newName WHERE handle = :handle "
      "; "_ns;

  const nsLiteralCString calculateHashesQuery =
      "CREATE TEMPORARY TABLE ParentChildHash AS "
      "WITH RECURSIVE "
      "rehashMap( depth, isFile, handle, parent, name, hash ) AS ( "
      "SELECT 0, isFile, handle, parent, name, hashEntry( :newParent, name ) "
      "FROM EntryNames WHERE handle = :handle UNION SELECT "
      "1 + depth, EntryNames.isFile, EntryNames.handle, EntryNames.parent, "
      "EntryNames.name, hashEntry( rehashMap.hash, EntryNames.name ) "
      "FROM rehashMap, EntryNames WHERE rehashMap.handle = EntryNames.parent ) "
      "SELECT depth, isFile, handle, parent, name, hash FROM rehashMap "
      ";"_ns;

  const nsLiteralCString createIndexByDepthQuery =
      "CREATE INDEX indexOnDepth ON ParentChildHash ( depth ); "_ns;

  // To avoid constraint violation, we insert new entries under the old parent.
  // The destination should be empty at this point: either we exited because
  // overwrite was not desired, or the existing content was removed.
  const nsLiteralCString insertNewEntriesQuery =
      "INSERT INTO Entries ( handle, parent ) "
      "SELECT hash, :parent FROM ParentChildHash "
      ";"_ns;

  const nsLiteralCString insertNewDirectoriesQuery =
      "INSERT INTO Directories ( handle, name ) "
      "SELECT hash, name FROM ParentChildHash WHERE isFile = 0 "
      "ORDER BY depth "
      ";"_ns;

  const nsLiteralCString insertNewFilesQuery =
      "INSERT INTO Files ( handle, name ) "
      "SELECT hash, name FROM ParentChildHash WHERE isFile = 1 "
      ";"_ns;

  const nsLiteralCString updateFileMappingsQuery =
      "UPDATE FileIds SET handle = hash "
      "FROM ( SELECT handle, hash FROM ParentChildHash ) AS replacement "
      "WHERE FileIds.handle = replacement.handle "
      ";"_ns;

  // Now fix the parents
  const nsLiteralCString updateEntryMappingsQuery =
      "UPDATE Entries SET parent = hash "
      "FROM ( SELECT Lhs.hash AS handle, Rhs.hash AS hash, Lhs.depth AS depth "
      "FROM ParentChildHash AS Lhs "
      "INNER JOIN ParentChildHash AS Rhs "
      "ON Rhs.handle = Lhs.parent ORDER BY depth ) AS replacement "
      "WHERE Entries.handle = replacement.handle "
      ";"_ns;

  const nsLiteralCString cleanupOldEntriesQuery =
      "DELETE FROM Entries WHERE handle = :handle "
      ";"_ns;

  // Index is automatically deleted
  const nsLiteralCString cleanupTemporaries =
      "DROP TABLE ParentChildHash "
      ";"_ns;

  nsCOMPtr<mozIStorageFunction> rehashFunction =
      new data::FileSystemHashStorageFunction();
  QM_TRY(MOZ_TO_RESULT(aConnection->CreateFunction("hashEntry"_ns,
                                                   /* number of arguments */ 2,
                                                   rehashFunction)));
  auto finallyRemoveFunction = MakeScopeExit([&aConnection]() {
    QM_WARNONLY_TRY(MOZ_TO_RESULT(aConnection->RemoveFunction("hashEntry"_ns)));
  });

  mozStorageTransaction transaction(
      aConnection.get(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  QM_TRY(QM_TO_RESULT(transaction.Start()));

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, updateNameQuery));
    QM_TRY(QM_TO_RESULT(
        stmt.BindNameByName("newName"_ns, aNewDesignation.childName())));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, aEntryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, calculateHashesQuery));
    QM_TRY(QM_TO_RESULT(
        stmt.BindEntryIdByName("newParent"_ns, aNewDesignation.parentId())));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, aEntryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(aConnection->ExecuteSimpleSQL(createIndexByDepthQuery)));

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, insertNewEntriesQuery));
    QM_TRY(QM_TO_RESULT(
        stmt.BindEntryIdByName("parent"_ns, aNewDesignation.parentId())));
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
    QM_TRY_UNWRAP(
        ResultStatement stmt,
        ResultStatement::Create(aConnection, updateEntryMappingsQuery));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, cleanupOldEntriesQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, aEntryId)));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, cleanupTemporaries));
    QM_TRY(QM_TO_RESULT(stmt.Execute()));
  }

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  return NS_OK;
}

/**
 * @brief Each entryId is interpreted as a large integer, which is increased
 * until an unused value is found. This process is in principle infallible.
 * The files associated with a given path will form a cluster next to the
 * entryId which could be used for recovery because our hash function is
 * expected to distribute all clusters far from each other.
 */
Result<FileId, QMResult> GetNextFreeFileId(
    const FileSystemConnection& aConnection,
    const FileSystemFileManager& aFileManager, const EntryId& aEntryId) {
  MOZ_ASSERT(32u == aEntryId.Length());

  auto DoesExist = [&aConnection, &aFileManager](
                       const FileId& aId) -> Result<bool, QMResult> {
    QM_TRY_INSPECT(const nsCOMPtr<nsIFile>& diskFile,
                   aFileManager.GetFile(aId));

    bool result = true;
    QM_TRY(QM_TO_RESULT(diskFile->Exists(&result)));
    if (result) {
      return true;
    }

    QM_TRY_RETURN(DoesFileIdExist(aConnection, aId));
  };

  auto Next = [](FileId& aId) {
    // Using a larger integer would make fileIds depend on platform endianness.
    using IntegerType = uint8_t;
    constexpr int32_t bufferSize = 32 / sizeof(IntegerType);
    using IdBuffer = std::array<IntegerType, bufferSize>;

    auto Increase = [](IdBuffer& aIn) {
      for (int i = 0; i < bufferSize; ++i) {
        if (1u + aIn[i] != 0u) {
          ++aIn[i];
          return;
        }
        aIn[i] = 0u;
      }
    };

    DebugOnly<nsCString> original = aId.Value();
    Increase(*reinterpret_cast<IdBuffer*>(aId.mValue.BeginWriting()));
    MOZ_ASSERT(!aId.Value().Equals(original));
  };

  FileId id = FileId(aEntryId);

  while (true) {
    QM_WARNONLY_TRY_UNWRAP(Maybe<bool> maybeExists, DoesExist(id));
    if (maybeExists.isSome() && !maybeExists.value()) {
      return id;
    }

    Next(id);
  }
}

nsresult AddNewFileId(const FileSystemConnection& aConnection,
                      const FileSystemFileManager& aFileManager,
                      const EntryId& aEntryId) {
  QM_TRY_INSPECT(const FileId& nextFreeId,
                 GetNextFreeFileId(aConnection, aFileManager, aEntryId)
                     .mapErr(toNSResult));

  const nsLiteralCString insertNewFileIdQuery =
      "INSERT INTO FileIds ( fileId, handle ) "
      "VALUES ( :fileId, :entryId ) "
      "; "_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, insertNewFileIdQuery)
                    .mapErr(toNSResult));
  QM_TRY(MOZ_TO_RESULT(stmt.BindFileIdByName("fileId"_ns, nextFreeId)));
  QM_TRY(MOZ_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));

  QM_TRY(MOZ_TO_RESULT(stmt.Execute()));

  return NS_OK;
}

}  // namespace

/* static */
nsresult FileSystemDatabaseManagerVersion002::RescanTrackedUsages(
    const FileSystemConnection& aConnection,
    const quota::OriginMetadata& aOriginMetadata) {
  return FileSystemDatabaseManagerVersion001::RescanTrackedUsages(
      aConnection, aOriginMetadata);
}

/* static */
Result<Usage, QMResult> FileSystemDatabaseManagerVersion002::GetFileUsage(
    const FileSystemConnection& aConnection) {
  return FileSystemDatabaseManagerVersion001::GetFileUsage(aConnection);
}

Result<EntryId, QMResult> FileSystemDatabaseManagerVersion002::GetEntryId(
    const FileSystemChildMetadata& aHandle) const {
  return fs::data::GetEntryHandle(aHandle);
}

nsresult FileSystemDatabaseManagerVersion002::EnsureFileId(
    const EntryId& aEntryId) {
  QM_TRY_UNWRAP(bool exists,
                DoesFileExist(mConnection, aEntryId).mapErr(toNSResult));
  if (!exists) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  const nsLiteralCString doesEntryAlreadyHaveFile =
      "SELECT EXISTS ( SELECT 1 FROM FileIds WHERE handle = :entryId );"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, doesEntryAlreadyHaveFile));
  QM_TRY(MOZ_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));
  QM_TRY_UNWRAP(const bool isEnsured, stmt.YesOrNoQuery());

  if (isEnsured) {
    return NS_OK;
  }

  // The query fails if and only if no path exists: already ruled out
  QM_TRY(MOZ_TO_RESULT(AddNewFileId(mConnection, *mFileManager, aEntryId)));

  return NS_OK;
}

Result<FileId, QMResult> FileSystemDatabaseManagerVersion002::GetFileId(
    const EntryId& aEntryId) const {
  MOZ_ASSERT(mConnection);
  return data::GetFileId002(mConnection, aEntryId);
}

Result<nsTArray<FileId>, QMResult>
FileSystemDatabaseManagerVersion002::FindDescendants(
    const EntryId& aEntryId) const {
  const nsLiteralCString descendantsQuery =
      "WITH RECURSIVE traceChildren(handle, parent) AS ( "
      "SELECT handle, parent FROM Entries WHERE handle = :handle "
      "UNION "
      "SELECT Entries.handle, Entries.parent FROM traceChildren, Entries "
      "WHERE traceChildren.handle = Entries.parent ) "
      "SELECT FileIds.fileId "
      "FROM traceChildren INNER JOIN FileIds USING (handle) "
      ";"_ns;

  nsTArray<FileId> descendants;
  {
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, descendantsQuery));
    QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, aEntryId)));

    while (true) {
      QM_TRY_INSPECT(const bool& moreResults, stmt.ExecuteStep());
      if (!moreResults) {
        break;
      }

      QM_TRY_INSPECT(const FileId& fileId,
                     stmt.GetFileIdByColumn(/* Column */ 0u));
      descendants.AppendElement(fileId);
    }
  }

  return descendants;
}

Result<bool, QMResult> FileSystemDatabaseManagerVersion002::DoesFileIdExist(
    const FileId& aFileId) const {
  QM_TRY_RETURN(data::DoesFileIdExist(mConnection, aFileId));
}

Result<Usage, QMResult>
FileSystemDatabaseManagerVersion002::GetUsagesOfDescendants(
    const EntryId& aEntryId) const {
  const nsLiteralCString descendantUsagesQuery =
      "WITH RECURSIVE traceChildren(handle, parent) AS ( "
      "SELECT handle, parent FROM Entries WHERE handle = :handle "
      "UNION "
      "SELECT Entries.handle, Entries.parent FROM traceChildren, Entries "
      "WHERE traceChildren.handle=Entries.parent ) "
      "SELECT sum(Usages.usage) "
      "FROM traceChildren "
      "INNER JOIN FileIds ON traceChildren.handle = FileIds.handle "
      "INNER JOIN Usages ON Usages.handle = FileIds.fileId "
      ";"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, descendantUsagesQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("handle"_ns, aEntryId)));
  QM_TRY_UNWRAP(const bool moreResults, stmt.ExecuteStep());
  if (!moreResults) {
    return 0;
  }

  QM_TRY_RETURN(stmt.GetUsageByColumn(/* Column */ 0u));
}

nsresult FileSystemDatabaseManagerVersion002::RemoveFileId(
    const FileId& aFileId) {
  const nsLiteralCString removeFileIdQuery =
      "DELETE FROM FileIds "
      "WHERE fileId = :fileId "
      ";"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, removeFileIdQuery)
                    .mapErr(toNSResult));

  QM_TRY(MOZ_TO_RESULT(stmt.BindEntryIdByName("fileId"_ns, aFileId.Value())));

  return stmt.Execute();
}

Result<EntryId, QMResult> FileSystemDatabaseManagerVersion002::RenameEntry(
    const FileSystemEntryMetadata& aHandle, const Name& aNewName) {
  MOZ_ASSERT(!aNewName.IsEmpty());

  const auto& entryId = aHandle.entryId();
  MOZ_ASSERT(!entryId.IsEmpty());

  // Can't rename root
  if (mRootEntry == entryId) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  // Verify the source exists
  QM_TRY_UNWRAP(bool isFile, IsFile(mConnection, entryId),
                Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR)));

  // Are we actually renaming?
  if (aHandle.entryName() == aNewName) {
    return entryId;
  }

  QM_TRY(QM_TO_RESULT(PrepareRenameEntry(mConnection, mDataManager, aHandle,
                                         aNewName, isFile)));

  QM_TRY_UNWRAP(EntryId parentId, FindParent(mConnection, entryId));
  FileSystemChildMetadata newDesignation(parentId, aNewName);

  if (isFile) {
    const ContentType type = FileSystemContentTypeGuess::FromPath(aNewName);
    QM_TRY(
        QM_TO_RESULT(RehashFile(mConnection, entryId, newDesignation, type)));
  } else {
    QM_TRY(QM_TO_RESULT(RehashDirectory(mConnection, entryId, newDesignation)));
  }

  QM_TRY_UNWRAP(DebugOnly<EntryId> dbId,
                FindEntryId(mConnection, newDesignation, isFile));
  QM_TRY_UNWRAP(EntryId generated,
                FileSystemHashSource::GenerateHash(parentId, aNewName));
  MOZ_ASSERT(static_cast<EntryId&>(dbId).Equals(generated));

  return generated;
}

Result<EntryId, QMResult> FileSystemDatabaseManagerVersion002::MoveEntry(
    const FileSystemEntryMetadata& aHandle,
    const FileSystemChildMetadata& aNewDesignation) {
  MOZ_ASSERT(!aHandle.entryId().IsEmpty());

  const auto& entryId = aHandle.entryId();

  if (mRootEntry == entryId) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  // Verify the source exists
  QM_TRY_UNWRAP(bool isFile, IsFile(mConnection, entryId),
                Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR)));

  // If the rename doesn't change the name or directory, just return success.
  // XXX Needs to be added to the spec
  QM_WARNONLY_TRY_UNWRAP(Maybe<bool> maybeSame,
                         IsSame(mConnection, aHandle, aNewDesignation, isFile));
  if (maybeSame && maybeSame.value()) {
    return entryId;
  }

  QM_TRY(QM_TO_RESULT(PrepareMoveEntry(mConnection, mDataManager, aHandle,
                                       aNewDesignation, isFile)));

  if (isFile) {
    const ContentType type =
        FileSystemContentTypeGuess::FromPath(aNewDesignation.childName());
    QM_TRY(
        QM_TO_RESULT(RehashFile(mConnection, entryId, aNewDesignation, type)));
  } else {
    QM_TRY(
        QM_TO_RESULT(RehashDirectory(mConnection, entryId, aNewDesignation)));
  }

  QM_TRY_UNWRAP(DebugOnly<EntryId> dbId,
                FindEntryId(mConnection, aNewDesignation, isFile));
  QM_TRY_UNWRAP(EntryId generated,
                FileSystemHashSource::GenerateHash(
                    aNewDesignation.parentId(), aNewDesignation.childName()));
  MOZ_ASSERT(static_cast<EntryId&>(dbId).Equals(generated));

  return generated;
}

}  // namespace mozilla::dom::fs::data
