/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebrtcTCPSocketChild_h
#define mozilla_net_WebrtcTCPSocketChild_h

#include "mozilla/net/PWebrtcTCPSocketChild.h"
#include "mozilla/dom/ipc/IdType.h"
#include "transport/nr_socket_proxy_config.h"

namespace mozilla::net {

class WebrtcTCPSocketCallback;

class WebrtcTCPSocketChild : public PWebrtcTCPSocketChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcTCPSocketChild)

  mozilla::ipc::IPCResult RecvOnClose(const nsresult& aReason) override;

  mozilla::ipc::IPCResult RecvOnConnected(
      const nsACString& aProxyType) override;

  mozilla::ipc::IPCResult RecvOnRead(nsTArray<uint8_t>&& aReadData) override;

  explicit WebrtcTCPSocketChild(WebrtcTCPSocketCallback* aProxyCallbacks);

  void AsyncOpen(const nsACString& aHost, const int& aPort,
                 const nsACString& aLocalAddress, const int& aLocalPort,
                 bool aUseTls,
                 const std::shared_ptr<NrSocketProxyConfig>& aProxyConfig);

  void AddIPDLReference() { AddRef(); }
  void ReleaseIPDLReference() { Release(); }

 protected:
  virtual ~WebrtcTCPSocketChild();

  RefPtr<WebrtcTCPSocketCallback> mProxyCallbacks;
};

}  // namespace mozilla::net

#endif  // mozilla_net_WebrtcTCPSocketChild_h
