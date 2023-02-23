/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebTransportStreams.h"

#include "mozilla/dom/WebTransportLog.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/WebTransport.h"
#include "mozilla/dom/WebTransportReceiveStream.h"
#include "mozilla/dom/WebTransportSendStream.h"
#include "mozilla/Result.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(WebTransportIncomingStreamsAlgorithms,
                                   UnderlyingSourceAlgorithmsWrapper,
                                   mTransport, mCallback)
NS_IMPL_ADDREF_INHERITED(WebTransportIncomingStreamsAlgorithms,
                         UnderlyingSourceAlgorithmsWrapper)
NS_IMPL_RELEASE_INHERITED(WebTransportIncomingStreamsAlgorithms,
                          UnderlyingSourceAlgorithmsWrapper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTransportIncomingStreamsAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceAlgorithmsWrapper)

WebTransportIncomingStreamsAlgorithms::WebTransportIncomingStreamsAlgorithms(
    StreamType aUnidirectional, WebTransport* aTransport)
    : mUnidirectional(aUnidirectional), mTransport(aTransport) {}

WebTransportIncomingStreamsAlgorithms::
    ~WebTransportIncomingStreamsAlgorithms() = default;

already_AddRefed<Promise>
WebTransportIncomingStreamsAlgorithms::PullCallbackImpl(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  // https://w3c.github.io/webtransport/#pullbidirectionalstream and
  // https://w3c.github.io/webtransport/#pullunidirectionalstream

  // Step 1: If transport.[[State]] is "connecting", then return the result
  // of performing the following steps upon fulfillment of
  // transport.[[Ready]]:

  // We don't explicitly check mState here, since we'll reject
  // mIncomingStreamPromise if we go to FAILED or CLOSED
  //
  // Step 2: Let session be transport.[[Session]].
  // Step 3: Let p be a new promise.
  RefPtr<Promise> promise =
      Promise::CreateInfallible(mTransport->GetParentObject());
  RefPtr<WebTransportIncomingStreamsAlgorithms> self(this);
  // The real work of PullCallback()
  // Step 5: Wait until there is an available incoming unidirectional stream.
  if (mTransport->mUnidirectionalStreams.GetSize() == 0) {
    // We need to wait.
    // Per
    // https://streams.spec.whatwg.org/#readablestreamdefaultcontroller-pulling
    // we can't be called again until the promise is resolved
    MOZ_ASSERT(!mCallback);
    mCallback = promise;

    LOG(("Incoming%sDirectionalStreams Pull waiting for a stream",
         mUnidirectional == StreamType::Unidirectional ? "Uni" : "Bi"));
    Result<RefPtr<Promise>, nsresult> returnResult =
        promise->ThenWithCycleCollectedArgs(
            [](JSContext* aCx, JS::Handle<JS::Value>, ErrorResult& aRv,
               RefPtr<WebTransportIncomingStreamsAlgorithms> self,
               RefPtr<Promise> aPromise) -> already_AddRefed<Promise> {
              self->BuildStream(aCx, aRv);
              return nullptr;
            },
            self, promise);
    if (returnResult.isErr()) {
      // XXX Reject?
      aRv.Throw(returnResult.unwrapErr());
      return nullptr;
    }
    // Step 4: Return p and run the remaining steps in parallel.
    return returnResult.unwrap().forget();
  }
  self->BuildStream(aCx, aRv);
  // Step 4: Return p and run the remaining steps in parallel.
  return promise.forget();
}

void WebTransportIncomingStreamsAlgorithms::BuildStream(JSContext* aCx,
                                                        ErrorResult& aRv) {
  // https://w3c.github.io/webtransport/#pullbidirectionalstream and
  // https://w3c.github.io/webtransport/#pullunidirectionalstream
  LOG(("Incoming%sDirectionalStreams Pull building a stream",
       mUnidirectional == StreamType::Unidirectional ? "Uni" : "Bi"));
  if (mUnidirectional == StreamType::Unidirectional) {
    // Step 6: Let internalStream be the result of receiving an incoming
    // unidirectional stream.
    RefPtr<DataPipeReceiver> pipe = mTransport->mUnidirectionalStreams.Pop();

    // Step 7.1: Let stream be the result of creating a
    // WebTransportReceiveStream with internalStream and transport
    RefPtr<WebTransportReceiveStream> readableStream =
        WebTransportReceiveStream::Create(mTransport, mTransport->mGlobal, pipe,
                                          aRv);
    if (MOZ_UNLIKELY(!readableStream)) {
      aRv.ThrowUnknownError("Internal error");
      return;
    }
    // Step 7.2 Enqueue stream to transport.[[IncomingUnidirectionalStreams]].
    JS::Rooted<JS::Value> jsStream(aCx);
    if (MOZ_UNLIKELY(!ToJSValue(aCx, readableStream, &jsStream))) {
      aRv.ThrowUnknownError("Internal error");
      return;
    }
    // EnqueueNative is CAN_RUN_SCRIPT
    RefPtr<ReadableStream> incomingStream =
        mTransport->mIncomingUnidirectionalStreams;
    incomingStream->EnqueueNative(aCx, jsStream, aRv);
    if (MOZ_UNLIKELY(aRv.Failed())) {
      aRv.ThrowUnknownError("Internal error");
      return;
    }
  } else {
    // Step 6: Let internalStream be the result of receiving a bidirectional
    // stream
    UniquePtr<Tuple<RefPtr<DataPipeReceiver>, RefPtr<DataPipeSender>>> pipes(
        mTransport->mBidirectionalStreams.Pop());

    // Step 7.1: Let stream be the result of creating a
    // WebTransportBidirectionalStream with internalStream and transport

    // Step 7.2 Enqueue stream to transport.[[IncomingBidirectionalStreams]].
    // Add to ReceiveStreams
    // mTransport->mReceiveStreams.AppendElement(stream);
  }
  // Step 7.3: Resolve p with undefined.
}

void WebTransportIncomingStreamsAlgorithms::NotifyIncomingStream() {
  if (mUnidirectional == StreamType::Unidirectional) {
    LOG(("NotifyIncomingStream: %zu Unidirectional ",
         mTransport->mUnidirectionalStreams.GetSize()));
    auto number = mTransport->mUnidirectionalStreams.GetSize();
    MOZ_ASSERT(number > 0);
#endif
    RefPtr<Promise> promise = mCallback.forget();
    if (promise) {
      promise->MaybeResolveWithUndefined();
    }
  } else {
    LOG(("NotifyIncomingStream: %zu Bidirectional ",
         mTransport->mBidirectionalStreams.GetSize()));
    auto number = mTransport->mBidirectionalStreams.GetSize();
    MOZ_ASSERT(number > 0);
#endif
    RefPtr<Promise> promise = mCallback.forget();
    if (promise) {
      promise->MaybeResolveWithUndefined();
    }
  }
}

void WebTransportIncomingStreamsAlgorithms::NotifyRejectAll() {
  // cancel all pulls
  LOG(("Cancel all WebTransport Pulls"));
  // Ensure we clear the callback before resolving/rejecting it
  if (RefPtr<Promise> promise = mCallback.forget()) {
    promise->MaybeReject(NS_ERROR_FAILURE);
  }
}

}  // namespace mozilla::dom
