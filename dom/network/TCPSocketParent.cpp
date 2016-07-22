/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPSocketParent.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsJSUtils.h"
#include "mozilla/unused.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/PNeckoParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"

namespace IPC {

//Defined in TCPSocketChild.cpp
extern bool
DeserializeArrayBuffer(JSContext* aCx,
                       const InfallibleTArray<uint8_t>& aBuffer,
                       JS::MutableHandle<JS::Value> aVal);

} // namespace IPC

namespace mozilla {

namespace net {
//
// set MOZ_LOG=TCPSocket:5
//
extern LazyLogModule gTCPSocketLog;
#define TCPSOCKET_LOG(args)     MOZ_LOG(gTCPSocketLog, LogLevel::Debug, args)
#define TCPSOCKET_LOG_ENABLED() MOZ_LOG_TEST(gTCPSocketLog, LogLevel::Debug)
} // namespace net

namespace dom {

static void
FireInteralError(mozilla::net::PTCPSocketParent* aActor, uint32_t aLineNo)
{
  mozilla::Unused <<
      aActor->SendCallback(NS_LITERAL_STRING("onerror"),
                           TCPError(NS_LITERAL_STRING("InvalidStateError"), NS_LITERAL_STRING("Internal error")),
                           static_cast<uint32_t>(TCPReadyState::Connecting));
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPSocketParentBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(TCPSocketParentBase, mSocket)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPSocketParentBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPSocketParentBase)

TCPSocketParentBase::TCPSocketParentBase()
: mIPCOpen(false)
{
  mObserver = new mozilla::net::OfflineObserver(this);
}

TCPSocketParentBase::~TCPSocketParentBase()
{
  if (mObserver) {
    mObserver->RemoveObserver();
  }
}

uint32_t
TCPSocketParent::GetAppId()
{
  const PContentParent *content = Manager()->Manager();
  if (PBrowserParent* browser = SingleManagedOrNull(content->ManagedPBrowserParent())) {
    TabParent *tab = TabParent::GetFrom(browser);
    return tab->OwnAppId();
  } else {
    return nsIScriptSecurityManager::UNKNOWN_APP_ID;
  }
};

bool
TCPSocketParent::GetInIsolatedMozBrowser()
{
  const PContentParent *content = Manager()->Manager();
  if (PBrowserParent* browser = SingleManagedOrNull(content->ManagedPBrowserParent())) {
    TabParent *tab = TabParent::GetFrom(browser);
    return tab->IsIsolatedMozBrowserElement();
  } else {
    return false;
  }
}

nsresult
TCPSocketParent::OfflineNotification(nsISupports *aSubject)
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
    mSocket = nullptr;
  }

  return NS_OK;
}


void
TCPSocketParentBase::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  this->Release();
}

void
TCPSocketParentBase::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType) TCPSocketParent::Release(void)
{
  nsrefcnt refcnt = TCPSocketParentBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    mozilla::Unused << PTCPSocketParent::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

bool
TCPSocketParent::RecvOpen(const nsString& aHost, const uint16_t& aPort, const bool& aUseSSL,
                          const bool& aUseArrayBuffers)
{
  // We don't have browser actors in xpcshell, and hence can't run automated
  // tests without this loophole.
  if (net::UsingNeckoIPCSecurity() &&
      !AssertAppProcessPermission(Manager()->Manager(), "tcp-socket")) {
    FireInteralError(this, __LINE__);
    return true;
  }

  // Obtain App ID
  uint32_t appId = GetAppId();
  bool     inIsolatedMozBrowser = GetInIsolatedMozBrowser();

  if (NS_IsAppOffline(appId)) {
    NS_ERROR("Can't open socket because app is offline");
    FireInteralError(this, __LINE__);
    return true;
  }

  mSocket = new TCPSocket(nullptr, aHost, aPort, aUseSSL, aUseArrayBuffers);
  mSocket->SetAppIdAndBrowser(appId, inIsolatedMozBrowser);
  mSocket->SetSocketBridgeParent(this);
  NS_ENSURE_SUCCESS(mSocket->Init(), true);
  return true;
}

bool
TCPSocketParent::RecvOpenBind(const nsCString& aRemoteHost,
                              const uint16_t& aRemotePort,
                              const nsCString& aLocalAddr,
                              const uint16_t& aLocalPort,
                              const bool&     aUseSSL,
                              const bool&     aUseArrayBuffers,
                              const nsCString& aFilter)
{
  if (net::UsingNeckoIPCSecurity() &&
      !AssertAppProcessPermission(Manager()->Manager(), "tcp-socket")) {
    FireInteralError(this, __LINE__);
    return true;
  }

  nsresult rv;
  nsCOMPtr<nsISocketTransportService> sts =
    do_GetService("@mozilla.org/network/socket-transport-service;1", &rv);
  if (NS_FAILED(rv)) {
    FireInteralError(this, __LINE__);
    return true;
  }

  nsCOMPtr<nsISocketTransport> socketTransport;
  rv = sts->CreateTransport(nullptr, 0,
                            aRemoteHost, aRemotePort,
                            nullptr, getter_AddRefs(socketTransport));
  if (NS_FAILED(rv)) {
    FireInteralError(this, __LINE__);
    return true;
  }

  PRNetAddr prAddr;
  if (PR_SUCCESS != PR_InitializeNetAddr(PR_IpAddrAny, aLocalPort, &prAddr)) {
    FireInteralError(this, __LINE__);
    return true;
  }
  if (PR_SUCCESS != PR_StringToNetAddr(aLocalAddr.BeginReading(), &prAddr)) {
    FireInteralError(this, __LINE__);
    return true;
  }

  mozilla::net::NetAddr addr;
  PRNetAddrToNetAddr(&prAddr, &addr);
  rv = socketTransport->Bind(&addr);
  if (NS_FAILED(rv)) {
    FireInteralError(this, __LINE__);
    return true;
  }

  if (!aFilter.IsEmpty()) {
    nsAutoCString contractId(NS_NETWORK_TCP_SOCKET_FILTER_HANDLER_PREFIX);
    contractId.Append(aFilter);
    nsCOMPtr<nsISocketFilterHandler> filterHandler =
      do_GetService(contractId.get());
    if (!filterHandler) {
      NS_ERROR("Content doesn't have a valid filter");
      FireInteralError(this, __LINE__);
      return true;
    }
    rv = filterHandler->NewFilter(getter_AddRefs(mFilter));
    if (NS_FAILED(rv)) {
      NS_ERROR("Cannot create filter that content specified");
      FireInteralError(this, __LINE__);
      return true;
    }
  }

  // Obtain App ID
  uint32_t appId = nsIScriptSecurityManager::NO_APP_ID;
  bool     inIsolatedMozBrowser = false;
  const PContentParent *content = Manager()->Manager();
  if (PBrowserParent* browser = SingleManagedOrNull(content->ManagedPBrowserParent())) {
    // appId's are for B2G only currently, where managees.Count() == 1
    // This is not guaranteed currently in Desktop, so skip this there.
    TabParent *tab = TabParent::GetFrom(browser);
    appId = tab->OwnAppId();
    inIsolatedMozBrowser = tab->IsIsolatedMozBrowserElement();
  }

  mSocket = new TCPSocket(nullptr, NS_ConvertUTF8toUTF16(aRemoteHost), aRemotePort, aUseSSL, aUseArrayBuffers);
  mSocket->SetAppIdAndBrowser(appId, inIsolatedMozBrowser);
  mSocket->SetSocketBridgeParent(this);
  rv = mSocket->InitWithUnconnectedTransport(socketTransport);
  NS_ENSURE_SUCCESS(rv, true);
  return true;
}

bool
TCPSocketParent::RecvStartTLS()
{
  NS_ENSURE_TRUE(mSocket, true);
  ErrorResult rv;
  mSocket->UpgradeToSecure(rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }

  return true;
}

bool
TCPSocketParent::RecvSuspend()
{
  NS_ENSURE_TRUE(mSocket, true);
  mSocket->Suspend();
  return true;
}

bool
TCPSocketParent::RecvResume()
{
  NS_ENSURE_TRUE(mSocket, true);
  ErrorResult rv;
  mSocket->Resume(rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }

  return true;
}

bool
TCPSocketParent::RecvData(const SendableData& aData,
                          const uint32_t& aTrackingNumber)
{
  ErrorResult rv;

  if (mFilter) {
    mozilla::net::NetAddr addr; // dummy value
    bool allowed;
    MOZ_ASSERT(aData.type() == SendableData::TArrayOfuint8_t,
               "Unsupported data type for filtering");
    const InfallibleTArray<uint8_t>& data(aData.get_ArrayOfuint8_t());
    nsresult nsrv = mFilter->FilterPacket(&addr, data.Elements(),
                                          data.Length(),
                                          nsISocketFilter::SF_OUTGOING,
                                          &allowed);

    // Reject sending of unallowed data
    if (NS_WARN_IF(NS_FAILED(nsrv)) || !allowed) {
      TCPSOCKET_LOG(("%s: Dropping outgoing TCP packet", __FUNCTION__));
      return false;
    }
  }

  switch (aData.type()) {
    case SendableData::TArrayOfuint8_t: {
      AutoSafeJSContext autoCx;
      JS::Rooted<JS::Value> val(autoCx);
      const nsTArray<uint8_t>& buffer = aData.get_ArrayOfuint8_t();
      bool ok = IPC::DeserializeArrayBuffer(autoCx, buffer, &val);
      NS_ENSURE_TRUE(ok, true);
      RootedTypedArray<ArrayBuffer> data(autoCx);
      data.Init(&val.toObject());
      Optional<uint32_t> byteLength(buffer.Length());
      mSocket->SendWithTrackingNumber(autoCx, data, 0, byteLength, aTrackingNumber, rv);
      break;
    }

    case SendableData::TnsCString: {
      const nsCString& strData = aData.get_nsCString();
      mSocket->SendWithTrackingNumber(strData, aTrackingNumber, rv);
      break;
    }

    default:
      MOZ_CRASH("unexpected SendableData type");
  }
  NS_ENSURE_SUCCESS(rv.StealNSResult(), true);
  return true;
}

bool
TCPSocketParent::RecvClose()
{
  NS_ENSURE_TRUE(mSocket, true);
  mSocket->Close();
  return true;
}

void
TCPSocketParent::FireErrorEvent(const nsAString& aName, const nsAString& aType, TCPReadyState aReadyState)
{
  SendEvent(NS_LITERAL_STRING("error"), TCPError(nsString(aName), nsString(aType)), aReadyState);
}

void
TCPSocketParent::FireEvent(const nsAString& aType, TCPReadyState aReadyState)
{
  return SendEvent(aType, mozilla::void_t(), aReadyState);
}

void
TCPSocketParent::FireArrayBufferDataEvent(nsTArray<uint8_t>& aBuffer, TCPReadyState aReadyState)
{
  InfallibleTArray<uint8_t> arr;
  arr.SwapElements(aBuffer);

  if (mFilter) {
    bool allowed;
    mozilla::net::NetAddr addr;
    nsresult nsrv = mFilter->FilterPacket(&addr, arr.Elements(), arr.Length(),
                                          nsISocketFilter::SF_INCOMING,
                                          &allowed);
    // receiving unallowed data, drop it.
    if (NS_WARN_IF(NS_FAILED(nsrv)) || !allowed) {
      TCPSOCKET_LOG(("%s: Dropping incoming TCP packet", __FUNCTION__));
      return;
    }
  }

  SendableData data(arr);
  SendEvent(NS_LITERAL_STRING("data"), data, aReadyState);
}

void
TCPSocketParent::FireStringDataEvent(const nsACString& aData, TCPReadyState aReadyState)
{
  SendableData data((nsCString(aData)));

  MOZ_ASSERT(!mFilter, "Socket filtering doesn't support nsCString");

  SendEvent(NS_LITERAL_STRING("data"), data, aReadyState);
}

void
TCPSocketParent::SendEvent(const nsAString& aType, CallbackData aData, TCPReadyState aReadyState)
{
  mozilla::Unused << PTCPSocketParent::SendCallback(nsString(aType), aData,
                                                    static_cast<uint32_t>(aReadyState));
}

void
TCPSocketParent::SetSocket(TCPSocket *socket)
{
  mSocket = socket;
}

nsresult
TCPSocketParent::GetHost(nsAString& aHost)
{
  if (!mSocket) {
    NS_ERROR("No internal socket instance mSocket!");
    return NS_ERROR_FAILURE;
  }
  mSocket->GetHost(aHost);
  return NS_OK;
}

nsresult
TCPSocketParent::GetPort(uint16_t* aPort)
{
  if (!mSocket) {
    NS_ERROR("No internal socket instance mSocket!");
    return NS_ERROR_FAILURE;
  }
  *aPort = mSocket->Port();
  return NS_OK;
}

void
TCPSocketParent::ActorDestroy(ActorDestroyReason why)
{
  if (mSocket) {
    mSocket->Close();
  }
  mSocket = nullptr;
}

bool
TCPSocketParent::RecvRequestDelete()
{
  mozilla::Unused << Send__delete__(this);
  return true;
}

} // namespace dom
} // namespace mozilla
