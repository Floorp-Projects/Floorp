/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WritableStreamDefaultWriter.h"
#include "js/Array.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/WritableStreamDefaultWriterBinding.h"
#include "nsCOMPtr.h"

#include "mozilla/dom/Promise-inl.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(WritableStreamDefaultWriter)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WritableStreamDefaultWriter)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal, mStream, mReadyPromise,
                                  mClosedPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WritableStreamDefaultWriter)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal, mStream, mReadyPromise,
                                    mClosedPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WritableStreamDefaultWriter)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WritableStreamDefaultWriter)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WritableStreamDefaultWriter)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WritableStreamDefaultWriter)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

WritableStreamDefaultWriter::WritableStreamDefaultWriter(
    nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal) {
  mozilla::HoldJSObjects(this);
}

WritableStreamDefaultWriter::~WritableStreamDefaultWriter() {
  mozilla::DropJSObjects(this);
}

void WritableStreamDefaultWriter::SetReadyPromise(Promise* aPromise) {
  MOZ_ASSERT(aPromise);
  mReadyPromise = aPromise;
}

void WritableStreamDefaultWriter::SetClosedPromise(Promise* aPromise) {
  MOZ_ASSERT(aPromise);
  mClosedPromise = aPromise;
}

JSObject* WritableStreamDefaultWriter::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return WritableStreamDefaultWriter_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<WritableStreamDefaultWriter>
WritableStreamDefaultWriter::Constructor(const GlobalObject& aGlobal,
                                         WritableStream& aStream,
                                         ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<WritableStreamDefaultWriter> writer =
      new WritableStreamDefaultWriter(global);
  SetUpWritableStreamDefaultWriter(writer, &aStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return writer.forget();
}

already_AddRefed<Promise> WritableStreamDefaultWriter::Closed() {
  RefPtr<Promise> closedPromise = mClosedPromise;
  return closedPromise.forget();
}

already_AddRefed<Promise> WritableStreamDefaultWriter::Ready() {
  RefPtr<Promise> readyPromise = mReadyPromise;
  return readyPromise.forget();
}

// https://streams.spec.whatwg.org/#writable-stream-default-writer-get-desired-size
Nullable<double> WritableStreamDefaultWriterGetDesiredSize(
    WritableStreamDefaultWriter* aWriter) {
  // Step 1. Let stream be writer.[[stream]].
  RefPtr<WritableStream> stream = aWriter->GetStream();

  // Step 2. Let state be stream.[[state]].
  WritableStream::WriterState state = stream->State();

  // Step 3. If state is "errored" or "erroring", return null.
  if (state == WritableStream::WriterState::Errored ||
      state == WritableStream::WriterState::Erroring) {
    return nullptr;
  }

  // Step 4. If state is "closed", return 0.
  if (state == WritableStream::WriterState::Closed) {
    return 0.0;
  }

  // Step 5. Return
  // ! WritableStreamDefaultControllerGetDesiredSize(stream.[[controller]]).
  return stream->Controller()->GetDesiredSize();
}

// https://streams.spec.whatwg.org/#default-writer-desired-size
Nullable<double> WritableStreamDefaultWriter::GetDesiredSize(ErrorResult& aRv) {
  // Step 1. If this.[[stream]] is undefined, throw a TypeError exception.
  if (!mStream) {
    aRv.ThrowTypeError("Missing stream");
    return nullptr;
  }

  // Step 2. Return ! WritableStreamDefaultWriterGetDesiredSize(this).
  RefPtr<WritableStreamDefaultWriter> thisRefPtr = this;
  return WritableStreamDefaultWriterGetDesiredSize(thisRefPtr);
}

// https://streams.spec.whatwg.org/#writable-stream-default-writer-abort
MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> WritableStreamDefaultWriterAbort(
    JSContext* aCx, WritableStreamDefaultWriter* aWriter,
    JS::Handle<JS::Value> aReason, ErrorResult& aRv) {
  // Step 1. Let stream be writer.[[stream]].
  RefPtr<WritableStream> stream = aWriter->GetStream();

  // Step 2. Assert: stream is not undefined.
  MOZ_ASSERT(stream);

  // Step 3. Return ! WritableStreamAbort(stream, reason).
  return WritableStreamAbort(aCx, stream, aReason, aRv);
}

// https://streams.spec.whatwg.org/#default-writer-abort
already_AddRefed<Promise> WritableStreamDefaultWriter::Abort(
    JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv) {
  // Step 1. If this.[[stream]] is undefined, return a promise rejected with a
  // TypeError exception.
  if (!mStream) {
    aRv.ThrowTypeError("Missing stream");
    return nullptr;
  }

  // Step 2. Return ! WritableStreamDefaultWriterAbort(this, reason).
  RefPtr<WritableStreamDefaultWriter> thisRefPtr = this;
  return WritableStreamDefaultWriterAbort(aCx, thisRefPtr, aReason, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-default-writer-close
MOZ_CAN_RUN_SCRIPT static already_AddRefed<Promise>
WritableStreamDefaultWriterClose(JSContext* aCx,
                                 WritableStreamDefaultWriter* aWriter,
                                 ErrorResult& aRv) {
  // Step 1. Let stream be writer.[[stream]].
  RefPtr<WritableStream> stream = aWriter->GetStream();

  // Step 2. Assert: stream is not undefined.
  MOZ_ASSERT(stream);

  // Step 3. Return ! WritableStreamClose(stream).
  return WritableStreamClose(aCx, stream, aRv);
}

// https://streams.spec.whatwg.org/#default-writer-close
already_AddRefed<Promise> WritableStreamDefaultWriter::Close(JSContext* aCx,
                                                             ErrorResult& aRv) {
  // Step 1. Let stream be this.[[stream]].
  RefPtr<WritableStream> stream = mStream;

  // Step 2. If stream is undefined, return a promise rejected with a TypeError
  // exception.
  if (!stream) {
    aRv.ThrowTypeError("Missing stream");
    return nullptr;
  }

  // Step 3. If ! WritableStreamCloseQueuedOrInFlight(stream) is true,
  // return a promise rejected with a TypeError exception.
  if (stream->CloseQueuedOrInFlight()) {
    aRv.ThrowTypeError("Stream is closing");
    return nullptr;
  }

  // Step 3. Return ! WritableStreamDefaultWriterClose(this).
  RefPtr<WritableStreamDefaultWriter> thisRefPtr = this;
  return WritableStreamDefaultWriterClose(aCx, thisRefPtr, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-default-writer-release
void WritableStreamDefaultWriterRelease(JSContext* aCx,
                                        WritableStreamDefaultWriter* aWriter,
                                        ErrorResult& aRv) {
  // Step 1. Let stream be writer.[[stream]].
  RefPtr<WritableStream> stream = aWriter->GetStream();

  // Step 2. Assert: stream is not undefined.
  MOZ_ASSERT(stream);

  // Step 3. Assert: stream.[[writer]] is writer.
  MOZ_ASSERT(stream->GetWriter() == aWriter);

  // Step 4. Let releasedError be a new TypeError.
  JS::Rooted<JS::Value> releasedError(RootingCx(), JS::UndefinedValue());
  {
    ErrorResult rv;
    rv.ThrowTypeError("Releasing lock");
    bool ok = ToJSValue(aCx, std::move(rv), &releasedError);
    MOZ_RELEASE_ASSERT(ok, "must be ok");
  }

  // Step 5. Perform !
  // WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer,
  // releasedError).
  WritableStreamDefaultWriterEnsureReadyPromiseRejected(aWriter, releasedError,
                                                        aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 6. Perform !
  // WritableStreamDefaultWriterEnsureClosedPromiseRejected(writer,
  // releasedError).
  WritableStreamDefaultWriterEnsureClosedPromiseRejected(aWriter, releasedError,
                                                         aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 7. Set stream.[[writer]] to undefined.
  stream->SetWriter(nullptr);

  // Step 8. Set writer.[[stream]] to undefined.
  aWriter->SetStream(nullptr);
}

// https://streams.spec.whatwg.org/#default-writer-release-lock
void WritableStreamDefaultWriter::ReleaseLock(JSContext* aCx,
                                              ErrorResult& aRv) {
  // Step 1. Let stream be this.[[stream]].
  RefPtr<WritableStream> stream = mStream;

  // Step 2. If stream is undefined, return.
  if (!stream) {
    return;
  }

  // Step 3. Assert: stream.[[writer]] is not undefined.
  MOZ_ASSERT(stream->GetWriter());

  // Step 4. Perform ! WritableStreamDefaultWriterRelease(this).
  RefPtr<WritableStreamDefaultWriter> thisRefPtr = this;
  return WritableStreamDefaultWriterRelease(aCx, thisRefPtr, aRv);
}

// https://streams.spec.whatwg.org/#writable-stream-default-writer-write
already_AddRefed<Promise> WritableStreamDefaultWriterWrite(
    JSContext* aCx, WritableStreamDefaultWriter* aWriter,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv) {
  // Step 1. Let stream be writer.[[stream]].
  RefPtr<WritableStream> stream = aWriter->GetStream();

  // Step 2. Assert: stream is not undefined.
  MOZ_ASSERT(stream);

  // Step 3. Let controller be stream.[[controller]].
  RefPtr<WritableStreamDefaultController> controller = stream->Controller();

  // Step 4. Let chunkSize be !
  // WritableStreamDefaultControllerGetChunkSize(controller, chunk).
  double chunkSize =
      WritableStreamDefaultControllerGetChunkSize(aCx, controller, aChunk, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 5. If stream is not equal to writer.[[stream]], return a promise
  // rejected with a TypeError exception.
  if (stream != aWriter->GetStream()) {
    aRv.ThrowTypeError(
        "Can not write on WritableStream owned by another writer.");
    return nullptr;
  }

  // Step 6. Let state be stream.[[state]].
  WritableStream::WriterState state = stream->State();

  // Step 7. If state is "errored", return a promise rejected with
  // stream.[[storedError]].
  if (state == WritableStream::WriterState::Errored) {
    JS::Rooted<JS::Value> error(aCx, stream->StoredError());
    return Promise::CreateRejected(aWriter->GetParentObject(), error, aRv);
  }

  // Step 8. If ! WritableStreamCloseQueuedOrInFlight(stream) is true or state
  // is "closed", return a promise rejected with a TypeError exception
  // indicating that the stream is closing or closed.
  if (stream->CloseQueuedOrInFlight() ||
      state == WritableStream::WriterState::Closed) {
    return Promise::CreateRejectedWithTypeError(
        aWriter->GetParentObject(), "Stream is closed or closing"_ns, aRv);
  }

  // Step 9. If state is "erroring", return a promise rejected with
  // stream.[[storedError]].
  if (state == WritableStream::WriterState::Erroring) {
    JS::Rooted<JS::Value> error(aCx, stream->StoredError());
    return Promise::CreateRejected(aWriter->GetParentObject(), error, aRv);
  }

  // Step 10. Assert: state is "writable".
  MOZ_ASSERT(state == WritableStream::WriterState::Writable);

  // Step 11. Let promise be ! WritableStreamAddWriteRequest(stream).
  RefPtr<Promise> promise = WritableStreamAddWriteRequest(stream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 12. Perform ! WritableStreamDefaultControllerWrite(controller, chunk,
  // chunkSize).
  WritableStreamDefaultControllerWrite(aCx, controller, aChunk, chunkSize, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 13. Return promise.
  return promise.forget();
}

// https://streams.spec.whatwg.org/#default-writer-write
already_AddRefed<Promise> WritableStreamDefaultWriter::Write(
    JSContext* aCx, JS::Handle<JS::Value> aChunk, ErrorResult& aRv) {
  // Step 1. If this.[[stream]] is undefined, return a promise rejected with a
  // TypeError exception.
  if (!mStream) {
    aRv.ThrowTypeError("Missing stream");
    return nullptr;
  }

  // Step 2. Return ! WritableStreamDefaultWriterWrite(this, chunk).
  return WritableStreamDefaultWriterWrite(aCx, this, aChunk, aRv);
}

// https://streams.spec.whatwg.org/#set-up-writable-stream-default-writer
void SetUpWritableStreamDefaultWriter(WritableStreamDefaultWriter* aWriter,
                                      WritableStream* aStream,
                                      ErrorResult& aRv) {
  // Step 1. If ! IsWritableStreamLocked(stream) is true, throw a TypeError
  // exception.
  if (IsWritableStreamLocked(aStream)) {
    aRv.ThrowTypeError("WritableStream is already locked!");
    return;
  }

  // Step 2. Set writer.[[stream]] to stream.
  aWriter->SetStream(aStream);

  // Step 3. Set stream.[[writer]] to writer.
  aStream->SetWriter(aWriter);

  // Step 4. Let state be stream.[[state]].
  WritableStream::WriterState state = aStream->State();

  // Step 5. If state is "writable",
  if (state == WritableStream::WriterState::Writable) {
    RefPtr<Promise> readyPromise =
        Promise::Create(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 5.1 If ! WritableStreamCloseQueuedOrInFlight(stream) is false and
    // stream.[[backpressure]] is true, set writer.[[readyPromise]] to a new
    // promise.
    if (!aStream->CloseQueuedOrInFlight() && aStream->Backpressure()) {
      aWriter->SetReadyPromise(readyPromise);
    } else {
      // Step 5.2. Otherwise, set writer.[[readyPromise]] to a promise resolved
      // with undefined.
      readyPromise->MaybeResolveWithUndefined();
      aWriter->SetReadyPromise(readyPromise);
    }

    // Step 5.3. Set writer.[[closedPromise]] to a new promise.
    RefPtr<Promise> closedPromise =
        Promise::Create(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    aWriter->SetClosedPromise(closedPromise);
  } else if (state == WritableStream::WriterState::Erroring) {
    // Step 6. Otherwise, if state is "erroring",

    // Step 6.1. Set writer.[[readyPromise]] to a promise rejected with
    // stream.[[storedError]].
    JS::Rooted<JS::Value> storedError(RootingCx(), aStream->StoredError());
    RefPtr<Promise> readyPromise =
        Promise::Create(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    readyPromise->MaybeReject(storedError);
    aWriter->SetReadyPromise(readyPromise);

    // Step 6.2. Set writer.[[readyPromise]].[[PromiseIsHandled]] to true.
    readyPromise->SetSettledPromiseIsHandled();

    // Step 6.3. Set writer.[[closedPromise]] to a new promise.
    RefPtr<Promise> closedPromise =
        Promise::Create(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    aWriter->SetClosedPromise(closedPromise);
  } else if (state == WritableStream::WriterState::Closed) {
    // Step 7. Otherwise, if state is "closed",
    // Step 7.1. Set writer.[[readyPromise]] to a promise resolved with
    // undefined.
    RefPtr<Promise> readyPromise =
        Promise::CreateResolvedWithUndefined(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    aWriter->SetReadyPromise(readyPromise);

    // Step 7.2. Set writer.[[closedPromise]] to a promise resolved with
    // undefined.
    RefPtr<Promise> closedPromise =
        Promise::CreateResolvedWithUndefined(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    aWriter->SetClosedPromise(closedPromise);
  } else {
    // Step 8. Otherwise,
    // Step 8.1 Assert: state is "errored".
    MOZ_ASSERT(state == WritableStream::WriterState::Errored);

    // Step 8.2. Step Let storedError be stream.[[storedError]].
    JS::Rooted<JS::Value> storedError(RootingCx(), aStream->StoredError());

    // Step 8.3. Set writer.[[readyPromise]] to a promise rejected with
    // storedError.
    RefPtr<Promise> readyPromise =
        Promise::Create(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    readyPromise->MaybeReject(storedError);
    aWriter->SetReadyPromise(readyPromise);

    // Step 8.4. Set writer.[[readyPromise]].[[PromiseIsHandled]] to true.
    readyPromise->SetSettledPromiseIsHandled();

    // Step 8.5. Set writer.[[closedPromise]] to a promise rejected with
    // storedError.
    RefPtr<Promise> closedPromise =
        Promise::Create(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    closedPromise->MaybeReject(storedError);
    aWriter->SetClosedPromise(closedPromise);

    // Step 8.6 Set writer.[[closedPromise]].[[PromiseIsHandled]] to true.
    closedPromise->SetSettledPromiseIsHandled();
  }
}

// https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-closed-promise-rejected
void WritableStreamDefaultWriterEnsureClosedPromiseRejected(
    WritableStreamDefaultWriter* aWriter, JS::Handle<JS::Value> aError,
    ErrorResult& aRv) {
  RefPtr<Promise> closedPromise = aWriter->ClosedPromise();
  // Step 1. If writer.[[closedPromise]].[[PromiseState]] is "pending", reject
  // writer.[[closedPromise]] with error.
  if (closedPromise->State() == Promise::PromiseState::Pending) {
    closedPromise->MaybeReject(aError);
  } else {
    // Step 2. Otherwise, set writer.[[closedPromise]] to a promise rejected
    // with error.
    closedPromise = Promise::Create(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    closedPromise->MaybeReject(aError);
    aWriter->SetClosedPromise(closedPromise);
  }

  // Step 3. Set writer.[[closedPromise]].[[PromiseIsHandled]] to true.
  closedPromise->SetSettledPromiseIsHandled();
}

// https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-ready-promise-rejected
void WritableStreamDefaultWriterEnsureReadyPromiseRejected(
    WritableStreamDefaultWriter* aWriter, JS::Handle<JS::Value> aError,
    ErrorResult& aRv) {
  RefPtr<Promise> readyPromise = aWriter->ReadyPromise();
  // Step 1. If writer.[[readyPromise]].[[PromiseState]] is "pending", reject
  // writer.[[readyPromise]] with error.
  if (readyPromise->State() == Promise::PromiseState::Pending) {
    readyPromise->MaybeReject(aError);
  } else {
    // Step 2. Otherwise, set writer.[[readyPromise]] to a promise rejected with
    // error.
    readyPromise = Promise::Create(aWriter->GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    readyPromise->MaybeReject(aError);
    aWriter->SetReadyPromise(readyPromise);
  }

  // Step 3. Set writer.[[readyPromise]].[[PromiseIsHandled]] to true.
  readyPromise->SetSettledPromiseIsHandled();
}

// https://streams.spec.whatwg.org/#writable-stream-default-writer-close-with-error-propagation
already_AddRefed<Promise> WritableStreamDefaultWriterCloseWithErrorPropagation(
    JSContext* aCx, WritableStreamDefaultWriter* aWriter, ErrorResult& aRv) {
  // Step 1. Let stream be writer.[[stream]].
  RefPtr<WritableStream> stream = aWriter->GetStream();

  // Step 2. Assert: stream is not undefined.
  MOZ_ASSERT(stream);

  // Step 3. Let state be stream.[[state]].
  WritableStream::WriterState state = stream->State();

  // Step 4. If ! WritableStreamCloseQueuedOrInFlight(stream) is true
  // or state is "closed", return a promise resolved with undefined.
  if (stream->CloseQueuedOrInFlight() ||
      state == WritableStream::WriterState::Closed) {
    return Promise::CreateResolvedWithUndefined(aWriter->GetParentObject(),
                                                aRv);
  }

  // Step 5. If state is "errored",
  // return a promise rejected with stream.[[storedError]].
  if (state == WritableStream::WriterState::Errored) {
    JS::Rooted<JS::Value> error(aCx, stream->StoredError());
    return Promise::CreateRejected(aWriter->GetParentObject(), error, aRv);
  }

  // Step 6. Assert: state is "writable" or "erroring".
  MOZ_ASSERT(state == WritableStream::WriterState::Writable ||
             state == WritableStream::WriterState::Erroring);

  // Step 7. Return ! WritableStreamDefaultWriterClose(writer).
  return WritableStreamDefaultWriterClose(aCx, aWriter, aRv);
}

}  // namespace mozilla::dom
