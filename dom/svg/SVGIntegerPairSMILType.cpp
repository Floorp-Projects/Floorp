/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGIntegerPairSMILType.h"
#include "nsSMILValue.h"
#include "nsMathUtils.h"
#include "nsDebug.h"

namespace mozilla {

void
SVGIntegerPairSMILType::Init(nsSMILValue& aValue) const
{
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mIntPair[0] = 0;
  aValue.mU.mIntPair[1] = 0;
  aValue.mType = this;
}

void
SVGIntegerPairSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mIntPair[0] = 0;
  aValue.mU.mIntPair[1] = 0;
  aValue.mType = nsSMILNullType::Singleton();
}

nsresult
SVGIntegerPairSMILType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");

  aDest.mU.mIntPair[0] = aSrc.mU.mIntPair[0];
  aDest.mU.mIntPair[1] = aSrc.mU.mIntPair[1];
  return NS_OK;
}

bool
SVGIntegerPairSMILType::IsEqual(const nsSMILValue& aLeft,
                                const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mIntPair[0] == aRight.mU.mIntPair[0] &&
         aLeft.mU.mIntPair[1] == aRight.mU.mIntPair[1];
}

nsresult
SVGIntegerPairSMILType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                            uint32_t aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");

  aDest.mU.mIntPair[0] += aValueToAdd.mU.mIntPair[0] * aCount;
  aDest.mU.mIntPair[1] += aValueToAdd.mU.mIntPair[1] * aCount;

  return NS_OK;
}

nsresult
SVGIntegerPairSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                        const nsSMILValue& aTo,
                                        double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");

  double delta[2];
  delta[0] = aTo.mU.mIntPair[0] - aFrom.mU.mIntPair[0];
  delta[1] = aTo.mU.mIntPair[1] - aFrom.mU.mIntPair[1];

  aDistance = NS_hypot(delta[0], delta[1]);
  return NS_OK;
}

nsresult
SVGIntegerPairSMILType::Interpolate(const nsSMILValue& aStartVal,
                                    const nsSMILValue& aEndVal,
                                    double aUnitDistance,
                                    nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType == this, "Unexpected result type");

  double currentVal[2];
  currentVal[0] = aStartVal.mU.mIntPair[0] +
                  (aEndVal.mU.mIntPair[0] - aStartVal.mU.mIntPair[0]) * aUnitDistance;
  currentVal[1] = aStartVal.mU.mIntPair[1] +
                  (aEndVal.mU.mIntPair[1] - aStartVal.mU.mIntPair[1]) * aUnitDistance;

  aResult.mU.mIntPair[0] = NS_lround(currentVal[0]);
  aResult.mU.mIntPair[1] = NS_lround(currentVal[1]);
  return NS_OK;
}

} // namespace mozilla
