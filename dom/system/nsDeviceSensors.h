/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceSensors_h
#define nsDeviceSensors_h

#include "nsIDeviceSensors.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "mozilla/dom/DeviceMotionEvent.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/HalSensor.h"
#include "nsDataHashtable.h"

class nsIDOMWindow;

namespace mozilla {
namespace dom {
class EventTarget;
} // namespace dom
} // namespace mozilla

class nsDeviceSensors : public nsIDeviceSensors, public mozilla::hal::ISensorObserver
{
  typedef mozilla::dom::DeviceAccelerationInit DeviceAccelerationInit;
  typedef mozilla::dom::DeviceRotationRateInit DeviceRotationRateInit;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEVICESENSORS

  nsDeviceSensors();

  void Notify(const mozilla::hal::SensorData& aSensorData) override;

private:
  virtual ~nsDeviceSensors();

  // sensor -> window listener
  nsTArray<nsTArray<nsIDOMWindow*>* > mWindowListeners;

  void FireDOMLightEvent(mozilla::dom::EventTarget* aTarget,
                         double value);

  void FireDOMProximityEvent(mozilla::dom::EventTarget* aTarget,
                             double aValue,
                             double aMin,
                             double aMax);

  void FireDOMUserProximityEvent(mozilla::dom::EventTarget* aTarget,
                                 bool aNear);

  void FireDOMOrientationEvent(mozilla::dom::EventTarget* target,
                               double aAlpha,
                               double aBeta,
                               double aGamma,
                               bool aIsAbsolute);

  void FireDOMMotionEvent(class nsIDOMDocument *domDoc,
                          mozilla::dom::EventTarget* target,
                          uint32_t type,
                          PRTime timestamp,
                          double x,
                          double y,
                          double z);

  bool mEnabled;

  inline bool IsSensorEnabled(uint32_t aType) {
    return mWindowListeners[aType]->Length() > 0;
  }

  mozilla::TimeStamp mLastDOMMotionEventTime;
  bool mIsUserProximityNear;
  mozilla::Maybe<DeviceAccelerationInit> mLastAcceleration;
  mozilla::Maybe<DeviceAccelerationInit> mLastAccelerationIncludingGravity;
  mozilla::Maybe<DeviceRotationRateInit> mLastRotationRate;
};

#endif
