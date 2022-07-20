/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMFILEMANAGER_H_
#define DOM_FS_PARENT_FILESYSTEMFILEMANAGER_H_

#include "mozilla/dom/FileSystemTypes.h"

#include "ErrorList.h"
#include "nsIFile.h"
#include "nsStringFwd.h"
#include "nsString.h"

template <class T>
class nsCOMPtr;

namespace mozilla::dom::fs::data {

Result<nsCOMPtr<nsIFile>, QMResult> GetDatabasePath(const Origin& aOrigin,
                                                    bool& aExists);

class FileSystemFileManager {
 public:
  static Result<FileSystemFileManager, QMResult> CreateFileSystemFileManager(
      const Origin& aOrigin);

  static Result<FileSystemFileManager, QMResult> CreateFileSystemFileManager(
      nsCOMPtr<nsIFile>&& topDirectory);

  Result<nsCOMPtr<nsIFile>, QMResult> GetOrCreateFile(const EntryId& aEntryId);

  Result<bool, QMResult> RemoveFile(const EntryId& aEntryId);

 private:
  explicit FileSystemFileManager(nsCOMPtr<nsIFile>&& aTopDirectory);

  nsCOMPtr<nsIFile> mTopDirectory;
};

}  // namespace mozilla::dom::fs::data

#endif  // DOM_FS_PARENT_FILESYSTEMFILEMANAGER_H_
