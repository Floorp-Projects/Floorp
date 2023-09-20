/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemSyncAccessHandle.h"

#include "fs/FileSystemAsyncCopy.h"
#include "fs/FileSystemRequestHandler.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FixedBufferOutputStream.h"
#include "mozilla/MozPromise.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/FileSystemAccessHandleChild.h"
#include "mozilla/dom/FileSystemAccessHandleControlChild.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/FileSystemManagerChild.h"
#include "mozilla/dom/FileSystemSyncAccessHandleBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/fs/IPCRejectReporter.h"
#include "mozilla/dom/fs/TargetPtrHolder.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/RandomAccessStreamUtils.h"
#include "nsNetCID.h"
#include "nsStringStream.h"

namespace mozilla::dom {

namespace {

using SizePromise = Int64Promise;
const auto CreateAndRejectSizePromise = CreateAndRejectInt64Promise;

}  // namespace

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    mozilla::ipc::RandomAccessStreamParams&& aStreamParams,
    RefPtr<FileSystemAccessHandleChild> aActor,
    RefPtr<FileSystemAccessHandleControlChild> aControlActor,
    RefPtr<TaskQueue> aIOTaskQueue,
    const fs::FileSystemEntryMetadata& aMetadata)
    : mGlobal(aGlobal),
      mManager(aManager),
      mActor(std::move(aActor)),
      mControlActor(std::move(aControlActor)),
      mIOTaskQueue(std::move(aIOTaskQueue)),
      mStreamParams(std::move(aStreamParams)),
      mMetadata(aMetadata),
      mState(State::Initial) {
  LOG(("Created SyncAccessHandle %p", this));

  // Connect with the actor directly in the constructor. This way the actor
  // can call `FileSystemSyncAccessHandle::ClearActor` when we call
  // `PFileSystemAccessHandleChild::Send__delete__` even when
  // FileSystemSyncAccessHandle::Create fails, in which case the not yet
  // fully constructed FileSystemSyncAccessHandle is being destroyed.
  mActor->SetAccessHandle(this);

  mControlActor->SetAccessHandle(this);
}

FileSystemSyncAccessHandle::~FileSystemSyncAccessHandle() {
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(IsClosed());
}

// static
Result<RefPtr<FileSystemSyncAccessHandle>, nsresult>
FileSystemSyncAccessHandle::Create(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    mozilla::ipc::RandomAccessStreamParams&& aStreamParams,
    mozilla::ipc::ManagedEndpoint<PFileSystemAccessHandleChild>&&
        aAccessHandleChildEndpoint,
    mozilla::ipc::Endpoint<PFileSystemAccessHandleControlChild>&&
        aAccessHandleControlChildEndpoint,
    const fs::FileSystemEntryMetadata& aMetadata) {
  WorkerPrivate* const workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  auto accessHandleChild = MakeRefPtr<FileSystemAccessHandleChild>();

  QM_TRY(MOZ_TO_RESULT(
      aManager->ActorStrongRef()->BindPFileSystemAccessHandleEndpoint(
          std::move(aAccessHandleChildEndpoint), accessHandleChild)));

  auto accessHandleControlChild =
      MakeRefPtr<FileSystemAccessHandleControlChild>();

  aAccessHandleControlChildEndpoint.Bind(accessHandleControlChild,
                                         workerPrivate->ControlEventTarget());

  QM_TRY_UNWRAP(auto streamTransportService,
                MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                        MOZ_SELECT_OVERLOAD(do_GetService),
                                        NS_STREAMTRANSPORTSERVICE_CONTRACTID));

  RefPtr<TaskQueue> ioTaskQueue = TaskQueue::Create(
      streamTransportService.forget(), "FileSystemSyncAccessHandle");
  QM_TRY(MOZ_TO_RESULT(ioTaskQueue));

  RefPtr<FileSystemSyncAccessHandle> result = new FileSystemSyncAccessHandle(
      aGlobal, aManager, std::move(aStreamParams), std::move(accessHandleChild),
      std::move(accessHandleControlChild), std::move(ioTaskQueue), aMetadata);

  auto autoClose = MakeScopeExit([result] {
    MOZ_ASSERT(result->mState == State::Initial);
    result->mState = State::Closed;
    result->mActor->SendClose();
  });

  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
      workerPrivate, "FileSystemSyncAccessHandle", [result]() {
        if (result->IsOpen()) {
          // We don't need to use the result, we just need to begin the closing
          // process.
          Unused << result->BeginClose();
        }
      });
  QM_TRY(MOZ_TO_RESULT(workerRef));

  autoClose.release();

  result->mWorkerRef = std::move(workerRef);
  result->mState = State::Open;

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
  if (tmp->IsOpen()) {
    // We don't need to use the result, we just need to begin the closing
    // process.
    Unused << tmp->BeginClose();
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

    // `PFileSystemAccessHandleChild::Send__delete__` is supposed to call
    // `FileSystemAccessHandleChild::ActorDestroy` which in turn calls
    // `FileSystemSyncAccessHandle::ClearActor`, so `mActor` should be be null
    // at this point.
    MOZ_ASSERT(!mActor);
  }

  if (mControlActor) {
    mControlActor->Close();

    // `FileSystemAccessHandleControlChild::Close` is supposed to call
    // `FileSystemAccessHandleControlChild::ActorDestroy` which in turn calls
    // `FileSystemSyncAccessHandle::ClearControlActor`, so `mControlActor`
    // should be be null at this point.
    MOZ_ASSERT(!mControlActor);
  }
}

void FileSystemSyncAccessHandle::ClearActor() {
  MOZ_ASSERT(mActor);

  mActor = nullptr;
}

void FileSystemSyncAccessHandle::ClearControlActor() {
  // `mControlActor` is initialized in the constructor and this method is
  // supposed to be called only once.
  MOZ_ASSERT(mControlActor);

  mControlActor = nullptr;
}

bool FileSystemSyncAccessHandle::IsOpen() const {
  MOZ_ASSERT(mState != State::Initial);

  return mState == State::Open;
}

bool FileSystemSyncAccessHandle::IsClosing() const {
  MOZ_ASSERT(mState != State::Initial);

  return mState == State::Closing;
}

bool FileSystemSyncAccessHandle::IsClosed() const {
  MOZ_ASSERT(mState != State::Initial);

  return mState == State::Closed;
}

RefPtr<BoolPromise> FileSystemSyncAccessHandle::BeginClose() {
  MOZ_ASSERT(IsOpen());

  mState = State::Closing;

  InvokeAsync(mIOTaskQueue, __func__,
              [selfHolder = fs::TargetPtrHolder(this)]() {
                if (selfHolder->mStream) {
                  LOG(("%p: Closing", selfHolder->mStream.get()));

                  selfHolder->mStream->OutputStream()->Close();
                  selfHolder->mStream = nullptr;
                } else {
                  LOG(("Closing (no stream)"));

                  // If the stream was not deserialized, `mStreamParams` still
                  // contains a pre-opened file descriptor which needs to be
                  // closed here by moving `mStreamParams` to a local variable
                  // (the file descriptor will be closed for real when
                  // `streamParams` goes out of scope).

                  mozilla::ipc::RandomAccessStreamParams streamParams(
                      std::move(selfHolder->mStreamParams));
                }

                return BoolPromise::CreateAndResolve(true, __func__);
              })
      ->Then(mWorkerRef->Private()->ControlEventTarget(), __func__,
             [self = RefPtr(this)](const BoolPromise::ResolveOrRejectValue&) {
               return self->mIOTaskQueue->BeginShutdown();
             })
      ->Then(
          mWorkerRef->Private()->ControlEventTarget(), __func__,
          [self = RefPtr(this)](const ShutdownPromise::ResolveOrRejectValue&) {
            if (self->mControlActor) {
              RefPtr<BoolPromise::Private> promise =
                  new BoolPromise::Private(__func__);

              self->mControlActor->SendClose(
                  [promise](void_t&&) { promise->Resolve(true, __func__); },
                  [promise](const mozilla::ipc::ResponseRejectReason& aReason) {
                    fs::IPCRejectReporter(aReason);

                    promise->Reject(NS_ERROR_FAILURE, __func__);
                  });

              return RefPtr<BoolPromise>(promise);
            }

            return BoolPromise::CreateAndResolve(true, __func__);
          })
      ->Then(mWorkerRef->Private()->ControlEventTarget(), __func__,
             [self = RefPtr(this)](const BoolPromise::ResolveOrRejectValue&) {
               self->mWorkerRef = nullptr;

               self->mState = State::Closed;

               self->mClosePromiseHolder.ResolveIfExists(true, __func__);
             });

  return OnClose();
}

RefPtr<BoolPromise> FileSystemSyncAccessHandle::OnClose() {
  MOZ_ASSERT(mState == State::Closing);

  return mClosePromiseHolder.Ensure(__func__);
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
  if (!IsOpen()) {
    aError.ThrowInvalidStateError("SyncAccessHandle is closed");
    return;
  }

  MOZ_ASSERT(mWorkerRef);

  AutoSyncLoopHolder syncLoop(mWorkerRef->Private(), Canceling);

  nsCOMPtr<nsISerialEventTarget> syncLoopTarget =
      syncLoop.GetSerialEventTarget();
  QM_TRY(MOZ_TO_RESULT(syncLoopTarget), [&aError](nsresult) {
    aError.ThrowInvalidStateError("Worker is shutting down");
  });

  InvokeAsync(
      mIOTaskQueue, __func__,
      [selfHolder = fs::TargetPtrHolder(this), aSize]() {
        QM_TRY(MOZ_TO_RESULT(selfHolder->EnsureStream()),
               CreateAndRejectBoolPromise);

        LOG(("%p: Truncate to %" PRIu64, selfHolder->mStream.get(), aSize));
        int64_t offset = 0;
        QM_TRY(MOZ_TO_RESULT(selfHolder->mStream->Tell(&offset)),
               CreateAndRejectBoolPromise);
        QM_TRY(MOZ_TO_RESULT(selfHolder->mStream->Seek(
                   nsISeekableStream::NS_SEEK_SET, aSize)),
               CreateAndRejectBoolPromise);

        QM_TRY(MOZ_TO_RESULT(selfHolder->mStream->SetEOF()),
               CreateAndRejectBoolPromise);
        // restore cursor position (clamp to end of file)
        QM_TRY(MOZ_TO_RESULT(selfHolder->mStream->Seek(
                   nsISeekableStream::NS_SEEK_SET,
                   std::min((uint64_t)offset, aSize))),
               CreateAndRejectBoolPromise);

        return BoolPromise::CreateAndResolve(true, __func__);
      })
      ->Then(syncLoopTarget, __func__,
             [this, &syncLoopTarget](
                 const BoolPromise::ResolveOrRejectValue& aValue) {
               MOZ_ASSERT(mWorkerRef);

               mWorkerRef->Private()->AssertIsOnWorkerThread();

               mWorkerRef->Private()->StopSyncLoop(
                   syncLoopTarget,
                   aValue.IsResolve() ? NS_OK : aValue.RejectValue());
             });

  QM_TRY(MOZ_TO_RESULT(syncLoop.Run()),
         [&aError](const nsresult rv) { aError.Throw(rv); });
}

uint64_t FileSystemSyncAccessHandle::GetSize(ErrorResult& aError) {
  if (!IsOpen()) {
    aError.ThrowInvalidStateError("SyncAccessHandle is closed");
    return 0;
  }

  MOZ_ASSERT(mWorkerRef);

  AutoSyncLoopHolder syncLoop(mWorkerRef->Private(), Canceling);

  nsCOMPtr<nsISerialEventTarget> syncLoopTarget =
      syncLoop.GetSerialEventTarget();
  QM_TRY(MOZ_TO_RESULT(syncLoopTarget), [&aError](nsresult) {
    aError.ThrowInvalidStateError("Worker is shutting down");
    return 0;
  });

  // XXX Could we somehow pass the size to `StopSyncLoop` and then get it via
  // `QM_TRY_INSPECT(const auto& size, syncLoop.Run)` ?
  // Could we use Result<UniquePtr<...>, nsresult> for that ?
  int64_t size;

  InvokeAsync(mIOTaskQueue, __func__,
              [selfHolder = fs::TargetPtrHolder(this)]() {
                QM_TRY(MOZ_TO_RESULT(selfHolder->EnsureStream()),
                       CreateAndRejectSizePromise);

                nsCOMPtr<nsIFileMetadata> fileMetadata =
                    do_QueryInterface(selfHolder->mStream);
                MOZ_ASSERT(fileMetadata);

                QM_TRY_INSPECT(
                    const auto& size,
                    MOZ_TO_RESULT_INVOKE_MEMBER(fileMetadata, GetSize),
                    CreateAndRejectSizePromise);

                LOG(("%p: GetSize %" PRIu64, selfHolder->mStream.get(), size));

                return SizePromise::CreateAndResolve(size, __func__);
              })
      ->Then(syncLoopTarget, __func__,
             [this, &syncLoopTarget,
              &size](const Int64Promise::ResolveOrRejectValue& aValue) {
               MOZ_ASSERT(mWorkerRef);

               mWorkerRef->Private()->AssertIsOnWorkerThread();

               if (aValue.IsResolve()) {
                 size = aValue.ResolveValue();

                 mWorkerRef->Private()->StopSyncLoop(syncLoopTarget, NS_OK);
               } else {
                 mWorkerRef->Private()->StopSyncLoop(syncLoopTarget,
                                                     aValue.RejectValue());
               }
             });

  QM_TRY(MOZ_TO_RESULT(syncLoop.Run()), [&aError](const nsresult rv) {
    aError.Throw(rv);
    return 0;
  });

  return size;
}

void FileSystemSyncAccessHandle::Flush(ErrorResult& aError) {
  if (!IsOpen()) {
    aError.ThrowInvalidStateError("SyncAccessHandle is closed");
    return;
  }

  MOZ_ASSERT(mWorkerRef);

  AutoSyncLoopHolder syncLoop(mWorkerRef->Private(), Canceling);

  nsCOMPtr<nsISerialEventTarget> syncLoopTarget =
      syncLoop.GetSerialEventTarget();
  QM_TRY(MOZ_TO_RESULT(syncLoopTarget), [&aError](nsresult) {
    aError.ThrowInvalidStateError("Worker is shutting down");
  });

  InvokeAsync(mIOTaskQueue, __func__,
              [selfHolder = fs::TargetPtrHolder(this)]() {
                QM_TRY(MOZ_TO_RESULT(selfHolder->EnsureStream()),
                       CreateAndRejectBoolPromise);

                LOG(("%p: Flush", selfHolder->mStream.get()));

                QM_TRY(
                    MOZ_TO_RESULT(selfHolder->mStream->OutputStream()->Flush()),
                    CreateAndRejectBoolPromise);

                return BoolPromise::CreateAndResolve(true, __func__);
              })
      ->Then(syncLoopTarget, __func__,
             [this, &syncLoopTarget](
                 const BoolPromise::ResolveOrRejectValue& aValue) {
               MOZ_ASSERT(mWorkerRef);

               mWorkerRef->Private()->AssertIsOnWorkerThread();

               mWorkerRef->Private()->StopSyncLoop(
                   syncLoopTarget,
                   aValue.IsResolve() ? NS_OK : aValue.RejectValue());
             });

  QM_TRY(MOZ_TO_RESULT(syncLoop.Run()),
         [&aError](const nsresult rv) { aError.Throw(rv); });
}

void FileSystemSyncAccessHandle::Close() {
  if (!(IsOpen() || IsClosing())) {
    return;
  }

  MOZ_ASSERT(mWorkerRef);

  // Normally mWorkerRef can be used directly for stopping the sync loop, but
  // the async close is special because mWorkerRef is cleared as part of the
  // operation. That's why we need to use this extra strong ref to the
  // `StrongWorkerRef`.
  RefPtr<StrongWorkerRef> workerRef = mWorkerRef;

  AutoSyncLoopHolder syncLoop(workerRef->Private(), Killing);

  nsCOMPtr<nsISerialEventTarget> syncLoopTarget =
      syncLoop.GetSerialEventTarget();
  MOZ_ASSERT(syncLoopTarget);

  InvokeAsync(syncLoopTarget, __func__, [self = RefPtr(this)]() {
    if (self->IsOpen()) {
      return self->BeginClose();
    }
    return self->OnClose();
  })->Then(syncLoopTarget, __func__, [&workerRef, &syncLoopTarget]() {
    MOZ_ASSERT(workerRef);

    workerRef->Private()->AssertIsOnWorkerThread();

    workerRef->Private()->StopSyncLoop(syncLoopTarget, NS_OK);
  });

  MOZ_ALWAYS_SUCCEEDS(syncLoop.Run());
}

uint64_t FileSystemSyncAccessHandle::ReadOrWrite(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer,
    const FileSystemReadWriteOptions& aOptions, const bool aRead,
    ErrorResult& aRv) {
  if (!IsOpen()) {
    aRv.ThrowInvalidStateError("SyncAccessHandle is closed");
    return 0;
  }

  MOZ_ASSERT(mWorkerRef);

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

  AutoSyncLoopHolder syncLoop(mWorkerRef->Private(), Canceling);

  nsCOMPtr<nsISerialEventTarget> syncLoopTarget =
      syncLoop.GetSerialEventTarget();
  QM_TRY(MOZ_TO_RESULT(syncLoopTarget), [&aRv](nsresult) {
    aRv.ThrowInvalidStateError("Worker is shutting down");
    return 0;
  });

  uint64_t totalCount = 0;

  ProcessTypedArraysFixed(aBuffer, [&](const Span<uint8_t> aData) {
    InvokeAsync(
        mIOTaskQueue, __func__,
        [selfHolder = fs::TargetPtrHolder(this), aData,
         use_offset = aOptions.mAt.WasPassed(), offset, aRead, syncLoopTarget,
         &totalCount]() {
          QM_TRY(MOZ_TO_RESULT(selfHolder->EnsureStream()),
                 CreateAndRejectBoolPromise);
          if (use_offset) {
            LOG_VERBOSE(("%p: Seeking to %" PRIu64, selfHolder->mStream.get(),
                         offset.value()));

            QM_TRY(MOZ_TO_RESULT(selfHolder->mStream->Seek(
                       nsISeekableStream::NS_SEEK_SET, offset.value())),
                   CreateAndRejectBoolPromise);
          }

          nsCOMPtr<nsIInputStream> inputStream;
          nsCOMPtr<nsIOutputStream> outputStream;

          if (aRead) {
            LOG_VERBOSE(("%p: Reading %zu bytes", selfHolder->mStream.get(),
                         aData.Length()));

            inputStream = selfHolder->mStream->InputStream();
            outputStream =
                FixedBufferOutputStream::Create(AsWritableChars(aData));
          } else {
            LOG_VERBOSE(("%p: Writing %zu bytes", selfHolder->mStream.get(),
                         aData.Length()));

            QM_TRY(MOZ_TO_RESULT(NS_NewByteInputStream(
                       getter_AddRefs(inputStream), AsChars(aData),
                       NS_ASSIGNMENT_DEPEND)),
                   CreateAndRejectBoolPromise);

            outputStream = selfHolder->mStream->OutputStream();
          }

          auto promiseHolder = MakeUnique<MozPromiseHolder<BoolPromise>>();
          RefPtr<BoolPromise> promise = promiseHolder->Ensure(__func__);

          QM_TRY(MOZ_TO_RESULT(fs::AsyncCopy(
                     inputStream, outputStream, GetCurrentSerialEventTarget(),
                     aRead ? NS_ASYNCCOPY_VIA_WRITESEGMENTS
                           : NS_ASYNCCOPY_VIA_READSEGMENTS,
                     /* aCloseSource */ !aRead, /* aCloseSink */ aRead,
                     [&totalCount](uint32_t count) { totalCount += count; },
                     [promiseHolder = std::move(promiseHolder)](nsresult rv) {
                       promiseHolder->ResolveIfExists(true, __func__);
                     })),
                 CreateAndRejectBoolPromise);

          return promise;
        })
        ->Then(syncLoopTarget, __func__,
               [this, &syncLoopTarget](
                   const BoolPromise::ResolveOrRejectValue& aValue) {
                 MOZ_ASSERT(mWorkerRef);

                 mWorkerRef->Private()->AssertIsOnWorkerThread();

                 mWorkerRef->Private()->StopSyncLoop(syncLoopTarget, NS_OK);
               });

    MOZ_ALWAYS_SUCCEEDS(syncLoop.Run());
  });

  return totalCount;
}

nsresult FileSystemSyncAccessHandle::EnsureStream() {
  if (!mStream) {
    QM_TRY_UNWRAP(mStream, DeserializeRandomAccessStream(mStreamParams),
                  NS_ERROR_FAILURE);

    mozilla::ipc::RandomAccessStreamParams streamParams(
        std::move(mStreamParams));
  }

  return NS_OK;
}

}  // namespace mozilla::dom
