/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UDPSocketParent_h__
#define mozilla_dom_UDPSocketParent_h__

#include "mozilla/net/PUDPSocketParent.h"
#include "nsCOMPtr.h"
#include "nsIUDPSocket.h"
#include "nsISocketFilter.h"
#include "mozilla/dom/PermissionMessageUtils.h"

namespace mozilla {
namespace net {
class PNeckoParent;
} // namespace net

namespace dom {

class UDPSocketParent : public mozilla::net::PUDPSocketParent
                      , public nsIUDPSocketListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER

  explicit UDPSocketParent(PBackgroundParent* aManager);
  explicit UDPSocketParent(PNeckoParent* aManager);

  bool Init(const IPC::Principal& aPrincipal, const nsACString& aFilter);

  virtual mozilla::ipc::IPCResult RecvBind(const UDPAddressInfo& aAddressInfo,
                                           const bool& aAddressReuse, const bool& aLoopback,
                                           const uint32_t& recvBufferSize,
                                           const uint32_t& sendBufferSize) override;
  virtual mozilla::ipc::IPCResult RecvConnect(const UDPAddressInfo& aAddressInfo) override;
  void DoSendConnectResponse(const UDPAddressInfo& aAddressInfo);
  void SendConnectResponse(nsIEventTarget *aThread,
                           const UDPAddressInfo& aAddressInfo);
  void DoConnect(nsCOMPtr<nsIUDPSocket>& aSocket,
                 nsCOMPtr<nsIEventTarget>& aReturnThread,
                 const UDPAddressInfo& aAddressInfo);

  virtual mozilla::ipc::IPCResult RecvOutgoingData(const UDPData& aData, const UDPSocketAddr& aAddr) override;

  virtual mozilla::ipc::IPCResult RecvClose() override;
  virtual mozilla::ipc::IPCResult RecvRequestDelete() override;
  virtual mozilla::ipc::IPCResult RecvJoinMulticast(const nsCString& aMulticastAddress,
                                                    const nsCString& aInterface) override;
  virtual mozilla::ipc::IPCResult RecvLeaveMulticast(const nsCString& aMulticastAddress,
                                                     const nsCString& aInterface) override;

private:
  virtual ~UDPSocketParent();

  virtual void ActorDestroy(ActorDestroyReason why) override;
  void Send(const InfallibleTArray<uint8_t>& aData, const UDPSocketAddr& aAddr);
  void Send(const InputStreamParams& aStream, const UDPSocketAddr& aAddr);
  nsresult BindInternal(const nsCString& aHost, const uint16_t& aPort,
                        const bool& aAddressReuse, const bool& aLoopback,
                        const uint32_t& recvBufferSize,
                        const uint32_t& sendBufferSize);
  nsresult ConnectInternal(const nsCString& aHost, const uint16_t& aPort);
  void FireInternalError(uint32_t aLineNo);
  void SendInternalError(nsIEventTarget *aThread,
                         uint32_t aLineNo);

  // One of these will be null and the other non-null.
  PBackgroundParent* mBackgroundManager;
  PNeckoParent* mNeckoManager;

  bool mIPCOpen;
  nsCOMPtr<nsIUDPSocket> mSocket;
  nsCOMPtr<nsISocketFilter> mFilter;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

} // namespace dom
} // namespace mozilla

#endif // !defined(mozilla_dom_UDPSocketParent_h__)
