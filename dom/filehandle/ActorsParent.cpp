/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PBackgroundFileHandleParent.h"
#include "mozilla/dom/PBackgroundFileRequestParent.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseParent.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/quota/MemoryOutputStream.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRunnable.h"
#include "nsISeekableStream.h"
#include "nsIThread.h"
#include "nsIThreadPool.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsThreadPool.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

namespace mozilla::dom {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

namespace {

/******************************************************************************
 * Constants
 ******************************************************************************/

const uint32_t kThreadLimit = 5;
const uint32_t kIdleThreadLimit = 1;
const uint32_t kIdleThreadTimeoutMs = 30000;

const uint32_t kStreamCopyBlockSize = 32768;

}  // namespace

class FileHandleThreadPool::FileHandleQueue final : public Runnable {
  friend class FileHandleThreadPool;

  RefPtr<FileHandleThreadPool> mOwningFileHandleThreadPool;
  RefPtr<FileHandle> mFileHandle;
  nsTArray<RefPtr<FileHandleOp>> mQueue;
  RefPtr<FileHandleOp> mCurrentOp;
  bool mShouldFinish;

 public:
  explicit FileHandleQueue(FileHandleThreadPool* aFileHandleThreadPool,
                           FileHandle* aFileHandle);

  void Enqueue(FileHandleOp* aFileHandleOp);

  void Finish();

  void ProcessQueue();

 private:
  ~FileHandleQueue() = default;

  NS_DECL_NSIRUNNABLE
};

struct FileHandleThreadPool::DelayedEnqueueInfo {
  RefPtr<FileHandle> mFileHandle;
  RefPtr<FileHandleOp> mFileHandleOp;
  bool mFinish;
};

class FileHandleThreadPool::DirectoryInfo {
  friend class FileHandleThreadPool;

  RefPtr<FileHandleThreadPool> mOwningFileHandleThreadPool;
  nsTArray<RefPtr<FileHandleQueue>> mFileHandleQueues;
  nsTArray<DelayedEnqueueInfo> mDelayedEnqueueInfos;
  nsTHashSet<nsString> mFilesReading;
  nsTHashSet<nsString> mFilesWriting;

 public:
  FileHandleQueue* CreateFileHandleQueue(FileHandle* aFileHandle);

  FileHandleQueue* GetFileHandleQueue(FileHandle* aFileHandle);

  void RemoveFileHandleQueue(FileHandle* aFileHandle);

  bool HasRunningFileHandles() { return !mFileHandleQueues.IsEmpty(); }

  DelayedEnqueueInfo* CreateDelayedEnqueueInfo(FileHandle* aFileHandle,
                                               FileHandleOp* aFileHandleOp,
                                               bool aFinish);

  void LockFileForReading(const nsAString& aFileName) {
    mFilesReading.Insert(aFileName);
  }

  void LockFileForWriting(const nsAString& aFileName) {
    mFilesWriting.Insert(aFileName);
  }

  bool IsFileLockedForReading(const nsAString& aFileName) {
    return mFilesReading.Contains(aFileName);
  }

  bool IsFileLockedForWriting(const nsAString& aFileName) {
    return mFilesWriting.Contains(aFileName);
  }

 private:
  explicit DirectoryInfo(FileHandleThreadPool* aFileHandleThreadPool)
      : mOwningFileHandleThreadPool(aFileHandleThreadPool) {}
};

struct FileHandleThreadPool::StoragesCompleteCallback final {
  friend class DefaultDelete<StoragesCompleteCallback>;

  nsTArray<nsCString> mDirectoryIds;
  nsCOMPtr<nsIRunnable> mCallback;

  StoragesCompleteCallback(nsTArray<nsCString>&& aDatabaseIds,
                           nsIRunnable* aCallback);

 private:
  ~StoragesCompleteCallback();
};

/******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class FileHandle : public PBackgroundFileHandleParent {
  friend class BackgroundMutableFileParentBase;

  class FinishOp;

  RefPtr<BackgroundMutableFileParentBase> mMutableFile;
  nsCOMPtr<nsISupports> mStream;
  uint64_t mActiveRequestCount;
  FileHandleStorage mStorage;
  Atomic<bool> mInvalidatedOnAnyThread;
  FileMode mMode;
  bool mHasBeenActive;
  bool mActorDestroyed;
  bool mInvalidated;
  bool mAborted;
  bool mFinishOrAbortReceived;
  bool mFinishedOrAborted;
  bool mForceAborted;

#ifdef DEBUG
  nsCOMPtr<nsIEventTarget> mThreadPoolEventTarget;
#endif

 public:
  void AssertIsOnThreadPool() const;

  bool IsActorDestroyed() const {
    AssertIsOnBackgroundThread();

    return mActorDestroyed;
  }

  // Must be called on the background thread.
  bool IsInvalidated() const {
    MOZ_ASSERT(IsOnBackgroundThread(), "Use IsInvalidatedOnAnyThread()");
    MOZ_ASSERT_IF(mInvalidated, mAborted);

    return mInvalidated;
  }

  // May be called on any thread, but is more expensive than IsInvalidated().
  bool IsInvalidatedOnAnyThread() const { return mInvalidatedOnAnyThread; }

  void SetActive() {
    AssertIsOnBackgroundThread();

    mHasBeenActive = true;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::FileHandle)

  nsresult GetOrCreateStream(nsISupports** aStream);

  void Abort(bool aForce);

  FileHandleStorage Storage() const { return mStorage; }

  FileMode Mode() const { return mMode; }

  BackgroundMutableFileParentBase* GetMutableFile() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mMutableFile);

    return mMutableFile;
  }

  bool IsAborted() const {
    AssertIsOnBackgroundThread();

    return mAborted;
  }

  PBackgroundParent* GetBackgroundParent() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(!IsActorDestroyed());

    return GetMutableFile()->GetBackgroundParent();
  }

  void NoteActiveRequest();

  void NoteFinishedRequest();

  void Invalidate();

 private:
  // This constructor is only called by BackgroundMutableFileParentBase.
  FileHandle(BackgroundMutableFileParentBase* aMutableFile, FileMode aMode);

  // Reference counted.
  ~FileHandle();

  void MaybeFinishOrAbort() {
    AssertIsOnBackgroundThread();

    // If we've already finished or aborted then there's nothing else to do.
    if (mFinishedOrAborted) {
      return;
    }

    // If there are active requests then we have to wait for those requests to
    // complete (see NoteFinishedRequest).
    if (mActiveRequestCount) {
      return;
    }

    // If we haven't yet received a finish or abort message then there could be
    // additional requests coming so we should wait unless we're being forced to
    // abort.
    if (!mFinishOrAbortReceived && !mForceAborted) {
      return;
    }

    FinishOrAbort();
  }

  void SendCompleteNotification(bool aAborted);

  bool VerifyRequestParams(const FileRequestParams& aParams) const;

  bool VerifyRequestData(const FileRequestData& aData) const;

  void FinishOrAbort();

  // IPDL methods are only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvDeleteMe() override;

  virtual mozilla::ipc::IPCResult RecvFinish() override;

  virtual mozilla::ipc::IPCResult RecvAbort() override;

  virtual PBackgroundFileRequestParent* AllocPBackgroundFileRequestParent(
      const FileRequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPBackgroundFileRequestConstructor(
      PBackgroundFileRequestParent* aActor,
      const FileRequestParams& aParams) override;

  virtual bool DeallocPBackgroundFileRequestParent(
      PBackgroundFileRequestParent* aActor) override;
};

class FileHandleOp {
 protected:
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  RefPtr<FileHandle> mFileHandle;
#ifdef DEBUG
  bool mEnqueued;
#endif

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileHandleOp)

  void AssertIsOnOwningThread() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mOwningEventTarget);
    DebugOnly<bool> current;
    MOZ_ASSERT(NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)));
    MOZ_ASSERT(current);
  }

  nsIEventTarget* OwningThread() const { return mOwningEventTarget; }

  void AssertIsOnThreadPool() const {
    MOZ_ASSERT(mFileHandle);
    mFileHandle->AssertIsOnThreadPool();
  }

  void Enqueue();

  virtual void RunOnThreadPool() = 0;

  virtual void RunOnOwningThread() = 0;

 protected:
  FileHandleOp(FileHandle* aFileHandle)
      : mOwningEventTarget(GetCurrentSerialEventTarget()),
        mFileHandle(aFileHandle)
#ifdef DEBUG
        ,
        mEnqueued(false)
#endif
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(aFileHandle);
  }

  virtual ~FileHandleOp() = default;
};

class FileHandle::FinishOp : public FileHandleOp {
  friend class FileHandle;

  bool mAborted;

 private:
  FinishOp(FileHandle* aFileHandle, bool aAborted)
      : FileHandleOp(aFileHandle), mAborted(aAborted) {
    MOZ_ASSERT(aFileHandle);
  }

  ~FinishOp() = default;

  virtual void RunOnThreadPool() override;

  virtual void RunOnOwningThread() override;
};

class NormalFileHandleOp : public FileHandleOp,
                           public PBackgroundFileRequestParent {
  nsresult mResultCode;
  Atomic<bool> mOperationMayProceed;
  bool mActorDestroyed;
  const bool mFileHandleIsAborted;

#ifdef DEBUG
  bool mResponseSent;
#endif

 protected:
  nsCOMPtr<nsISupports> mFileStream;

 public:
  void NoteActorDestroyed() {
    AssertIsOnOwningThread();

    mActorDestroyed = true;
    mOperationMayProceed = false;
  }

  bool IsActorDestroyed() const {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

  // May be called on any thread, but you should call IsActorDestroyed() if
  // you know you're on the background thread because it is slightly faster.
  bool OperationMayProceed() const { return mOperationMayProceed; }

  // May be overridden by subclasses if they need to perform work on the
  // background thread before being enqueued. Returning false will kill the
  // child actors and prevent enqueue.
  virtual bool Init(FileHandle* aFileHandle);

  // This callback will be called on the background thread before releasing the
  // final reference to this request object. Subclasses may perform any
  // additional cleanup here but must always call the base class implementation.
  virtual void Cleanup();

 protected:
  NormalFileHandleOp(FileHandle* aFileHandle)
      : FileHandleOp(aFileHandle),
        mResultCode(NS_OK),
        mOperationMayProceed(true),
        mActorDestroyed(false),
        mFileHandleIsAborted(aFileHandle->IsAborted())
#ifdef DEBUG
        ,
        mResponseSent(false)
#endif
  {
    MOZ_ASSERT(aFileHandle);
  }

  virtual ~NormalFileHandleOp();

  // Must be overridden in subclasses. Called on the target thread to allow the
  // subclass to perform necessary file operations. A successful return value
  // will trigger a SendSuccessResult callback on the background thread while
  // a failure value will trigger a SendFailureResult callback.
  virtual nsresult DoFileWork(FileHandle* aFileHandle) = 0;

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(FileRequestResponse& aResponse) = 0;

 private:
  nsresult SendSuccessResult();

  bool SendFailureResult(nsresult aResultCode);

  virtual void RunOnThreadPool() override;

  virtual void RunOnOwningThread() override;

  // IPDL methods.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

class CopyFileHandleOp : public NormalFileHandleOp {
  class ProgressRunnable;

 protected:
  nsCOMPtr<nsISupports> mBufferStream;

  uint64_t mOffset;
  uint64_t mSize;

  bool mRead;

 protected:
  CopyFileHandleOp(FileHandle* aFileHandle)
      : NormalFileHandleOp(aFileHandle), mOffset(0), mSize(0), mRead(true) {}

  virtual nsresult DoFileWork(FileHandle* aFileHandle) override;

  virtual void Cleanup() override;
};

class CopyFileHandleOp::ProgressRunnable final : public Runnable {
  RefPtr<CopyFileHandleOp> mCopyFileHandleOp;
  uint64_t mProgress;
  uint64_t mProgressMax;

 public:
  ProgressRunnable(CopyFileHandleOp* aCopyFileHandleOp, uint64_t aProgress,
                   uint64_t aProgressMax)
      : Runnable("dom::CopyFileHandleOp::ProgressRunnable"),
        mCopyFileHandleOp(aCopyFileHandleOp),
        mProgress(aProgress),
        mProgressMax(aProgressMax) {}

 private:
  ~ProgressRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

class GetMetadataOp : public NormalFileHandleOp {
  friend class FileHandle;

  const FileRequestGetMetadataParams mParams;

 protected:
  FileRequestMetadata mMetadata;

 protected:
  // Only created by FileHandle.
  GetMetadataOp(FileHandle* aFileHandle, const FileRequestParams& aParams);

  ~GetMetadataOp() = default;

  virtual nsresult DoFileWork(FileHandle* aFileHandle) override;

  virtual void GetResponse(FileRequestResponse& aResponse) override;
};

class ReadOp final : public CopyFileHandleOp {
  friend class FileHandle;

  const FileRequestReadParams mParams;

 private:
  // Only created by FileHandle.
  ReadOp(FileHandle* aFileHandle, const FileRequestParams& aParams);

  ~ReadOp() = default;

  virtual bool Init(FileHandle* aFileHandle) override;

  virtual void GetResponse(FileRequestResponse& aResponse) override;
};

class WriteOp final : public CopyFileHandleOp {
  friend class FileHandle;

  const FileRequestWriteParams mParams;

 private:
  // Only created by FileHandle.
  WriteOp(FileHandle* aFileHandle, const FileRequestParams& aParams);

  ~WriteOp() = default;

  virtual bool Init(FileHandle* aFileHandle) override;

  virtual void GetResponse(FileRequestResponse& aResponse) override;
};

class TruncateOp final : public NormalFileHandleOp {
  friend class FileHandle;

  const FileRequestTruncateParams mParams;

 private:
  // Only created by FileHandle.
  TruncateOp(FileHandle* aFileHandle, const FileRequestParams& aParams);

  ~TruncateOp() = default;

  virtual nsresult DoFileWork(FileHandle* aFileHandle) override;

  virtual void GetResponse(FileRequestResponse& aResponse) override;
};

class FlushOp final : public NormalFileHandleOp {
  friend class FileHandle;

  const FileRequestFlushParams mParams;

 private:
  // Only created by FileHandle.
  FlushOp(FileHandle* aFileHandle, const FileRequestParams& aParams);

  ~FlushOp() = default;

  virtual nsresult DoFileWork(FileHandle* aFileHandle) override;

  virtual void GetResponse(FileRequestResponse& aResponse) override;
};

namespace {

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

FileHandleThreadPool* GetFileHandleThreadPoolFor(FileHandleStorage aStorage) {
  switch (aStorage) {
    case FILE_HANDLE_STORAGE_IDB:
      return mozilla::dom::indexedDB::GetFileHandleThreadPool();

    default:
      MOZ_CRASH("Bad file handle storage value!");
  }
}

nsresult ClampResultCode(nsresult aResultCode) {
  if (NS_SUCCEEDED(aResultCode) ||
      NS_ERROR_GET_MODULE(aResultCode) == NS_ERROR_MODULE_DOM_FILEHANDLE) {
    return aResultCode;
  }

  NS_WARNING(nsPrintfCString("Converting non-filehandle error code (0x%" PRIX32
                             ") to "
                             "NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR",
                             static_cast<uint32_t>(aResultCode))
                 .get());

  return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
}

}  // namespace

/*******************************************************************************
 * FileHandleThreadPool implementation
 ******************************************************************************/

FileHandleThreadPool::FileHandleThreadPool()
    : mOwningEventTarget(GetCurrentSerialEventTarget()),
      mShutdownRequested(false),
      mShutdownComplete(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOwningEventTarget);
  AssertIsOnOwningThread();
}

FileHandleThreadPool::~FileHandleThreadPool() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mDirectoryInfos.Count());
  MOZ_ASSERT(mCompleteCallbacks.IsEmpty());
  MOZ_ASSERT(mShutdownRequested);
  MOZ_ASSERT(mShutdownComplete);
}

// static
already_AddRefed<FileHandleThreadPool> FileHandleThreadPool::Create() {
  AssertIsOnBackgroundThread();

  RefPtr<FileHandleThreadPool> fileHandleThreadPool =
      new FileHandleThreadPool();
  fileHandleThreadPool->AssertIsOnOwningThread();

  if (NS_WARN_IF(NS_FAILED(fileHandleThreadPool->Init()))) {
    return nullptr;
  }

  return fileHandleThreadPool.forget();
}

#ifdef DEBUG

void FileHandleThreadPool::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mOwningEventTarget);

  bool current;
  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->IsOnCurrentThread(&current));
  MOZ_ASSERT(current);
}

nsIEventTarget* FileHandleThreadPool::GetThreadPoolEventTarget() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mThreadPool);

  return mThreadPool;
}

#endif  // DEBUG

void FileHandleThreadPool::Enqueue(FileHandle* aFileHandle,
                                   FileHandleOp* aFileHandleOp, bool aFinish) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);
  MOZ_ASSERT(!mShutdownRequested);

  BackgroundMutableFileParentBase* mutableFile = aFileHandle->GetMutableFile();

  const nsACString& directoryId = mutableFile->DirectoryId();
  const nsAString& fileName = mutableFile->FileName();
  bool modeIsWrite = aFileHandle->Mode() == FileMode::Readwrite;

  DirectoryInfo* directoryInfo =
      mDirectoryInfos
          .LookupOrInsertWith(
              directoryId,
              [&] { return UniquePtr<DirectoryInfo>(new DirectoryInfo(this)); })
          .get();

  FileHandleQueue* existingFileHandleQueue =
      directoryInfo->GetFileHandleQueue(aFileHandle);

  if (existingFileHandleQueue) {
    existingFileHandleQueue->Enqueue(aFileHandleOp);
    if (aFinish) {
      existingFileHandleQueue->Finish();
    }
    return;
  }

  bool lockedForReading = directoryInfo->IsFileLockedForReading(fileName);
  bool lockedForWriting = directoryInfo->IsFileLockedForWriting(fileName);

  if (modeIsWrite) {
    if (!lockedForWriting) {
      directoryInfo->LockFileForWriting(fileName);
    }
  } else {
    if (!lockedForReading) {
      directoryInfo->LockFileForReading(fileName);
    }
  }

  if (lockedForWriting || (lockedForReading && modeIsWrite)) {
    directoryInfo->CreateDelayedEnqueueInfo(aFileHandle, aFileHandleOp,
                                            aFinish);
  } else {
    FileHandleQueue* fileHandleQueue =
        directoryInfo->CreateFileHandleQueue(aFileHandle);

    if (aFileHandleOp) {
      fileHandleQueue->Enqueue(aFileHandleOp);
      if (aFinish) {
        fileHandleQueue->Finish();
      }
    }
  }
}

void FileHandleThreadPool::WaitForDirectoriesToComplete(
    nsTArray<nsCString>&& aDirectoryIds, nsIRunnable* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aDirectoryIds.IsEmpty());
  MOZ_ASSERT(aCallback);

  auto callback =
      MakeUnique<StoragesCompleteCallback>(std::move(aDirectoryIds), aCallback);

  if (!MaybeFireCallback(callback.get())) {
    mCompleteCallbacks.AppendElement(std::move(callback));
  }
}

void FileHandleThreadPool::Shutdown() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mShutdownRequested);
  MOZ_ASSERT(!mShutdownComplete);

  mShutdownRequested = true;

  if (!mThreadPool) {
    MOZ_ASSERT(!mDirectoryInfos.Count());
    MOZ_ASSERT(mCompleteCallbacks.IsEmpty());

    mShutdownComplete = true;
    return;
  }

  if (!mDirectoryInfos.Count()) {
    Cleanup();

    MOZ_ASSERT(mShutdownComplete);
    return;
  }

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil("FileHandleThreadPool::Shutdown"_ns,
                                     [&]() { return mShutdownComplete; }));
}

nsresult FileHandleThreadPool::Init() {
  AssertIsOnOwningThread();

  mThreadPool = new nsThreadPool();

  nsresult rv = mThreadPool->SetName("FileHandles"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mThreadPool->SetThreadLimit(kThreadLimit);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mThreadPool->SetIdleThreadLimit(kIdleThreadLimit);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mThreadPool->SetIdleThreadTimeout(kIdleThreadTimeoutMs);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void FileHandleThreadPool::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mThreadPool);
  MOZ_ASSERT(mShutdownRequested);
  MOZ_ASSERT(!mShutdownComplete);
  MOZ_ASSERT(!mDirectoryInfos.Count());

  MOZ_ALWAYS_SUCCEEDS(mThreadPool->Shutdown());

  if (!mCompleteCallbacks.IsEmpty()) {
    // Run all callbacks manually now.
    for (uint32_t count = mCompleteCallbacks.Length(), index = 0; index < count;
         index++) {
      UniquePtr<StoragesCompleteCallback> completeCallback =
          std::move(mCompleteCallbacks[index]);
      MOZ_ASSERT(completeCallback);
      MOZ_ASSERT(completeCallback->mCallback);

      Unused << completeCallback->mCallback->Run();
    }

    mCompleteCallbacks.Clear();

    // And make sure they get processed.
    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

    MOZ_ALWAYS_SUCCEEDS(NS_ProcessPendingEvents(currentThread));
  }

  mShutdownComplete = true;
}

void FileHandleThreadPool::FinishFileHandle(FileHandle* aFileHandle) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);

  BackgroundMutableFileParentBase* mutableFile = aFileHandle->GetMutableFile();
  const nsACString& directoryId = mutableFile->DirectoryId();

  DirectoryInfo* directoryInfo;
  if (!mDirectoryInfos.Get(directoryId, &directoryInfo)) {
    NS_ERROR("We don't know anyting about this directory?!");
    return;
  }

  directoryInfo->RemoveFileHandleQueue(aFileHandle);

  if (!directoryInfo->HasRunningFileHandles()) {
    mDirectoryInfos.Remove(directoryId);

    // See if we need to fire any complete callbacks.
    mCompleteCallbacks.RemoveElementsBy(
        [this](const UniquePtr<StoragesCompleteCallback>& callback) {
          return MaybeFireCallback(callback.get());
        });

    if (mShutdownRequested && !mDirectoryInfos.Count()) {
      Cleanup();
    }
  }
}

bool FileHandleThreadPool::MaybeFireCallback(
    StoragesCompleteCallback* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!aCallback->mDirectoryIds.IsEmpty());
  MOZ_ASSERT(aCallback->mCallback);

  for (uint32_t count = aCallback->mDirectoryIds.Length(), index = 0;
       index < count; index++) {
    const nsCString& directoryId = aCallback->mDirectoryIds[index];
    MOZ_ASSERT(!directoryId.IsEmpty());

    if (mDirectoryInfos.Get(directoryId, nullptr)) {
      return false;
    }
  }

  aCallback->mCallback->Run();
  return true;
}

FileHandleThreadPool::FileHandleQueue::FileHandleQueue(
    FileHandleThreadPool* aFileHandleThreadPool, FileHandle* aFileHandle)
    : Runnable("dom::FileHandleThreadPool::FileHandleQueue"),
      mOwningFileHandleThreadPool(aFileHandleThreadPool),
      mFileHandle(aFileHandle),
      mShouldFinish(false) {
  MOZ_ASSERT(aFileHandleThreadPool);
  aFileHandleThreadPool->AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);
}

void FileHandleThreadPool::FileHandleQueue::Enqueue(
    FileHandleOp* aFileHandleOp) {
  MOZ_ASSERT(!mShouldFinish, "Enqueue called after Finish!");

  mQueue.AppendElement(aFileHandleOp);

  ProcessQueue();
}

void FileHandleThreadPool::FileHandleQueue::Finish() {
  MOZ_ASSERT(!mShouldFinish, "Finish called more than once!");

  mShouldFinish = true;
}

void FileHandleThreadPool::FileHandleQueue::ProcessQueue() {
  if (mCurrentOp) {
    return;
  }

  if (mQueue.IsEmpty()) {
    if (mShouldFinish) {
      mOwningFileHandleThreadPool->FinishFileHandle(mFileHandle);

      // Make sure this is released on this thread.
      mOwningFileHandleThreadPool = nullptr;
    }

    return;
  }

  mCurrentOp = mQueue[0];
  mQueue.RemoveElementAt(0);

  nsCOMPtr<nsIThreadPool> threadPool = mOwningFileHandleThreadPool->mThreadPool;
  MOZ_ASSERT(threadPool);

  MOZ_ALWAYS_SUCCEEDS(threadPool->Dispatch(this, NS_DISPATCH_NORMAL));
}

NS_IMETHODIMP
FileHandleThreadPool::FileHandleQueue::Run() {
  MOZ_ASSERT(mCurrentOp);

  if (IsOnBackgroundThread()) {
    RefPtr<FileHandleOp> currentOp;

    mCurrentOp.swap(currentOp);
    ProcessQueue();

    currentOp->RunOnOwningThread();
  } else {
    mCurrentOp->RunOnThreadPool();

    nsCOMPtr<nsIEventTarget> backgroundThread = mCurrentOp->OwningThread();

    MOZ_ALWAYS_SUCCEEDS(backgroundThread->Dispatch(this, NS_DISPATCH_NORMAL));
  }

  return NS_OK;
}

auto FileHandleThreadPool::DirectoryInfo::CreateFileHandleQueue(
    FileHandle* aFileHandle) -> FileHandleQueue* {
  RefPtr<FileHandleQueue>* fileHandleQueue = mFileHandleQueues.AppendElement();
  *fileHandleQueue =
      new FileHandleQueue(mOwningFileHandleThreadPool, aFileHandle);
  return fileHandleQueue->get();
}

auto FileHandleThreadPool::DirectoryInfo::GetFileHandleQueue(
    FileHandle* aFileHandle) -> FileHandleQueue* {
  uint32_t count = mFileHandleQueues.Length();
  for (uint32_t index = 0; index < count; index++) {
    RefPtr<FileHandleQueue>& fileHandleQueue = mFileHandleQueues[index];
    if (fileHandleQueue->mFileHandle == aFileHandle) {
      return fileHandleQueue;
    }
  }
  return nullptr;
}

void FileHandleThreadPool::DirectoryInfo::RemoveFileHandleQueue(
    FileHandle* aFileHandle) {
  for (uint32_t index = 0; index < mDelayedEnqueueInfos.Length(); index++) {
    if (mDelayedEnqueueInfos[index].mFileHandle == aFileHandle) {
      MOZ_ASSERT(!mDelayedEnqueueInfos[index].mFileHandleOp, "Should be null!");
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
    FileHandle* fileHandle = mFileHandleQueues[index]->mFileHandle;
    if (fileHandle == aFileHandle) {
      MOZ_ASSERT(count == fileHandleCount, "More than one match?!");

      mFileHandleQueues.RemoveElementAt(index);
      index--;
      count--;

      continue;
    }

    const nsAString& fileName = fileHandle->GetMutableFile()->FileName();

    if (fileHandle->Mode() == FileMode::Readwrite) {
      if (!IsFileLockedForWriting(fileName)) {
        LockFileForWriting(fileName);
      }
    } else {
      if (!IsFileLockedForReading(fileName)) {
        LockFileForReading(fileName);
      }
    }
  }

  MOZ_ASSERT(mFileHandleQueues.Length() == fileHandleCount - 1,
             "Didn't find the file handle we were looking for!");

  nsTArray<DelayedEnqueueInfo> delayedEnqueueInfos =
      std::move(mDelayedEnqueueInfos);

  for (uint32_t index = 0; index < delayedEnqueueInfos.Length(); index++) {
    DelayedEnqueueInfo& delayedEnqueueInfo = delayedEnqueueInfos[index];
    mOwningFileHandleThreadPool->Enqueue(delayedEnqueueInfo.mFileHandle,
                                         delayedEnqueueInfo.mFileHandleOp,
                                         delayedEnqueueInfo.mFinish);
  }
}

auto FileHandleThreadPool::DirectoryInfo::CreateDelayedEnqueueInfo(
    FileHandle* aFileHandle, FileHandleOp* aFileHandleOp, bool aFinish)
    -> DelayedEnqueueInfo* {
  DelayedEnqueueInfo* info = mDelayedEnqueueInfos.AppendElement();
  info->mFileHandle = aFileHandle;
  info->mFileHandleOp = aFileHandleOp;
  info->mFinish = aFinish;
  return info;
}

FileHandleThreadPool::StoragesCompleteCallback::StoragesCompleteCallback(
    nsTArray<nsCString>&& aDirectoryIds, nsIRunnable* aCallback)
    : mDirectoryIds(std::move(aDirectoryIds)), mCallback(aCallback) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mDirectoryIds.IsEmpty());
  MOZ_ASSERT(aCallback);

  MOZ_COUNT_CTOR(FileHandleThreadPool::StoragesCompleteCallback);
}

FileHandleThreadPool::StoragesCompleteCallback::~StoragesCompleteCallback() {
  AssertIsOnBackgroundThread();

  MOZ_COUNT_DTOR(FileHandleThreadPool::StoragesCompleteCallback);
}

/*******************************************************************************
 * BackgroundMutableFileParentBase
 ******************************************************************************/

BackgroundMutableFileParentBase::BackgroundMutableFileParentBase(
    FileHandleStorage aStorage, const nsACString& aDirectoryId,
    const nsAString& aFileName, nsIFile* aFile)
    : mDirectoryId(aDirectoryId),
      mFileName(aFileName),
      mStorage(aStorage),
      mInvalidated(false),
      mActorWasAlive(false),
      mActorDestroyed(false),
      mFile(aFile) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aStorage != FILE_HANDLE_STORAGE_MAX);
  MOZ_ASSERT(!aDirectoryId.IsEmpty());
  MOZ_ASSERT(!aFileName.IsEmpty());
  MOZ_ASSERT(aFile);
}

BackgroundMutableFileParentBase::~BackgroundMutableFileParentBase() {
  MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);
}

void BackgroundMutableFileParentBase::Invalidate() {
  AssertIsOnBackgroundThread();

  class MOZ_STACK_CLASS Helper final {
   public:
    static bool InvalidateFileHandles(nsTHashSet<FileHandle*>& aTable) {
      AssertIsOnBackgroundThread();

      const uint32_t count = aTable.Count();
      if (!count) {
        return true;
      }

      nsTArray<RefPtr<FileHandle>> fileHandles;
      if (NS_WARN_IF(!fileHandles.SetCapacity(count, fallible))) {
        return false;
      }

      // This can't fail, since we already reserved the required capacity.
      std::copy(aTable.cbegin(), aTable.cend(), MakeBackInserter(fileHandles));

      for (uint32_t index = 0; index < count; index++) {
        RefPtr<FileHandle> fileHandle = std::move(fileHandles[index]);
        MOZ_ASSERT(fileHandle);

        fileHandle->Invalidate();
      }

      return true;
    }
  };

  if (mInvalidated) {
    return;
  }

  mInvalidated = true;

  if (!Helper::InvalidateFileHandles(mFileHandles)) {
    NS_WARNING("Failed to abort all file handles!");
  }
}

bool BackgroundMutableFileParentBase::RegisterFileHandle(
    FileHandle* aFileHandle) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileHandle);
  MOZ_ASSERT(!mFileHandles.Contains(aFileHandle));
  MOZ_ASSERT(!mInvalidated);

  if (NS_WARN_IF(!mFileHandles.Insert(aFileHandle, fallible))) {
    return false;
  }

  if (mFileHandles.Count() == 1) {
    NoteActiveState();
  }

  return true;
}

void BackgroundMutableFileParentBase::UnregisterFileHandle(
    FileHandle* aFileHandle) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileHandle);
  MOZ_ASSERT(mFileHandles.Contains(aFileHandle));

  mFileHandles.Remove(aFileHandle);

  if (!mFileHandles.Count()) {
    NoteInactiveState();
  }
}

void BackgroundMutableFileParentBase::SetActorAlive() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!mActorDestroyed);

  mActorWasAlive = true;

  // This reference will be absorbed by IPDL and released when the actor is
  // destroyed.
  AddRef();
}

already_AddRefed<nsISupports> BackgroundMutableFileParentBase::CreateStream(
    bool aReadOnly) {
  AssertIsOnBackgroundThread();

  nsresult rv;

  if (aReadOnly) {
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), mFile, -1, -1,
                                    nsIFileInputStream::DEFER_OPEN);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    return stream.forget();
  }

  nsCOMPtr<nsIFileStream> stream;
  rv = NS_NewLocalFileStream(getter_AddRefs(stream), mFile, -1, -1,
                             nsIFileStream::DEFER_OPEN);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return stream.forget();
}

void BackgroundMutableFileParentBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!IsInvalidated()) {
    Invalidate();
  }
}

PBackgroundFileHandleParent*
BackgroundMutableFileParentBase::AllocPBackgroundFileHandleParent(
    const FileMode& aMode) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(aMode != FileMode::Readonly && aMode != FileMode::Readwrite)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<FileHandle> fileHandle = new FileHandle(this, aMode);

  return fileHandle.forget().take();
}

mozilla::ipc::IPCResult
BackgroundMutableFileParentBase::RecvPBackgroundFileHandleConstructor(
    PBackgroundFileHandleParent* aActor, const FileMode& aMode) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aMode == FileMode::Readonly || aMode == FileMode::Readwrite);

  FileHandleThreadPool* fileHandleThreadPool =
      GetFileHandleThreadPoolFor(mStorage);
  MOZ_ASSERT(fileHandleThreadPool);

  auto* fileHandle = static_cast<FileHandle*>(aActor);

  // Add a placeholder for this file handle immediately.
  fileHandleThreadPool->Enqueue(fileHandle, nullptr, false);

  fileHandle->SetActive();

  if (NS_WARN_IF(!RegisterFileHandle(fileHandle))) {
    fileHandle->Abort(/* aForce */ false);
    return IPC_OK();
  }

  return IPC_OK();
}

bool BackgroundMutableFileParentBase::DeallocPBackgroundFileHandleParent(
    PBackgroundFileHandleParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<FileHandle> fileHandle = dont_AddRef(static_cast<FileHandle*>(aActor));
  return true;
}

mozilla::ipc::IPCResult BackgroundMutableFileParentBase::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundMutableFileParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BackgroundMutableFileParentBase::RecvGetFileId(
    int64_t* aFileId) {
  AssertIsOnBackgroundThread();

  *aFileId = -1;
  return IPC_OK();
}

/*******************************************************************************
 * FileHandle
 ******************************************************************************/

FileHandle::FileHandle(BackgroundMutableFileParentBase* aMutableFile,
                       FileMode aMode)
    : mMutableFile(aMutableFile),
      mActiveRequestCount(0),
      mStorage(aMutableFile->Storage()),
      mInvalidatedOnAnyThread(false),
      mMode(aMode),
      mHasBeenActive(false),
      mActorDestroyed(false),
      mInvalidated(false),
      mAborted(false),
      mFinishOrAbortReceived(false),
      mFinishedOrAborted(false),
      mForceAborted(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aMutableFile);

#ifdef DEBUG
  FileHandleThreadPool* fileHandleThreadPool =
      GetFileHandleThreadPoolFor(mStorage);
  MOZ_ASSERT(fileHandleThreadPool);

  mThreadPoolEventTarget = fileHandleThreadPool->GetThreadPoolEventTarget();
#endif
}

FileHandle::~FileHandle() {
  MOZ_ASSERT(!mActiveRequestCount);
  MOZ_ASSERT(mActorDestroyed);
  MOZ_ASSERT_IF(mHasBeenActive, mFinishedOrAborted);
}

void FileHandle::AssertIsOnThreadPool() const {
  MOZ_ASSERT(mThreadPoolEventTarget);
  DebugOnly<bool> current;
  MOZ_ASSERT(NS_SUCCEEDED(mThreadPoolEventTarget->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

nsresult FileHandle::GetOrCreateStream(nsISupports** aStream) {
  AssertIsOnBackgroundThread();

  if (!mStream) {
    nsCOMPtr<nsISupports> stream =
        mMutableFile->CreateStream(mMode == FileMode::Readonly);
    if (NS_WARN_IF(!stream)) {
      return NS_ERROR_FAILURE;
    }

    stream.swap(mStream);
  }

  nsCOMPtr<nsISupports> stream(mStream);
  stream.forget(aStream);

  return NS_OK;
}

void FileHandle::Abort(bool aForce) {
  AssertIsOnBackgroundThread();

  mAborted = true;

  if (aForce) {
    mForceAborted = true;
  }

  MaybeFinishOrAbort();
}

void FileHandle::NoteActiveRequest() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveRequestCount < UINT64_MAX);

  mActiveRequestCount++;
}

void FileHandle::NoteFinishedRequest() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActiveRequestCount);

  mActiveRequestCount--;

  MaybeFinishOrAbort();
}

void FileHandle::Invalidate() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mInvalidated == mInvalidatedOnAnyThread);

  if (!mInvalidated) {
    mInvalidated = true;
    mInvalidatedOnAnyThread = true;

    Abort(/* aForce */ true);
  }
}

void FileHandle::SendCompleteNotification(bool aAborted) {
  AssertIsOnBackgroundThread();

  if (!IsActorDestroyed()) {
    Unused << SendComplete(aAborted);
  }
}

bool FileHandle::VerifyRequestParams(const FileRequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != FileRequestParams::T__None);

  switch (aParams.type()) {
    case FileRequestParams::TFileRequestGetMetadataParams: {
      const FileRequestGetMetadataParams& params =
          aParams.get_FileRequestGetMetadataParams();

      if (NS_WARN_IF(!params.size() && !params.lastModified())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case FileRequestParams::TFileRequestReadParams: {
      const FileRequestReadParams& params = aParams.get_FileRequestReadParams();

      if (NS_WARN_IF(params.offset() == UINT64_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(!params.size())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(params.size() > UINT32_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case FileRequestParams::TFileRequestWriteParams: {
      if (NS_WARN_IF(mMode != FileMode::Readwrite)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const FileRequestWriteParams& params =
          aParams.get_FileRequestWriteParams();

      if (NS_WARN_IF(!params.dataLength())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(!VerifyRequestData(params.data()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case FileRequestParams::TFileRequestTruncateParams: {
      if (NS_WARN_IF(mMode != FileMode::Readwrite)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      const FileRequestTruncateParams& params =
          aParams.get_FileRequestTruncateParams();

      if (NS_WARN_IF(params.offset() == UINT64_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case FileRequestParams::TFileRequestFlushParams: {
      if (NS_WARN_IF(mMode != FileMode::Readwrite)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

bool FileHandle::VerifyRequestData(const FileRequestData& aData) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aData.type() != FileRequestData::T__None);

  switch (aData.type()) {
    case FileRequestData::TFileRequestStringData: {
      const FileRequestStringData& data = aData.get_FileRequestStringData();

      if (NS_WARN_IF(data.string().IsEmpty())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case FileRequestData::TFileRequestBlobData: {
      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

void FileHandle::FinishOrAbort() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mFinishedOrAborted);

  mFinishedOrAborted = true;

  if (!mHasBeenActive) {
    return;
  }

  RefPtr<FinishOp> finishOp = new FinishOp(this, mAborted);

  FileHandleThreadPool* fileHandleThreadPool =
      GetFileHandleThreadPoolFor(mStorage);
  MOZ_ASSERT(fileHandleThreadPool);

  fileHandleThreadPool->Enqueue(this, finishOp, true);
}

void FileHandle::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!mFinishedOrAborted) {
    mAborted = true;

    mForceAborted = true;

    MaybeFinishOrAbort();
  }
}

mozilla::ipc::IPCResult FileHandle::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!IsActorDestroyed());

  IProtocol* mgr = Manager();
  if (!PBackgroundFileHandleParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FileHandle::RecvFinish() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mFinishOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mFinishOrAbortReceived = true;

  MaybeFinishOrAbort();
  return IPC_OK();
}

mozilla::ipc::IPCResult FileHandle::RecvAbort() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mFinishOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mFinishOrAbortReceived = true;

  Abort(/* aForce */ false);
  return IPC_OK();
}

PBackgroundFileRequestParent* FileHandle::AllocPBackgroundFileRequestParent(
    const FileRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != FileRequestParams::T__None);

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  PBackgroundParent* backgroundActor = GetBackgroundParent();
  MOZ_ASSERT(backgroundActor);

  bool trustParams = !BackgroundParent::IsOtherProcessActor(backgroundActor);
#endif

  if (NS_WARN_IF(!trustParams && !VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(mFinishOrAbortReceived)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<NormalFileHandleOp> actor;

  switch (aParams.type()) {
    case FileRequestParams::TFileRequestGetMetadataParams:
      actor = new GetMetadataOp(this, aParams);
      break;

    case FileRequestParams::TFileRequestReadParams:
      actor = new ReadOp(this, aParams);
      break;

    case FileRequestParams::TFileRequestWriteParams:
      actor = new WriteOp(this, aParams);
      break;

    case FileRequestParams::TFileRequestTruncateParams:
      actor = new TruncateOp(this, aParams);
      break;

    case FileRequestParams::TFileRequestFlushParams:
      actor = new FlushOp(this, aParams);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult FileHandle::RecvPBackgroundFileRequestConstructor(
    PBackgroundFileRequestParent* aActor, const FileRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != FileRequestParams::T__None);

  auto* op = static_cast<NormalFileHandleOp*>(aActor);

  if (NS_WARN_IF(!op->Init(this))) {
    op->Cleanup();
    return IPC_FAIL_NO_REASON(this);
  }

  op->Enqueue();
  return IPC_OK();
}

bool FileHandle::DeallocPBackgroundFileRequestParent(
    PBackgroundFileRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<NormalFileHandleOp> actor =
      dont_AddRef(static_cast<NormalFileHandleOp*>(aActor));
  return true;
}

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

void FileHandleOp::Enqueue() {
  AssertIsOnOwningThread();

  FileHandleThreadPool* fileHandleThreadPool =
      GetFileHandleThreadPoolFor(mFileHandle->Storage());
  MOZ_ASSERT(fileHandleThreadPool);

  fileHandleThreadPool->Enqueue(mFileHandle, this, false);

#ifdef DEBUG
  mEnqueued = true;
#endif

  mFileHandle->NoteActiveRequest();
}

void FileHandle::FinishOp::RunOnThreadPool() {
  AssertIsOnThreadPool();
  MOZ_ASSERT(mFileHandle);

  nsCOMPtr<nsISupports>& stream = mFileHandle->mStream;

  if (!stream) {
    return;
  }

  nsCOMPtr<nsIInputStream> inputStream = do_QueryInterface(stream);
  MOZ_ASSERT(inputStream);

  MOZ_ALWAYS_SUCCEEDS(inputStream->Close());

  stream = nullptr;
}

void FileHandle::FinishOp::RunOnOwningThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandle);

  mFileHandle->SendCompleteNotification(mAborted);

  mFileHandle->GetMutableFile()->UnregisterFileHandle(mFileHandle);

  mFileHandle = nullptr;
}

NormalFileHandleOp::~NormalFileHandleOp() {
  MOZ_ASSERT(!mFileHandle,
             "NormalFileHandleOp::Cleanup() was not called by a subclass!");
}

bool NormalFileHandleOp::Init(FileHandle* aFileHandle) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);

  nsresult rv = aFileHandle->GetOrCreateStream(getter_AddRefs(mFileStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

void NormalFileHandleOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandle);
  MOZ_ASSERT_IF(mEnqueued && !IsActorDestroyed(), mResponseSent);

  mFileHandle = nullptr;
}

nsresult NormalFileHandleOp::SendSuccessResult() {
  AssertIsOnOwningThread();

  if (!IsActorDestroyed()) {
    FileRequestResponse response;
    GetResponse(response);

    MOZ_ASSERT(response.type() != FileRequestResponse::T__None);

    if (response.type() == FileRequestResponse::Tnsresult) {
      MOZ_ASSERT(NS_FAILED(response.get_nsresult()));

      return response.get_nsresult();
    }

    if (NS_WARN_IF(
            !PBackgroundFileRequestParent::Send__delete__(this, response))) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }
  }

#ifdef DEBUG
  mResponseSent = true;
#endif

  return NS_OK;
}

bool NormalFileHandleOp::SendFailureResult(nsresult aResultCode) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));

  bool result = false;

  if (!IsActorDestroyed()) {
    result = PBackgroundFileRequestParent::Send__delete__(
        this, ClampResultCode(aResultCode));
  }

#ifdef DEBUG
  mResponseSent = true;
#endif

  return result;
}

void NormalFileHandleOp::RunOnThreadPool() {
  AssertIsOnThreadPool();
  MOZ_ASSERT(mFileHandle);
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));

  // There are several cases where we don't actually have to to any work here.

  if (mFileHandleIsAborted) {
    // This transaction is already set to be aborted.
    mResultCode = NS_ERROR_DOM_FILEHANDLE_ABORT_ERR;
  } else if (mFileHandle->IsInvalidatedOnAnyThread()) {
    // This file handle is being invalidated.
    mResultCode = NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
  } else if (!OperationMayProceed()) {
    // The operation was canceled in some way, likely because the child process
    // has crashed.
    mResultCode = NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
  } else {
    nsresult rv = DoFileWork(mFileHandle);
    if (NS_FAILED(rv)) {
      mResultCode = rv;
    }
  }
}

void NormalFileHandleOp::RunOnOwningThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandle);

  if (NS_WARN_IF(IsActorDestroyed())) {
    // Don't send any notifications if the actor was destroyed already.
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }
  } else {
    if (mFileHandle->IsInvalidated()) {
      mResultCode = NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    } else if (mFileHandle->IsAborted()) {
      // Aborted file handles always see their requests fail with ABORT_ERR,
      // even if the request succeeded or failed with another error.
      mResultCode = NS_ERROR_DOM_FILEHANDLE_ABORT_ERR;
    } else if (NS_SUCCEEDED(mResultCode)) {
      // This may release the IPDL reference.
      mResultCode = SendSuccessResult();
    }

    if (NS_FAILED(mResultCode)) {
      // This should definitely release the IPDL reference.
      if (!SendFailureResult(mResultCode)) {
        // Abort the file handle.
        mFileHandle->Abort(/* aForce */ false);
      }
    }
  }

  mFileHandle->NoteFinishedRequest();

  Cleanup();
}

void NormalFileHandleOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

nsresult CopyFileHandleOp::DoFileWork(FileHandle* aFileHandle) {
  AssertIsOnThreadPool();

  nsCOMPtr<nsIInputStream> inputStream;
  nsCOMPtr<nsIOutputStream> outputStream;

  if (mRead) {
    inputStream = do_QueryInterface(mFileStream);
    outputStream = do_QueryInterface(mBufferStream);
  } else {
    inputStream = do_QueryInterface(mBufferStream);
    outputStream = do_QueryInterface(mFileStream);
  }

  MOZ_ASSERT(inputStream);
  MOZ_ASSERT(outputStream);

  nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(mFileStream);

  nsresult rv;

  if (seekableStream) {
    if (mOffset == UINT64_MAX) {
      rv = seekableStream->Seek(nsISeekableStream::NS_SEEK_END, 0);
    } else {
      rv = seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mOffset = 0;

  do {
    char copyBuffer[kStreamCopyBlockSize];

    uint64_t max = mSize - mOffset;
    if (max == 0) {
      break;
    }

    uint32_t count = sizeof(copyBuffer);
    if (count > max) {
      count = max;
    }

    uint32_t numRead;
    rv = inputStream->Read(copyBuffer, count, &numRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!numRead) {
      break;
    }

    uint32_t numWrite;
    rv = outputStream->Write(copyBuffer, numRead, &numWrite);
    if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      rv = NS_ERROR_DOM_FILEHANDLE_QUOTA_ERR;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(numWrite != numRead)) {
      return NS_ERROR_FAILURE;
    }

    mOffset += numWrite;

    nsCOMPtr<nsIRunnable> runnable = new ProgressRunnable(this, mOffset, mSize);

    mOwningEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);
  } while (true);

  if (mOffset < mSize) {
    // end-of-file reached
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(mOffset == mSize);

  if (mRead) {
    MOZ_ALWAYS_SUCCEEDS(outputStream->Close());
  } else {
    MOZ_ALWAYS_SUCCEEDS(inputStream->Close());
  }

  return NS_OK;
}

void CopyFileHandleOp::Cleanup() {
  AssertIsOnOwningThread();

  mBufferStream = nullptr;

  NormalFileHandleOp::Cleanup();
}

NS_IMETHODIMP
CopyFileHandleOp::ProgressRunnable::Run() {
  AssertIsOnBackgroundThread();

  Unused << mCopyFileHandleOp->SendProgress(mProgress, mProgressMax);

  mCopyFileHandleOp = nullptr;

  return NS_OK;
}

GetMetadataOp::GetMetadataOp(FileHandle* aFileHandle,
                             const FileRequestParams& aParams)
    : NormalFileHandleOp(aFileHandle),
      mParams(aParams.get_FileRequestGetMetadataParams()) {
  MOZ_ASSERT(aParams.type() ==
             FileRequestParams::TFileRequestGetMetadataParams);
}

nsresult GetMetadataOp::DoFileWork(FileHandle* aFileHandle) {
  AssertIsOnThreadPool();

  nsresult rv;

  if (mFileHandle->Mode() == FileMode::Readwrite) {
    // Force a flush (so all pending writes are flushed to the disk and file
    // metadata is updated too).

    nsCOMPtr<nsIOutputStream> ostream = do_QueryInterface(mFileStream);
    MOZ_ASSERT(ostream);

    rv = ostream->Flush();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIFileMetadata> metadata = do_QueryInterface(mFileStream);
  MOZ_ASSERT(metadata);

  if (mParams.size()) {
    int64_t size;
    rv = metadata->GetSize(&size);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(size < 0)) {
      return NS_ERROR_FAILURE;
    }

    mMetadata.size() = Some(uint64_t(size));
  } else {
    mMetadata.size() = Nothing();
  }

  if (mParams.lastModified()) {
    int64_t lastModified;
    rv = metadata->GetLastModified(&lastModified);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mMetadata.lastModified() = Some(lastModified);
  } else {
    mMetadata.lastModified() = Nothing();
  }

  return NS_OK;
}

void GetMetadataOp::GetResponse(FileRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = FileRequestGetMetadataResponse(mMetadata);
}

ReadOp::ReadOp(FileHandle* aFileHandle, const FileRequestParams& aParams)
    : CopyFileHandleOp(aFileHandle),
      mParams(aParams.get_FileRequestReadParams()) {
  MOZ_ASSERT(aParams.type() == FileRequestParams::TFileRequestReadParams);
}

bool ReadOp::Init(FileHandle* aFileHandle) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);

  if (NS_WARN_IF(!NormalFileHandleOp::Init(aFileHandle))) {
    return false;
  }

  mBufferStream = MemoryOutputStream::Create(mParams.size());
  if (NS_WARN_IF(!mBufferStream)) {
    return false;
  }

  mOffset = mParams.offset();
  mSize = mParams.size();
  mRead = true;

  return true;
}

void ReadOp::GetResponse(FileRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  auto* stream = static_cast<MemoryOutputStream*>(mBufferStream.get());

  aResponse = FileRequestReadResponse(stream->Data());
}

WriteOp::WriteOp(FileHandle* aFileHandle, const FileRequestParams& aParams)
    : CopyFileHandleOp(aFileHandle),
      mParams(aParams.get_FileRequestWriteParams()) {
  MOZ_ASSERT(aParams.type() == FileRequestParams::TFileRequestWriteParams);
}

bool WriteOp::Init(FileHandle* aFileHandle) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);

  if (NS_WARN_IF(!NormalFileHandleOp::Init(aFileHandle))) {
    return false;
  }

  nsCOMPtr<nsIInputStream> inputStream;

  const FileRequestData& data = mParams.data();
  switch (data.type()) {
    case FileRequestData::TFileRequestStringData: {
      const FileRequestStringData& stringData =
          data.get_FileRequestStringData();

      const nsCString& string = stringData.string();

      nsresult rv =
          NS_NewCStringInputStream(getter_AddRefs(inputStream), string);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      break;
    }
    case FileRequestData::TFileRequestBlobData: {
      const FileRequestBlobData& blobData = data.get_FileRequestBlobData();

      RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(blobData.blob());
      if (NS_WARN_IF(!blobImpl)) {
        return false;
      }

      IgnoredErrorResult rv;
      blobImpl->CreateInputStream(getter_AddRefs(inputStream), rv);
      if (NS_WARN_IF(rv.Failed())) {
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  mBufferStream = inputStream;
  mOffset = mParams.offset();
  mSize = mParams.dataLength();
  mRead = false;

  return true;
}

void WriteOp::GetResponse(FileRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  aResponse = FileRequestWriteResponse();
}

TruncateOp::TruncateOp(FileHandle* aFileHandle,
                       const FileRequestParams& aParams)
    : NormalFileHandleOp(aFileHandle),
      mParams(aParams.get_FileRequestTruncateParams()) {
  MOZ_ASSERT(aParams.type() == FileRequestParams::TFileRequestTruncateParams);
}

nsresult TruncateOp::DoFileWork(FileHandle* aFileHandle) {
  AssertIsOnThreadPool();

  nsCOMPtr<nsISeekableStream> sstream = do_QueryInterface(mFileStream);
  MOZ_ASSERT(sstream);

  if (mParams.offset()) {
    nsCOMPtr<nsIFileMetadata> fileMetadata = do_QueryInterface(mFileStream);
    MOZ_ASSERT(fileMetadata);

    int64_t size;
    nsresult rv = fileMetadata->GetSize(&size);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(size >= 0);

    if (mParams.offset() > static_cast<uint64_t>(size)) {
      // Cannot extend the size of a file through truncate.
      return NS_ERROR_DOM_INVALID_MODIFICATION_ERR;
    }
  }

  // XXX If we allowed truncate to extend the file size, we would to ensure that
  // the quota limit is checked, e.g. by making FileQuotaStream override Seek.
  nsresult rv = sstream->Seek(nsISeekableStream::NS_SEEK_SET, mParams.offset());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = sstream->SetEOF();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void TruncateOp::GetResponse(FileRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  aResponse = FileRequestTruncateResponse();
}

FlushOp::FlushOp(FileHandle* aFileHandle, const FileRequestParams& aParams)
    : NormalFileHandleOp(aFileHandle),
      mParams(aParams.get_FileRequestFlushParams()) {
  MOZ_ASSERT(aParams.type() == FileRequestParams::TFileRequestFlushParams);
}

nsresult FlushOp::DoFileWork(FileHandle* aFileHandle) {
  AssertIsOnThreadPool();

  nsCOMPtr<nsIOutputStream> ostream = do_QueryInterface(mFileStream);
  MOZ_ASSERT(ostream);

  nsresult rv = ostream->Flush();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void FlushOp::GetResponse(FileRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  aResponse = FileRequestFlushResponse();
}

}  // namespace mozilla::dom
