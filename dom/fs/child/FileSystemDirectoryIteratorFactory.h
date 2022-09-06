/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMDIRECTORYITERATORFACTORY_H_
#define DOM_FS_CHILD_FILESYSTEMDIRECTORYITERATORFACTORY_H_

#include "mozilla/dom/FileSystemDirectoryIterator.h"

namespace mozilla::dom::fs {

class FileSystemEntryMetadata;

struct FileSystemDirectoryIteratorFactory {
  static UniquePtr<mozilla::dom::FileSystemDirectoryIterator::Impl> Create(
      const FileSystemEntryMetadata& aMetadata,
      IterableIteratorBase::IteratorType aType);
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_CHILD_FILESYSTEMDIRECTORYITERATORFACTORY_H_
