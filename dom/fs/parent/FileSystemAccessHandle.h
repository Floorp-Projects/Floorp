/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMACCESSHANDLE_H_
#define DOM_FS_PARENT_FILESYSTEMACCESSHANDLE_H_

#include "mozilla/RefPtr.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "nsISupportsUtils.h"
#include "nsString.h"

enum class nsresult : uint32_t;

namespace mozilla {

template <typename V, typename E>
class Result;

namespace dom {

namespace fs::data {

class FileSystemDataManager;

}  // namespace fs::data

class FileSystemAccessHandle {
 public:
  static Result<RefPtr<FileSystemAccessHandle>, nsresult> Create(
      RefPtr<fs::data::FileSystemDataManager> aDataManager,
      const fs::EntryId& aEntryId);

  NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET(FileSystemAccessHandle)

  bool IsOpen() const;

  void Close();

 private:
  FileSystemAccessHandle(RefPtr<fs::data::FileSystemDataManager> aDataManager,
                         const fs::EntryId& aEntryId);

  ~FileSystemAccessHandle();

  const fs::EntryId mEntryId;
  RefPtr<fs::data::FileSystemDataManager> mDataManager;
  bool mClosed;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_PARENT_FILESYSTEMACCESSHANDLE_H_
