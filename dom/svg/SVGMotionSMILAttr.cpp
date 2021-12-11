/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a dummy attribute targeted by <animateMotion> element */

#include "SVGMotionSMILAttr.h"

#include "mozilla/dom/SVGAnimationElement.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/SMILValue.h"
#include "SVGMotionSMILType.h"
#include "nsDebug.h"
#include "gfx2DGlue.h"

namespace mozilla {

nsresult SVGMotionSMILAttr::ValueFromString(
    const nsAString& aStr, const dom::SVGAnimationElement* aSrcElement,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  MOZ_ASSERT_UNREACHABLE(
      "Shouldn't using SMILAttr::ValueFromString for "
      "parsing animateMotion's SMIL values.");
  return NS_ERROR_FAILURE;
}

SMILValue SVGMotionSMILAttr::GetBaseValue() const {
  return SMILValue(&SVGMotionSMILType::sSingleton);
}

void SVGMotionSMILAttr::ClearAnimValue() {
  mSVGElement->SetAnimateMotionTransform(nullptr);
}

nsresult SVGMotionSMILAttr::SetAnimValue(const SMILValue& aValue) {
  gfx::Matrix matrix = SVGMotionSMILType::CreateMatrix(aValue);
  mSVGElement->SetAnimateMotionTransform(&matrix);
  return NS_OK;
}

const nsIContent* SVGMotionSMILAttr::GetTargetNode() const {
  return mSVGElement;
}

}  // namespace mozilla
