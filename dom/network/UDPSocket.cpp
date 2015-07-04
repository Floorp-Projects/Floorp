/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UDPSocket.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/UDPMessageEvent.h"
#include "mozilla/dom/UDPSocketBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/net/DNS.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsINetAddr.h"
#include "nsStringStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(UDPSocket::ListenerProxy,
                  nsIUDPSocketListener,
                  nsIUDPSocketInternal)

NS_IMPL_CYCLE_COLLECTION_CLASS(UDPSocket)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(UDPSocket, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOpened)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mClosed)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(UDPSocket, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOpened)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mClosed)
  tmp->CloseWithReason(NS_OK);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(UDPSocket, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(UDPSocket, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(UDPSocket)
  NS_INTERFACE_MAP_ENTRY(nsIUDPSocketListener)
  NS_INTERFACE_MAP_ENTRY(nsIUDPSocketInternal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<UDPSocket>
UDPSocket::Constructor(const GlobalObject& aGlobal,
                       const UDPOptions& aOptions,
                       ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bool addressReuse = aOptions.mAddressReuse;
  bool loopback = aOptions.mLoopback;

  nsCString remoteAddress;
  if (aOptions.mRemoteAddress.WasPassed()) {
    remoteAddress = NS_ConvertUTF16toUTF8(aOptions.mRemoteAddress.Value());
  } else {
    remoteAddress.SetIsVoid(true);
  }

  Nullable<uint16_t> remotePort;
  if (aOptions.mRemotePort.WasPassed()) {
    remotePort.SetValue(aOptions.mRemotePort.Value());

    if (remotePort.Value() == 0) {
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
      return nullptr;
    }
  }

  nsString localAddress;
  if (aOptions.mLocalAddress.WasPassed()) {
    localAddress = aOptions.mLocalAddress.Value();

    // check if localAddress is a valid IPv4/6 address
    NS_ConvertUTF16toUTF8 address(localAddress);
    PRNetAddr prAddr;
    PRStatus status = PR_StringToNetAddr(address.BeginReading(), &prAddr);
    if (status != PR_SUCCESS) {
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
      return nullptr;
    }
  } else {
    SetDOMStringToNull(localAddress);
  }

  Nullable<uint16_t> localPort;
  if (aOptions.mLocalPort.WasPassed()) {
    localPort.SetValue(aOptions.mLocalPort.Value());

    if (localPort.Value() == 0) {
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
      return nullptr;
    }
  }

  nsRefPtr<UDPSocket> socket = new UDPSocket(ownerWindow, remoteAddress, remotePort);
  aRv = socket->Init(localAddress, localPort, addressReuse, loopback);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return socket.forget();
}

UDPSocket::UDPSocket(nsPIDOMWindow* aOwner,
                     const nsCString& aRemoteAddress,
                     const Nullable<uint16_t>& aRemotePort)
  : DOMEventTargetHelper(aOwner)
  , mRemoteAddress(aRemoteAddress)
  , mRemotePort(aRemotePort)
  , mReadyState(SocketReadyState::Opening)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(aOwner->IsInnerWindow());

  nsIDocument* aDoc = aOwner->GetExtantDoc();
  if (aDoc) {
    aDoc->DisallowBFCaching();
  }
}

UDPSocket::~UDPSocket()
{
  CloseWithReason(NS_OK);
}

JSObject*
UDPSocket::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return UDPSocketBinding::Wrap(aCx, this, aGivenProto);
}

void
UDPSocket::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();
  CloseWithReason(NS_OK);
}

already_AddRefed<Promise>
UDPSocket::Close()
{
  MOZ_ASSERT(mClosed);

  nsRefPtr<Promise> promise = mClosed;

  if (mReadyState == SocketReadyState::Closed) {
    return promise.forget();
  }

  CloseWithReason(NS_OK);
  return promise.forget();
}

void
UDPSocket::CloseWithReason(nsresult aReason)
{
  if (mReadyState == SocketReadyState::Closed) {
    return;
  }

  if (mOpened) {
    if (mReadyState == SocketReadyState::Opening) {
      // reject openedPromise with AbortError if socket is closed without error
      nsresult openFailedReason = NS_FAILED(aReason) ? aReason : NS_ERROR_DOM_ABORT_ERR;
      mOpened->MaybeReject(openFailedReason);
    }
  }

  mReadyState = SocketReadyState::Closed;

  if (mListenerProxy) {
    mListenerProxy->Disconnect();
    mListenerProxy = nullptr;
  }

  if (mSocket) {
    mSocket->Close();
    mSocket = nullptr;
  }

  if (mSocketChild) {
    mSocketChild->Close();
    mSocketChild = nullptr;
  }

  if (mClosed) {
    if (NS_SUCCEEDED(aReason)) {
      mClosed->MaybeResolve(JS::UndefinedHandleValue);
    } else {
      mClosed->MaybeReject(aReason);
    }
  }

  mPendingMcastCommands.Clear();
}

void
UDPSocket::JoinMulticastGroup(const nsAString& aMulticastGroupAddress,
                              ErrorResult& aRv)
{
  if (mReadyState == SocketReadyState::Closed) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mReadyState == SocketReadyState::Opening) {
    MulticastCommand joinCommand(MulticastCommand::Join, aMulticastGroupAddress);
    mPendingMcastCommands.AppendElement(joinCommand);
    return;
  }

  MOZ_ASSERT(mSocket || mSocketChild);

  NS_ConvertUTF16toUTF8 address(aMulticastGroupAddress);

  if (mSocket) {
    MOZ_ASSERT(!mSocketChild);

    aRv = mSocket->JoinMulticast(address, EmptyCString());
    NS_WARN_IF(aRv.Failed());

    return;
  }

  MOZ_ASSERT(mSocketChild);

  aRv = mSocketChild->JoinMulticast(address, EmptyCString());
  NS_WARN_IF(aRv.Failed());
}

void
UDPSocket::LeaveMulticastGroup(const nsAString& aMulticastGroupAddress,
                               ErrorResult& aRv)
{
  if (mReadyState == SocketReadyState::Closed) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mReadyState == SocketReadyState::Opening) {
    MulticastCommand leaveCommand(MulticastCommand::Leave, aMulticastGroupAddress);
    mPendingMcastCommands.AppendElement(leaveCommand);
    return;
  }

  MOZ_ASSERT(mSocket || mSocketChild);

  nsCString address = NS_ConvertUTF16toUTF8(aMulticastGroupAddress);
  if (mSocket) {
    MOZ_ASSERT(!mSocketChild);

    aRv = mSocket->LeaveMulticast(address, EmptyCString());
    NS_WARN_IF(aRv.Failed());
    return;
  }

  MOZ_ASSERT(mSocketChild);

  aRv = mSocketChild->LeaveMulticast(address, EmptyCString());
  NS_WARN_IF(aRv.Failed());
}

nsresult
UDPSocket::DoPendingMcastCommand()
{
  MOZ_ASSERT(mReadyState == SocketReadyState::Open, "Multicast command can only be executed after socket opened");

  for (uint32_t i = 0; i < mPendingMcastCommands.Length(); ++i) {
    MulticastCommand& command = mPendingMcastCommands[i];
    ErrorResult rv;

    switch (command.mCommand) {
      case MulticastCommand::Join: {
        JoinMulticastGroup(command.mAddress, rv);
        break;
      }
      case MulticastCommand::Leave: {
        LeaveMulticastGroup(command.mAddress, rv);
        break;
      }
    }

    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
  }

  mPendingMcastCommands.Clear();
  return NS_OK;
}

bool
UDPSocket::Send(const StringOrBlobOrArrayBufferOrArrayBufferView& aData,
                const Optional<nsAString>& aRemoteAddress,
                const Optional<Nullable<uint16_t>>& aRemotePort,
                ErrorResult& aRv)
{
  if (mReadyState != SocketReadyState::Open) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  MOZ_ASSERT(mSocket || mSocketChild);

  // If the remote address and port were not specified in the constructor or as arguments,
  // throw InvalidAccessError.
  nsCString remoteAddress;
  if (aRemoteAddress.WasPassed()) {
    remoteAddress = NS_ConvertUTF16toUTF8(aRemoteAddress.Value());
    UDPSOCKET_LOG(("%s: Send to %s", __FUNCTION__, remoteAddress.get()));
  } else if (!mRemoteAddress.IsVoid()) {
    remoteAddress = mRemoteAddress;
    UDPSOCKET_LOG(("%s: Send to %s", __FUNCTION__, remoteAddress.get()));
  } else {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return false;
  }

  uint16_t remotePort;
  if (aRemotePort.WasPassed() && !aRemotePort.Value().IsNull()) {
    remotePort = aRemotePort.Value().Value();
  } else if (!mRemotePort.IsNull()) {
    remotePort = mRemotePort.Value();
  } else {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return false;
  }

  nsCOMPtr<nsIInputStream> stream;
  if (aData.IsBlob()) {
    Blob& blob = aData.GetAsBlob();

    blob.GetInternalStream(getter_AddRefs(stream), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return false;
    }
  } else {
    nsresult rv;
    nsCOMPtr<nsIStringInputStream> strStream = do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return false;
    }

    if (aData.IsString()) {
      NS_ConvertUTF16toUTF8 data(aData.GetAsString());
      aRv = strStream->SetData(data.BeginReading(), data.Length());
    } else if (aData.IsArrayBuffer()) {
      const ArrayBuffer& data = aData.GetAsArrayBuffer();
      data.ComputeLengthAndData();
      aRv = strStream->SetData(reinterpret_cast<const char*>(data.Data()), data.Length());
    } else {
      const ArrayBufferView& data = aData.GetAsArrayBufferView();
      data.ComputeLengthAndData();
      aRv = strStream->SetData(reinterpret_cast<const char*>(data.Data()), data.Length());
    }

    if (NS_WARN_IF(aRv.Failed())) {
      return false;
    }

    stream = strStream;
  }

  if (mSocket) {
    aRv = mSocket->SendBinaryStream(remoteAddress, remotePort, stream);
  } else if (mSocketChild) {
    aRv = mSocketChild->SendBinaryStream(remoteAddress, remotePort, stream);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return false;
  }

  return true;
}

nsresult
UDPSocket::InitLocal(const nsAString& aLocalAddress,
                     const uint16_t& aLocalPort)
{
  nsresult rv;

  nsCOMPtr<nsIUDPSocket> sock =
      do_CreateInstance("@mozilla.org/network/udp-socket;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner(), &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();
  if (!principal) {
    return NS_ERROR_FAILURE;
  }

  if (aLocalAddress.IsEmpty()) {
    rv = sock->Init(aLocalPort, /* loopback = */ false, principal,
                    mAddressReuse, /* optionalArgc = */ 1);
  } else {
    PRNetAddr prAddr;
    PR_InitializeNetAddr(PR_IpAddrAny, aLocalPort, &prAddr);
    PR_StringToNetAddr(NS_ConvertUTF16toUTF8(aLocalAddress).BeginReading(), &prAddr);
    UDPSOCKET_LOG(("%s: %s:%u", __FUNCTION__, NS_ConvertUTF16toUTF8(aLocalAddress).get(), aLocalPort));

    mozilla::net::NetAddr addr;
    PRNetAddrToNetAddr(&prAddr, &addr);
    rv = sock->InitWithAddress(&addr, principal, mAddressReuse,
                               /* optionalArgc = */ 1);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = sock->SetMulticastLoopback(mLoopback);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mSocket = sock;

  // Get real local address and local port
  nsCOMPtr<nsINetAddr> localAddr;
  rv = mSocket->GetLocalAddr(getter_AddRefs(localAddr));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString localAddress;
  rv = localAddr->GetAddress(localAddress);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mLocalAddress = NS_ConvertUTF8toUTF16(localAddress);

  uint16_t localPort;
  rv = localAddr->GetPort(&localPort);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mLocalPort.SetValue(localPort);

  mListenerProxy = new ListenerProxy(this);

  rv = mSocket->AsyncListen(mListenerProxy);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mReadyState = SocketReadyState::Open;
  rv = DoPendingMcastCommand();
  if (NS_FAILED(rv)) {
    return rv;
  }

  mOpened->MaybeResolve(JS::UndefinedHandleValue);

  return NS_OK;
}

nsresult
UDPSocket::InitRemote(const nsAString& aLocalAddress,
                      const uint16_t& aLocalPort)
{
  nsresult rv;

  nsCOMPtr<nsIUDPSocketChild> sock =
    do_CreateInstance("@mozilla.org/udp-socket-child;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mListenerProxy = new ListenerProxy(this);

  nsCOMPtr<nsIGlobalObject> obj = do_QueryInterface(GetOwner(), &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPrincipal> principal = obj->PrincipalOrNull();
  if (!principal) {
    return NS_ERROR_FAILURE;
  }

  rv = sock->Bind(mListenerProxy,
                  principal,
                  NS_ConvertUTF16toUTF8(aLocalAddress),
                  aLocalPort,
                  mAddressReuse,
                  mLoopback);

  if (NS_FAILED(rv)) {
    return rv;
  }

  mSocketChild = sock;

  return NS_OK;
}

nsresult
UDPSocket::Init(const nsString& aLocalAddress,
                const Nullable<uint16_t>& aLocalPort,
                const bool& aAddressReuse,
                const bool& aLoopback)
{
  MOZ_ASSERT(!mSocket && !mSocketChild);

  mLocalAddress = aLocalAddress;
  mLocalPort = aLocalPort;
  mAddressReuse = aAddressReuse;
  mLoopback = aLoopback;

  ErrorResult rv;
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());

  mOpened = Promise::Create(global, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  mClosed = Promise::Create(global, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  class OpenSocketRunnable final : public nsRunnable
  {
  public:
    explicit OpenSocketRunnable(UDPSocket* aSocket) : mSocket(aSocket)
    { }

    NS_IMETHOD Run() override
    {
      MOZ_ASSERT(mSocket);

      if (mSocket->mReadyState != SocketReadyState::Opening) {
        return NS_OK;
      }

      uint16_t localPort = 0;
      if (!mSocket->mLocalPort.IsNull()) {
        localPort = mSocket->mLocalPort.Value();
      }

      nsresult rv;
      if (!XRE_IsParentProcess()) {
        rv = mSocket->InitRemote(mSocket->mLocalAddress, localPort);
      } else {
        rv = mSocket->InitLocal(mSocket->mLocalAddress, localPort);
      }

      if (NS_WARN_IF(NS_FAILED(rv))) {
        mSocket->CloseWithReason(NS_ERROR_DOM_NETWORK_ERR);
      }

      return NS_OK;
    }

  private:
    nsRefPtr<UDPSocket> mSocket;
  };

  nsCOMPtr<nsIRunnable> runnable = new OpenSocketRunnable(this);

  return NS_DispatchToMainThread(runnable);
}

void
UDPSocket::HandleReceivedData(const nsACString& aRemoteAddress,
                              const uint16_t& aRemotePort,
                              const uint8_t* aData,
                              const uint32_t& aDataLength)
{
  if (mReadyState != SocketReadyState::Open) {
    return;
  }

  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  if (NS_FAILED(DispatchReceivedData(aRemoteAddress, aRemotePort, aData, aDataLength))) {
    CloseWithReason(NS_ERROR_TYPE_ERR);
  }
}

nsresult
UDPSocket::DispatchReceivedData(const nsACString& aRemoteAddress,
                                const uint16_t& aRemotePort,
                                const uint8_t* aData,
                                const uint32_t& aDataLength)
{
  AutoJSAPI jsapi;

  if (NS_WARN_IF(!jsapi.Init(GetOwner()))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  // Copy packet data to ArrayBuffer
  JS::Rooted<JSObject*> arrayBuf(cx, ArrayBuffer::Create(cx, aDataLength, aData));

  if (NS_WARN_IF(!arrayBuf)) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JS::Value> jsData(cx, JS::ObjectValue(*arrayBuf));

  // Create DOM event
  RootedDictionary<UDPMessageEventInit> init(cx);
  init.mRemoteAddress = NS_ConvertUTF8toUTF16(aRemoteAddress);
  init.mRemotePort = aRemotePort;
  init.mData = jsData;

  nsRefPtr<UDPMessageEvent> udpEvent =
    UDPMessageEvent::Constructor(this, NS_LITERAL_STRING("message"), init);

  if (NS_WARN_IF(!udpEvent)) {
    return NS_ERROR_FAILURE;
  }

  udpEvent->SetTrusted(true);

  nsRefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(this, udpEvent);

  return asyncDispatcher->PostDOMEvent();
}

// nsIUDPSocketListener

NS_IMETHODIMP
UDPSocket::OnPacketReceived(nsIUDPSocket* aSocket, nsIUDPMessage* aMessage)
{
  // nsIUDPSocketListener callbacks should be invoked on main thread.
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  // Create appropriate JS object for message
  FallibleTArray<uint8_t>& buffer = aMessage->GetDataAsTArray();

  nsCOMPtr<nsINetAddr> addr;
  if (NS_WARN_IF(NS_FAILED(aMessage->GetFromAddr(getter_AddRefs(addr))))) {
    return NS_OK;
  }

  nsCString remoteAddress;
  if (NS_WARN_IF(NS_FAILED(addr->GetAddress(remoteAddress)))) {
    return NS_OK;
  }

  uint16_t remotePort;
  if (NS_WARN_IF(NS_FAILED(addr->GetPort(&remotePort)))) {
    return NS_OK;
  }

  HandleReceivedData(remoteAddress, remotePort, buffer.Elements(), buffer.Length());
  return NS_OK;
}

NS_IMETHODIMP
UDPSocket::OnStopListening(nsIUDPSocket* aSocket, nsresult aStatus)
{
  // nsIUDPSocketListener callbacks should be invoked on main thread.
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  CloseWithReason(aStatus);

  return NS_OK;
}

// nsIUDPSocketInternal

NS_IMETHODIMP
UDPSocket::CallListenerError(const nsACString& aMessage,
                             const nsACString& aFilename,
                             uint32_t aLineNumber)
{
  CloseWithReason(NS_ERROR_DOM_NETWORK_ERR);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocket::CallListenerReceivedData(const nsACString& aRemoteAddress,
                                    uint16_t aRemotePort,
                                    const uint8_t* aData,
                                    uint32_t aDataLength)
{
  HandleReceivedData(aRemoteAddress, aRemotePort, aData, aDataLength);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocket::CallListenerOpened()
{
  if (mReadyState != SocketReadyState::Opening) {
    return NS_OK;
  }

  MOZ_ASSERT(mSocketChild);

  // Get real local address and local port
  nsCString localAddress;
  mSocketChild->GetLocalAddress(localAddress);
  mLocalAddress = NS_ConvertUTF8toUTF16(localAddress);

  uint16_t localPort;
  mSocketChild->GetLocalPort(&localPort);
  mLocalPort.SetValue(localPort);

  mReadyState = SocketReadyState::Open;
  nsresult rv = DoPendingMcastCommand();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    CloseWithReason(rv);
    return NS_OK;
  }

  mOpened->MaybeResolve(JS::UndefinedHandleValue);

  return NS_OK;
}

NS_IMETHODIMP
UDPSocket::CallListenerClosed()
{
  CloseWithReason(NS_OK);

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
