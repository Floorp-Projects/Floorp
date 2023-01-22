/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebTransportStreams.h"

#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/WebTransport.h"
#include "mozilla/Result.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(WebTransportIncomingStreamsAlgorithms,
                                   UnderlyingSourceAlgorithmsWrapper, mStream)
NS_IMPL_ADDREF_INHERITED(WebTransportIncomingStreamsAlgorithms,
                         UnderlyingSourceAlgorithmsWrapper)
NS_IMPL_RELEASE_INHERITED(WebTransportIncomingStreamsAlgorithms,
                          UnderlyingSourceAlgorithmsWrapper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTransportIncomingStreamsAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceAlgorithmsWrapper)

WebTransportIncomingStreamsAlgorithms::
    ~WebTransportIncomingStreamsAlgorithms() = default;

already_AddRefed<Promise>
WebTransportIncomingStreamsAlgorithms::PullCallbackImpl(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  // https://w3c.github.io/webtransport/#pullbidirectionalstream and
  // https://w3c.github.io/webtransport/#pullunidirectionalstream
  // pullUnidirectionalStream Step 1: If WebTransport state is CONNECTING,
  // return a promise and resolve or reject on state change

  // We don't explicitly check Ready here, since we'll reject
  // mIncomingStreamPromise if we go to FAILED or CLOSED
  //
  // Step 2
  RefPtr<Promise> promise = Promise::Create(mStream->GetParentObject(), aRv);
  RefPtr<WebTransportIncomingStreamsAlgorithms> self(this);
  // The real work of PullCallback()
  // Wait until there's an incoming stream (or rejection)
  // Step 5
  Result<RefPtr<Promise>, nsresult> returnResult =
      mIncomingStreamPromise->ThenWithCycleCollectedArgs(
          [](JSContext* aCx, JS::Handle<JS::Value>, ErrorResult& aRv,
             const RefPtr<WebTransportIncomingStreamsAlgorithms>& self,
             RefPtr<Promise> newPromise) {
            Unused << self->mUnidirectional;
            // XXX Use self->mUnidirectional here
            // Step 6 Get new transport stream
            // Step 7.1 create stream using transport stream
            // Step 7.2 Enqueue
            // Add to ReceiveStreams
            // mStream->mReceiveStreams.AppendElement(stream);
            // Step 7.3
            newPromise->MaybeResolveWithUndefined();
            return newPromise.forget();
          },
          self, promise);
  if (returnResult.isErr()) {
    // XXX Reject?
    aRv.Throw(returnResult.unwrapErr());
    return nullptr;
  }
  // Step 4
  return returnResult.unwrap().forget();
}

// This will go away...
already_AddRefed<Promise>
WebTransportIncomingStreamsAlgorithms::CancelCallbackImpl(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  // XXXX wrong?
  return nullptr;  // Promise::CreateResolvedWithUndefined(mGlobal, aRv);
}

}  // namespace mozilla::dom
