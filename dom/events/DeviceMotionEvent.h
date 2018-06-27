/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

class DeviceRotationRate final : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DeviceRotationRate)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DeviceRotationRate)

  DeviceRotationRate(DeviceMotionEvent* aOwner,
                     const Nullable<double>& aAlpha,
                     const Nullable<double>& aBeta,
                     const Nullable<double>& aGamma);

  DeviceMotionEvent* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return DeviceRotationRate_Binding::Wrap(aCx, this, aGivenProto);
  }

  Nullable<double> GetAlpha() const { return mAlpha; }
  Nullable<double> GetBeta() const { return mBeta; }
  Nullable<double> GetGamma() const { return mGamma; }

private:
  ~DeviceRotationRate();

protected:
  RefPtr<DeviceMotionEvent> mOwner;
  Nullable<double> mAlpha, mBeta, mGamma;
};

class DeviceAcceleration final : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DeviceAcceleration)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DeviceAcceleration)

  DeviceAcceleration(DeviceMotionEvent* aOwner,
                     const Nullable<double>& aX,
                     const Nullable<double>& aY,
                     const Nullable<double>& aZ);

  DeviceMotionEvent* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return DeviceAcceleration_Binding::Wrap(aCx, this, aGivenProto);
  }

  Nullable<double> GetX() const { return mX; }
  Nullable<double> GetY() const { return mY; }
  Nullable<double> GetZ() const { return mZ; }

private:
  ~DeviceAcceleration();

protected:
  RefPtr<DeviceMotionEvent> mOwner;
  Nullable<double> mX, mY, mZ;
};

class DeviceMotionEvent final : public Event
{
public:

  DeviceMotionEvent(EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    WidgetEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeviceMotionEvent, Event)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return DeviceMotionEvent_Binding::Wrap(aCx, this, aGivenProto);
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
         const Nullable<double>& aInterval);

  void InitDeviceMotionEvent(
         const nsAString& aType,
         bool aCanBubble,
         bool aCancelable,
         const DeviceAccelerationInit& aAcceleration,
         const DeviceAccelerationInit& aAccelerationIncludingGravity,
         const DeviceRotationRateInit& aRotationRate,
         const Nullable<double>& aInterval,
         const Nullable<uint64_t>& aTimeStamp);

  static already_AddRefed<DeviceMotionEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const DeviceMotionEventInit& aEventInitDict,
              ErrorResult& aRv);

protected:
  ~DeviceMotionEvent() {}

  RefPtr<DeviceAcceleration> mAcceleration;
  RefPtr<DeviceAcceleration> mAccelerationIncludingGravity;
  RefPtr<DeviceRotationRate> mRotationRate;
  Nullable<double> mInterval;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::DeviceMotionEvent>
NS_NewDOMDeviceMotionEvent(mozilla::dom::EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           mozilla::WidgetEvent* aEvent);

#endif // mozilla_dom_DeviceMotionEvent_h_
