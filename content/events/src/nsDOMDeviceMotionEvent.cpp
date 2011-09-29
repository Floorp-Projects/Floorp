/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Device Motion System.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@dougt.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMDeviceMotionEvent.h"
#include "nsContentUtils.h"



NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMDeviceMotionEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mAcceleration)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mAccelerationIncludingGravity)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRotationRate)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAcceleration)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAccelerationIncludingGravity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRotationRate)
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
