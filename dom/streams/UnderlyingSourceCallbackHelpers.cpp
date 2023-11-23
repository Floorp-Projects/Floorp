/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamUtils.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "js/experimental/TypedData.h"
#include "nsStreamUtils.h"

namespace mozilla::dom {

using namespace streams_abstract;

// UnderlyingSourceAlgorithmsBase
NS_IMPL_CYCLE_COLLECTION(UnderlyingSourceAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSourceAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourceAlgorithmsBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_INHERITED_WITH_JS_MEMBERS(
    UnderlyingSourceAlgorithms, UnderlyingSourceAlgorithmsBase,
    (mGlobal, mStartCallback, mPullCallback, mCancelCallback),
    (mUnderlyingSource))
NS_IMPL_ADDREF_INHERITED(UnderlyingSourceAlgorithms,
                         UnderlyingSourceAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(UnderlyingSourceAlgorithms,
                          UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourceAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceAlgorithmsBase)

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
void UnderlyingSourceAlgorithms::StartCallback(
    JSContext* aCx, ReadableStreamController& aController,
    JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) {
  if (!mStartCallback) {
    // Step 2: Let startAlgorithm be an algorithm that returns undefined.
    aRetVal.setUndefined();
    return;
  }

  // Step 5: If underlyingSourceDict["start"] exists, then set startAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSourceDict["start"] with argument list « controller » and
  // callback this value underlyingSource.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  ReadableStreamDefaultControllerOrReadableByteStreamController controller;
  if (aController.IsDefault()) {
    controller.SetAsReadableStreamDefaultController() = aController.AsDefault();
  } else {
    controller.SetAsReadableByteStreamController() = aController.AsByte();
  }

  return mStartCallback->Call(thisObj, controller, aRetVal, aRv,
                              "UnderlyingSource.start",
                              CallbackFunction::eRethrowExceptions);
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
already_AddRefed<Promise> UnderlyingSourceAlgorithms::PullCallback(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  if (!mPullCallback) {
    // Step 3: Let pullAlgorithm be an algorithm that returns a promise resolved
    // with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 6: If underlyingSourceDict["pull"] exists, then set pullAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSourceDict["pull"] with argument list « controller » and callback
  // this value underlyingSource.
  ReadableStreamDefaultControllerOrReadableByteStreamController controller;
  if (aController.IsDefault()) {
    controller.SetAsReadableStreamDefaultController() = aController.AsDefault();
  } else {
    controller.SetAsReadableByteStreamController() = aController.AsByte();
  }

  RefPtr<Promise> promise =
      mPullCallback->Call(thisObj, controller, aRv, "UnderlyingSource.pull",
                          CallbackFunction::eRethrowExceptions);

  return promise.forget();
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
already_AddRefed<Promise> UnderlyingSourceAlgorithms::CancelCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  if (!mCancelCallback) {
    // Step 4: Let cancelAlgorithm be an algorithm that returns a promise
    // resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 7: If underlyingSourceDict["cancel"] exists, then set cancelAlgorithm
  // to an algorithm which takes an argument reason and returns the result of
  // invoking underlyingSourceDict["cancel"] with argument list « reason » and
  // callback this value underlyingSource.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  RefPtr<Promise> promise =
      mCancelCallback->Call(thisObj, aReason, aRv, "UnderlyingSource.cancel",
                            CallbackFunction::eRethrowExceptions);

  return promise.forget();
}

// Shared between:
// https://streams.spec.whatwg.org/#readablestream-set-up
// https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support
// Step 1: Let startAlgorithm be an algorithm that returns undefined.
void UnderlyingSourceAlgorithmsWrapper::StartCallback(
    JSContext*, ReadableStreamController&, JS::MutableHandle<JS::Value> aRetVal,
    ErrorResult&) {
  aRetVal.setUndefined();
}

// Shared between:
// https://streams.spec.whatwg.org/#readablestream-set-up
// https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support
// Step 2: Let pullAlgorithmWrapper be an algorithm that runs these steps:
already_AddRefed<Promise> UnderlyingSourceAlgorithmsWrapper::PullCallback(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = aController.GetParentObject();
  return PromisifyAlgorithm(
      global,
      [&](ErrorResult& aRv) MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
        return PullCallbackImpl(aCx, aController, aRv);
      },
      aRv);
}

// Shared between:
// https://streams.spec.whatwg.org/#readablestream-set-up
// https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support
// Step 3: Let cancelAlgorithmWrapper be an algorithm that runs these steps:
already_AddRefed<Promise> UnderlyingSourceAlgorithmsWrapper::CancelCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(aCx);
  return PromisifyAlgorithm(
      global,
      [&](ErrorResult& aRv) MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
        return CancelCallbackImpl(aCx, aReason, aRv);
      },
      aRv);
}

NS_IMPL_ISUPPORTS(InputStreamHolder, nsIInputStreamCallback)

InputStreamHolder::InputStreamHolder(nsIGlobalObject* aGlobal,
                                     InputToReadableStreamAlgorithms* aCallback,
                                     nsIAsyncInputStream* aInput)
    : GlobalTeardownObserver(aGlobal), mCallback(aCallback), mInput(aInput) {}

void InputStreamHolder::Init(JSContext* aCx) {
  if (!NS_IsMainThread()) {
    // We're in a worker
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    MOZ_ASSERT(workerPrivate);

    workerPrivate->AssertIsOnWorkerThread();

    // Note, this will create a ref-cycle between the holder and the stream.
    // The cycle is broken when the stream is closed or the worker begins
    // shutting down.
    mWorkerRef = StrongWorkerRef::Create(workerPrivate, "InputStreamHolder",
                                         [self = RefPtr{this}]() {});
    if (NS_WARN_IF(!mWorkerRef)) {
      return;
    }
  }
}

InputStreamHolder::~InputStreamHolder() = default;

void InputStreamHolder::DisconnectFromOwner() {
  Shutdown();
  GlobalTeardownObserver::DisconnectFromOwner();
}

void InputStreamHolder::Shutdown() {
  if (mInput) {
    mInput->Close();
  }
  // NOTE(krosylight): Dropping mAsyncWaitAlgorithms here means letting cycle
  // collection happen on the underlying source, which can cause a dangling
  // read promise that never resolves. Doing so shouldn't be a problem at
  // shutdown phase.
  // Note that this is currently primarily for Fetch which does not explicitly
  // close its streams at shutdown. (i.e. to prevent memory leak for cases e.g
  // WPT /fetch/api/basic/stream-response.any.html)
  mAsyncWaitAlgorithms = nullptr;
  // If we have an AsyncWait running, we'll get a callback and clear
  // the mAsyncWaitWorkerRef
  mWorkerRef = nullptr;
}

nsresult InputStreamHolder::AsyncWait(uint32_t aFlags, uint32_t aRequestedCount,
                                      nsIEventTarget* aEventTarget) {
  nsresult rv = mInput->AsyncWait(this, aFlags, aRequestedCount, aEventTarget);
  if (NS_SUCCEEDED(rv)) {
    mAsyncWaitWorkerRef = mWorkerRef;
    mAsyncWaitAlgorithms = mCallback;
  }
  return rv;
}

NS_IMETHODIMP InputStreamHolder::OnInputStreamReady(
    nsIAsyncInputStream* aStream) {
  mAsyncWaitWorkerRef = nullptr;
  mAsyncWaitAlgorithms = nullptr;
  // We may get called back after ::Shutdown()
  if (mCallback) {
    return mCallback->OnInputStreamReady(aStream);
  }
  return NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(InputToReadableStreamAlgorithms,
                                             UnderlyingSourceAlgorithmsWrapper,
                                             nsIInputStreamCallback)
NS_IMPL_CYCLE_COLLECTION_WEAK_PTR_INHERITED(InputToReadableStreamAlgorithms,
                                            UnderlyingSourceAlgorithmsWrapper,
                                            mPullPromise, mStream)

InputToReadableStreamAlgorithms::InputToReadableStreamAlgorithms(
    JSContext* aCx, nsIAsyncInputStream* aInput, ReadableStream* aStream)
    : mOwningEventTarget(GetCurrentSerialEventTarget()),
      mInput(new InputStreamHolder(aStream->GetParentObject(), this, aInput)),
      mStream(aStream) {
  mInput->Init(aCx);
}

already_AddRefed<Promise> InputToReadableStreamAlgorithms::PullCallbackImpl(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  MOZ_ASSERT(aController.IsByte());
  ReadableStream* stream = aController.Stream();
  MOZ_ASSERT(stream);

  MOZ_DIAGNOSTIC_ASSERT(stream->Disturbed());

  MOZ_DIAGNOSTIC_ASSERT(!IsClosed());
  MOZ_ASSERT(!mPullPromise);
  mPullPromise = Promise::CreateInfallible(aController.GetParentObject());

  MOZ_DIAGNOSTIC_ASSERT(mInput);

  nsresult rv = mInput->AsyncWait(0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(aCx, stream, rv);
    return nullptr;
  }

  // All good.
  return do_AddRef(mPullPromise);
}

// _BOUNDARY because OnInputStreamReady doesn't have [can-run-script]
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
InputToReadableStreamAlgorithms::OnInputStreamReady(
    nsIAsyncInputStream* aStream) {
  MOZ_DIAGNOSTIC_ASSERT(aStream);

  // Already closed. We have nothing else to do here.
  if (IsClosed()) {
    return NS_OK;
  }

  AutoEntryScript aes(mStream->GetParentObject(),
                      "InputToReadableStream data available");

  MOZ_DIAGNOSTIC_ASSERT(mInput);

  JSContext* cx = aes.cx();

  uint64_t size = 0;
  nsresult rv = mInput->Available(&size);
  MOZ_ASSERT_IF(NS_SUCCEEDED(rv), size > 0);

  // No warning for stream closed.
  if (rv == NS_BASE_STREAM_CLOSED || NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagation(cx, mStream, rv);
    return NS_OK;
  }

  // Not having a promise means we are pinged by stream closure
  // (WAIT_CLOSURE_ONLY below), but here we still have more data to read. Let's
  // wait for the next read request in that case.
  if (!mPullPromise) {
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(mPullPromise->State() ==
                        Promise::PromiseState::Pending);

  ErrorResult errorResult;
  PullFromInputStream(cx, size, errorResult);
  errorResult.WouldReportJSException();
  if (errorResult.Failed()) {
    ErrorPropagation(cx, mStream, errorResult.StealNSResult());
    return NS_OK;
  }

  // PullFromInputStream can fulfill read request, which can trigger read
  // request chunk steps, which again may execute JS. But it should be still
  // safe from cycle collection as the caller nsIAsyncInputStream should hold
  // the reference of `this`.
  //
  // That said, it's generally good to be cautious as there's no guarantee that
  // the interface is implemented in the safest way.
  MOZ_DIAGNOSTIC_ASSERT(mPullPromise);
  if (mPullPromise) {
    mPullPromise->MaybeResolveWithUndefined();
    mPullPromise = nullptr;
  }

  MOZ_DIAGNOSTIC_ASSERT(mInput);
  if (mInput) {
    // Subscribe WAIT_CLOSURE_ONLY so that OnInputStreamReady can be called when
    // mInput is closed.
    rv = mInput->AsyncWait(nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0,
                           mOwningEventTarget);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ErrorPropagation(cx, mStream, errorResult.StealNSResult());
      return NS_OK;
    }
  }

  return NS_OK;
}

void InputToReadableStreamAlgorithms::WriteIntoReadRequestBuffer(
    JSContext* aCx, ReadableStream* aStream, JS::Handle<JSObject*> aBuffer,
    uint32_t aLength, uint32_t* aByteWritten, ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(aBuffer);
  MOZ_DIAGNOSTIC_ASSERT(aByteWritten);
  MOZ_DIAGNOSTIC_ASSERT(mInput);
  MOZ_DIAGNOSTIC_ASSERT(!IsClosed());
  MOZ_DIAGNOSTIC_ASSERT(mPullPromise->State() ==
                        Promise::PromiseState::Pending);

  uint32_t written;
  nsresult rv;
  void* buffer;
  {
    // Bug 1754513: Hazard suppression.
    //
    // Because mInput->Read is detected as possibly GCing by the
    // current state of our static hazard analysis, we need to do the
    // suppression here. This can be removed with future improvements
    // to the static analysis.
    JS::AutoSuppressGCAnalysis suppress;
    JS::AutoCheckCannotGC noGC;
    bool isSharedMemory;

    buffer = JS_GetArrayBufferViewData(aBuffer, &isSharedMemory, noGC);
    MOZ_ASSERT(!isSharedMemory);

    rv = mInput->Read(static_cast<char*>(buffer), aLength, &written);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return;
    }
  }

  *aByteWritten = written;

  if (written == 0) {
    // If bytesWritten is zero, then the stream has been closed; return rather
    // than enqueueing a chunk filled with zeros.
    aRv.Throw(NS_BASE_STREAM_CLOSED);
    return;
  }

  // All good.
}

// https://streams.spec.whatwg.org/#readablestream-pull-from-bytes
// This is a ReadableStream algorithm but will probably be used solely in
// InputToReadableStreamAlgorithms.
void InputToReadableStreamAlgorithms::PullFromInputStream(JSContext* aCx,
                                                          uint64_t aAvailable,
                                                          ErrorResult& aRv) {
  // Step 1. Assert: stream.[[controller]] implements
  // ReadableByteStreamController.
  MOZ_ASSERT(mStream->Controller()->IsByte());

  // Step 2. Let available be bytes’s length. (aAvailable)
  // Step 3. Let desiredSize be available.
  uint64_t desiredSize = aAvailable;

  // Step 4. If stream’s current BYOB request view is non-null, then set
  // desiredSize to stream’s current BYOB request view's byte length.
  JS::Rooted<JSObject*> byobView(aCx);
  mStream->GetCurrentBYOBRequestView(aCx, &byobView, aRv);
  if (aRv.Failed()) {
    return;
  }
  if (byobView) {
    desiredSize = JS_GetArrayBufferViewByteLength(byobView);
  }

  // Step 5. Let pullSize be the smaller value of available and desiredSize.
  //
  // To avoid OOMing up on huge amounts of available data on a 32 bit system,
  // as well as potentially overflowing nsIInputStream's Read method's
  // parameter, let's limit our maximum chunk size to 256MB.
  //
  // (Note that nsIInputStream uses uint64_t for Available and uint32_t for
  // Read.)
  uint64_t pullSize = std::min(static_cast<uint64_t>(256 * 1024 * 1024),
                               std::min(aAvailable, desiredSize));

  // Step 6. Let pulled be the first pullSize bytes of bytes.
  // Step 7. Remove the first pullSize bytes from bytes.
  //
  // We do this in step 8 and 9, as we don't have a direct access to the data
  // but need to let nsIInputStream to write into the view.

  // Step 8. If stream’s current BYOB request view is non-null, then:
  if (byobView) {
    // Step 8.1. Write pulled into stream’s current BYOB request view.
    uint32_t bytesWritten = 0;
    WriteIntoReadRequestBuffer(aCx, mStream, byobView, pullSize, &bytesWritten,
                               aRv);
    if (aRv.Failed()) {
      return;
    }

    // Step 8.2. Perform ?
    // ReadableByteStreamControllerRespond(stream.[[controller]], pullSize).
    //
    // But we do not use pullSize but use byteWritten here, since nsIInputStream
    // does not guarantee to read as much as it told in Available().
    MOZ_DIAGNOSTIC_ASSERT(pullSize == bytesWritten);
    ReadableByteStreamControllerRespond(
        aCx, MOZ_KnownLive(mStream->Controller()->AsByte()), bytesWritten, aRv);
  }
  // Step 9. Otherwise,
  else {
    // Step 9.1. Set view to the result of creating a Uint8Array from pulled in
    // stream’s relevant Realm.
    UniquePtr<uint8_t[], JS::FreePolicy> buffer(
        static_cast<uint8_t*>(JS_malloc(aCx, pullSize)));
    if (!buffer) {
      aRv.ThrowTypeError("Out of memory");
      return;
    }

    uint32_t bytesWritten = 0;
    nsresult rv = mInput->Read((char*)buffer.get(), pullSize, &bytesWritten);
    if (!bytesWritten) {
      rv = NS_BASE_STREAM_CLOSED;
    }
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }

    MOZ_DIAGNOSTIC_ASSERT(pullSize == bytesWritten);
    JS::Rooted<JSObject*> view(aCx, nsJSUtils::MoveBufferAsUint8Array(
                                        aCx, bytesWritten, std::move(buffer)));
    if (!view) {
      JS_ClearPendingException(aCx);
      aRv.ThrowTypeError("Out of memory");
      return;
    }

    // Step 9.2. Perform ?
    // ReadableByteStreamControllerEnqueue(stream.[[controller]], view).
    ReadableByteStreamControllerEnqueue(
        aCx, MOZ_KnownLive(mStream->Controller()->AsByte()), view, aRv);
  }
}

void InputToReadableStreamAlgorithms::CloseAndReleaseObjects(
    JSContext* aCx, ReadableStream* aStream) {
  MOZ_DIAGNOSTIC_ASSERT(!IsClosed());

  ReleaseObjects();

  if (aStream->State() == ReadableStream::ReaderState::Readable) {
    IgnoredErrorResult rv;
    aStream->CloseNative(aCx, rv);
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to Close Stream");
  }
}

void InputToReadableStreamAlgorithms::ReleaseObjects() {
  if (mInput) {
    mInput->CloseWithStatus(NS_BASE_STREAM_CLOSED);
    mInput->Shutdown();
    mInput = nullptr;
  }

  // It's okay to leave a potentially unsettled promise as-is as this is only
  // used to prevent reentrant to PullCallback. CloseNative() or ErrorNative()
  // will settle the read requests for us.
  mPullPromise = nullptr;
}

void InputToReadableStreamAlgorithms::ErrorPropagation(JSContext* aCx,
                                                       ReadableStream* aStream,
                                                       nsresult aError) {
  // Nothing to do.
  if (IsClosed()) {
    return;
  }

  // Let's close the stream.
  if (aError == NS_BASE_STREAM_CLOSED) {
    CloseAndReleaseObjects(aCx, aStream);
    return;
  }

  // Let's use a generic error.
  ErrorResult rv;
  // XXXbz can we come up with a better error message here to tell the
  // consumer what went wrong?
  rv.ThrowTypeError("Error in input stream");

  JS::Rooted<JS::Value> errorValue(aCx);
  bool ok = ToJSValue(aCx, std::move(rv), &errorValue);
  MOZ_RELEASE_ASSERT(ok, "ToJSValue never fails for ErrorResult");

  {
    // This will be ignored if it's already errored.
    IgnoredErrorResult rv;
    aStream->ErrorNative(aCx, errorValue, rv);
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to error InputToReadableStream");
  }

  MOZ_ASSERT(IsClosed());
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    NonAsyncInputToReadableStreamAlgorithms, UnderlyingSourceAlgorithmsWrapper)
NS_IMPL_CYCLE_COLLECTION_INHERITED(NonAsyncInputToReadableStreamAlgorithms,
                                   UnderlyingSourceAlgorithmsWrapper,
                                   mAsyncAlgorithms)

already_AddRefed<Promise>
NonAsyncInputToReadableStreamAlgorithms::PullCallbackImpl(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  if (!mAsyncAlgorithms) {
    nsCOMPtr<nsIAsyncInputStream> asyncStream;

    // NS_MakeAsyncNonBlockingInputStream may immediately start a stream read
    // via nsInputStreamTransport::OpenInputStream, which is why this should be
    // called on a pull callback instead of in the constructor.
    nsresult rv = NS_MakeAsyncNonBlockingInputStream(
        mInput.forget(), getter_AddRefs(asyncStream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return nullptr;
    }

    mAsyncAlgorithms = MakeRefPtr<InputToReadableStreamAlgorithms>(
        aCx, asyncStream, aController.Stream());
  }

  MOZ_ASSERT(!mInput);
  return mAsyncAlgorithms->PullCallbackImpl(aCx, aController, aRv);
}

}  // namespace mozilla::dom
