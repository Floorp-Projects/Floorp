/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMDATAMANAGER_H_
#define DOM_FS_PARENT_FILESYSTEMDATAMANAGER_H_

#include "ResultConnection.h"

#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/POriginPrivateFileSystem.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/QuotaCommon.h"

template <class T>
class nsTArray;

namespace mozilla::dom {

namespace fs {
class FileSystemChildMetadata;
class FileSystemEntryMetadata;
class FileSystemEntryPair;
}  // namespace fs

using FileSystemEntries = nsTArray<fs::FileSystemEntryMetadata>;

namespace fs::data {

class FileSystemFileManager;

using FileSystemConnection = fs::ResultConnection;

Result<EntryId, QMResult> GetRootHandle(const Origin& origin);
Result<EntryId, QMResult> GetEntryHandle(
    const FileSystemChildMetadata& aHandle);

class FileSystemDataManager {
 public:
  static Result<FileSystemDataManager*, QMResult> CreateFileSystemDataManager(
      const Origin& aOrigin);

  /**
   * @brief Returns current total usage
   *
   * @return Result<int64_t, QMResult> Usage or error
   */
  virtual Result<int64_t, QMResult> GetUsage() const = 0;

  /**
   * @brief Returns directory identifier, optionally creating it if it doesn't
   * exist
   *
   * @param aHandle Current directory and filename
   * @return Result<bool, QMResult> Directory identifier or error
   */
  virtual Result<EntryId, QMResult> GetOrCreateDirectory(
      const FileSystemChildMetadata& aHandle, bool aCreate) = 0;

  /**
   * @brief Returns file identifier, optionally creating it if it doesn't exist
   *
   * @param aHandle Current directory and filename
   * @return Result<bool, QMResult> File identifier or error
   */
  virtual Result<EntryId, QMResult> GetOrCreateFile(
      const FileSystemChildMetadata& aHandle, bool aCreate) = 0;

  /**
   * @brief Returns the properties of a file corresponding to a file handle
   */
  virtual nsresult GetFile(const FileSystemEntryPair& aEndpoints,
                           nsString& aType, TimeStamp& lastModifiedMilliSeconds,
                           Path& aPath, nsCOMPtr<nsIFile>& aFile) const = 0;

  virtual Result<FileSystemDirectoryListing, QMResult> GetDirectoryEntries(
      const EntryId& aParent, PageNumber aPage) const = 0;

  /**
   * @brief Removes a directory
   *
   * @param aHandle Current directory and filename
   * @return Result<bool, QMResult> False if file did not exist, otherwise true
   * or error
   */
  virtual Result<bool, QMResult> RemoveDirectory(
      const FileSystemChildMetadata& aHandle, bool aRecursive) = 0;

  /**
   * @brief Removes a file
   *
   * @param aHandle Current directory and filename
   * @return Result<bool, QMResult> False if file did not exist, otherwise true
   * or error
   */
  virtual Result<bool, QMResult> RemoveFile(
      const FileSystemChildMetadata& aHandle) = 0;

  /**
   * @brief Tries to connect a parent directory to a file system item with a
   * path, excluding the parent directory
   *
   * @param aHandle Pair of parent directory and child item candidates
   * @return Result<Path, QMResult> Path or error if no it didn't exists
   */
  virtual Result<Path, QMResult> Resolve(
      const FileSystemEntryPair& aEndpoints) const = 0;

  virtual inline ~FileSystemDataManager() = default;
};

}  // namespace fs::data

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMDATAMANAGER_H_
