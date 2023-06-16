/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDatabaseManagerVersion002.h"

#include "FileSystemDataManager.h"
#include "FileSystemFileManager.h"
#include "FileSystemParentTypes.h"
#include "ResultStatement.h"
#include "mozStorageHelper.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/PFileSystemManager.h"
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

nsresult AddNewFileId(const FileSystemConnection& aConnection,
                      const EntryId& aEntryId) {
  const nsLiteralCString insertFileIdQuery =
      "INSERT INTO FileIds ( fileId, handle ) "
      "VALUES ( :entryId, :entryId ) ;"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(aConnection, insertFileIdQuery)
                    .mapErr(toNSResult));

  QM_TRY(MOZ_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));

  QM_TRY(MOZ_TO_RESULT(stmt.Execute()));

  return NS_OK;
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
  const nsLiteralCString doesEntryAlreadyHaveFile =
      "SELECT EXISTS ( SELECT 1 FROM FileIds WHERE handle = :entryId );"_ns;

  QM_TRY_UNWRAP(ResultStatement stmt,
                ResultStatement::Create(mConnection, doesEntryAlreadyHaveFile));
  QM_TRY(MOZ_TO_RESULT(stmt.BindEntryIdByName("entryId"_ns, aEntryId)));
  QM_TRY_UNWRAP(const bool isEnsured, stmt.YesOrNoQuery());

  if (isEnsured) {
    return NS_OK;
  }

  QM_TRY(MOZ_TO_RESULT(AddNewFileId(mConnection, aEntryId)));

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
  // TODO: Implement this properly
  return Err(QMResult(NS_ERROR_NOT_IMPLEMENTED));
}

Result<EntryId, QMResult> FileSystemDatabaseManagerVersion002::MoveEntry(
    const FileSystemEntryMetadata& aHandle,
    const FileSystemChildMetadata& aNewDesignation) {
  // TODO: Implement this properly
  return Err(QMResult(NS_ERROR_NOT_IMPLEMENTED));
}

}  // namespace mozilla::dom::fs::data
