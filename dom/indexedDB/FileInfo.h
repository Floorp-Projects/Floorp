/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INDEXEDDB_FILEINFO_H_
#define DOM_INDEXEDDB_FILEINFO_H_

#include "nsISupportsImpl.h"
#include "nsCOMPtr.h"
#include "SafeRefPtr.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

class FileInfoBase {
 public:
  using IdType = int64_t;

  IdType Id() const { return mFileId; }

 protected:
  explicit FileInfoBase(const int64_t aFileId) : mFileId(aFileId) {
    MOZ_ASSERT(mFileId > 0);
  }

 private:
  const IdType mFileId;
};

template <typename FileManager>
class FileInfo final : public FileInfoBase {
 public:
  using AutoLockType = typename FileManager::AutoLockType;

  FileInfo(const typename FileManager::FileInfoManagerGuard& aGuard,
           SafeRefPtr<FileManager> aFileManager, const int64_t aFileId,
           const nsrefcnt aInitialDBRefCnt = 0);

  void AddRef();
  void Release(const bool aSyncDeleteFile = false);

  void UpdateDBRefs(int32_t aDelta);

  void GetReferences(int32_t* aRefCnt, int32_t* aDBRefCnt);

  FileManager& Manager() const;

  nsCOMPtr<nsIFile> GetFileForFileInfo() const;

  void LockedAddRef();
  bool LockedClearDBRefs(
      const typename FileManager::FileInfoManagerGuard& aGuard);

 private:
  void UpdateReferences(ThreadSafeAutoRefCnt& aRefCount, int32_t aDelta,
                        bool aSyncDeleteFile = false);

  void Cleanup();

  ThreadSafeAutoRefCnt mRefCnt;
  ThreadSafeAutoRefCnt mDBRefCnt;

  const SafeRefPtr<FileManager> mFileManager;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // DOM_INDEXEDDB_FILEINFO_H_
