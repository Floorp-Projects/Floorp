/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileInfo.h"

#include "mozilla/dom/quota/QuotaManager.h"

USING_INDEXEDDB_NAMESPACE

// static
FileInfo*
FileInfo::Create(FileManager* aFileManager, int64_t aId)
{
  NS_ASSERTION(aId > 0, "Wrong id!");

  if (aId <= INT16_MAX) {
    return new FileInfo16(aFileManager, aId);
  }

  if (aId <= INT32_MAX) {
    return new FileInfo32(aFileManager, aId);
  }

  return new FileInfo64(aFileManager, aId);
}

void
FileInfo::GetReferences(int32_t* aRefCnt, int32_t* aDBRefCnt,
                        int32_t* aSliceRefCnt)
{
  if (IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");

    if (aRefCnt) {
      *aRefCnt = -1;
    }

    if (aDBRefCnt) {
      *aDBRefCnt = -1;
    }

    if (aSliceRefCnt) {
      *aSliceRefCnt = -1;
    }

    return;
  }

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

void
FileInfo::UpdateReferences(mozilla::ThreadSafeAutoRefCnt& aRefCount,
                           int32_t aDelta, bool aClear)
{
  if (IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return;
  }

  bool needsCleanup;
  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

    aRefCount = aClear ? 0 : aRefCount + aDelta;

    if (mRefCnt + mDBRefCnt + mSliceRefCnt > 0) {
      return;
    }

    mFileManager->mFileInfos.Remove(Id());

    needsCleanup = !mFileManager->Invalidated();
  }

  if (needsCleanup) {
    Cleanup();
  }

  delete this;
}

void
FileInfo::Cleanup()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (quota::QuotaManager::IsShuttingDown()) {
    return;
  }

  nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "Shouldn't be null!");

  if (NS_FAILED(mgr->AsyncDeleteFile(mFileManager, Id()))) {
    NS_WARNING("Failed to delete file asynchronously!");
  }
}
