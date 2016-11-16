/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPServerSocketChild.h"
#include "TCPSocketChild.h"
#include "TCPServerSocket.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/TabChild.h"
#include "nsJSUtils.h"
#include "jsfriendapi.h"

using mozilla::net::gNeckoChild;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(TCPServerSocketChildBase, mServerSocket)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPServerSocketChildBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPServerSocketChildBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPServerSocketChildBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TCPServerSocketChildBase::TCPServerSocketChildBase()
: mIPCOpen(false)
{
}

TCPServerSocketChildBase::~TCPServerSocketChildBase()
{
}

NS_IMETHODIMP_(MozExternalRefCountType) TCPServerSocketChild::Release(void)
{
  nsrefcnt refcnt = TCPServerSocketChildBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    PTCPServerSocketChild::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

TCPServerSocketChild::TCPServerSocketChild(TCPServerSocket* aServerSocket, uint16_t aLocalPort,
                                           uint16_t aBacklog, bool aUseArrayBuffers)
{
  mServerSocket = aServerSocket;
  AddIPDLReference();
  gNeckoChild->SendPTCPServerSocketConstructor(this, aLocalPort, aBacklog, aUseArrayBuffers);
}

void
TCPServerSocketChildBase::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  this->Release();
}

void
TCPServerSocketChildBase::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

TCPServerSocketChild::~TCPServerSocketChild()
{
}

mozilla::ipc::IPCResult
TCPServerSocketChild::RecvCallbackAccept(PTCPSocketChild *psocket)
{
  RefPtr<TCPSocketChild> socket = static_cast<TCPSocketChild*>(psocket);
  nsresult rv = mServerSocket->AcceptChildSocket(socket);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  return IPC_OK();
}

void
TCPServerSocketChild::Close()
{
  SendClose();
}

} // namespace dom
} // namespace mozilla
