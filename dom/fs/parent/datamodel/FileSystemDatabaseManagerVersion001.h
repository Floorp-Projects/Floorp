/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATABASEMANAGERVERSION001_H_
#define DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATABASEMANAGERVERSION001_H_

#include "FileSystemDatabaseManager.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "nsString.h"

namespace mozilla::dom::fs::data {

class FileSystemDataManager;
class FileSystemFileManager;
using FileSystemConnection = fs::ResultConnection;

/**
 * @brief Versioned implementation of database interface enables backwards
 * support after the schema has changed. Version number 0 refers to
 * uninitialized database, and versions after that are sequential upgrades.
 *
 * To change the schema to the next version x,
 * - a new implementation FileSystemDatabaseManagerVersion00x is derived from
 * the previous version and the required methods are overridden
 * - a new apppropriate schema initialization class SchemaVersion00x is created
 * or derived
 * - the factory method of FileSystemDatabaseManager is extended to try to
 * migrate the data from the previous version to version x, and to return
 * FileSystemDatabaseManagerVersion00x implementation if the database version
 * after the migrations is x
 * - note that if the migration fails at some old version, the corresponding
 * old implementation should be returned: this way the users whose migrations
 * fail systematically due to hardware or other issues will not get locked out
 */
class FileSystemDatabaseManagerVersion001 : public FileSystemDatabaseManager {
 public:
  FileSystemDatabaseManagerVersion001(
      FileSystemDataManager* aDataManager, FileSystemConnection&& aConnection,
      UniquePtr<FileSystemFileManager>&& aFileManager,
      const EntryId& aRootEntry);

  /* Static to allow use by quota client without instantiation */
  static nsresult RescanTrackedUsages(
      const FileSystemConnection& aConnection,
      const quota::OriginMetadata& aOriginMetadata);

  /* Static to allow use by quota client without instantiation */
  static Result<Usage, QMResult> GetFileUsage(
      const FileSystemConnection& aConnection);

  virtual nsresult UpdateUsage(const EntryId& aEntry) override;

  virtual Result<EntryId, QMResult> GetOrCreateDirectory(
      const FileSystemChildMetadata& aHandle, bool aCreate) override;

  virtual Result<EntryId, QMResult> GetOrCreateFile(
      const FileSystemChildMetadata& aHandle, bool aCreate) override;

  virtual nsresult GetFile(const EntryId& aEntryId, nsString& aType,
                           TimeStamp& lastModifiedMilliSeconds, Path& aPath,
                           nsCOMPtr<nsIFile>& aFile) const override;

  virtual Result<FileSystemDirectoryListing, QMResult> GetDirectoryEntries(
      const EntryId& aParent, PageNumber aPage) const override;

  virtual Result<bool, QMResult> RenameEntry(
      const FileSystemEntryMetadata& aHandle, const Name& aNewName) override;

  virtual Result<bool, QMResult> MoveEntry(
      const FileSystemEntryMetadata& aHandle,
      const FileSystemChildMetadata& aNewDesignation) override;

  virtual Result<bool, QMResult> RemoveDirectory(
      const FileSystemChildMetadata& aHandle, bool aRecursive) override;

  virtual Result<bool, QMResult> RemoveFile(
      const FileSystemChildMetadata& aHandle) override;

  virtual Result<Path, QMResult> Resolve(
      const FileSystemEntryPair& aEndpoints) const override;

  virtual void Close() override;

  virtual nsresult BeginUsageTracking(const EntryId& aEntryId) override;

  virtual nsresult EndUsageTracking(const EntryId& aEntryId) override;

  virtual ~FileSystemDatabaseManagerVersion001() = default;

 private:
  nsresult UpdateUsageInDatabase(const EntryId& aEntry, Usage aNewDiskUsage);

  Result<Ok, QMResult> EnsureUsageIsKnown(const EntryId& aEntryId);

  void DecreaseCachedQuotaUsage(int64_t aDelta);

  nsresult UpdateCachedQuotaUsage(const EntryId& aEntryId, Usage aOldUsage,
                                  Usage aNewUsage);

  nsresult ClearDestinationIfNotLocked(
      const FileSystemConnection& aConnection,
      const FileSystemDataManager* const aDataManager,
      const FileSystemEntryMetadata& aHandle,
      const FileSystemChildMetadata& aNewDesignation);

  nsresult PrepareMoveEntry(const FileSystemConnection& aConnection,
                            const FileSystemDataManager* const aDataManager,
                            const FileSystemEntryMetadata& aHandle,
                            const FileSystemChildMetadata& aNewDesignation,
                            bool aIsFile);

  nsresult PrepareRenameEntry(const FileSystemConnection& aConnection,
                              const FileSystemDataManager* const aDataManager,
                              const FileSystemEntryMetadata& aHandle,
                              const Name& aNewName, bool aIsFile);

  // This is a raw pointer since we're owned by the FileSystemDataManager.
  FileSystemDataManager* MOZ_NON_OWNING_REF mDataManager;

  FileSystemConnection mConnection;

  UniquePtr<FileSystemFileManager> mFileManager;

  const EntryId mRootEntry;

  const quota::ClientMetadata mClientMetadata;

  int32_t mFilesOfUnknownUsage;
};

}  // namespace mozilla::dom::fs::data

#endif  // DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATABASEMANAGERVERSION001_H_
