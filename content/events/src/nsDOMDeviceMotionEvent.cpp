/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "nsDOMDeviceMotionEvent.h"

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAcceleration)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAccelerationIncludingGravity)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRotationRate)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAcceleration)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAccelerationIncludingGravity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRotationRate)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)

DOMCI_DATA(DeviceMotionEvent, nsDOMDeviceMotionEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDeviceMotionEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDeviceMotionEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceMotionEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceMotionEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMDeviceMotionEvent::InitDeviceMotionEvent(const nsAString & aEventTypeArg,
                                              bool aCanBubbleArg,
                                              bool aCancelableArg,
                                              nsIDOMDeviceAcceleration* aAcceleration,
                                              nsIDOMDeviceAcceleration* aAccelerationIncludingGravity,
                                              nsIDOMDeviceRotationRate* aRotationRate,
                                              double aInterval)
{
  nsresult rv = nsDOMEvent::InitEvent(aEventTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mAcceleration = aAcceleration;
  mAccelerationIncludingGravity = aAccelerationIncludingGravity;
  mRotationRate = aRotationRate;
  mInterval = aInterval;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceMotionEvent::GetAcceleration(nsIDOMDeviceAcceleration **aAcceleration)
{
  NS_ENSURE_ARG_POINTER(aAcceleration);

  NS_IF_ADDREF(*aAcceleration = mAcceleration);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceMotionEvent::GetAccelerationIncludingGravity(nsIDOMDeviceAcceleration **aAccelerationIncludingGravity)
{
  NS_ENSURE_ARG_POINTER(aAccelerationIncludingGravity);

  NS_IF_ADDREF(*aAccelerationIncludingGravity = mAccelerationIncludingGravity);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceMotionEvent::GetRotationRate(nsIDOMDeviceRotationRate **aRotationRate)
{
  NS_ENSURE_ARG_POINTER(aRotationRate);

  NS_IF_ADDREF(*aRotationRate = mRotationRate);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceMotionEvent::GetInterval(double *aInterval)
{
  NS_ENSURE_ARG_POINTER(aInterval);

  *aInterval = mInterval;
  return NS_OK;
}


nsresult
NS_NewDOMDeviceMotionEvent(nsIDOMEvent** aInstancePtrResult,
                           nsPresContext* aPresContext,
                           nsEvent *aEvent) 
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsDOMDeviceMotionEvent* it = new nsDOMDeviceMotionEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}


DOMCI_DATA(DeviceAcceleration, nsDOMDeviceAcceleration)

NS_INTERFACE_MAP_BEGIN(nsDOMDeviceAcceleration)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDeviceAcceleration)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceAcceleration)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceAcceleration)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMDeviceAcceleration)
NS_IMPL_RELEASE(nsDOMDeviceAcceleration)

nsDOMDeviceAcceleration::nsDOMDeviceAcceleration(double aX, double aY, double aZ)
: mX(aX), mY(aY), mZ(aZ)
{
}

nsDOMDeviceAcceleration::~nsDOMDeviceAcceleration()
{
}

NS_IMETHODIMP
nsDOMDeviceAcceleration::GetX(double *aX)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = mX;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceAcceleration::GetY(double *aY)
{
  NS_ENSURE_ARG_POINTER(aY);
  *aY = mY;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceAcceleration::GetZ(double *aZ)
{
  NS_ENSURE_ARG_POINTER(aZ);
  *aZ = mZ;
  return NS_OK;
}


DOMCI_DATA(DeviceRotationRate, nsDOMDeviceRotationRate)

NS_INTERFACE_MAP_BEGIN(nsDOMDeviceRotationRate)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDeviceRotationRate)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceRotationRate)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceRotationRate)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMDeviceRotationRate)
NS_IMPL_RELEASE(nsDOMDeviceRotationRate)

nsDOMDeviceRotationRate::nsDOMDeviceRotationRate(double aAlpha, double aBeta, double aGamma)
: mAlpha(aAlpha), mBeta(aBeta), mGamma(aGamma)
{
}

nsDOMDeviceRotationRate::~nsDOMDeviceRotationRate()
{
}

NS_IMETHODIMP
nsDOMDeviceRotationRate::GetAlpha(double *aAlpha)
{
  NS_ENSURE_ARG_POINTER(aAlpha);
  *aAlpha = mAlpha;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceRotationRate::GetBeta(double *aBeta)
{
  NS_ENSURE_ARG_POINTER(aBeta);
  *aBeta = mBeta;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceRotationRate::GetGamma(double *aGamma)
{
  NS_ENSURE_ARG_POINTER(aGamma);
  *aGamma = mGamma;
  return NS_OK;
}
