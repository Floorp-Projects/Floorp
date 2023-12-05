/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WAKELOCKSENTINEL_H_
#define DOM_WAKELOCKSENTINEL_H_

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/WakeLockBinding.h"

namespace mozilla::dom {

class Promise;

}  // namespace mozilla::dom

namespace mozilla::dom {

class WakeLockSentinel final : public DOMEventTargetHelper {
 public:
  WakeLockSentinel(nsIGlobalObject* aOwnerWindow, WakeLockType aType)
      : DOMEventTargetHelper(aOwnerWindow),
        mType(aType),
        mCreationTime(TimeStamp::Now()) {}

 protected:
  ~WakeLockSentinel() {
    MOZ_DIAGNOSTIC_ASSERT(mReleased);
    MOZ_DIAGNOSTIC_ASSERT(!mHoldsActualLock);
  }

 public:
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  bool Released() const;

  WakeLockType Type() const { return mType; }

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<Promise> ReleaseLock(ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  void NotifyLockReleased();

  IMPL_EVENT_HANDLER(release);

  // Acquire underlying system wake lock by modifying the HAL wake lock counter
  void AcquireActualLock();

 private:
  WakeLockType mType;

  bool mReleased = false;

  /**
   * To avoid user fingerprinting, WakeLockJS::Request will provide a
   * WakeLockSentinel even if the lock type is not applicable or cannot be
   * obtained.
   * But when releasing this sentinel, we have to know whether
   * AcquireActualLock was called.
   *
   * https://w3c.github.io/screen-wake-lock/#dfn-applicable-wake-lock
   * https://w3c.github.io/screen-wake-lock/#the-request-method
   */
  bool mHoldsActualLock = false;

  // Time when this object was created
  TimeStamp mCreationTime;
};

}  // namespace mozilla::dom

#endif  // DOM_WAKELOCKSENTINEL_H_
