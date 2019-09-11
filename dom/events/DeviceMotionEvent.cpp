/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DeviceMotionEvent.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

/******************************************************************************
 * DeviceMotionEvent
 *****************************************************************************/

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeviceMotionEvent, Event, mAcceleration,
                                   mAccelerationIncludingGravity, mRotationRate)

NS_IMPL_ADDREF_INHERITED(DeviceMotionEvent, Event)
NS_IMPL_RELEASE_INHERITED(DeviceMotionEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeviceMotionEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

void DeviceMotionEvent::InitDeviceMotionEvent(
    const nsAString& aType, bool aCanBubble, bool aCancelable,
    const DeviceAccelerationInit& aAcceleration,
    const DeviceAccelerationInit& aAccelIncludingGravity,
    const DeviceRotationRateInit& aRotationRate,
    const Nullable<double>& aInterval) {
  InitDeviceMotionEvent(aType, aCanBubble, aCancelable, aAcceleration,
                        aAccelIncludingGravity, aRotationRate, aInterval,
                        Nullable<uint64_t>());
}

void DeviceMotionEvent::InitDeviceMotionEvent(
    const nsAString& aType, bool aCanBubble, bool aCancelable,
    const DeviceAccelerationInit& aAcceleration,
    const DeviceAccelerationInit& aAccelIncludingGravity,
    const DeviceRotationRateInit& aRotationRate,
    const Nullable<double>& aInterval, const Nullable<uint64_t>& aTimeStamp) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  Event::InitEvent(aType, aCanBubble, aCancelable);

  mAcceleration = new DeviceAcceleration(this, aAcceleration.mX,
                                         aAcceleration.mY, aAcceleration.mZ);

  mAccelerationIncludingGravity = new DeviceAcceleration(
      this, aAccelIncludingGravity.mX, aAccelIncludingGravity.mY,
      aAccelIncludingGravity.mZ);

  mRotationRate = new DeviceRotationRate(
      this, aRotationRate.mAlpha, aRotationRate.mBeta, aRotationRate.mGamma);
  mInterval = aInterval;
  if (!aTimeStamp.IsNull()) {
    mEvent->mTime = aTimeStamp.Value();

    static mozilla::TimeStamp sInitialNow = mozilla::TimeStamp::Now();
    static uint64_t sInitialEventTime = aTimeStamp.Value();
    mEvent->mTimeStamp =
        sInitialNow + mozilla::TimeDuration::FromMicroseconds(
                          aTimeStamp.Value() - sInitialEventTime);
  }
}

already_AddRefed<DeviceMotionEvent> DeviceMotionEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const DeviceMotionEventInit& aEventInitDict) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<DeviceMotionEvent> e = new DeviceMotionEvent(t, nullptr, nullptr);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  bool trusted = e->Init(t);

  e->mAcceleration = new DeviceAcceleration(e, aEventInitDict.mAcceleration.mX,
                                            aEventInitDict.mAcceleration.mY,
                                            aEventInitDict.mAcceleration.mZ);

  e->mAccelerationIncludingGravity =
      new DeviceAcceleration(e, aEventInitDict.mAccelerationIncludingGravity.mX,
                             aEventInitDict.mAccelerationIncludingGravity.mY,
                             aEventInitDict.mAccelerationIncludingGravity.mZ);

  e->mRotationRate = new DeviceRotationRate(
      e, aEventInitDict.mRotationRate.mAlpha,
      aEventInitDict.mRotationRate.mBeta, aEventInitDict.mRotationRate.mGamma);

  e->mInterval = aEventInitDict.mInterval;
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

/******************************************************************************
 * DeviceAcceleration
 *****************************************************************************/

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DeviceAcceleration, mOwner)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DeviceAcceleration, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DeviceAcceleration, Release)

DeviceAcceleration::DeviceAcceleration(DeviceMotionEvent* aOwner,
                                       const Nullable<double>& aX,
                                       const Nullable<double>& aY,
                                       const Nullable<double>& aZ)
    : mOwner(aOwner), mX(aX), mY(aY), mZ(aZ) {}

DeviceAcceleration::~DeviceAcceleration() {}

/******************************************************************************
 * DeviceRotationRate
 *****************************************************************************/

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DeviceRotationRate, mOwner)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DeviceRotationRate, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DeviceRotationRate, Release)

DeviceRotationRate::DeviceRotationRate(DeviceMotionEvent* aOwner,
                                       const Nullable<double>& aAlpha,
                                       const Nullable<double>& aBeta,
                                       const Nullable<double>& aGamma)
    : mOwner(aOwner), mAlpha(aAlpha), mBeta(aBeta), mGamma(aGamma) {}

DeviceRotationRate::~DeviceRotationRate() {}

}  // namespace dom
}  // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<DeviceMotionEvent> NS_NewDOMDeviceMotionEvent(
    EventTarget* aOwner, nsPresContext* aPresContext, WidgetEvent* aEvent) {
  RefPtr<DeviceMotionEvent> it =
      new DeviceMotionEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
