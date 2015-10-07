/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkermessageevent_h__
#define mozilla_dom_serviceworkermessageevent_h__

#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {

struct ServiceWorkerMessageEventInit;
class MessagePort;
class MessagePortList;
class OwningServiceWorkerOrMessagePort;

namespace workers {

class ServiceWorker;

}

class ServiceWorkerMessageEvent final : public Event
{
public:
  ServiceWorkerMessageEvent(EventTarget* aOwner,
                            nsPresContext* aPresContext,
                            WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ServiceWorkerMessageEvent, Event)

  // Forward to base class
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData,
               ErrorResult& aRv) const;

  void GetOrigin(nsAString& aOrigin) const;

  void GetLastEventId(nsAString& aLastEventId) const;

  void GetSource(Nullable<OwningServiceWorkerOrMessagePort>& aValue) const;

  MessagePortList* GetPorts() const;

  void SetSource(mozilla::dom::MessagePort* aPort);

  void SetSource(workers::ServiceWorker* aServiceWorker);

  void SetPorts(MessagePortList* aPorts);

  static already_AddRefed<ServiceWorkerMessageEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const ServiceWorkerMessageEventInit& aEventInit,
              ErrorResult& aRv);

  static already_AddRefed<ServiceWorkerMessageEvent>
  Constructor(EventTarget* aEventTarget,
              const nsAString& aType,
              const ServiceWorkerMessageEventInit& aEventInit,
              ErrorResult& aRv);

protected:
  ~ServiceWorkerMessageEvent();

private:
  JS::Heap<JS::Value> mData;
  nsString mOrigin;
  nsString mLastEventId;
  nsRefPtr<workers::ServiceWorker> mServiceWorker;
  nsRefPtr<MessagePort> mMessagePort;
  nsRefPtr<MessagePortList> mPorts;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_workers_serviceworkermessageevent_h__ */

