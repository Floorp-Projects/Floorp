/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Touch_h_
#define mozilla_dom_Touch_h_

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/TouchBinding.h"
#include "nsWrapperCache.h"
#include "Units.h"

class nsPresContext;

namespace mozilla {
namespace dom {

class EventTarget;

class Touch final : public nsISupports,
                    public nsWrapperCache,
                    public WidgetPointerHelper {
 public:
  static bool PrefEnabled(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<Touch> Constructor(const GlobalObject& aGlobal,
                                             const TouchInit& aParam);

  Touch(EventTarget* aTarget, int32_t aIdentifier, int32_t aPageX,
        int32_t aPageY, int32_t aScreenX, int32_t aScreenY, int32_t aClientX,
        int32_t aClientY, int32_t aRadiusX, int32_t aRadiusY,
        float aRotationAngle, float aForce);
  Touch(int32_t aIdentifier, LayoutDeviceIntPoint aPoint,
        LayoutDeviceIntPoint aRadius, float aRotationAngle, float aForce);
  Touch(const Touch& aOther);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Touch)

  void InitializePoints(nsPresContext* aPresContext, WidgetEvent* aEvent);

  // Note, this sets both mOriginalTarget and mTarget.
  void SetTouchTarget(EventTarget* aTarget);

  bool Equals(Touch* aTouch) const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject* GetParentObject();

  // WebIDL
  int32_t Identifier() const { return mIdentifier; }
  EventTarget* GetTarget() const;
  int32_t ScreenX(CallerType aCallerType) const;
  int32_t ScreenY(CallerType aCallerType) const;
  int32_t ClientX() const { return mClientPoint.x; }
  int32_t ClientY() const { return mClientPoint.y; }
  int32_t PageX() const { return mPagePoint.x; }
  int32_t PageY() const { return mPagePoint.y; }
  int32_t RadiusX(CallerType aCallerType) const;
  int32_t RadiusY(CallerType aCallerType) const;
  float RotationAngle(CallerType aCallerType) const;
  float Force(CallerType aCallerType) const;

  nsCOMPtr<EventTarget> mOriginalTarget;
  nsCOMPtr<EventTarget> mTarget;
  LayoutDeviceIntPoint mRefPoint;
  bool mChanged;

  // Is this touch instance being suppressed to dispatch touch event to content.
  // We can't remove touch instance from WidgetTouchEvent::mTouches because we
  // still need it when dispatching pointer events.
  bool mIsTouchEventSuppressed;

  uint32_t mMessage;
  int32_t mIdentifier;
  CSSIntPoint mPagePoint;
  CSSIntPoint mClientPoint;
  CSSIntPoint mScreenPoint;
  LayoutDeviceIntPoint mRadius;
  float mRotationAngle;
  float mForce;

 protected:
  ~Touch();

  bool mPointsInitialized;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Touch_h_
