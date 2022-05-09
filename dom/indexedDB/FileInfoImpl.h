/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INDEXEDDB_FILEINFOIMPL_H_
#define DOM_INDEXEDDB_FILEINFOIMPL_H_

#include "FileInfo.h"

#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/Mutex.h"
#include "nsIFile.h"

namespace mozilla::dom::indexedDB {

template <typename FileManager>
FileInfo<FileManager>::FileInfo(
    const typename FileManager::FileInfoManagerGuard& aGuard,
    SafeRefPtr<FileManager> aFileManager, const int64_t aFileId,
    const nsrefcnt aInitialDBRefCnt)
    : FileInfoBase{aFileId},
      mDBRefCnt(aInitialDBRefCnt),
      mFileManager(std::move(aFileManager)) {
  MOZ_ASSERT(mFileManager);
}

template <typename FileManager>
void FileInfo<FileManager>::AddRef() {
  AutoLockType lock(FileManager::Mutex());

  LockedAddRef();
}

template <typename FileManager>
void FileInfo<FileManager>::Release(const bool aSyncDeleteFile) {
  UpdateReferences(mRefCnt, -1, aSyncDeleteFile);
}

template <typename FileManager>
void FileInfo<FileManager>::UpdateDBRefs(int32_t aDelta) {
  UpdateReferences(mDBRefCnt, aDelta);
}

template <typename FileManager>
void FileInfo<FileManager>::GetReferences(int32_t* const aRefCnt,
                                          int32_t* const aDBRefCnt) {
  AutoLockType lock(FileManager::Mutex());

  if (aRefCnt) {
    *aRefCnt = mRefCnt;
  }

  if (aDBRefCnt) {
    *aDBRefCnt = mDBRefCnt;
  }
}

template <typename FileManager>
FileManager& FileInfo<FileManager>::Manager() const {
  return *mFileManager;
}

template <typename FileManager>
void FileInfo<FileManager>::UpdateReferences(ThreadSafeAutoRefCnt& aRefCount,
                                             const int32_t aDelta,
                                             const bool aSyncDeleteFile) {
  bool needsCleanup;
  {
    AutoLockType lock(FileManager::Mutex());

    aRefCount = aRefCount + aDelta;

    if (mRefCnt + mDBRefCnt > 0) {
      return;
    }

    mFileManager->RemoveFileInfo(Id(), lock);

    // If the FileManager was already invalidated, we don't need to do any
    // cleanup anymore. In that case, the entire origin directory has already
    // been deleted by the quota manager, and we don't need to delete individual
    // files.
    needsCleanup = !mFileManager->Invalidated();
  }

  if (needsCleanup) {
    if (aSyncDeleteFile) {
      QM_WARNONLY_TRY(QM_TO_RESULT(mFileManager->SyncDeleteFile(Id())));
    } else {
      Cleanup();
    }
  }

  delete this;
}

template <typename FileManager>
void FileInfo<FileManager>::LockedAddRef() {
  FileManager::Mutex().AssertCurrentThreadOwns();

  ++mRefCnt;
}

template <typename FileManager>
bool FileInfo<FileManager>::LockedClearDBRefs(
    const typename FileManager::FileInfoManagerGuard&) {
  FileManager::Mutex().AssertCurrentThreadOwns();

  mDBRefCnt = 0;

  if (mRefCnt) {
    return true;
  }

  // In this case, we are not responsible for removing the FileInfo from the
  // hashtable. It's up to FileManager which is the only caller of this method.

  MOZ_ASSERT(mFileManager->Invalidated());

  delete this;

  return false;
}

template <typename FileManager>
void FileInfo<FileManager>::Cleanup() {
  QM_WARNONLY_TRY(QM_TO_RESULT(mFileManager->AsyncDeleteFile(Id())));
}

template <typename FileManager>
nsCOMPtr<nsIFile> FileInfo<FileManager>::GetFileForFileInfo() const {
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

}  // namespace mozilla::dom::indexedDB

#endif  // DOM_INDEXEDDB_FILEINFOIMPL_H_
