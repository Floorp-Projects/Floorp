/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPServerSocketParent.h"
#include "nsJSUtils.h"
#include "TCPSocketParent.h"
#include "mozilla/unused.h"
#include "mozilla/AppProcessChecker.h"

namespace mozilla {
namespace dom {

static void
FireInteralError(mozilla::net::PTCPServerSocketParent* aActor,
                 uint32_t aLineNo)
{
  mozilla::unused <<
      aActor->SendCallbackError(NS_LITERAL_STRING("Internal error"),
                          NS_LITERAL_STRING(__FILE__), aLineNo, 0);
}

NS_IMPL_CYCLE_COLLECTION(TCPServerSocketParent, mServerSocket, mIntermediary)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPServerSocketParent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPServerSocketParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPServerSocketParent)
  NS_INTERFACE_MAP_ENTRY(nsITCPServerSocketParent)
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

bool
TCPServerSocketParent::Init(PNeckoParent* neckoParent, const uint16_t& aLocalPort,
                            const uint16_t& aBacklog, const nsString& aBinaryType)
{
  mNeckoParent = neckoParent;

  nsresult rv;
  mIntermediary = do_CreateInstance("@mozilla.org/tcp-socket-intermediary;1", &rv);
  if (NS_FAILED(rv)) {
    FireInteralError(this, __LINE__);
    return true;
  }

  rv = mIntermediary->Listen(this, aLocalPort, aBacklog, aBinaryType, getter_AddRefs(mServerSocket));
  if (NS_FAILED(rv) || !mServerSocket) {
    FireInteralError(this, __LINE__);
    return true;
  }
  return true;
}

NS_IMETHODIMP
TCPServerSocketParent::SendCallbackAccept(nsITCPSocketParent *socket)
{
  TCPSocketParent* _socket = static_cast<TCPSocketParent*>(socket);
  PTCPSocketParent* _psocket = static_cast<PTCPSocketParent*>(_socket);

  _socket->AddIPDLReference();

  if (mNeckoParent) {
    if (mNeckoParent->SendPTCPSocketConstructor(_psocket)) {
      mozilla::unused << PTCPServerSocketParent::SendCallbackAccept(_psocket);
    }
    else {
      NS_ERROR("Sending data from PTCPSocketParent was failed.");
    };
  }
  else {
    NS_ERROR("The member value for NeckoParent is wrong.");
  }
  return NS_OK;
}

NS_IMETHODIMP
TCPServerSocketParent::SendCallbackError(const nsAString& message,
                                         const nsAString& filename,
                                         uint32_t lineNumber,
                                         uint32_t columnNumber)
{
  mozilla::unused <<
    PTCPServerSocketParent::SendCallbackError(nsString(message), nsString(filename),
                                              lineNumber, columnNumber);
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
  mIntermediary = nullptr;
}

bool
TCPServerSocketParent::RecvRequestDelete()
{
  mozilla::unused << Send__delete__(this);
  return true;
}

} // namespace dom
} // namespace mozilla
