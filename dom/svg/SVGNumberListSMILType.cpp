/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGNumberListSMILType.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/SMILValue.h"
#include "nsMathUtils.h"
#include "SVGNumberList.h"
#include <math.h>

/* The "identity" number list for a given number list attribute (the effective
 * number list that is used if an attribute value is not specified) varies
 * widely for different number list attributes, and can depend on the value of
 * other attributes on the same element:
 *
 * http://www.w3.org/TR/SVG11/filters.html#feColorMatrixValuesAttribute
 *
 http://www.w3.org/TR/SVG11/filters.html#feComponentTransferTableValuesAttribute
 *
 http://www.w3.org/TR/SVG11/filters.html#feConvolveMatrixElementKernelMatrixAttribute
 * http://www.w3.org/TR/SVG11/text.html#TextElementRotateAttribute
 *
 * Note that we don't need to worry about that variation here, however. The way
 * that the SMIL engine creates and composites sandwich layers together allows
 * us to treat "identity" SMILValue objects as a number list of zeros. Such
 * identity SMILValues are identified by the fact that their
 # SVGNumberListAndInfo has not been given an element yet.
 */

namespace mozilla {

/*static*/
SVGNumberListSMILType SVGNumberListSMILType::sSingleton;

//----------------------------------------------------------------------
// nsISMILType implementation

void SVGNumberListSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mPtr = new SVGNumberListAndInfo();
  aValue.mType = this;
}

void SVGNumberListSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<SVGNumberListAndInfo*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nullptr;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGNumberListSMILType::Assign(SMILValue& aDest,
                                       const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");

  const SVGNumberListAndInfo* src =
      static_cast<const SVGNumberListAndInfo*>(aSrc.mU.mPtr);
  SVGNumberListAndInfo* dest =
      static_cast<SVGNumberListAndInfo*>(aDest.mU.mPtr);

  return dest->CopyFrom(*src);
}

bool SVGNumberListSMILType::IsEqual(const SMILValue& aLeft,
                                    const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return *static_cast<const SVGNumberListAndInfo*>(aLeft.mU.mPtr) ==
         *static_cast<const SVGNumberListAndInfo*>(aRight.mU.mPtr);
}

nsresult SVGNumberListSMILType::Add(SMILValue& aDest,
                                    const SMILValue& aValueToAdd,
                                    uint32_t aCount) const {
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL type");
  MOZ_ASSERT(aValueToAdd.mType == this, "Incompatible SMIL type");

  SVGNumberListAndInfo& dest =
      *static_cast<SVGNumberListAndInfo*>(aDest.mU.mPtr);
  const SVGNumberListAndInfo& valueToAdd =
      *static_cast<const SVGNumberListAndInfo*>(aValueToAdd.mU.mPtr);

  MOZ_ASSERT(dest.Element() || valueToAdd.Element(),
             "Target element propagation failure");

  if (!valueToAdd.Element()) {
    MOZ_ASSERT(valueToAdd.Length() == 0,
               "Not identity value - target element propagation failure");
    return NS_OK;
  }
  if (!dest.Element()) {
    MOZ_ASSERT(dest.Length() == 0,
               "Not identity value - target element propagation failure");
    if (!dest.SetLength(valueToAdd.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < dest.Length(); ++i) {
      dest[i] = aCount * valueToAdd[i];
    }
    dest.SetInfo(valueToAdd.Element());  // propagate target element info!
    return NS_OK;
  }
  MOZ_ASSERT(dest.Element() == valueToAdd.Element(),
             "adding values from different elements...?");
  if (dest.Length() != valueToAdd.Length()) {
    // For now we only support animation between lists with the same number of
    // items. SVGContentUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }
  for (uint32_t i = 0; i < dest.Length(); ++i) {
    dest[i] += aCount * valueToAdd[i];
  }
  dest.SetInfo(valueToAdd.Element());  // propagate target element info!
  return NS_OK;
}

nsresult SVGNumberListSMILType::ComputeDistance(const SMILValue& aFrom,
                                                const SMILValue& aTo,
                                                double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == this, "Unexpected SMIL type");
  MOZ_ASSERT(aTo.mType == this, "Incompatible SMIL type");

  const SVGNumberListAndInfo& from =
      *static_cast<const SVGNumberListAndInfo*>(aFrom.mU.mPtr);
  const SVGNumberListAndInfo& to =
      *static_cast<const SVGNumberListAndInfo*>(aTo.mU.mPtr);

  if (from.Length() != to.Length()) {
    // Lists in the 'values' attribute must have the same length.
    // SVGContentUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }

  // We return the root of the sum of the squares of the delta between the
  // numbers at each correspanding index.

  double total = 0.0;

  for (uint32_t i = 0; i < to.Length(); ++i) {
    double delta = to[i] - from[i];
    total += delta * delta;
  }
  double distance = sqrt(total);
  if (!std::isfinite(distance)) {
    return NS_ERROR_FAILURE;
  }
  aDistance = distance;

  return NS_OK;
}

nsresult SVGNumberListSMILType::Interpolate(const SMILValue& aStartVal,
                                            const SMILValue& aEndVal,
                                            double aUnitDistance,
                                            SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  const SVGNumberListAndInfo& start =
      *static_cast<const SVGNumberListAndInfo*>(aStartVal.mU.mPtr);
  const SVGNumberListAndInfo& end =
      *static_cast<const SVGNumberListAndInfo*>(aEndVal.mU.mPtr);
  SVGNumberListAndInfo& result =
      *static_cast<SVGNumberListAndInfo*>(aResult.mU.mPtr);

  MOZ_ASSERT(end.Element(), "Can't propagate target element");
  MOZ_ASSERT(start.Element() == end.Element() || !start.Element(),
             "Different target elements");

  if (start.Element() &&  // 'start' is not an "identity" value
      start.Length() != end.Length()) {
    // For now we only support animation between lists of the same length.
    // SVGContentUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }
  if (!result.SetLength(end.Length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  result.SetInfo(end.Element());  // propagate target element info!

  if (start.Length() != end.Length()) {
    MOZ_ASSERT(start.Length() == 0, "Not an identity value");
    for (uint32_t i = 0; i < end.Length(); ++i) {
      result[i] = aUnitDistance * end[i];
    }
    return NS_OK;
  }
  for (uint32_t i = 0; i < end.Length(); ++i) {
    result[i] = start[i] + (end[i] - start[i]) * aUnitDistance;
  }
  return NS_OK;
}

}  // namespace mozilla
