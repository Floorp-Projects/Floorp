/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMENTRYMETADATAARRAY_H_
#define DOM_FS_CHILD_FILESYSTEMENTRYMETADATAARRAY_H_

#include "nsTArray.h"

namespace mozilla::dom::fs {

class FileSystemEntryMetadata;

class FileSystemEntryMetadataArray : public nsTArray<FileSystemEntryMetadata> {
 public:
  NS_INLINE_DECL_REFCOUNTING(FileSystemEntryMetadataArray);

 private:
  ~FileSystemEntryMetadataArray() = default;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_CHILD_FILESYSTEMENTRYMETADATAARRAY_H_
