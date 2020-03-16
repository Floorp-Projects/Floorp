/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_filemanagerbase_h__
#define mozilla_dom_indexeddb_filemanagerbase_h__

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsISupportsImpl.h"
#include "FlippedOnce.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

class FileInfo;

template <typename FileManager, typename IndexedDatabaseManager>
class FileManagerBase {
 public:
  using FileInfo = indexedDB::FileInfo;
  using MutexType = decltype(IndexedDatabaseManager::FileMutex());
  using AutoLock = mozilla::detail::BaseAutoLock<MutexType>;

  MOZ_MUST_USE RefPtr<FileInfo> GetFileInfo(int64_t aId) const {
    if (IndexedDatabaseManager::IsClosed()) {
      MOZ_ASSERT(false, "Shouldn't be called after shutdown!");
      return nullptr;
    }

    // TODO: We cannot simply change this to RefPtr<FileInfo>, because
    // FileInfo::AddRef also acquires the IndexedDatabaseManager::FileMutex.
    // This looks quirky at least.
    FileInfo* fileInfo;
    {
      AutoLock lock(IndexedDatabaseManager::FileMutex());
      fileInfo = mFileInfos.Get(aId);
    }

    return fileInfo;
  }

  MOZ_MUST_USE RefPtr<FileInfo> CreateFileInfo() {
    MOZ_ASSERT(!IndexedDatabaseManager::IsClosed());

    // TODO: We cannot simply change this to RefPtr<FileInfo>, because
    // FileInfo::AddRef also acquires the IndexedDatabaseManager::FileMutex.
    // This looks quirky at least.
    FileInfo* fileInfo;
    {
      AutoLock lock(IndexedDatabaseManager::FileMutex());

      const int64_t id = ++mLastFileId;

      fileInfo = new FileInfo(static_cast<FileManager*>(this), id);

      mFileInfos.Put(id, fileInfo);
    }

    return fileInfo;
  }

  void RemoveFileInfo(const int64_t aId, const AutoLock& aFilesMutexLock) {
#ifdef DEBUG
    aFilesMutexLock.AssertOwns(IndexedDatabaseManager::FileMutex());
#endif
    mFileInfos.Remove(aId);
  }

  nsresult Invalidate() {
    if (IndexedDatabaseManager::IsClosed()) {
      MOZ_ASSERT(false, "Shouldn't be called after shutdown!");
      return NS_ERROR_UNEXPECTED;
    }

    AutoLock lock(IndexedDatabaseManager::FileMutex());

    mInvalidated.Flip();

    mFileInfos.RemoveIf([](const auto& iter) {
      FileInfo* info = iter.Data();
      MOZ_ASSERT(info);

      return !info->LockedClearDBRefs();
    });

    return NS_OK;
  }

  bool Invalidated() const { return mInvalidated; }

  class FileManagerGuard {
    FileManagerGuard() = default;
  };

 protected:
  ~FileManagerBase() = default;

  // Access to the following fields must be protected by
  // IndexedDatabaseManager::FileMutex()
  int64_t mLastFileId = 0;
  nsDataHashtable<nsUint64HashKey, FileInfo*> mFileInfos;

  FlippedOnce<false> mInvalidated;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_filemanagerbase_h__
