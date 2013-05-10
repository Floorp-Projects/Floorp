/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UDPSocketChild.h"
#include "mozilla/net/NeckoChild.h"

using mozilla::net::gNeckoChild;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS1(UDPSocketChildBase, nsIUDPSocketChild)

UDPSocketChildBase::UDPSocketChildBase()
: mIPCOpen(false)
{
}

UDPSocketChildBase::~UDPSocketChildBase()
{
}
void
UDPSocketChildBase::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  this->Release();
}

void
UDPSocketChildBase::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

NS_IMETHODIMP_(nsrefcnt) UDPSocketChild::Release(void)
{
  nsrefcnt refcnt = UDPSocketChildBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    PUDPSocketChild::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

UDPSocketChild::UDPSocketChild()
:mLocalPort(0)
{
}

UDPSocketChild::~UDPSocketChild()
{
}

// nsIUDPSocketChild Methods

NS_IMETHODIMP
UDPSocketChild::Bind(nsIUDPSocketInternal *aSocket,
                     const nsACString& aHost,
                     uint16_t aPort)
{
  NS_ENSURE_ARG(aSocket);

  mSocket = aSocket;
  AddIPDLReference();

  gNeckoChild->SendPUDPSocketConstructor(this, nsCString(aHost), aPort);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::Close()
{
  SendClose();
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::Send(const nsACString& aHost,
                     uint16_t aPort,
                     const uint8_t *aData,
                     uint32_t aByteLength)
{
  NS_ENSURE_ARG(aData);

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, aData, aByteLength)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  InfallibleTArray<uint8_t> array;
  array.SwapElements(fallibleArray);
  SendData(array, nsCString(aHost), aPort);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::SendWithAddr(nsINetAddr *aAddr,
                             const uint8_t *aData,
                             uint32_t aByteLength)
{
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aData);

  NetAddr addr;
  aAddr->GetNetAddr(&addr);

  return SendWithAddress(&addr, aData, aByteLength);
}

NS_IMETHODIMP
UDPSocketChild::SendWithAddress(const NetAddr *aAddr,
                                const uint8_t *aData,
                                uint32_t aByteLength)
{
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aData);

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, aData, aByteLength)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  InfallibleTArray<uint8_t> array;
  array.SwapElements(fallibleArray);
  SendDataWithAddress(array, *aAddr);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::GetLocalPort(uint16_t *aLocalPort)
{
  NS_ENSURE_ARG_POINTER(aLocalPort);

  *aLocalPort = mLocalPort;
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::GetLocalAddress(nsACString &aLocalAddress)
{
  aLocalAddress = mLocalAddress;
  return NS_OK;
}

// PUDPSocketChild Methods
bool
UDPSocketChild::RecvCallback(const nsCString &aType,
                             const UDPCallbackData &aData,
                             const nsCString &aState)
{
  if (NS_FAILED(mSocket->UpdateReadyState(aState)))
    NS_ERROR("Shouldn't fail!");

  nsresult rv = NS_ERROR_FAILURE;
  if (aData.type() == UDPCallbackData::Tvoid_t) {
    rv = mSocket->CallListenerVoid(aType);
  } else if (aData.type() == UDPCallbackData::TUDPError) {
    const UDPError& err(aData.get_UDPError());
    rv = mSocket->CallListenerError(aType, err.message(), err.filename(),
                                    err.lineNumber(), err.columnNumber());
  } else if (aData.type() == UDPCallbackData::TUDPMessage) {
    const UDPMessage& message(aData.get_UDPMessage());
    InfallibleTArray<uint8_t> data(message.data());
    rv = mSocket->CallListenerReceivedData(aType, message.fromAddr(), message.port(),
                                           data.Elements(), data.Length());
  } else if (aData.type() == UDPCallbackData::TUDPAddressInfo) {
    //update local address and port.
    const UDPAddressInfo& addressInfo(aData.get_UDPAddressInfo());
    mLocalAddress = addressInfo.local();
    mLocalPort = addressInfo.port();
    rv = mSocket->CallListenerVoid(aType);
  } else if (aData.type() == UDPCallbackData::TUDPSendResult) {
    const UDPSendResult& returnValue(aData.get_UDPSendResult());
    rv = mSocket->CallListenerSent(aType, returnValue.value());
  } else {
    MOZ_ASSERT("Invalid callback type!");
  }

  NS_ENSURE_SUCCESS(rv, true);

  return true;
}

} // namespace dom
} // namespace mozilla
