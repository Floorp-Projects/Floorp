/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemSyncAccessHandle.h"

#include "fs/FileSystemRequestHandler.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FixedBufferOutputStream.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/FileSystemAccessHandleChild.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/FileSystemSyncAccessHandleBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsNetCID.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

namespace mozilla::dom {

namespace {

const uint32_t kStreamCopyBlockSize = 1024 * 1024;

nsresult AsyncCopy(nsIInputStream* aSource, nsIOutputStream* aSink,
                   const nsAsyncCopyMode aMode, const bool aCloseSource,
                   const bool aCloseSink,
                   std::function<void(uint32_t)>&& aProgressCallback,
                   std::function<void(nsresult)>&& aCompleteCallback) {
  QM_TRY_INSPECT(const auto& ioTarget,
                 MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                         MOZ_SELECT_OVERLOAD(do_GetService),
                                         NS_STREAMTRANSPORTSERVICE_CONTRACTID));

  struct CallbackClosure {
    CallbackClosure(std::function<void(uint32_t)>&& aProgressCallback,
                    std::function<void(nsresult)>&& aCompleteCallback) {
      mProgressCallbackWrapper = MakeUnique<std::function<void(uint32_t)>>(
          [progressCallback = std::move(aProgressCallback)](uint32_t count) {
            progressCallback(count);
          });

      mCompleteCallbackWrapper = MakeUnique<std::function<void(nsresult)>>(
          [completeCallback = std::move(aCompleteCallback)](nsresult rv) {
            completeCallback(rv);
          });
    }

    UniquePtr<std::function<void(uint32_t)>> mProgressCallbackWrapper;
    UniquePtr<std::function<void(nsresult)>> mCompleteCallbackWrapper;
  };

  auto* callbackClosure = new CallbackClosure(std::move(aProgressCallback),
                                              std::move(aCompleteCallback));

  QM_TRY(
      MOZ_TO_RESULT(NS_AsyncCopy(
          aSource, aSink, ioTarget, aMode, kStreamCopyBlockSize,
          [](void* aClosure, nsresult aRv) {
            auto* callbackClosure = static_cast<CallbackClosure*>(aClosure);
            (*callbackClosure->mCompleteCallbackWrapper)(aRv);
            delete callbackClosure;
          },
          callbackClosure, aCloseSource, aCloseSink, /* aCopierCtx */ nullptr,
          [](void* aClosure, uint32_t aCount) {
            auto* callbackClosure = static_cast<CallbackClosure*>(aClosure);
            (*callbackClosure->mProgressCallbackWrapper)(aCount);
          })),
      [callbackClosure](nsresult rv) {
        delete callbackClosure;
        return rv;
      });

  return NS_OK;
}

}  // namespace

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    RefPtr<FileSystemAccessHandleChild> aActor,
    nsCOMPtr<nsIRandomAccessStream> aStream,
    const fs::FileSystemEntryMetadata& aMetadata)
    : mGlobal(aGlobal),
      mManager(aManager),
      mActor(std::move(aActor)),
      mStream(std::move(aStream)),
      mMetadata(aMetadata),
      mClosed(false) {
  LOG(("Created SyncAccessHandle %p for stream %p", this, mStream.get()));

  // Connect with the actor directly in the constructor. This way the actor
  // can call `FileSystemSyncAccessHandle::ClearActor` when we call
  // `PFileSystemAccessHandleChild::Send__delete__` even when
  // FileSystemSyncAccessHandle::Create fails, in which case the not yet
  // fully constructed FileSystemSyncAccessHandle is being destroyed.
  mActor->SetAccessHandle(this);
}

FileSystemSyncAccessHandle::~FileSystemSyncAccessHandle() {
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(mClosed);
}

// static
Result<RefPtr<FileSystemSyncAccessHandle>, nsresult>
FileSystemSyncAccessHandle::Create(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    RefPtr<FileSystemAccessHandleChild> aActor,
    nsCOMPtr<nsIRandomAccessStream> aStream,
    const fs::FileSystemEntryMetadata& aMetadata) {
  RefPtr<FileSystemSyncAccessHandle> result = new FileSystemSyncAccessHandle(
      aGlobal, aManager, std::move(aActor), std::move(aStream), aMetadata);

  auto autoClose = MakeScopeExit([result] {
    MOZ_ASSERT(!result->mClosed);
    result->mClosed = true;
    result->mActor->SendClose();
  });

  WorkerPrivate* const workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
      workerPrivate, "FileSystemSyncAccessHandle", [result]() {
        if (!result->IsClosed()) {
          result->CloseInternal();
        }
      });
  QM_TRY(MOZ_TO_RESULT(workerRef));

  autoClose.release();

  result->mWorkerRef = std::move(workerRef);

  return result;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemSyncAccessHandle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemSyncAccessHandle)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(FileSystemSyncAccessHandle,
                                                   LastRelease())

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileSystemSyncAccessHandle)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FileSystemSyncAccessHandle)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  // Don't unlink mManager!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  if (!tmp->IsClosed()) {
    tmp->CloseInternal();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FileSystemSyncAccessHandle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void FileSystemSyncAccessHandle::LastRelease() {
  // We can't call `FileSystemSyncAccessHandle::Close` here because it may need
  // to keep FileSystemSyncAccessHandle object alive which isn't possible when
  // the object is about to be deleted. There are other mechanisms which ensure
  // that the object is correctly closed before destruction. For example the
  // object unlinking and the worker shutdown (we get notified about it via the
  // callback passed to `StrongWorkerRef`) are used to close the object if it
  // hasn't been closed yet.

  if (mActor) {
    PFileSystemAccessHandleChild::Send__delete__(mActor);
    MOZ_ASSERT(!mActor);
  }
}

void FileSystemSyncAccessHandle::ClearActor() {
  MOZ_ASSERT(mActor);

  mActor = nullptr;
}

void FileSystemSyncAccessHandle::CloseInternal() {
  MOZ_ASSERT(!mClosed);

  LOG(("%p: Closing", mStream.get()));

  mClosed = true;

  mStream->OutputStream()->Close();
  mStream = nullptr;

  if (mActor) {
    mActor->SendClose();
  }

  mWorkerRef = nullptr;
}

// WebIDL Boilerplate

nsIGlobalObject* FileSystemSyncAccessHandle::GetParentObject() const {
  return mGlobal;
}

JSObject* FileSystemSyncAccessHandle::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FileSystemSyncAccessHandle_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

uint64_t FileSystemSyncAccessHandle::Read(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
    const FileSystemReadWriteOptions& aOptions, ErrorResult& aRv) {
  return ReadOrWrite(aBuffer, aOptions, /* aRead */ true, aRv);
}

uint64_t FileSystemSyncAccessHandle::Write(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
    const FileSystemReadWriteOptions& aOptions, ErrorResult& aRv) {
  return ReadOrWrite(aBuffer, aOptions, /* aRead */ false, aRv);
}

void FileSystemSyncAccessHandle::Truncate(uint64_t aSize, ErrorResult& aError) {
  if (mClosed) {
    aError.ThrowInvalidStateError("SyncAccessHandle is closed");
    return;
  }

  auto throwAndReturn = [&aError](const nsresult rv) {
    aError.Throw(rv);
    return;
  };

  LOG(("%p: Truncate to %" PRIu64, mStream.get(), aSize));

  QM_TRY(MOZ_TO_RESULT(mStream->Seek(nsISeekableStream::NS_SEEK_SET, aSize)),
         throwAndReturn);

  QM_TRY(MOZ_TO_RESULT(mStream->SetEOF()), throwAndReturn);
}

uint64_t FileSystemSyncAccessHandle::GetSize(ErrorResult& aError) {
  if (mClosed) {
    aError.ThrowInvalidStateError("SyncAccessHandle is closed");
    return 0;
  }

  auto throwAndReturn = [&aError](const nsresult rv) {
    aError.Throw(rv);
    return 0;
  };

  nsCOMPtr<nsIFileMetadata> fileMetadata = do_QueryInterface(mStream);
  MOZ_ASSERT(fileMetadata);

  QM_TRY_INSPECT(const auto& size,
                 MOZ_TO_RESULT_INVOKE_MEMBER(fileMetadata, GetSize),
                 throwAndReturn);

  LOG(("%p: GetSize %" PRIu64, mStream.get(), size));
  return size;
}

void FileSystemSyncAccessHandle::Flush(ErrorResult& aError) {
  if (mClosed) {
    aError.ThrowInvalidStateError("SyncAccessHandle is closed");
    return;
  }

  LOG(("%p: Flush", mStream.get()));

  mStream->OutputStream()->Flush();
}

void FileSystemSyncAccessHandle::Close() {
  if (!IsClosed()) {
    CloseInternal();
  }
}

uint64_t FileSystemSyncAccessHandle::ReadOrWrite(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
    const FileSystemReadWriteOptions& aOptions, const bool aRead,
    ErrorResult& aRv) {
  if (mClosed) {
    aRv.ThrowInvalidStateError("SyncAccessHandle is closed");
    return 0;
  }

  auto throwAndReturn = [&aRv](const nsresult rv) {
    aRv.Throw(rv);
    return 0;
  };

  // Handle seek before read ('at')
  const auto at = [&aOptions]() -> uint64_t {
    if (aOptions.mAt.WasPassed()) {
      return aOptions.mAt.Value();
    }
    // Spec says default for at is 0 (2.6)
    return 0;
  }();

  const auto offset = CheckedInt<int64_t>(at);
  QM_TRY(MOZ_TO_RESULT(offset.isValid()), throwAndReturn);

  LOG_VERBOSE(("%p: Seeking to %" PRIu64, mStream.get(), offset.value()));

  QM_TRY(MOZ_TO_RESULT(
             mStream->Seek(nsISeekableStream::NS_SEEK_SET, offset.value())),
         throwAndReturn);

  const auto dataSpan = [&aBuffer]() {
    if (aBuffer.IsArrayBuffer()) {
      const ArrayBuffer& buffer = aBuffer.GetAsArrayBuffer();
      buffer.ComputeState();
      return Span{buffer.Data(), buffer.Length()};
    }
    MOZ_ASSERT(aBuffer.IsArrayBufferView());
    const ArrayBufferView& buffer = aBuffer.GetAsArrayBufferView();
    buffer.ComputeState();
    return Span{buffer.Data(), buffer.Length()};
  }();

  nsCOMPtr<nsIInputStream> inputStream;
  nsCOMPtr<nsIOutputStream> outputStream;

  if (aRead) {
    LOG_VERBOSE(("%p: Reading %zu bytes", mStream.get(), dataSpan.Length()));

    inputStream = mStream->InputStream();

    outputStream = FixedBufferOutputStream::Create(AsWritableChars(dataSpan));
  } else {
    LOG_VERBOSE(("%p: Writing %zu bytes", mStream.get(), dataSpan.Length()));

    QM_TRY(MOZ_TO_RESULT(NS_NewByteInputStream(getter_AddRefs(inputStream),
                                               AsChars(dataSpan),
                                               NS_ASSIGNMENT_DEPEND)),
           throwAndReturn);

    outputStream = mStream->OutputStream();
  }

  AutoSyncLoopHolder syncLoop(mWorkerRef->Private(), Canceling);

  nsCOMPtr<nsISerialEventTarget> syncLoopTarget =
      syncLoop.GetSerialEventTarget();
  QM_TRY(MOZ_TO_RESULT(syncLoopTarget), [&aRv](nsresult) {
    aRv.ThrowInvalidStateError("Worker is shutting down");
    return 0;
  });

  uint64_t totalCount = 0;

  QM_TRY(MOZ_TO_RESULT(AsyncCopy(
             inputStream, outputStream,
             aRead ? NS_ASYNCCOPY_VIA_WRITESEGMENTS
                   : NS_ASYNCCOPY_VIA_READSEGMENTS,
             /* aCloseSource */ !aRead, /* aCloseSink */ aRead,
             [&totalCount](uint32_t count) { totalCount += count; },
             [this, &syncLoopTarget](nsresult rv) {
               InvokeAsync(syncLoopTarget, __func__, [this, &syncLoopTarget]() {
                 mWorkerRef->Private()->AssertIsOnWorkerThread();

                 mWorkerRef->Private()->StopSyncLoop(syncLoopTarget, NS_OK);

                 return BoolPromise::CreateAndResolve(true, __func__);
               });
             })),
         throwAndReturn);

  MOZ_ALWAYS_SUCCEEDS(syncLoop.Run());

  return totalCount;
}

}  // namespace mozilla::dom
