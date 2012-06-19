/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDeviceMotionEvent_h__
#define nsDOMDeviceMotionEvent_h__

#include "nsIDOMDeviceMotionEvent.h"
#include "nsDOMEvent.h"
#include "mozilla/Attributes.h"

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

  nsDOMDeviceMotionEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent)
  {}

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMDeviceMotionEvent Interface
  NS_DECL_NSIDOMDEVICEMOTIONEVENT

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)

  nsCOMPtr<nsIDOMDeviceAcceleration> mAcceleration;
  nsCOMPtr<nsIDOMDeviceAcceleration> mAccelerationIncludingGravity;
  nsCOMPtr<nsIDOMDeviceRotationRate> mRotationRate;
  double mInterval;
};

#endif
