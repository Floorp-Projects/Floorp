/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventCallbackDebuggerNotification.h"

#include "DebuggerNotificationManager.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/Worker.h"
#include "mozilla/dom/XMLHttpRequestEventTarget.h"
#include "nsIGlobalObject.h"
#include "nsINode.h"
#include "nsQueryObject.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(EventCallbackDebuggerNotification,
                                   CallbackDebuggerNotification, mEvent)

NS_IMPL_ADDREF_INHERITED(EventCallbackDebuggerNotification,
                         CallbackDebuggerNotification)
NS_IMPL_RELEASE_INHERITED(EventCallbackDebuggerNotification,
                          CallbackDebuggerNotification)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EventCallbackDebuggerNotification)
NS_INTERFACE_MAP_END_INHERITING(CallbackDebuggerNotification)

JSObject* EventCallbackDebuggerNotification::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return EventCallbackDebuggerNotification_Binding::Wrap(aCx, this,
                                                         aGivenProto);
}

already_AddRefed<DebuggerNotification>
EventCallbackDebuggerNotification::CloneInto(nsIGlobalObject* aNewOwner) const {
  RefPtr<EventCallbackDebuggerNotification> notification(
      new EventCallbackDebuggerNotification(mDebuggeeGlobal, mType, mEvent,
                                            mTargetType, mPhase, aNewOwner));
  return notification.forget();
}

void EventCallbackDebuggerNotificationGuard::DispatchToManager(
    const RefPtr<DebuggerNotificationManager>& aManager,
    CallbackDebuggerNotificationPhase aPhase) {
  if (!mEventTarget) {
    MOZ_ASSERT(false, "target should exist");
    return;
  }

  Maybe<EventCallbackDebuggerNotificationType> notificationType(
      mEventTarget->GetDebuggerNotificationType());

  if (notificationType) {
    aManager->Dispatch<EventCallbackDebuggerNotification>(
        DebuggerNotificationType::DomEvent,
        // The DOM event will always be live during event dispatch.
        MOZ_KnownLive(mEvent), *notificationType, aPhase);
  }
}

}  // namespace mozilla::dom
