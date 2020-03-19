/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_fileinfotimpl_h__
#define mozilla_dom_indexeddb_fileinfotimpl_h__

#include "FileInfoT.h"

#include "mozilla/Mutex.h"
#include "nsIFile.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

template <typename FileManager>
FileInfoT<FileManager>::FileInfoT(
    const typename FileManager::FileManagerGuard& aGuard,
    SafeRefPtr<FileManager> aFileManager, const int64_t aFileId,
    const nsrefcnt aInitialDBRefCnt)
    : mFileId(aFileId),
      mDBRefCnt(aInitialDBRefCnt),
      mFileManager(std::move(aFileManager)) {
  MOZ_ASSERT(mFileId > 0);
  MOZ_ASSERT(mFileManager);
}

template <typename FileManager>
void FileInfoT<FileManager>::AddRef() {
  UpdateReferences(mRefCnt, 1);
}

template <typename FileManager>
void FileInfoT<FileManager>::Release(const bool aSyncDeleteFile) {
  UpdateReferences(mRefCnt, -1, aSyncDeleteFile);
}

template <typename FileManager>
void FileInfoT<FileManager>::UpdateDBRefs(int32_t aDelta) {
  UpdateReferences(mDBRefCnt, aDelta);
}

template <typename FileManager>
void FileInfoT<FileManager>::GetReferences(int32_t* const aRefCnt,
                                           int32_t* const aDBRefCnt) {
  AutoLock lock(FileManager::Mutex());

  if (aRefCnt) {
    *aRefCnt = mRefCnt;
  }

  if (aDBRefCnt) {
    *aDBRefCnt = mDBRefCnt;
  }
}

template <typename FileManager>
FileManager& FileInfoT<FileManager>::Manager() const {
  return *mFileManager;
}

template <typename FileManager>
int64_t FileInfoT<FileManager>::Id() const {
  return mFileId;
}

template <typename FileManager>
void FileInfoT<FileManager>::UpdateReferences(ThreadSafeAutoRefCnt& aRefCount,
                                              const int32_t aDelta,
                                              const bool aSyncDeleteFile) {
  bool needsCleanup;
  {
    AutoLock lock(FileManager::Mutex());

    aRefCount = aRefCount + aDelta;

    if (mRefCnt + mDBRefCnt > 0) {
      return;
    }

    mFileManager->RemoveFileInfo(Id(), lock);

    // If the file manager was already invalidated, we don't need to do any
    // cleanup anymore. In that case, the entire origin directory has already
    // been deleted by the quota manager, and we don't need to delete individual
    // files.
    needsCleanup = !mFileManager->Invalidated();
  }

  if (needsCleanup) {
    if (aSyncDeleteFile) {
      nsresult rv = mFileManager->SyncDeleteFile(Id());
      if (NS_FAILED(rv)) {
        NS_WARNING("FileManager cleanup failed!");
      }
    } else {
      Cleanup();
    }
  }

  delete this;
}

template <typename FileManager>
bool FileInfoT<FileManager>::LockedClearDBRefs(
    const typename FileManager::FileManagerGuard&) {
  FileManager::Mutex().AssertCurrentThreadOwns();

  mDBRefCnt = 0;

  if (mRefCnt) {
    return true;
  }

  // In this case, we are not responsible for removing the file info from the
  // hashtable. It's up to FileManager which is the only caller of this method.

  MOZ_ASSERT(mFileManager->Invalidated());

  delete this;

  return false;
}

template <typename FileManager>
void FileInfoT<FileManager>::Cleanup() {
  int64_t id = Id();

  if (NS_FAILED(mFileManager->AsyncDeleteFile(id))) {
    NS_WARNING("Failed to delete file asynchronously!");
  }
}

template <typename FileManager>
nsCOMPtr<nsIFile> FileInfoT<FileManager>::GetFileForFileInfo() const {
  const nsCOMPtr<nsIFile> directory = Manager().GetDirectory();
  if (NS_WARN_IF(!directory)) {
    return nullptr;
  }

  nsCOMPtr<nsIFile> file = FileManager::GetFileForId(directory, Id());
  if (NS_WARN_IF(!file)) {
    return nullptr;
  }

  return file;
}

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_fileinfotimpl_h__
