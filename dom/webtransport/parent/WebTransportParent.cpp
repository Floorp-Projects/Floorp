/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportParent.h"

#include "mozilla/StaticPrefs_network.h"
#include "mozilla/dom/WebTransportBinding.h"
#include "mozilla/dom/WebTransportLog.h"
#include "mozilla/ipc/BackgroundParent.h"

using IPCResult = mozilla::ipc::IPCResult;

namespace mozilla::dom {

WebTransportParent::~WebTransportParent() {
  LOG(("Destroying WebTransportParent %p", this));
}

bool WebTransportParent::Init(
    const nsAString& aURL, const bool& aDedicated,
    const bool& aRequireUnreliable, const uint32_t& aCongestionControl,
    // Sequence<WebTransportHash>* aServerCertHashes,
    Endpoint<PWebTransportParent>&& aParentEndpoint,
    std::function<void(const nsresult&)>&& aResolver) {
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
    aResolver(NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return false;
  }

  if (!aParentEndpoint.IsValid()) {
    aResolver(NS_ERROR_INVALID_ARG);
    return false;
  }

  if (!aParentEndpoint.Bind(this)) {
    aResolver(NS_ERROR_FAILURE);
    return false;
  }
  // XXX Send connection to the server
  aResolver(NS_OK);
  return true;
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
  // XXX shut down streams cleanly
  Close();
  return IPC_OK();
}

}  // namespace mozilla::dom
