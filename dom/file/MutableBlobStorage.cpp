/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MutableBlobStorage.h"
#include "MemoryBlobImpl.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/ipc/TemporaryIPCBlobChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/TaskQueue.h"
#include "File.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsNetCID.h"
#include "nsProxyRelease.h"
#include "WorkerPrivate.h"

#define BLOB_MEMORY_TEMPORARY_FILE 1048576

namespace mozilla {
namespace dom {

namespace {

// This class uses the callback to inform when the Blob is created or when the
// error must be propagated.
class BlobCreationDoneRunnable final : public Runnable
{
public:
  BlobCreationDoneRunnable(MutableBlobStorage* aBlobStorage,
                           MutableBlobStorageCallback* aCallback,
                           Blob* aBlob,
                           nsresult aRv)
    : Runnable("dom::BlobCreationDoneRunnable")
    , mBlobStorage(aBlobStorage)
    , mCallback(aCallback)
    , mBlob(aBlob)
    , mRv(aRv)
  {
    MOZ_ASSERT(aBlobStorage);
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT((NS_FAILED(aRv) && !aBlob) ||
               (NS_SUCCEEDED(aRv) && aBlob));
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlobStorage);
    mCallback->BlobStoreCompleted(mBlobStorage, mBlob, mRv);
    mCallback = nullptr;
    mBlob = nullptr;
    return NS_OK;
  }

private:
  ~BlobCreationDoneRunnable()
  {
    MOZ_ASSERT(mBlobStorage);
    // If something when wrong, we still have to release these objects in the
    // correct thread.
    NS_ProxyRelease(
      "BlobCreationDoneRunnable::mCallback",
      mBlobStorage->EventTarget(), mCallback.forget());
    NS_ProxyRelease(
      "BlobCreationDoneRunnable::mBlob",
      mBlobStorage->EventTarget(), mBlob.forget());
  }

  RefPtr<MutableBlobStorage> mBlobStorage;
  RefPtr<MutableBlobStorageCallback> mCallback;
  RefPtr<Blob> mBlob;
  nsresult mRv;
};

// Simple runnable to propagate the error to the BlobStorage.
class ErrorPropagationRunnable final : public Runnable
{
public:
  ErrorPropagationRunnable(MutableBlobStorage* aBlobStorage, nsresult aRv)
    : Runnable("dom::ErrorPropagationRunnable")
    , mBlobStorage(aBlobStorage)
    , mRv(aRv)
  {}

  NS_IMETHOD
  Run() override
  {
    mBlobStorage->ErrorPropagated(mRv);
    return NS_OK;
  }

private:
  RefPtr<MutableBlobStorage> mBlobStorage;
  nsresult mRv;
};

// This runnable moves a buffer to the IO thread and there, it writes it into
// the temporary file, if its File Descriptor has not been already closed.
class WriteRunnable final : public Runnable
{
public:
  static WriteRunnable*
  CopyBuffer(MutableBlobStorage* aBlobStorage,
             const void* aData, uint32_t aLength)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aBlobStorage);
    MOZ_ASSERT(aData);

    // We have to take a copy of this buffer.
    void* data = malloc(aLength);
    if (!data) {
      return nullptr;
    }

    memcpy((char*)data, aData, aLength);
    return new WriteRunnable(aBlobStorage, data, aLength);
  }

  static WriteRunnable*
  AdoptBuffer(MutableBlobStorage* aBlobStorage, void* aData, uint32_t aLength)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aBlobStorage);
    MOZ_ASSERT(aData);

    return new WriteRunnable(aBlobStorage, aData, aLength);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mBlobStorage);

    PRFileDesc* fd = mBlobStorage->GetFD();
    if (!fd) {
      // The file descriptor has been closed in the meantime.
      return NS_OK;
    }

    int32_t written = PR_Write(fd, mData, mLength);
    if (NS_WARN_IF(written < 0 || uint32_t(written) != mLength)) {
      mBlobStorage->CloseFD();
      return mBlobStorage->EventTarget()->Dispatch(
        new ErrorPropagationRunnable(mBlobStorage, NS_ERROR_FAILURE),
        NS_DISPATCH_NORMAL);
    }

    return NS_OK;
  }

private:
  WriteRunnable(MutableBlobStorage* aBlobStorage, void* aData, uint32_t aLength)
    : Runnable("dom::WriteRunnable")
    , mBlobStorage(aBlobStorage)
    , mData(aData)
    , mLength(aLength)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlobStorage);
    MOZ_ASSERT(aData);
  }

  ~WriteRunnable()
  {
    free(mData);
  }

  RefPtr<MutableBlobStorage> mBlobStorage;
  void* mData;
  uint32_t mLength;
};

// This runnable closes the FD in case something goes wrong or the temporary
// file is not needed anymore.
class CloseFileRunnable final : public Runnable
{
public:
  explicit CloseFileRunnable(PRFileDesc* aFD)
    : Runnable("dom::CloseFileRunnable")
    , mFD(aFD)
  {}

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    PR_Close(mFD);
    mFD = nullptr;
    return NS_OK;
  }

private:
  ~CloseFileRunnable()
  {
    if (mFD) {
      PR_Close(mFD);
    }
  }

  PRFileDesc* mFD;
};

// This runnable is dispatched to the main-thread from the IO thread and its
// task is to create the blob and inform the callback.
class CreateBlobRunnable final : public Runnable
                               , public TemporaryIPCBlobChildCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  CreateBlobRunnable(MutableBlobStorage* aBlobStorage,
                     already_AddRefed<nsISupports> aParent,
                     const nsACString& aContentType,
                     already_AddRefed<MutableBlobStorageCallback> aCallback)
    : Runnable("dom::CreateBlobRunnable")
    , mBlobStorage(aBlobStorage)
    , mParent(aParent)
    , mContentType(aContentType)
    , mCallback(aCallback)
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aBlobStorage);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlobStorage);
    mBlobStorage->AskForBlob(this, mContentType);
    return NS_OK;
  }

  void
  OperationSucceeded(BlobImpl* aBlobImpl) override
  {
    nsCOMPtr<nsISupports> parent(Move(mParent));
    RefPtr<MutableBlobStorageCallback> callback(Move(mCallback));

    RefPtr<Blob> blob = Blob::Create(parent, aBlobImpl);
    callback->BlobStoreCompleted(mBlobStorage, blob, NS_OK);
  }

  void
  OperationFailed(nsresult aRv) override
  {
    RefPtr<MutableBlobStorageCallback> callback(Move(mCallback));
    callback->BlobStoreCompleted(mBlobStorage, nullptr, aRv);
  }

private:
  ~CreateBlobRunnable()
  {
    MOZ_ASSERT(mBlobStorage);
    // If something when wrong, we still have to release data in the correct
    // thread.
    NS_ProxyRelease(
      "CreateBlobRunnable::mParent",
      mBlobStorage->EventTarget(), mParent.forget());
    NS_ProxyRelease(
      "CreateBlobRunnable::mCallback",
      mBlobStorage->EventTarget(), mCallback.forget());
  }

  RefPtr<MutableBlobStorage> mBlobStorage;
  nsCOMPtr<nsISupports> mParent;
  nsCString mContentType;
  RefPtr<MutableBlobStorageCallback> mCallback;
};

NS_IMPL_ISUPPORTS_INHERITED0(CreateBlobRunnable, Runnable)

// This task is used to know when the writing is completed. From the IO thread
// it dispatches a CreateBlobRunnable to the main-thread.
class LastRunnable final : public Runnable
{
public:
  LastRunnable(MutableBlobStorage* aBlobStorage,
               nsISupports* aParent,
               const nsACString& aContentType,
               MutableBlobStorageCallback* aCallback)
    : Runnable("dom::LastRunnable")
    , mBlobStorage(aBlobStorage)
    , mParent(aParent)
    , mContentType(aContentType)
    , mCallback(aCallback)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlobStorage);
    MOZ_ASSERT(aCallback);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());

    RefPtr<Runnable> runnable =
      new CreateBlobRunnable(mBlobStorage, mParent.forget(),
                             mContentType, mCallback.forget());
    return mBlobStorage->EventTarget()->Dispatch(runnable, NS_DISPATCH_NORMAL);
  }

private:
  ~LastRunnable()
  {
    MOZ_ASSERT(mBlobStorage);
    // If something when wrong, we still have to release data in the correct
    // thread.
    NS_ProxyRelease(
      "LastRunnable::mParent",
      mBlobStorage->EventTarget(), mParent.forget());
    NS_ProxyRelease(
      "LastRunnable::mCallback",
      mBlobStorage->EventTarget(), mCallback.forget());
  }

  RefPtr<MutableBlobStorage> mBlobStorage;
  nsCOMPtr<nsISupports> mParent;
  nsCString mContentType;
  RefPtr<MutableBlobStorageCallback> mCallback;
};

} // anonymous namespace

MutableBlobStorage::MutableBlobStorage(MutableBlobStorageType aType,
                                       nsIEventTarget* aEventTarget,
                                       uint32_t aMaxMemory)
  : mData(nullptr)
  , mDataLen(0)
  , mDataBufferLen(0)
  , mStorageState(aType == eOnlyInMemory ? eKeepInMemory : eInMemory)
  , mFD(nullptr)
  , mErrorResult(NS_OK)
  , mEventTarget(aEventTarget)
  , mMaxMemory(aMaxMemory)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mEventTarget) {
    mEventTarget = GetMainThreadEventTarget();
  }

  if (aMaxMemory == 0 && aType == eCouldBeInTemporaryFile) {
    mMaxMemory = Preferences::GetUint("dom.blob.memoryToTemporaryFile",
                                      BLOB_MEMORY_TEMPORARY_FILE);
  }

  MOZ_ASSERT(mEventTarget);
}

MutableBlobStorage::~MutableBlobStorage()
{
  free(mData);

  if (mFD) {
    RefPtr<Runnable> runnable = new CloseFileRunnable(mFD);
    Unused << DispatchToIOThread(runnable.forget());
  }

  if (mTaskQueue) {
    mTaskQueue->BeginShutdown();
  }

  if (mActor) {
    NS_ProxyRelease("MutableBlobStorage::mActor",
                    EventTarget(), mActor.forget());
  }
}

void
MutableBlobStorage::GetBlobWhenReady(nsISupports* aParent,
                                     const nsACString& aContentType,
                                     MutableBlobStorageCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  // GetBlob can be called just once.
  MOZ_ASSERT(mStorageState != eClosed);
  StorageState previousState = mStorageState;
  mStorageState = eClosed;

  if (previousState == eInTemporaryFile) {
    if (NS_FAILED(mErrorResult)) {
      MOZ_ASSERT(!mActor);

      RefPtr<Runnable> runnable =
        new BlobCreationDoneRunnable(this, aCallback, nullptr, mErrorResult);
      EventTarget()->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
      return;
    }

    MOZ_ASSERT(mActor);

    // We want to wait until all the WriteRunnable are completed. The way we do
    // this is to go to the I/O thread and then we come back: the runnables are
    // executed in order and this LastRunnable will be... the last one.
    // This Runnable will also close the FD on the I/O thread.
    RefPtr<Runnable> runnable =
      new LastRunnable(this, aParent, aContentType, aCallback);

    // If the dispatching fails, we are shutting down and it's fine to do not
    // run the callback.
    Unused << DispatchToIOThread(runnable.forget());
    return;
  }

  // If we are waiting for the temporary file, it's better to wait...
  if (previousState == eWaitingForTemporaryFile) {
    mPendingParent = aParent;
    mPendingContentType = aContentType;
    mPendingCallback = aCallback;
    return;
  }

  RefPtr<BlobImpl> blobImpl;

  if (mData) {
    blobImpl = new MemoryBlobImpl(mData, mDataLen,
                                  NS_ConvertUTF8toUTF16(aContentType));

    mData = nullptr; // The MemoryBlobImpl takes ownership of the buffer
    mDataLen = 0;
    mDataBufferLen = 0;
  } else {
    blobImpl = new EmptyBlobImpl(NS_ConvertUTF8toUTF16(aContentType));
  }

  RefPtr<Blob> blob = Blob::Create(aParent, blobImpl);
  RefPtr<BlobCreationDoneRunnable> runnable =
    new BlobCreationDoneRunnable(this, aCallback, blob, NS_OK);

  nsresult error = EventTarget()->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(error))) {
    return;
  }
}

nsresult
MutableBlobStorage::Append(const void* aData, uint32_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStorageState != eClosed);
  NS_ENSURE_ARG_POINTER(aData);

  if (!aLength) {
    return NS_OK;
  }

  // If eInMemory is the current Storage state, we could maybe migrate to
  // a temporary file.
  if (mStorageState == eInMemory && ShouldBeTemporaryStorage(aLength) &&
      !MaybeCreateTemporaryFile()) {
    return NS_ERROR_FAILURE;
  }

  // If we are already in the temporaryFile mode, we have to dispatch a
  // runnable.
  if (mStorageState == eInTemporaryFile) {
    // If a previous operation failed, let's return that error now.
    if (NS_FAILED(mErrorResult)) {
      return mErrorResult;
    }

    RefPtr<WriteRunnable> runnable =
      WriteRunnable::CopyBuffer(this, aData, aLength);
    if (NS_WARN_IF(!runnable)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = DispatchToIOThread(runnable.forget());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mDataLen += aLength;
    return NS_OK;
  }

  // By default, we store in memory.

  uint64_t offset = mDataLen;

  if (!ExpandBufferSize(aLength)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy((char*)mData + offset, aData, aLength);
  return NS_OK;
}

bool
MutableBlobStorage::ExpandBufferSize(uint64_t aSize)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStorageState < eInTemporaryFile);

  if (mDataBufferLen >= mDataLen + aSize) {
    mDataLen += aSize;
    return true;
  }

  // Start at 1 or we'll loop forever.
  CheckedUint32 bufferLen =
    std::max<uint32_t>(static_cast<uint32_t>(mDataBufferLen), 1);
  while (bufferLen.isValid() && bufferLen.value() < mDataLen + aSize) {
    bufferLen *= 2;
  }

  if (!bufferLen.isValid()) {
    return false;
  }

  void* data = realloc(mData, bufferLen.value());
  if (!data) {
    return false;
  }

  mData = data;
  mDataBufferLen = bufferLen.value();
  mDataLen += aSize;
  return true;
}

bool
MutableBlobStorage::ShouldBeTemporaryStorage(uint64_t aSize) const
{
  MOZ_ASSERT(mStorageState == eInMemory);

  CheckedUint32 bufferSize = mDataLen;
  bufferSize += aSize;

  if (!bufferSize.isValid()) {
    return false;
  }

  return bufferSize.value() >= mMaxMemory;
}

bool
MutableBlobStorage::MaybeCreateTemporaryFile()
{
  MOZ_ASSERT(NS_IsMainThread());

  mStorageState = eWaitingForTemporaryFile;

  mozilla::ipc::PBackgroundChild* actorChild =
    mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    return false;
  }

  mActor = new TemporaryIPCBlobChild(this);
  actorChild->SendPTemporaryIPCBlobConstructor(mActor);

  // We need manually to increase the reference for this actor because the
  // IPC allocator method is not triggered. The Release() is called by IPDL
  // when the actor is deleted.
  mActor.get()->AddRef();

  // The actor will call us when the FileDescriptor is received.

  return true;
}

void
MutableBlobStorage::TemporaryFileCreated(PRFileDesc* aFD)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStorageState == eWaitingForTemporaryFile ||
             mStorageState == eClosed);
  MOZ_ASSERT_IF(mPendingCallback, mStorageState == eClosed);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(aFD);

  // If the object has been already closed and we don't need to execute a
  // callback, we need just to close the file descriptor in the correct thread.
  if (mStorageState == eClosed && !mPendingCallback) {
    RefPtr<Runnable> runnable = new CloseFileRunnable(aFD);

    // If this dispatching fails, CloseFileRunnable will close the FD in the
    // DTOR on the current thread.
    Unused << DispatchToIOThread(runnable.forget());

    // Let's inform the parent that we have nothing else to do.
    mActor->SendOperationFailed();
    mActor = nullptr;
    return;
  }

  // If we still receiving data, we can proceed in temporary-file mode.
  if (mStorageState == eWaitingForTemporaryFile) {
    mStorageState = eInTemporaryFile;
  }

  mFD = aFD;
  MOZ_ASSERT(NS_SUCCEEDED(mErrorResult));

  // This runnable takes the ownership of mData and it will write this buffer
  // into the temporary file.
  RefPtr<WriteRunnable> runnable =
    WriteRunnable::AdoptBuffer(this, mData, mDataLen);
  MOZ_ASSERT(runnable);

  mData = nullptr;

  nsresult rv = DispatchToIOThread(runnable.forget());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Shutting down, we cannot continue.
    return;
  }

  // If we are closed, it means that GetBlobWhenReady() has been called when we
  // were already waiting for a temporary file-descriptor. Finally we are here,
  // AdoptBuffer runnable is going to write the current buffer into this file.
  // After that, there is nothing else to write, and we dispatch LastRunnable
  // which ends up calling mPendingCallback via CreateBlobRunnable.
  if (mStorageState == eClosed) {
    MOZ_ASSERT(mPendingCallback);

    RefPtr<Runnable> runnable =
      new LastRunnable(this, mPendingParent, mPendingContentType,
                       mPendingCallback);
    Unused << DispatchToIOThread(runnable.forget());

    mPendingParent = nullptr;
    mPendingCallback = nullptr;
  }
}

void
MutableBlobStorage::AskForBlob(TemporaryIPCBlobChildCallback* aCallback,
                               const nsACString& aContentType)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStorageState == eClosed);
  MOZ_ASSERT(mFD);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(aCallback);

  // Let's pass the FileDescriptor to the parent actor in order to keep the file
  // locked on windows.
  mActor->AskForBlob(aCallback, aContentType, mFD);

  // The previous operation has duplicated the file descriptor. Now we can close
  // mFD. The parent will take care of closing the duplicated file descriptor on
  // its side.
  RefPtr<Runnable> runnable = new CloseFileRunnable(mFD);
  Unused << DispatchToIOThread(runnable.forget());

  mFD = nullptr;
  mActor = nullptr;
}

void
MutableBlobStorage::ErrorPropagated(nsresult aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  mErrorResult = aRv;

  if (mActor) {
    mActor->SendOperationFailed();
    mActor = nullptr;
  }
}

nsresult
MutableBlobStorage::DispatchToIOThread(already_AddRefed<nsIRunnable> aRunnable)
{
  if (!mTaskQueue) {
    nsCOMPtr<nsIEventTarget> target
      = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT(target);

    mTaskQueue = new TaskQueue(target.forget());
  }

  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  nsresult rv = mTaskQueue->Dispatch(runnable.forget());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

size_t
MutableBlobStorage::SizeOfCurrentMemoryBuffer() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mStorageState < eInTemporaryFile ? mDataLen : 0;
}

PRFileDesc*
MutableBlobStorage::GetFD() const
{
  MOZ_ASSERT(!NS_IsMainThread());
  return mFD;
}

void
MutableBlobStorage::CloseFD()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mFD);

  PR_Close(mFD);
  mFD = nullptr;
}

} // dom namespace
} // mozilla namespace
