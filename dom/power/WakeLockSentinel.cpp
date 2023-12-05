/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WakeLockSentinelBinding.h"
#include "mozilla/Hal.h"
#include "WakeLockJS.h"
#include "WakeLockSentinel.h"

namespace mozilla::dom {

JSObject* WakeLockSentinel::WrapObject(JSContext* cx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return WakeLockSentinel_Binding::Wrap(cx, this, aGivenProto);
}

bool WakeLockSentinel::Released() const { return mReleased; }

void WakeLockSentinel::NotifyLockReleased() {
  MOZ_ASSERT(!mReleased);
  mReleased = true;

  Telemetry::AccumulateTimeDelta(Telemetry::SCREENWAKELOCK_HELD_DURATION_MS,
                                 mCreationTime);

  hal::BatteryInformation batteryInfo;
  hal::GetCurrentBatteryInformation(&batteryInfo);
  if (!batteryInfo.charging()) {
    uint32_t level = static_cast<uint32_t>(100 * batteryInfo.level());
    Telemetry::Accumulate(
        Telemetry::SCREENWAKELOCK_RELEASE_BATTERY_LEVEL_DISCHARGING, level);
  }

  if (mHoldsActualLock) {
    MOZ_ASSERT(mType == WakeLockType::Screen);
    NS_DispatchToMainThread(NS_NewRunnableFunction("ReleaseWakeLock", []() {
      hal::ModifyWakeLock(u"screen"_ns, hal::WAKE_LOCK_REMOVE_ONE,
                          hal::WAKE_LOCK_NO_CHANGE);
    }));
    mHoldsActualLock = false;
  }

  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  RefPtr<Event> event = Event::Constructor(this, u"release"_ns, init);
  DispatchTrustedEvent(event);
}

void WakeLockSentinel::AcquireActualLock() {
  MOZ_ASSERT(mType == WakeLockType::Screen);
  MOZ_ASSERT(!mHoldsActualLock);
  mHoldsActualLock = true;
  NS_DispatchToMainThread(NS_NewRunnableFunction("AcquireWakeLock", []() {
    hal::ModifyWakeLock(u"screen"_ns, hal::WAKE_LOCK_ADD_ONE,
                        hal::WAKE_LOCK_NO_CHANGE);
  }));
}

// https://w3c.github.io/screen-wake-lock/#the-release-method
already_AddRefed<Promise> WakeLockSentinel::ReleaseLock(ErrorResult& aRv) {
  // ReleaseWakeLock will remove this from document.[[ActiveLocks]]
  RefPtr<WakeLockSentinel> kungFuDeathGrip(this);

  if (!mReleased) {
    nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
    if (!global) {
      aRv.Throw(NS_ERROR_NULL_POINTER);
      return nullptr;
    }
    nsCOMPtr<nsPIDOMWindowInner> window = global->GetAsInnerWindow();
    if (!window) {
      aRv.Throw(NS_ERROR_NULL_POINTER);
      return nullptr;
    }
    nsCOMPtr<Document> doc = window->GetExtantDoc();
    if (!doc) {
      aRv.Throw(NS_ERROR_NULL_POINTER);
      return nullptr;
    }
    ReleaseWakeLock(doc, this, mType);
  }

  if (RefPtr<Promise> p =
          Promise::CreateResolvedWithUndefined(GetOwnerGlobal(), aRv)) {
    return p.forget();
  }
  return nullptr;
}

}  // namespace mozilla::dom
