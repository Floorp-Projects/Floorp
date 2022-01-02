/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILIntegerType.h"

#include "mozilla/SMILValue.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {

void SMILIntegerType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mInt = 0;
  aValue.mType = this;
}

void SMILIntegerType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mInt = 0;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SMILIntegerType::Assign(SMILValue& aDest,
                                 const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");
  aDest.mU.mInt = aSrc.mU.mInt;
  return NS_OK;
}

bool SMILIntegerType::IsEqual(const SMILValue& aLeft,
                              const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mInt == aRight.mU.mInt;
}

nsresult SMILIntegerType::Add(SMILValue& aDest, const SMILValue& aValueToAdd,
                              uint32_t aCount) const {
  MOZ_ASSERT(aValueToAdd.mType == aDest.mType, "Trying to add invalid types");
  MOZ_ASSERT(aValueToAdd.mType == this, "Unexpected source type");
  aDest.mU.mInt += aValueToAdd.mU.mInt * aCount;
  return NS_OK;
}

nsresult SMILIntegerType::ComputeDistance(const SMILValue& aFrom,
                                          const SMILValue& aTo,
                                          double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType, "Trying to compare different types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected source type");
  aDistance = fabs(double(aTo.mU.mInt - aFrom.mU.mInt));
  return NS_OK;
}

nsresult SMILIntegerType::Interpolate(const SMILValue& aStartVal,
                                      const SMILValue& aEndVal,
                                      double aUnitDistance,
                                      SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  const double startVal = double(aStartVal.mU.mInt);
  const double endVal = double(aEndVal.mU.mInt);
  const double currentVal = startVal + (endVal - startVal) * aUnitDistance;

  // When currentVal is exactly midway between its two nearest integers, we
  // jump to the "next" integer to provide simple, easy to remember and
  // consistent behaviour (from the SMIL author's point of view).

  if (startVal < endVal) {
    aResult.mU.mInt = int64_t(floor(currentVal + 0.5));  // round mid up
  } else {
    aResult.mU.mInt = int64_t(ceil(currentVal - 0.5));  // round mid down
  }

  return NS_OK;
}

}  // namespace mozilla
