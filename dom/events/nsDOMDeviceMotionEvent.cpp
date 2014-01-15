/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMDeviceMotionEvent.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED_3(nsDOMDeviceMotionEvent, nsDOMEvent,
                                     mAcceleration,
                                     mAccelerationIncludingGravity,
                                     mRotationRate)

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDeviceMotionEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

void
nsDOMDeviceMotionEvent::InitDeviceMotionEvent(const nsAString& aType,
                                              bool aCanBubble,
                                              bool aCancelable,
                                              const DeviceAccelerationInit& aAcceleration,
                                              const DeviceAccelerationInit& aAccelerationIncludingGravity,
                                              const DeviceRotationRateInit& aRotationRate,
                                              Nullable<double> aInterval,
                                              ErrorResult& aRv)
{
  aRv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  if (aRv.Failed()) {
    return;
  }

  mAcceleration = new nsDOMDeviceAcceleration(this, aAcceleration.mX,
                                              aAcceleration.mY,
                                              aAcceleration.mZ);

  mAccelerationIncludingGravity =
    new nsDOMDeviceAcceleration(this, aAccelerationIncludingGravity.mX,
                                aAccelerationIncludingGravity.mY,
                                aAccelerationIncludingGravity.mZ);

  mRotationRate = new nsDOMDeviceRotationRate(this, aRotationRate.mAlpha,
                                              aRotationRate.mBeta,
                                              aRotationRate.mGamma);
  mInterval = aInterval;
}

already_AddRefed<nsDOMDeviceMotionEvent>
nsDOMDeviceMotionEvent::Constructor(const GlobalObject& aGlobal,
                                    const nsAString& aType,
                                    const DeviceMotionEventInit& aEventInitDict,
                                    ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t =
    do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<nsDOMDeviceMotionEvent> e =
    new nsDOMDeviceMotionEvent(t, nullptr, nullptr);
  aRv = e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  if (aRv.Failed()) {
    return nullptr;
  }
  bool trusted = e->Init(t);

  e->mAcceleration = new nsDOMDeviceAcceleration(e,
    aEventInitDict.mAcceleration.mX,
    aEventInitDict.mAcceleration.mY,
    aEventInitDict.mAcceleration.mZ);

  e->mAccelerationIncludingGravity = new nsDOMDeviceAcceleration(e,
    aEventInitDict.mAccelerationIncludingGravity.mX,
    aEventInitDict.mAccelerationIncludingGravity.mY,
    aEventInitDict.mAccelerationIncludingGravity.mZ);

  e->mRotationRate = new nsDOMDeviceRotationRate(e,
    aEventInitDict.mRotationRate.mAlpha,
    aEventInitDict.mRotationRate.mBeta,
    aEventInitDict.mRotationRate.mGamma);

  e->mInterval = aEventInitDict.mInterval;
  e->SetTrusted(trusted);

  return e.forget();
}


nsresult
NS_NewDOMDeviceMotionEvent(nsIDOMEvent** aInstancePtrResult,
                           mozilla::dom::EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           WidgetEvent* aEvent) 
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsDOMDeviceMotionEvent* it =
    new nsDOMDeviceMotionEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsDOMDeviceAcceleration, mOwner)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsDOMDeviceAcceleration, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsDOMDeviceAcceleration, Release)

nsDOMDeviceAcceleration::nsDOMDeviceAcceleration(nsDOMDeviceMotionEvent* aOwner,
                                                 Nullable<double> aX,
                                                 Nullable<double> aY,
                                                 Nullable<double> aZ)
: mOwner(aOwner), mX(aX), mY(aY), mZ(aZ)
{
  SetIsDOMBinding();
}

nsDOMDeviceAcceleration::~nsDOMDeviceAcceleration()
{
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsDOMDeviceRotationRate, mOwner)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsDOMDeviceRotationRate, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsDOMDeviceRotationRate, Release)

nsDOMDeviceRotationRate::nsDOMDeviceRotationRate(nsDOMDeviceMotionEvent* aOwner,
                                                 Nullable<double> aAlpha,
                                                 Nullable<double> aBeta,
                                                 Nullable<double> aGamma)
: mOwner(aOwner), mAlpha(aAlpha), mBeta(aBeta), mGamma(aGamma)
{
  SetIsDOMBinding();
}

nsDOMDeviceRotationRate::~nsDOMDeviceRotationRate()
{
}
