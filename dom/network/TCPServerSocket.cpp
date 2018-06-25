/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TCPServerSocketBinding.h"
#include "mozilla/dom/TCPServerSocketEvent.h"
#include "mozilla/dom/TCPSocketBinding.h"
#include "TCPServerSocketParent.h"
#include "TCPServerSocketChild.h"
#include "mozilla/dom/Event.h"
#include "mozilla/ErrorResult.h"
#include "TCPServerSocket.h"
#include "TCPSocket.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(TCPServerSocket)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(TCPServerSocket,
                                               DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TCPServerSocket,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServerSocket)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServerBridgeChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServerBridgeParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TCPServerSocket,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServerSocket)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServerBridgeChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServerBridgeParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(TCPServerSocket, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TCPServerSocket, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPServerSocket)
  NS_INTERFACE_MAP_ENTRY(nsIServerSocketListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TCPServerSocket::TCPServerSocket(nsIGlobalObject* aGlobal, uint16_t aPort,
                                 bool aUseArrayBuffers, uint16_t aBacklog)
  : DOMEventTargetHelper(aGlobal)
  , mPort(aPort)
  , mBacklog(aBacklog)
  , mUseArrayBuffers(aUseArrayBuffers)
{
}

TCPServerSocket::~TCPServerSocket()
{
}

nsresult
TCPServerSocket::Init()
{
  if (mServerSocket || mServerBridgeChild) {
    NS_WARNING("Child TCPServerSocket is already listening.");
    return NS_ERROR_FAILURE;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    nsCOMPtr<nsIEventTarget> target;
    if (nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal()) {
      target = global->EventTargetFor(TaskCategory::Other);
    }
    mServerBridgeChild =
      new TCPServerSocketChild(this, mPort, mBacklog, mUseArrayBuffers, target);
    return NS_OK;
  }

  nsresult rv;
  mServerSocket = do_CreateInstance("@mozilla.org/network/server-socket;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mServerSocket->Init(mPort, false, mBacklog);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mServerSocket->GetPort(&mPort);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mServerSocket->AsyncListen(this);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

already_AddRefed<TCPServerSocket>
TCPServerSocket::Constructor(const GlobalObject& aGlobal,
                             uint16_t aPort,
                             const ServerSocketOptions& aOptions,
                             uint16_t aBacklog,
                             mozilla::ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }
  bool useArrayBuffers = aOptions.mBinaryType == TCPSocketBinaryType::Arraybuffer;
  RefPtr<TCPServerSocket> socket = new TCPServerSocket(global, aPort, useArrayBuffers, aBacklog);
  nsresult rv = socket->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }
  return socket.forget();
}

uint16_t
TCPServerSocket::LocalPort()
{
  return mPort;
}

void
TCPServerSocket::Close()
{
  if (mServerBridgeChild) {
    mServerBridgeChild->Close();
  }
  if (mServerSocket) {
    mServerSocket->Close();
  }
}

void
TCPServerSocket::FireEvent(const nsAString& aType, TCPSocket* aSocket)
{
  TCPServerSocketEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mSocket = aSocket;

  RefPtr<TCPServerSocketEvent> event =
      TCPServerSocketEvent::Constructor(this, aType, init);
  event->SetTrusted(true);
  DispatchEvent(*event);

  if (mServerBridgeParent) {
    mServerBridgeParent->OnConnect(event);
  }
}

NS_IMETHODIMP
TCPServerSocket::OnSocketAccepted(nsIServerSocket* aServer, nsISocketTransport* aTransport)
{
  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
  RefPtr<TCPSocket> socket = TCPSocket::CreateAcceptedSocket(global, aTransport, mUseArrayBuffers);
  FireEvent(NS_LITERAL_STRING("connect"), socket);
  return NS_OK;
}

NS_IMETHODIMP
TCPServerSocket::OnStopListening(nsIServerSocket* aServer, nsresult aStatus)
{
  if (aStatus != NS_BINDING_ABORTED) {
    RefPtr<Event> event = new Event(GetOwner());
    event->InitEvent(NS_LITERAL_STRING("error"), false, false);
    event->SetTrusted(true);
    DispatchEvent(*event);

    NS_WARNING("Server socket was closed by unexpected reason.");
    return NS_ERROR_FAILURE;
  }
  mServerSocket = nullptr;
  return NS_OK;
}

nsresult
TCPServerSocket::AcceptChildSocket(TCPSocketChild* aSocketChild)
{
  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
  NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);
  RefPtr<TCPSocket> socket = TCPSocket::CreateAcceptedSocket(global, aSocketChild, mUseArrayBuffers);
  NS_ENSURE_TRUE(socket, NS_ERROR_FAILURE);
  FireEvent(NS_LITERAL_STRING("connect"), socket);
  return NS_OK;
}

void
TCPServerSocket::SetServerBridgeParent(TCPServerSocketParent* aBridgeParent)
{
  mServerBridgeParent = aBridgeParent;
}

JSObject*
TCPServerSocket::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TCPServerSocket_Binding::Wrap(aCx, this, aGivenProto);
}
