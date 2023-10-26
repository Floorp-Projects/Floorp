/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemWritableFileStream.h"

#include "fs/FileSystemAsyncCopy.h"
#include "fs/FileSystemShutdownBlocker.h"
#include "fs/FileSystemThreadSafeStreamOwner.h"
#include "mozilla/Buffer.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/MozPromise.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/FileSystemWritableFileStreamBinding.h"
#include "mozilla/dom/FileSystemWritableFileStreamChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WritableStreamDefaultController.h"
#include "mozilla/dom/fs/TargetPtrHolder.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/RandomAccessStreamUtils.h"
#include "nsAsyncStreamCopier.h"
#include "nsIInputStream.h"
#include "nsIRequestObserver.h"
#include "nsISupportsImpl.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

namespace mozilla::dom {

namespace {

constexpr bool IsFileNotFoundError(const nsresult aRv) {
  return NS_ERROR_DOM_FILE_NOT_FOUND_ERR == aRv ||
         NS_ERROR_FILE_NOT_FOUND == aRv;
}

class WritableFileStreamUnderlyingSinkAlgorithms final
    : public UnderlyingSinkAlgorithmsWrapper {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      WritableFileStreamUnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)

  explicit WritableFileStreamUnderlyingSinkAlgorithms(
      FileSystemWritableFileStream& aStream)
      : mStream(&aStream) {}

  already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) override;

  already_AddRefed<Promise> CloseCallbackImpl(JSContext* aCx,
                                              ErrorResult& aRv) override;

  already_AddRefed<Promise> AbortCallbackImpl(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override;

  void ReleaseObjects() override;

 private:
  ~WritableFileStreamUnderlyingSinkAlgorithms() = default;

  RefPtr<FileSystemWritableFileStream> mStream;
};

}  // namespace

class FileSystemWritableFileStream::Command {
 public:
  explicit Command(RefPtr<FileSystemWritableFileStream> aWritableFileStream)
      : mWritableFileStream(std::move(aWritableFileStream)) {
    MOZ_ASSERT(mWritableFileStream);
  }

  NS_INLINE_DECL_REFCOUNTING(FileSystemWritableFileStream::Command)

 private:
  ~Command() { mWritableFileStream->NoteFinishedCommand(); }

  RefPtr<FileSystemWritableFileStream> mWritableFileStream;
};

class FileSystemWritableFileStream::CloseHandler {
  enum struct State : uint8_t { Initial = 0, Open, Closing, Closed };

 public:
  CloseHandler()
      : mShutdownBlocker(fs::FileSystemShutdownBlocker::CreateForWritable()),
        mClosePromiseHolder(),
        mState(State::Initial) {}

  NS_INLINE_DECL_REFCOUNTING(FileSystemWritableFileStream::CloseHandler)

  /**
   * @brief Are we not yet closing?
   */
  bool IsOpen() const { return State::Open == mState; }

  /**
   * @brief Are we already fully closed?
   */
  bool IsClosed() const { return State::Closed == mState; }

  /**
   * @brief Transition from open to closing state
   *
   * @return true if the state was open and became closing after the call
   * @return false in all the other cases the previous state is preserved
   */
  bool TestAndSetClosing() {
    const bool isOpen = State::Open == mState;

    if (isOpen) {
      mState = State::Closing;
    }

    return isOpen;
  }

  RefPtr<BoolPromise> GetClosePromise() const {
    MOZ_ASSERT(State::Open != mState,
               "Please call TestAndSetClosing before GetClosePromise");

    if (State::Closing == mState) {
      return mClosePromiseHolder.Ensure(__func__);
    }

    // Instant resolve for initial state due to early shutdown or closed state
    return BoolPromise::CreateAndResolve(true, __func__);
  }

  /**
   * @brief Transition from initial to open state. In initial state
   *
   */
  void Open() {
    MOZ_ASSERT(State::Initial == mState);
    mShutdownBlocker->Block();

    mState = State::Open;
  }

  /**
   * @brief Transition to closed state and resolve all pending promises.
   *
   */
  void Close() {
    mShutdownBlocker->Unblock();
    mState = State::Closed;
    mClosePromiseHolder.ResolveIfExists(true, __func__);
  }

 protected:
  virtual ~CloseHandler() = default;

 private:
  RefPtr<fs::FileSystemShutdownBlocker> mShutdownBlocker;

  mutable MozPromiseHolder<BoolPromise> mClosePromiseHolder;

  State mState;
};

FileSystemWritableFileStream::FileSystemWritableFileStream(
    const nsCOMPtr<nsIGlobalObject>& aGlobal,
    RefPtr<FileSystemManager>& aManager,
    mozilla::ipc::RandomAccessStreamParams&& aStreamParams,
    RefPtr<FileSystemWritableFileStreamChild> aActor,
    already_AddRefed<TaskQueue> aTaskQueue,
    const fs::FileSystemEntryMetadata& aMetadata)
    : WritableStream(aGlobal, HoldDropJSObjectsCaller::Explicit),
      mManager(aManager),
      mActor(std::move(aActor)),
      mTaskQueue(aTaskQueue),
      mWorkerRef(),
      mStreamParams(std::move(aStreamParams)),
      mMetadata(std::move(aMetadata)),
      mCloseHandler(MakeAndAddRef<CloseHandler>()),
      mCommandActive(false) {
  LOG(("Created WritableFileStream %p", this));

  // Connect with the actor directly in the constructor. This way the actor
  // can call `FileSystemWritableFileStream::ClearActor` when we call
  // `PFileSystemWritableFileStreamChild::Send__delete__` even when
  // FileSystemWritableFileStream::Create fails, in which case the not yet
  // fully constructed FileSystemWritableFileStream is being destroyed.
  mActor->SetStream(this);

  mozilla::HoldJSObjects(this);
}

FileSystemWritableFileStream::~FileSystemWritableFileStream() {
  MOZ_ASSERT(!mCommandActive);
  MOZ_ASSERT(IsClosed());

  mozilla::DropJSObjects(this);
}

// https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
// * This is fallible because of OOM handling of JSAPI. See bug 1762233.
// XXX(krosylight): _BOUNDARY because SetUpNative here can't run script because
// StartCallback here is no-op. Can we let the static check automatically detect
// this situation?
/* static */
MOZ_CAN_RUN_SCRIPT_BOUNDARY
Result<RefPtr<FileSystemWritableFileStream>, nsresult>
FileSystemWritableFileStream::Create(
    const nsCOMPtr<nsIGlobalObject>& aGlobal,
    RefPtr<FileSystemManager>& aManager,
    mozilla::ipc::RandomAccessStreamParams&& aStreamParams,
    RefPtr<FileSystemWritableFileStreamChild> aActor,
    const fs::FileSystemEntryMetadata& aMetadata) {
  MOZ_ASSERT(aGlobal);

  QM_TRY_UNWRAP(auto streamTransportService,
                MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                        MOZ_SELECT_OVERLOAD(do_GetService),
                                        NS_STREAMTRANSPORTSERVICE_CONTRACTID));

  RefPtr<TaskQueue> taskQueue =
      TaskQueue::Create(streamTransportService.forget(), "WritableStreamQueue");
  MOZ_ASSERT(taskQueue);

  AutoJSAPI jsapi;
  if (!jsapi.Init(aGlobal)) {
    return Err(NS_ERROR_FAILURE);
  }
  JSContext* cx = jsapi.cx();

  // Step 1. Let stream be a new FileSystemWritableFileStream in realm.
  // Step 2. Set stream.[[file]] to file. (Covered by the constructor)
  RefPtr<FileSystemWritableFileStream> stream =
      new FileSystemWritableFileStream(
          aGlobal, aManager, std::move(aStreamParams), std::move(aActor),
          taskQueue.forget(), aMetadata);

  auto autoClose = MakeScopeExit([stream] {
    stream->mCloseHandler->Close();
    stream->mActor->SendClose();
  });

  QM_TRY_UNWRAP(
      RefPtr<StrongWorkerRef> workerRef,
      ([stream]() -> Result<RefPtr<StrongWorkerRef>, nsresult> {
        WorkerPrivate* const workerPrivate = GetCurrentThreadWorkerPrivate();
        if (!workerPrivate) {
          return RefPtr<StrongWorkerRef>();
        }

        RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
            workerPrivate, "FileSystemWritableFileStream::Create", [stream]() {
              if (stream->IsOpen()) {
                // We don't need the promise, we just
                // begin the closing process.
                Unused << stream->BeginClose();
              }
            });
        QM_TRY(MOZ_TO_RESULT(workerRef));

        return workerRef;
      }()));

  // Step 3 - 5
  auto algorithms =
      MakeRefPtr<WritableFileStreamUnderlyingSinkAlgorithms>(*stream);

  // Step 8: Set up stream with writeAlgorithm set to writeAlgorithm,
  // closeAlgorithm set to closeAlgorithm, abortAlgorithm set to
  // abortAlgorithm, highWaterMark set to highWaterMark, and
  // sizeAlgorithm set to sizeAlgorithm.
  IgnoredErrorResult rv;
  stream->SetUpNative(cx, *algorithms,
                      // Step 6. Let highWaterMark be 1.
                      Some(1),
                      // Step 7. Let sizeAlgorithm be an algorithm
                      // that returns 1. (nullptr returns 1, See
                      // WritableStream::Constructor for details)
                      nullptr, rv);
  if (rv.Failed()) {
    return Err(rv.StealNSResult());
  }

  autoClose.release();

  stream->mWorkerRef = std::move(workerRef);
  stream->mCloseHandler->Open();

  // Step 9: Return stream.
  return stream;
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(FileSystemWritableFileStream,
                                               WritableStream)

NS_IMPL_CYCLE_COLLECTION_CLASS(FileSystemWritableFileStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FileSystemWritableFileStream,
                                                WritableStream)
  // Per the comment for the FileSystemManager class, don't unlink mManager!
  if (tmp->IsOpen()) {
    Unused << tmp->BeginClose();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FileSystemWritableFileStream,
                                                  WritableStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void FileSystemWritableFileStream::LastRelease() {
  // We can't call `FileSystemWritableFileStream::Close` here because it may
  // need to keep FileSystemWritableFileStream object alive which isn't possible
  // when the object is about to be deleted. There are other mechanisms which
  // ensure that the object is correctly closed before destruction. For example
  // the object unlinking and the worker shutdown (we get notified about it via
  // the callback passed to `StrongWorkerRef`) are used to close the object if
  // it hasn't been closed yet.

  if (mActor) {
    PFileSystemWritableFileStreamChild::Send__delete__(mActor);
    MOZ_ASSERT(!mActor);
  }
}

RefPtr<FileSystemWritableFileStream::Command>
FileSystemWritableFileStream::CreateCommand() {
  MOZ_ASSERT(!mCommandActive);

  mCommandActive = true;

  return MakeRefPtr<Command>(this);
}

bool FileSystemWritableFileStream::IsCommandActive() const {
  return mCommandActive;
}

void FileSystemWritableFileStream::ClearActor() {
  MOZ_ASSERT(mActor);

  mActor = nullptr;
}

bool FileSystemWritableFileStream::IsOpen() const {
  return mCloseHandler->IsOpen();
}

bool FileSystemWritableFileStream::IsClosed() const {
  return mCloseHandler->IsClosed();
}

RefPtr<BoolPromise> FileSystemWritableFileStream::BeginClose() {
  using ClosePromise = PFileSystemWritableFileStreamChild::ClosePromise;
  if (mCloseHandler->TestAndSetClosing()) {
    Finish()
        ->Then(mTaskQueue, __func__,
               [selfHolder = fs::TargetPtrHolder(this)]() mutable {
                 if (selfHolder->mStreamOwner) {
                   selfHolder->mStreamOwner->Close();
                 } else {
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
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [self = RefPtr(this)](const BoolPromise::ResolveOrRejectValue&) {
                 return self->mTaskQueue->BeginShutdown();
               })
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [self = RefPtr(this)](
                   const ShutdownPromise::ResolveOrRejectValue& /* aValue */) {
                 if (!self->mActor) {
                   return ClosePromise::CreateAndResolve(void_t(), __func__);
                 }

                 return self->mActor->SendClose();
               })
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [self = RefPtr(this)](
                   const ClosePromise::ResolveOrRejectValue& aValue) {
                 self->mWorkerRef = nullptr;
                 self->mCloseHandler->Close();

                 QM_TRY(OkIf(aValue.IsResolve()), QM_VOID);
               });
  }

  return mCloseHandler->GetClosePromise();
}

already_AddRefed<Promise> FileSystemWritableFileStream::Write(
    JSContext* aCx, JS::Handle<JS::Value> aChunk, ErrorResult& aError) {
  // https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
  // Step 3. Let writeAlgorithm be an algorithm which takes a chunk argument
  // and returns the result of running the write a chunk algorithm with stream
  // and chunk.

  // https://fs.spec.whatwg.org/#write-a-chunk
  // Step 1. Let input be the result of converting chunk to a
  // FileSystemWriteChunkType.

  aError.MightThrowJSException();

  ArrayBufferViewOrArrayBufferOrBlobOrUTF8StringOrWriteParams data;
  if (!data.Init(aCx, aChunk)) {
    aError.StealExceptionFromJSContext(aCx);
    return nullptr;
  }

  // Step 2. Let p be a new promise.
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (!IsOpen()) {
    promise->MaybeRejectWithTypeError("WritableFileStream closed");
    return promise.forget();
  }

  // Step 3.3. Let command be input.type if input is a WriteParams, ...
  if (data.IsWriteParams()) {
    const WriteParams& params = data.GetAsWriteParams();
    switch (params.mType) {
      // Step 3.4. If command is "write":
      case WriteCommandType::Write: {
        if (!params.mData.WasPassed()) {
          promise->MaybeRejectWithSyntaxError("write() requires data");
          return promise.forget();
        }

        // Step 3.4.2. If data is undefined, reject p with a TypeError and
        // abort.
        if (params.mData.Value().IsNull()) {
          promise->MaybeRejectWithTypeError("write() of null data");
          return promise.forget();
        }

        Maybe<uint64_t> position;

        if (params.mPosition.WasPassed()) {
          if (params.mPosition.Value().IsNull()) {
            promise->MaybeRejectWithTypeError("write() with null position");
            return promise.forget();
          }

          position = Some(params.mPosition.Value().Value());
        }

        Write(params.mData.Value().Value(), position, promise);
        return promise.forget();
      }

      // Step 3.5. Otherwise, if command is "seek":
      case WriteCommandType::Seek:
        if (!params.mPosition.WasPassed()) {
          promise->MaybeRejectWithSyntaxError("seek() requires a position");
          return promise.forget();
        }

        // Step 3.5.1. If chunk.position is undefined, reject p with a
        // TypeError and abort.
        if (params.mPosition.Value().IsNull()) {
          promise->MaybeRejectWithTypeError("seek() with null position");
          return promise.forget();
        }

        Seek(params.mPosition.Value().Value(), promise);
        return promise.forget();

      // Step 3.6. Otherwise, if command is "truncate":
      case WriteCommandType::Truncate:
        if (!params.mSize.WasPassed()) {
          promise->MaybeRejectWithSyntaxError("truncate() requires a size");
          return promise.forget();
        }

        // Step 3.6.1. If chunk.size is undefined, reject p with a TypeError
        // and abort.
        if (params.mSize.Value().IsNull()) {
          promise->MaybeRejectWithTypeError("truncate() with null size");
          return promise.forget();
        }

        Truncate(params.mSize.Value().Value(), promise);
        return promise.forget();

      default:
        MOZ_CRASH("Bad WriteParams value!");
    }
  }

  // Step 3.3. ... and "write" otherwise.
  // Step 3.4. If command is "write":
  Write(data, Nothing(), promise);
  return promise.forget();
}

// WebIDL Boilerplate

JSObject* FileSystemWritableFileStream::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FileSystemWritableFileStream_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

already_AddRefed<Promise> FileSystemWritableFileStream::Write(
    const ArrayBufferViewOrArrayBufferOrBlobOrUTF8StringOrWriteParams& aData,
    ErrorResult& aError) {
  // https://fs.spec.whatwg.org/#dom-filesystemwritablefilestream-write
  // Step 1. Let writer be the result of getting a writer for this.
  RefPtr<WritableStreamDefaultWriter> writer = GetWriter(aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Step 2. Let result be the result of writing a chunk to writer given data.
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    aError.ThrowUnknownError("Internal error");
    return nullptr;
  }

  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));

  JS::Rooted<JS::Value> val(cx);
  if (!aData.ToJSVal(cx, global, &val)) {
    aError.ThrowUnknownError("Internal error");
    return nullptr;
  }

  RefPtr<Promise> promise = writer->Write(cx, val, aError);

  // Step 3. Release writer.
  writer->ReleaseLock(cx);

  // Step 4. Return result.
  return promise.forget();
}

already_AddRefed<Promise> FileSystemWritableFileStream::Seek(
    uint64_t aPosition, ErrorResult& aError) {
  // https://fs.spec.whatwg.org/#dom-filesystemwritablefilestream-seek
  // Step 1. Let writer be the result of getting a writer for this.
  RefPtr<WritableStreamDefaultWriter> writer = GetWriter(aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Step 2. Let result be the result of writing a chunk to writer given
  // «[ "type" → "seek", "position" → position ]».
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    aError.ThrowUnknownError("Internal error");
    return nullptr;
  }

  JSContext* cx = jsapi.cx();

  RootedDictionary<WriteParams> writeParams(cx);
  writeParams.mType = WriteCommandType::Seek;
  writeParams.mPosition.Construct(aPosition);

  JS::Rooted<JS::Value> val(cx);
  if (!ToJSValue(cx, writeParams, &val)) {
    aError.ThrowUnknownError("Internal error");
    return nullptr;
  }

  RefPtr<Promise> promise = writer->Write(cx, val, aError);

  // Step 3. Release writer.
  writer->ReleaseLock(cx);

  // Step 4. Return result.
  return promise.forget();
}

already_AddRefed<Promise> FileSystemWritableFileStream::Truncate(
    uint64_t aSize, ErrorResult& aError) {
  // https://fs.spec.whatwg.org/#dom-filesystemwritablefilestream-truncate
  // Step 1. Let writer be the result of getting a writer for this.
  RefPtr<WritableStreamDefaultWriter> writer = GetWriter(aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Step 2. Let result be the result of writing a chunk to writer given
  // «[ "type" → "truncate", "size" → size ]».
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    aError.ThrowUnknownError("Internal error");
    return nullptr;
  }

  JSContext* cx = jsapi.cx();

  RootedDictionary<WriteParams> writeParams(cx);
  writeParams.mType = WriteCommandType::Truncate;
  writeParams.mSize.Construct(aSize);

  JS::Rooted<JS::Value> val(cx);
  if (!ToJSValue(cx, writeParams, &val)) {
    aError.ThrowUnknownError("Internal error");
    return nullptr;
  }

  RefPtr<Promise> promise = writer->Write(cx, val, aError);

  // Step 3. Release writer.
  writer->ReleaseLock(cx);

  // Step 4. Return result.
  return promise.forget();
}

template <typename T>
void FileSystemWritableFileStream::Write(const T& aData,
                                         const Maybe<uint64_t> aPosition,
                                         const RefPtr<Promise>& aPromise) {
  MOZ_ASSERT(IsOpen());

  auto rejectAndReturn = [&aPromise](const nsresult rv) {
    if (IsFileNotFoundError(rv)) {
      aPromise->MaybeRejectWithNotFoundError("File not found");
      return;
    }
    aPromise->MaybeReject(rv);
  };

  nsCOMPtr<nsIInputStream> inputStream;

  // https://fs.spec.whatwg.org/#write-a-chunk
  // Step 3.4.6 If data is a BufferSource, let dataBytes be a copy of data.
  if (aData.IsArrayBuffer() || aData.IsArrayBufferView()) {
    const auto dataSpan = [&aData]() -> mozilla::Span<uint8_t> {
      if (aData.IsArrayBuffer()) {
        const ArrayBuffer& buffer = aData.GetAsArrayBuffer();
        buffer.ComputeState();
        return Span{buffer.Data(), buffer.Length()};
      }

      const ArrayBufferView& buffer = aData.GetAsArrayBufferView();
      buffer.ComputeState();
      return Span{buffer.Data(), buffer.Length()};
    }();

    // Here we copy

    QM_TRY(MOZ_TO_RESULT(NS_NewByteInputStream(getter_AddRefs(inputStream),
                                               AsChars(dataSpan),
                                               NS_ASSIGNMENT_COPY)),
           rejectAndReturn);

    WriteImpl(std::move(inputStream), aPosition, aPromise);
    return;
  }

  // Step 3.4.7 Otherwise, if data is a Blob ...
  if (aData.IsBlob()) {
    Blob& blob = aData.GetAsBlob();

    ErrorResult error;
    blob.CreateInputStream(getter_AddRefs(inputStream), error);
    QM_TRY((MOZ_TO_RESULT(!error.Failed()).mapErr([&error](const nsresult rv) {
             return error.StealNSResult();
           })),
           rejectAndReturn);

    WriteImpl(std::move(inputStream), aPosition, aPromise);
    return;
  }

  // Step 3.4.8 Otherwise ...
  MOZ_ASSERT(aData.IsUTF8String());

  // Here we copy
  nsCString dataString;
  if (!dataString.Assign(aData.GetAsUTF8String(), mozilla::fallible)) {
    rejectAndReturn(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // Input stream takes ownership
  QM_TRY(MOZ_TO_RESULT(NS_NewCStringInputStream(getter_AddRefs(inputStream),
                                                std::move(dataString))),
         rejectAndReturn);

  WriteImpl(std::move(inputStream), aPosition, aPromise);
}

void FileSystemWritableFileStream::WriteImpl(
    nsCOMPtr<nsIInputStream> aInputStream, const Maybe<uint64_t> aPosition,
    const RefPtr<Promise>& aPromise) {
  auto command = CreateCommand();

  InvokeAsync(
      mTaskQueue, __func__,
      [selfHolder = fs::TargetPtrHolder(this),
       inputStream = std::move(aInputStream), aPosition]() {
        QM_TRY(MOZ_TO_RESULT(selfHolder->EnsureStream()),
               CreateAndRejectInt64Promise);

        if (aPosition.isSome()) {
          LOG(("%p: Seeking to %" PRIu64, selfHolder->mStreamOwner.get(),
               aPosition.value()));

          QM_TRY(
              MOZ_TO_RESULT(selfHolder->mStreamOwner->Seek(aPosition.value())),
              CreateAndRejectInt64Promise);
        }

        nsCOMPtr<nsIOutputStream> streamSink =
            selfHolder->mStreamOwner->OutputStream();

        auto written = std::make_shared<int64_t>(0);
        auto writingProgress = [written](uint32_t aDelta) {
          *written += static_cast<int64_t>(aDelta);
        };

        auto promiseHolder = MakeUnique<MozPromiseHolder<Int64Promise>>();
        RefPtr<Int64Promise> promise = promiseHolder->Ensure(__func__);

        auto writingCompletion =
            [written,
             promiseHolder = std::move(promiseHolder)](nsresult aStatus) {
              if (NS_SUCCEEDED(aStatus)) {
                promiseHolder->ResolveIfExists(*written, __func__);
                return;
              }

              promiseHolder->RejectIfExists(aStatus, __func__);
            };

        QM_TRY(MOZ_TO_RESULT(fs::AsyncCopy(
                   inputStream, streamSink, selfHolder->mTaskQueue,
                   nsAsyncCopyMode::NS_ASYNCCOPY_VIA_READSEGMENTS,
                   /* aCloseSource */ true, /* aCloseSink */ false,
                   std::move(writingProgress), std::move(writingCompletion))),
               CreateAndRejectInt64Promise);

        return promise;
      })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [command,
           aPromise](const Int64Promise::ResolveOrRejectValue& aValue) {
            if (aValue.IsResolve()) {
              aPromise->MaybeResolve(aValue.ResolveValue());
              return;
            }

            if (IsFileNotFoundError(aValue.RejectValue())) {
              aPromise->MaybeRejectWithNotFoundError("File not found");
            } else if (aValue.RejectValue() == NS_ERROR_FILE_NO_DEVICE_SPACE) {
              aPromise->MaybeRejectWithQuotaExceededError("Quota exceeded");
            } else {
              aPromise->MaybeReject(aValue.RejectValue());
            }

            aPromise->MaybeReject(aValue.RejectValue());
          });
}

void FileSystemWritableFileStream::Seek(uint64_t aPosition,
                                        const RefPtr<Promise>& aPromise) {
  MOZ_ASSERT(IsOpen());

  LOG_VERBOSE(("%p: Seeking to %" PRIu64, mStreamOwner.get(), aPosition));

  auto command = CreateCommand();

  InvokeAsync(mTaskQueue, __func__,
              [selfHolder = fs::TargetPtrHolder(this), aPosition]() mutable {
                QM_TRY(MOZ_TO_RESULT(selfHolder->EnsureStream()),
                       CreateAndRejectBoolPromise);

                QM_TRY(MOZ_TO_RESULT(selfHolder->mStreamOwner->Seek(aPosition)),
                       CreateAndRejectBoolPromise);

                return BoolPromise::CreateAndResolve(true, __func__);
              })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [command, aPromise](const BoolPromise::ResolveOrRejectValue& aValue) {
            if (aValue.IsReject()) {
              auto rv = aValue.RejectValue();
              if (IsFileNotFoundError(rv)) {
                aPromise->MaybeRejectWithNotFoundError("File not found");
                return;
              }
              aPromise->MaybeReject(rv);
              return;
            }
            MOZ_ASSERT(aValue.IsResolve());
            aPromise->MaybeResolveWithUndefined();
          });
}

void FileSystemWritableFileStream::Truncate(uint64_t aSize,
                                            const RefPtr<Promise>& aPromise) {
  MOZ_ASSERT(IsOpen());

  auto command = CreateCommand();

  InvokeAsync(mTaskQueue, __func__,
              [selfHolder = fs::TargetPtrHolder(this), aSize]() mutable {
                QM_TRY(MOZ_TO_RESULT(selfHolder->EnsureStream()),
                       CreateAndRejectBoolPromise);

                QM_TRY(MOZ_TO_RESULT(selfHolder->mStreamOwner->Truncate(aSize)),
                       CreateAndRejectBoolPromise);

                return BoolPromise::CreateAndResolve(true, __func__);
              })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [command, aPromise](const BoolPromise::ResolveOrRejectValue& aValue) {
            if (aValue.IsReject()) {
              aPromise->MaybeReject(aValue.RejectValue());
              return;
            }

            aPromise->MaybeResolveWithUndefined();
          });
}

nsresult FileSystemWritableFileStream::EnsureStream() {
  if (!mStreamOwner) {
    QM_TRY_UNWRAP(MovingNotNull<nsCOMPtr<nsIRandomAccessStream>> stream,
                  DeserializeRandomAccessStream(mStreamParams),
                  NS_ERROR_FAILURE);

    mozilla::ipc::RandomAccessStreamParams streamParams(
        std::move(mStreamParams));

    mStreamOwner = MakeRefPtr<fs::FileSystemThreadSafeStreamOwner>(
        this, std::move(stream));
  }

  return NS_OK;
}

void FileSystemWritableFileStream::NoteFinishedCommand() {
  MOZ_ASSERT(mCommandActive);

  mCommandActive = false;

  mFinishPromiseHolder.ResolveIfExists(true, __func__);
}

RefPtr<BoolPromise> FileSystemWritableFileStream::Finish() {
  if (!mCommandActive) {
    return BoolPromise::CreateAndResolve(true, __func__);
  }

  return mFinishPromiseHolder.Ensure(__func__);
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    WritableFileStreamUnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTION_INHERITED(WritableFileStreamUnderlyingSinkAlgorithms,
                                   UnderlyingSinkAlgorithmsBase, mStream)

// Step 3 of
// https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
already_AddRefed<Promise>
WritableFileStreamUnderlyingSinkAlgorithms::WriteCallback(
    JSContext* aCx, JS::Handle<JS::Value> aChunk,
    WritableStreamDefaultController& aController, ErrorResult& aRv) {
  return mStream->Write(aCx, aChunk, aRv);
}

// Step 4 of
// https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
already_AddRefed<Promise>
WritableFileStreamUnderlyingSinkAlgorithms::CloseCallbackImpl(
    JSContext* aCx, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mStream->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!mStream->IsOpen()) {
    promise->MaybeRejectWithTypeError("WritableFileStream closed");
    return promise.forget();
  }

  mStream->BeginClose()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise](const BoolPromise::ResolveOrRejectValue& aValue) {
        // Step 2.3. Return a promise resolved with undefined.
        if (aValue.IsResolve()) {
          promise->MaybeResolveWithUndefined();
          return;
        }
        promise->MaybeRejectWithAbortError(
            "Internal error closing file stream");
      });

  return promise.forget();
}

// Step 5 of
// https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
already_AddRefed<Promise>
WritableFileStreamUnderlyingSinkAlgorithms::AbortCallbackImpl(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& /* aReason */,
    ErrorResult& aRv) {
  // https://streams.spec.whatwg.org/#writablestream-set-up
  // Step 3. Let abortAlgorithmWrapper be an algorithm that runs these steps:
  // Step 3.3. Return a promise resolved with undefined.

  return CloseCallbackImpl(aCx, aRv);
}

void WritableFileStreamUnderlyingSinkAlgorithms::ReleaseObjects() {
  // XXX We shouldn't be calling close here. We should just release the lock.
  // However, calling close here is not a big issue for now because we don't
  // write to a temporary file which would atomically replace the real file
  // during close.
  if (mStream->IsOpen()) {
    Unused << mStream->BeginClose();
  }
}

}  // namespace mozilla::dom
