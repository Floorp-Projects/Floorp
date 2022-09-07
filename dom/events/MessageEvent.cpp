/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/ServiceWorker.h"

#include "mozilla/HoldDropJSObjects.h"
#include "jsapi.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessageEvent, Event)
  tmp->mData.setUndefined();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindowSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPortSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServiceWorkerSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPorts)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindowSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPortSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorkerSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPorts)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(MessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(MessageEvent, Event)

MessageEvent::MessageEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                           WidgetEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent), mData(JS::UndefinedValue()) {}

MessageEvent::~MessageEvent() { DropJSObjects(this); }

JSObject* MessageEvent::WrapObjectInternal(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::MessageEvent_Binding::Wrap(aCx, this, aGivenProto);
}

void MessageEvent::GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData,
                           ErrorResult& aRv) {
  aData.set(mData);
  if (!JS_WrapValue(aCx, aData)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void MessageEvent::GetOrigin(nsAString& aOrigin) const { aOrigin = mOrigin; }

void MessageEvent::GetLastEventId(nsAString& aLastEventId) const {
  aLastEventId = mLastEventId;
}

void MessageEvent::GetSource(
    Nullable<OwningWindowProxyOrMessagePortOrServiceWorker>& aValue) const {
  if (mWindowSource) {
    aValue.SetValue().SetAsWindowProxy() = mWindowSource;
  } else if (mPortSource) {
    aValue.SetValue().SetAsMessagePort() = mPortSource;
  } else if (mServiceWorkerSource) {
    aValue.SetValue().SetAsServiceWorker() = mServiceWorkerSource;
  }
}

/* static */
already_AddRefed<MessageEvent> MessageEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const MessageEventInit& aParam) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(t, aType, aParam);
}

/* static */
already_AddRefed<MessageEvent> MessageEvent::Constructor(
    EventTarget* aEventTarget, const nsAString& aType,
    const MessageEventInit& aParam) {
  RefPtr<MessageEvent> event = new MessageEvent(aEventTarget, nullptr, nullptr);

  event->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  bool trusted = event->Init(aEventTarget);
  event->SetTrusted(trusted);

  event->mData = aParam.mData;

  mozilla::HoldJSObjects(event.get());

  event->mOrigin = aParam.mOrigin;
  event->mLastEventId = aParam.mLastEventId;

  if (!aParam.mSource.IsNull()) {
    if (aParam.mSource.Value().IsWindowProxy()) {
      event->mWindowSource = aParam.mSource.Value().GetAsWindowProxy().get();
    } else if (aParam.mSource.Value().IsMessagePort()) {
      event->mPortSource = aParam.mSource.Value().GetAsMessagePort();
    } else {
      event->mServiceWorkerSource = aParam.mSource.Value().GetAsServiceWorker();
    }

    MOZ_ASSERT(event->mWindowSource || event->mPortSource ||
               event->mServiceWorkerSource);
  }

  event->mPorts.AppendElements(aParam.mPorts);

  return event.forget();
}

void MessageEvent::InitMessageEvent(
    JSContext* aCx, const nsAString& aType, mozilla::CanBubble aCanBubble,
    mozilla::Cancelable aCancelable, JS::Handle<JS::Value> aData,
    const nsAString& aOrigin, const nsAString& aLastEventId,
    const Nullable<WindowProxyOrMessagePortOrServiceWorker>& aSource,
    const Sequence<OwningNonNull<MessagePort>>& aPorts) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  Event::InitEvent(aType, aCanBubble, aCancelable);
  mData = aData;
  mozilla::HoldJSObjects(this);
  mOrigin = aOrigin;
  mLastEventId = aLastEventId;

  mWindowSource = nullptr;
  mPortSource = nullptr;
  mServiceWorkerSource = nullptr;

  if (!aSource.IsNull()) {
    if (aSource.Value().IsWindowProxy()) {
      mWindowSource = aSource.Value().GetAsWindowProxy().get();
    } else if (aSource.Value().IsMessagePort()) {
      mPortSource = &aSource.Value().GetAsMessagePort();
    } else {
      mServiceWorkerSource = &aSource.Value().GetAsServiceWorker();
    }
  }

  mPorts.Clear();
  mPorts.AppendElements(aPorts);
  MessageEvent_Binding::ClearCachedPortsValue(this);
}

void MessageEvent::GetPorts(nsTArray<RefPtr<MessagePort>>& aPorts) {
  aPorts = mPorts.Clone();
}

}  // namespace mozilla::dom
