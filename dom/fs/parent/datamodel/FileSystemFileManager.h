/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_DATAMODEL_FILESYSTEMFILEMANAGER_H_
#define DOM_FS_PARENT_DATAMODEL_FILESYSTEMFILEMANAGER_H_

#include "ErrorList.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/QMResult.h"
#include "nsIFile.h"
#include "nsString.h"

template <class T>
class nsCOMPtr;

class nsIFileURL;

namespace mozilla::dom {

namespace quota {

struct OriginMetadata;

}  // namespace quota

namespace fs {

struct FileId;

namespace data {

/**
 * @brief Get the directory for file system items of specified origin.
 * Use this instead of constructing the path from quota manager's storage path.
 *
 * @param aOrigin Specified origin
 * @return Result<nsCOMPtr<nsIFile>, QMResult> Top file system directory
 */
Result<nsCOMPtr<nsIFile>, QMResult> GetFileSystemDirectory(
    const quota::OriginMetadata& aOriginMetadata);

/**
 * @brief Ensure that the origin-specific directory for file system exists.
 *
 * @param aOriginMetadata Specified origin metadata
 * @return nsresult Error if operation failed
 */
nsresult EnsureFileSystemDirectory(
    const quota::OriginMetadata& aOriginMetadata);

/**
 * @brief Get file system's database path for specified origin.
 * Use this to get the database path instead of constructing it from
 * quota manager's storage path - without the side effect of potentially
 * creating it.
 *
 * @param aOrigin Specified origin
 * @return Result<nsCOMPtr<nsIFile>, QMResult> Database file object
 */
Result<nsCOMPtr<nsIFile>, QMResult> GetDatabaseFile(
    const quota::OriginMetadata& aOriginMetadata);

/**
 * @brief Get file system's database url with directory lock parameter for
 * specified origin. Use this to open a database connection and have the quota
 * manager guard against its deletion or busy errors due to other connections.
 *
 * @param aOrigin Specified origin
 * @param aDirectoryLockId Directory lock id from the quota manager
 * @return Result<nsCOMPtr<nsIFileURL>, QMResult> Database file URL object
 */
Result<nsCOMPtr<nsIFileURL>, QMResult> GetDatabaseFileURL(
    const quota::OriginMetadata& aOriginMetadata,
    const int64_t aDirectoryLockId);

/**
 * @brief Creates and removes disk-backed representations of the file systems'
 * file entries for a specified origin.
 *
 * Other components should not depend on how the files are organized on disk
 * but instead rely on the entry id and have access to the local file using the
 * GetOrCreateFile result.
 *
 * The local paths are not necessarily stable in the long term and if they
 * absolutely must be cached, there should be a way to repopulate the cache
 * after an internal reorganization of the file entry represenations on disk,
 * for some currently unforeseen maintenance reason.
 *
 * Example: if GetOrCreateFile used to map entryId 'abc' to path '/c/u/1/123'
 * and now it maps it to '/d/u/1/12/123', the cache should either update all
 * paths at once through a migration, or purge them and save a new value
 * whenever a call to GetOrCreateFile is made.
 */
class FileSystemFileManager {
 public:
  /**
   * @brief Create a File System File Manager object for a specified origin.
   *
   * @param aOrigin
   * @return Result<FileSystemFileManager, QMResult>
   */
  static Result<UniquePtr<FileSystemFileManager>, QMResult>
  CreateFileSystemFileManager(const quota::OriginMetadata& aOriginMetadata);

  /**
   * @brief Create a File System File Manager object which keeps file entries
   * under a specified directory instead of quota manager's storage path.
   * This should only be used for testing and preferably removed.
   *
   * @param topDirectory
   * @return Result<FileSystemFileManager, QMResult>
   */
  static Result<FileSystemFileManager, QMResult> CreateFileSystemFileManager(
      nsCOMPtr<nsIFile>&& topDirectory);

  /**
   * @brief Get a file object for a specified entry id. If a file for the entry
   * does not exist, returns an appropriate error.
   *
   * @param aEntryId Specified id of a file system entry
   * @return Result<nsCOMPtr<nsIFile>, QMResult> File or error.
   */
  Result<nsCOMPtr<nsIFile>, QMResult> GetFile(const FileId& aFileId) const;

  /**
   * @brief Get or create a disk-backed file object for a specified entry id.
   *
   * @param aFileId Specified id of a file system entry
   * @return Result<nsCOMPtr<nsIFile>, QMResult> File abstraction or IO error
   */
  Result<nsCOMPtr<nsIFile>, QMResult> GetOrCreateFile(const FileId& aFileId);

  /**
   * @brief Create a disk-backed file object as a copy.
   *
   * @param aDestinationFileId Specified id of file to be created
   * @param aSourceFileId Specified id of the file from which we make a copy
   * @return Result<nsCOMPtr<nsIFile>, QMResult> File abstraction or IO error
   */
  Result<nsCOMPtr<nsIFile>, QMResult> CreateFileFrom(
      const FileId& aDestinationFileId, const FileId& aSourceFileId);

  /**
   * @brief Remove the disk-backed file object for a specified entry id.
   * Note: The returned value is 0 in release builds.
   *
   * @param aFileId  Specified id of a file system entry
   * @return Result<Usage, QMResult> Error or file size
   */
  Result<Usage, QMResult> RemoveFile(const FileId& aFileId);

  /**
   * @brief This method can be used to try to delete a group of files from the
   * disk. In debug builds, the sum of the usages is provided ad return value,
   * in release builds the sum is not calculated.
   * The method attempts to remove all the files requested.
   */
  Result<DebugOnly<Usage>, QMResult> RemoveFiles(
      const nsTArray<FileId>& aFileIds, nsTArray<FileId>& aFailedRemovals);

 private:
  explicit FileSystemFileManager(nsCOMPtr<nsIFile>&& aTopDirectory);

  nsCOMPtr<nsIFile> mTopDirectory;
};

}  // namespace data
}  // namespace fs
}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_DATAMODEL_FILESYSTEMFILEMANAGER_H_
