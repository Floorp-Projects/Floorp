/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"

#include "mozilla/HoldDropJSObjects.h"
#include "jsapi.h"
#include "nsGlobalWindow.h" // So we can assign an nsGlobalWindow* to mWindowSource

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessageEvent, Event)
  tmp->mData.setUndefined();
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
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(MessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(MessageEvent, Event)

MessageEvent::MessageEvent(EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           WidgetEvent* aEvent)
  : Event(aOwner, aPresContext, aEvent)
  , mData(JS::UndefinedValue())
{
}

MessageEvent::~MessageEvent()
{
  mData.setUndefined();
  DropJSObjects(this);
}

JSObject*
MessageEvent::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::MessageEventBinding::Wrap(aCx, this, aGivenProto);
}

void
MessageEvent::GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData,
                      ErrorResult& aRv)
{
  aData.set(mData);
  if (!JS_WrapValue(aCx, aData)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void
MessageEvent::GetOrigin(nsAString& aOrigin) const
{
  aOrigin = mOrigin;
}

void
MessageEvent::GetLastEventId(nsAString& aLastEventId) const
{
  aLastEventId = mLastEventId;
}

void
MessageEvent::GetSource(Nullable<OwningWindowProxyOrMessagePort>& aValue) const
{
  if (mWindowSource) {
    aValue.SetValue().SetAsWindowProxy() = mWindowSource->GetOuterWindow();
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
  return Constructor(t, aType, aParam, aRv);
}

/* static */ already_AddRefed<MessageEvent>
MessageEvent::Constructor(EventTarget* aEventTarget,
                          const nsAString& aType,
                          const MessageEventInit& aParam,
                          ErrorResult& aRv)
{
  RefPtr<MessageEvent> event = new MessageEvent(aEventTarget, nullptr, nullptr);

  event->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  bool trusted = event->Init(aEventTarget);
  event->SetTrusted(trusted);

  event->mData = aParam.mData;

  mozilla::HoldJSObjects(event.get());

  event->mOrigin = aParam.mOrigin;
  event->mLastEventId = aParam.mLastEventId;

  if (!aParam.mSource.IsNull()) {
    if (aParam.mSource.Value().IsWindow()) {
      event->mWindowSource = aParam.mSource.Value().GetAsWindow()->AsInner();
    } else {
      event->mPortSource = aParam.mSource.Value().GetAsMessagePort();
    }

    MOZ_ASSERT(event->mWindowSource || event->mPortSource);
  }

  event->mPorts.AppendElements(aParam.mPorts);

  return event.forget();
}

void
MessageEvent::InitMessageEvent(JSContext* aCx, const nsAString& aType,
                               bool aCanBubble, bool aCancelable,
                               JS::Handle<JS::Value> aData,
                               const nsAString& aOrigin,
                               const nsAString& aLastEventId,
                               const Nullable<WindowProxyOrMessagePort>& aSource,
                               const Sequence<OwningNonNull<MessagePort>>& aPorts)
{
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  Event::InitEvent(aType, aCanBubble, aCancelable);
  mData = aData;
  mozilla::HoldJSObjects(this);
  mOrigin = aOrigin;
  mLastEventId = aLastEventId;

  mWindowSource = nullptr;
  mPortSource = nullptr;

  if (!aSource.IsNull()) {
    if (aSource.Value().IsWindowProxy()) {
      auto* windowProxy = aSource.Value().GetAsWindowProxy();
      mWindowSource = windowProxy ? windowProxy->GetCurrentInnerWindow() : nullptr;
    } else {
      mPortSource = &aSource.Value().GetAsMessagePort();
    }
  }

  mPorts.Clear();
  mPorts.AppendElements(aPorts);
  MessageEventBinding::ClearCachedPortsValue(this);
}

void
MessageEvent::GetPorts(nsTArray<RefPtr<MessagePort>>& aPorts)
{
  aPorts = mPorts;
}

} // namespace dom
} // namespace mozilla
