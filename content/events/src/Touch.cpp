/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Touch.h"
#include "nsGUIEvent.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIXPCScriptable.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "nsPresContext.h"

DOMCI_DATA(Touch, mozilla::dom::Touch)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_1(Touch, mTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Touch)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMTouch)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTouch)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Touch)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Touch)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Touch)

NS_IMETHODIMP
Touch::GetIdentifier(int32_t* aIdentifier)
{
  *aIdentifier = mIdentifier;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetTarget(nsIDOMEventTarget** aTarget)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTarget);
  if (content && content->ChromeOnlyAccess() &&
      !nsContentUtils::CanAccessNativeAnon()) {
    content = content->FindFirstNonChromeOnlyAccessContent();
    *aTarget = content.forget().get();
    return NS_OK;
  }
  NS_IF_ADDREF(*aTarget = mTarget);
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetScreenX(int32_t* aScreenX)
{
  *aScreenX = mScreenPoint.x;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetScreenY(int32_t* aScreenY)
{
  *aScreenY = mScreenPoint.y;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetClientX(int32_t* aClientX)
{
  *aClientX = mClientPoint.x;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetClientY(int32_t* aClientY)
{
  *aClientY = mClientPoint.y;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetPageX(int32_t* aPageX)
{
  *aPageX = mPagePoint.x;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetPageY(int32_t* aPageY)
{
  *aPageY = mPagePoint.y;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetRadiusX(int32_t* aRadiusX)
{
  *aRadiusX = mRadius.x;
  return NS_OK;
}
                                             
NS_IMETHODIMP
Touch::GetRadiusY(int32_t* aRadiusY)
{
  *aRadiusY = mRadius.y;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetRotationAngle(float* aRotationAngle)
{
  *aRotationAngle = mRotationAngle;
  return NS_OK;
}

NS_IMETHODIMP
Touch::GetForce(float* aForce)
{
  *aForce = mForce;
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

} // namespace dom
} // namespace mozilla
