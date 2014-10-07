/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "TCPSocketChild.h"
#include "mozilla/unused.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/TabChild.h"
#include "nsIDOMTCPSocket.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "jswrapper.h"

using mozilla::net::gNeckoChild;

namespace IPC {

bool
DeserializeArrayBuffer(JS::Handle<JSObject*> aObj,
                       const InfallibleTArray<uint8_t>& aBuffer,
                       JS::MutableHandle<JS::Value> aVal)
{
  mozilla::AutoSafeJSContext cx;
  JSAutoCompartment ac(cx, aObj);

  mozilla::UniquePtr<uint8_t[], JS::FreePolicy> data(js_pod_malloc<uint8_t>(aBuffer.Length()));
  if (!data)
      return false;
  memcpy(data.get(), aBuffer.Elements(), aBuffer.Length());

  JSObject* obj = JS_NewArrayBufferWithContents(cx, aBuffer.Length(), data.get());
  if (!obj)
      return false;
  data.release();

  aVal.setObject(*obj);
  return true;
}

} // namespace IPC

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(TCPSocketChildBase, mSocket)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPSocketChildBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPSocketChildBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPSocketChildBase)
  NS_INTERFACE_MAP_ENTRY(nsITCPSocketChild)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TCPSocketChildBase::TCPSocketChildBase()
: mIPCOpen(false)
{
}

TCPSocketChildBase::~TCPSocketChildBase()
{
}

NS_IMETHODIMP_(MozExternalRefCountType) TCPSocketChild::Release(void)
{
  nsrefcnt refcnt = TCPSocketChildBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    PTCPSocketChild::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

TCPSocketChild::TCPSocketChild()
: mWindowObj(nullptr)
, mHost()
, mPort(0)
{
}

void TCPSocketChild::Init(const nsString& aHost, const uint16_t& aPort) {
  mHost = aHost;
  mPort = aPort;
}

NS_IMETHODIMP
TCPSocketChild::SendOpen(nsITCPSocketInternal* aSocket,
                         const nsAString& aHost, uint16_t aPort,
                         bool aUseSSL, const nsAString& aBinaryType,
                         nsIDOMWindow* aWindow, JS::Handle<JS::Value> aWindowObj,
                         JSContext* aCx)
{
  mSocket = aSocket;

  MOZ_ASSERT(aWindowObj.isObject());
  mWindowObj = js::CheckedUnwrap(&aWindowObj.toObject());
  if (!mWindowObj) {
    return NS_ERROR_FAILURE;
  }
  AddIPDLReference();
  gNeckoChild->SendPTCPSocketConstructor(this, nsString(aHost), aPort);
  PTCPSocketChild::SendOpen(nsString(aHost), aPort,
                            aUseSSL, nsString(aBinaryType));
  return NS_OK;
}

void
TCPSocketChildBase::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  this->Release();
}

void
TCPSocketChildBase::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

TCPSocketChild::~TCPSocketChild()
{
}

bool
TCPSocketChild::RecvUpdateBufferedAmount(const uint32_t& aBuffered,
                                         const uint32_t& aTrackingNumber)
{
  if (NS_FAILED(mSocket->UpdateBufferedAmount(aBuffered, aTrackingNumber))) {
    NS_ERROR("Shouldn't fail!");
  }
  return true;
}

bool
TCPSocketChild::RecvCallback(const nsString& aType,
                             const CallbackData& aData,
                             const nsString& aReadyState)
{
  if (NS_FAILED(mSocket->UpdateReadyState(aReadyState)))
    NS_ERROR("Shouldn't fail!");

  nsresult rv = NS_ERROR_FAILURE;
  if (aData.type() == CallbackData::Tvoid_t) {
    rv = mSocket->CallListenerVoid(aType);

  } else if (aData.type() == CallbackData::TTCPError) {
    const TCPError& err(aData.get_TCPError());
    rv = mSocket->CallListenerError(aType, err.name());

  } else if (aData.type() == CallbackData::TSendableData) {
    const SendableData& data = aData.get_SendableData();

    if (data.type() == SendableData::TArrayOfuint8_t) {
      JSContext* cx = nsContentUtils::GetSafeJSContext();
      JSAutoRequest ar(cx);
      JS::Rooted<JS::Value> val(cx);
      JS::Rooted<JSObject*> window(cx, mWindowObj);
      bool ok = IPC::DeserializeArrayBuffer(window, data.get_ArrayOfuint8_t(), &val);
      NS_ENSURE_TRUE(ok, true);
      rv = mSocket->CallListenerArrayBuffer(aType, val);

    } else if (data.type() == SendableData::TnsString) {
      rv = mSocket->CallListenerData(aType, data.get_nsString());

    } else {
      MOZ_CRASH("Invalid callback data type!");
    }

  } else {
    MOZ_CRASH("Invalid callback type!");
  }
  NS_ENSURE_SUCCESS(rv, true);
  return true;
}

NS_IMETHODIMP
TCPSocketChild::SendStartTLS()
{
  PTCPSocketChild::SendStartTLS();
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::SendSuspend()
{
  PTCPSocketChild::SendSuspend();
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::SendResume()
{
  PTCPSocketChild::SendResume();
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::SendClose()
{
  PTCPSocketChild::SendClose();
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::SendSend(JS::Handle<JS::Value> aData,
                         uint32_t aByteOffset,
                         uint32_t aByteLength,
                         uint32_t aTrackingNumber,
                         JSContext* aCx)
{
  if (aData.isString()) {
    JSString* jsstr = aData.toString();
    nsAutoJSString str;
    bool ok = str.init(aCx, jsstr);
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
    SendData(str, aTrackingNumber);
  } else {
    NS_ENSURE_TRUE(aData.isObject(), NS_ERROR_FAILURE);
    JS::Rooted<JSObject*> obj(aCx, &aData.toObject());
    NS_ENSURE_TRUE(JS_IsArrayBufferObject(obj), NS_ERROR_FAILURE);
    uint32_t buflen = JS_GetArrayBufferByteLength(obj);
    aByteOffset = std::min(buflen, aByteOffset);
    uint32_t nbytes = std::min(buflen - aByteOffset, aByteLength);
    FallibleTArray<uint8_t> fallibleArr;
    {
        JS::AutoCheckCannotGC nogc;
        uint8_t* data = JS_GetArrayBufferData(obj, nogc);
        if (!data) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        if (!fallibleArr.InsertElementsAt(0, data + aByteOffset, nbytes)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    InfallibleTArray<uint8_t> arr;
    arr.SwapElements(fallibleArr);
    SendData(arr, aTrackingNumber);
  }
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::SetSocketAndWindow(nsITCPSocketInternal *aSocket,
                                   JS::Handle<JS::Value> aWindowObj,
                                   JSContext* aCx)
{
  mSocket = aSocket;
  MOZ_ASSERT(aWindowObj.isObject());
  mWindowObj = js::CheckedUnwrap(&aWindowObj.toObject());
  if (!mWindowObj) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::GetHost(nsAString& aHost)
{
  aHost = mHost;
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::GetPort(uint16_t* aPort)
{
  *aPort = mPort;
  return NS_OK;
}

bool
TCPSocketChild::RecvRequestDelete()
{
  mozilla::unused << Send__delete__(this);
  return true;
}

} // namespace dom
} // namespace mozilla
