/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileInfo.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/quota/QuotaManager.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class CleanupFileRunnable MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  CleanupFileRunnable(FileManager* aFileManager, int64_t aFileId);

private:
  nsRefPtr<FileManager> mFileManager;
  int64_t mFileId;
};

} // anonymous namespace

// static
FileInfo*
FileInfo::Create(FileManager* aFileManager, int64_t aId)
{
  MOZ_ASSERT(aId > 0, "Wrong id!");

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
  nsRefPtr<CleanupFileRunnable> cleaner =
    new CleanupFileRunnable(mFileManager, Id());

  // IndexedDatabaseManager is main-thread only.
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(cleaner);
    return;
  }

  cleaner->Run();
}

CleanupFileRunnable::CleanupFileRunnable(FileManager* aFileManager,
                                         int64_t aFileId)
: mFileManager(aFileManager), mFileId(aFileId)
{
}

NS_IMPL_ISUPPORTS1(CleanupFileRunnable,
                   nsIRunnable)

NS_IMETHODIMP
CleanupFileRunnable::Run()
{
  if (mozilla::dom::quota::QuotaManager::IsShuttingDown()) {
    return NS_OK;
  }

  nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::Get();
  MOZ_ASSERT(mgr);

  if (NS_FAILED(mgr->AsyncDeleteFile(mFileManager, mFileId))) {
    NS_WARNING("Failed to delete file asynchronously!");
  }

  return NS_OK;
}
