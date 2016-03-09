/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ServiceWorkerMessageEvent.h"
#include "mozilla/dom/ServiceWorkerMessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/MessagePortList.h"

#include "mozilla/HoldDropJSObjects.h"
#include "jsapi.h"

#include "ServiceWorker.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(ServiceWorkerMessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ServiceWorkerMessageEvent, Event)
  tmp->mData.setUndefined();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServiceWorker)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPorts)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServiceWorkerMessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPorts)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ServiceWorkerMessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerMessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerMessageEvent, Event)

ServiceWorkerMessageEvent::ServiceWorkerMessageEvent(EventTarget* aOwner,
                                                     nsPresContext* aPresContext,
                                                     WidgetEvent* aEvent)
  : Event(aOwner, aPresContext, aEvent)
  , mData(JS::UndefinedValue())
{
  mozilla::HoldJSObjects(this);
}

ServiceWorkerMessageEvent::~ServiceWorkerMessageEvent()
{
  mData.setUndefined();
  DropJSObjects(this);
}

JSObject*
ServiceWorkerMessageEvent::WrapObjectInternal(JSContext* aCx,
                                              JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::ServiceWorkerMessageEventBinding::Wrap(aCx, this, aGivenProto);
}


void
ServiceWorkerMessageEvent::GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData,
                                   ErrorResult& aRv) const
{
  JS::ExposeValueToActiveJS(mData);
  aData.set(mData);
  if (!JS_WrapValue(aCx, aData)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void
ServiceWorkerMessageEvent::GetOrigin(nsAString& aOrigin) const
{
  aOrigin = mOrigin;
}

void
ServiceWorkerMessageEvent::GetLastEventId(nsAString& aLastEventId) const
{
  aLastEventId = mLastEventId;
}

void
ServiceWorkerMessageEvent::GetSource(Nullable<OwningServiceWorkerOrMessagePort>& aValue) const
{
  if (mServiceWorker) {
    aValue.SetValue().SetAsServiceWorker() = mServiceWorker;
  } else if (mMessagePort) {
    aValue.SetValue().SetAsMessagePort() = mMessagePort;
  }
}

void
ServiceWorkerMessageEvent::SetSource(mozilla::dom::MessagePort* aPort)
{
  mMessagePort = aPort;
}

void
ServiceWorkerMessageEvent::SetSource(workers::ServiceWorker* aServiceWorker)
{
  mServiceWorker = aServiceWorker;
}

void
ServiceWorkerMessageEvent::SetPorts(MessagePortList* aPorts)
{
  MOZ_ASSERT(!mPorts && aPorts);
  mPorts = aPorts;
}

MessagePortList*
ServiceWorkerMessageEvent::GetPorts() const
{
  return mPorts;
}

/* static */ already_AddRefed<ServiceWorkerMessageEvent>
ServiceWorkerMessageEvent::Constructor(const GlobalObject& aGlobal,
                                       const nsAString& aType,
                                       const ServiceWorkerMessageEventInit& aParam,
                                       ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(t, aType, aParam, aRv);
}

/* static */ already_AddRefed<ServiceWorkerMessageEvent>
ServiceWorkerMessageEvent::Constructor(EventTarget* aEventTarget,
                                       const nsAString& aType,
                                       const ServiceWorkerMessageEventInit& aParam,
                                       ErrorResult& aRv)
{
  RefPtr<ServiceWorkerMessageEvent> event =
    new ServiceWorkerMessageEvent(aEventTarget, nullptr, nullptr);

  event->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);

  bool trusted = event->Init(aEventTarget);
  event->SetTrusted(trusted);

  event->mData = aParam.mData;

  if (aParam.mOrigin.WasPassed()) {
    event->mOrigin = aParam.mOrigin.Value();
  }

  if (aParam.mLastEventId.WasPassed()) {
    event->mLastEventId = aParam.mLastEventId.Value();
  }

  if (aParam.mSource.WasPassed() && !aParam.mSource.Value().IsNull()) {

    if (aParam.mSource.Value().Value().IsServiceWorker()) {
      event->mServiceWorker = aParam.mSource.Value().Value().GetAsServiceWorker();
    } else {
      event->mMessagePort = aParam.mSource.Value().Value().GetAsMessagePort();
    }

    MOZ_ASSERT(event->mServiceWorker || event->mMessagePort);
  }

  if (aParam.mPorts.WasPassed() && !aParam.mPorts.Value().IsNull()) {
    nsTArray<RefPtr<MessagePort>> ports;
    for (uint32_t i = 0, len = aParam.mPorts.Value().Value().Length(); i < len; ++i) {
      ports.AppendElement(aParam.mPorts.Value().Value()[i].get());
    }

    event->mPorts = new MessagePortList(static_cast<EventBase*>(event), ports);
  }

  return event.forget();
}

} // namespace dom
} // namespace mozilla
