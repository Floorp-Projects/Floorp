/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGViewBoxSMILType.h"
#include "nsSMILValue.h"
#include "nsSVGViewBox.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {
  
/*static*/ SVGViewBoxSMILType SVGViewBoxSMILType::sSingleton;

void
SVGViewBoxSMILType::Init(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");

  nsSVGViewBoxRect* viewBox = new nsSVGViewBoxRect();
  aValue.mU.mPtr = viewBox;
  aValue.mType = this;
}

void
SVGViewBoxSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  delete static_cast<nsSVGViewBoxRect*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nsnull;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
SVGViewBoxSMILType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");

  const nsSVGViewBoxRect* src = static_cast<const nsSVGViewBoxRect*>(aSrc.mU.mPtr);
  nsSVGViewBoxRect* dst = static_cast<nsSVGViewBoxRect*>(aDest.mU.mPtr);
  *dst = *src;
  return NS_OK;
}

bool
SVGViewBoxSMILType::IsEqual(const nsSMILValue& aLeft,
                            const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  const nsSVGViewBoxRect* leftBox =
    static_cast<const nsSVGViewBoxRect*>(aLeft.mU.mPtr);
  const nsSVGViewBoxRect* rightBox =
    static_cast<nsSVGViewBoxRect*>(aRight.mU.mPtr);
  return *leftBox == *rightBox;
}

nsresult
SVGViewBoxSMILType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                        PRUint32 aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=541884#c3 and the two
  // comments that follow that one for arguments for and against allowing
  // viewBox to be additive.

  return NS_ERROR_FAILURE;
}

nsresult
SVGViewBoxSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                    const nsSMILValue& aTo,
                                    double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");

  const nsSVGViewBoxRect* from = static_cast<const nsSVGViewBoxRect*>(aFrom.mU.mPtr);
  const nsSVGViewBoxRect* to = static_cast<const nsSVGViewBoxRect*>(aTo.mU.mPtr);

  // We use the distances between the edges rather than the difference between
  // the x, y, width and height for the "distance". This is necessary in
  // order for the "distance" result that we calculate to be the same for a
  // given change in the left side as it is for an equal change in the opposite
  // side. See https://bugzilla.mozilla.org/show_bug.cgi?id=541884#c12

  float dLeft = to->x - from->x;
  float dTop = to->y - from->y;
  float dRight = ( to->x + to->width ) - ( from->x + from->width );
  float dBottom = ( to->y + to->height ) - ( from->y + from->height );

  aDistance = sqrt(dLeft*dLeft + dTop*dTop + dRight*dRight + dBottom*dBottom);

  return NS_OK;
}

nsresult
SVGViewBoxSMILType::Interpolate(const nsSMILValue& aStartVal,
                                const nsSMILValue& aEndVal,
                                double aUnitDistance,
                                nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType == this, "Unexpected result type");
  
  const nsSVGViewBoxRect* start = static_cast<const nsSVGViewBoxRect*>(aStartVal.mU.mPtr);
  const nsSVGViewBoxRect* end = static_cast<const nsSVGViewBoxRect*>(aEndVal.mU.mPtr);
  nsSVGViewBoxRect* current = static_cast<nsSVGViewBoxRect*>(aResult.mU.mPtr);

  float x = (start->x + (end->x - start->x) * aUnitDistance);
  float y = (start->y + (end->y - start->y) * aUnitDistance);
  float width = (start->width + (end->width - start->width) * aUnitDistance);
  float height = (start->height + (end->height - start->height) * aUnitDistance);

  *current = nsSVGViewBoxRect(x, y, width, height);
  
  return NS_OK;
}

} // namespace mozilla
