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
#include "nsIUDPSocketFilter.h"
#include "mozilla/net/OfflineObserver.h"
#include "mozilla/dom/PermissionMessageUtils.h"

namespace mozilla {
namespace net {
class PNeckoParent;
} // namespace net

namespace dom {

class UDPSocketParent : public mozilla::net::PUDPSocketParent
                      , public nsIUDPSocketListener
                      , public mozilla::net::DisconnectableParent
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER

  explicit UDPSocketParent(PBackgroundParent* aManager);
  explicit UDPSocketParent(PNeckoParent* aManager);

  bool Init(const IPC::Principal& aPrincipal, const nsACString& aFilter);

  virtual bool RecvBind(const UDPAddressInfo& aAddressInfo,
                        const bool& aAddressReuse, const bool& aLoopback) override;

  virtual bool RecvOutgoingData(const UDPData& aData, const UDPSocketAddr& aAddr) override;

  virtual bool RecvClose() override;
  virtual bool RecvRequestDelete() override;
  virtual bool RecvJoinMulticast(const nsCString& aMulticastAddress,
                                 const nsCString& aInterface) override;
  virtual bool RecvLeaveMulticast(const nsCString& aMulticastAddress,
                                  const nsCString& aInterface) override;
  virtual nsresult OfflineNotification(nsISupports *) override;
  virtual uint32_t GetAppId() override;

private:
  virtual ~UDPSocketParent();

  virtual void ActorDestroy(ActorDestroyReason why) override;
  void Send(const InfallibleTArray<uint8_t>& aData, const UDPSocketAddr& aAddr);
  void Send(const InputStreamParams& aStream, const UDPSocketAddr& aAddr);
  nsresult BindInternal(const nsCString& aHost, const uint16_t& aPort,
                        const bool& aAddressReuse, const bool& aLoopback);

  void FireInternalError(uint32_t aLineNo);

  // One of these will be null and the other non-null.
  PBackgroundParent* mBackgroundManager;
  PNeckoParent* mNeckoManager;

  bool mIPCOpen;
  nsCOMPtr<nsIUDPSocket> mSocket;
  nsCOMPtr<nsIUDPSocketFilter> mFilter;
  nsRefPtr<mozilla::net::OfflineObserver> mObserver;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

} // namespace dom
} // namespace mozilla

#endif // !defined(mozilla_dom_UDPSocketParent_h__)
