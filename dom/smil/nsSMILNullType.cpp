/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILNullType.h"
#include "nsSMILValue.h"
#include "nsDebug.h"

nsresult
nsSMILNullType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aSrc.mType == this, "Unexpected source type");
  aDest.mU    = aSrc.mU;
  aDest.mType = Singleton();
  return NS_OK;
}

bool
nsSMILNullType::IsEqual(const nsSMILValue& aLeft,
                        const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return true;  // All null-typed values are equivalent.
}

nsresult
nsSMILNullType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                    uint32_t aCount) const
{
  NS_NOTREACHED("Adding NULL type");
  return NS_ERROR_FAILURE;
}

nsresult
nsSMILNullType::ComputeDistance(const nsSMILValue& aFrom,
                                const nsSMILValue& aTo,
                                double& aDistance) const
{
  NS_NOTREACHED("Computing distance for NULL type");
  return NS_ERROR_FAILURE;
}

nsresult
nsSMILNullType::Interpolate(const nsSMILValue& aStartVal,
                            const nsSMILValue& aEndVal,
                            double aUnitDistance,
                            nsSMILValue& aResult) const
{
  NS_NOTREACHED("Interpolating NULL type");
  return NS_ERROR_FAILURE;
}
