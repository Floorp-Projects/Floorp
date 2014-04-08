/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Touch_h_
#define mozilla_dom_Touch_h_

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/MouseEvents.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "Units.h"

class nsPresContext;

namespace mozilla {
namespace dom {

class EventTarget;

class Touch MOZ_FINAL : public nsISupports
                      , public nsWrapperCache
                      , public WidgetPointerHelper
{
public:
  static bool PrefEnabled(JSContext* aCx, JSObject* aGlobal);

  Touch(EventTarget* aTarget,
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
        float aForce);
  Touch(int32_t aIdentifier,
        nsIntPoint aPoint,
        nsIntPoint aRadius,
        float aRotationAngle,
        float aForce);

  ~Touch();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Touch)

  void InitializePoints(nsPresContext* aPresContext, WidgetEvent* aEvent);

  void SetTarget(EventTarget* aTarget);

  bool Equals(Touch* aTouch);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  EventTarget* GetParentObject();

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

#endif // mozilla_dom_Touch_h_
