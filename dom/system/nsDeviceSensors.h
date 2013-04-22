/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceSensors_h
#define nsDeviceSensors_h

#include "nsIDeviceSensors.h"
#include "nsIDOMDeviceMotionEvent.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsIDOMDeviceLightEvent.h"
#include "nsIDOMDeviceOrientationEvent.h"
#include "nsIDOMDeviceProximityEvent.h"
#include "nsIDOMUserProximityEvent.h"
#include "nsIDOMDeviceMotionEvent.h"
#include "nsDOMDeviceMotionEvent.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/HalSensor.h"
#include "nsDataHashtable.h"

#define NS_DEVICE_SENSORS_CID \
{ 0xecba5203, 0x77da, 0x465a, \
{ 0x86, 0x5e, 0x78, 0xb7, 0xaf, 0x10, 0xd8, 0xf7 } }

#define NS_DEVICE_SENSORS_CONTRACTID "@mozilla.org/devicesensors;1"

class nsIDOMWindow;

namespace mozilla {
namespace dom {
class EventTarget;
}
}

class nsDeviceSensors : public nsIDeviceSensors, public mozilla::hal::ISensorObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEVICESENSORS

  nsDeviceSensors();

  virtual ~nsDeviceSensors();

  void Notify(const mozilla::hal::SensorData& aSensorData);

private:
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

  void FireDOMOrientationEvent(class nsIDOMDocument *domDoc,
                               mozilla::dom::EventTarget* target,
                               double alpha,
                               double beta,
                               double gamma);

  void FireDOMMotionEvent(class nsIDOMDocument *domDoc,
                          mozilla::dom::EventTarget* target,
                          uint32_t type,
                          double x,
                          double y,
                          double z);

  bool mEnabled;

  inline bool IsSensorEnabled(uint32_t aType) {
    return mWindowListeners[aType]->Length() > 0;
  }

  mozilla::TimeStamp mLastDOMMotionEventTime;
  bool mIsUserProximityNear;
  nsRefPtr<nsDOMDeviceAcceleration> mLastAcceleration;
  nsRefPtr<nsDOMDeviceAcceleration> mLastAccelerationIncluduingGravity;
  nsRefPtr<nsDOMDeviceRotationRate> mLastRotationRate;
};

#endif
