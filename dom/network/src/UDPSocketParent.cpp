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
#include "mozilla/AppProcessChecker.h"
#include "mozilla/unused.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/net/DNS.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/PNeckoParent.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(UDPSocketParent, nsIUDPSocketListener)

UDPSocketParent::~UDPSocketParent()
{
}

bool
UDPSocketParent::Init(const nsACString& aFilter)
{
  if (!aFilter.IsEmpty()) {
    nsAutoCString contractId(NS_NETWORK_UDP_SOCKET_FILTER_HANDLER_PREFIX);
    contractId.Append(aFilter);
    nsCOMPtr<nsIUDPSocketFilterHandler> filterHandler =
      do_GetService(contractId.get());
    if (filterHandler) {
      nsresult rv = filterHandler->NewFilter(getter_AddRefs(mFilter));
      if (NS_FAILED(rv)) {
        printf_stderr("Cannot create filter that content specified. "
                      "filter name: %s, error code: %d.", aFilter.BeginReading(), rv);
        return false;
      }
    } else {
      printf_stderr("Content doesn't have a valid filter. "
                    "filter name: %s.", aFilter.BeginReading());
      return false;
    }
  }
  return true;
}

// PUDPSocketParent methods

bool
UDPSocketParent::RecvBind(const UDPAddressInfo& aAddressInfo,
                          const bool& aAddressReuse, const bool& aLoopback)
{
  // We don't have browser actors in xpcshell, and hence can't run automated
  // tests without this loophole.
  if (net::UsingNeckoIPCSecurity() && !mFilter &&
      !AssertAppProcessPermission(Manager()->Manager(), "udp-socket")) {
    FireInternalError(__LINE__);
    return false;
  }

  if (NS_FAILED(BindInternal(aAddressInfo.addr(), aAddressInfo.port(), aAddressReuse, aLoopback))) {
    FireInternalError(__LINE__);
    return true;
  }

  nsCOMPtr<nsINetAddr> localAddr;
  mSocket->GetLocalAddr(getter_AddRefs(localAddr));

  nsCString addr;
  if (NS_FAILED(localAddr->GetAddress(addr))) {
    FireInternalError(__LINE__);
    return true;
  }

  uint16_t port;
  if (NS_FAILED(localAddr->GetPort(&port))) {
    FireInternalError(__LINE__);
    return true;
  }

  mozilla::unused << SendCallbackOpened(UDPAddressInfo(addr, port));

  return true;
}

nsresult
UDPSocketParent::BindInternal(const nsCString& aHost, const uint16_t& aPort,
                              const bool& aAddressReuse, const bool& aLoopback)
{
  nsresult rv;

  nsCOMPtr<nsIUDPSocket> sock =
      do_CreateInstance("@mozilla.org/network/udp-socket;1", &rv);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aHost.IsEmpty()) {
    rv = sock->Init(aPort, false, aAddressReuse, /* optional_argc = */ 1);
  } else {
    PRNetAddr prAddr;
    PR_InitializeNetAddr(PR_IpAddrAny, aPort, &prAddr);
    PRStatus status = PR_StringToNetAddr(aHost.BeginReading(), &prAddr);
    if (status != PR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }

    mozilla::net::NetAddr addr;
    PRNetAddrToNetAddr(&prAddr, &addr);
    rv = sock->InitWithAddress(&addr, aAddressReuse, /* optional_argc = */ 1);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = sock->SetMulticastLoopback(aLoopback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // register listener
  rv = sock->AsyncListen(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mSocket = sock;

  return NS_OK;
}

bool
UDPSocketParent::RecvOutgoingData(const UDPData& aData,
                                  const UDPSocketAddr& aAddr)
{
  MOZ_ASSERT(mSocket);

  nsresult rv;
  if (mFilter) {
    // TODO, Bug 933102, filter packets that are sent with hostname.
    // Until then we simply throw away packets that are sent to a hostname.
    if (aAddr.type() != UDPSocketAddr::TNetAddr) {
      return true;
    }

    // TODO, Packet filter doesn't support input stream yet.
    if (aData.type() != UDPData::TArrayOfuint8_t) {
      return true;
    }

    bool allowed;
    const InfallibleTArray<uint8_t>& data(aData.get_ArrayOfuint8_t());
    rv = mFilter->FilterPacket(&aAddr.get_NetAddr(), data.Elements(),
                               data.Length(), nsIUDPSocketFilter::SF_OUTGOING,
                               &allowed);

    // Sending unallowed data, kill content.
    if (NS_WARN_IF(NS_FAILED(rv)) || !allowed) {
      return false;
    }
  }

  switch(aData.type()) {
    case UDPData::TArrayOfuint8_t:
      Send(aData.get_ArrayOfuint8_t(), aAddr);
      break;
    case UDPData::TInputStreamParams:
      Send(aData.get_InputStreamParams(), aAddr);
      break;
    default:
      MOZ_ASSERT(false, "Invalid data type!");
      return true;
  }

  return true;
}

void
UDPSocketParent::Send(const InfallibleTArray<uint8_t>& aData,
                      const UDPSocketAddr& aAddr)
{
  nsresult rv;
  uint32_t count;
  switch(aAddr.type()) {
    case UDPSocketAddr::TUDPAddressInfo: {
      const UDPAddressInfo& addrInfo(aAddr.get_UDPAddressInfo());
      rv = mSocket->Send(addrInfo.addr(), addrInfo.port(),
                         aData.Elements(), aData.Length(), &count);
      break;
    }
    case UDPSocketAddr::TNetAddr: {
      const NetAddr& addr(aAddr.get_NetAddr());
      rv = mSocket->SendWithAddress(&addr, aData.Elements(),
                                    aData.Length(), &count);
      break;
    }
    default:
      MOZ_ASSERT(false, "Invalid address type!");
      return;
  }

  if (NS_WARN_IF(NS_FAILED(rv)) || count == 0) {
    FireInternalError(__LINE__);
  }
}

void
UDPSocketParent::Send(const InputStreamParams& aStream,
                      const UDPSocketAddr& aAddr)
{
  nsTArray<mozilla::ipc::FileDescriptor> fds;
  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(aStream, fds);

  if (NS_WARN_IF(!stream)) {
    return;
  }

  nsresult rv;
  switch(aAddr.type()) {
    case UDPSocketAddr::TUDPAddressInfo: {
      const UDPAddressInfo& addrInfo(aAddr.get_UDPAddressInfo());
      rv = mSocket->SendBinaryStream(addrInfo.addr(), addrInfo.port(), stream);
      break;
    }
    case UDPSocketAddr::TNetAddr: {
      const NetAddr& addr(aAddr.get_NetAddr());
      rv = mSocket->SendBinaryStreamWithAddress(&addr, stream);
      break;
    }
    default:
      MOZ_ASSERT(false, "Invalid address type!");
      return;
  }

  if (NS_FAILED(rv)) {
    FireInternalError(__LINE__);
  }
}

bool
UDPSocketParent::RecvJoinMulticast(const nsCString& aMulticastAddress,
                                   const nsCString& aInterface)
{
  nsresult rv = mSocket->JoinMulticast(aMulticastAddress, aInterface);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    FireInternalError(__LINE__);
  }

  return true;
}

bool
UDPSocketParent::RecvLeaveMulticast(const nsCString& aMulticastAddress,
                                    const nsCString& aInterface)
{
  nsresult rv = mSocket->LeaveMulticast(aMulticastAddress, aInterface);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    FireInternalError(__LINE__);
  }

  return true;
}

bool
UDPSocketParent::RecvClose()
{
  if (!mSocket) {
    return true;
  }

  nsresult rv = mSocket->Close();
  mSocket = nullptr;

  mozilla::unused << NS_WARN_IF(NS_FAILED(rv));

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

  if (mFilter) {
    bool allowed;
    mozilla::net::NetAddr addr;
    fromAddr->GetNetAddr(&addr);
    nsresult rv = mFilter->FilterPacket(&addr,
                                        (const uint8_t*)buffer, len,
                                        nsIUDPSocketFilter::SF_INCOMING,
                                        &allowed);
    // Receiving unallowed data, drop.
    if (NS_WARN_IF(NS_FAILED(rv)) || !allowed) {
      return NS_OK;
    }
  }

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, buffer, len)) {
    FireInternalError(__LINE__);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InfallibleTArray<uint8_t> infallibleArray;
  infallibleArray.SwapElements(fallibleArray);

  // compose callback
  mozilla::unused << SendCallbackReceivedData(UDPAddressInfo(ip, port), infallibleArray);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocketParent::OnStopListening(nsIUDPSocket* aSocket, nsresult aStatus)
{
  // underlying socket is dead, send state update to child process
  if (mIPCOpen) {
    mozilla::unused << SendCallbackClosed();
  }
  return NS_OK;
}

void
UDPSocketParent::FireInternalError(uint32_t aLineNo)
{
  if (!mIPCOpen) {
    return;
  }

  mozilla::unused << SendCallbackError(NS_LITERAL_CSTRING("Internal error"),
                                       NS_LITERAL_CSTRING(__FILE__), aLineNo);
}

} // namespace dom
} // namespace mozilla
