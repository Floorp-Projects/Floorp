/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "TCPSocketChild.h"
#include "mozilla/Unused.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsITCPSocketCallback.h"
#include "TCPSocket.h"
#include "nsContentUtils.h"
#include "js/ArrayBuffer.h"  // JS::NewArrayBufferWithContents
#include "js/RootingAPI.h"   // JS::MutableHandle
#include "js/Utility.h"  // js::ArrayBufferContentsArena, JS::FreePolicy, js_pod_arena_malloc
#include "js/Value.h"  // JS::Value

using mozilla::net::gNeckoChild;

namespace IPC {

bool DeserializeArrayBuffer(JSContext* cx,
                            const InfallibleTArray<uint8_t>& aBuffer,
                            JS::MutableHandle<JS::Value> aVal) {
  mozilla::UniquePtr<uint8_t[], JS::FreePolicy> data(
      js_pod_arena_malloc<uint8_t>(js::ArrayBufferContentsArena,
                                   aBuffer.Length()));
  if (!data) return false;
  memcpy(data.get(), aBuffer.Elements(), aBuffer.Length());

  JSObject* obj =
      JS::NewArrayBufferWithContents(cx, aBuffer.Length(), data.get());
  if (!obj) return false;
  // If JS::NewArrayBufferWithContents returns non-null, the ownership of
  // the data is transfered to obj, so we release the ownership here.
  mozilla::Unused << data.release();

  aVal.setObject(*obj);
  return true;
}

}  // namespace IPC

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(TCPSocketChildBase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TCPSocketChildBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSocket)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TCPSocketChildBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSocket)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TCPSocketChildBase)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPSocketChildBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPSocketChildBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPSocketChildBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TCPSocketChildBase::TCPSocketChildBase() : mIPCOpen(false) {
  mozilla::HoldJSObjects(this);
}

TCPSocketChildBase::~TCPSocketChildBase() { mozilla::DropJSObjects(this); }

NS_IMETHODIMP_(MozExternalRefCountType) TCPSocketChild::Release(void) {
  nsrefcnt refcnt = TCPSocketChildBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    PTCPSocketChild::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

TCPSocketChild::TCPSocketChild(const nsAString& aHost, const uint16_t& aPort,
                               nsIEventTarget* aTarget)
    : mHost(aHost), mPort(aPort), mIPCEventTarget(aTarget) {}

void TCPSocketChild::SendOpen(nsITCPSocketCallback* aSocket, bool aUseSSL,
                              bool aUseArrayBuffers) {
  mSocket = aSocket;

  if (mIPCEventTarget) {
    gNeckoChild->SetEventTargetForActor(this, mIPCEventTarget);
  }

  AddIPDLReference();
  gNeckoChild->SendPTCPSocketConstructor(this, mHost, mPort);
  MOZ_ASSERT(mFilterName.IsEmpty());  // Currently nobody should use this
  PTCPSocketChild::SendOpen(mHost, mPort, aUseSSL, aUseArrayBuffers);
}

void TCPSocketChild::SendWindowlessOpenBind(nsITCPSocketCallback* aSocket,
                                            const nsACString& aRemoteHost,
                                            uint16_t aRemotePort,
                                            const nsACString& aLocalHost,
                                            uint16_t aLocalPort, bool aUseSSL,
                                            bool aReuseAddrPort) {
  mSocket = aSocket;

  if (mIPCEventTarget) {
    gNeckoChild->SetEventTargetForActor(this, mIPCEventTarget);
  }

  AddIPDLReference();
  gNeckoChild->SendPTCPSocketConstructor(
      this, NS_ConvertUTF8toUTF16(aRemoteHost), aRemotePort);
  PTCPSocketChild::SendOpenBind(nsCString(aRemoteHost), aRemotePort,
                                nsCString(aLocalHost), aLocalPort, aUseSSL,
                                aReuseAddrPort, true, mFilterName);
}

void TCPSocketChildBase::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  mSocket = nullptr;
  this->Release();
}

void TCPSocketChildBase::AddIPDLReference() {
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

TCPSocketChild::~TCPSocketChild() {}

mozilla::ipc::IPCResult TCPSocketChild::RecvUpdateBufferedAmount(
    const uint32_t& aBuffered, const uint32_t& aTrackingNumber) {
  mSocket->UpdateBufferedAmount(aBuffered, aTrackingNumber);
  return IPC_OK();
}

mozilla::ipc::IPCResult TCPSocketChild::RecvCallback(
    const nsString& aType, const CallbackData& aData,
    const uint32_t& aReadyState) {
  mSocket->UpdateReadyState(aReadyState);

  if (aData.type() == CallbackData::Tvoid_t) {
    mSocket->FireEvent(aType);

  } else if (aData.type() == CallbackData::TTCPError) {
    const TCPError& err(aData.get_TCPError());
    mSocket->FireErrorEvent(err.name(), err.message());

  } else if (aData.type() == CallbackData::TSendableData) {
    const SendableData& data = aData.get_SendableData();

    if (data.type() == SendableData::TArrayOfuint8_t) {
      mSocket->FireDataArrayEvent(aType, data.get_ArrayOfuint8_t());
    } else if (data.type() == SendableData::TnsCString) {
      mSocket->FireDataStringEvent(aType, data.get_nsCString());
    } else {
      MOZ_CRASH("Invalid callback data type!");
    }
  } else {
    MOZ_CRASH("Invalid callback type!");
  }
  return IPC_OK();
}

void TCPSocketChild::SendSend(const nsACString& aData,
                              uint32_t aTrackingNumber) {
  SendData(nsCString(aData), aTrackingNumber);
}

nsresult TCPSocketChild::SendSend(const ArrayBuffer& aData,
                                  uint32_t aByteOffset, uint32_t aByteLength,
                                  uint32_t aTrackingNumber) {
  uint32_t buflen = aData.Length();
  uint32_t offset = std::min(buflen, aByteOffset);
  uint32_t nbytes = std::min(buflen - aByteOffset, aByteLength);
  FallibleTArray<uint8_t> fallibleArr;
  if (!fallibleArr.InsertElementsAt(0, aData.Data() + offset, nbytes,
                                    fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  InfallibleTArray<uint8_t> arr;
  arr.SwapElements(fallibleArr);
  SendData(arr, aTrackingNumber);
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::SendSendArray(nsTArray<uint8_t>& aArray,
                              uint32_t aTrackingNumber) {
  SendData(aArray, aTrackingNumber);
  return NS_OK;
}

void TCPSocketChild::SetSocket(TCPSocket* aSocket) { mSocket = aSocket; }

void TCPSocketChild::GetHost(nsAString& aHost) { aHost = mHost; }

void TCPSocketChild::GetPort(uint16_t* aPort) { *aPort = mPort; }

nsresult TCPSocketChild::SetFilterName(const nsACString& aFilterName) {
  if (!mFilterName.IsEmpty()) {
    // filter name can only be set once.
    return NS_ERROR_FAILURE;
  }
  mFilterName = aFilterName;
  return NS_OK;
}

mozilla::ipc::IPCResult TCPSocketChild::RecvRequestDelete() {
  mozilla::Unused << Send__delete__(this);
  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
