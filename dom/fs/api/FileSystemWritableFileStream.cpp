/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemWritableFileStream.h"

#include "mozilla/Buffer.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/FileSystemWritableFileStreamBinding.h"
#include "mozilla/dom/FileSystemWritableFileStreamChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WritableStreamDefaultController.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "private/pprio.h"

namespace mozilla::dom {

namespace {

class WritableFileStreamUnderlyingSinkAlgorithms final
    : public UnderlyingSinkAlgorithmsBase {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      WritableFileStreamUnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)

  explicit WritableFileStreamUnderlyingSinkAlgorithms(
      FileSystemWritableFileStream& aStream)
      : mStream(&aStream) {}

  // Streams algorithms
  void StartCallback(JSContext* aCx,
                     WritableStreamDefaultController& aController,
                     JS::MutableHandle<JS::Value> aRetVal,
                     ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CloseCallback(
      JSContext* aCx, ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override;

  void ReleaseObjects() override;

 private:
  ~WritableFileStreamUnderlyingSinkAlgorithms() = default;

  RefPtr<FileSystemWritableFileStream> mStream;
};

/**
 * TODO: Duplicated from netwerk/cache2/CacheFileIOManager.cpp
 * Please remove after bug 1286601 is fixed,
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1286601
 */
nsresult TruncFile(PRFileDesc* aFD, int64_t aEOF) {
#if defined(XP_UNIX)
  if (ftruncate(PR_FileDesc2NativeHandle(aFD), aEOF) != 0) {
    NS_ERROR("ftruncate failed");
    return NS_ERROR_FAILURE;
  }
#elif defined(XP_WIN)
  const int64_t currentOffset = PR_Seek64(aFD, 0, PR_SEEK_CUR);
  if (currentOffset == -1) {
    return NS_ERROR_FAILURE;
  }

  int64_t cnt = PR_Seek64(aFD, aEOF, PR_SEEK_SET);
  if (cnt == -1) {
    return NS_ERROR_FAILURE;
  }

  if (!SetEndOfFile((HANDLE)PR_FileDesc2NativeHandle(aFD))) {
    NS_ERROR("SetEndOfFile failed");
    return NS_ERROR_FAILURE;
  }

  if (PR_Seek64(aFD, currentOffset, PR_SEEK_SET) == -1) {
    NS_ERROR("Restoring seek offset failed");
    return NS_ERROR_FAILURE;
  }

#else
  MOZ_ASSERT(false, "Not implemented!");
  return NS_ERROR_NOT_IMPLEMENTED;
#endif

  return NS_OK;
}

}  // namespace

FileSystemWritableFileStream::FileSystemWritableFileStream(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    RefPtr<FileSystemWritableFileStreamChild> aActor,
    const ::mozilla::ipc::FileDescriptor& aFileDescriptor,
    const fs::FileSystemEntryMetadata& aMetadata)
    : WritableStream(aGlobal, HoldDropJSObjectsCaller::Explicit),
      mManager(aManager),
      mActor(std::move(aActor)),
      mFileDesc(nullptr),
      mMetadata(aMetadata),
      mClosed(false) {
  auto rawFD = aFileDescriptor.ClonePlatformHandle();
  mFileDesc = PR_ImportFile(PROsfd(rawFD.release()));

  LOG(("Created WritableFileStream %p for fd %p", this, mFileDesc));

  // Connect with the actor directly in the constructor. This way the actor
  // can call `FileSystemWritableFileStream::ClearActor` when we call
  // `PFileSystemWritableFileStreamChild::Send__delete__` even when
  // FileSystemWritableFileStream::Create fails, in which case the not yet
  // fully constructed FileSystemWritableFileStream is being destroyed.
  mActor->SetStream(this);

  mozilla::HoldJSObjects(this);
}

FileSystemWritableFileStream::~FileSystemWritableFileStream() {
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(mClosed);

  mozilla::DropJSObjects(this);
}

// https://streams.spec.whatwg.org/#writablestream-set-up
// * This is fallible because of OOM handling of JSAPI. See bug 1762233.
// * Consider extracting this as UnderlyingSinkAlgorithmsWrapper if more classes
//   start subclassing WritableStream.
//   For now this skips step 2 - 4 as they are not required here.
// XXX(krosylight): _BOUNDARY because SetUpWritableStreamDefaultController here
// can't run script because StartCallback here is no-op. Can we let the static
// check automatically detect this situation?
/* static */
MOZ_CAN_RUN_SCRIPT_BOUNDARY already_AddRefed<FileSystemWritableFileStream>
FileSystemWritableFileStream::Create(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    RefPtr<FileSystemWritableFileStreamChild> aActor,
    const ::mozilla::ipc::FileDescriptor& aFileDescriptor,
    const fs::FileSystemEntryMetadata& aMetadata) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(aGlobal)) {
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  // Step 5. Perform ! InitializeWritableStream(stream).
  // (Done by the constructor)
  RefPtr<FileSystemWritableFileStream> stream =
      new FileSystemWritableFileStream(aGlobal, aManager, std::move(aActor),
                                       aFileDescriptor, aMetadata);

  // Step 1 - 3
  auto algorithms =
      MakeRefPtr<WritableFileStreamUnderlyingSinkAlgorithms>(*stream);

  // Step 6. Let controller be a new WritableStreamDefaultController.
  auto controller =
      MakeRefPtr<WritableStreamDefaultController>(aGlobal, *stream);

  // Step 7. Perform ! SetUpWritableStreamDefaultController(stream, controller,
  // startAlgorithm, writeAlgorithm, closeAlgorithmWrapper,
  // abortAlgorithmWrapper, highWaterMark, sizeAlgorithm).
  IgnoredErrorResult rv;
  SetUpWritableStreamDefaultController(
      cx, stream, controller, algorithms,
      // https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
      // Step 6. Let highWaterMark be 1.
      1,
      // Step 7. Let sizeAlgorithm be an algorithm that returns 1.
      // (nullptr returns 1, See WritableStream::Constructor for details)
      nullptr, rv);
  if (rv.Failed()) {
    return nullptr;
  }
  return stream.forget();
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(FileSystemWritableFileStream,
                                               WritableStream)

NS_IMPL_CYCLE_COLLECTION_CLASS(FileSystemWritableFileStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FileSystemWritableFileStream,
                                                WritableStream)
  // Per the comment for the FileSystemManager class, don't unlink mManager!
  tmp->Close();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FileSystemWritableFileStream,
                                                  WritableStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void FileSystemWritableFileStream::LastRelease() {
  Close();

  if (mActor) {
    PFileSystemWritableFileStreamChild::Send__delete__(mActor);
    MOZ_ASSERT(!mActor);
  }
}

void FileSystemWritableFileStream::ClearActor() {
  MOZ_ASSERT(mActor);

  mActor = nullptr;
}

void FileSystemWritableFileStream::Close() {
  // https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
  // Step 4. Let closeAlgorithm be these steps:

  if (mClosed) {
    return;
  }

  LOG(("%p: Closing", mFileDesc));

  mClosed = true;

  PR_Close(mFileDesc);
  mFileDesc = nullptr;

  if (mActor) {
    mActor->SendClose();
  }
}

already_AddRefed<Promise> FileSystemWritableFileStream::Write(
    JSContext* aCx, JS::Handle<JS::Value> aChunk, ErrorResult& aError) {
  MOZ_ASSERT(!mClosed);

  // https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
  // Step 3. Let writeAlgorithm be an algorithm which takes a chunk argument
  // and returns the result of running the write a chunk algorithm with stream
  // and chunk.

  // https://fs.spec.whatwg.org/#write-a-chunk
  // Step 1. Let input be the result of converting chunk to a
  // FileSystemWriteChunkType.

  aError.MightThrowJSException();

  ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams data;
  if (!data.Init(aCx, aChunk)) {
    aError.StealExceptionFromJSContext(aCx);
    return nullptr;
  }

  // Step 2. Let p be a new promise.
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
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

        // Step 3.5.1. If chunk.position is undefined, reject p with a TypeError
        // and abort.
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
    const ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams& aData,
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
  {
    IgnoredErrorResult error;
    writer->ReleaseLock(cx, error);
  }

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
  {
    IgnoredErrorResult error;
    writer->ReleaseLock(cx, error);
  }

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
  {
    IgnoredErrorResult error;
    writer->ReleaseLock(cx, error);
  }

  // Step 4. Return result.
  return promise.forget();
}

template <typename T>
void FileSystemWritableFileStream::Write(const T& aData,
                                         const Maybe<uint64_t> aPosition,
                                         RefPtr<Promise> aPromise) {
  MOZ_ASSERT(!mClosed);

  auto rejectAndReturn = [&aPromise](const nsresult rv) {
    if (rv == NS_ERROR_FILE_NOT_FOUND) {
      aPromise->MaybeRejectWithNotFoundError("File not found");
    } else {
      aPromise->MaybeReject(rv);
    }
  };

  // https://fs.spec.whatwg.org/#write-a-chunk
  // Step 3.4.6 If data is a BufferSource, let dataBytes be a copy of data.
  if (aData.IsArrayBuffer() || aData.IsArrayBufferView()) {
    const auto dataSpan = [&aData]() {
      if (aData.IsArrayBuffer()) {
        const ArrayBuffer& buffer = aData.GetAsArrayBuffer();
        buffer.ComputeState();
        return Span{buffer.Data(), buffer.Length()};
      }
      MOZ_ASSERT(aData.IsArrayBufferView());
      const ArrayBufferView& buffer = aData.GetAsArrayBufferView();
      buffer.ComputeState();
      return Span{buffer.Data(), buffer.Length()};
    }();

    auto maybeBuffer = Buffer<char>::CopyFrom(AsChars(dataSpan));
    QM_TRY(MOZ_TO_RESULT(maybeBuffer.isSome()), rejectAndReturn);

    QM_TRY_INSPECT(const auto& written,
                   WriteBuffer(maybeBuffer.extract(), aPosition),
                   rejectAndReturn);

    LOG_VERBOSE(("WritableFileStream: Wrote %" PRId64, written));
    aPromise->MaybeResolve(written);
    return;
  }

  // Step 3.4.7 Otherwise, if data is a Blob ...
  if (aData.IsBlob()) {
    Blob& blob = aData.GetAsBlob();

    nsCOMPtr<nsIInputStream> stream;
    ErrorResult error;
    blob.CreateInputStream(getter_AddRefs(stream), error);
    QM_TRY((MOZ_TO_RESULT(!error.Failed()).mapErr([&error](const nsresult rv) {
             return error.StealNSResult();
           })),
           rejectAndReturn);

    QM_TRY_INSPECT(const auto& written,
                   WriteStream(std::move(stream), aPosition), rejectAndReturn);

    LOG_VERBOSE(("WritableFileStream: Wrote %" PRId64, written));
    aPromise->MaybeResolve(written);
    return;
  }

  // Step 3.4.8 Otherwise ...
  MOZ_ASSERT(aData.IsUSVString());

  uint32_t count;
  UniquePtr<char[]> string(
      ToNewUTF8String(aData.GetAsUSVString(), &count, fallible));
  QM_TRY((MOZ_TO_RESULT(string.get()).mapErr([](const nsresult) {
           return NS_ERROR_OUT_OF_MEMORY;
         })),
         rejectAndReturn);

  Buffer<char> buffer(std::move(string), count);

  QM_TRY_INSPECT(const auto& written, WriteBuffer(std::move(buffer), aPosition),
                 rejectAndReturn);

  LOG_VERBOSE(("WritableFileStream: Wrote %" PRId64, written));
  aPromise->MaybeResolve(written);
}

void FileSystemWritableFileStream::Seek(uint64_t aPosition,
                                        RefPtr<Promise> aPromise) {
  MOZ_ASSERT(!mClosed);

  LOG_VERBOSE(("%p: Seeking to %" PRIu64, mFileDesc, aPosition));

  QM_TRY(SeekPosition(aPosition), [&aPromise](const nsresult rv) {
    aPromise->MaybeReject(rv);
    return;
  });

  aPromise->MaybeResolveWithUndefined();
}

void FileSystemWritableFileStream::Truncate(uint64_t aSize,
                                            RefPtr<Promise> aPromise) {
  MOZ_ASSERT(!mClosed);

  // truncate filehandle (can extend with 0's)
  LOG_VERBOSE(("%p: Truncate to %" PRIu64, mFileDesc, aSize));
  if (NS_WARN_IF(NS_FAILED(TruncFile(mFileDesc, aSize)))) {
    aPromise->MaybeReject(NS_ErrorAccordingToNSPR());
    return;
  }

  // We truncated; per non-normative text in the spec (2.5.3) we should
  // adjust the cursor position to be within the new file size
  int64_t where = PR_Seek(mFileDesc, 0, PR_SEEK_CUR);
  if (where == -1) {
    aPromise->MaybeReject(NS_ErrorAccordingToNSPR());
    return;
  }

  if (where > (int64_t)aSize) {
    where = PR_Seek(mFileDesc, 0, PR_SEEK_END);
    if (where == -1) {
      aPromise->MaybeReject(NS_ErrorAccordingToNSPR());
      return;
    }
  }

  aPromise->MaybeResolveWithUndefined();
}

Result<uint64_t, nsresult> FileSystemWritableFileStream::WriteBuffer(
    Buffer<char>&& aBuffer, const Maybe<uint64_t> aPosition) {
  MOZ_ASSERT(!mClosed);

  Buffer<char> buffer = std::move(aBuffer);

  const auto checkedLength = CheckedInt<PRInt32>(buffer.Length());
  QM_TRY(MOZ_TO_RESULT(checkedLength.isValid()));

  if (aPosition) {
    QM_TRY(SeekPosition(*aPosition));
  }

  return PR_Write(mFileDesc, buffer.Elements(), checkedLength.value());
}

Result<uint64_t, nsresult> FileSystemWritableFileStream::WriteStream(
    nsCOMPtr<nsIInputStream> aStream, const Maybe<uint64_t> aPosition) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(!mClosed);

  void* rawBuffer = nullptr;
  uint64_t length;
  QM_TRY(MOZ_TO_RESULT(
      NS_ReadInputStreamToBuffer(aStream, &rawBuffer, -1, &length)));

  Buffer<char> buffer(UniquePtr<char[]>(reinterpret_cast<char*>(rawBuffer)),
                      length);

  QM_TRY_RETURN(WriteBuffer(std::move(buffer), aPosition));
}

Result<Ok, nsresult> FileSystemWritableFileStream::SeekPosition(
    uint64_t aPosition) {
  MOZ_ASSERT(!mClosed);

  const auto checkedPosition = CheckedInt<int64_t>(aPosition);
  QM_TRY(MOZ_TO_RESULT(checkedPosition.isValid()));

  int64_t cnt = PR_Seek64(mFileDesc, checkedPosition.value(), PR_SEEK_SET);
  if (cnt == int64_t(-1)) {
    LOG(("Failed to seek to %" PRIu64 " (errno %d)", aPosition, errno));
    return Err(NS_ErrorAccordingToNSPR());
  }

  if (cnt != checkedPosition.value()) {
    LOG(("Failed to seek to %" PRIu64 " (errno %d), ended up at %" PRId64,
         aPosition, errno, cnt));
    return Err(NS_ERROR_FAILURE);
  }

  return Ok{};
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    WritableFileStreamUnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTION_INHERITED(WritableFileStreamUnderlyingSinkAlgorithms,
                                   UnderlyingSinkAlgorithmsBase, mStream)

void WritableFileStreamUnderlyingSinkAlgorithms::StartCallback(
    JSContext* aCx, WritableStreamDefaultController& aController,
    JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) {
  // https://streams.spec.whatwg.org/#writablestream-set-up
  // Step 1. Let startAlgorithm be an algorithm that returns undefined.
  aRetVal.setUndefined();
}

already_AddRefed<Promise>
WritableFileStreamUnderlyingSinkAlgorithms::WriteCallback(
    JSContext* aCx, JS::Handle<JS::Value> aChunk,
    WritableStreamDefaultController& aController, ErrorResult& aRv) {
  return mStream->Write(aCx, aChunk, aRv);
}

already_AddRefed<Promise>
WritableFileStreamUnderlyingSinkAlgorithms::CloseCallback(JSContext* aCx,
                                                          ErrorResult& aRv) {
  // https://streams.spec.whatwg.org/#writablestream-set-up
  // Step 2. Let closeAlgorithmWrapper be an algorithm that runs these steps:

  RefPtr<Promise> promise = Promise::Create(mStream->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mStream->IsClosed()) {
    promise->MaybeRejectWithTypeError("WritableFileStream closed");
    return promise.forget();
  }

  // XXX The close should be async. For now we have to always fallback to the
  // Step 2.3 below.
  mStream->Close();

  // Step 2.3. Return a promise resolved with undefined.
  promise->MaybeResolveWithUndefined();
  return promise.forget();
}

already_AddRefed<Promise>
WritableFileStreamUnderlyingSinkAlgorithms::AbortCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  // https://streams.spec.whatwg.org/#writablestream-set-up
  // Step 3. Let abortAlgorithmWrapper be an algorithm that runs these steps:

  // XXX The close or rather a dedicated abort should be async. For now we have
  // to always fall back to the Step 3.3 below.
  mStream->Close();

  // Step 3.3. Return a promise resolved with undefined.
  return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(), aRv);
}

void WritableFileStreamUnderlyingSinkAlgorithms::ReleaseObjects() {
  // XXX We shouldn't be calling close here. We should just release the lock.
  // However, calling close here is not a big issue for now because we don't
  // write to a temporary file which would atomically replace the real file
  // during close.
  mStream->Close();
}

}  // namespace mozilla::dom
