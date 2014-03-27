/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPServerSocketChild.h"
#include "TCPSocketChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/TabChild.h"
#include "nsIDOMTCPSocket.h"
#include "nsJSUtils.h"
#include "jsfriendapi.h"

using mozilla::net::gNeckoChild;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_1(TCPServerSocketChildBase, mServerSocket)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPServerSocketChildBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPServerSocketChildBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPServerSocketChildBase)
  NS_INTERFACE_MAP_ENTRY(nsITCPServerSocketChild)
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

TCPServerSocketChild::TCPServerSocketChild()
{
}

NS_IMETHODIMP
TCPServerSocketChild::Listen(nsITCPServerSocketInternal* aServerSocket, uint16_t aLocalPort,
                             uint16_t aBacklog, const nsAString & aBinaryType, JSContext* aCx)
{
  mServerSocket = aServerSocket;
  AddIPDLReference();
  gNeckoChild->SendPTCPServerSocketConstructor(this, aLocalPort, aBacklog, nsString(aBinaryType));
  return NS_OK;
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

bool
TCPServerSocketChild::RecvCallbackAccept(PTCPSocketChild *psocket)
{
  TCPSocketChild* socket = static_cast<TCPSocketChild*>(psocket);

  nsresult rv = mServerSocket->CallListenerAccept(static_cast<nsITCPSocketChild*>(socket));
  if (NS_FAILED(rv)) {
    NS_WARNING("CallListenerAccept threw exception.");
  }
  return true;
}

bool
TCPServerSocketChild::RecvCallbackError(const nsString& aMessage,
                                        const nsString& aFilename,
                                        const uint32_t& aLineNumber,
                                        const uint32_t& aColumnNumber)
{
  nsresult rv = mServerSocket->CallListenerError(aMessage, aFilename,
                                                 aLineNumber, aColumnNumber);
  if (NS_FAILED(rv)) {
    NS_WARNING("CallListenerError threw exception.");
  }
  return true;
}

NS_IMETHODIMP
TCPServerSocketChild::Close()
{
  SendClose();
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
