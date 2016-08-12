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
#include "nsNetUtil.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "nsIPermissionManager.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mtransport/runnable_utils.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(UDPSocketParent, nsIUDPSocketListener)

UDPSocketParent::UDPSocketParent(PBackgroundParent* aManager)
  : mBackgroundManager(aManager)
  , mNeckoManager(nullptr)
  , mIPCOpen(true)
{
  mObserver = new mozilla::net::OfflineObserver(this);
}

UDPSocketParent::UDPSocketParent(PNeckoParent* aManager)
  : mBackgroundManager(nullptr)
  , mNeckoManager(aManager)
  , mIPCOpen(true)
{
  mObserver = new mozilla::net::OfflineObserver(this);
}

UDPSocketParent::~UDPSocketParent()
{
  if (mObserver) {
    mObserver->RemoveObserver();
  }
}

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

uint32_t
UDPSocketParent::GetAppId()
{
  return mPrincipal ? mPrincipal->GetAppId()
                    : nsIScriptSecurityManager::UNKNOWN_APP_ID;
}

bool
UDPSocketParent::Init(const IPC::Principal& aPrincipal,
                      const nsACString& aFilter)
{
  MOZ_ASSERT_IF(mBackgroundManager, !aPrincipal);
  // will be used once we move all UDPSocket to PBackground, or
  // if we add in Principal checking for mtransport
  Unused << mBackgroundManager;

  mPrincipal = aPrincipal;
  if (net::UsingNeckoIPCSecurity() &&
      mPrincipal &&
      !ContentParent::IgnoreIPCPrincipal()) {
    if (mNeckoManager) {
      if (!AssertAppPrincipal(mNeckoManager->Manager(), mPrincipal)) {
        return false;
      }
    } else {
      // PBackground is (for now) using a STUN filter for verification
      // it's not being used for DoS
    }

    nsCOMPtr<nsIPermissionManager> permMgr =
      services::GetPermissionManager();
    if (!permMgr) {
      NS_WARNING("No PermissionManager available!");
      return false;
    }

    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    permMgr->TestExactPermissionFromPrincipal(mPrincipal, "udp-socket",
                                              &permission);
    if (permission != nsIPermissionManager::ALLOW_ACTION) {
      return false;
    }
  }

  if (!aFilter.IsEmpty()) {
    nsAutoCString contractId(NS_NETWORK_UDP_SOCKET_FILTER_HANDLER_PREFIX);
    contractId.Append(aFilter);
    nsCOMPtr<nsISocketFilterHandler> filterHandler =
      do_GetService(contractId.get());
    if (filterHandler) {
      nsresult rv = filterHandler->NewFilter(getter_AddRefs(mFilter));
      if (NS_FAILED(rv)) {
        printf_stderr("Cannot create filter that content specified. "
                      "filter name: %s, error code: %u.", aFilter.BeginReading(),  static_cast<uint32_t>(rv));
        return false;
      }
    } else {
      printf_stderr("Content doesn't have a valid filter. "
                    "filter name: %s.", aFilter.BeginReading());
      return false;
    }
  }
  // We don't have browser actors in xpcshell, and hence can't run automated
  // tests without this loophole.
  if (net::UsingNeckoIPCSecurity() && !mFilter &&
      (!mPrincipal || ContentParent::IgnoreIPCPrincipal())) {
    return false;
  }
  return true;
}

// PUDPSocketParent methods

bool
UDPSocketParent::RecvBind(const UDPAddressInfo& aAddressInfo,
                          const bool& aAddressReuse, const bool& aLoopback,
                          const uint32_t& recvBufferSize,
                          const uint32_t& sendBufferSize)
{
  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, aAddressInfo.addr().get(), aAddressInfo.port()));

  if (NS_FAILED(BindInternal(aAddressInfo.addr(), aAddressInfo.port(),
                             aAddressReuse, aLoopback, recvBufferSize,
                             sendBufferSize))) {
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

  UDPSOCKET_LOG(("%s: SendCallbackOpened: %s:%u", __FUNCTION__, addr.get(), port));
  mozilla::Unused << SendCallbackOpened(UDPAddressInfo(addr, port));

  return true;
}

nsresult
UDPSocketParent::BindInternal(const nsCString& aHost, const uint16_t& aPort,
                              const bool& aAddressReuse, const bool& aLoopback,
                              const uint32_t& recvBufferSize,
                              const uint32_t& sendBufferSize)
{
  nsresult rv;

  UDPSOCKET_LOG(("%s: [this=%p] %s:%u addressReuse: %d loopback: %d recvBufferSize: %lu, sendBufferSize: %lu",
                __FUNCTION__, this, nsCString(aHost).get(), aPort,
                aAddressReuse, aLoopback, recvBufferSize, sendBufferSize));

  nsCOMPtr<nsIUDPSocket> sock =
      do_CreateInstance("@mozilla.org/network/udp-socket;1", &rv);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aHost.IsEmpty()) {
    rv = sock->Init(aPort, false, mPrincipal, aAddressReuse,
                    /* optional_argc = */ 1);
  } else {
    PRNetAddr prAddr;
    PR_InitializeNetAddr(PR_IpAddrAny, aPort, &prAddr);
    PRStatus status = PR_StringToNetAddr(aHost.BeginReading(), &prAddr);
    if (status != PR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }

    mozilla::net::NetAddr addr;
    PRNetAddrToNetAddr(&prAddr, &addr);
    rv = sock->InitWithAddress(&addr, mPrincipal, aAddressReuse,
                               /* optional_argc = */ 1);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsINetAddr> laddr;
  rv = sock->GetLocalAddr(getter_AddRefs(laddr));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  uint16_t family;
  rv = laddr->GetFamily(&family);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (family == nsINetAddr::FAMILY_INET) {
    rv = sock->SetMulticastLoopback(aLoopback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  // TODO: once bug 1252759 is fixed query buffer first and only increase
  if (recvBufferSize != 0) {
    rv = sock->SetRecvBufferSize(recvBufferSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      UDPSOCKET_LOG(("%s: [this=%p] %s:%u failed to set recv buffer size to: %lu", __FUNCTION__, this, nsCString(aHost).get(), aPort, recvBufferSize));
    }
  }
  if (sendBufferSize != 0) {
    rv = sock->SetSendBufferSize(sendBufferSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      UDPSOCKET_LOG(("%s: [this=%p] %s:%u failed to set send buffer size to: %lu", __FUNCTION__, this, nsCString(aHost).get(), aPort, sendBufferSize));
    }
  }

  // register listener
  rv = sock->AsyncListen(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mSocket = sock;

  return NS_OK;
}


static nsCOMPtr<nsIEventTarget> GetSTSThread()
{
  nsresult rv;

  nsCOMPtr<nsIEventTarget> sts_thread;

  sts_thread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return sts_thread;
}

static void CheckSTSThread()
{
  nsCOMPtr<nsIEventTarget> sts_thread = GetSTSThread();

  ASSERT_ON_THREAD(sts_thread);
}


// Proxy the Connect() request to the STS thread, since it may block and
// should be done there.
bool
UDPSocketParent::RecvConnect(const UDPAddressInfo& aAddressInfo)
{
  nsCOMPtr<nsIEventTarget> thread(NS_GetCurrentThread());
  NS_WARN_IF(NS_FAILED(GetSTSThread()->Dispatch(WrapRunnable(
                                                  this,
                                                  &UDPSocketParent::DoConnect,
                                                  mSocket,
                                                  thread,
                                                  aAddressInfo),
                                                NS_DISPATCH_NORMAL)));
  return true;
}

void
UDPSocketParent::DoSendConnectResponse(const UDPAddressInfo& aAddressInfo)
{
  // can't use directly with WrapRunnable due to warnings
  mozilla::Unused << SendCallbackConnected(aAddressInfo);
}

void
UDPSocketParent::SendConnectResponse(nsIEventTarget *aThread,
                                     const UDPAddressInfo& aAddressInfo)
{
  NS_WARN_IF(NS_FAILED(aThread->Dispatch(WrapRunnable(
                                           this,
                                           &UDPSocketParent::DoSendConnectResponse,
                                           aAddressInfo),
                                         NS_DISPATCH_NORMAL)));
}

// Runs on STS thread
void
UDPSocketParent::DoConnect(nsCOMPtr<nsIUDPSocket>& aSocket,
                           nsCOMPtr<nsIEventTarget>& aReturnThread,
                           const UDPAddressInfo& aAddressInfo)
{
  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, aAddressInfo.addr().get(), aAddressInfo.port()));
  if (NS_FAILED(ConnectInternal(aAddressInfo.addr(), aAddressInfo.port()))) {
    SendInternalError(aReturnThread, __LINE__);
    return;
  }
  CheckSTSThread();

  nsCOMPtr<nsINetAddr> localAddr;
  aSocket->GetLocalAddr(getter_AddRefs(localAddr));

  nsCString addr;
  if (NS_FAILED(localAddr->GetAddress(addr))) {
    SendInternalError(aReturnThread, __LINE__);
    return;
  }

  uint16_t port;
  if (NS_FAILED(localAddr->GetPort(&port))) {
    SendInternalError(aReturnThread, __LINE__);
    return;
  }

  UDPSOCKET_LOG(("%s: SendConnectResponse: %s:%u", __FUNCTION__, addr.get(), port));
  SendConnectResponse(aReturnThread, UDPAddressInfo(addr, port));
}

nsresult
UDPSocketParent::ConnectInternal(const nsCString& aHost, const uint16_t& aPort)
{
  nsresult rv;

  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, nsCString(aHost).get(), aPort));
  PRNetAddr prAddr;
  PR_InitializeNetAddr(PR_IpAddrAny, aPort, &prAddr);
  PRStatus status = PR_StringToNetAddr(aHost.BeginReading(), &prAddr);
  if (status != PR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  mozilla::net::NetAddr addr;
  PRNetAddrToNetAddr(&prAddr, &addr);

  rv = mSocket->Connect(&addr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool
UDPSocketParent::RecvOutgoingData(const UDPData& aData,
                                  const UDPSocketAddr& aAddr)
{
  MOZ_ASSERT(mSocket);

  nsresult rv;
  if (mFilter) {
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
                               data.Length(), nsISocketFilter::SF_OUTGOING,
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

  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

bool
UDPSocketParent::RecvRequestDelete()
{
  mozilla::Unused << Send__delete__(this);
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
  UDPSOCKET_LOG(("%s: %s:%u, length %u", __FUNCTION__, ip.get(), port, len));

  if (mFilter) {
    bool allowed;
    mozilla::net::NetAddr addr;
    fromAddr->GetNetAddr(&addr);
    nsresult rv = mFilter->FilterPacket(&addr,
                                        (const uint8_t*)buffer, len,
                                        nsISocketFilter::SF_INCOMING,
                                        &allowed);
    // Receiving unallowed data, drop.
    if (NS_WARN_IF(NS_FAILED(rv)) || !allowed) {
      if (!allowed) {
        UDPSOCKET_LOG(("%s: not allowed", __FUNCTION__));
      }
      return NS_OK;
    }
  }

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, buffer, len, fallible)) {
    FireInternalError(__LINE__);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InfallibleTArray<uint8_t> infallibleArray;
  infallibleArray.SwapElements(fallibleArray);

  // compose callback
  mozilla::Unused << SendCallbackReceivedData(UDPAddressInfo(ip, port), infallibleArray);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocketParent::OnStopListening(nsIUDPSocket* aSocket, nsresult aStatus)
{
  // underlying socket is dead, send state update to child process
  if (mIPCOpen) {
    mozilla::Unused << SendCallbackClosed();
  }
  return NS_OK;
}

void
UDPSocketParent::FireInternalError(uint32_t aLineNo)
{
  if (!mIPCOpen) {
    return;
  }

  mozilla::Unused << SendCallbackError(NS_LITERAL_CSTRING("Internal error"),
                                       NS_LITERAL_CSTRING(__FILE__), aLineNo);
}

void
UDPSocketParent::SendInternalError(nsIEventTarget *aThread,
                                   uint32_t aLineNo)
{
  UDPSOCKET_LOG(("SendInternalError: %u", aLineNo));
  NS_WARN_IF(NS_FAILED(aThread->Dispatch(WrapRunnable(
                                           this,
                                           &UDPSocketParent::FireInternalError,
                                           aLineNo),
                                         NS_DISPATCH_NORMAL)));
}

} // namespace dom
} // namespace mozilla
