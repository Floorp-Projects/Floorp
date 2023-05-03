/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATABASEMANAGER_H_
#define DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATABASEMANAGER_H_

#include "ResultConnection.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "nsStringFwd.h"

template <class T>
class nsCOMPtr;

class nsIFile;

namespace mozilla {

template <typename V, typename E>
class Result;

namespace dom {

namespace quota {

struct OriginMetadata;

}  // namespace quota

namespace fs {

class FileSystemChildMetadata;
class FileSystemEntryMetadata;
class FileSystemDirectoryListing;
class FileSystemEntryPair;

namespace data {

class FileSystemDatabaseManager {
 public:
  /**
   * @brief Updates stored usage data for all tracked files.
   *
   * @return nsresult error code
   */
  static nsresult RescanUsages(const ResultConnection& aConnection,
                               const quota::OriginMetadata& aOriginMetadata);

  /**
   * @brief Obtains the current total usage for origin and connection.
   *
   * @return Result<quota::UsageInfo, QMResult> On success,
   *  - field UsageInfo::DatabaseUsage contains the sum of current
   *    total database and file usage,
   *  - field UsageInfo::FileUsage is not used and should be equal to Nothing.
   *
   * If the disk is inaccessible, various IO related errors may be returned.
   */
  static Result<quota::UsageInfo, QMResult> GetUsage(
      const ResultConnection& aConnection,
      const quota::OriginMetadata& aOriginMetadata);

  /**
   * @brief Refreshes the stored file size.
   *
   * @param aEntry EntryId of the file whose size is refreshed.
   */
  virtual nsresult UpdateUsage(const EntryId& aEntry) = 0;

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
   * @param aType Content type which is ignored if the file already exists
   * @param aCreate true if file is to be created when it does not already exist
   * @return Result<bool, QMResult> File identifier or error
   */
  virtual Result<EntryId, QMResult> GetOrCreateFile(
      const FileSystemChildMetadata& aHandle, const ContentType& aType,
      bool aCreate) = 0;

  /**
   * @brief Returns the properties of a file corresponding to a file handle
   */
  virtual nsresult GetFile(const EntryId& aEntryId, ContentType& aType,
                           TimeStamp& lastModifiedMilliSeconds, Path& aPath,
                           nsCOMPtr<nsIFile>& aFile) const = 0;

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
   * @brief Rename a file/directory
   *
   * @param aHandle Source directory or file
   * @param aNewName New entry name
   * @return Result<bool, QMResult> False if entry didn't exist, otherwise true
   * or error
   */
  virtual Result<bool, QMResult> RenameEntry(
      const FileSystemEntryMetadata& aHandle, const Name& aNewName) = 0;

  /**
   * @brief Move a file/directory
   *
   * @param aHandle Source directory or file
   * @param aNewDesignation Destination directory and entry name
   * @return Result<bool, QMResult> False if entry didn't exist, otherwise true
   * or error
   */
  virtual Result<bool, QMResult> MoveEntry(
      const FileSystemEntryMetadata& aHandle,
      const FileSystemChildMetadata& aNewDesignation) = 0;

  /**
   * @brief Tries to connect a parent directory to a file system item with a
   * path, excluding the parent directory
   *
   * @param aHandle Pair of parent directory and child item candidates
   * @return Result<Path, QMResult> Path or error if no it didn't exists
   */
  virtual Result<Path, QMResult> Resolve(
      const FileSystemEntryPair& aEndpoints) const = 0;

  /**
   * @brief Close database connection.
   */
  virtual void Close() = 0;

  /**
   * @brief Start tracking file's usage.
   */
  virtual nsresult BeginUsageTracking(const EntryId& aEntryId) = 0;

  /**
   * @brief Stop tracking file's usage.
   */
  virtual nsresult EndUsageTracking(const EntryId& aEntryId) = 0;

  virtual ~FileSystemDatabaseManager() = default;
};

}  // namespace data
}  // namespace fs
}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATABASEMANAGER_H_
