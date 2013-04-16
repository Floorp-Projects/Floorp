/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDeviceMotionEvent_h__
#define nsDOMDeviceMotionEvent_h__

#include "nsIDOMDeviceMotionEvent.h"
#include "nsDOMEvent.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/DeviceMotionEventBinding.h"

class nsDOMDeviceRotationRate MOZ_FINAL : public nsIDOMDeviceRotationRate
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDEVICEROTATIONRATE

  nsDOMDeviceRotationRate(double aAlpha, double aBeta, double aGamma);

private:
  ~nsDOMDeviceRotationRate();

protected:
  double mAlpha, mBeta, mGamma;
};

class nsDOMDeviceAcceleration MOZ_FINAL : public nsIDOMDeviceAcceleration
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDEVICEACCELERATION

  nsDOMDeviceAcceleration(double aX, double aY, double aZ);

private:
  ~nsDOMDeviceAcceleration();

protected:
  double mX, mY, mZ;
};

class nsDOMDeviceMotionEvent MOZ_FINAL : public nsDOMEvent,
                                         public nsIDOMDeviceMotionEvent
{
public:

  nsDOMDeviceMotionEvent(mozilla::dom::EventTarget* aOwner,
                         nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent)
  {
    SetIsDOMBinding();
  }

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMDeviceMotionEvent Interface
  NS_DECL_NSIDOMDEVICEMOTIONEVENT

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope)
  {
    return mozilla::dom::DeviceMotionEventBinding::Wrap(aCx, aScope, this);
  }

  nsIDOMDeviceAcceleration* GetAcceleration()
  {
    return mAcceleration;
  }

  nsIDOMDeviceAcceleration* GetAccelerationIncludingGravity()
  {
    return mAccelerationIncludingGravity;
  }

  nsIDOMDeviceRotationRate* GetRotationRate()
  {
    return mRotationRate;
  }

  double Interval() const
  {
    return mInterval;
  }

  void InitDeviceMotionEvent(const nsAString& aType,
                             bool aCanBubble,
                             bool aCancelable,
                             nsIDOMDeviceAcceleration* aAcceleration,
                             nsIDOMDeviceAcceleration* aAccelerationIncludingGravity,
                             nsIDOMDeviceRotationRate* aRotationRate,
                             double aInterval,
                             mozilla::ErrorResult& aRv);

  nsCOMPtr<nsIDOMDeviceAcceleration> mAcceleration;
  nsCOMPtr<nsIDOMDeviceAcceleration> mAccelerationIncludingGravity;
  nsCOMPtr<nsIDOMDeviceRotationRate> mRotationRate;
  double mInterval;
};

#endif
