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
#include "nsTHashMap.h"
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

  [[nodiscard]] SafeRefPtr<FileInfo> GetFileInfo(int64_t aId) const {
    return AcquireFileInfo([this, aId] { return mFileInfos.MaybeGet(aId); });
  }

  [[nodiscard]] SafeRefPtr<FileInfo> CreateFileInfo() {
    return AcquireFileInfo([this] {
      const int64_t id = ++mLastFileId;

      auto fileInfo =
          MakeNotNull<FileInfo*>(FileManagerGuard{},
                                 SafeRefPtr{static_cast<FileManager*>(this),
                                            AcquireStrongRefFromRawPtr{}},
                                 id);

      mFileInfos.InsertOrUpdate(id, fileInfo);
      return Some(fileInfo);
    });
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

 private:
  // Runs the given aFileInfoTableOp operation, which must return a FileInfo*,
  // under the file manager lock, acquires a strong reference to the returned
  // object under the lock, and returns the strong reference.
  template <typename FileInfoTableOp>
  [[nodiscard]] SafeRefPtr<FileInfo> AcquireFileInfo(
      const FileInfoTableOp& aFileInfoTableOp) const {
    if (!AssertValid()) {
      // In release, the assertions are disabled.
      return nullptr;
    }

    // We cannot simply change this to SafeRefPtr<FileInfo>, because
    // FileInfo::AddRef also acquires the FileManager::Mutex.
    auto fileInfo = [&aFileInfoTableOp]() -> RefPtr<FileInfo> {
      AutoLock lock(FileManager::Mutex());

      const auto maybeFileInfo = aFileInfoTableOp();
      if (maybeFileInfo) {
        const auto& fileInfo = maybeFileInfo.ref();
        fileInfo->LockedAddRef();
        return dont_AddRef(fileInfo.get());
      }

      return {};
    }();

    return SafeRefPtr{std::move(fileInfo)};
  }

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
  nsTHashMap<nsUint64HashKey, NotNull<FileInfo*>> mFileInfos;

  FlippedOnce<false> mInvalidated;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_filemanagerbase_h__
