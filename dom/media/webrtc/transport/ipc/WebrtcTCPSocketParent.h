/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebrtcTCPSocketParent_h
#define mozilla_net_WebrtcTCPSocketParent_h

#include "mozilla/net/PWebrtcTCPSocketParent.h"

#include "WebrtcTCPSocketCallback.h"

class nsIAuthPromptProvider;

namespace mozilla::net {

class WebrtcTCPSocket;

class WebrtcTCPSocketParent : public PWebrtcTCPSocketParent,
                              public WebrtcTCPSocketCallback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcTCPSocketParent, override)

  mozilla::ipc::IPCResult RecvAsyncOpen(
      const nsACString& aHost, const int& aPort,
      const nsACString& aLocalAddress, const int& aLocalPort,
      const bool& aUseTls,
      const Maybe<WebrtcProxyConfig>& aProxyConfig) override;

  mozilla::ipc::IPCResult RecvWrite(nsTArray<uint8_t>&& aWriteData) override;

  mozilla::ipc::IPCResult RecvClose() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  explicit WebrtcTCPSocketParent(const Maybe<dom::TabId>& aTabId);

  // WebrtcTCPSocketCallback
  void OnClose(nsresult aReason) override;
  void OnConnected(const nsACString& aProxyType) override;
  void OnRead(nsTArray<uint8_t>&& bytes) override;

  void AddIPDLReference() { AddRef(); }
  void ReleaseIPDLReference() { Release(); }

 protected:
  virtual ~WebrtcTCPSocketParent();

 private:
  void CleanupChannel();

  // Indicates that IPC is open.
  RefPtr<WebrtcTCPSocket> mChannel;
};

}  // namespace mozilla::net

#endif  // mozilla_net_WebrtcTCPSocketParent_h
