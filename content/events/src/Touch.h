/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Touch_h
#define mozilla_dom_Touch_h

#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "nsJSEnvironment.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/EventTarget.h"
#include "Units.h"

namespace mozilla {
namespace dom {

class Touch MOZ_FINAL : public nsISupports
                      , public nsWrapperCache
{
public:
  static bool PrefEnabled();

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
      SetIsDOMBinding();
      mTarget = aTarget;
      mIdentifier = aIdentifier;
      mPagePoint = CSSIntPoint(aPageX, aPageY);
      mScreenPoint = nsIntPoint(aScreenX, aScreenY);
      mClientPoint = CSSIntPoint(aClientX, aClientY);
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
      SetIsDOMBinding();
      mIdentifier = aIdentifier;
      mPagePoint = CSSIntPoint(0, 0);
      mScreenPoint = nsIntPoint(0, 0);
      mClientPoint = CSSIntPoint(0, 0);
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
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Touch)

  void InitializePoints(nsPresContext* aPresContext, nsEvent* aEvent);

  void SetTarget(mozilla::dom::EventTarget *aTarget)
  {
    mTarget = aTarget;
  }
  bool Equals(Touch* aTouch);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
  EventTarget* GetParentObject() { return mTarget; }

  // WebIDL
  int32_t Identifier() const { return mIdentifier; }
  EventTarget* Target() const;
  int32_t ScreenX() const { return mScreenPoint.x; }
  int32_t ScreenY() const { return mScreenPoint.y; }
  int32_t ClientX() const { return mClientPoint.x; }
  int32_t ClientY() const { return mClientPoint.y; }
  int32_t PageX() const { return mPagePoint.x; }
  int32_t PageY() const { return mPagePoint.y; }
  int32_t RadiusX() const { return mRadius.x; }
  int32_t RadiusY() const { return mRadius.y; }
  float RotationAngle() const { return mRotationAngle; }
  float Force() const { return mForce; }

  nsCOMPtr<EventTarget> mTarget;
  nsIntPoint mRefPoint;
  bool mChanged;
  uint32_t mMessage;
  int32_t mIdentifier;
  CSSIntPoint mPagePoint;
  CSSIntPoint mClientPoint;
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
