/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_notificationevent_h__
#define mozilla_dom_workers_notificationevent_h__

#include "mozilla/dom/Event.h"
#include "mozilla/dom/NotificationEventBinding.h"
#include "mozilla/dom/ServiceWorkerEvents.h"
#include "mozilla/dom/WorkerCommon.h"

namespace mozilla {
namespace dom {

class ServiceWorker;
class ServiceWorkerClient;

class NotificationEvent final : public ExtendableEvent
{
protected:
  explicit NotificationEvent(EventTarget* aOwner);
  ~NotificationEvent()
  {}

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NotificationEvent, ExtendableEvent)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return NotificationEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  static already_AddRefed<NotificationEvent>
  Constructor(mozilla::dom::EventTarget* aOwner,
              const nsAString& aType,
              const NotificationEventInit& aOptions,
              ErrorResult& aRv)
  {
    RefPtr<NotificationEvent> e = new NotificationEvent(aOwner);
    bool trusted = e->Init(aOwner);
    e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
    e->SetTrusted(trusted);
    e->SetComposed(aOptions.mComposed);
    e->mNotification = aOptions.mNotification;
    e->SetWantsPopupControlCheck(e->IsTrusted());
    return e.forget();
  }

  static already_AddRefed<NotificationEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const NotificationEventInit& aOptions,
              ErrorResult& aRv)
  {
    nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
    return Constructor(owner, aType, aOptions, aRv);
  }

  already_AddRefed<Notification>
  Notification_()
  {
    RefPtr<Notification> n = mNotification;
    return n.forget();
  }

private:
  RefPtr<Notification> mNotification;
};

} // dom namspace
} // mozilla namespace

#endif /* mozilla_dom_workers_notificationevent_h__ */
