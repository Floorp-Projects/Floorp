/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UDPSocketChild.h"
#include "mozilla/unused.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"

using mozilla::net::gNeckoChild;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(UDPSocketChildBase, nsIUDPSocketChild)

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
  mSocket = nullptr;
  this->Release();
}

void
UDPSocketChildBase::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType) UDPSocketChild::Release(void)
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
UDPSocketChild::Bind(nsIUDPSocketInternal* aSocket,
                     nsIPrincipal* aPrincipal,
                     const nsACString& aHost,
                     uint16_t aPort,
                     bool aAddressReuse,
                     bool aLoopback)
{
  NS_ENSURE_ARG(aSocket);

  mSocket = aSocket;
  AddIPDLReference();

  gNeckoChild->SendPUDPSocketConstructor(this, IPC::Principal(aPrincipal),
                                         mFilterName);

  SendBind(UDPAddressInfo(nsCString(aHost), aPort), aAddressReuse, aLoopback);
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
                     const uint8_t* aData,
                     uint32_t aByteLength)
{
  NS_ENSURE_ARG(aData);

  return SendDataInternal(UDPSocketAddr(UDPAddressInfo(nsCString(aHost), aPort)),
                          aData, aByteLength);
}

NS_IMETHODIMP
UDPSocketChild::SendWithAddr(nsINetAddr* aAddr,
                             const uint8_t* aData,
                             uint32_t aByteLength)
{
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aData);

  NetAddr addr;
  aAddr->GetNetAddr(&addr);

  return SendDataInternal(UDPSocketAddr(addr), aData, aByteLength);
}

NS_IMETHODIMP
UDPSocketChild::SendWithAddress(const NetAddr* aAddr,
                                const uint8_t* aData,
                                uint32_t aByteLength)
{
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aData);

  return SendDataInternal(UDPSocketAddr(*aAddr), aData, aByteLength);
}

nsresult
UDPSocketChild::SendDataInternal(const UDPSocketAddr& aAddr,
                                 const uint8_t* aData,
                                 const uint32_t aByteLength)
{
  NS_ENSURE_ARG(aData);

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, aData, aByteLength)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  InfallibleTArray<uint8_t> array;
  array.SwapElements(fallibleArray);

  SendOutgoingData(array, aAddr);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::SendBinaryStream(const nsACString& aHost,
                                 uint16_t aPort,
                                 nsIInputStream* aStream)
{
  NS_ENSURE_ARG(aStream);

  OptionalInputStreamParams stream;
  nsTArray<mozilla::ipc::FileDescriptor> fds;
  SerializeInputStream(aStream, stream, fds);

  MOZ_ASSERT(fds.IsEmpty());

  SendOutgoingData(UDPData(stream), UDPSocketAddr(UDPAddressInfo(nsCString(aHost), aPort)));

  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::JoinMulticast(const nsACString& aMulticastAddress,
                              const nsACString& aInterface)
{
  SendJoinMulticast(nsCString(aMulticastAddress), nsCString(aInterface));
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::LeaveMulticast(const nsACString& aMulticastAddress,
                               const nsACString& aInterface)
{
  SendLeaveMulticast(nsCString(aMulticastAddress), nsCString(aInterface));
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::GetLocalPort(uint16_t* aLocalPort)
{
  NS_ENSURE_ARG_POINTER(aLocalPort);

  *aLocalPort = mLocalPort;
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::GetLocalAddress(nsACString& aLocalAddress)
{
  aLocalAddress = mLocalAddress;
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::SetFilterName(const nsACString& aFilterName)
{
  if (!mFilterName.IsEmpty()) {
    // filter name can only be set once.
    return NS_ERROR_FAILURE;
  }
  mFilterName = aFilterName;
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::GetFilterName(nsACString& aFilterName)
{
  aFilterName = mFilterName;
  return NS_OK;
}

// PUDPSocketChild Methods
bool
UDPSocketChild::RecvCallbackOpened(const UDPAddressInfo& aAddressInfo)
{
  mLocalAddress = aAddressInfo.addr();
  mLocalPort = aAddressInfo.port();

  nsresult rv = mSocket->CallListenerOpened();
  mozilla::unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

bool
UDPSocketChild::RecvCallbackClosed()
{
  nsresult rv = mSocket->CallListenerClosed();
  mozilla::unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

bool
UDPSocketChild::RecvCallbackReceivedData(const UDPAddressInfo& aAddressInfo,
                                         InfallibleTArray<uint8_t>&& aData)
{
  nsresult rv = mSocket->CallListenerReceivedData(aAddressInfo.addr(), aAddressInfo.port(),
                                                  aData.Elements(), aData.Length());
  mozilla::unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

bool
UDPSocketChild::RecvCallbackError(const nsCString& aMessage,
                                  const nsCString& aFilename,
                                  const uint32_t& aLineNumber)
{
  nsresult rv = mSocket->CallListenerError(aMessage, aFilename, aLineNumber);
  mozilla::unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

} // namespace dom
} // namespace mozilla
