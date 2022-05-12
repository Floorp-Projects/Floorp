/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CallbackDebuggerNotification_h
#define mozilla_dom_CallbackDebuggerNotification_h

#include "DebuggerNotification.h"
#include "DebuggerNotificationManager.h"

namespace mozilla::dom {

class CallbackDebuggerNotification : public DebuggerNotification {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CallbackDebuggerNotification,
                                           DebuggerNotification)

  CallbackDebuggerNotification(nsIGlobalObject* aDebuggeeGlobal,
                               DebuggerNotificationType aType,
                               CallbackDebuggerNotificationPhase aPhase,
                               nsIGlobalObject* aOwnerGlobal = nullptr)
      : DebuggerNotification(aDebuggeeGlobal, aType, aOwnerGlobal),
        mPhase(aPhase) {}

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<DebuggerNotification> CloneInto(
      nsIGlobalObject* aNewOwner) const override;

  CallbackDebuggerNotificationPhase Phase() const { return mPhase; }

 protected:
  ~CallbackDebuggerNotification() = default;

  CallbackDebuggerNotificationPhase mPhase;
};

class MOZ_RAII CallbackDebuggerNotificationGuard final {
 public:
  MOZ_CAN_RUN_SCRIPT CallbackDebuggerNotificationGuard(
      nsIGlobalObject* aDebuggeeGlobal, DebuggerNotificationType aType)
      : mDebuggeeGlobal(aDebuggeeGlobal), mType(aType) {
    Dispatch(CallbackDebuggerNotificationPhase::Pre);
  }
  CallbackDebuggerNotificationGuard(const CallbackDebuggerNotificationGuard&) =
      delete;
  CallbackDebuggerNotificationGuard(CallbackDebuggerNotificationGuard&&) =
      delete;
  CallbackDebuggerNotificationGuard& operator=(
      const CallbackDebuggerNotificationGuard&) = delete;
  CallbackDebuggerNotificationGuard& operator=(
      CallbackDebuggerNotificationGuard&&) = delete;

  MOZ_CAN_RUN_SCRIPT ~CallbackDebuggerNotificationGuard() {
    Dispatch(CallbackDebuggerNotificationPhase::Post);
  }

 private:
  MOZ_CAN_RUN_SCRIPT void Dispatch(CallbackDebuggerNotificationPhase aPhase) {
    auto manager = DebuggerNotificationManager::ForDispatch(mDebuggeeGlobal);
    if (MOZ_UNLIKELY(manager)) {
      manager->Dispatch<CallbackDebuggerNotification>(mType, aPhase);
    }
  }

  nsIGlobalObject* mDebuggeeGlobal;
  DebuggerNotificationType mType;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CallbackDebuggerNotification_h
