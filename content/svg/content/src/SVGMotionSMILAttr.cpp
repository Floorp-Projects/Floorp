/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a dummy attribute targeted by <animateMotion> element */

#include "SVGMotionSMILAttr.h"
#include "SVGMotionSMILType.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "nsSMILValue.h"
#include "nsDebug.h"
#include "nsSVGElement.h"

namespace mozilla {

nsresult
SVGMotionSMILAttr::ValueFromString(const nsAString& aStr,
                                   const dom::SVGAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const
{
  NS_NOTREACHED("Shouldn't using nsISMILAttr::ValueFromString for parsing "
                "animateMotion's SMIL values.");
  return NS_ERROR_FAILURE;
}

nsSMILValue
SVGMotionSMILAttr::GetBaseValue() const
{
  return nsSMILValue(&SVGMotionSMILType::sSingleton);
}

void
SVGMotionSMILAttr::ClearAnimValue()
{
  mSVGElement->SetAnimateMotionTransform(nullptr);
}

nsresult
SVGMotionSMILAttr::SetAnimValue(const nsSMILValue& aValue)
{
  gfxMatrix matrix = SVGMotionSMILType::CreateMatrix(aValue);
  mSVGElement->SetAnimateMotionTransform(&matrix);
  return NS_OK;
}

const nsIContent*
SVGMotionSMILAttr::GetTargetNode() const
{
  return mSVGElement;
}

} // namespace mozilla
