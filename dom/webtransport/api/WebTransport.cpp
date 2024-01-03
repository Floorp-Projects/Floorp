/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransport.h"

#include "WebTransportBidirectionalStream.h"
#include "mozilla/RefPtr.h"
#include "nsUTF8Utils.h"
#include "nsIURL.h"
#include "nsIWebTransportStream.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PWebTransport.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/WebTransportDatagramDuplexStream.h"
#include "mozilla/dom/WebTransportError.h"
#include "mozilla/dom/WebTransportLog.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundChild.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebTransport)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebTransport)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIncomingUnidirectionalStreams)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIncomingBidirectionalStreams)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIncomingUnidirectionalAlgorithm)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIncomingBidirectionalAlgorithm)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDatagrams)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReady)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mClosed)
  for (const auto& hashEntry : tmp->mSendStreams.Values()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSendStreams entry item");
    cb.NoteXPCOMChild(hashEntry);
  }
  for (const auto& hashEntry : tmp->mReceiveStreams.Values()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mReceiveStreams entry item");
    cb.NoteXPCOMChild(hashEntry);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebTransport)
  tmp->mSendStreams.Clear();
  tmp->mReceiveStreams.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUnidirectionalStreams)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBidirectionalStreams)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIncomingUnidirectionalStreams)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIncomingBidirectionalStreams)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIncomingUnidirectionalAlgorithm)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIncomingBidirectionalAlgorithm)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDatagrams)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReady)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mClosed)
  if (tmp->mChild) {
    tmp->mChild->Shutdown(false);
    tmp->mChild = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

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
  LOG(("~WebTransport() for %p", this));
  MOZ_ASSERT(mSendStreams.IsEmpty());
  MOZ_ASSERT(mReceiveStreams.IsEmpty());
  // If this WebTransport was destroyed without being closed properly, make
  // sure to clean up the channel.
  // Since child has a raw ptr to us, we MUST call Shutdown() before we're
  // destroyed
  if (mChild) {
    mChild->Shutdown(true);
  }
}

// From parent
void WebTransport::NewBidirectionalStream(
    uint64_t aStreamId, const RefPtr<DataPipeReceiver>& aIncoming,
    const RefPtr<DataPipeSender>& aOutgoing) {
  LOG_VERBOSE(("NewBidirectionalStream()"));
  // Create a Bidirectional stream and push it into the
  // IncomingBidirectionalStreams stream. Must be added to the ReceiveStreams
  // and SendStreams arrays

  UniquePtr<BidirectionalPair> streams(
      new BidirectionalPair(aIncoming, aOutgoing));
  auto tuple = std::tuple<uint64_t, UniquePtr<BidirectionalPair>>(
      aStreamId, std::move(streams));
  mBidirectionalStreams.AppendElement(std::move(tuple));
  // We need to delete them all!

  // Notify something to wake up readers of IncomingReceiveStreams
  // The callback is always set/used from the same thread (MainThread or a
  // Worker thread).
  if (mIncomingBidirectionalAlgorithm) {
    RefPtr<WebTransportIncomingStreamsAlgorithms> callback =
        mIncomingBidirectionalAlgorithm;
    LOG(("NotifyIncomingStream"));
    callback->NotifyIncomingStream();
  }
}

void WebTransport::NewUnidirectionalStream(
    uint64_t aStreamId, const RefPtr<mozilla::ipc::DataPipeReceiver>& aStream) {
  LOG_VERBOSE(("NewUnidirectionalStream()"));
  // Create a Unidirectional stream and push it into the
  // IncomingUnidirectionalStreams stream. Must be added to the ReceiveStreams
  // array

  mUnidirectionalStreams.AppendElement(
      std::tuple<uint64_t, RefPtr<mozilla::ipc::DataPipeReceiver>>(aStreamId,
                                                                   aStream));
  // Notify something to wake up readers of IncomingReceiveStreams
  // The callback is always set/used from the same thread (MainThread or a
  // Worker thread).
  if (mIncomingUnidirectionalAlgorithm) {
    RefPtr<WebTransportIncomingStreamsAlgorithms> callback =
        mIncomingUnidirectionalAlgorithm;
    LOG(("NotifyIncomingStream"));
    callback->NotifyIncomingStream();
  }
}

void WebTransport::NewDatagramReceived(nsTArray<uint8_t>&& aData,
                                       const mozilla::TimeStamp& aTimeStamp) {
  mDatagrams->NewDatagramReceived(std::move(aData), aTimeStamp);
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
  // https://w3c.github.io/webtransport/#webtransport-constructor

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<WebTransport> result = new WebTransport(global);
  result->Init(aGlobal, aURL, aOptions, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Don't let this document go into BFCache
  result->NotifyToWindow(true);

  // Step 25 Return transport
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
  nsTArray<mozilla::ipc::WebTransportHash> aServerCertHashes;
  if (aOptions.mServerCertificateHashes.WasPassed()) {
    if (!dedicated) {
      aError.ThrowNotSupportedError(
          "serverCertificateHashes not supported for non-dedicated "
          "connections");
      return;
    }
    for (const auto& hash : aOptions.mServerCertificateHashes.Value()) {
      if (!hash.mAlgorithm.WasPassed() || !hash.mValue.WasPassed()) continue;

      if (hash.mAlgorithm.Value() != u"sha-256") {
        LOG(("Algorithms other than SHA-256 are not supported"));
        continue;
      }

      nsTArray<uint8_t> data;
      if (!AppendTypedArrayDataTo(hash.mValue.Value(), data)) {
        aError.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }

      nsCString alg = NS_ConvertUTF16toUTF8(hash.mAlgorithm.Value());
      aServerCertHashes.EmplaceBack(alg, data);
    }
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
  mDatagrams = new WebTransportDatagramDuplexStream(mGlobal, this);
  mDatagrams->Init(aError);
  if (aError.Failed()) {
    return;
  }

  // XXX TODO

  // Step 15 Let transport be a newly constructed WebTransport object, with:
  // SendStreams: empty ordered set
  // ReceiveStreams: empty ordered set
  // Ready: new promise
  mReady = Promise::CreateInfallible(mGlobal);

  // Closed: new promise
  mClosed = Promise::CreateInfallible(mGlobal);

  PBackgroundChild* backgroundChild =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundChild)) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = mGlobal->PrincipalOrNull();
  mozilla::Maybe<IPCClientInfo> ipcClientInfo;

  if (mGlobal->GetClientInfo().isSome()) {
    ipcClientInfo = mozilla::Some(mGlobal->GetClientInfo().ref().ToIPC());
  }
  // Create a new IPC connection
  Endpoint<PWebTransportParent> parentEndpoint;
  Endpoint<PWebTransportChild> childEndpoint;
  MOZ_ALWAYS_SUCCEEDS(
      PWebTransport::CreateEndpoints(&parentEndpoint, &childEndpoint));

  RefPtr<WebTransportChild> child = new WebTransportChild(this);
  if (NS_IsMainThread()) {
    if (!childEndpoint.Bind(child)) {
      return;
    }
  } else if (!childEndpoint.Bind(child, mGlobal->SerialEventTarget())) {
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

  mIncomingBidirectionalAlgorithm = new WebTransportIncomingStreamsAlgorithms(
      WebTransportIncomingStreamsAlgorithms::StreamType::Bidirectional, this);

  RefPtr<WebTransportIncomingStreamsAlgorithms> algorithm =
      mIncomingBidirectionalAlgorithm;
  mIncomingBidirectionalStreams = ReadableStream::CreateNative(
      cx, global, *algorithm, Some(0.0), nullptr, aError);
  if (aError.Failed()) {
    return;
  }
  // Step 22: Let pullUnidirectionalStreamAlgorithm be an action that runs
  // pullUnidirectionalStream with transport.
  // Step 23: Set up transport.[[IncomingUnidirectionalStreams]] with
  // pullAlgorithm set to pullUnidirectionalStreamAlgorithm, and highWaterMark
  // set to 0.

  mIncomingUnidirectionalAlgorithm = new WebTransportIncomingStreamsAlgorithms(
      WebTransportIncomingStreamsAlgorithms::StreamType::Unidirectional, this);

  algorithm = mIncomingUnidirectionalAlgorithm;
  mIncomingUnidirectionalStreams = ReadableStream::CreateNative(
      cx, global, *algorithm, Some(0.0), nullptr, aError);
  if (aError.Failed()) {
    return;
  }

  // Step 24: Initialize WebTransport over HTTP with transport, parsedURL,
  // dedicated, requireUnreliable, and congestionControl.
  LOG(("Connecting WebTransport to parent for %s",
       NS_ConvertUTF16toUTF8(aURL).get()));

  // https://w3c.github.io/webtransport/#webtransport-constructor Spec 5.2
  mChild = child;
  backgroundChild
      ->SendCreateWebTransportParent(
          aURL, principal, ipcClientInfo, dedicated, requireUnreliable,
          (uint32_t)congestionControl, std::move(aServerCertHashes),
          std::move(parentEndpoint))
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}](
                 PBackgroundChild::CreateWebTransportParentPromise::
                     ResolveOrRejectValue&& aResult) {
               // aResult is a std::tuple<nsresult, uint8_t>
               // TODO: is there a better/more-spec-compliant error in the
               // reject case? Which begs the question, why would we get a
               // reject?
               nsresult rv = aResult.IsReject()
                                 ? NS_ERROR_FAILURE
                                 : std::get<0>(aResult.ResolveValue());
               LOG(("isreject: %d nsresult 0x%x", aResult.IsReject(),
                    (uint32_t)rv));
               if (NS_FAILED(rv)) {
                 self->RejectWaitingConnection(rv);
               } else {
                 // This will process anything waiting for the connection to
                 // complete;

                 self->ResolveWaitingConnection(
                     static_cast<WebTransportReliabilityMode>(
                         std::get<1>(aResult.ResolveValue())));
               }
             });
}

void WebTransport::ResolveWaitingConnection(
    WebTransportReliabilityMode aReliability) {
  LOG(("Resolved Connection %p, reliability = %u", this,
       (unsigned)aReliability));
  // https://w3c.github.io/webtransport/#webtransport-constructor
  // Step 17 of  initialize WebTransport over HTTP
  // Step 17.1 If transport.[[State]] is not "connecting":
  if (mState != WebTransportState::CONNECTING) {
    // Step 17.1.1: In parallel, terminate session.
    // Step 17.1.2: abort these steps
    // Cleanup should have been called, which means Ready has been rejected
    return;
  }

  // Step 17.2: Set transport.[[State]] to "connected".
  mState = WebTransportState::CONNECTED;
  // Step 17.3: Set transport.[[Session]] to session.
  // Step 17.4: Set transport’s [[Reliability]] to "supports-unreliable".
  mReliability = aReliability;

  mChild->SendGetMaxDatagramSize()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}](uint64_t&& aMaxDatagramSize) {
        MOZ_ASSERT(self->mDatagrams);
        self->mDatagrams->SetMaxDatagramSize(aMaxDatagramSize);
        LOG(("max datagram size for the session is %" PRIu64,
             aMaxDatagramSize));
      },
      [](const mozilla::ipc::ResponseRejectReason& aReason) {
        LOG(("WebTransport fetching maxDatagramSize failed"));
      });

  // Step 17.5: Resolve transport.[[Ready]] with undefined.
  mReady->MaybeResolveWithUndefined();

  // We can now release any queued datagrams
  mDatagrams->SetChild(mChild);
}

void WebTransport::RejectWaitingConnection(nsresult aRv) {
  LOG(("Rejected connection %p %x", this, (uint32_t)aRv));
  // https://w3c.github.io/webtransport/#initialize-webtransport-over-http

  // Step 10: If connection is failure, then abort the remaining steps and
  // queue a network task with transport to run these steps:
  // Step 10.1: If transport.[[State]] is "closed" or "failed", then abort
  // these steps.

  // Step 14: If the previous step fails, abort the remaining steps and
  // queue a network task with transport to run these steps:
  // Step 14.1: If transport.[[State]] is "closed" or "failed", then abort
  // these steps.
  if (mState == WebTransportState::CLOSED ||
      mState == WebTransportState::FAILED) {
    mChild->Shutdown(true);
    mChild = nullptr;
    // Cleanup should have been called, which means Ready has been
    // rejected and pulls resolved
    return;
  }

  // Step 14.2: Let error be the result of creating a WebTransportError with
  // "session".
  RefPtr<WebTransportError> error = new WebTransportError(
      "WebTransport connection rejected"_ns, WebTransportErrorSource::Session);
  // Step 14.3: Cleanup transport with error.
  Cleanup(error, nullptr, IgnoreErrors());

  mChild->Shutdown(true);
  mChild = nullptr;
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

void WebTransport::RemoteClosed(bool aCleanly, const uint32_t& aCode,
                                const nsACString& aReason) {
  LOG(("Server closed: cleanly: %d, code %u, reason %s", aCleanly, aCode,
       PromiseFlatCString(aReason).get()));
  // Step 2 of https://w3c.github.io/webtransport/#web-transport-termination
  // We calculate cleanly on the parent
  // Step 2.1: If transport.[[State]] is "closed" or "failed", abort these
  // steps.
  if (mState == WebTransportState::CLOSED ||
      mState == WebTransportState::FAILED) {
    return;
  }
  // Step 2.2: Let error be the result of creating a WebTransportError with
  // "session".
  RefPtr<WebTransportError> error = new WebTransportError(
      "remote WebTransport close"_ns, WebTransportErrorSource::Session);
  // Step 2.3: If cleanly is false, then cleanup transport with error, and
  // abort these steps.
  ErrorResult errorresult;
  if (!aCleanly) {
    Cleanup(error, nullptr, errorresult);
    return;
  }
  // Step 2.4: Let closeInfo be a new WebTransportCloseInfo.
  // Step 2.5: If code is given, set closeInfo’s closeCode to code.
  // Step 2.6: If reasonBytes is given, set closeInfo’s reason to reasonBytes,
  // UTF-8 decoded.
  WebTransportCloseInfo closeinfo;
  closeinfo.mCloseCode = aCode;
  closeinfo.mReason = aReason;

  // Step 2.7: Cleanup transport with error and closeInfo.
  Cleanup(error, &closeinfo, errorresult);
}

template <typename Stream>
void WebTransport::PropagateError(Stream* aStream, WebTransportError* aError) {
  ErrorResult rv;
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    rv.ThrowUnknownError("Internal error");
    return;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> errorValue(cx);
  bool ok = ToJSValue(cx, aError, &errorValue);
  if (!ok) {
    rv.ThrowUnknownError("Internal error");
    return;
  }

  aStream->ErrorNative(cx, errorValue, IgnoreErrors());
}

void WebTransport::OnStreamResetOrStopSending(
    uint64_t aStreamId, const StreamResetOrStopSendingError& aError) {
  LOG(("WebTransport::OnStreamResetOrStopSending %p id=%" PRIx64, this,
       aStreamId));
  if (aError.type() == StreamResetOrStopSendingError::TStopSendingError) {
    RefPtr<WebTransportSendStream> stream = mSendStreams.Get(aStreamId);
    if (!stream) {
      return;
    }
    uint8_t errorCode = net::GetWebTransportErrorFromNSResult(
        aError.get_StopSendingError().error());
    RefPtr<WebTransportError> error = new WebTransportError(
        "WebTransportStream StopSending"_ns, WebTransportErrorSource::Stream,
        Nullable<uint8_t>(errorCode));
    PropagateError(stream.get(), error);
  } else if (aError.type() == StreamResetOrStopSendingError::TResetError) {
    RefPtr<WebTransportReceiveStream> stream = mReceiveStreams.Get(aStreamId);
    LOG(("WebTransport::OnStreamResetOrStopSending reset %p stream=%p", this,
         stream.get()));
    if (!stream) {
      return;
    }
    uint8_t errorCode =
        net::GetWebTransportErrorFromNSResult(aError.get_ResetError().error());
    RefPtr<WebTransportError> error = new WebTransportError(
        "WebTransportStream Reset"_ns, WebTransportErrorSource::Stream,
        Nullable<uint8_t>(errorCode));
    PropagateError(stream.get(), error);
  }
}

void WebTransport::Close(const WebTransportCloseInfo& aOptions,
                         ErrorResult& aRv) {
  LOG(("Close() called"));
  // https://w3c.github.io/webtransport/#dom-webtransport-close
  // Step 1 and Step 2: If transport.[[State]] is "closed" or "failed", then
  // abort these steps.
  if (mState == WebTransportState::CLOSED ||
      mState == WebTransportState::FAILED) {
    return;
  }
  // Step 3: If transport.[[State]] is "connecting":
  if (mState == WebTransportState::CONNECTING) {
    // Step 3.1: Let error be the result of creating a WebTransportError with
    // "session".
    RefPtr<WebTransportError> error = new WebTransportError(
        "close() called on WebTransport while connecting"_ns,
        WebTransportErrorSource::Session);
    // Step 3.2: Cleanup transport with error.
    Cleanup(error, nullptr, aRv);
    // Step 3.3: Abort these steps.
    mChild->Shutdown(true);
    mChild = nullptr;
    return;
  }
  LOG(("Sending Close"));
  MOZ_ASSERT(mChild);
  // Step 4: Let session be transport.[[Session]].
  // Step 5: Let code be closeInfo.closeCode.
  // Step 6: "Let reasonString be the maximal code unit prefix of
  // closeInfo.reason where the length of the UTF-8 encoded prefix
  // doesn’t exceed 1024."
  // Take the maximal "code unit prefix" of mReason and limit to 1024 bytes
  // Step 7: Let reason be reasonString, UTF-8 encoded.
  // Step 8: In parallel, terminate session with code and reason.
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
    LOG(("Close sent"));
  }

  // Step 9: Cleanup transport with AbortError and closeInfo. (sets mState to
  // Closed)
  RefPtr<WebTransportError> error =
      new WebTransportError("close()"_ns, WebTransportErrorSource::Session,
                            DOMException_Binding::ABORT_ERR);
  Cleanup(error, &aOptions, aRv);
  LOG(("Cleanup done"));

  // The other side will call `Close()` for us now, make sure we don't call it
  // in our destructor.
  mChild->Shutdown(false);
  mChild = nullptr;
  LOG(("Close done"));
}

already_AddRefed<WebTransportDatagramDuplexStream> WebTransport::GetDatagrams(
    ErrorResult& aError) {
  return do_AddRef(mDatagrams);
}

already_AddRefed<Promise> WebTransport::CreateBidirectionalStream(
    const WebTransportSendStreamOptions& aOptions, ErrorResult& aRv) {
  LOG(("CreateBidirectionalStream() called"));
  // https://w3c.github.io/webtransport/#dom-webtransport-createbidirectionalstream
  RefPtr<Promise> promise = Promise::CreateInfallible(GetParentObject());

  // Step 2: If transport.[[State]] is "closed" or "failed", return a new
  // rejected promise with an InvalidStateError.
  if (mState == WebTransportState::CLOSED ||
      mState == WebTransportState::FAILED || !mChild) {
    aRv.ThrowInvalidStateError("WebTransport closed or failed");
    return nullptr;
  }

  // Step 3: Let sendOrder be options's sendOrder.
  Maybe<int64_t> sendOrder;
  if (!aOptions.mSendOrder.IsNull()) {
    sendOrder = Some(aOptions.mSendOrder.Value());
  }
  // Step 4: Let p be a new promise.
  // Step 5: Run the following steps in parallel, but abort them whenever
  // transport’s [[State]] becomes "closed" or "failed", and instead queue
  // a network task with transport to reject p with an InvalidStateError.

  // Ask the parent to create the stream and send us the DataPipeSender/Receiver
  // pair
  mChild->SendCreateBidirectionalStream(
      sendOrder,
      [self = RefPtr{this}, sendOrder, promise](
          BidirectionalStreamResponse&& aPipes) MOZ_CAN_RUN_SCRIPT_BOUNDARY {
        LOG(("CreateBidirectionalStream response"));
        if (BidirectionalStreamResponse::Tnsresult == aPipes.type()) {
          promise->MaybeReject(aPipes.get_nsresult());
          return;
        }
        // Step 5.2.1: If transport.[[State]] is "closed" or "failed",
        // reject p with an InvalidStateError and abort these steps.
        if (BidirectionalStreamResponse::Tnsresult == aPipes.type()) {
          promise->MaybeReject(aPipes.get_nsresult());
          return;
        }
        if (self->mState == WebTransportState::CLOSED ||
            self->mState == WebTransportState::FAILED) {
          promise->MaybeRejectWithInvalidStateError(
              "Transport close/errored before CreateBidirectional finished");
          return;
        }
        uint64_t id = aPipes.get_BidirectionalStream().streamId();
        LOG(("Create WebTransportBidirectionalStream id=%" PRIx64, id));
        ErrorResult error;
        RefPtr<WebTransportBidirectionalStream> newStream =
            WebTransportBidirectionalStream::Create(
                self, self->mGlobal, id,
                aPipes.get_BidirectionalStream().inStream(),
                aPipes.get_BidirectionalStream().outStream(), sendOrder, error);
        LOG(("Returning a bidirectionalStream"));
        promise->MaybeResolve(newStream);
      },
      [self = RefPtr{this}, promise](mozilla::ipc::ResponseRejectReason) {
        LOG(("CreateBidirectionalStream reject"));
        promise->MaybeRejectWithInvalidStateError(
            "Transport close/errored before CreateBidirectional started");
      });

  // Step 6: return p
  return promise.forget();
}

already_AddRefed<ReadableStream> WebTransport::IncomingBidirectionalStreams() {
  return do_AddRef(mIncomingBidirectionalStreams);
}

already_AddRefed<Promise> WebTransport::CreateUnidirectionalStream(
    const WebTransportSendStreamOptions& aOptions, ErrorResult& aRv) {
  LOG(("CreateUnidirectionalStream() called"));
  // https://w3c.github.io/webtransport/#dom-webtransport-createunidirectionalstream
  // Step 2: If transport.[[State]] is "closed" or "failed", return a new
  // rejected promise with an InvalidStateError.
  if (mState == WebTransportState::CLOSED ||
      mState == WebTransportState::FAILED || !mChild) {
    aRv.ThrowInvalidStateError("WebTransport closed or failed");
    return nullptr;
  }

  // Step 3: Let sendOrder be options's sendOrder.
  Maybe<int64_t> sendOrder;
  if (!aOptions.mSendOrder.IsNull()) {
    sendOrder = Some(aOptions.mSendOrder.Value());
  }
  // Step 4: Let p be a new promise.
  RefPtr<Promise> promise = Promise::CreateInfallible(GetParentObject());

  // Step 5: Run the following steps in parallel, but abort them whenever
  // transport’s [[State]] becomes "closed" or "failed", and instead queue
  // a network task with transport to reject p with an InvalidStateError.

  // Ask the parent to create the stream and send us the DataPipeSender
  mChild->SendCreateUnidirectionalStream(
      sendOrder,
      [self = RefPtr{this}, sendOrder,
       promise](UnidirectionalStreamResponse&& aResponse)
          MOZ_CAN_RUN_SCRIPT_BOUNDARY {
            LOG(("CreateUnidirectionalStream response"));
            if (UnidirectionalStreamResponse::Tnsresult == aResponse.type()) {
              promise->MaybeReject(aResponse.get_nsresult());
              return;
            }
            // Step 5.1: Let internalStream be the result of creating an
            // outgoing unidirectional stream with transport.[[Session]].
            // Step 5.2: Queue a network task with transport to run the
            // following steps:
            // Step 5.2.1 If transport.[[State]] is "closed" or "failed",
            // reject p with an InvalidStateError and abort these steps.
            if (self->mState == WebTransportState::CLOSED ||
                self->mState == WebTransportState::FAILED ||
                aResponse.type() !=
                    UnidirectionalStreamResponse::TUnidirectionalStream) {
              promise->MaybeRejectWithInvalidStateError(
                  "Transport close/errored during CreateUnidirectional");
              return;
            }

            // Step 5.2.2.: Let stream be the result of creating a
            // WebTransportSendStream with internalStream, transport, and
            // sendOrder.
            ErrorResult error;
            uint64_t id = aResponse.get_UnidirectionalStream().streamId();
            LOG(("Create WebTransportSendStream id=%" PRIx64, id));
            RefPtr<WebTransportSendStream> writableStream =
                WebTransportSendStream::Create(
                    self, self->mGlobal, id,
                    aResponse.get_UnidirectionalStream().outStream(), sendOrder,
                    error);
            if (!writableStream) {
              promise->MaybeReject(std::move(error));
              return;
            }
            LOG(("Returning a writableStream"));
            // Step 5.2.3: Resolve p with stream.
            promise->MaybeResolve(writableStream);
          },
      [self = RefPtr{this}, promise](mozilla::ipc::ResponseRejectReason) {
        LOG(("CreateUnidirectionalStream reject"));
        promise->MaybeRejectWithInvalidStateError(
            "Transport close/errored during CreateUnidirectional");
      });

  // Step 6: return p
  return promise.forget();
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
  LOG(("Cleanup started"));
  nsTHashMap<uint64_t, RefPtr<WebTransportSendStream>> sendStreams;
  sendStreams.SwapElements(mSendStreams);
  nsTHashMap<uint64_t, RefPtr<WebTransportReceiveStream>> receiveStreams;
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

  for (const auto& stream : sendStreams.Values()) {
    // This MOZ_KnownLive is redundant, see bug 1620312
    MOZ_KnownLive(stream)->ErrorNative(cx, errorValue, IgnoreErrors());
  }
  // Step 11: For each receiveStream in receiveStreams, error receiveStream with
  // error.
  for (const auto& stream : receiveStreams.Values()) {
    stream->ErrorNative(cx, errorValue, IgnoreErrors());
  }
  // Step 12:
  if (aCloseInfo) {
    // 12.1: Resolve closed with closeInfo.
    LOG(("Resolving mClosed with closeinfo"));
    mClosed->MaybeResolve(*aCloseInfo);
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
    LOG(("Rejecting mClosed"));
    mClosed->MaybeReject(errorValue);
    // 13.2: Reject ready with error
    mReady->MaybeReject(errorValue);
    // 13.3: Error incomingBidirectionalStreams with error
    mIncomingBidirectionalStreams->ErrorNative(cx, errorValue, IgnoreErrors());
    // 13.4: Error incomingUnidirectionalStreams with error
    mIncomingUnidirectionalStreams->ErrorNative(cx, errorValue, IgnoreErrors());
  }
  // Let go of the algorithms
  mIncomingBidirectionalAlgorithm = nullptr;
  mIncomingUnidirectionalAlgorithm = nullptr;

  // We no longer block BFCache
  NotifyToWindow(false);
}

void WebTransport::SendSetSendOrder(uint64_t aStreamId,
                                    Maybe<int64_t> aSendOrder) {
  mChild->SendSetSendOrder(aStreamId, aSendOrder);
}

void WebTransport::NotifyBFCacheOnMainThread(nsPIDOMWindowInner* aInner,
                                             bool aCreated) {
  AssertIsOnMainThread();
  if (!aInner) {
    return;
  }
  if (aCreated) {
    aInner->RemoveFromBFCacheSync();
  }

  uint32_t count = aInner->UpdateWebTransportCount(aCreated);
  // It's okay for WindowGlobalChild to not exist, as it should mean it already
  // is destroyed and can't enter bfcache anyway.
  if (WindowGlobalChild* child = aInner->GetWindowGlobalChild()) {
    if (aCreated && count == 1) {
      // The first WebTransport is active.
      child->BlockBFCacheFor(BFCacheStatus::ACTIVE_WEBTRANSPORT);
    } else if (count == 0) {
      child->UnblockBFCacheFor(BFCacheStatus::ACTIVE_WEBTRANSPORT);
    }
  }
}

class BFCacheNotifyWTRunnable final : public WorkerProxyToMainThreadRunnable {
 public:
  explicit BFCacheNotifyWTRunnable(bool aCreated) : mCreated(aCreated) {}

  void RunOnMainThread(WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    AssertIsOnMainThread();
    if (aWorkerPrivate->IsDedicatedWorker()) {
      WebTransport::NotifyBFCacheOnMainThread(
          aWorkerPrivate->GetAncestorWindow(), mCreated);
      return;
    }
    if (aWorkerPrivate->IsSharedWorker()) {
      aWorkerPrivate->GetRemoteWorkerController()->NotifyWebTransport(mCreated);
      return;
    }
    MOZ_ASSERT_UNREACHABLE("Unexpected worker type");
  }

  void RunBackOnWorkerThreadForCleanup(WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

 private:
  bool mCreated;
};

void WebTransport::NotifyToWindow(bool aCreated) const {
  if (NS_IsMainThread()) {
    NotifyBFCacheOnMainThread(GetParentObject()->GetAsInnerWindow(), aCreated);
    return;
  }

  WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
  if (wp->IsDedicatedWorker() || wp->IsSharedWorker()) {
    RefPtr<BFCacheNotifyWTRunnable> runnable =
        new BFCacheNotifyWTRunnable(aCreated);

    runnable->Dispatch(wp);
  }
};

}  // namespace mozilla::dom
