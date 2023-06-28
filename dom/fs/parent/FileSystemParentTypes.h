/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMPARENTTYPES_H_
#define DOM_FS_PARENT_FILESYSTEMPARENTTYPES_H_

#include "nsStringFwd.h"
#include "nsTString.h"

namespace mozilla::dom::fs {

/**
 * @brief FileId refers to a file on disk while EntryId refers to a path.
 * Same user input path will always generate the same EntryId while the FileId
 * can be different. Move methods can change the FileId which underlies
 * an EntryId and multiple FileIds for temporary files can all map to the same
 * EntryId.
 */
struct FileId {
  explicit FileId(const nsCString& aValue) : mValue(aValue) {}

  explicit FileId(nsCString&& aValue) : mValue(std::move(aValue)) {}

  constexpr bool IsEmpty() const { return mValue.IsEmpty(); }

  constexpr const nsCString& Value() const { return mValue; }

  nsCString mValue;
};

inline bool operator==(const FileId& aLhs, const FileId& aRhs) {
  return aLhs.mValue == aRhs.mValue;
}

inline bool operator!=(const FileId& aLhs, const FileId& aRhs) {
  return aLhs.mValue != aRhs.mValue;
}

enum class FileMode { EXCLUSIVE, SHARED_FROM_EMPTY, SHARED_FROM_COPY };

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_PARENT_FILESYSTEMPARENTTYPES_H_
