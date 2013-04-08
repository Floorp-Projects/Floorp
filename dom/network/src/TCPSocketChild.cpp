/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPSocketChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/TabChild.h"
#include "nsIDOMTCPSocket.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"

using mozilla::net::gNeckoChild;

namespace IPC {

bool
DeserializeUint8Array(JSRawObject aObj,
                      const InfallibleTArray<uint8_t>& aBuffer,
                      JS::Value* aVal)
{
  JSContext* cx = nsContentUtils::GetSafeJSContext();
  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, aObj);

  JSObject* obj = JS_NewArrayBuffer(cx, aBuffer.Length());
  if (!obj)
    return false;
  uint8_t* data = JS_GetArrayBufferData(obj);
  if (!data)
    return false;
  memcpy(data, aBuffer.Elements(), aBuffer.Length());
  JSObject* arr = JS_NewUint8ArrayWithBuffer(cx, obj, 0, aBuffer.Length());
  if (!arr)
    return false;
  *aVal = OBJECT_TO_JSVAL(arr);
  return true;
}

} // namespace IPC

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_1(TCPSocketChildBase, mSocket)
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

NS_IMETHODIMP_(nsrefcnt) TCPSocketChild::Release(void)
{
  nsrefcnt refcnt = TCPSocketChildBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    PTCPSocketChild::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

TCPSocketChild::TCPSocketChild()
: mSocketObj(nullptr)
{
}

NS_IMETHODIMP
TCPSocketChild::Open(nsITCPSocketInternal* aSocket, const nsAString& aHost,
                     uint16_t aPort, bool aUseSSL, const nsAString& aBinaryType,
                     nsIDOMWindow* aWindow, const JS::Value& aSocketObj,
                     JSContext* aCx)
{
  mSocket = aSocket;
  MOZ_ASSERT(aSocketObj.isObject());
  mSocketObj = &aSocketObj.toObject();
  AddIPDLReference();
  gNeckoChild->SendPTCPSocketConstructor(this, nsString(aHost), aPort,
                                         aUseSSL, nsString(aBinaryType),
                                         GetTabChildFrom(aWindow));
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
TCPSocketChild::RecvCallback(const nsString& aType,
                             const CallbackData& aData,
                             const nsString& aReadyState,
                             const uint32_t& aBuffered)
{
  if (NS_FAILED(mSocket->UpdateReadyStateAndBuffered(aReadyState, aBuffered)))
    NS_ERROR("Shouldn't fail!");

  nsresult rv = NS_ERROR_FAILURE;
  if (aData.type() == CallbackData::Tvoid_t) {
    rv = mSocket->CallListenerVoid(aType);

  } else if (aData.type() == CallbackData::TJSError) {
    const JSError& err(aData.get_JSError());
    rv = mSocket->CallListenerError(aType, err.message(), err.filename(),
                                   err.lineNumber(), err.columnNumber());

  } else if (aData.type() == CallbackData::TSendableData) {
    const SendableData& data = aData.get_SendableData();

    if (data.type() == SendableData::TArrayOfuint8_t) {
      JS::Value val;
      IPC::DeserializeUint8Array(mSocketObj, data.get_ArrayOfuint8_t(), &val);
      rv = mSocket->CallListenerArrayBuffer(aType, val);

    } else if (data.type() == SendableData::TnsString) {
      rv = mSocket->CallListenerData(aType, data.get_nsString());

    } else {
      MOZ_NOT_REACHED("Invalid callback data type!");
    }

  } else {
    MOZ_NOT_REACHED("Invalid callback type!");
  }
  NS_ENSURE_SUCCESS(rv, true);
  return true;
}

NS_IMETHODIMP
TCPSocketChild::Suspend()
{
  SendSuspend();
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::Resume()
{
  SendResume();
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::Close()
{
  SendClose();
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketChild::Send(const JS::Value& aData, JSContext* aCx)
{
  if (aData.isString()) {
    JSString* jsstr = aData.toString();
    nsDependentJSString str;
    bool ok = str.init(aCx, jsstr);
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
    SendData(str);

  } else {
    NS_ENSURE_TRUE(aData.isObject(), NS_ERROR_FAILURE);
    JSObject* obj = &aData.toObject();
    NS_ENSURE_TRUE(JS_IsTypedArrayObject(obj), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(JS_IsUint8Array(obj), NS_ERROR_FAILURE);
    uint32_t nbytes = JS_GetTypedArrayByteLength(obj);
    uint8_t* data = JS_GetUint8ArrayData(obj);
    if (!data) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    FallibleTArray<uint8_t> fallibleArr;
    if (!fallibleArr.InsertElementsAt(0, data, nbytes)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    InfallibleTArray<uint8_t> arr;
    arr.SwapElements(fallibleArr);
    SendData(arr);
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
