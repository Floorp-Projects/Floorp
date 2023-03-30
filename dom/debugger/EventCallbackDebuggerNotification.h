/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EventCallbackDebuggerNotification_h
#define mozilla_dom_EventCallbackDebuggerNotification_h

#include "CallbackDebuggerNotification.h"
#include "DebuggerNotificationManager.h"
#include "mozilla/dom/Event.h"
#include "mozilla/RefPtr.h"

namespace mozilla::dom {

class EventCallbackDebuggerNotification : public CallbackDebuggerNotification {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(EventCallbackDebuggerNotification,
                                           CallbackDebuggerNotification)

  EventCallbackDebuggerNotification(
      nsIGlobalObject* aDebuggeeGlobal, DebuggerNotificationType aType,
      Event* aEvent, EventCallbackDebuggerNotificationType aTargetType,
      CallbackDebuggerNotificationPhase aPhase,
      nsIGlobalObject* aOwnerGlobal = nullptr)
      : CallbackDebuggerNotification(aDebuggeeGlobal, aType, aPhase,
                                     aOwnerGlobal),
        mEvent(aEvent),
        mTargetType(aTargetType) {}

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<DebuggerNotification> CloneInto(
      nsIGlobalObject* aNewOwner) const override;

  mozilla::dom::Event* Event() const { return mEvent; }
  EventCallbackDebuggerNotificationType TargetType() const {
    return mTargetType;
  }

 private:
  ~EventCallbackDebuggerNotification() = default;

  RefPtr<mozilla::dom::Event> mEvent;
  EventCallbackDebuggerNotificationType mTargetType;
};

class MOZ_RAII EventCallbackDebuggerNotificationGuard final {
 public:
  MOZ_CAN_RUN_SCRIPT_BOUNDARY explicit EventCallbackDebuggerNotificationGuard(
      mozilla::dom::EventTarget* aEventTarget, mozilla::dom::Event* aEvent)
      : mDebuggeeGlobal(aEventTarget ? aEventTarget->GetOwnerGlobal()
                                     : nullptr),
        mEventTarget(aEventTarget),
        mEvent(aEvent) {
    Dispatch(CallbackDebuggerNotificationPhase::Pre);
  }
  EventCallbackDebuggerNotificationGuard(
      const EventCallbackDebuggerNotificationGuard&) = delete;
  EventCallbackDebuggerNotificationGuard(
      EventCallbackDebuggerNotificationGuard&&) = delete;
  EventCallbackDebuggerNotificationGuard& operator=(
      const EventCallbackDebuggerNotificationGuard&) = delete;
  EventCallbackDebuggerNotificationGuard& operator=(
      EventCallbackDebuggerNotificationGuard&&) = delete;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY ~EventCallbackDebuggerNotificationGuard() {
    Dispatch(CallbackDebuggerNotificationPhase::Post);
  }

 private:
  MOZ_CAN_RUN_SCRIPT void Dispatch(CallbackDebuggerNotificationPhase aPhase) {
    auto manager = DebuggerNotificationManager::ForDispatch(mDebuggeeGlobal);
    if (MOZ_UNLIKELY(manager)) {
      DispatchToManager(manager, aPhase);
    }
  }

  MOZ_CAN_RUN_SCRIPT void DispatchToManager(
      const RefPtr<DebuggerNotificationManager>& aManager,
      CallbackDebuggerNotificationPhase aPhase);

  nsIGlobalObject* mDebuggeeGlobal;
  mozilla::dom::EventTarget* mEventTarget;
  mozilla::dom::Event* mEvent;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EventCallbackDebuggerNotification_h
