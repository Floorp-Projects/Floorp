/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGNumberPairSMILType.h"
#include "nsSMILValue.h"
#include "nsMathUtils.h"
#include "nsDebug.h"

namespace mozilla {

/*static*/ SVGNumberPairSMILType SVGNumberPairSMILType::sSingleton;

void
SVGNumberPairSMILType::Init(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mNumberPair[0] = 0;
  aValue.mU.mNumberPair[1] = 0;
  aValue.mType = this;
}

void
SVGNumberPairSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mNumberPair[0] = 0;
  aValue.mU.mNumberPair[1] = 0;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
SVGNumberPairSMILType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");

  aDest.mU.mNumberPair[0] = aSrc.mU.mNumberPair[0];
  aDest.mU.mNumberPair[1] = aSrc.mU.mNumberPair[1];
  return NS_OK;
}

bool
SVGNumberPairSMILType::IsEqual(const nsSMILValue& aLeft,
                               const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mNumberPair[0] == aRight.mU.mNumberPair[0] &&
         aLeft.mU.mNumberPair[1] == aRight.mU.mNumberPair[1];
}

nsresult
SVGNumberPairSMILType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                           uint32_t aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");

  aDest.mU.mNumberPair[0] += aValueToAdd.mU.mNumberPair[0] * aCount;
  aDest.mU.mNumberPair[1] += aValueToAdd.mU.mNumberPair[1] * aCount;

  return NS_OK;
}

nsresult
SVGNumberPairSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                       const nsSMILValue& aTo,
                                       double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");

  double delta[2];
  delta[0] = aTo.mU.mNumberPair[0] - aFrom.mU.mNumberPair[0];
  delta[1] = aTo.mU.mNumberPair[1] - aFrom.mU.mNumberPair[1];

  aDistance = NS_hypot(delta[0], delta[1]);
  return NS_OK;
}

nsresult
SVGNumberPairSMILType::Interpolate(const nsSMILValue& aStartVal,
                                   const nsSMILValue& aEndVal,
                                   double aUnitDistance,
                                   nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType == this, "Unexpected result type");

  aResult.mU.mNumberPair[0] =
    float(aStartVal.mU.mNumberPair[0] +
          (aEndVal.mU.mNumberPair[0] - aStartVal.mU.mNumberPair[0]) * aUnitDistance);
  aResult.mU.mNumberPair[1] =
    float(aStartVal.mU.mNumberPair[1] +
          (aEndVal.mU.mNumberPair[1] - aStartVal.mU.mNumberPair[1]) * aUnitDistance);
  return NS_OK;
}

} // namespace mozilla
