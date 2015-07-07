/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPSocketParent.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsJSUtils.h"
#include "nsIDOMTCPSocket.h"
#include "mozilla/unused.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/PNeckoParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"

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
                           NS_LITERAL_STRING("connecting"));
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TCPSocketParentBase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TCPSocketParentBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSocket)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIntermediary)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TCPSocketParentBase)
  tmp->mIntermediaryObj = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSocket)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIntermediary)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TCPSocketParentBase)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mIntermediaryObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPSocketParentBase)
  NS_INTERFACE_MAP_ENTRY(nsITCPSocketParent)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TCPSocketParentBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TCPSocketParentBase)

TCPSocketParentBase::TCPSocketParentBase()
: mIPCOpen(false)
{
  mObserver = new mozilla::net::OfflineObserver(this);
  mozilla::HoldJSObjects(this);
}

TCPSocketParentBase::~TCPSocketParentBase()
{
  if (mObserver) {
    mObserver->RemoveObserver();
  }
  mozilla::DropJSObjects(this);
}

uint32_t
TCPSocketParent::GetAppId()
{
  uint32_t appId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
  const PContentParent *content = Manager()->Manager();
  const InfallibleTArray<PBrowserParent*>& browsers = content->ManagedPBrowserParent();
  if (browsers.Length() > 0) {
    TabParent *tab = TabParent::GetFrom(browsers[0]);
    appId = tab->OwnAppId();
  }
  return appId;
};

bool
TCPSocketParent::GetInBrowser()
{
  bool inBrowser = false;
  const PContentParent *content = Manager()->Manager();
  const InfallibleTArray<PBrowserParent*>& browsers = content->ManagedPBrowserParent();
  if (browsers.Length() > 0) {
    TabParent *tab = TabParent::GetFrom(browsers[0]);
    inBrowser = tab->IsBrowserElement();
  }
  return inBrowser;
}

nsresult
TCPSocketParent::OfflineNotification(nsISupports *aSubject)
{
  nsCOMPtr<nsIAppOfflineInfo> info(do_QueryInterface(aSubject));
  if (!info) {
    return NS_OK;
  }

  uint32_t targetAppId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
  info->GetAppId(&targetAppId);

  // Obtain App ID
  uint32_t appId = GetAppId();
  if (appId != targetAppId) {
    return NS_OK;
  }

  // If the app is offline, close the socket
  if (mSocket && NS_IsAppOffline(appId)) {
    mSocket->Close();
    mSocket = nullptr;
    mIntermediaryObj = nullptr;
    mIntermediary = nullptr;
  }

  return NS_OK;
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

NS_IMETHODIMP_(MozExternalRefCountType) TCPSocketParent::Release(void)
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

  // Obtain App ID
  uint32_t appId = GetAppId();
  bool     inBrowser = GetInBrowser();

  if (NS_IsAppOffline(appId)) {
    NS_ERROR("Can't open socket because app is offline");
    FireInteralError(this, __LINE__);
    return true;
  }

  nsresult rv;
  mIntermediary = do_CreateInstance("@mozilla.org/tcp-socket-intermediary;1", &rv);
  if (NS_FAILED(rv)) {
    FireInteralError(this, __LINE__);
    return true;
  }

  rv = mIntermediary->Open(this, aHost, aPort, aUseSSL, aBinaryType, appId,
                           inBrowser, getter_AddRefs(mSocket));
  if (NS_FAILED(rv) || !mSocket) {
    FireInteralError(this, __LINE__);
    return true;
  }

  return true;
}

NS_IMETHODIMP
TCPSocketParent::InitJS(JS::Handle<JS::Value> aIntermediary, JSContext* aCx)
{
  MOZ_ASSERT(aIntermediary.isObject());
  mIntermediaryObj = &aIntermediary.toObject();
  return NS_OK;
}

bool
TCPSocketParent::RecvStartTLS()
{
  NS_ENSURE_TRUE(mSocket, true);
  nsresult rv = mSocket->UpgradeToSecure();
  NS_ENSURE_SUCCESS(rv, true);
  return true;
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
TCPSocketParent::RecvData(const SendableData& aData,
                          const uint32_t& aTrackingNumber)
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
      rv = mIntermediary->OnRecvSendArrayBuffer(val, aTrackingNumber);
      NS_ENSURE_SUCCESS(rv, true);
      break;
    }

    case SendableData::TnsString:
      rv = mIntermediary->OnRecvSendString(aData.get_nsString(), aTrackingNumber);
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
TCPSocketParent::SendEvent(const nsAString& aType, JS::Handle<JS::Value> aDataVal,
                           const nsAString& aReadyState, JSContext* aCx)
{
  if (!mIPCOpen) {
    NS_WARNING("Dropping callback due to no IPC connection");
    return NS_OK;
  }

  CallbackData data;
  if (aDataVal.isString()) {
    JSString* jsstr = aDataVal.toString();
    nsAutoJSString str;
    if (!str.init(aCx, jsstr)) {
      FireInteralError(this, __LINE__);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    data = SendableData(str);

  } else if (aDataVal.isUndefined() || aDataVal.isNull()) {
    data = mozilla::void_t();

  } else if (aDataVal.isObject()) {
    JS::Rooted<JSObject *> obj(aCx, &aDataVal.toObject());
    if (JS_IsArrayBufferObject(obj)) {
      FallibleTArray<uint8_t> fallibleArr;
      uint32_t errLine = 0;
      do {
          JS::AutoCheckCannotGC nogc;
          uint32_t nbytes = JS_GetArrayBufferByteLength(obj);
          uint8_t* buffer = JS_GetArrayBufferData(obj, nogc);
          if (!buffer) {
              errLine = __LINE__;
              break;
          }
          if (!fallibleArr.InsertElementsAt(0, buffer, nbytes, fallible)) {
              errLine = __LINE__;
              break;
          }
      } while (false);

      if (errLine) {
          FireInteralError(this, errLine);
          return NS_ERROR_OUT_OF_MEMORY;
      }

      InfallibleTArray<uint8_t> arr;
      arr.SwapElements(fallibleArr);
      data = SendableData(arr);

    } else {
      nsAutoJSString name;

      JS::Rooted<JS::Value> val(aCx);
      if (!JS_GetProperty(aCx, obj, "name", &val)) {
        NS_ERROR("No name property on supposed error object");
      } else if (val.isString()) {
        if (!name.init(aCx, val.toString())) {
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
                                     nsString(aReadyState));
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

NS_IMETHODIMP
TCPSocketParent::SendUpdateBufferedAmount(uint32_t aBufferedAmount,
                                          uint32_t aTrackingNumber)
{
  mozilla::unused << PTCPSocketParent::SendUpdateBufferedAmount(aBufferedAmount,
                                                                aTrackingNumber);
  return NS_OK;
}

NS_IMETHODIMP
TCPSocketParent::GetHost(nsAString& aHost)
{
  if (!mSocket) {
    NS_ERROR("No internal socket instance mSocket!");
    return NS_ERROR_FAILURE;
  }
  return mSocket->GetHost(aHost);
}

NS_IMETHODIMP
TCPSocketParent::GetPort(uint16_t* aPort)
{
  if (!mSocket) {
    NS_ERROR("No internal socket instance mSocket!");
    return NS_ERROR_FAILURE;
  }
  return mSocket->GetPort(aPort);
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
