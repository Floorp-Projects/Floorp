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
#include "mozilla/dom/WebTransportLog.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebTransport, mGlobal,
                                      mIncomingUnidirectionalStreams,
                                      mIncomingBidirectionalStreams, mReady)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebTransport)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebTransport)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTransport)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

WebTransport::WebTransport(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal), mState(WebTransportState::CONNECTING) {
  LOG(("Creating WebTransport %p", this));
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
  if (!result->Init(aGlobal, aURL, aOptions, aError)) {
    return nullptr;
  }

  return result.forget();
}

bool WebTransport::Init(const GlobalObject& aGlobal, const nsAString& aURL,
                        const WebTransportOptions& aOptions,
                        ErrorResult& aError) {
  // Initiate connection with parent
  using mozilla::ipc::BackgroundChild;
  using mozilla::ipc::Endpoint;
  using mozilla::ipc::PBackgroundChild;

  mReady = Promise::Create(mGlobal, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return false;
  }

  QueuingStrategy strategy;
  Optional<JS::Handle<JSObject*>> underlying;
  mIncomingUnidirectionalStreams =
      ReadableStream::Constructor(aGlobal, underlying, strategy, aError);
  if (aError.Failed()) {
    return false;
  }
  mIncomingBidirectionalStreams =
      ReadableStream::Constructor(aGlobal, underlying, strategy, aError);
  if (aError.Failed()) {
    return false;
  }

  PBackgroundChild* backgroundChild =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundChild)) {
    return false;
  }

  // Create a new IPC connection
  Endpoint<PWebTransportParent> parentEndpoint;
  Endpoint<PWebTransportChild> childEndpoint;
  MOZ_ALWAYS_SUCCEEDS(
      PWebTransport::CreateEndpoints(&parentEndpoint, &childEndpoint));

  RefPtr<WebTransportChild> child = new WebTransportChild();
  if (!childEndpoint.Bind(child)) {
    return false;
  }

  mState = WebTransportState::CONNECTING;
  LOG(("Connecting WebTransport to parent for %s",
       NS_ConvertUTF16toUTF8(aURL).get()));

  // Parse string for validity and Throw a SyntaxError if it isn't
  if (!ParseURL(aURL)) {
    aError.ThrowSyntaxError("Invalid WebTransport URL");
    return false;
  }
  bool dedicated =
      !aOptions.mAllowPooling;  // spec language, optimizer will eliminate this
  bool requireUnreliable = aOptions.mRequireUnreliable;
  WebTransportCongestionControl congestionControl = aOptions.mCongestionControl;
  if (aOptions.mServerCertificateHashes.WasPassed()) {
    // XXX bug 1806693
    aError.ThrowNotSupportedError("No support for serverCertificateHashes yet");
    // XXX if dedicated is false and serverCertificateHashes is non-null, then
    // throw a TypeError. Also should enforce in parent
    return false;
  }

  // https://w3c.github.io/webtransport/#webtransport-constructor Spec 5.2
  backgroundChild
      ->SendCreateWebTransportParent(aURL, dedicated, requireUnreliable,
                                     (uint32_t)congestionControl,
                                     // XXX serverCertHashes,
                                     std::move(parentEndpoint))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, child](nsresult rv) {
            if (NS_FAILED(rv)) {
              self->RejectWaitingConnection(rv);
            } else {
              // This will process anything waiting for the connection to
              // complete;
              self->ResolveWaitingConnection(child);
            }
          },
          [self = RefPtr<WebTransport>(this)](
              const mozilla::ipc::ResponseRejectReason&) {
            // This will process anything waiting for the connection to
            // complete;
            self->RejectWaitingConnection(NS_ERROR_FAILURE);
          });

  return true;
}

void WebTransport::ResolveWaitingConnection(WebTransportChild* aChild) {
  LOG(("Resolved Connection %p", this));
  MOZ_ASSERT(mState == WebTransportState::CONNECTING);
  mChild = aChild;
  mState = WebTransportState::CONNECTED;

  mReady->MaybeResolve(true);
}

void WebTransport::RejectWaitingConnection(nsresult aRv) {
  LOG(("Reject Connection %p", this));
  MOZ_ASSERT(mState == WebTransportState::CONNECTING);
  mState = WebTransportState::FAILED;
  LOG(("Rejected connection %x", (uint32_t)aRv));

  mReady->MaybeReject(aRv);
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

already_AddRefed<Promise> WebTransport::Ready() { return do_AddRef(mReady); }

WebTransportReliabilityMode WebTransport::Reliability() {
  // XXX not implemented
  return WebTransportReliabilityMode::Pending;
}

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
    // "Let reasonString be the maximal code unit prefix of
    // closeInfo.reason where the length of the UTF-8 encoded prefix
    // doesn’t exceed 1024."
    // Take the maximal "code unit prefix" of mReason and limit to 1024 bytes
    // https://w3c.github.io/webtransport/#webtransport-methods 5.4

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

    // The other side will call `Close()` for us now, make sure we don't call it
    // in our destructor.
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

}  // namespace mozilla::dom
