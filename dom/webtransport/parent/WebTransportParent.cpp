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
#include "nsStreamUtils.h"
#include "nsIWebTransportStream.h"

using IPCResult = mozilla::ipc::IPCResult;

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(WebTransportParent, WebTransportSessionEventListener);

using CreateWebTransportPromise =
    MozPromise<WebTransportReliabilityMode, nsresult, true>;
WebTransportParent::~WebTransportParent() {
  LOG(("Destroying WebTransportParent %p", this));
}

void WebTransportParent::Create(
    const nsAString& aURL, nsIPrincipal* aPrincipal, const bool& aDedicated,
    const bool& aRequireUnreliable, const uint32_t& aCongestionControl,
    // Sequence<WebTransportHash>* aServerCertHashes,
    Endpoint<PWebTransportParent>&& aParentEndpoint,
    std::function<void(Tuple<const nsresult&, const uint8_t&>)>&& aResolver) {
  LOG(("Created WebTransportParent %p %s %s %s congestion=%s", this,
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

  mOwningEventTarget = GetCurrentSerialEventTarget();

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
        LOG(("WebTransport %p AsyncConnect", self.get()));
        self->mWebTransport->AsyncConnect(uri, principal, flags, self);
      });

  // Bind to SocketThread for IPC - connection creation/destruction must
  // hit MainThread, but keep all other traffic on SocketThread.  Note that
  // we must call aResolver() on this (PBackground) thread.
  mSocketThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  InvokeAsync(mSocketThread, __func__,
              [parentEndpoint = std::move(aParentEndpoint), runnable = r,
               resolver = std::move(aResolver), p = RefPtr{this}]() mutable {
                p->mResolver = resolver;

                LOG(("Binding parent endpoint"));
                if (!parentEndpoint.Bind(p)) {
                  return CreateWebTransportPromise::CreateAndReject(
                      NS_ERROR_FAILURE, __func__);
                }
                // IPC now holds a ref to parent
                // Send connection to the server via MainThread
                NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);

                return CreateWebTransportPromise::CreateAndResolve(
                    WebTransportReliabilityMode::Supports_unreliable, __func__);
              })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [p = RefPtr{this}](
              const CreateWebTransportPromise::ResolveOrRejectValue& aValue) {
            if (aValue.IsReject()) {
              p->mResolver(ResolveType(
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
IPCResult WebTransportParent::RecvClose(const uint32_t& aCode,
                                        const nsACString& aReason) {
  LOG(("Close for %p received, code = %u, reason = %s", this, aCode,
       PromiseFlatCString(aReason).get()));
  MOZ_ASSERT(!mClosed);
  mClosed.Flip();
  mWebTransport->CloseSession(aCode, aReason);
  Close();
  return IPC_OK();
}

IPCResult WebTransportParent::RecvCreateUnidirectionalStream(
    CreateUnidirectionalStreamResolver&& aResolver) {
  return IPC_OK();
}

IPCResult WebTransportParent::RecvCreateBidirectionalStream(
    CreateBidirectionalStreamResolver&& aResolver) {
  return IPC_OK();
}

// We recieve this notification from the WebTransportSessionProxy if session was
// successfully created at the end of
// WebTransportSessionProxy::OnStopRequest
NS_IMETHODIMP
WebTransportParent::OnSessionReady(uint64_t aSessionId) {
  MOZ_ASSERT(mOwningEventTarget);
  MOZ_ASSERT(!mOwningEventTarget->IsOnCurrentThread());

  LOG(("Created web transport session, sessionID = %" PRIu64 ", for %p",
       aSessionId, this));

  mOwningEventTarget->Dispatch(NS_NewRunnableFunction(
      "WebTransportParent::OnSessionReady", [self = RefPtr{this}] {
        if (!self->IsClosed() && self->mResolver) {
          self->mResolver(ResolveType(
              NS_OK, static_cast<uint8_t>(
                         WebTransportReliabilityMode::Supports_unreliable)));
          self->mResolver = nullptr;
        }
      }));

  nsresult rv;
  nsCOMPtr<nsISerialEventTarget> sts =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Retarget to socket thread. After this, WebTransportParent and
  // |mWebTransport| should be only accessed on the socket thread.
  Unused << mWebTransport->RetargetTo(sts);
  return NS_OK;
}

// We recieve this notification from the WebTransportSessionProxy if session
// creation was unsuccessful at the end of
// WebTransportSessionProxy::OnStopRequest
NS_IMETHODIMP
WebTransportParent::OnSessionClosed(const uint32_t aErrorCode,
                                    const nsACString& aReason) {
  nsresult rv = NS_OK;

  MOZ_ASSERT(mOwningEventTarget);
  MOZ_ASSERT(!mOwningEventTarget->IsOnCurrentThread());

  // currently we just know if session was closed gracefully or not.
  // we need better error propagation from lower-levels of http3
  // webtransport session and it's subsequent error mapping to DOM.
  // XXX See Bug 1806834
  if (mResolver) {
    LOG(("webtransport %p session creation failed code= %u, reason= %s", this,
         aErrorCode, PromiseFlatCString(aReason).get()));
    // we know we haven't gone Ready yet
    rv = NS_ERROR_FAILURE;
    mOwningEventTarget->Dispatch(NS_NewRunnableFunction(
        "WebTransportParent::OnSessionClosed",
        [self = RefPtr{this}, result = rv] {
          if (!self->IsClosed() && self->mResolver) {
            self->mResolver(ResolveType(
                result, static_cast<uint8_t>(
                            WebTransportReliabilityMode::Supports_unreliable)));
          }
        }));
  } else {
    // https://w3c.github.io/webtransport/#web-transport-termination
    // Step 1: Let cleanly be a boolean representing whether the HTTP/3
    // stream associated with the CONNECT request that initiated
    // transport.[[Session]] is in the "Data Recvd" state. [QUIC]
    // XXX not calculated yet
    LOG(("webtransport %p session remote closed code= %u, reason= %s", this,
         aErrorCode, PromiseFlatCString(aReason).get()));
    mSocketThread->Dispatch(NS_NewRunnableFunction(
        __func__,
        [self = RefPtr{this}, aErrorCode, reason = nsCString{aReason}]() {
          // Tell the content side we were closed by the server
          Unused << self->SendRemoteClosed(/*XXX*/ true, aErrorCode, reason);
          // Let the other end shut down the IPC channel after RecvClose()
        }));
  }

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
  // Note: we need to hold a reference to the stream if we want to get stats,
  // etc
  LOG(("%p IncomingUnidirectonalStream available", this));
  // We should be on the Socket Thread
  RefPtr<DataPipeSender> sender;
  RefPtr<DataPipeReceiver> receiver;
  nsresult rv = NewDataPipe(mozilla::ipc::kDefaultDataPipeCapacity,
                            getter_AddRefs(sender), getter_AddRefs(receiver));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIEventTarget> target = GetCurrentSerialEventTarget();
  nsCOMPtr<nsIAsyncInputStream> stream = do_QueryInterface(aStream);
  rv = NS_AsyncCopy(stream, sender, target, NS_ASYNCCOPY_VIA_READSEGMENTS,
                    mozilla::ipc::kDefaultDataPipeCapacity, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LOG(("%p Sending UnidirectionalStream pipe to content", this));
  // pass the DataPipeReceiver to the content process
  Unused << SendIncomingUnidirectionalStream(receiver);

  return NS_OK;
}

NS_IMETHODIMP
WebTransportParent::OnIncomingBidirectionalStreamAvailable(
    nsIWebTransportBidirectionalStream* aStream) {
  // XXX implement once DOM WebAPI supports creation of streams
  LOG(("%p Sending BidirectionalStream pipe to content", this));
  Unused << aStream;
  return NS_OK;
}

NS_IMETHODIMP WebTransportParent::OnDatagramReceived(
    const nsTArray<uint8_t>& aData) {
  // XXX revisit this function once DOM WebAPI supports datagrams
  return NS_OK;
}

NS_IMETHODIMP WebTransportParent::OnDatagramReceivedInternal(
    nsTArray<uint8_t>&& aData) {
  // XXX revisit this function once DOM WebAPI supports datagrams
  return NS_OK;
}

NS_IMETHODIMP
WebTransportParent::OnOutgoingDatagramOutCome(
    uint64_t aId, WebTransportSessionEventListener::DatagramOutcome aOutCome) {
  // XXX revisit this function once DOM WebAPI supports datagrams
  return NS_OK;
}

NS_IMETHODIMP WebTransportParent::OnMaxDatagramSize(uint64_t aSize) {
  // XXX revisit this function once DOM WebAPI supports datagrams
  return NS_OK;
}
}  // namespace mozilla::dom
