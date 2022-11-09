/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemWritableFileStream.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/FileSystemHandle.h"
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

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_VERBOSE(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

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
                     ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#writablestream-set-up
    // Step 1. Let startAlgorithm be an algorithm that returns undefined.
    aRetVal.setUndefined();
  }

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // https://streams.spec.whatwg.org/#writablestream-set-up
    // Step 3.3. Return a promise resolved with undefined.
    // (No abort algorithm is defined for this interface)
    return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                aRv);
  }

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CloseCallback(
      JSContext* aCx, ErrorResult& aRv) override;

 private:
  ~WritableFileStreamUnderlyingSinkAlgorithms() = default;

  RefPtr<FileSystemWritableFileStream> mStream;
};

}  // namespace

FileSystemWritableFileStream::FileSystemWritableFileStream(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    RefPtr<FileSystemWritableFileStreamChild> aActor,
    const ::mozilla::ipc::FileDescriptor& aFileDescriptor,
    const fs::FileSystemEntryMetadata& aMetadata)
    : WritableStream(aGlobal),
      mManager(aManager),
      mActor(std::move(aActor)),
      mFileDesc(nullptr),
      mMetadata(aMetadata),
      mClosed(false) {
  auto rawFD = aFileDescriptor.ClonePlatformHandle();
  mFileDesc = PR_ImportFile(PROsfd(rawFD.release()));

  LOG(("Created WritableFileStream %p for fd %p", this, mFileDesc));
  mActor->SetStream(this);
}

FileSystemWritableFileStream::~FileSystemWritableFileStream() {
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(mClosed);
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
      // default highWaterMark
      1,
      // default sizeAlgorithm
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
  if (mClosed) {
    return;
  }

  LOG(("%p: Closing", mFileDesc));

  mClosed = true;

  PR_Close(mFileDesc);
  mFileDesc = nullptr;

  mActor->SendClose();
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
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  nsCString utf8Str;
  const uint8_t* data;
  size_t length = 0;
  uint64_t written = 0;
  nsresult result;
  UniquePtr<uint8_t> temp;

  if (mClosed) {
    promise->MaybeRejectWithTypeError("WritableFileStream closed");
    return promise.forget();
  }

  if (aData.IsWriteParams()) {
    const WriteParams& params = aData.GetAsWriteParams();
    if (params.mType == WriteCommandType::Write) {
      if (!params.mData.WasPassed()) {
        aError.ThrowSyntaxError("write() requires data");
        return nullptr;
      }
      if (params.mData.Value().IsNull()) {
        promise->MaybeRejectWithTypeError("write() of null data");
        return promise.forget();
      }
      if (params.mPosition.WasPassed()) {
        if (params.mPosition.Value().IsNull()) {
          promise->MaybeRejectWithTypeError("write() with null position");
          return promise.forget();
        }
        if (!DoSeek(promise, params.mPosition.Value().Value())) {
          return promise.forget();
        }
      }
      if (params.mData.Value().Value().IsUSVString()) {
        if (NS_WARN_IF(!AppendUTF16toUTF8(
                params.mData.Value().Value().GetAsUSVString(), utf8Str,
                mozilla::fallible))) {
          return nullptr;
        }
        temp.reset(new uint8_t[utf8Str.Length() + 1]);
        memcpy(temp.get(), PromiseFlatCString(utf8Str).get(),
               utf8Str.Length() + 1);
        data = (uint8_t*)temp.get();
        length = strlen((char*)data);
      } else if (params.mData.Value().Value().IsArrayBuffer()) {
        const ArrayBuffer& buffer =
            params.mData.Value().Value().GetAsArrayBuffer();
        buffer.ComputeState();
        data = buffer.Data();
        length = buffer.Length();
      } else if (params.mData.Value().Value().IsArrayBufferView()) {
        const ArrayBufferView& buffer =
            params.mData.Value().Value().GetAsArrayBufferView();
        buffer.ComputeState();
        data = buffer.Data();
        length = buffer.Length();
      } else if (params.mData.Value().Value().IsBlob()) {
        RefPtr<Blob> blob = params.mData.Value().Value().GetAsBlob().get();
        if (NS_FAILED(result = WriteBlob(blob, written))) {
          promise->MaybeReject(result);
        } else {
          promise->MaybeResolve(written);
        }
        return promise.forget();
      }
    } else if (params.mType == WriteCommandType::Seek) {
      if (!params.mPosition.WasPassed()) {
        aError.ThrowSyntaxError("seek() requires a position");
        return nullptr;
      }
      if (params.mPosition.Value().IsNull()) {
        promise->MaybeRejectWithTypeError("seek() with null position");
        return promise.forget();
      }
      return Seek(params.mPosition.Value().Value(), aError);
    } else if (params.mType == WriteCommandType::Truncate) {
      if (!params.mSize.WasPassed()) {
        aError.ThrowSyntaxError("truncate() requires a size");
        return nullptr;
      }
      if (params.mSize.Value().IsNull()) {
        promise->MaybeRejectWithTypeError("truncate() with null size");
        return promise.forget();
      }
      return Truncate(params.mSize.Value().Value(), aError);
    }
  } else if (aData.IsUSVString()) {
    if (NS_WARN_IF(!AppendUTF16toUTF8(aData.GetAsUSVString(), utf8Str,
                                      mozilla::fallible))) {
      promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
      return promise.forget();
    }
    temp.reset(new uint8_t[utf8Str.Length() + 1]);
    memcpy(temp.get(), PromiseFlatCString(utf8Str).get(), utf8Str.Length() + 1);
    data = (uint8_t*)temp.get();
    length = strlen((char*)data);
  } else if (aData.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aData.GetAsArrayBuffer();
    buffer.ComputeState();
    data = buffer.Data();
    length = buffer.Length();
  } else if (aData.IsArrayBufferView()) {
    const ArrayBufferView& buffer = aData.GetAsArrayBufferView();
    buffer.ComputeState();
    data = buffer.Data();
    length = buffer.Length();
  } else if (aData.IsBlob()) {
    if (NS_FAILED(result = WriteBlob(&aData.GetAsBlob(), written))) {
      // XXX wpt tests check for throwing a NotFound error if the
      // underlying file is removed, not the generic
      // NS_ERROR_FILE_NOT_FOUND.  This is not in the spec
      if (result == NS_ERROR_FILE_NOT_FOUND) {
        promise->MaybeRejectWithNotFoundError("File not found");
      } else {
        promise->MaybeReject(result);
      }
    } else {
      promise->MaybeResolve(written);
    }
    return promise.forget();
  } else {
    promise->MaybeReject(NS_ERROR_FAILURE);  // XXX
    return promise.forget();
  }

  const auto checkedLength = CheckedInt<PRInt32>(length);
  QM_TRY(MOZ_TO_RESULT(checkedLength.isValid()), [&promise](const nsresult rv) {
    promise->MaybeReject(rv);
    return promise.forget();
  });

  UniquePtr<uint8_t> buffer(new uint8_t[checkedLength.value()]);
  memcpy(buffer.get(), data, checkedLength.value());
  written =
      PR_Write(mFileDesc, buffer.get(), checkedLength.value());  // XXX  errors?
  promise->MaybeResolve(written);
  return promise.forget();
}

already_AddRefed<Promise> FileSystemWritableFileStream::Seek(
    uint64_t aPosition, ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }
  if (mClosed) {
    promise->MaybeRejectWithTypeError("WritableFileStream closed");
    return promise.forget();
  }
  if (DoSeek(promise, aPosition)) {
    promise->MaybeResolveWithUndefined();
  }  // on errors DoSeek rejects the promise

  return promise.forget();
}

already_AddRefed<Promise> FileSystemWritableFileStream::Truncate(
    uint64_t aSize, ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }
  if (mClosed) {
    promise->MaybeRejectWithTypeError("WritableFileStream closed");
    return promise.forget();
  }

  // XXX!!! this should be submitted to a TaskQueue instead of sync IO on
  // mainthread! submit async truncate
  // XXX what happens if we read/write before seek finishes?
  // Should we block read/write if an async operation is pending?
  // What if there's an error, and several operations are pending?
  // Spec issues raised.

  // truncate filehandle (can extend with 0's)
  LOG(("%p: Truncate to %" PRIu64, mFileDesc, aSize));
  if (NS_WARN_IF(NS_FAILED(TruncFile(mFileDesc, aSize)))) {
    promise->MaybeReject(NS_ErrorAccordingToNSPR());
  } else {
    // We truncated; per non-normative text in the spec (2.5.3) we should
    // adjust the cursor position to be within the new file size
    int64_t where = PR_Seek(mFileDesc, 0, PR_SEEK_CUR);
    if (where == -1) {
      promise->MaybeReject(NS_ErrorAccordingToNSPR());
      return promise.forget();
    }
    if (where > (int64_t)aSize) {
      where = PR_Seek(mFileDesc, 0, PR_SEEK_END);
      if (where == -1) {
        promise->MaybeReject(NS_ErrorAccordingToNSPR());
      }
    }
    promise->MaybeResolveWithUndefined();
  }

  return promise.forget();
}

nsresult FileSystemWritableFileStream::WriteBlob(Blob* aBlob,
                                                 uint64_t& aWritten) {
  NS_ENSURE_ARG_POINTER(aBlob);
  if (mClosed) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;  // XXX
  }

  nsresult rv;
  nsCOMPtr<nsIInputStream> msgStream;
  ErrorResult error;
  aBlob->CreateInputStream(getter_AddRefs(msgStream), error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  void* data = nullptr;
  uint64_t length;
  if (NS_WARN_IF(NS_FAILED(
          rv = NS_ReadInputStreamToBuffer(msgStream, &data, -1, &length)))) {
    return rv;
  }

  const auto checkedLength = CheckedInt<PRInt32>(length);
  QM_TRY(MOZ_TO_RESULT(checkedLength.isValid()));

  // XXX!!! this should be submitted to a TaskQueue instead of sync IO on
  // mainthread!
  aWritten += PR_Write(mFileDesc, data, checkedLength.value());  // XXX errors?
  free(data);
  return NS_OK;
}

bool FileSystemWritableFileStream::DoSeek(RefPtr<Promise>& aPromise,
                                          uint64_t aPosition) {
  // submit async seek
  // XXX!!! this should be submitted to a TaskQueue instead of sync IO on
  // mainthread!
  // XXX what happens if we read/write before seek finishes?
  // Should we block read/write if an async operation is pending?
  // Handle seek before write ('at')
  LOG_VERBOSE(("%p: Seeking to %" PRIu64, mFileDesc, aPosition));
  const CheckedInt<PROffset32> checkedPosition(aPosition);
  if (NS_WARN_IF(!checkedPosition.isValid())) {
    return false;
  }
  int64_t where = PR_Seek(mFileDesc, checkedPosition.value(), PR_SEEK_SET);
  if (where == -1) {
    LOG(("Failed to seek to %" PRIu64 " (errno %d)", aPosition, errno));
    aPromise->MaybeReject(NS_ERROR_FAILURE);  // XXX
    return false;
  }

  if (where != (int64_t)aPosition) {
    LOG(("Failed to seek to %" PRIu64 " (errno %d), ended up at %" PRId64,
         aPosition, errno, where));
    aPromise->MaybeReject(NS_ERROR_FAILURE);  // XXX
    return false;
  }
  // caller resolves the promise for success
  return true;
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    WritableFileStreamUnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTION_INHERITED(WritableFileStreamUnderlyingSinkAlgorithms,
                                   UnderlyingSinkAlgorithmsBase, mStream)

already_AddRefed<Promise>
WritableFileStreamUnderlyingSinkAlgorithms::WriteCallback(
    JSContext* aCx, JS::Handle<JS::Value> aChunk,
    WritableStreamDefaultController& aController, ErrorResult& aRv) {
  // https://fs.spec.whatwg.org/#create-a-new-filesystemwritablefilestream
  // Step 3. Let writeAlgorithm be an algorithm which takes a chunk argument ...
  ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams chunkUnion;
  if (!chunkUnion.Init(aCx, aChunk)) {
    aRv.MightThrowJSException();
    aRv.StealExceptionFromJSContext(aCx);
    return nullptr;
  }
  // Step 3. ... and returns the result of running the write a chunk algorithm
  // with stream and chunk.
  return mStream->Write(chunkUnion, aRv);
}

already_AddRefed<Promise>
WritableFileStreamUnderlyingSinkAlgorithms::CloseCallback(JSContext* aCx,
                                                          ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mStream->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mStream->IsClosed()) {
    promise->MaybeRejectWithTypeError("WritableFileStream closed");
    return promise.forget();
  }

  mStream->Close();

  promise->MaybeResolveWithUndefined();
  return promise.forget();
}

}  // namespace mozilla::dom
