/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcTCPSocketParent.h"

#include "mozilla/net/NeckoParent.h"

#include "WebrtcTCPSocket.h"
#include "WebrtcTCPSocketLog.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla::net {

mozilla::ipc::IPCResult WebrtcTCPSocketParent::RecvAsyncOpen(
    const nsACString& aHost, const int& aPort, const nsACString& aLocalAddress,
    const int& aLocalPort, const bool& aUseTls,
    const Maybe<WebrtcProxyConfig>& aProxyConfig) {
  LOG(("WebrtcTCPSocketParent::RecvAsyncOpen %p to %s:%d\n", this,
       PromiseFlatCString(aHost).get(), aPort));

  MOZ_ASSERT(mChannel, "webrtc TCP socket should be non-null");
  if (!mChannel) {
    return IPC_FAIL(this, "Called with null channel.");
  }

  mChannel->Open(aHost, aPort, aLocalAddress, aLocalPort, aUseTls,
                 aProxyConfig);

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcTCPSocketParent::RecvWrite(
    nsTArray<uint8_t>&& aWriteData) {
  LOG(("WebrtcTCPSocketParent::RecvWrite %p for %zu\n", this,
       aWriteData.Length()));

  // Need to check this here in case there are Writes in the queue after OnClose
  if (mChannel) {
    mChannel->Write(std::move(aWriteData));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcTCPSocketParent::RecvClose() {
  LOG(("WebrtcTCPSocketParent::RecvClose %p\n", this));

  CleanupChannel();

  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }

  return IPC_OK();
}

void WebrtcTCPSocketParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("WebrtcTCPSocketParent::ActorDestroy %p for %d\n", this, aWhy));

  CleanupChannel();
}

WebrtcTCPSocketParent::WebrtcTCPSocketParent(const Maybe<dom::TabId>& aTabId) {
  MOZ_COUNT_CTOR(WebrtcTCPSocketParent);

  LOG(("WebrtcTCPSocketParent::WebrtcTCPSocketParent %p\n", this));

  mChannel = new WebrtcTCPSocket(this);
  if (aTabId.isSome()) {
    mChannel->SetTabId(*aTabId);
  }
}

WebrtcTCPSocketParent::~WebrtcTCPSocketParent() {
  MOZ_COUNT_DTOR(WebrtcTCPSocketParent);

  LOG(("WebrtcTCPSocketParent::~WebrtcTCPSocketParent %p\n", this));

  CleanupChannel();
}

// WebrtcTCPSocketCallback
void WebrtcTCPSocketParent::OnClose(nsresult aReason) {
  LOG(("WebrtcTCPSocketParent::OnClose %p\n", this));

  if (mChannel) {
    Unused << SendOnClose(aReason);
  }

  CleanupChannel();
}

void WebrtcTCPSocketParent::OnRead(nsTArray<uint8_t>&& aReadData) {
  LOG(("WebrtcTCPSocketParent::OnRead %p %zu\n", this, aReadData.Length()));

  if (mChannel && !SendOnRead(std::move(aReadData))) {
    CleanupChannel();
  }
}

void WebrtcTCPSocketParent::OnConnected(const nsACString& aProxyType) {
  LOG(("WebrtcTCPSocketParent::OnConnected %p\n", this));

  if (mChannel && !SendOnConnected(aProxyType)) {
    CleanupChannel();
  }
}

void WebrtcTCPSocketParent::CleanupChannel() {
  if (mChannel) {
    mChannel->Close();
    mChannel = nullptr;
  }
}

}  // namespace mozilla::net
