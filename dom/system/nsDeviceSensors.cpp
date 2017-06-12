/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/HalSensor.h"

#include "nsDeviceSensors.h"

#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIServiceManager.h"
#include "nsIServiceManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"
#include "mozilla/Services.h"
#include "nsIPermissionManager.h"
#include "mozilla/dom/DeviceLightEvent.h"
#include "mozilla/dom/DeviceOrientationEvent.h"
#include "mozilla/dom/DeviceProximityEvent.h"
#include "mozilla/dom/UserProximityEvent.h"

#include <cmath>

using namespace mozilla;
using namespace mozilla::dom;
using namespace hal;

#undef near

#define DEFAULT_SENSOR_POLL 100

static const nsTArray<nsIDOMWindow*>::index_type NoIndex =
  nsTArray<nsIDOMWindow*>::NoIndex;

class nsDeviceSensorData final : public nsIDeviceSensorData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEVICESENSORDATA

  nsDeviceSensorData(unsigned long type, double x, double y, double z);

private:
  ~nsDeviceSensorData();

protected:
  unsigned long mType;
  double mX, mY, mZ;
};

nsDeviceSensorData::nsDeviceSensorData(unsigned long type, double x, double y, double z)
  : mType(type), mX(x), mY(y), mZ(z)
{
}

NS_INTERFACE_MAP_BEGIN(nsDeviceSensorData)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDeviceSensorData)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDeviceSensorData)
NS_IMPL_RELEASE(nsDeviceSensorData)

nsDeviceSensorData::~nsDeviceSensorData()
{
}

NS_IMETHODIMP nsDeviceSensorData::GetType(uint32_t *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceSensorData::GetX(double *aX)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = mX;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceSensorData::GetY(double *aY)
{
  NS_ENSURE_ARG_POINTER(aY);
  *aY = mY;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceSensorData::GetZ(double *aZ)
{
  NS_ENSURE_ARG_POINTER(aZ);
  *aZ = mZ;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsDeviceSensors, nsIDeviceSensors)

nsDeviceSensors::nsDeviceSensors()
{
  mIsUserProximityNear = false;
  mLastDOMMotionEventTime = TimeStamp::Now();
  mEnabled = Preferences::GetBool("device.sensors.enabled", true);

  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    nsTArray<nsIDOMWindow*> *windows = new nsTArray<nsIDOMWindow*>();
    mWindowListeners.AppendElement(windows);
  }

  mLastDOMMotionEventTime = TimeStamp::Now();
}

nsDeviceSensors::~nsDeviceSensors()
{
  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    if (IsSensorEnabled(i))
      UnregisterSensorObserver((SensorType)i, this);
  }

  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    delete mWindowListeners[i];
  }
}

NS_IMETHODIMP nsDeviceSensors::HasWindowListener(uint32_t aType, nsIDOMWindow *aWindow, bool *aRetVal)
{
  if (!mEnabled)
    *aRetVal = false;
  else
    *aRetVal = mWindowListeners[aType]->IndexOf(aWindow) != NoIndex;

  return NS_OK;
}

class DeviceSensorTestEvent : public Runnable
{
public:
  DeviceSensorTestEvent(nsDeviceSensors* aTarget, uint32_t aType)
    : mozilla::Runnable("DeviceSensorTestEvent")
    , mTarget(aTarget)
    , mType(aType)
  {
  }

  NS_IMETHOD Run() override
  {
    SensorData sensorData;
    sensorData.sensor() = static_cast<SensorType>(mType);
    sensorData.timestamp() = PR_Now();
    sensorData.values().AppendElement(0.5f);
    sensorData.values().AppendElement(0.5f);
    sensorData.values().AppendElement(0.5f);
    sensorData.values().AppendElement(0.5f);
    sensorData.accuracy() = SENSOR_ACCURACY_UNRELIABLE;
    mTarget->Notify(sensorData);
    return NS_OK;
  }

private:
  RefPtr<nsDeviceSensors> mTarget;
  uint32_t mType;
};

static bool sTestSensorEvents = false;

NS_IMETHODIMP nsDeviceSensors::AddWindowListener(uint32_t aType, nsIDOMWindow *aWindow)
{
  if (!mEnabled)
    return NS_OK;

  if (mWindowListeners[aType]->IndexOf(aWindow) != NoIndex)
    return NS_OK;

  if (!IsSensorEnabled(aType)) {
    RegisterSensorObserver((SensorType)aType, this);
  }

  mWindowListeners[aType]->AppendElement(aWindow);

  static bool sPrefCacheInitialized = false;
  if (!sPrefCacheInitialized) {
    sPrefCacheInitialized = true;
    Preferences::AddBoolVarCache(&sTestSensorEvents,
                                 "device.sensors.test.events",
                                 false);
  }

  if (sTestSensorEvents) {
    nsCOMPtr<nsIRunnable> event = new DeviceSensorTestEvent(this, aType);
    NS_DispatchToCurrentThread(event);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceSensors::RemoveWindowListener(uint32_t aType, nsIDOMWindow *aWindow)
{
  if (mWindowListeners[aType]->IndexOf(aWindow) == NoIndex)
    return NS_OK;

  mWindowListeners[aType]->RemoveElement(aWindow);

  if (mWindowListeners[aType]->Length() == 0)
    UnregisterSensorObserver((SensorType)aType, this);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceSensors::RemoveWindowAsListener(nsIDOMWindow *aWindow)
{
  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    RemoveWindowListener((SensorType)i, aWindow);
  }
  return NS_OK;
}

static bool
WindowCannotReceiveSensorEvent (nsPIDOMWindowInner* aWindow)
{
  // Check to see if this window is in the background.  If
  // it is and it does not have the "background-sensors" permission,
  // don't send any device motion events to it.
  if (!aWindow || !aWindow->IsCurrentInnerWindow()) {
    return true;
  }

  bool disabled = aWindow->GetOuterWindow()->IsBackground() ||
                  !aWindow->IsTopLevelWindowActive();
  if (!disabled) {
    nsCOMPtr<nsPIDOMWindowOuter> top = aWindow->GetScriptableTop();
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
    nsCOMPtr<nsIScriptObjectPrincipal> topSop = do_QueryInterface(top);
    if (!sop || !topSop) {
      return true;
    }

    nsIPrincipal* principal = sop->GetPrincipal();
    nsIPrincipal* topPrincipal = topSop->GetPrincipal();
    if (!principal || !topPrincipal) {
      return true;
    }

    disabled = !principal->Subsumes(topPrincipal);
  }

  if (disabled) {
    nsCOMPtr<nsIPermissionManager> permMgr =
      services::GetPermissionManager();
    NS_ENSURE_TRUE(permMgr, true);
    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    permMgr->TestPermissionFromWindow(aWindow, "background-sensors", &permission);
    return permission != nsIPermissionManager::ALLOW_ACTION;
  }

  return false;
}

// Holds the device orientation in Euler angle degrees (azimuth, pitch, roll).
struct Orientation
{
  enum OrientationReference
  {
    kRelative = 0,
    kAbsolute
  };

  static Orientation RadToDeg(const Orientation& aOrient)
  {
    const static double kRadToDeg = 180.0 / M_PI;
    return { aOrient.alpha * kRadToDeg,
             aOrient.beta * kRadToDeg,
             aOrient.gamma * kRadToDeg };
  }

  double alpha;
  double beta;
  double gamma;
};

static Orientation
RotationVectorToOrientation(double aX, double aY, double aZ, double aW)
{
  static const double kFuzzyOne = 1.0 - 1e-6;
  static const double kCircleRad = 2.0 * M_PI;

  Orientation orient = { 2.0 * std::atan2(aY, aW),
                         M_PI_2,
                         0.0 };

  const double sqX = aX * aX;
  const double sqY = aY * aY;
  const double sqZ = aZ * aZ;
  const double sqW = aW * aW;
  const double unitLength = sqX + sqY + sqZ + sqW;
  const double xwyz = 2.0 * (aX * aW + aY * aZ) / unitLength;

  if (xwyz < -kFuzzyOne) {
    orient.alpha *= -1.0;
    orient.beta *= -1.0;
  } else if (xwyz <= kFuzzyOne) {
    const double gammaX = -sqX - sqY + sqZ + sqW;
    const double gammaY = 2.0 * (aY * aW - aX * aZ);
    const double alphaX = -sqX + sqY - sqZ + sqW;
    const double alphaY = 2.0 * (aZ * aW - aX * aY);
    const double fac = gammaX > 0 ? 1.0 : -1.0;

    orient.alpha = std::fmod(kCircleRad + std::atan2(fac * alphaY, fac * alphaX),
                             kCircleRad);
    orient.beta = fac * std::asin(xwyz);
    orient.gamma = std::atan2(fac * gammaY, fac * gammaX);
    if (fac < 0.0) {
      orient.beta = fmod(M_PI + orient.beta, M_PI);
    }
  }

  return Orientation::RadToDeg(orient);
}

void
nsDeviceSensors::Notify(const mozilla::hal::SensorData& aSensorData)
{
  uint32_t type = aSensorData.sensor();

  const InfallibleTArray<float>& values = aSensorData.values();
  size_t len = values.Length();
  double x = len > 0 ? values[0] : 0.0;
  double y = len > 1 ? values[1] : 0.0;
  double z = len > 2 ? values[2] : 0.0;
  double w = len > 3 ? values[3] : 0.0;
  PRTime timestamp = aSensorData.timestamp();

  nsCOMArray<nsIDOMWindow> windowListeners;
  for (uint32_t i = 0; i < mWindowListeners[type]->Length(); i++) {
    windowListeners.AppendObject(mWindowListeners[type]->SafeElementAt(i));
  }

  for (uint32_t i = windowListeners.Count(); i > 0 ; ) {
    --i;

    nsCOMPtr<nsPIDOMWindowInner> pwindow = do_QueryInterface(windowListeners[i]);
    if (WindowCannotReceiveSensorEvent(pwindow)) {
        continue;
    }

    if (nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(pwindow->GetDoc())) {
      nsCOMPtr<mozilla::dom::EventTarget> target = do_QueryInterface(windowListeners[i]);
      if (type == nsIDeviceSensorData::TYPE_ACCELERATION ||
        type == nsIDeviceSensorData::TYPE_LINEAR_ACCELERATION ||
        type == nsIDeviceSensorData::TYPE_GYROSCOPE) {
        FireDOMMotionEvent(domDoc, target, type, timestamp, x, y, z);
      } else if (type == nsIDeviceSensorData::TYPE_ORIENTATION) {
        FireDOMOrientationEvent(target, x, y, z, Orientation::kAbsolute);
      } else if (type == nsIDeviceSensorData::TYPE_ROTATION_VECTOR) {
        const Orientation orient = RotationVectorToOrientation(x, y, z, w);
        FireDOMOrientationEvent(target, orient.alpha, orient.beta, orient.gamma,
                                Orientation::kAbsolute);
      } else if (type == nsIDeviceSensorData::TYPE_GAME_ROTATION_VECTOR) {
        const Orientation orient = RotationVectorToOrientation(x, y, z, w);
        FireDOMOrientationEvent(target, orient.alpha, orient.beta, orient.gamma,
                                Orientation::kRelative);
      } else if (type == nsIDeviceSensorData::TYPE_PROXIMITY) {
        FireDOMProximityEvent(target, x, y, z);
      } else if (type == nsIDeviceSensorData::TYPE_LIGHT) {
        FireDOMLightEvent(target, x);
      }
    }
  }
}

void
nsDeviceSensors::FireDOMLightEvent(mozilla::dom::EventTarget* aTarget,
                                   double aValue)
{
  DeviceLightEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mValue = round(aValue);
  RefPtr<DeviceLightEvent> event =
    DeviceLightEvent::Constructor(aTarget, NS_LITERAL_STRING("devicelight"), init);

  event->SetTrusted(true);

  bool defaultActionEnabled;
  aTarget->DispatchEvent(event, &defaultActionEnabled);
}

void
nsDeviceSensors::FireDOMProximityEvent(mozilla::dom::EventTarget* aTarget,
                                       double aValue,
                                       double aMin,
                                       double aMax)
{
  DeviceProximityEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mValue = aValue;
  init.mMin = aMin;
  init.mMax = aMax;
  RefPtr<DeviceProximityEvent> event =
    DeviceProximityEvent::Constructor(aTarget,
                                      NS_LITERAL_STRING("deviceproximity"),
                                      init);
  event->SetTrusted(true);

  bool defaultActionEnabled;
  aTarget->DispatchEvent(event, &defaultActionEnabled);

  // Some proximity sensors only support a binary near or
  // far measurement. In this case, the sensor should report
  // its maximum range value in the far state and a lesser
  // value in the near state.

  bool near = (aValue < aMax);
  if (mIsUserProximityNear != near) {
    mIsUserProximityNear = near;
    FireDOMUserProximityEvent(aTarget, mIsUserProximityNear);
  }
}

void
nsDeviceSensors::FireDOMUserProximityEvent(mozilla::dom::EventTarget* aTarget,
                                           bool aNear)
{
  UserProximityEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mNear = aNear;
  RefPtr<UserProximityEvent> event =
    UserProximityEvent::Constructor(aTarget,
                                    NS_LITERAL_STRING("userproximity"),
                                    init);

  event->SetTrusted(true);

  bool defaultActionEnabled;
  aTarget->DispatchEvent(event, &defaultActionEnabled);
}

void
nsDeviceSensors::FireDOMOrientationEvent(EventTarget* aTarget,
                                         double aAlpha,
                                         double aBeta,
                                         double aGamma,
                                         bool aIsAbsolute)
{
  DeviceOrientationEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mAlpha.SetValue(aAlpha);
  init.mBeta.SetValue(aBeta);
  init.mGamma.SetValue(aGamma);
  init.mAbsolute = aIsAbsolute;

  auto Dispatch = [&](EventTarget* aEventTarget, const nsAString& aType)
  {
    RefPtr<DeviceOrientationEvent> event =
      DeviceOrientationEvent::Constructor(aEventTarget, aType, init);
    event->SetTrusted(true);
    bool dummy;
    aEventTarget->DispatchEvent(event, &dummy);
  };

  Dispatch(aTarget, aIsAbsolute ? NS_LITERAL_STRING("absolutedeviceorientation") :
                                  NS_LITERAL_STRING("deviceorientation"));

  // This is used to determine whether relative events have been dispatched
  // during the current session, in which case we don't dispatch the additional
  // compatibility events.
  static bool sIsDispatchingRelativeEvents = false;
  sIsDispatchingRelativeEvents = sIsDispatchingRelativeEvents || !aIsAbsolute;

  // Android devices with SENSOR_GAME_ROTATION_VECTOR support dispatch
  // relative events for "deviceorientation" by default, while other platforms
  // and devices without such support dispatch absolute events by default.
  if (aIsAbsolute && !sIsDispatchingRelativeEvents) {
    // For absolute events on devices without support for relative events,
    // we need to additionally dispatch type "deviceorientation" to keep
    // backwards-compatibility.
    Dispatch(aTarget, NS_LITERAL_STRING("deviceorientation"));
  }
}

void
nsDeviceSensors::FireDOMMotionEvent(nsIDOMDocument *domdoc,
                                    EventTarget* target,
                                    uint32_t type,
                                    PRTime timestamp,
                                    double x,
                                    double y,
                                    double z)
{
  // Attempt to coalesce events
  TimeDuration sensorPollDuration =
    TimeDuration::FromMilliseconds(DEFAULT_SENSOR_POLL);
  bool fireEvent =
    (TimeStamp::Now() > mLastDOMMotionEventTime + sensorPollDuration) ||
    sTestSensorEvents;

  switch (type) {
  case nsIDeviceSensorData::TYPE_LINEAR_ACCELERATION:
    if (!mLastAcceleration) {
      mLastAcceleration.emplace();
    }
    mLastAcceleration->mX.SetValue(x);
    mLastAcceleration->mY.SetValue(y);
    mLastAcceleration->mZ.SetValue(z);
    break;
  case nsIDeviceSensorData::TYPE_ACCELERATION:
    if (!mLastAccelerationIncludingGravity) {
      mLastAccelerationIncludingGravity.emplace();
    }
    mLastAccelerationIncludingGravity->mX.SetValue(x);
    mLastAccelerationIncludingGravity->mY.SetValue(y);
    mLastAccelerationIncludingGravity->mZ.SetValue(z);
    break;
  case nsIDeviceSensorData::TYPE_GYROSCOPE:
    if (!mLastRotationRate) {
      mLastRotationRate.emplace();
    }
    mLastRotationRate->mAlpha.SetValue(x);
    mLastRotationRate->mBeta.SetValue(y);
    mLastRotationRate->mGamma.SetValue(z);
    break;
  }

  if (fireEvent) {
    if (!mLastAcceleration) {
      mLastAcceleration.emplace();
    }
    if (!mLastAccelerationIncludingGravity) {
      mLastAccelerationIncludingGravity.emplace();
    }
    if (!mLastRotationRate) {
      mLastRotationRate.emplace();
    }
  } else if (!mLastAcceleration ||
             !mLastAccelerationIncludingGravity ||
             !mLastRotationRate) {
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  domdoc->CreateEvent(NS_LITERAL_STRING("DeviceMotionEvent"), getter_AddRefs(event));

  DeviceMotionEvent* me = static_cast<DeviceMotionEvent*>(event.get());

  me->InitDeviceMotionEvent(NS_LITERAL_STRING("devicemotion"),
                            true,
                            false,
                            *mLastAcceleration,
                            *mLastAccelerationIncludingGravity,
                            *mLastRotationRate,
                            Nullable<double>(DEFAULT_SENSOR_POLL),
                            Nullable<uint64_t>(timestamp));

  event->SetTrusted(true);

  bool defaultActionEnabled = true;
  target->DispatchEvent(event, &defaultActionEnabled);

  mLastRotationRate.reset();
  mLastAccelerationIncludingGravity.reset();
  mLastAcceleration.reset();
  mLastDOMMotionEventTime = TimeStamp::Now();
}
