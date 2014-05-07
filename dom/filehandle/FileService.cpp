/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileService.h"

#include "FileHandle.h"
#include "LockedFile.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIObserverService.h"
#include "nsIOfflineStorage.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

namespace {

FileService* gInstance = nullptr;
bool gShutdown = false;

} // anonymous namespace

FileService::FileService()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "More than one instance!");
}

FileService::~FileService()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "More than one instance!");
}

nsresult
FileService::Init()
{
  nsresult rv;
  mStreamTransportTarget =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);

  return rv;
}

nsresult
FileService::Cleanup()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsIThread* thread = NS_GetCurrentThread();
  while (mStorageInfos.Count()) {
    if (!NS_ProcessNextEvent(thread)) {
      NS_ERROR("Failed to process next event!");
      break;
    }
  }

  // Make sure the service is still accessible while any generated callbacks
  // are processed.
  nsresult rv = NS_ProcessPendingEvents(thread);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mCompleteCallbacks.IsEmpty()) {
    // Run all callbacks manually now.
    for (uint32_t index = 0; index < mCompleteCallbacks.Length(); index++) {
      mCompleteCallbacks[index].mCallback->Run();
    }
    mCompleteCallbacks.Clear();

    // And make sure they get processed.
    rv = NS_ProcessPendingEvents(thread);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// static
FileService*
FileService::GetOrCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (gShutdown) {
    NS_WARNING("Calling GetOrCreate() after shutdown!");
    return nullptr;
  }

  if (!gInstance) {
    nsRefPtr<FileService> service(new FileService);

    nsresult rv = service->Init();
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsCOMPtr<nsIObserverService> obs =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = obs->AddObserver(service, "profile-before-change", false);
    NS_ENSURE_SUCCESS(rv, nullptr);

    // The observer service now owns us.
    gInstance = service;
  }

  return gInstance;
}

// static
FileService*
FileService::Get()
{
  // Does not return an owning reference.
  return gInstance;
}

// static
void
FileService::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  gShutdown = true;

  if (gInstance) {
    if (NS_FAILED(gInstance->Cleanup())) {
      NS_WARNING("Failed to shutdown file service!");
    }
    gInstance = nullptr;
  }
}

// static
bool
FileService::IsShuttingDown()
{
  return gShutdown;
}

nsresult
FileService::Enqueue(LockedFile* aLockedFile, FileHelper* aFileHelper)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aLockedFile, "Null pointer!");

  FileHandle* fileHandle = aLockedFile->mFileHandle;

  if (fileHandle->IsInvalid()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const nsACString& storageId = fileHandle->mStorageId;
  const nsAString& fileName = fileHandle->mFileName;
  bool modeIsWrite = aLockedFile->mMode == FileMode::Readwrite;

  StorageInfo* storageInfo;
  if (!mStorageInfos.Get(storageId, &storageInfo)) {
    nsAutoPtr<StorageInfo> newStorageInfo(new StorageInfo());

    mStorageInfos.Put(storageId, newStorageInfo);

    storageInfo = newStorageInfo.forget();
  }

  LockedFileQueue* existingLockedFileQueue =
    storageInfo->GetLockedFileQueue(aLockedFile);

  if (existingLockedFileQueue) {
    existingLockedFileQueue->Enqueue(aFileHelper);
    return NS_OK;
  }

  bool lockedForReading = storageInfo->IsFileLockedForReading(fileName);
  bool lockedForWriting = storageInfo->IsFileLockedForWriting(fileName);

  if (modeIsWrite) {
    if (!lockedForWriting) {
      storageInfo->LockFileForWriting(fileName);
    }
  }
  else {
    if (!lockedForReading) {
      storageInfo->LockFileForReading(fileName);
    }
  }

  if (lockedForWriting || (lockedForReading && modeIsWrite)) {
    storageInfo->CreateDelayedEnqueueInfo(aLockedFile, aFileHelper);
  }
  else {
    LockedFileQueue* lockedFileQueue =
      storageInfo->CreateLockedFileQueue(aLockedFile);

    if (aFileHelper) {
      // Enqueue() will queue the file helper if there's already something
      // running. That can't fail, so no need to eventually remove
      // storageInfo from the hash table.
      //
      // If the file helper is free to run then AsyncRun() is called on the
      // file helper. AsyncRun() is responsible for calling all necessary
      // callbacks when something fails. We're propagating the error here,
      // however there's no need to eventually remove storageInfo from
      // the hash table. Code behind AsyncRun() will take care of it. The last
      // item in the code path is NotifyLockedFileCompleted() which removes
      // storageInfo from the hash table if there are no locked files for
      // the file storage.
      nsresult rv = lockedFileQueue->Enqueue(aFileHelper);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

void
FileService::NotifyLockedFileCompleted(LockedFile* aLockedFile)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aLockedFile, "Null pointer!");

  FileHandle* fileHandle = aLockedFile->mFileHandle;
  const nsACString& storageId = fileHandle->mStorageId;

  StorageInfo* storageInfo;
  if (!mStorageInfos.Get(storageId, &storageInfo)) {
    NS_ERROR("We don't know anyting about this locked file?!");
    return;
  }

  storageInfo->RemoveLockedFileQueue(aLockedFile);

  if (!storageInfo->HasRunningLockedFiles()) {
    mStorageInfos.Remove(storageId);

    // See if we need to fire any complete callbacks.
    uint32_t index = 0;
    while (index < mCompleteCallbacks.Length()) {
      if (MaybeFireCallback(mCompleteCallbacks[index])) {
        mCompleteCallbacks.RemoveElementAt(index);
      }
      else {
        index++;
      }
    }
  }
}

void
FileService::WaitForStoragesToComplete(
                                 nsTArray<nsCOMPtr<nsIOfflineStorage> >& aStorages,
                                 nsIRunnable* aCallback)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aStorages.IsEmpty(), "No databases to wait on!");
  NS_ASSERTION(aCallback, "Null pointer!");

  StoragesCompleteCallback* callback = mCompleteCallbacks.AppendElement();
  callback->mCallback = aCallback;
  callback->mStorages.SwapElements(aStorages);

  if (MaybeFireCallback(*callback)) {
    mCompleteCallbacks.RemoveElementAt(mCompleteCallbacks.Length() - 1);
  }
}

void
FileService::AbortLockedFilesForStorage(nsIOfflineStorage* aStorage)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(aStorage, "Null pointer!");

  StorageInfo* storageInfo;
  if (!mStorageInfos.Get(aStorage->Id(), &storageInfo)) {
    return;
  }

  nsAutoTArray<nsRefPtr<LockedFile>, 10> lockedFiles;
  storageInfo->CollectRunningAndDelayedLockedFiles(aStorage, lockedFiles);

  for (uint32_t index = 0; index < lockedFiles.Length(); index++) {
    ErrorResult ignored;
    lockedFiles[index]->Abort(ignored);
  }
}

bool
FileService::HasLockedFilesForStorage(nsIOfflineStorage* aStorage)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(aStorage, "Null pointer!");

  StorageInfo* storageInfo;
  if (!mStorageInfos.Get(aStorage->Id(), &storageInfo)) {
    return false;
  }

  return storageInfo->HasRunningLockedFiles(aStorage);
}

NS_IMPL_ISUPPORTS(FileService, nsIObserver)

NS_IMETHODIMP
FileService::Observe(nsISupports* aSubject, const char*  aTopic,
                     const char16_t* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!strcmp(aTopic, "profile-before-change"), "Wrong topic!");

  Shutdown();

  return NS_OK;
}

bool
FileService::MaybeFireCallback(StoragesCompleteCallback& aCallback)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  for (uint32_t index = 0; index < aCallback.mStorages.Length(); index++) {
    if (mStorageInfos.Get(aCallback.mStorages[index]->Id(), nullptr)) {
      return false;
    }
  }

  aCallback.mCallback->Run();
  return true;
}

FileService::LockedFileQueue::LockedFileQueue(LockedFile* aLockedFile)
: mLockedFile(aLockedFile)
{
  NS_ASSERTION(aLockedFile, "Null pointer!");
}

NS_IMPL_ADDREF(FileService::LockedFileQueue)
NS_IMPL_RELEASE(FileService::LockedFileQueue)

nsresult
FileService::LockedFileQueue::Enqueue(FileHelper* aFileHelper)
{
  mQueue.AppendElement(aFileHelper);

  nsresult rv;
  if (mLockedFile->mRequestMode == LockedFile::PARALLEL) {
    rv = aFileHelper->AsyncRun(this);
  }
  else {
    rv = ProcessQueue();
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
FileService::LockedFileQueue::OnFileHelperComplete(FileHelper* aFileHelper)
{
  if (mLockedFile->mRequestMode == LockedFile::PARALLEL) {
    int32_t index = mQueue.IndexOf(aFileHelper);
    NS_ASSERTION(index != -1, "We don't know anything about this helper!");

    mQueue.RemoveElementAt(index);
  }
  else {
    NS_ASSERTION(mCurrentHelper == aFileHelper, "How can this happen?!");

    mCurrentHelper = nullptr;

    nsresult rv = ProcessQueue();
    if (NS_FAILED(rv)) {
      return;
    }
  }
}

nsresult
FileService::LockedFileQueue::ProcessQueue()
{
  if (mQueue.IsEmpty() || mCurrentHelper) {
    return NS_OK;
  }

  mCurrentHelper = mQueue[0];
  mQueue.RemoveElementAt(0);

  nsresult rv = mCurrentHelper->AsyncRun(this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

FileService::DelayedEnqueueInfo::DelayedEnqueueInfo()
{
}

FileService::DelayedEnqueueInfo::~DelayedEnqueueInfo()
{
}

FileService::LockedFileQueue*
FileService::StorageInfo::CreateLockedFileQueue(LockedFile* aLockedFile)
{
  nsRefPtr<LockedFileQueue>* lockedFileQueue =
    mLockedFileQueues.AppendElement();
  *lockedFileQueue = new LockedFileQueue(aLockedFile);
  return lockedFileQueue->get();
}

FileService::LockedFileQueue*
FileService::StorageInfo::GetLockedFileQueue(LockedFile* aLockedFile)
{
  uint32_t count = mLockedFileQueues.Length();
  for (uint32_t index = 0; index < count; index++) {
    nsRefPtr<LockedFileQueue>& lockedFileQueue = mLockedFileQueues[index];
    if (lockedFileQueue->mLockedFile == aLockedFile) {
      return lockedFileQueue;
    }
  }
  return nullptr;
}

void
FileService::StorageInfo::RemoveLockedFileQueue(LockedFile* aLockedFile)
{
  for (uint32_t index = 0; index < mDelayedEnqueueInfos.Length(); index++) {
    if (mDelayedEnqueueInfos[index].mLockedFile == aLockedFile) {
      NS_ASSERTION(!mDelayedEnqueueInfos[index].mFileHelper, "Should be null!");
      mDelayedEnqueueInfos.RemoveElementAt(index);
      return;
    }
  }

  uint32_t lockedFileCount = mLockedFileQueues.Length();

  // We can't just remove entries from lock hash tables, we have to rebuild
  // them instead. Multiple LockedFile objects may lock the same file
  // (one entry can represent multiple locks).

  mFilesReading.Clear();
  mFilesWriting.Clear();

  for (uint32_t index = 0, count = lockedFileCount; index < count; index++) {
    LockedFile* lockedFile = mLockedFileQueues[index]->mLockedFile;
    if (lockedFile == aLockedFile) {
      NS_ASSERTION(count == lockedFileCount, "More than one match?!");

      mLockedFileQueues.RemoveElementAt(index);
      index--;
      count--;

      continue;
    }

    const nsAString& fileName = lockedFile->mFileHandle->mFileName;

    if (lockedFile->mMode == FileMode::Readwrite) {
      if (!IsFileLockedForWriting(fileName)) {
        LockFileForWriting(fileName);
      }
    }
    else {
      if (!IsFileLockedForReading(fileName)) {
        LockFileForReading(fileName);
      }
    }
  }

  NS_ASSERTION(mLockedFileQueues.Length() == lockedFileCount - 1,
               "Didn't find the locked file we were looking for!");

  nsTArray<DelayedEnqueueInfo> delayedEnqueueInfos;
  delayedEnqueueInfos.SwapElements(mDelayedEnqueueInfos);

  for (uint32_t index = 0; index < delayedEnqueueInfos.Length(); index++) {
    DelayedEnqueueInfo& delayedEnqueueInfo = delayedEnqueueInfos[index];
    if (NS_FAILED(gInstance->Enqueue(delayedEnqueueInfo.mLockedFile,
                                     delayedEnqueueInfo.mFileHelper))) {
      NS_WARNING("Enqueue failed!");
    }
  }
}

bool
FileService::StorageInfo::HasRunningLockedFiles(nsIOfflineStorage* aStorage)
{
  for (uint32_t index = 0; index < mLockedFileQueues.Length(); index++) {
    LockedFile* lockedFile = mLockedFileQueues[index]->mLockedFile;
    if (lockedFile->mFileHandle->Storage() == aStorage) {
      return true;
    }
  }
  return false;
}

FileService::DelayedEnqueueInfo*
FileService::StorageInfo::CreateDelayedEnqueueInfo(LockedFile* aLockedFile,
                                                   FileHelper* aFileHelper)
{
  DelayedEnqueueInfo* info = mDelayedEnqueueInfos.AppendElement();
  info->mLockedFile = aLockedFile;
  info->mFileHelper = aFileHelper;
  return info;
}

void
FileService::StorageInfo::CollectRunningAndDelayedLockedFiles(
                                 nsIOfflineStorage* aStorage,
                                 nsTArray<nsRefPtr<LockedFile> >& aLockedFiles)
{
  for (uint32_t index = 0; index < mLockedFileQueues.Length(); index++) {
    LockedFile* lockedFile = mLockedFileQueues[index]->mLockedFile;
    if (lockedFile->mFileHandle->Storage() == aStorage) {
      aLockedFiles.AppendElement(lockedFile);
    }
  }

  for (uint32_t index = 0; index < mDelayedEnqueueInfos.Length(); index++) {
    LockedFile* lockedFile = mDelayedEnqueueInfos[index].mLockedFile;
    if (lockedFile->mFileHandle->Storage() == aStorage) {
      aLockedFiles.AppendElement(lockedFile);
    }
  }
}

} // namespace dom
} // namespace mozilla
