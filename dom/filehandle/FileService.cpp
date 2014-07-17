/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileService.h"

#include "FileHandle.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "MutableFile.h"
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
FileService::Enqueue(FileHandleBase* aFileHandle, FileHelper* aFileHelper)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(aFileHandle, "Null pointer!");

  MutableFileBase* mutableFile = aFileHandle->MutableFile();

  if (mutableFile->IsInvalid()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const nsACString& storageId = mutableFile->mStorageId;
  const nsAString& fileName = mutableFile->mFileName;
  bool modeIsWrite = aFileHandle->mMode == FileMode::Readwrite;

  StorageInfo* storageInfo;
  if (!mStorageInfos.Get(storageId, &storageInfo)) {
    nsAutoPtr<StorageInfo> newStorageInfo(new StorageInfo());

    mStorageInfos.Put(storageId, newStorageInfo);

    storageInfo = newStorageInfo.forget();
  }

  FileHandleQueue* existingFileHandleQueue =
    storageInfo->GetFileHandleQueue(aFileHandle);

  if (existingFileHandleQueue) {
    existingFileHandleQueue->Enqueue(aFileHelper);
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
    storageInfo->CreateDelayedEnqueueInfo(aFileHandle, aFileHelper);
  }
  else {
    FileHandleQueue* fileHandleQueue =
      storageInfo->CreateFileHandleQueue(aFileHandle);

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
      // item in the code path is NotifyFileHandleCompleted() which removes
      // storageInfo from the hash table if there are no file handles for
      // the file storage.
      nsresult rv = fileHandleQueue->Enqueue(aFileHelper);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

void
FileService::NotifyFileHandleCompleted(FileHandleBase* aFileHandle)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(aFileHandle, "Null pointer!");

  MutableFileBase* mutableFile = aFileHandle->MutableFile();
  const nsACString& storageId = mutableFile->mStorageId;

  StorageInfo* storageInfo;
  if (!mStorageInfos.Get(storageId, &storageInfo)) {
    NS_ERROR("We don't know anyting about this file handle?!");
    return;
  }

  storageInfo->RemoveFileHandleQueue(aFileHandle);

  if (!storageInfo->HasRunningFileHandles()) {
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
FileService::AbortFileHandlesForStorage(nsIOfflineStorage* aStorage)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(aStorage, "Null pointer!");

  StorageInfo* storageInfo;
  if (!mStorageInfos.Get(aStorage->Id(), &storageInfo)) {
    return;
  }

  nsAutoTArray<nsRefPtr<FileHandleBase>, 10> fileHandles;
  storageInfo->CollectRunningAndDelayedFileHandles(aStorage, fileHandles);

  for (uint32_t index = 0; index < fileHandles.Length(); index++) {
    ErrorResult ignored;
    fileHandles[index]->Abort(ignored);
  }
}

bool
FileService::HasFileHandlesForStorage(nsIOfflineStorage* aStorage)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(aStorage, "Null pointer!");

  StorageInfo* storageInfo;
  if (!mStorageInfos.Get(aStorage->Id(), &storageInfo)) {
    return false;
  }

  return storageInfo->HasRunningFileHandles(aStorage);
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

FileService::FileHandleQueue::FileHandleQueue(FileHandleBase* aFileHandle)
: mFileHandle(aFileHandle)
{
  MOZ_ASSERT(aFileHandle, "Null pointer!");
}

FileService::FileHandleQueue::~FileHandleQueue()
{
}

NS_IMPL_ADDREF(FileService::FileHandleQueue)
NS_IMPL_RELEASE(FileService::FileHandleQueue)

nsresult
FileService::FileHandleQueue::Enqueue(FileHelper* aFileHelper)
{
  mQueue.AppendElement(aFileHelper);

  nsresult rv;
  if (mFileHandle->mRequestMode == FileHandleBase::PARALLEL) {
    rv = aFileHelper->AsyncRun(this);
  }
  else {
    rv = ProcessQueue();
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
FileService::
FileHandleQueue::OnFileHelperComplete(FileHelper* aFileHelper)
{
  if (mFileHandle->mRequestMode == FileHandleBase::PARALLEL) {
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
FileService::FileHandleQueue::ProcessQueue()
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

FileService::FileHandleQueue*
FileService::StorageInfo::CreateFileHandleQueue(FileHandleBase* aFileHandle)
{
  nsRefPtr<FileHandleQueue>* fileHandleQueue =
    mFileHandleQueues.AppendElement();
  *fileHandleQueue = new FileHandleQueue(aFileHandle);
  return fileHandleQueue->get();
}

FileService::FileHandleQueue*
FileService::StorageInfo::GetFileHandleQueue(FileHandleBase* aFileHandle)
{
  uint32_t count = mFileHandleQueues.Length();
  for (uint32_t index = 0; index < count; index++) {
    nsRefPtr<FileHandleQueue>& fileHandleQueue = mFileHandleQueues[index];
    if (fileHandleQueue->mFileHandle == aFileHandle) {
      return fileHandleQueue;
    }
  }
  return nullptr;
}

void
FileService::StorageInfo::RemoveFileHandleQueue(FileHandleBase* aFileHandle)
{
  for (uint32_t index = 0; index < mDelayedEnqueueInfos.Length(); index++) {
    if (mDelayedEnqueueInfos[index].mFileHandle == aFileHandle) {
      MOZ_ASSERT(!mDelayedEnqueueInfos[index].mFileHelper, "Should be null!");
      mDelayedEnqueueInfos.RemoveElementAt(index);
      return;
    }
  }

  uint32_t fileHandleCount = mFileHandleQueues.Length();

  // We can't just remove entries from lock hash tables, we have to rebuild
  // them instead. Multiple FileHandle objects may lock the same file
  // (one entry can represent multiple locks).

  mFilesReading.Clear();
  mFilesWriting.Clear();

  for (uint32_t index = 0, count = fileHandleCount; index < count; index++) {
    FileHandleBase* fileHandle = mFileHandleQueues[index]->mFileHandle;
    if (fileHandle == aFileHandle) {
      MOZ_ASSERT(count == fileHandleCount, "More than one match?!");

      mFileHandleQueues.RemoveElementAt(index);
      index--;
      count--;

      continue;
    }

    const nsAString& fileName = fileHandle->MutableFile()->mFileName;

    if (fileHandle->mMode == FileMode::Readwrite) {
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

  MOZ_ASSERT(mFileHandleQueues.Length() == fileHandleCount - 1,
             "Didn't find the file handle we were looking for!");

  nsTArray<DelayedEnqueueInfo> delayedEnqueueInfos;
  delayedEnqueueInfos.SwapElements(mDelayedEnqueueInfos);

  for (uint32_t index = 0; index < delayedEnqueueInfos.Length(); index++) {
    DelayedEnqueueInfo& delayedEnqueueInfo = delayedEnqueueInfos[index];
    if (NS_FAILED(gInstance->Enqueue(delayedEnqueueInfo.mFileHandle,
                                     delayedEnqueueInfo.mFileHelper))) {
      NS_WARNING("Enqueue failed!");
    }
  }
}

bool
FileService::StorageInfo::HasRunningFileHandles(nsIOfflineStorage* aStorage)
{
  for (uint32_t index = 0; index < mFileHandleQueues.Length(); index++) {
    FileHandleBase* fileHandle = mFileHandleQueues[index]->mFileHandle;
    if (fileHandle->MutableFile()->Storage() == aStorage) {
      return true;
    }
  }
  return false;
}

FileService::DelayedEnqueueInfo*
FileService::StorageInfo::CreateDelayedEnqueueInfo(FileHandleBase* aFileHandle,
                                                   FileHelper* aFileHelper)
{
  DelayedEnqueueInfo* info = mDelayedEnqueueInfos.AppendElement();
  info->mFileHandle = aFileHandle;
  info->mFileHelper = aFileHelper;
  return info;
}

void
FileService::StorageInfo::CollectRunningAndDelayedFileHandles(
                               nsIOfflineStorage* aStorage,
                               nsTArray<nsRefPtr<FileHandleBase>>& aFileHandles)
{
  for (uint32_t index = 0; index < mFileHandleQueues.Length(); index++) {
    FileHandleBase* fileHandle = mFileHandleQueues[index]->mFileHandle;
    if (fileHandle->MutableFile()->Storage() == aStorage) {
      aFileHandles.AppendElement(fileHandle);
    }
  }

  for (uint32_t index = 0; index < mDelayedEnqueueInfos.Length(); index++) {
    FileHandleBase* fileHandle = mDelayedEnqueueInfos[index].mFileHandle;
    if (fileHandle->MutableFile()->Storage() == aStorage) {
      aFileHandles.AppendElement(fileHandle);
    }
  }
}

} // namespace dom
} // namespace mozilla
