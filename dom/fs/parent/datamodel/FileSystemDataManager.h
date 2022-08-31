/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_
#define DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_

#include "mozilla/dom/FileSystemTypes.h"

namespace mozilla {

template <typename V, typename E>
class Result;

namespace dom::fs::data {

class FileSystemDataManager {
 public:
  using result_t = Result<FileSystemDataManager*, nsresult>;
  static FileSystemDataManager::result_t CreateFileSystemDataManager(
      const fs::Origin& aOrigin);
};

}  // namespace dom::fs::data
}  // namespace mozilla

#endif  // DOM_FS_PARENT_DATAMODEL_FILESYSTEMDATAMANAGER_H_
