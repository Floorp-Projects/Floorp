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
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIIPCBackgroundChildCreateCallback.h"

using mozilla::net::gNeckoChild;

//
// set NSPR_LOG_MODULES=UDPSocket:5
//
extern mozilla::LazyLogModule gUDPSocketLog;
#define UDPSOCKET_LOG(args)     MOZ_LOG(gUDPSocketLog, mozilla::LogLevel::Debug, args)
#define UDPSOCKET_LOG_ENABLED() MOZ_LOG_TEST(gUDPSocketLog, mozilla::LogLevel::Debug)

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
:mBackgroundManager(nullptr)
,mLocalPort(0)
{
}

UDPSocketChild::~UDPSocketChild()
{
}

class UDPSocketBackgroundChildCallback final :
  public nsIIPCBackgroundChildCreateCallback
{
  bool* mDone;

public:
  explicit UDPSocketBackgroundChildCallback(bool* aDone)
  : mDone(aDone)
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mDone);
    MOZ_ASSERT(!*mDone);
  }

  NS_DECL_ISUPPORTS

private:
  ~UDPSocketBackgroundChildCallback()
  { }

  virtual void
  ActorCreated(PBackgroundChild* aActor) override
  {
    *mDone = true;
  }

  virtual void
  ActorFailed() override
  {
    *mDone = true;
  }
};

NS_IMPL_ISUPPORTS(UDPSocketBackgroundChildCallback, nsIIPCBackgroundChildCreateCallback)

nsresult
UDPSocketChild::CreatePBackgroundSpinUntilDone()
{
  using mozilla::ipc::BackgroundChild;

  // Spinning the event loop in MainThread would be dangerous
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!BackgroundChild::GetForCurrentThread());

  bool done = false;
  nsCOMPtr<nsIIPCBackgroundChildCreateCallback> callback =
    new UDPSocketBackgroundChildCallback(&done);

  if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread(callback))) {
    return NS_ERROR_FAILURE;
  }

  nsIThread* thread = NS_GetCurrentThread();
  while (!done) {
    if (NS_WARN_IF(!NS_ProcessNextEvent(thread, true /* aMayWait */))) {
      return NS_ERROR_FAILURE;
    }
  }

  if (NS_WARN_IF(!BackgroundChild::GetForCurrentThread())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// nsIUDPSocketChild Methods

NS_IMETHODIMP
UDPSocketChild::SetBackgroundSpinsEvents()
{
  using mozilla::ipc::BackgroundChild;

  PBackgroundChild* existingBackgroundChild =
    BackgroundChild::GetForCurrentThread();
  // If it's not spun up yet, block until it is, and retry
  if (!existingBackgroundChild) {
    nsresult rv = CreatePBackgroundSpinUntilDone();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    existingBackgroundChild =
      BackgroundChild::GetForCurrentThread();
    MOZ_ASSERT(existingBackgroundChild);
  }
  // By now PBackground is guaranteed to be/have-been up
  mBackgroundManager = existingBackgroundChild;
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::Bind(nsIUDPSocketInternal* aSocket,
                     nsIPrincipal* aPrincipal,
                     const nsACString& aHost,
                     uint16_t aPort,
                     bool aAddressReuse,
                     bool aLoopback)
{
  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, PromiseFlatCString(aHost).get(), aPort));

  NS_ENSURE_ARG(aSocket);

  mSocket = aSocket;
  AddIPDLReference();

  if (mBackgroundManager) {
    // If we want to support a passed-in principal here we'd need to
    // convert it to a PrincipalInfo
    MOZ_ASSERT(!aPrincipal);
    mBackgroundManager->SendPUDPSocketConstructor(this, void_t(), mFilterName);
  } else {
    gNeckoChild->SendPUDPSocketConstructor(this, IPC::Principal(aPrincipal),
                                           mFilterName);
  }

  SendBind(UDPAddressInfo(nsCString(aHost), aPort), aAddressReuse, aLoopback);
  return NS_OK;
}

NS_IMETHODIMP
UDPSocketChild::Connect(nsIUDPSocketInternal* aSocket, const nsACString & aHost, uint16_t aPort)
{
  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, PromiseFlatCString(aHost).get(), aPort));

  mSocket = aSocket;

  SendConnect(UDPAddressInfo(nsCString(aHost), aPort));

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

  UDPSOCKET_LOG(("%s: %s:%u - %u bytes", __FUNCTION__, PromiseFlatCString(aHost).get(), aPort, aByteLength));
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

  UDPSOCKET_LOG(("%s: %u bytes", __FUNCTION__, aByteLength));
  return SendDataInternal(UDPSocketAddr(addr), aData, aByteLength);
}

NS_IMETHODIMP
UDPSocketChild::SendWithAddress(const NetAddr* aAddr,
                                const uint8_t* aData,
                                uint32_t aByteLength)
{
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aData);

  UDPSOCKET_LOG(("%s: %u bytes", __FUNCTION__, aByteLength));
  return SendDataInternal(UDPSocketAddr(*aAddr), aData, aByteLength);
}

nsresult
UDPSocketChild::SendDataInternal(const UDPSocketAddr& aAddr,
                                 const uint8_t* aData,
                                 const uint32_t aByteLength)
{
  NS_ENSURE_ARG(aData);

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, aData, aByteLength, fallible)) {
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

  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, PromiseFlatCString(aHost).get(), aPort));
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

  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, mLocalAddress.get(), mLocalPort));
  nsresult rv = mSocket->CallListenerOpened();
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

// PUDPSocketChild Methods
bool
UDPSocketChild::RecvCallbackConnected(const UDPAddressInfo& aAddressInfo)
{
  mLocalAddress = aAddressInfo.addr();
  mLocalPort = aAddressInfo.port();

  UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, mLocalAddress.get(), mLocalPort));
  nsresult rv = mSocket->CallListenerConnected();
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

bool
UDPSocketChild::RecvCallbackClosed()
{
  nsresult rv = mSocket->CallListenerClosed();
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

bool
UDPSocketChild::RecvCallbackReceivedData(const UDPAddressInfo& aAddressInfo,
                                         InfallibleTArray<uint8_t>&& aData)
{
  UDPSOCKET_LOG(("%s: %s:%u length %u", __FUNCTION__,
                 aAddressInfo.addr().get(), aAddressInfo.port(), aData.Length()));
  nsresult rv = mSocket->CallListenerReceivedData(aAddressInfo.addr(), aAddressInfo.port(),
                                                  aData.Elements(), aData.Length());
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

bool
UDPSocketChild::RecvCallbackError(const nsCString& aMessage,
                                  const nsCString& aFilename,
                                  const uint32_t& aLineNumber)
{
  UDPSOCKET_LOG(("%s: %s:%s:%u", __FUNCTION__, aMessage.get(), aFilename.get(), aLineNumber));
  nsresult rv = mSocket->CallListenerError(aMessage, aFilename, aLineNumber);
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return true;
}

} // namespace dom
} // namespace mozilla
