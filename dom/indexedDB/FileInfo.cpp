/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileInfo.h"

#include "ActorsParent.h"
#include "FileManager.h"
#include "IndexedDatabaseManager.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsError.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

void FileInfo::GetReferences(int32_t* aRefCnt, int32_t* aDBRefCnt,
                             int32_t* aSliceRefCnt) {
  MOZ_ASSERT(!IndexedDatabaseManager::IsClosed());

  MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

  if (aRefCnt) {
    *aRefCnt = mRefCnt;
  }

  if (aDBRefCnt) {
    *aDBRefCnt = mDBRefCnt;
  }

  if (aSliceRefCnt) {
    *aSliceRefCnt = mSliceRefCnt;
  }
}

void FileInfo::UpdateReferences(ThreadSafeAutoRefCnt& aRefCount, int32_t aDelta,
                                CustomCleanupCallback* aCustomCleanupCallback) {
  // XXX This can go away once DOM objects no longer hold FileInfo objects...
  //     Looking at you, BlobImplBase...
  //     BlobImplBase is being addressed in bug 1068975.
  if (IndexedDatabaseManager::IsClosed()) {
    MOZ_ASSERT(&aRefCount == &mRefCnt);
    MOZ_ASSERT(aDelta == 1 || aDelta == -1);

    if (aDelta > 0) {
      ++aRefCount;
    } else {
      nsrefcnt count = --aRefCount;
      if (!count) {
        mRefCnt = 1;
        delete this;
      }
    }
    return;
  }

  MOZ_ASSERT(!IndexedDatabaseManager::IsClosed());

  bool needsCleanup;
  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

    aRefCount = aRefCount + aDelta;

    if (mRefCnt + mDBRefCnt + mSliceRefCnt > 0) {
      return;
    }

    mFileManager->RemoveFileInfo(Id(), lock);

    needsCleanup = !mFileManager->Invalidated();
  }

  if (needsCleanup) {
    if (aCustomCleanupCallback) {
      nsresult rv = aCustomCleanupCallback->Cleanup(mFileManager, Id());
      if (NS_FAILED(rv)) {
        NS_WARNING("Custom cleanup failed!");
      }
    } else {
      Cleanup();
    }
  }

  delete this;
}

bool FileInfo::LockedClearDBRefs() {
  MOZ_ASSERT(!IndexedDatabaseManager::IsClosed());

  IndexedDatabaseManager::FileMutex().AssertCurrentThreadOwns();

  mDBRefCnt = 0;

  if (mRefCnt || mSliceRefCnt) {
    return true;
  }

  // In this case, we are not responsible for removing the file info from the
  // hashtable. It's up to FileManager which is the only caller of this method.

  MOZ_ASSERT(mFileManager->Invalidated());

  delete this;

  return false;
}

void FileInfo::Cleanup() {
  AssertIsOnBackgroundThread();

  int64_t id = Id();

  if (NS_FAILED(AsyncDeleteFile(mFileManager, id))) {
    NS_WARNING("Failed to delete file asynchronously!");
  }
}

/* static */
nsCOMPtr<nsIFile> FileInfo::GetFileForFileInfo(const FileInfo& aFileInfo) {
  FileManager* const fileManager = aFileInfo.Manager();
  const nsCOMPtr<nsIFile> directory = fileManager->GetDirectory();
  if (NS_WARN_IF(!directory)) {
    return nullptr;
  }

  nsCOMPtr<nsIFile> file = FileManager::GetFileForId(directory, aFileInfo.Id());
  if (NS_WARN_IF(!file)) {
    return nullptr;
  }

  return file;
}

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla
