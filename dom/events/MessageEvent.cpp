/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/MessagePortList.h"

#include "mozilla/HoldDropJSObjects.h"
#include "jsapi.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessageEvent, Event)
  tmp->mData = JSVAL_VOID;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindowSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPortSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPorts)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindowSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPortSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPorts)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MessageEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(MessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(MessageEvent, Event)

MessageEvent::MessageEvent(EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           WidgetEvent* aEvent)
  : Event(aOwner, aPresContext, aEvent)
  , mData(JSVAL_VOID)
{
}

MessageEvent::~MessageEvent()
{
  mData = JSVAL_VOID;
  DropJSObjects(this);
}

JSObject*
MessageEvent::WrapObjectInternal(JSContext* aCx)
{
  return mozilla::dom::MessageEventBinding::Wrap(aCx, this);
}

NS_IMETHODIMP
MessageEvent::GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData)
{
  ErrorResult rv;
  GetData(aCx, aData, rv);
  return rv.ErrorCode();
}

void
MessageEvent::GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData,
                      ErrorResult& aRv)
{
  JS::ExposeValueToActiveJS(mData);
  aData.set(mData);
  if (!JS_WrapValue(aCx, aData)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

NS_IMETHODIMP
MessageEvent::GetOrigin(nsAString& aOrigin)
{
  aOrigin = mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
MessageEvent::GetLastEventId(nsAString& aLastEventId)
{
  aLastEventId = mLastEventId;
  return NS_OK;
}

NS_IMETHODIMP
MessageEvent::GetSource(nsIDOMWindow** aSource)
{
  NS_IF_ADDREF(*aSource = mWindowSource);
  return NS_OK;
}

void
MessageEvent::GetSource(Nullable<OwningWindowProxyOrMessagePort>& aValue) const
{
  if (mWindowSource) {
    aValue.SetValue().SetAsWindowProxy() = mWindowSource;
  } else if (mPortSource) {
    aValue.SetValue().SetAsMessagePort() = mPortSource;
  }
}

/* static */ already_AddRefed<MessageEvent>
MessageEvent::Constructor(const GlobalObject& aGlobal,
                          const nsAString& aType,
                          const MessageEventInit& aParam,
                          ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<MessageEvent> event = new MessageEvent(t, nullptr, nullptr);

  aRv = event->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  if (aRv.Failed()) {
    return nullptr;
  }

  bool trusted = event->Init(t);
  event->SetTrusted(trusted);

  event->mData = aParam.mData;

  mozilla::HoldJSObjects(event.get());

  if (aParam.mOrigin.WasPassed()) {
    event->mOrigin = aParam.mOrigin.Value();
  }

  if (aParam.mLastEventId.WasPassed()) {
    event->mLastEventId = aParam.mLastEventId.Value();
  }

  if (!aParam.mSource.IsNull()) {
    if (aParam.mSource.Value().IsWindow()) {
      event->mWindowSource = aParam.mSource.Value().GetAsWindow();
    } else {
      event->mPortSource = aParam.mSource.Value().GetAsMessagePort();
    }

    MOZ_ASSERT(event->mWindowSource || event->mPortSource);
  }

  if (aParam.mPorts.WasPassed() && !aParam.mPorts.Value().IsNull()) {
    nsTArray<nsRefPtr<MessagePortBase>> ports;
    for (uint32_t i = 0, len = aParam.mPorts.Value().Value().Length(); i < len; ++i) {
      ports.AppendElement(aParam.mPorts.Value().Value()[i].get());
    }

    event->mPorts = new MessagePortList(static_cast<EventBase*>(event), ports);
  }

  return event.forget();
}

NS_IMETHODIMP
MessageEvent::InitMessageEvent(const nsAString& aType,
                               bool aCanBubble,
                               bool aCancelable,
                               JS::Handle<JS::Value> aData,
                               const nsAString& aOrigin,
                               const nsAString& aLastEventId,
                               nsIDOMWindow* aSource)
{
  nsresult rv = Event::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mData = aData;
  mozilla::HoldJSObjects(this);
  mOrigin = aOrigin;
  mLastEventId = aLastEventId;
  mWindowSource = aSource;

  return NS_OK;
}

void
MessageEvent::SetPorts(MessagePortList* aPorts)
{
  MOZ_ASSERT(!mPorts && aPorts);
  mPorts = aPorts;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMMessageEvent(nsIDOMEvent** aInstancePtrResult,
                      EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      WidgetEvent* aEvent) 
{
  MessageEvent* it = new MessageEvent(aOwner, aPresContext, aEvent);
  NS_ADDREF(it);
  *aInstancePtrResult = static_cast<Event*>(it);
  return NS_OK;
}
