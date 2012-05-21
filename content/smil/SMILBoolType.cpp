/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILBoolType.h"
#include "nsSMILValue.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {

/*static*/ SMILBoolType SMILBoolType::sSingleton;

void
SMILBoolType::Init(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mBool = false;
  aValue.mType = this;
}

void
SMILBoolType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mBool = false;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
SMILBoolType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");
  aDest.mU.mBool = aSrc.mU.mBool;
  return NS_OK;
}

bool
SMILBoolType::IsEqual(const nsSMILValue& aLeft,
                      const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mBool == aRight.mU.mBool;
}

nsresult
SMILBoolType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                  PRUint32 aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");
  return NS_ERROR_FAILURE; // bool values can't be added to each other
}

nsresult
SMILBoolType::ComputeDistance(const nsSMILValue& aFrom,
                              const nsSMILValue& aTo,
                              double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");
  return NS_ERROR_FAILURE; // there is no concept of distance between bool values
}

nsresult
SMILBoolType::Interpolate(const nsSMILValue& aStartVal,
                          const nsSMILValue& aEndVal,
                          double aUnitDistance,
                          nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
      "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
      "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType   == this, "Unexpected result type");
  return NS_ERROR_FAILURE; // bool values do not interpolate
}

} // namespace mozilla
