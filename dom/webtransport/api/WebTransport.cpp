/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransport.h"

#include "nsUTF8Utils.h"
#include "nsIURL.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PWebTransport.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/WebTransportDatagramDuplexStream.h"
#include "mozilla/dom/WebTransportError.h"
#include "mozilla/dom/WebTransportStreams.h"
#include "mozilla/dom/WebTransportLog.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebTransport, mGlobal,
                                      mIncomingUnidirectionalStreams,
                                      mIncomingBidirectionalStreams,
                                      mSendStreams, mReceiveStreams, mDatagrams,
                                      mReady, mClosed)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebTransport)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebTransport)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTransport)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

WebTransport::WebTransport(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal),
      mState(WebTransportState::CONNECTING),
      mReliability(WebTransportReliabilityMode::Pending) {
  LOG(("Creating WebTransport %p", this));
}

WebTransport::~WebTransport() {
  // Should be empty by this point, because we should always have run cleanup:
  // https://w3c.github.io/webtransport/#webtransport-procedures
  MOZ_ASSERT(mSendStreams.IsEmpty());
  MOZ_ASSERT(mReceiveStreams.IsEmpty());
  // If this WebTransport was destroyed without being closed properly, make
  // sure to clean up the channel.
  // Since child has a raw ptr to us, we MUST call Shutdown() before we're
  // destroyed
  if (mChild) {
    mChild->Shutdown();
  }
}

// From parent
void WebTransport::NewBidirectionalStream(
    const RefPtr<mozilla::ipc::DataPipeReceiver>& aIncoming,
    const RefPtr<mozilla::ipc::DataPipeSender>& aOutgoing) {
  // XXX
}

void WebTransport::NewUnidirectionalStream(
    const RefPtr<mozilla::ipc::DataPipeReceiver>& aStream) {
  // Create a Unidirectional stream and push it into the
  // IncomingUnidirectionalStreams stream. Must be added to the ReceiveStreams
  // array
  //    RefPtr<ReadableStream> stream = CreateReadableByteStream(cx, global,
  //    algorithm, aRV);
}

// WebIDL Boilerplate

nsIGlobalObject* WebTransport::GetParentObject() const { return mGlobal; }

JSObject* WebTransport::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return WebTransport_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

/* static */
already_AddRefed<WebTransport> WebTransport::Constructor(
    const GlobalObject& aGlobal, const nsAString& aURL,
    const WebTransportOptions& aOptions, ErrorResult& aError) {
  LOG(("Creating WebTransport for %s", NS_ConvertUTF16toUTF8(aURL).get()));

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<WebTransport> result = new WebTransport(global);
  result->Init(aGlobal, aURL, aOptions, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  return result.forget();
}

void WebTransport::Init(const GlobalObject& aGlobal, const nsAString& aURL,
                        const WebTransportOptions& aOptions,
                        ErrorResult& aError) {
  // https://w3c.github.io/webtransport/#webtransport-constructor
  // Initiate connection with parent
  using mozilla::ipc::BackgroundChild;
  using mozilla::ipc::Endpoint;
  using mozilla::ipc::PBackgroundChild;

  // https://w3c.github.io/webtransport/#webtransport-constructor
  // Steps 1-4: Parse string for validity and Throw a SyntaxError if it isn't
  // Let parsedURL be the URL record resulting from parsing url.
  // If parsedURL is a failure, throw a SyntaxError exception.
  // If parsedURL scheme is not https, throw a SyntaxError exception.
  // If parsedURL fragment is not null, throw a SyntaxError exception.
  if (!ParseURL(aURL)) {
    aError.ThrowSyntaxError("Invalid WebTransport URL");
    return;
  }
  // Step 5: Let allowPooling be options's allowPooling if it exists, and false
  // otherwise.
  // Step 6: Let dedicated be the negation of allowPooling.
  bool dedicated = !aOptions.mAllowPooling;
  // Step 7: Let serverCertificateHashes be options's serverCertificateHashes if
  // it exists, and null otherwise.
  // Step 8: If dedicated is false and serverCertificateHashes is non-null,
  // then throw a TypeError.
  if (aOptions.mServerCertificateHashes.WasPassed()) {
    // XXX bug 1806693
    aError.ThrowNotSupportedError("No support for serverCertificateHashes yet");
    // XXX if dedicated is false and serverCertificateHashes is non-null, then
    // throw a TypeError. Also should enforce in parent
    return;
  }
  // Step 9: Let requireUnreliable be options's requireUnreliable.
  bool requireUnreliable = aOptions.mRequireUnreliable;
  // Step 10: Let congestionControl be options's congestionControl.
  // Step 11: If congestionControl is not "default", and the user agent
  // does not support any congestion control algorithms that optimize for
  // congestionControl, as allowed by [RFC9002] section 7, then set
  // congestionControl to "default".
  WebTransportCongestionControl congestionControl =
      WebTransportCongestionControl::Default;  // aOptions.mCongestionControl;
  // Set this to 'default' until we add congestion control setting

  // Setup up WebTransportDatagramDuplexStream
  // Step 12: Let incomingDatagrams be a new ReadableStream.
  // Step 13: Let outgoingDatagrams be a new WritableStream.
  // Step 14: Let datagrams be the result of creating a
  // WebTransportDatagramDuplexStream, its readable set to
  // incomingDatagrams and its writable set to outgoingDatagrams.

  // XXX TODO

  // Step 15 Let transport be a newly constructed WebTransport object, with:
  // SendStreams: empty ordered set
  // ReceiveStreams: empty ordered set
  // Ready: new promise
  mReady = Promise::Create(mGlobal, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  // Closed: new promise
  mClosed = Promise::Create(mGlobal, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  PBackgroundChild* backgroundChild =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundChild)) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = nsContentUtils::GetSystemPrincipal();
  // Create a new IPC connection
  Endpoint<PWebTransportParent> parentEndpoint;
  Endpoint<PWebTransportChild> childEndpoint;
  MOZ_ALWAYS_SUCCEEDS(
      PWebTransport::CreateEndpoints(&parentEndpoint, &childEndpoint));

  RefPtr<WebTransportChild> child = new WebTransportChild(this);
  if (!childEndpoint.Bind(child)) {
    return;
  }

  mState = WebTransportState::CONNECTING;

  JSContext* cx = aGlobal.Context();
  // Set up Datagram streams
  // Step 16: Let pullDatagramsAlgorithm be an action that runs pullDatagrams
  // with transport.
  // Step 17: Let writeDatagramsAlgorithm be an action that runs writeDatagrams
  // with transport.
  // Step 18: Set up incomingDatagrams with pullAlgorithm set to
  // pullDatagramsAlgorithm, and highWaterMark set to 0.
  // Step 19: Set up outgoingDatagrams with writeAlgorithm set to
  // writeDatagramsAlgorithm.

  // XXX TODO

  // Step 20: Let pullBidirectionalStreamAlgorithm be an action that runs
  // pullBidirectionalStream with transport.
  // Step 21: Set up transport.[[IncomingBidirectionalStreams]] with
  // pullAlgorithm set to pullBidirectionalStreamAlgorithm, and highWaterMark
  // set to 0.
  Optional<JS::Handle<JSObject*>> underlying;
  // Suppress warnings about risk of mGlobal getting nulled during script.
  // We set the global from the aGlobalObject parameter of the constructor, so
  // it must still be set here.
  const nsCOMPtr<nsIGlobalObject> global(mGlobal);
  // Used to implement the "wait until" aspects of the pull algorithm
  mIncomingBidirectionalPromise = Promise::Create(mGlobal, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  RefPtr<WebTransportIncomingStreamsAlgorithms> algorithm =
      new WebTransportIncomingStreamsAlgorithms(mIncomingBidirectionalPromise,
                                                false, this);

  mIncomingBidirectionalStreams = CreateReadableStream(
      cx, global, algorithm, Some(0.0), nullptr, aError);  // XXX
  if (aError.Failed()) {
    return;
  }
  // Step 22: Let pullUnidirectionalStreamAlgorithm be an action that runs
  // pullUnidirectionalStream with transport.
  // Step 23: Set up transport.[[IncomingUnidirectionalStreams]] with
  // pullAlgorithm set to pullUnidirectionalStreamAlgorithm, and highWaterMark
  // set to 0.

  // Used to implement the "wait until" aspects of the pull algorithm
  mIncomingUnidirectionalPromise = Promise::Create(mGlobal, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  algorithm = new WebTransportIncomingStreamsAlgorithms(
      mIncomingUnidirectionalPromise, true, this);

  mIncomingUnidirectionalStreams =
      CreateReadableStream(cx, global, algorithm, Some(0.0), nullptr, aError);
  if (aError.Failed()) {
    return;
  }

  // Step 24: Initialize WebTransport over HTTP with transport, parsedURL,
  // dedicated, requireUnreliable, and congestionControl.
  LOG(("Connecting WebTransport to parent for %s",
       NS_ConvertUTF16toUTF8(aURL).get()));

  // https://w3c.github.io/webtransport/#webtransport-constructor Spec 5.2
  backgroundChild
      ->SendCreateWebTransportParent(aURL, principal, dedicated,
                                     requireUnreliable,
                                     (uint32_t)congestionControl,
                                     // XXX serverCertHashes,
                                     std::move(parentEndpoint))
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this},
              child](PBackgroundChild::CreateWebTransportParentPromise::
                         ResolveOrRejectValue&& aResult) {
               // aResult is a Tuple<nsresult, uint8_t>
               // TODO: is there a better/more-spec-compliant error in the
               // reject case? Which begs the question, why would we get a
               // reject?
               nsresult rv = aResult.IsReject()
                                 ? NS_ERROR_FAILURE
                                 : Get<0>(aResult.ResolveValue());
               if (NS_FAILED(rv)) {
                 self->RejectWaitingConnection(rv);
               } else {
                 // This will process anything waiting for the connection to
                 // complete;

                 // Step 25 Return transport
                 self->ResolveWaitingConnection(
                     static_cast<WebTransportReliabilityMode>(
                         Get<1>(aResult.ResolveValue())),
                     child);
               }
             });
}

void WebTransport::ResolveWaitingConnection(
    WebTransportReliabilityMode aReliability, WebTransportChild* aChild) {
  LOG(("Resolved Connection %p, reliability = %u", this,
       (unsigned)aReliability));

  MOZ_ASSERT(mState == WebTransportState::CONNECTING);
  mChild = aChild;
  mState = WebTransportState::CONNECTED;
  mReliability = aReliability;

  mReady->MaybeResolve(true);
}

void WebTransport::RejectWaitingConnection(nsresult aRv) {
  LOG(("Reject Connection %p", this));
  MOZ_ASSERT(mState == WebTransportState::CONNECTING);
  mState = WebTransportState::FAILED;
  LOG(("Rejected connection %x", (uint32_t)aRv));

  // https://w3c.github.io/webtransport/#webtransport-internal-slots
  // "Reliability returns "pending" until a connection is established" so
  // we leave it pending
  mReady->MaybeReject(aRv);
  // This will abort any pulls for IncomingBidirectional/UnidirectionalStreams
  mIncomingBidirectionalPromise->MaybeResolveWithUndefined();
  mIncomingUnidirectionalPromise->MaybeResolveWithUndefined();

  // We never set mChild, so we aren't holding a reference that blocks GC
  // (spec 5.8)
}

bool WebTransport::ParseURL(const nsAString& aURL) const {
  NS_ENSURE_TRUE(!aURL.IsEmpty(), false);

  // 5.4 = https://w3c.github.io/webtransport/#webtransport-constructor
  // 5.4 #1 and #2
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  NS_ENSURE_SUCCESS(rv, false);

  // 5.4 #3
  if (!uri->SchemeIs("https")) {
    return false;
  }

  // 5.4 #4 no fragments
  bool hasRef;
  rv = uri->GetHasRef(&hasRef);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && !hasRef, false);

  return true;
}

already_AddRefed<Promise> WebTransport::GetStats(ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

WebTransportReliabilityMode WebTransport::Reliability() { return mReliability; }

WebTransportCongestionControl WebTransport::CongestionControl() {
  // XXX not implemented
  return WebTransportCongestionControl::Default;
}

already_AddRefed<Promise> WebTransport::Closed() {
  ErrorResult error;
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), error);
  if (error.Failed()) {
    return nullptr;
  }
  promise->MaybeResolve(mState == WebTransportState::CLOSED);
  return promise.forget();
}

void WebTransport::Close(const WebTransportCloseInfo& aOptions) {
  LOG(("Close() called"));
  if (mState == WebTransportState::CONNECTED ||
      mState == WebTransportState::CONNECTING) {
    MOZ_ASSERT(mChild);
    LOG(("Sending Close"));
    // https://w3c.github.io/webtransport/#dom-webtransport-close
    // Step 6: "Let reasonString be the maximal code unit prefix of
    // closeInfo.reason where the length of the UTF-8 encoded prefix
    // doesn’t exceed 1024."
    // Take the maximal "code unit prefix" of mReason and limit to 1024 bytes

    if (aOptions.mReason.Length() > 1024u) {
      // We want to start looking for the previous code point at one past the
      // limit, since if a code point ends exactly at the specified length, the
      // next byte will be the start of a new code point. Note
      // RewindToPriorUTF8Codepoint doesn't reduce the index if it points to the
      // start of a code point. We know reason[1024] is accessible since
      // Length() > 1024
      mChild->SendClose(
          aOptions.mCloseCode,
          Substring(aOptions.mReason, 0,
                    RewindToPriorUTF8Codepoint(aOptions.mReason.get(), 1024u)));
    } else {
      mChild->SendClose(aOptions.mCloseCode, aOptions.mReason);
    }
    mState = WebTransportState::CLOSED;

    // This will abort any pulls for IncomingBidirectional/UnidirectionalStreams
    mIncomingBidirectionalPromise->MaybeResolveWithUndefined();   // XXX
    mIncomingUnidirectionalPromise->MaybeResolveWithUndefined();  // XXX

    // The other side will call `Close()` for us now, make sure we don't call it
    // in our destructor.
    // This also causes IPC to drop the reference to us, allowing us to be
    // GC'd (spec 5.8)
    mChild->Shutdown();
    mChild = nullptr;
  }
}

already_AddRefed<WebTransportDatagramDuplexStream> WebTransport::Datagrams() {
  LOG(("Datagrams() called"));
  // XXX not implemented
  return nullptr;
}

already_AddRefed<Promise> WebTransport::CreateBidirectionalStream(
    ErrorResult& aError) {
  LOG(("CreateBidirectionalStream() called"));
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<ReadableStream> WebTransport::IncomingBidirectionalStreams() {
  return do_AddRef(mIncomingBidirectionalStreams);
}

already_AddRefed<Promise> WebTransport::CreateUnidirectionalStream(
    ErrorResult& aError) {
  LOG(("CreateUnidirectionalStream() called"));
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<ReadableStream> WebTransport::IncomingUnidirectionalStreams() {
  return do_AddRef(mIncomingUnidirectionalStreams);
}

// Can be invoked with "error", "error, error, and true/false", or "error and
// closeInfo", but reason and abruptly are never used, and it does use closeinfo
void WebTransport::Cleanup(WebTransportError* aError,
                           const WebTransportCloseInfo* aCloseInfo,
                           ErrorResult& aRv) {
  // https://w3c.github.io/webtransport/#webtransport-cleanup
  // Step 1: Let sendStreams be a copy of transport.[[SendStreams]]
  // Step 2: Let receiveStreams be a copy of transport.[[ReceiveStreams]]
  // Step 3: Let ready be transport.[[Ready]]  -> (mReady)
  // Step 4: Let closed be transport.[[Closed]] -> (mClosed)
  // Step 5: Let incomingBidirectionalStreams be
  // transport.[[IncomingBidirectionalStreams]].
  // Step 6: Let incomingUnidirectionalStreams be
  // transport.[[IncomingUnidirectionalStreams]].
  // Step 7: Set transport.[[SendStreams]] to an empty set.
  // Step 8: Set transport.[[ReceiveStreams]] to an empty set.
  nsTArray<RefPtr<WritableStream>> sendStreams;
  sendStreams.SwapElements(mSendStreams);
  nsTArray<RefPtr<ReadableStream>> receiveStreams;
  receiveStreams.SwapElements(mReceiveStreams);

  // Step 9: If closeInfo is given, then set transport.[[State]] to "closed".
  // Otherwise, set transport.[[State]] to "failed".
  mState = aCloseInfo ? WebTransportState::CLOSED : WebTransportState::FAILED;

  // Step 10: For each sendStream in sendStreams, error sendStream with error.
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    aRv.ThrowUnknownError("Internal error");
    return;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> errorValue(cx);
  bool ok = ToJSValue(cx, aError, &errorValue);
  if (!ok) {
    aRv.ThrowUnknownError("Internal error");
    return;
  }

  // Ignoring errors if we're cleaning up; we don't want to stop
  for (const auto& stream : sendStreams) {
    RefPtr<WritableStreamDefaultController> controller = stream->Controller();
    WritableStreamDefaultControllerErrorIfNeeded(cx, controller, errorValue,
                                                 IgnoreErrors());
  }
  // Step 11: For each receiveStream in receiveStreams, error receiveStream with
  // error.
  for (const auto& stream : receiveStreams) {
    RefPtr<ReadableStreamDefaultController> controller =
        stream->Controller()->AsDefault();
    // XXX replace with ErrorNative() when bug 1809895 lands
    ReadableStreamDefaultControllerError(cx, controller, errorValue,
                                         IgnoreErrors());
  }
  // Step 12:
  if (aCloseInfo) {
    // 12.1: Resolve closed with closeInfo.
    mClosed->MaybeResolve(aCloseInfo);
    // 12.2: Assert: ready is settled.
    MOZ_ASSERT(mReady->State() != Promise::PromiseState::Pending);
    // 12.3: Close incomingBidirectionalStreams
    // This keeps the clang-plugin happy
    RefPtr<ReadableStream> stream = mIncomingBidirectionalStreams;
    stream->CloseNative(cx, IgnoreErrors());
    // 12.4: Close incomingUnidirectionalStreams
    stream = mIncomingUnidirectionalStreams;
    stream->CloseNative(cx, IgnoreErrors());
  } else {
    // Step 13
    // 13.1: Reject closed with error
    mClosed->MaybeReject(errorValue);
    // 13.2: Reject ready with error
    mReady->MaybeReject(errorValue);
    // 13.3: Error incomingBidirectionalStreams with error
    RefPtr<ReadableStreamDefaultController> controller =
        mIncomingBidirectionalStreams->Controller()->AsDefault();
    // XXX replace with ErrorNative() when bug 1809895 lands
    ReadableStreamDefaultControllerError(cx, controller, errorValue,
                                         IgnoreErrors());
    // 13.4: Error incomingUnidirectionalStreams with error
    controller = mIncomingUnidirectionalStreams->Controller()->AsDefault();
    ReadableStreamDefaultControllerError(cx, controller, errorValue,
                                         IgnoreErrors());
  }
  // abort any pending pulls from Incoming*Streams (not in spec)
  mIncomingUnidirectionalPromise->MaybeReject(errorValue);
  mIncomingBidirectionalPromise->MaybeReject(errorValue);
}

}  // namespace mozilla::dom
