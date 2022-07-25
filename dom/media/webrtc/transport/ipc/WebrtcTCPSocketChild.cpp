/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcTCPSocketChild.h"

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/SocketProcessChild.h"

#include "LoadInfo.h"

#include "WebrtcTCPSocketLog.h"
#include "WebrtcTCPSocketCallback.h"

using namespace mozilla::ipc;

namespace mozilla::net {

mozilla::ipc::IPCResult WebrtcTCPSocketChild::RecvOnClose(
    const nsresult& aReason) {
  LOG(("WebrtcTCPSocketChild::RecvOnClose %p\n", this));

  MOZ_ASSERT(mProxyCallbacks, "webrtc TCP callbacks should be non-null");
  mProxyCallbacks->OnClose(aReason);
  mProxyCallbacks = nullptr;

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcTCPSocketChild::RecvOnConnected(
    const nsACString& aProxyType) {
  LOG(("WebrtcTCPSocketChild::RecvOnConnected %p\n", this));

  MOZ_ASSERT(mProxyCallbacks, "webrtc TCP callbacks should be non-null");
  mProxyCallbacks->OnConnected(aProxyType);

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcTCPSocketChild::RecvOnRead(
    nsTArray<uint8_t>&& aReadData) {
  LOG(("WebrtcTCPSocketChild::RecvOnRead %p\n", this));

  MOZ_ASSERT(mProxyCallbacks, "webrtc TCP callbacks should be non-null");
  mProxyCallbacks->OnRead(std::move(aReadData));

  return IPC_OK();
}

WebrtcTCPSocketChild::WebrtcTCPSocketChild(
    WebrtcTCPSocketCallback* aProxyCallbacks)
    : mProxyCallbacks(aProxyCallbacks) {
  MOZ_COUNT_CTOR(WebrtcTCPSocketChild);

  LOG(("WebrtcTCPSocketChild::WebrtcTCPSocketChild %p\n", this));
}

WebrtcTCPSocketChild::~WebrtcTCPSocketChild() {
  MOZ_COUNT_DTOR(WebrtcTCPSocketChild);

  LOG(("WebrtcTCPSocketChild::~WebrtcTCPSocketChild %p\n", this));
}

void WebrtcTCPSocketChild::AsyncOpen(
    const nsACString& aHost, const int& aPort, const nsACString& aLocalAddress,
    const int& aLocalPort, bool aUseTls,
    const std::shared_ptr<NrSocketProxyConfig>& aProxyConfig) {
  LOG(("WebrtcTCPSocketChild::AsyncOpen %p %s:%d\n", this,
       PromiseFlatCString(aHost).get(), aPort));

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  AddIPDLReference();

  Maybe<net::WebrtcProxyConfig> proxyConfig;
  Maybe<dom::TabId> tabId;
  if (aProxyConfig) {
    proxyConfig = Some(aProxyConfig->GetConfig());
    tabId = Some(proxyConfig->tabId());
  }

  if (IsNeckoChild()) {
    // We're on a content process
    gNeckoChild->SendPWebrtcTCPSocketConstructor(this, tabId);
  } else if (IsSocketProcessChild()) {
    // We're on a socket process
    SocketProcessChild::GetSingleton()->SendPWebrtcTCPSocketConstructor(this,
                                                                        tabId);
  }

  SendAsyncOpen(aHost, aPort, aLocalAddress, aLocalPort, aUseTls, proxyConfig);
}

}  // namespace mozilla::net
