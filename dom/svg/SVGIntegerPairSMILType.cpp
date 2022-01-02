/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGIntegerPairSMILType.h"

#include "mozilla/SMILValue.h"
#include "nsMathUtils.h"
#include "nsDebug.h"

namespace mozilla {

void SVGIntegerPairSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mIntPair[0] = 0;
  aValue.mU.mIntPair[1] = 0;
  aValue.mType = this;
}

void SVGIntegerPairSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mIntPair[0] = 0;
  aValue.mU.mIntPair[1] = 0;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGIntegerPairSMILType::Assign(SMILValue& aDest,
                                        const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");

  aDest.mU.mIntPair[0] = aSrc.mU.mIntPair[0];
  aDest.mU.mIntPair[1] = aSrc.mU.mIntPair[1];
  return NS_OK;
}

bool SVGIntegerPairSMILType::IsEqual(const SMILValue& aLeft,
                                     const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mIntPair[0] == aRight.mU.mIntPair[0] &&
         aLeft.mU.mIntPair[1] == aRight.mU.mIntPair[1];
}

nsresult SVGIntegerPairSMILType::Add(SMILValue& aDest,
                                     const SMILValue& aValueToAdd,
                                     uint32_t aCount) const {
  MOZ_ASSERT(aValueToAdd.mType == aDest.mType, "Trying to add invalid types");
  MOZ_ASSERT(aValueToAdd.mType == this, "Unexpected source type");

  aDest.mU.mIntPair[0] += aValueToAdd.mU.mIntPair[0] * aCount;
  aDest.mU.mIntPair[1] += aValueToAdd.mU.mIntPair[1] * aCount;

  return NS_OK;
}

nsresult SVGIntegerPairSMILType::ComputeDistance(const SMILValue& aFrom,
                                                 const SMILValue& aTo,
                                                 double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType, "Trying to compare different types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected source type");

  double delta[2];
  delta[0] = aTo.mU.mIntPair[0] - aFrom.mU.mIntPair[0];
  delta[1] = aTo.mU.mIntPair[1] - aFrom.mU.mIntPair[1];

  aDistance = NS_hypot(delta[0], delta[1]);
  return NS_OK;
}

nsresult SVGIntegerPairSMILType::Interpolate(const SMILValue& aStartVal,
                                             const SMILValue& aEndVal,
                                             double aUnitDistance,
                                             SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  double currentVal[2];
  currentVal[0] =
      aStartVal.mU.mIntPair[0] +
      (aEndVal.mU.mIntPair[0] - aStartVal.mU.mIntPair[0]) * aUnitDistance;
  currentVal[1] =
      aStartVal.mU.mIntPair[1] +
      (aEndVal.mU.mIntPair[1] - aStartVal.mU.mIntPair[1]) * aUnitDistance;

  aResult.mU.mIntPair[0] = NS_lround(currentVal[0]);
  aResult.mU.mIntPair[1] = NS_lround(currentVal[1]);
  return NS_OK;
}

}  // namespace mozilla
