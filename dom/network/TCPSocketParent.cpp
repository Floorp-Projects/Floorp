/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPSocketParent.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsJSUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/PNeckoParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsISocketTransport.h"
#include "nsNetUtil.h"
#include "TCPSocket.h"

namespace IPC {

// Defined in TCPSocketChild.cpp
extern bool DeserializeArrayBuffer(JSContext* aCx,
                                   const nsTArray<uint8_t>& aBuffer,
                                   JS::MutableHandle<JS::Value> aVal);

}  // namespace IPC

namespace mozilla {

namespace net {
//
// set MOZ_LOG=TCPSocket:5
//
extern LazyLogModule gTCPSocketLog;
#define TCPSOCKET_LOG(args) MOZ_LOG(gTCPSocketLog, LogLevel::Debug, args)
#define TCPSOCKET_LOG_ENABLED() MOZ_LOG_TEST(gTCPSocketLog, LogLevel::Debug)
}  // namespace net

using namespace net;

namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPSocketParentBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(TCPSocketParentBase, mSocket)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPSocketParentBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPSocketParentBase)

TCPSocketParentBase::TCPSocketParentBase() : mIPCOpen(false) {}

TCPSocketParentBase::~TCPSocketParentBase() = default;

void TCPSocketParentBase::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  this->Release();
}

void TCPSocketParentBase::AddIPDLReference() {
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType) TCPSocketParent::Release(void) {
  nsrefcnt refcnt = TCPSocketParentBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    mozilla::Unused << PTCPSocketParent::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

mozilla::ipc::IPCResult TCPSocketParent::RecvOpen(
    const nsString& aHost, const uint16_t& aPort, const bool& aUseSSL,
    const bool& aUseArrayBuffers) {
  mSocket = new TCPSocket(nullptr, aHost, aPort, aUseSSL, aUseArrayBuffers);
  mSocket->SetSocketBridgeParent(this);
  NS_ENSURE_SUCCESS(mSocket->Init(nullptr), IPC_OK());
  return IPC_OK();
}

mozilla::ipc::IPCResult TCPSocketParent::RecvStartTLS() {
  NS_ENSURE_TRUE(mSocket, IPC_OK());
  ErrorResult rv;
  mSocket->UpgradeToSecure(rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult TCPSocketParent::RecvSuspend() {
  NS_ENSURE_TRUE(mSocket, IPC_OK());
  mSocket->Suspend();
  return IPC_OK();
}

mozilla::ipc::IPCResult TCPSocketParent::RecvResume() {
  NS_ENSURE_TRUE(mSocket, IPC_OK());
  ErrorResult rv;
  mSocket->Resume(rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult TCPSocketParent::RecvData(const SendableData& aData) {
  ErrorResult rv;

  switch (aData.type()) {
    case SendableData::TArrayOfuint8_t: {
      AutoSafeJSContext autoCx;
      JS::Rooted<JS::Value> val(autoCx);
      const nsTArray<uint8_t>& buffer = aData.get_ArrayOfuint8_t();
      bool ok = IPC::DeserializeArrayBuffer(autoCx, buffer, &val);
      NS_ENSURE_TRUE(ok, IPC_OK());
      RootedSpiderMonkeyInterface<ArrayBuffer> data(autoCx);
      if (!data.Init(&val.toObject())) {
        TCPSOCKET_LOG(("%s: Failed to allocate memory", __FUNCTION__));
        return IPC_FAIL_NO_REASON(this);
      }
      Optional<uint32_t> byteLength(buffer.Length());
      mSocket->Send(data, 0, byteLength, rv);
      break;
    }

    case SendableData::TnsCString: {
      const nsCString& strData = aData.get_nsCString();
      mSocket->Send(strData, rv);
      break;
    }

    default:
      MOZ_CRASH("unexpected SendableData type");
  }
  NS_ENSURE_SUCCESS(rv.StealNSResult(), IPC_OK());
  return IPC_OK();
}

mozilla::ipc::IPCResult TCPSocketParent::RecvClose() {
  NS_ENSURE_TRUE(mSocket, IPC_OK());
  mSocket->Close();
  return IPC_OK();
}

void TCPSocketParent::FireErrorEvent(const nsAString& aName,
                                     const nsAString& aType, nsresult aError,
                                     TCPReadyState aReadyState) {
  SendEvent(u"error"_ns, TCPError(nsString(aName), nsString(aType), aError),
            aReadyState);
}

void TCPSocketParent::FireEvent(const nsAString& aType,
                                TCPReadyState aReadyState) {
  return SendEvent(aType, mozilla::void_t(), aReadyState);
}

void TCPSocketParent::FireArrayBufferDataEvent(nsTArray<uint8_t>& aBuffer,
                                               TCPReadyState aReadyState) {
  nsTArray<uint8_t> arr = std::move(aBuffer);

  SendableData data(arr);
  SendEvent(u"data"_ns, data, aReadyState);
}

void TCPSocketParent::FireStringDataEvent(const nsACString& aData,
                                          TCPReadyState aReadyState) {
  SendableData data((nsCString(aData)));

  SendEvent(u"data"_ns, data, aReadyState);
}

void TCPSocketParent::SendEvent(const nsAString& aType, CallbackData aData,
                                TCPReadyState aReadyState) {
  if (mIPCOpen) {
    mozilla::Unused << PTCPSocketParent::SendCallback(
        nsString(aType), aData, static_cast<uint32_t>(aReadyState));
  }
}

void TCPSocketParent::SetSocket(TCPSocket* socket) { mSocket = socket; }

nsresult TCPSocketParent::GetHost(nsAString& aHost) {
  if (!mSocket) {
    NS_ERROR("No internal socket instance mSocket!");
    return NS_ERROR_FAILURE;
  }
  mSocket->GetHost(aHost);
  return NS_OK;
}

nsresult TCPSocketParent::GetPort(uint16_t* aPort) {
  if (!mSocket) {
    NS_ERROR("No internal socket instance mSocket!");
    return NS_ERROR_FAILURE;
  }
  *aPort = mSocket->Port();
  return NS_OK;
}

void TCPSocketParent::ActorDestroy(ActorDestroyReason why) {
  if (mSocket) {
    mSocket->Close();
  }
  mSocket = nullptr;
}

mozilla::ipc::IPCResult TCPSocketParent::RecvRequestDelete() {
  mozilla::Unused << Send__delete__(this);
  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
