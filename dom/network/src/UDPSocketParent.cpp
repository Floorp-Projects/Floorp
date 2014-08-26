/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "UDPSocketParent.h"
#include "nsComponentManagerUtils.h"
#include "nsIUDPSocket.h"
#include "nsINetAddr.h"
#include "mozilla/unused.h"
#include "mozilla/net/DNS.h"
#include "nsNetUtil.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/net/PNeckoParent.h"

namespace mozilla {
namespace dom {

static void
FireInternalError(mozilla::net::PUDPSocketParent *aActor, uint32_t aLineNo)
{
  mozilla::unused <<
      aActor->SendCallback(NS_LITERAL_CSTRING("onerror"),
                          UDPError(NS_LITERAL_CSTRING("Internal error"),
                                   NS_LITERAL_CSTRING(__FILE__), aLineNo, 0),
                          NS_LITERAL_CSTRING("connecting"));
}

static nsresult
ConvertNetAddrToString(mozilla::net::NetAddr &netAddr, nsACString *address, uint16_t *port)
{
  NS_ENSURE_ARG_POINTER(address);
  NS_ENSURE_ARG_POINTER(port);

  *port = 0;
  uint32_t bufSize = 0;

  switch(netAddr.raw.family) {
  case AF_INET:
    *port = PR_ntohs(netAddr.inet.port);
    bufSize = mozilla::net::kIPv4CStrBufSize;
    break;
  case AF_INET6:
    *port = PR_ntohs(netAddr.inet6.port);
    bufSize = mozilla::net::kIPv6CStrBufSize;
    break;
  default:
    //impossible
    MOZ_ASSERT(false, "Unexpected address family");
    return NS_ERROR_INVALID_ARG;
  }

  address->SetCapacity(bufSize);
  NetAddrToString(&netAddr, address->BeginWriting(), bufSize);
  address->SetLength(strlen(address->BeginReading()));

  return NS_OK;
}

NS_IMPL_ISUPPORTS(UDPSocketParent, nsIUDPSocketListener)

UDPSocketParent::UDPSocketParent(nsIUDPSocketFilter *filter)
  : mIPCOpen(true)
  , mFilter(filter)
{
  mObserver = new mozilla::net::OfflineObserver(this);
}

UDPSocketParent::~UDPSocketParent()
{
  if (mObserver) {
    mObserver->RemoveObserver();
  }
}

uint32_t
UDPSocketParent::GetAppId()
{
  uint32_t appId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
  const PContentParent *content = Manager()->Manager();
  const InfallibleTArray<PBrowserParent*>& browsers = content->ManagedPBrowserParent();
  if (browsers.Length() > 0) {
    TabParent *tab = static_cast<TabParent*>(browsers[0]);
    appId = tab->OwnAppId();
  }
  return appId;
};

nsresult
UDPSocketParent::OfflineNotification(nsISupports *aSubject)
{
  nsCOMPtr<nsIAppOfflineInfo> info(do_QueryInterface(aSubject));
  if (!info) {
    return NS_OK;
  }

  uint32_t targetAppId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
  info->GetAppId(&targetAppId);

  // Obtain App ID
  uint32_t appId = GetAppId();

  if (appId != targetAppId) {
    return NS_OK;
  }

  // If the app is offline, close the socket
  if (mSocket && NS_IsAppOffline(appId)) {
    mSocket->Close();
  }

  return NS_OK;
}


// PUDPSocketParent methods

bool
UDPSocketParent::Init(const nsCString &aHost, const uint16_t aPort)
{
  nsresult rv;
  NS_ASSERTION(mFilter, "No packet filter");

  nsCOMPtr<nsIUDPSocket> sock =
      do_CreateInstance("@mozilla.org/network/udp-socket;1", &rv);
  if (NS_FAILED(rv)) {
    FireInternalError(this, __LINE__);
    return true;
  }

  if (aHost.IsEmpty()) {
    rv = sock->Init(aPort, false);
  } else {
    PRNetAddr prAddr;
    PR_InitializeNetAddr(PR_IpAddrAny, aPort, &prAddr);
    PRStatus status = PR_StringToNetAddr(aHost.BeginReading(), &prAddr);
    if (status != PR_SUCCESS) {
      FireInternalError(this, __LINE__);
      return true;
    }

    mozilla::net::NetAddr addr;
    PRNetAddrToNetAddr(&prAddr, &addr);
    rv = sock->InitWithAddress(&addr);
  }

  if (NS_FAILED(rv)) {
    FireInternalError(this, __LINE__);
    return true;
  }

  mSocket = sock;

  net::NetAddr localAddr;
  mSocket->GetAddress(&localAddr);

  uint16_t port;
  nsCString addr;
  rv = ConvertNetAddrToString(localAddr, &addr, &port);

  if (NS_FAILED(rv)) {
    FireInternalError(this, __LINE__);
    return true;
  }

  // register listener
  mSocket->AsyncListen(this);
  mozilla::unused <<
      PUDPSocketParent::SendCallback(NS_LITERAL_CSTRING("onopen"),
                                     UDPAddressInfo(addr, port),
                                     NS_LITERAL_CSTRING("connected"));

  return true;
}

bool
UDPSocketParent::RecvData(const InfallibleTArray<uint8_t> &aData,
                          const nsCString& aRemoteAddress,
                          const uint16_t& aPort)
{
  NS_ENSURE_TRUE(mSocket, true);
  NS_ASSERTION(mFilter, "No packet filter");
  // TODO, Bug 933102, filter packets that are sent with hostname.
  // Until then we simply throw away packets that are sent to a hostname.
  return true;

#if 0
  // Enable this once we have filtering working with hostname delivery.
  uint32_t count;
  nsresult rv = mSocket->Send(aRemoteAddress,
                              aPort, aData.Elements(),
                              aData.Length(), &count);
  mozilla::unused <<
      PUDPSocketParent::SendCallback(NS_LITERAL_CSTRING("onsent"),
                                     UDPSendResult(rv),
                                     NS_LITERAL_CSTRING("connected"));
  NS_ENSURE_SUCCESS(rv, true);
  NS_ENSURE_TRUE(count > 0, true);
  return true;
#endif
}

bool
UDPSocketParent::RecvDataWithAddress(const InfallibleTArray<uint8_t>& aData,
                                     const mozilla::net::NetAddr& aAddr)
{
  NS_ENSURE_TRUE(mSocket, true);
  NS_ASSERTION(mFilter, "No packet filter");

  uint32_t count;
  nsresult rv;
  bool allowed;
  rv = mFilter->FilterPacket(&aAddr, aData.Elements(),
                             aData.Length(), nsIUDPSocketFilter::SF_OUTGOING,
                             &allowed);
  // Sending unallowed data, kill content.
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(allowed, false);

  rv = mSocket->SendWithAddress(&aAddr, aData.Elements(),
                                aData.Length(), &count);
  mozilla::unused <<
      PUDPSocketParent::SendCallback(NS_LITERAL_CSTRING("onsent"),
                                     UDPSendResult(rv),
                                     NS_LITERAL_CSTRING("connected"));
  NS_ENSURE_SUCCESS(rv, true);
  NS_ENSURE_TRUE(count > 0, true);
  return true;
}

bool
UDPSocketParent::RecvClose()
{
  NS_ENSURE_TRUE(mSocket, true);
  nsresult rv = mSocket->Close();
  mSocket = nullptr;
  NS_ENSURE_SUCCESS(rv, true);
  return true;
}

bool
UDPSocketParent::RecvRequestDelete()
{
  mozilla::unused << Send__delete__(this);
  return true;
}

void
UDPSocketParent::ActorDestroy(ActorDestroyReason why)
{
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  if (mSocket) {
    mSocket->Close();
  }
  mSocket = nullptr;
}

// nsIUDPSocketListener

NS_IMETHODIMP
UDPSocketParent::OnPacketReceived(nsIUDPSocket* aSocket, nsIUDPMessage* aMessage)
{
  // receiving packet from remote host, forward the message content to child process
  if (!mIPCOpen) {
    return NS_OK;
  }
  NS_ASSERTION(mFilter, "No packet filter");

  uint16_t port;
  nsCString ip;
  nsCOMPtr<nsINetAddr> fromAddr;
  aMessage->GetFromAddr(getter_AddRefs(fromAddr));
  fromAddr->GetPort(&port);
  fromAddr->GetAddress(ip);

  nsCString data;
  aMessage->GetData(data);

  const char* buffer = data.get();
  uint32_t len = data.Length();

  bool allowed;
  mozilla::net::NetAddr addr;
  fromAddr->GetNetAddr(&addr);
  nsresult rv = mFilter->FilterPacket(&addr,
                                      (const uint8_t*)buffer, len,
                                      nsIUDPSocketFilter::SF_INCOMING,
                                      &allowed);
  // Receiving unallowed data, drop.
  NS_ENSURE_SUCCESS(rv, NS_OK);
  NS_ENSURE_TRUE(allowed, NS_OK);

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, buffer, len)) {
    FireInternalError(this, __LINE__);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InfallibleTArray<uint8_t> infallibleArray;
  infallibleArray.SwapElements(fallibleArray);

  // compose callback
  mozilla::unused <<
      PUDPSocketParent::SendCallback(NS_LITERAL_CSTRING("ondata"),
                                     UDPMessage(ip, port, infallibleArray),
                                     NS_LITERAL_CSTRING("connected"));

  return NS_OK;
}

NS_IMETHODIMP
UDPSocketParent::OnStopListening(nsIUDPSocket* aSocket, nsresult aStatus)
{
  // underlying socket is dead, send state update to child process
  if (mIPCOpen) {
    mozilla::unused <<
        PUDPSocketParent::SendCallback(NS_LITERAL_CSTRING("onclose"),
                                       mozilla::void_t(),
                                       NS_LITERAL_CSTRING("closed"));
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
