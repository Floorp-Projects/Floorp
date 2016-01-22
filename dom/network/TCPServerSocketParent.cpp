/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIScriptSecurityManager.h"
#include "TCPServerSocket.h"
#include "TCPServerSocketParent.h"
#include "nsJSUtils.h"
#include "TCPSocketParent.h"
#include "mozilla/unused.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(TCPServerSocketParent, mServerSocket)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPServerSocketParent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPServerSocketParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPServerSocketParent)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
TCPServerSocketParent::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  this->Release();
}

void
TCPServerSocketParent::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

TCPServerSocketParent::TCPServerSocketParent(PNeckoParent* neckoParent,
                                             uint16_t aLocalPort,
                                             uint16_t aBacklog,
                                             bool aUseArrayBuffers)
: mNeckoParent(neckoParent)
, mIPCOpen(false)
{
  mServerSocket = new TCPServerSocket(nullptr, aLocalPort, aUseArrayBuffers, aBacklog);
  mServerSocket->SetServerBridgeParent(this);
}

TCPServerSocketParent::~TCPServerSocketParent()
{
}

void
TCPServerSocketParent::Init()
{
  NS_ENSURE_SUCCESS_VOID(mServerSocket->Init());
}

uint32_t
TCPServerSocketParent::GetAppId()
{
  const PContentParent *content = Manager()->Manager();
  if (PBrowserParent* browser = SingleManagedOrNull(content->ManagedPBrowserParent())) {
    TabParent *tab = TabParent::GetFrom(browser);
    return tab->OwnAppId();
  } else {
    return nsIScriptSecurityManager::UNKNOWN_APP_ID;
  }
}

bool
TCPServerSocketParent::GetInBrowser()
{
  const PContentParent *content = Manager()->Manager();
  if (PBrowserParent* browser = SingleManagedOrNull(content->ManagedPBrowserParent())) {
    TabParent *tab = TabParent::GetFrom(browser);
    return tab->IsBrowserElement();
  } else {
    return false;
  }
}

nsresult
TCPServerSocketParent::SendCallbackAccept(TCPSocketParent *socket)
{
  socket->AddIPDLReference();

  nsresult rv;

  nsString host;
  rv = socket->GetHost(host);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to get host from nsITCPSocketParent");
    return NS_ERROR_FAILURE;
  }

  uint16_t port;
  rv = socket->GetPort(&port);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to get port from nsITCPSocketParent");
    return NS_ERROR_FAILURE;
  }

  if (mNeckoParent) {
    if (mNeckoParent->SendPTCPSocketConstructor(socket, host, port)) {
      mozilla::Unused << PTCPServerSocketParent::SendCallbackAccept(socket);
    }
    else {
      NS_ERROR("Sending data from PTCPSocketParent was failed.");
    }
  }
  else {
    NS_ERROR("The member value for NeckoParent is wrong.");
  }
  return NS_OK;
}

bool
TCPServerSocketParent::RecvClose()
{
  NS_ENSURE_TRUE(mServerSocket, true);
  mServerSocket->Close();
  return true;
}

void
TCPServerSocketParent::ActorDestroy(ActorDestroyReason why)
{
  if (mServerSocket) {
    mServerSocket->Close();
    mServerSocket = nullptr;
  }
  mNeckoParent = nullptr;
}

bool
TCPServerSocketParent::RecvRequestDelete()
{
  mozilla::Unused << Send__delete__(this);
  return true;
}

void
TCPServerSocketParent::OnConnect(TCPServerSocketEvent* event)
{
  RefPtr<TCPSocket> socket = event->Socket();
  socket->SetAppIdAndBrowser(GetAppId(), GetInBrowser());

  RefPtr<TCPSocketParent> socketParent = new TCPSocketParent();
  socketParent->SetSocket(socket);

  socket->SetSocketBridgeParent(socketParent);

  SendCallbackAccept(socketParent);
}

} // namespace dom
} // namespace mozilla
