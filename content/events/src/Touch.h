/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Touch_h
#define mozilla_dom_Touch_h

#include "nsDOMUIEvent.h"
#include "nsIDOMTouchEvent.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "nsJSEnvironment.h"

namespace mozilla {
namespace dom {

class Touch MOZ_FINAL : public nsIDOMTouch
{
public:
  Touch(mozilla::dom::EventTarget* aTarget,
        int32_t aIdentifier,
        int32_t aPageX,
        int32_t aPageY,
        int32_t aScreenX,
        int32_t aScreenY,
        int32_t aClientX,
        int32_t aClientY,
        int32_t aRadiusX,
        int32_t aRadiusY,
        float aRotationAngle,
        float aForce)
    {
      mTarget = aTarget;
      mIdentifier = aIdentifier;
      mPagePoint = nsIntPoint(aPageX, aPageY);
      mScreenPoint = nsIntPoint(aScreenX, aScreenY);
      mClientPoint = nsIntPoint(aClientX, aClientY);
      mRefPoint = nsIntPoint(0, 0);
      mPointsInitialized = true;
      mRadius.x = aRadiusX;
      mRadius.y = aRadiusY;
      mRotationAngle = aRotationAngle;
      mForce = aForce;

      mChanged = false;
      mMessage = 0;
      nsJSContext::LikelyShortLivingObjectCreated();
    }
  Touch(int32_t aIdentifier,
        nsIntPoint aPoint,
        nsIntPoint aRadius,
        float aRotationAngle,
        float aForce)
    {
      mIdentifier = aIdentifier;
      mPagePoint = nsIntPoint(0, 0);
      mScreenPoint = nsIntPoint(0, 0);
      mClientPoint = nsIntPoint(0, 0);
      mRefPoint = aPoint;
      mPointsInitialized = false;
      mRadius = aRadius;
      mRotationAngle = aRotationAngle;
      mForce = aForce;

      mChanged = false;
      mMessage = 0;
      nsJSContext::LikelyShortLivingObjectCreated();
    }
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(Touch)
  NS_DECL_NSIDOMTOUCH
  void InitializePoints(nsPresContext* aPresContext, nsEvent* aEvent)
  {
    if (mPointsInitialized) {
      return;
    }
    mClientPoint = nsDOMEvent::GetClientCoords(aPresContext,
                                               aEvent,
                                               mRefPoint,
                                               mClientPoint);
    mPagePoint = nsDOMEvent::GetPageCoords(aPresContext,
                                           aEvent,
                                           mRefPoint,
                                           mClientPoint);
    mScreenPoint = nsDOMEvent::GetScreenCoords(aPresContext, aEvent, mRefPoint);
    mPointsInitialized = true;
  }
  void SetTarget(mozilla::dom::EventTarget *aTarget)
  {
    mTarget = aTarget;
  }
  bool Equals(nsIDOMTouch* aTouch);

  int32_t mIdentifier;
  nsIntPoint mPagePoint;
  nsIntPoint mClientPoint;
  nsIntPoint mScreenPoint;
  nsIntPoint mRadius;
  float mRotationAngle;
  float mForce;
protected:
  bool mPointsInitialized;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Touch_h
