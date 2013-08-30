/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPSocketParent.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsJSUtils.h"
#include "nsIDOMTCPSocket.h"
#include "nsCxPusher.h"
#include "mozilla/unused.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/PNeckoParent.h"

namespace IPC {

//Defined in TCPSocketChild.cpp
extern bool
DeserializeArrayBuffer(JS::Handle<JSObject*> aObj,
                       const InfallibleTArray<uint8_t>& aBuffer,
                       JS::MutableHandle<JS::Value> aVal);

}

namespace mozilla {
namespace dom {

static void
FireInteralError(mozilla::net::PTCPSocketParent* aActor, uint32_t aLineNo)
{
  mozilla::unused <<
      aActor->SendCallback(NS_LITERAL_STRING("onerror"),
                           TCPError(NS_LITERAL_STRING("InvalidStateError")),
                           NS_LITERAL_STRING("connecting"), 0);
}

NS_IMPL_CYCLE_COLLECTION_2(TCPSocketParentBase, mSocket, mIntermediary)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPSocketParentBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPSocketParentBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPSocketParentBase)
  NS_INTERFACE_MAP_ENTRY(nsITCPSocketParent)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TCPSocketParentBase::TCPSocketParentBase()
: mIPCOpen(false)
{
}

TCPSocketParentBase::~TCPSocketParentBase()
{
}

void
TCPSocketParentBase::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen);
  mIPCOpen = false;
  this->Release();
}

void
TCPSocketParentBase::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen);
  mIPCOpen = true;
  this->AddRef();
}

NS_IMETHODIMP_(nsrefcnt) TCPSocketParent::Release(void)
{
  nsrefcnt refcnt = TCPSocketParentBase::Release();
  if (refcnt == 1 && mIPCOpen) {
    mozilla::unused << PTCPSocketParent::SendRequestDelete();
    return 1;
  }
  return refcnt;
}

bool
TCPSocketParent::RecvOpen(const nsString& aHost, const uint16_t& aPort, const bool& aUseSSL,
                          const nsString& aBinaryType)
{
  // We don't have browser actors in xpcshell, and hence can't run automated
  // tests without this loophole.
  if (net::UsingNeckoIPCSecurity() &&
      !AssertAppProcessPermission(Manager()->Manager(), "tcp-socket")) {
    FireInteralError(this, __LINE__);
    return true;
  }

  nsresult rv;
  mIntermediary = do_CreateInstance("@mozilla.org/tcp-socket-intermediary;1", &rv);
  if (NS_FAILED(rv)) {
    FireInteralError(this, __LINE__);
    return true;
  }

  rv = mIntermediary->Open(this, aHost, aPort, aUseSSL, aBinaryType, getter_AddRefs(mSocket));
  if (NS_FAILED(rv) || !mSocket) {
    FireInteralError(this, __LINE__);
    return true;
  }

  return true;
}

NS_IMETHODIMP
TCPSocketParent::InitJS(const JS::Value& aIntermediary, JSContext* aCx)
{
  MOZ_ASSERT(aIntermediary.isObject());
  mIntermediaryObj = &aIntermediary.toObject();
  return NS_OK;
}

bool
TCPSocketParent::RecvSuspend()
{
  NS_ENSURE_TRUE(mSocket, true);
  nsresult rv = mSocket->Suspend();
  NS_ENSURE_SUCCESS(rv, true);
  return true;
}

bool
TCPSocketParent::RecvResume()
{
  NS_ENSURE_TRUE(mSocket, true);
  nsresult rv = mSocket->Resume();
  NS_ENSURE_SUCCESS(rv, true);
  return true;
}

bool
TCPSocketParent::RecvData(const SendableData& aData)
{
  NS_ENSURE_TRUE(mIntermediary, true);

  nsresult rv;
  switch (aData.type()) {
    case SendableData::TArrayOfuint8_t: {
      AutoSafeJSContext cx;
      JSAutoRequest ar(cx);
      JS::Rooted<JS::Value> val(cx);
      JS::Rooted<JSObject*> obj(cx, mIntermediaryObj);
      IPC::DeserializeArrayBuffer(obj, aData.get_ArrayOfuint8_t(), &val);
      rv = mIntermediary->SendArrayBuffer(val);
      NS_ENSURE_SUCCESS(rv, true);
      break;
    }

    case SendableData::TnsString:
      rv = mIntermediary->SendString(aData.get_nsString());
      NS_ENSURE_SUCCESS(rv, true);
      break;

    default:
      MOZ_CRASH("unexpected SendableData type");
  }
  return true;
}

bool
TCPSocketParent::RecvClose()
{
  NS_ENSURE_TRUE(mSocket, true);
  nsresult rv = mSocket->Close();
  NS_ENSURE_SUCCESS(rv, true);
  return true;
}

NS_IMETHODIMP
TCPSocketParent::SendCallback(const nsAString& aType, const JS::Value& aDataVal,
                              const nsAString& aReadyState, uint32_t aBuffered,
                              JSContext* aCx)
{
  if (!mIPCOpen) {
    NS_WARNING("Dropping callback due to no IPC connection");
    return NS_OK;
  }

  CallbackData data;
  if (aDataVal.isString()) {
    JSString* jsstr = aDataVal.toString();
    nsDependentJSString str;
    if (!str.init(aCx, jsstr)) {
      FireInteralError(this, __LINE__);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    data = SendableData(str);

  } else if (aDataVal.isUndefined() || aDataVal.isNull()) {
    data = mozilla::void_t();

  } else if (aDataVal.isObject()) {
    JSObject* obj = &aDataVal.toObject();
    if (JS_IsArrayBufferObject(obj)) {
      uint32_t nbytes = JS_GetArrayBufferByteLength(obj);
      uint8_t* buffer = JS_GetArrayBufferData(obj);
      if (!buffer) {
        FireInteralError(this, __LINE__);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      FallibleTArray<uint8_t> fallibleArr;
      if (!fallibleArr.InsertElementsAt(0, buffer, nbytes)) {
        FireInteralError(this, __LINE__);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      InfallibleTArray<uint8_t> arr;
      arr.SwapElements(fallibleArr);
      data = SendableData(arr);

    } else {
      nsDependentJSString name;

      JS::Rooted<JS::Value> val(aCx);
      if (!JS_GetProperty(aCx, obj, "name", &val)) {
        NS_ERROR("No name property on supposed error object");
      } else if (JSVAL_IS_STRING(val)) {
        if (!name.init(aCx, JSVAL_TO_STRING(val))) {
          NS_WARNING("couldn't initialize string");
        }
      }

      data = TCPError(name);
    }
  } else {
    NS_ERROR("Unexpected JS value encountered");
    FireInteralError(this, __LINE__);
    return NS_ERROR_FAILURE;
  }
  mozilla::unused <<
      PTCPSocketParent::SendCallback(nsString(aType), data,
                                     nsString(aReadyState), aBuffered);
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketParent::SetSocketAndIntermediary(nsIDOMTCPSocket *socket,
                                          nsITCPSocketIntermediary *intermediary,
                                          JSContext* cx)
{
  mSocket = socket;
  mIntermediary = intermediary;
  return NS_OK;
}

void
TCPSocketParent::ActorDestroy(ActorDestroyReason why)
{
  if (mSocket) {
    mSocket->Close();
  }
  mSocket = nullptr;
  mIntermediaryObj = nullptr;
  mIntermediary = nullptr;
}

bool
TCPSocketParent::RecvRequestDelete()
{
  mozilla::unused << Send__delete__(this);
  return true;
}

} // namespace dom
} // namespace mozilla
