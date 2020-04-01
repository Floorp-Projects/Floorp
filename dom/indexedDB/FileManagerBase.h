/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_filemanagerbase_h__
#define mozilla_dom_indexeddb_filemanagerbase_h__

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsISupportsImpl.h"
#include "FileInfoT.h"
#include "FlippedOnce.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

template <typename FileManager>
class FileManagerBase {
 public:
  using FileInfo = FileInfoT<FileManager>;
  using MutexType = StaticMutex;
  using AutoLock = mozilla::detail::BaseAutoLock<MutexType&>;

  MOZ_MUST_USE SafeRefPtr<FileInfo> GetFileInfo(int64_t aId) const {
    if (!AssertValid()) {
      // In release, the assertions are disabled.
      return nullptr;
    }

    // TODO: We cannot simply change this to SafeRefPtr<FileInfo>, because
    // FileInfo::AddRef also acquires the FileManager::Mutex.
    // This looks quirky at least.
    FileInfo* fileInfo;
    {
      AutoLock lock(FileManager::Mutex());
      fileInfo = mFileInfos.Get(aId);
    }

    return {fileInfo, AcquireStrongRefFromRawPtr{}};
  }

  MOZ_MUST_USE SafeRefPtr<FileInfo> CreateFileInfo() {
    if (!AssertValid()) {
      // In release, the assertions are disabled.
      return nullptr;
    }

    // TODO: We cannot simply change this to SafeRefPtr<FileInfo>, because
    // FileInfo::AddRef also acquires the FileManager::Mutex.
    // This looks quirky at least.
    FileInfo* fileInfo;
    {
      AutoLock lock(FileManager::Mutex());

      const int64_t id = ++mLastFileId;

      fileInfo = new FileInfo(FileManagerGuard{},
                              SafeRefPtr{static_cast<FileManager*>(this),
                                         AcquireStrongRefFromRawPtr{}},
                              id);

      mFileInfos.Put(id, fileInfo);
    }

    return {fileInfo, AcquireStrongRefFromRawPtr{}};
  }

  void RemoveFileInfo(const int64_t aId, const AutoLock& aFileMutexLock) {
#ifdef DEBUG
    aFileMutexLock.AssertOwns(FileManager::Mutex());
#endif
    mFileInfos.Remove(aId);
  }

  nsresult Invalidate() {
    AutoLock lock(FileManager::Mutex());

    mInvalidated.Flip();

    mFileInfos.RemoveIf([](const auto& iter) {
      FileInfo* info = iter.Data();
      MOZ_ASSERT(info);

      return !info->LockedClearDBRefs(FileManagerGuard{});
    });

    return NS_OK;
  }

  bool Invalidated() const { return mInvalidated; }

  class FileManagerGuard {
    FileManagerGuard() = default;
  };

 protected:
  bool AssertValid() const {
    if (NS_WARN_IF(static_cast<const FileManager*>(this)->Invalidated())) {
      MOZ_ASSERT(false);
      return false;
    }

    return true;
  }

#ifdef DEBUG
  ~FileManagerBase() { MOZ_ASSERT(mFileInfos.IsEmpty()); }
#else
  ~FileManagerBase() = default;
#endif

  // Access to the following fields must be protected by
  // FileManager::Mutex()
  int64_t mLastFileId = 0;
  nsDataHashtable<nsUint64HashKey, FileInfo*> mFileInfos;

  FlippedOnce<false> mInvalidated;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_filemanagerbase_h__
