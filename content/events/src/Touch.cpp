/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Touch.h"

#include "mozilla/dom/TouchBinding.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsDOMTouchEvent.h"
#include "nsGUIEvent.h"
#include "nsPresContext.h"

namespace mozilla {
namespace dom {

/* static */ bool
Touch::PrefEnabled()
{
  return nsDOMTouchEvent::PrefEnabled();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(Touch, mTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Touch)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMTouch)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Touch)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Touch)

NS_IMETHODIMP
Touch::GetIdentifier(int32_t* aIdentifier)
{
  *aIdentifier = Identifier();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetTarget(nsIDOMEventTarget** aTarget)
{
  NS_ADDREF(*aTarget = Target());
  return NS_OK;
}

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

NS_IMETHODIMP
Touch::GetScreenX(int32_t* aScreenX)
{
  *aScreenX = ScreenX();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetScreenY(int32_t* aScreenY)
{
  *aScreenY = ScreenY();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetClientX(int32_t* aClientX)
{
  *aClientX = ClientX();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetClientY(int32_t* aClientY)
{
  *aClientY = ClientY();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetPageX(int32_t* aPageX)
{
  *aPageX = PageX();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetPageY(int32_t* aPageY)
{
  *aPageY = PageY();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetRadiusX(int32_t* aRadiusX)
{
  *aRadiusX = RadiusX();
  return NS_OK;
}
                                             
NS_IMETHODIMP
Touch::GetRadiusY(int32_t* aRadiusY)
{
  *aRadiusY = RadiusY();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetRotationAngle(float* aRotationAngle)
{
  *aRotationAngle = RotationAngle();
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetForce(float* aForce)
{
  *aForce = Force();
  return NS_OK;
}

bool
Touch::Equals(nsIDOMTouch* aTouch)
{
  float force;
  float orientation;
  int32_t radiusX, radiusY;
  aTouch->GetForce(&force);
  aTouch->GetRotationAngle(&orientation);
  aTouch->GetRadiusX(&radiusX);
  aTouch->GetRadiusY(&radiusY);
  return mRefPoint != aTouch->mRefPoint ||
         (mForce != force) ||
         (mRotationAngle != orientation) ||
         (mRadius.x != radiusX) || (mRadius.y != radiusY);
}

/* virtual */ JSObject*
Touch::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TouchBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
