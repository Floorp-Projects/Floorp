/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILIntegerType.h"
#include "nsSMILValue.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {

void
SMILIntegerType::Init(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mInt = 0;
  aValue.mType = this;
}

void
SMILIntegerType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mInt = 0;
  aValue.mType = nsSMILNullType::Singleton();
}

nsresult
SMILIntegerType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");
  aDest.mU.mInt = aSrc.mU.mInt;
  return NS_OK;
}

bool
SMILIntegerType::IsEqual(const nsSMILValue& aLeft,
                         const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mInt == aRight.mU.mInt;
}

nsresult
SMILIntegerType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                     uint32_t aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");
  aDest.mU.mInt += aValueToAdd.mU.mInt * aCount;
  return NS_OK;
}

nsresult
SMILIntegerType::ComputeDistance(const nsSMILValue& aFrom,
                                 const nsSMILValue& aTo,
                                 double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");
  aDistance = fabs(double(aTo.mU.mInt - aFrom.mU.mInt));
  return NS_OK;
}

nsresult
SMILIntegerType::Interpolate(const nsSMILValue& aStartVal,
                             const nsSMILValue& aEndVal,
                             double aUnitDistance,
                             nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType   == this, "Unexpected result type");

  const double startVal   = double(aStartVal.mU.mInt);
  const double endVal     = double(aEndVal.mU.mInt);
  const double currentVal = startVal + (endVal - startVal) * aUnitDistance;

  // When currentVal is exactly midway between its two nearest integers, we
  // jump to the "next" integer to provide simple, easy to remember and
  // consistent behaviour (from the SMIL author's point of view).

  if (startVal < endVal) {
    aResult.mU.mInt = int64_t(floor(currentVal + 0.5)); // round mid up
  } else {
    aResult.mU.mInt = int64_t(ceil(currentVal - 0.5)); // round mid down
  }

  return NS_OK;
}

} // namespace mozilla
