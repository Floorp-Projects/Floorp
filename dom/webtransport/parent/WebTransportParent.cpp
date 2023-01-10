/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportParent.h"
#include "Http3WebTransportSession.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/dom/WebTransportBinding.h"
#include "mozilla/dom/WebTransportLog.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsIEventTarget.h"
#include "nsIOService.h"
#include "nsIPrincipal.h"
#include "nsIWebTransport.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(WebTransportParent, WebTransportSessionEventListener);

using IPCResult = mozilla::ipc::IPCResult;
using CreateWebTransportPromise =
    MozPromise<WebTransportReliabilityMode, nsresult, true>;
WebTransportParent::~WebTransportParent() {
  LOG(("Destroying WebTransportParent %p", this));
}

// static
void WebTransportParent::Create(
    const nsAString& aURL, nsIPrincipal* aPrincipal, const bool& aDedicated,
    const bool& aRequireUnreliable, const uint32_t& aCongestionControl,
    // Sequence<WebTransportHash>* aServerCertHashes,
    Endpoint<PWebTransportParent>&& aParentEndpoint,
    std::function<void(Tuple<const nsresult&, const uint8_t&>)>&& aResolver) {
  LOG(("Created WebTransportParent %s %s %s congestion=%s",
       NS_ConvertUTF16toUTF8(aURL).get(),
       aDedicated ? "Dedicated" : "AllowPooling",
       aRequireUnreliable ? "RequireUnreliable" : "",
       aCongestionControl ==
               (uint32_t)dom::WebTransportCongestionControl::Throughput
           ? "ThroughPut"
           : (aCongestionControl ==
                      (uint32_t)dom::WebTransportCongestionControl::Low_latency
                  ? "Low-Latency"
                  : "Default")));

  if (!StaticPrefs::network_webtransport_enabled()) {
    aResolver(ResolveType(
        NS_ERROR_DOM_NOT_ALLOWED_ERR,
        static_cast<uint8_t>(WebTransportReliabilityMode::Pending)));
    return;
  }

  if (!aParentEndpoint.IsValid()) {
    aResolver(ResolveType(
        NS_ERROR_INVALID_ARG,
        static_cast<uint8_t>(WebTransportReliabilityMode::Pending)));
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mozilla::net::gIOService);
  nsresult rv =
      mozilla::net::gIOService->NewWebTransport(getter_AddRefs(mWebTransport));
  if (NS_FAILED(rv)) {
    aResolver(ResolveType(
        rv, static_cast<uint8_t>(WebTransportReliabilityMode::Pending)));
    return;
  }

  mOwningEventTarget = GetCurrentEventTarget();

  MOZ_ASSERT(aPrincipal);
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aURL);
  if (NS_FAILED(rv)) {
    aResolver(ResolveType(
        NS_ERROR_INVALID_ARG,
        static_cast<uint8_t>(WebTransportReliabilityMode::Pending)));
    return;
  }

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "WebTransport AsyncConnect",
      [self = RefPtr{this}, uri = std::move(uri),
       principal = RefPtr{aPrincipal},
       flags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL] {
        self->mWebTransport->AsyncConnect(uri, principal, flags, self);
      });

  // Bind to SocketThread for IPC - connection creation/destruction must
  // hit MainThread, but keep all other traffic on SocketThread.  Note that
  // we must call aResolver() on this (PBackground) thread.
  nsCOMPtr<nsISerialEventTarget> sts =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  mResolver = aResolver;

  InvokeAsync(
      sts, __func__,
      [parentEndpoint = std::move(aParentEndpoint), runnable = r]() mutable {
        RefPtr<WebTransportParent> parent = new WebTransportParent();

        LOG(("Binding parent endpoint"));
        if (!parentEndpoint.Bind(parent)) {
          return CreateWebTransportPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                            __func__);
        }
        // IPC now holds a ref to parent
        // Send connection to the server via MainThread
        NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
        return CreateWebTransportPromise::CreateAndResolve(
            WebTransportReliabilityMode::Supports_unreliable, __func__);
      })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}](
              const CreateWebTransportPromise::ResolveOrRejectValue& aValue) {
            if (aValue.IsReject()) {
              self->mResolver(ResolveType(
                  aValue.RejectValue(),
                  static_cast<uint8_t>(WebTransportReliabilityMode::Pending)));
            }
          });
}

void WebTransportParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("ActorDestroy WebTransportParent %d", aWhy));
}

// We may not receive this response if the child side is destroyed without
// `Close` or `Shutdown` being explicitly called.
mozilla::ipc::IPCResult WebTransportParent::RecvClose(
    const uint32_t& aCode, const nsACString& aReason) {
  LOG(("Close received, code = %u, reason = %s", aCode,
       PromiseFlatCString(aReason).get()));
  MOZ_ASSERT(!mClosed);
  mClosed.Flip();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "WebTransport Close", [self = RefPtr{this}, aCode, &aReason] {
        if (self->mWebTransport) {
          self->mWebTransport->CloseSession(aCode, aReason);
        }
      }));
  Close();
  return IPC_OK();
}

// We recieve this notification from the WebTransportSessionProxy if session was
// successfully created at the end of
// WebTransportSessionProxy::OnStopRequest
NS_IMETHODIMP
WebTransportParent::OnSessionReady(uint64_t aSessionId) {
  MOZ_ASSERT(mOwningEventTarget);
  MOZ_ASSERT(!mOwningEventTarget->IsOnCurrentThread());

  LOG(("Created web transport session, sessionID = %" PRIu64 "", aSessionId));

  mOwningEventTarget->Dispatch(NS_NewRunnableFunction(
      "WebTransportParent::OnSessionReady", [self = RefPtr{this}] {
        if (!self->IsClosed()) {
          self->mResolver(ResolveType(
              NS_OK, static_cast<uint8_t>(
                         WebTransportReliabilityMode::Supports_unreliable)));
        }
      }));

  return NS_OK;
}

// We recieve this notification from the WebTransportSessionProxy if session
// creation was unsuccessful at the end of
// WebTransportSessionProxy::OnStopRequest
NS_IMETHODIMP
WebTransportParent::OnSessionClosed(const uint32_t aErrorCode,
                                    const nsACString& aReason) {
  LOG(("Creating web transport session failed code= %u, reason= %s", aErrorCode,
       PromiseFlatCString(aReason).get()));
  nsresult rv = NS_OK;

  if (aErrorCode != 0) {
    // currently we just know if session was closed gracefully or not.
    // we need better error propagation from lower-levels of http3
    // webtransport session and it's subsequent error mapping to DOM.
    // XXX See Bug 1806834
    rv = NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(mOwningEventTarget);
  MOZ_ASSERT(!mOwningEventTarget->IsOnCurrentThread());

  mOwningEventTarget->Dispatch(NS_NewRunnableFunction(
      "WebTransportParent::OnSessionClosed",
      [self = RefPtr{this}, result = rv] {
        if (!self->IsClosed()) {
          self->mResolver(ResolveType(
              result, static_cast<uint8_t>(
                          WebTransportReliabilityMode::Supports_unreliable)));
        }
      }));

  return NS_OK;
}

// This method is currently not used by WebTransportSessionProxy to inform of
// any session related events. All notification is recieved via
// WebTransportSessionProxy::OnSessionReady and
// WebTransportSessionProxy::OnSessionClosed methods
NS_IMETHODIMP
WebTransportParent::OnSessionReadyInternal(
    mozilla::net::Http3WebTransportSession* aSession) {
  Unused << aSession;
  return NS_OK;
}

NS_IMETHODIMP
WebTransportParent::OnIncomingStreamAvailableInternal(
    mozilla::net::Http3WebTransportStream* aStream) {
  // XXX implement once DOM WebAPI supports creation of streams
  Unused << aStream;
  return NS_OK;
}

NS_IMETHODIMP
WebTransportParent::OnIncomingUnidirectionalStreamAvailable(
    nsIWebTransportReceiveStream* aStream) {
  // XXX implement once DOM WebAPI supports creation of streams
  Unused << aStream;
  return NS_OK;
}

NS_IMETHODIMP
WebTransportParent::OnIncomingBidirectionalStreamAvailable(
    nsIWebTransportBidirectionalStream* aStream) {
  // XXX implement once DOM WebAPI supports creation of streams
  Unused << aStream;
  return NS_OK;
}

}  // namespace mozilla::dom
