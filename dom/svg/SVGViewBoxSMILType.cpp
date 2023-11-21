/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGViewBoxSMILType.h"

#include "mozilla/SMILValue.h"
#include "nsDebug.h"
#include "SVGAnimatedViewBox.h"
#include <math.h>

namespace mozilla {

/*static*/
SVGViewBoxSMILType SVGViewBoxSMILType::sSingleton;

void SVGViewBoxSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mPtr = new SVGViewBox(0.0f, 0.0f, 0.0f, 0.0f);
  aValue.mType = this;
}

void SVGViewBoxSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value");
  delete static_cast<SVGViewBox*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nullptr;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGViewBoxSMILType::Assign(SMILValue& aDest,
                                    const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");

  const SVGViewBox* src = static_cast<const SVGViewBox*>(aSrc.mU.mPtr);
  SVGViewBox* dst = static_cast<SVGViewBox*>(aDest.mU.mPtr);
  *dst = *src;
  return NS_OK;
}

bool SVGViewBoxSMILType::IsEqual(const SMILValue& aLeft,
                                 const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  const SVGViewBox* leftBox = static_cast<const SVGViewBox*>(aLeft.mU.mPtr);
  const SVGViewBox* rightBox = static_cast<SVGViewBox*>(aRight.mU.mPtr);
  return *leftBox == *rightBox;
}

nsresult SVGViewBoxSMILType::Add(SMILValue& aDest, const SMILValue& aValueToAdd,
                                 uint32_t aCount) const {
  MOZ_ASSERT(aValueToAdd.mType == aDest.mType, "Trying to add invalid types");
  MOZ_ASSERT(aValueToAdd.mType == this, "Unexpected source type");

  auto* dest = static_cast<SVGViewBox*>(aDest.mU.mPtr);
  const auto* valueToAdd = static_cast<const SVGViewBox*>(aValueToAdd.mU.mPtr);

  if (dest->none || valueToAdd->none) {
    return NS_ERROR_FAILURE;
  }

  dest->x += valueToAdd->x * aCount;
  dest->y += valueToAdd->y * aCount;
  dest->width += valueToAdd->width * aCount;
  dest->height += valueToAdd->height * aCount;

  return NS_OK;
}

nsresult SVGViewBoxSMILType::ComputeDistance(const SMILValue& aFrom,
                                             const SMILValue& aTo,
                                             double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType, "Trying to compare different types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected source type");

  const SVGViewBox* from = static_cast<const SVGViewBox*>(aFrom.mU.mPtr);
  const SVGViewBox* to = static_cast<const SVGViewBox*>(aTo.mU.mPtr);

  if (from->none || to->none) {
    return NS_ERROR_FAILURE;
  }

  // We use the distances between the edges rather than the difference between
  // the x, y, width and height for the "distance". This is necessary in
  // order for the "distance" result that we calculate to be the same for a
  // given change in the left side as it is for an equal change in the opposite
  // side. See https://bugzilla.mozilla.org/show_bug.cgi?id=541884#c12

  float dLeft = to->x - from->x;
  float dTop = to->y - from->y;
  float dRight = (to->x + to->width) - (from->x + from->width);
  float dBottom = (to->y + to->height) - (from->y + from->height);

  aDistance = std::sqrt(dLeft * dLeft + dTop * dTop + dRight * dRight +
                        dBottom * dBottom);

  return NS_OK;
}

nsresult SVGViewBoxSMILType::Interpolate(const SMILValue& aStartVal,
                                         const SMILValue& aEndVal,
                                         double aUnitDistance,
                                         SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  const SVGViewBox* start = static_cast<const SVGViewBox*>(aStartVal.mU.mPtr);
  const SVGViewBox* end = static_cast<const SVGViewBox*>(aEndVal.mU.mPtr);

  if (start->none || end->none) {
    return NS_ERROR_FAILURE;
  }

  SVGViewBox* current = static_cast<SVGViewBox*>(aResult.mU.mPtr);

  float x = (start->x + (end->x - start->x) * aUnitDistance);
  float y = (start->y + (end->y - start->y) * aUnitDistance);
  float width = (start->width + (end->width - start->width) * aUnitDistance);
  float height =
      (start->height + (end->height - start->height) * aUnitDistance);

  *current = SVGViewBox(x, y, width, height);

  return NS_OK;
}

}  // namespace mozilla
