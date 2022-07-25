/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UDPSocketChild.h"
#include "UDPSocket.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"

using mozilla::net::gNeckoChild;

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(UDPSocketChildBase, nsISupports)

UDPSocketChildBase::UDPSocketChildBase() : mIPCOpen(false) {}

UDPSocketChildBase::~UDPSocketChildBase() = default;

void UDPSocketChildBase::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  mSocket = nullptr;
  this->Release();
}

void UDPSocketChildBase::AddIPDLReference() {
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType) UDPSocketChild::Release(void) {
  nsrefcnt refcnt = UDPSocketChildBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    PUDPSocketChild::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

UDPSocketChild::UDPSocketChild() : mBackgroundManager(nullptr), mLocalPort(0) {}

UDPSocketChild::~UDPSocketChild() = default;

nsresult UDPSocketChild::SetBackgroundSpinsEvents() {
  using mozilla::ipc::BackgroundChild;

  mBackgroundManager = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!mBackgroundManager)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult UDPSocketChild::Bind(nsIUDPSocketInternal* aSocket,
                              nsIPrincipal* aPrincipal, const nsACString& aHost,
                              uint16_t aPort, bool aAddressReuse,
                              bool aLoopback, uint32_t recvBufferSize,
                              uint32_t sendBufferSize) {
  UDPSOCKET_LOG(
      ("%s: %s:%u", __FUNCTION__, PromiseFlatCString(aHost).get(), aPort));

  NS_ENSURE_ARG(aSocket);

  if (NS_IsMainThread()) {
    if (!gNeckoChild->SendPUDPSocketConstructor(this, aPrincipal,
                                                mFilterName)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    if (!mBackgroundManager) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // If we want to support a passed-in principal here we'd need to
    // convert it to a PrincipalInfo
    MOZ_ASSERT(!aPrincipal);
    if (!mBackgroundManager->SendPUDPSocketConstructor(this, Nothing(),
                                                       mFilterName)) {
      return NS_ERROR_FAILURE;
    }
  }

  mSocket = aSocket;
  AddIPDLReference();

  SendBind(UDPAddressInfo(nsCString(aHost), aPort), aAddressReuse, aLoopback,
           recvBufferSize, sendBufferSize);
  return NS_OK;
}

void UDPSocketChild::Connect(nsIUDPSocketInternal* aSocket,
                             const nsACString& aHost, uint16_t aPort) {
  UDPSOCKET_LOG(
      ("%s: %s:%u", __FUNCTION__, PromiseFlatCString(aHost).get(), aPort));

  mSocket = aSocket;

  SendConnect(UDPAddressInfo(nsCString(aHost), aPort));
}

void UDPSocketChild::Close() { SendClose(); }

nsresult UDPSocketChild::SendWithAddress(const NetAddr* aAddr,
                                         const uint8_t* aData,
                                         uint32_t aByteLength) {
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aData);

  UDPSOCKET_LOG(("%s: %u bytes", __FUNCTION__, aByteLength));
  return SendDataInternal(UDPSocketAddr(*aAddr), aData, aByteLength);
}

nsresult UDPSocketChild::SendDataInternal(const UDPSocketAddr& aAddr,
                                          const uint8_t* aData,
                                          const uint32_t aByteLength) {
  NS_ENSURE_ARG(aData);

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, aData, aByteLength, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SendOutgoingData(UDPData{std::move(fallibleArray)}, aAddr);

  return NS_OK;
}

nsresult UDPSocketChild::SendBinaryStream(const nsACString& aHost,
                                          uint16_t aPort,
                                          nsIInputStream* aStream) {
  NS_ENSURE_ARG(aStream);

  mozilla::ipc::IPCStream stream;
  if (NS_WARN_IF(!mozilla::ipc::SerializeIPCStream(do_AddRef(aStream), stream,
                                                   /* aAllowLazy */ false))) {
    return NS_ERROR_UNEXPECTED;
  }

  UDPSOCKET_LOG(
      ("%s: %s:%u", __FUNCTION__, PromiseFlatCString(aHost).get(), aPort));
  SendOutgoingData(UDPData(stream),
                   UDPSocketAddr(UDPAddressInfo(nsCString(aHost), aPort)));

  return NS_OK;
}

void UDPSocketChild::JoinMulticast(const nsACString& aMulticastAddress,
                                   const nsACString& aInterface) {
  SendJoinMulticast(aMulticastAddress, aInterface);
}

void UDPSocketChild::LeaveMulticast(const nsACString& aMulticastAddress,
                                    const nsACString& aInterface) {
  SendLeaveMulticast(aMulticastAddress, aInterface);
}

nsresult UDPSocketChild::SetFilterName(const nsACString& aFilterName) {
  if (!mFilterName.IsEmpty()) {
    // filter name can only be set once.
    return NS_ERROR_FAILURE;
  }
  mFilterName = aFilterName;
  return NS_OK;
}

// PUDPSocketChild Methods
mozilla::ipc::IPCResult UDPSocketChild::RecvCallbackOpened(
    const UDPAddressInfo& aAddressInfo) {
  mLocalAddress = aAddressInfo.addr();
  mLocalPort = aAddressInfo.port();

  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, mLocalAddress.get(), mLocalPort));
  nsresult rv = mSocket->CallListenerOpened();
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return IPC_OK();
}

// PUDPSocketChild Methods
mozilla::ipc::IPCResult UDPSocketChild::RecvCallbackConnected(
    const UDPAddressInfo& aAddressInfo) {
  mLocalAddress = aAddressInfo.addr();
  mLocalPort = aAddressInfo.port();

  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, mLocalAddress.get(), mLocalPort));
  nsresult rv = mSocket->CallListenerConnected();
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return IPC_OK();
}

mozilla::ipc::IPCResult UDPSocketChild::RecvCallbackClosed() {
  nsresult rv = mSocket->CallListenerClosed();
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return IPC_OK();
}

mozilla::ipc::IPCResult UDPSocketChild::RecvCallbackReceivedData(
    const UDPAddressInfo& aAddressInfo, nsTArray<uint8_t>&& aData) {
  UDPSOCKET_LOG(("%s: %s:%u length %zu", __FUNCTION__,
                 aAddressInfo.addr().get(), aAddressInfo.port(),
                 aData.Length()));
  nsresult rv = mSocket->CallListenerReceivedData(aAddressInfo.addr(),
                                                  aAddressInfo.port(), aData);
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return IPC_OK();
}

mozilla::ipc::IPCResult UDPSocketChild::RecvCallbackError(
    const nsCString& aMessage, const nsCString& aFilename,
    const uint32_t& aLineNumber) {
  UDPSOCKET_LOG(("%s: %s:%s:%u", __FUNCTION__, aMessage.get(), aFilename.get(),
                 aLineNumber));
  nsresult rv = mSocket->CallListenerError(aMessage, aFilename, aLineNumber);
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return IPC_OK();
}

}  // namespace mozilla::dom
