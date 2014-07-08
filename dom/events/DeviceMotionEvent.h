/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DeviceMotionEvent_h_
#define mozilla_dom_DeviceMotionEvent_h_

#include "mozilla/Attributes.h"
#include "mozilla/dom/DeviceMotionEventBinding.h"
#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {

class DeviceRotationRate MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DeviceRotationRate)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DeviceRotationRate)

  DeviceRotationRate(DeviceMotionEvent* aOwner,
                     Nullable<double> aAlpha, Nullable<double> aBeta,
                     Nullable<double> aGamma);

  DeviceMotionEvent* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return DeviceRotationRateBinding::Wrap(aCx, this);
  }

  Nullable<double> GetAlpha() const { return mAlpha; }
  Nullable<double> GetBeta() const { return mBeta; }
  Nullable<double> GetGamma() const { return mGamma; }

private:
  ~DeviceRotationRate();

protected:
  nsRefPtr<DeviceMotionEvent> mOwner;
  Nullable<double> mAlpha, mBeta, mGamma;
};

class DeviceAcceleration MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DeviceAcceleration)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DeviceAcceleration)

  DeviceAcceleration(DeviceMotionEvent* aOwner,
                     Nullable<double> aX, Nullable<double> aY,
                     Nullable<double> aZ);

  DeviceMotionEvent* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return DeviceAccelerationBinding::Wrap(aCx, this);
  }

  Nullable<double> GetX() const { return mX; }
  Nullable<double> GetY() const { return mY; }
  Nullable<double> GetZ() const { return mZ; }

private:
  ~DeviceAcceleration();

protected:
  nsRefPtr<DeviceMotionEvent> mOwner;
  Nullable<double> mX, mY, mZ;
};

class DeviceMotionEvent MOZ_FINAL : public Event
{
public:

  DeviceMotionEvent(EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    WidgetEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to Event
  NS_FORWARD_TO_EVENT

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeviceMotionEvent, Event)

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return DeviceMotionEventBinding::Wrap(aCx, this);
  }

  DeviceAcceleration* GetAcceleration() const
  {
    return mAcceleration;
  }

  DeviceAcceleration* GetAccelerationIncludingGravity() const
  {
    return mAccelerationIncludingGravity;
  }

  DeviceRotationRate* GetRotationRate() const
  {
    return mRotationRate;
  }

  Nullable<double> GetInterval() const
  {
    return mInterval;
  }

  void InitDeviceMotionEvent(
         const nsAString& aType,
         bool aCanBubble,
         bool aCancelable,
         const DeviceAccelerationInit& aAcceleration,
         const DeviceAccelerationInit& aAccelerationIncludingGravity,
         const DeviceRotationRateInit& aRotationRate,
         Nullable<double> aInterval,
         ErrorResult& aRv);

  static already_AddRefed<DeviceMotionEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const DeviceMotionEventInit& aEventInitDict,
              ErrorResult& aRv);

protected:
  ~DeviceMotionEvent() {}

  nsRefPtr<DeviceAcceleration> mAcceleration;
  nsRefPtr<DeviceAcceleration> mAccelerationIncludingGravity;
  nsRefPtr<DeviceRotationRate> mRotationRate;
  Nullable<double> mInterval;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DeviceMotionEvent_h_
