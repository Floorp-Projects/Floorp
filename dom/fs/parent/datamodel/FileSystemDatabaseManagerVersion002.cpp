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
#include "StartedTransaction.h"
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
      "SELECT fileId FROM MainFiles WHERE handle = :entryId ;"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, fileIdQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));
  QM_TRY_UNWRAP(bool moreResults, stmt.ExecuteStep());

  if (!moreResults) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  QM_TRY_INSPECT(const FileId& fileId, stmt.GetFileIdByColumn(/* Column */ 0u));

  return fileId;
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

  const nsLiteralCString insertNewFileAndTypeQuery =
      "INSERT INTO Files ( handle, type, name ) "
      "VALUES ( :newId, :type, :newName ) "
      ";"_ns;

  const nsLiteralCString insertNewFileKeepTypeQuery =
      "INSERT INTO Files ( handle, type, name ) "
      "SELECT :newId, type, :newName FROM Files "
      "WHERE handle = :oldId ;"_ns;

  const auto& insertNewFileQuery = aNewType.IsVoid()
                                       ? insertNewFileKeepTypeQuery
                                       : insertNewFileAndTypeQuery;

  const nsLiteralCString updateFileMappingsQuery =
      "UPDATE FileIds SET handle = :newId WHERE handle = :handle ;"_ns;

  const nsLiteralCString updateMainFilesQuery =
      "UPDATE MainFiles SET handle = :newId WHERE handle = :handle ;"_ns;

  const nsLiteralCString cleanupOldEntryQuery =
      "DELETE FROM Entries WHERE handle = :handle ;"_ns;

  QM_TRY_UNWRAP(auto transaction, StartedTransaction::Create(aConnection));

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
    if (aNewType.IsVoid()) {
      QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("oldId"_ns, aEntryId)));
    } else {
      QM_TRY(QM_TO_RESULT(stmt.BindContentTypeByName("type"_ns, aNewType)));
    }
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
                  ResultStatement::Create(aConnection, updateMainFilesQuery));
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
      "INSERT INTO Files ( handle, type, name ) "
      "SELECT ParentChildHash.hash, Files.type, ParentChildHash.name "
      "FROM ParentChildHash INNER JOIN Files USING (handle) "
      "WHERE ParentChildHash.isFile = 1 "
      ";"_ns;

  const nsLiteralCString updateFileMappingsQuery =
      "UPDATE FileIds SET handle = hash "
      "FROM ( SELECT handle, hash FROM ParentChildHash ) AS replacement "
      "WHERE FileIds.handle = replacement.handle "
      ";"_ns;

  const nsLiteralCString updateMainFilesQuery =
      "UPDATE MainFiles SET handle = hash "
      "FROM ( SELECT handle, hash FROM ParentChildHash ) AS replacement "
      "WHERE MainFiles.handle = replacement.handle "
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

  QM_TRY_UNWRAP(auto transaction, StartedTransaction::Create(aConnection));

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
    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(aConnection, updateMainFilesQuery));
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

Result<FileId, QMResult> AddNewFileId(const FileSystemConnection& aConnection,
                                      const FileSystemFileManager& aFileManager,
                                      const EntryId& aEntryId) {
  QM_TRY_INSPECT(const FileId& nextFreeId,
                 GetNextFreeFileId(aConnection, aFileManager, aEntryId));

  const nsLiteralCString insertNewFileIdQuery =
      "INSERT INTO FileIds ( fileId, handle ) "
      "VALUES ( :fileId, :entryId ) "
      "; "_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, insertNewFileIdQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindFileIdByName("fileId"_ns, nextFreeId)));
  QM_TRY(QM_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));

  QM_TRY(QM_TO_RESULT(stmt.Execute()));

  return nextFreeId;
}

/**
 * @brief Get recorded usage or zero if nothing was ever written to the file.
 * Removing files is only allowed when there is no lock on the file, and their
 * usage is either correctly recorded in the database during unlock, or nothing,
 * or they remain in tracked state and the quota manager assumes their usage to
 * be equal to the latest recorded value. In all cases, the latest recorded
 * value (or nothing) is the correct amount of quota to be released.
 */
Result<Usage, QMResult> GetKnownUsage(const FileSystemConnection& aConnection,
                                      const FileId& aFileId) {
  const nsLiteralCString trackedUsageQuery =
      "SELECT usage FROM Usages WHERE handle = :handle ;"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, trackedUsageQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindFileIdByName("handle"_ns, aFileId)));

  QM_TRY_UNWRAP(const bool moreResults, stmt.ExecuteStep());
  if (!moreResults) {
    return 0;
  }

  QM_TRY_RETURN(stmt.GetUsageByColumn(/* Column */ 0u));
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

nsresult FileSystemDatabaseManagerVersion002::GetFile(
    const EntryId& aEntryId, const FileId& aFileId, const FileMode& aMode,
    ContentType& aType, TimeStamp& lastModifiedMilliSeconds,
    nsTArray<Name>& aPath, nsCOMPtr<nsIFile>& aFile) const {
  MOZ_ASSERT(!aFileId.IsEmpty());

  const FileSystemEntryPair endPoints(mRootEntry, aEntryId);
  QM_TRY_UNWRAP(aPath, ResolveReversedPath(mConnection, endPoints));
  if (aPath.IsEmpty()) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  QM_TRY(MOZ_TO_RESULT(GetFileAttributes(mConnection, aEntryId, aType)));

  if (aMode == FileMode::SHARED_FROM_COPY) {
    QM_WARNONLY_TRY_UNWRAP(Maybe<FileId> mainFileId, GetFileId(aEntryId));
    if (mainFileId) {
      QM_TRY_UNWRAP(aFile,
                    mFileManager->CreateFileFrom(aFileId, mainFileId.value()));
    } else {
      // LockShared/EnsureTemporaryFileId has provided a brand new fileId.
      QM_TRY_UNWRAP(aFile, mFileManager->GetOrCreateFile(aFileId));
    }
  } else {
    MOZ_ASSERT(aMode == FileMode::EXCLUSIVE ||
               aMode == FileMode::SHARED_FROM_EMPTY);

    QM_TRY_UNWRAP(aFile, mFileManager->GetOrCreateFile(aFileId));
  }

  PRTime lastModTime = 0;
  QM_TRY(MOZ_TO_RESULT(aFile->GetLastModifiedTime(&lastModTime)));
  lastModifiedMilliSeconds = static_cast<TimeStamp>(lastModTime);

  aPath.Reverse();

  return NS_OK;
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
    const ContentType type = DetermineContentType(aNewName);
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
    const ContentType type = DetermineContentType(aNewDesignation.childName());
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

Result<EntryId, QMResult> FileSystemDatabaseManagerVersion002::GetEntryId(
    const FileSystemChildMetadata& aHandle) const {
  return fs::data::GetEntryHandle(aHandle);
}

Result<EntryId, QMResult> FileSystemDatabaseManagerVersion002::GetEntryId(
    const FileId& aFileId) const {
  const nsLiteralCString getEntryIdQuery =
      "SELECT handle FROM FileIds WHERE fileId = :fileId ;"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, getEntryIdQuery));
  QM_TRY(QM_TO_RESULT(stmt.BindFileIdByName("fileId"_ns, aFileId)));
  QM_TRY_UNWRAP(bool hasEntries, stmt.ExecuteStep());

  if (!hasEntries || stmt.IsNullByColumn(/* Column */ 0u)) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  QM_TRY_RETURN(stmt.GetEntryIdByColumn(/* Column */ 0u));
}

Result<FileId, QMResult> FileSystemDatabaseManagerVersion002::EnsureFileId(
    const EntryId& aEntryId) {
  QM_TRY_UNWRAP(bool exists, DoesFileExist(mConnection, aEntryId));
  if (!exists) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  QM_TRY_UNWRAP(Maybe<FileId> maybeMainFileId,
                QM_OR_ELSE_LOG_VERBOSE_IF(
                    // Expression.
                    GetFileId(aEntryId).map([](auto mainFileId) {
                      return Some(std::move(mainFileId));
                    }),
                    // Predicate.
                    IsSpecificError<NS_ERROR_DOM_NOT_FOUND_ERR>,
                    // Fallback.
                    ([](const auto&) -> Result<Maybe<FileId>, QMResult> {
                      return Maybe<FileId>{};
                    })));

  if (maybeMainFileId) {
    return *maybeMainFileId;
  }

  QM_TRY_UNWRAP(auto transaction, StartedTransaction::Create(mConnection));

  QM_TRY_INSPECT(const FileId& fileId,
                 AddNewFileId(mConnection, *mFileManager, aEntryId));

  QM_TRY(QM_TO_RESULT(MergeFileId(aEntryId, fileId, /* aAbort */ false)));

  QM_TRY(QM_TO_RESULT(transaction.Commit()));

  return fileId;
}

Result<FileId, QMResult>
FileSystemDatabaseManagerVersion002::EnsureTemporaryFileId(
    const EntryId& aEntryId) {
  QM_TRY_UNWRAP(bool exists, DoesFileExist(mConnection, aEntryId));
  if (!exists) {
    return Err(QMResult(NS_ERROR_DOM_NOT_FOUND_ERR));
  }

  QM_TRY_RETURN(AddNewFileId(mConnection, *mFileManager, aEntryId));
}

Result<FileId, QMResult> FileSystemDatabaseManagerVersion002::GetFileId(
    const EntryId& aEntryId) const {
  MOZ_ASSERT(mConnection);
  return data::GetFileId002(mConnection, aEntryId);
}

nsresult FileSystemDatabaseManagerVersion002::MergeFileId(
    const EntryId& aEntryId, const FileId& aFileId, bool aAbort) {
  MOZ_ASSERT(mConnection);

  auto doCleanUp = [this](const FileId& aCleanable) -> nsresult {
    // We need to clean up the old main file.
    QM_TRY_UNWRAP(Usage usage,
                  GetKnownUsage(mConnection, aCleanable).mapErr(toNSResult));

    QM_WARNONLY_TRY_UNWRAP(Maybe<Usage> removedUsage,
                           mFileManager->RemoveFile(aCleanable));

    if (removedUsage) {
      // Removal of file data was ok, update the related fileId and usage
      QM_WARNONLY_TRY(QM_TO_RESULT(RemoveFileId(aCleanable)));

      if (usage > 0) {  // Performance!
        DecreaseCachedQuotaUsage(usage);
      }

      // We only check the most common case. This can fail spuriously if an
      // external application writes to the file, or OS reports zero size due to
      // corruption.
      MOZ_ASSERT_IF(0 == mFilesOfUnknownUsage, usage == removedUsage.value());

      return NS_OK;
    }

    // Removal failed
    const nsLiteralCString forgetCleanable =
        "UPDATE FileIds SET handle = NULL WHERE fileId = :fileId ; "_ns;

    QM_TRY_UNWRAP(ResultStatement stmt,
                  ResultStatement::Create(mConnection, forgetCleanable)
                      .mapErr(toNSResult));
    QM_TRY(MOZ_TO_RESULT(stmt.BindFileIdByName("fileId"_ns, aCleanable)));
    QM_TRY(MOZ_TO_RESULT(stmt.Execute()));

    TryRemoveDuringIdleMaintenance({aCleanable});

    return NS_OK;
  };

  if (aAbort) {
    QM_TRY(MOZ_TO_RESULT(doCleanUp(aFileId)));

    return NS_OK;
  }

  QM_TRY_UNWRAP(
      Maybe<FileId> maybeOldFileId,
      QM_OR_ELSE_LOG_VERBOSE_IF(
          // Expression.
          GetFileId(aEntryId)
              .map([](auto oldFileId) { return Some(std::move(oldFileId)); })
              .mapErr(toNSResult),
          // Predicate.
          IsSpecificError<NS_ERROR_DOM_NOT_FOUND_ERR>,
          // Fallback.
          ErrToDefaultOk<Maybe<FileId>>));

  if (maybeOldFileId && *maybeOldFileId == aFileId) {
    return NS_OK;  // Nothing to do
  }

  // Main file changed
  const nsLiteralCString flagAsMainFileQuery =
      "INSERT INTO MainFiles ( handle, fileId ) "
      "VALUES ( :entryId, :fileId ) "
      "ON CONFLICT (handle) "
      "DO UPDATE SET fileId = excluded.fileId "
      "; "_ns;

  QM_TRY_UNWRAP(auto transaction, StartedTransaction::Create(mConnection));

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, flagAsMainFileQuery)
                    .mapErr(toNSResult));
  QM_TRY(MOZ_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));
  QM_TRY(MOZ_TO_RESULT(stmt.BindFileIdByName("fileId"_ns, aFileId)));

  QM_TRY(MOZ_TO_RESULT(stmt.Execute()));

  if (!maybeOldFileId) {
    // We successfully added a new main file and there is nothing to clean up.
    QM_TRY(MOZ_TO_RESULT(transaction.Commit()));

    return NS_OK;
  }

  MOZ_ASSERT(maybeOldFileId);
  MOZ_ASSERT(*maybeOldFileId != aFileId);

  QM_TRY(MOZ_TO_RESULT(doCleanUp(*maybeOldFileId)));

  // If the old fileId and usage were not deleted, main file update fails.
  QM_TRY(MOZ_TO_RESULT(transaction.Commit()));

  return NS_OK;
}

Result<bool, QMResult> FileSystemDatabaseManagerVersion002::DoesFileIdExist(
    const FileId& aFileId) const {
  QM_TRY_RETURN(data::DoesFileIdExist(mConnection, aFileId));
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

Result<nsTArray<FileId>, QMResult>
FileSystemDatabaseManagerVersion002::FindFilesUnderEntry(
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

}  // namespace mozilla::dom::fs::data
