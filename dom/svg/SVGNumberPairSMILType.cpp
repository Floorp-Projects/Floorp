/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGNumberPairSMILType.h"

#include "mozilla/SMILValue.h"
#include "nsMathUtils.h"
#include "nsDebug.h"

namespace mozilla {

/*static*/
SVGNumberPairSMILType SVGNumberPairSMILType::sSingleton;

void SVGNumberPairSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mNumberPair[0] = 0;
  aValue.mU.mNumberPair[1] = 0;
  aValue.mType = this;
}

void SVGNumberPairSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mNumberPair[0] = 0;
  aValue.mU.mNumberPair[1] = 0;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGNumberPairSMILType::Assign(SMILValue& aDest,
                                       const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");

  aDest.mU.mNumberPair[0] = aSrc.mU.mNumberPair[0];
  aDest.mU.mNumberPair[1] = aSrc.mU.mNumberPair[1];
  return NS_OK;
}

bool SVGNumberPairSMILType::IsEqual(const SMILValue& aLeft,
                                    const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mNumberPair[0] == aRight.mU.mNumberPair[0] &&
         aLeft.mU.mNumberPair[1] == aRight.mU.mNumberPair[1];
}

nsresult SVGNumberPairSMILType::Add(SMILValue& aDest,
                                    const SMILValue& aValueToAdd,
                                    uint32_t aCount) const {
  MOZ_ASSERT(aValueToAdd.mType == aDest.mType, "Trying to add invalid types");
  MOZ_ASSERT(aValueToAdd.mType == this, "Unexpected source type");

  aDest.mU.mNumberPair[0] += aValueToAdd.mU.mNumberPair[0] * aCount;
  aDest.mU.mNumberPair[1] += aValueToAdd.mU.mNumberPair[1] * aCount;

  return NS_OK;
}

nsresult SVGNumberPairSMILType::ComputeDistance(const SMILValue& aFrom,
                                                const SMILValue& aTo,
                                                double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType, "Trying to compare different types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected source type");

  double delta[2];
  delta[0] = aTo.mU.mNumberPair[0] - aFrom.mU.mNumberPair[0];
  delta[1] = aTo.mU.mNumberPair[1] - aFrom.mU.mNumberPair[1];

  aDistance = NS_hypot(delta[0], delta[1]);
  return NS_OK;
}

nsresult SVGNumberPairSMILType::Interpolate(const SMILValue& aStartVal,
                                            const SMILValue& aEndVal,
                                            double aUnitDistance,
                                            SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  aResult.mU.mNumberPair[0] =
      float(aStartVal.mU.mNumberPair[0] +
            (aEndVal.mU.mNumberPair[0] - aStartVal.mU.mNumberPair[0]) *
                aUnitDistance);
  aResult.mU.mNumberPair[1] =
      float(aStartVal.mU.mNumberPair[1] +
            (aEndVal.mU.mNumberPair[1] - aStartVal.mU.mNumberPair[1]) *
                aUnitDistance);
  return NS_OK;
}

}  // namespace mozilla
