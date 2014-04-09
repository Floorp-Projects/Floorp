/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Touch.h"

#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/TouchBinding.h"
#include "mozilla/dom/TouchEvent.h"
#include "nsGlobalWindow.h"
#include "nsContentUtils.h"
#include "nsIContent.h"

namespace mozilla {
namespace dom {

Touch::Touch(EventTarget* aTarget,
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

Touch::Touch(int32_t aIdentifier,
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

Touch::~Touch()
{
}

// static
bool
Touch::PrefEnabled(JSContext* aCx, JSObject* aGlobal)
{
  return TouchEvent::PrefEnabled(aCx, aGlobal);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(Touch, mTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Touch)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Touch)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Touch)

EventTarget*
Touch::Target() const
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTarget);
  if (content && content->ChromeOnlyAccess() &&
      !nsContentUtils::CanAccessNativeAnon()) {
    return content->FindFirstNonChromeOnlyAccessContent();
  }

  return mTarget;
}

void
Touch::InitializePoints(nsPresContext* aPresContext, WidgetEvent* aEvent)
{
  if (mPointsInitialized) {
    return;
  }
  mClientPoint = Event::GetClientCoords(
    aPresContext, aEvent, LayoutDeviceIntPoint::FromUntyped(mRefPoint),
    mClientPoint);
  mPagePoint = Event::GetPageCoords(
    aPresContext, aEvent, LayoutDeviceIntPoint::FromUntyped(mRefPoint),
    mClientPoint);
  mScreenPoint = Event::GetScreenCoords(aPresContext, aEvent,
    LayoutDeviceIntPoint::FromUntyped(mRefPoint));
  mPointsInitialized = true;
}

void
Touch::SetTarget(EventTarget* aTarget)
{
  mTarget = aTarget;
}

bool
Touch::Equals(Touch* aTouch)
{
  return mRefPoint == aTouch->mRefPoint &&
         mForce == aTouch->Force() &&
         mRotationAngle == aTouch->RotationAngle() &&
         mRadius.x == aTouch->RadiusX() &&
         mRadius.y == aTouch->RadiusY();
}

JSObject*
Touch::WrapObject(JSContext* aCx)
{
  return TouchBinding::Wrap(aCx, this);
}

// Parent ourselves to the window of the target. This achieves the desirable
// effects of parenting to the target, but avoids making the touch inaccessible
// when the target happens to be NAC and therefore reflected into the XBL scope.
EventTarget*
Touch::GetParentObject()
{
  if (!mTarget) {
    return nullptr;
  }
  nsCOMPtr<nsPIDOMWindow> outer = do_QueryInterface(mTarget->GetOwnerGlobal());
  if (!outer) {
    return nullptr;
  }
  MOZ_ASSERT(outer->IsOuterWindow());
  return static_cast<nsGlobalWindow*>(outer->GetCurrentInnerWindow());
}

} // namespace dom
} // namespace mozilla
