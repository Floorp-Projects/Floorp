/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDeviceMotionEvent_h__
#define nsDOMDeviceMotionEvent_h__

#include "nsDOMEvent.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/DeviceMotionEventBinding.h"

class nsDOMDeviceRotationRate MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsDOMDeviceRotationRate)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsDOMDeviceRotationRate)

  nsDOMDeviceRotationRate(nsDOMDeviceMotionEvent* aOwner,
                          Nullable<double> aAlpha, Nullable<double> aBeta,
                          Nullable<double> aGamma);
  nsDOMDeviceRotationRate(double aAlpha, double aBeta, double aGamma)
  {
    nsDOMDeviceRotationRate(nullptr, Nullable<double>(aAlpha),
                            Nullable<double>(aBeta), Nullable<double>(aGamma));
  }

  nsDOMDeviceMotionEvent* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::DeviceRotationRateBinding::Wrap(aCx, aScope, this);
  }

  Nullable<double> GetAlpha() const { return mAlpha; }
  Nullable<double> GetBeta() const { return mBeta; }
  Nullable<double> GetGamma() const { return mGamma; }

private:
  ~nsDOMDeviceRotationRate();

protected:
  nsRefPtr<nsDOMDeviceMotionEvent> mOwner;
  Nullable<double> mAlpha, mBeta, mGamma;
};

class nsDOMDeviceAcceleration MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsDOMDeviceAcceleration)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsDOMDeviceAcceleration)

  nsDOMDeviceAcceleration(nsDOMDeviceMotionEvent* aOwner,
                          Nullable<double> aX, Nullable<double> aY,
                          Nullable<double> aZ);
  nsDOMDeviceAcceleration(double aX, double aY, double aZ)
  {
    nsDOMDeviceAcceleration(nullptr, Nullable<double>(aX),
                            Nullable<double>(aY), Nullable<double>(aZ));
  }

  nsDOMDeviceMotionEvent* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::DeviceAccelerationBinding::Wrap(aCx, aScope, this);
  }

  Nullable<double> GetX() const { return mX; }
  Nullable<double> GetY() const { return mY; }
  Nullable<double> GetZ() const { return mZ; }

private:
  ~nsDOMDeviceAcceleration();

protected:
  nsRefPtr<nsDOMDeviceMotionEvent> mOwner;
  Nullable<double> mX, mY, mZ;
};

class nsDOMDeviceMotionEvent MOZ_FINAL : public nsDOMEvent
{
  typedef mozilla::dom::DeviceAccelerationInit DeviceAccelerationInit;
  typedef mozilla::dom::DeviceRotationRateInit DeviceRotationRateInit;
public:

  nsDOMDeviceMotionEvent(mozilla::dom::EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         mozilla::WidgetEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDeviceMotionEvent, nsDOMEvent)

  virtual JSObject* WrapObject(JSContext* aCx,
			       JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::DeviceMotionEventBinding::Wrap(aCx, aScope, this);
  }

  nsDOMDeviceAcceleration* GetAcceleration() const
  {
    return mAcceleration;
  }

  nsDOMDeviceAcceleration* GetAccelerationIncludingGravity() const
  {
    return mAccelerationIncludingGravity;
  }

  nsDOMDeviceRotationRate* GetRotationRate() const
  {
    return mRotationRate;
  }

  Nullable<double> GetInterval() const
  {
    return mInterval;
  }

  void InitDeviceMotionEvent(const nsAString& aType,
                             bool aCanBubble,
                             bool aCancelable,
                             const DeviceAccelerationInit& aAcceleration,
                             const DeviceAccelerationInit& aAccelerationIncludingGravity,
                             const DeviceRotationRateInit& aRotationRate,
                             Nullable<double> aInterval,
                             mozilla::ErrorResult& aRv);

  static already_AddRefed<nsDOMDeviceMotionEvent>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              const nsAString& aType,
              const mozilla::dom::DeviceMotionEventInit& aEventInitDict,
              mozilla::ErrorResult& aRv);

protected:
  nsRefPtr<nsDOMDeviceAcceleration> mAcceleration;
  nsRefPtr<nsDOMDeviceAcceleration> mAccelerationIncludingGravity;
  nsRefPtr<nsDOMDeviceRotationRate> mRotationRate;
  Nullable<double> mInterval;
};

#endif
