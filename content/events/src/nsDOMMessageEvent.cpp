/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMessageEvent.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/MessagePortList.h"
#include "mozilla/dom/UnionTypes.h"

#include "mozilla/HoldDropJSObjects.h"
#include "jsapi.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  tmp->mData = JSVAL_VOID;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindowSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPortSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPorts)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindowSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPortSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPorts)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsDOMMessageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMMessageEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMessageEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMessageEvent, nsDOMEvent)

nsDOMMessageEvent::nsDOMMessageEvent(mozilla::dom::EventTarget* aOwner,
                                     nsPresContext* aPresContext,
                                     WidgetEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent),
    mData(JSVAL_VOID)
{
}

nsDOMMessageEvent::~nsDOMMessageEvent()
{
  mData = JSVAL_VOID;
  mozilla::DropJSObjects(this);
}

NS_IMETHODIMP
nsDOMMessageEvent::GetData(JSContext* aCx, JS::Value* aData)
{
  ErrorResult rv;
  *aData = GetData(aCx, rv);
  return rv.ErrorCode();
}

JS::Value
nsDOMMessageEvent::GetData(JSContext* aCx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> data(aCx, mData);
  if (!JS_WrapValue(aCx, data.address())) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
  return data;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetOrigin(nsAString& aOrigin)
{
  aOrigin = mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetLastEventId(nsAString& aLastEventId)
{
  aLastEventId = mLastEventId;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMessageEvent::GetSource(nsIDOMWindow** aSource)
{
  NS_IF_ADDREF(*aSource = mWindowSource);
  return NS_OK;
}

void
nsDOMMessageEvent::GetSource(Nullable<mozilla::dom::OwningWindowProxyOrMessagePort>& aValue) const
{
  if (mWindowSource) {
    aValue.SetValue().SetAsWindowProxy() = mWindowSource;
  } else if (mPortSource) {
    aValue.SetValue().SetAsMessagePort() = mPortSource;
  }
}

/* static */ already_AddRefed<nsDOMMessageEvent>
nsDOMMessageEvent::Constructor(const mozilla::dom::GlobalObject& aGlobal,
                               JSContext* aCx, const nsAString& aType,
                               const mozilla::dom::MessageEventInit& aParam,
                               mozilla::ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t =
    do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<nsDOMMessageEvent> event =
    new nsDOMMessageEvent(t, nullptr, nullptr);

  aRv = event->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  if (aRv.Failed()) {
    return nullptr;
  }

  bool trusted = event->Init(t);
  event->SetTrusted(trusted);

  if (aParam.mData.WasPassed()) {
    event->mData = aParam.mData.Value();
  }

  mozilla::HoldJSObjects(event.get());

  if (aParam.mOrigin.WasPassed()) {
    event->mOrigin = aParam.mOrigin.Value();
  }

  if (aParam.mLastEventId.WasPassed()) {
    event->mLastEventId = aParam.mLastEventId.Value();
  }

  if (aParam.mSource) {
    nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
    nsContentUtils::XPConnect()->
      GetWrappedNativeOfJSObject(aCx, aParam.mSource,
                                 getter_AddRefs(wrappedNative));

    if (wrappedNative) {
      event->mWindowSource = do_QueryWrappedNative(wrappedNative);
    }

    if (!event->mWindowSource) {
      MessagePort* port = nullptr;
      nsresult rv = UNWRAP_OBJECT(MessagePort, aCx, aParam.mSource, port);
      if (NS_FAILED(rv)) {
        aRv.Throw(NS_ERROR_INVALID_ARG);
        return nullptr;
      }

      event->mPortSource = port;
    }
  }

  if (aParam.mPorts.WasPassed() && !aParam.mPorts.Value().IsNull()) {
    nsTArray<nsRefPtr<MessagePort> > ports;
    for (uint32_t i = 0, len = aParam.mPorts.Value().Value().Length(); i < len; ++i) {
      ports.AppendElement(aParam.mPorts.Value().Value()[i].get());
    }

    event->mPorts = new MessagePortList(static_cast<nsDOMEventBase*>(event),
                                        ports);
  }

  return event.forget();
}

NS_IMETHODIMP
nsDOMMessageEvent::InitMessageEvent(const nsAString& aType,
                                    bool aCanBubble,
                                    bool aCancelable,
                                    const JS::Value& aData,
                                    const nsAString& aOrigin,
                                    const nsAString& aLastEventId,
                                    nsIDOMWindow* aSource)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mData = aData;
  mozilla::HoldJSObjects(this);
  mOrigin = aOrigin;
  mLastEventId = aLastEventId;
  mWindowSource = aSource;

  return NS_OK;
}

nsresult
NS_NewDOMMessageEvent(nsIDOMEvent** aInstancePtrResult,
                      mozilla::dom::EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      WidgetEvent* aEvent) 
{
  nsDOMMessageEvent* it = new nsDOMMessageEvent(aOwner, aPresContext, aEvent);

  return CallQueryInterface(it, aInstancePtrResult);
}
