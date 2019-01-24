/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILEnumType.h"

#include "mozilla/SMILValue.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {

void SMILEnumType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mUint = 0;
  aValue.mType = this;
}

void SMILEnumType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mUint = 0;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SMILEnumType::Assign(SMILValue& aDest, const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");
  aDest.mU.mUint = aSrc.mU.mUint;
  return NS_OK;
}

bool SMILEnumType::IsEqual(const SMILValue& aLeft,
                           const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mUint == aRight.mU.mUint;
}

nsresult SMILEnumType::Add(SMILValue& aDest, const SMILValue& aValueToAdd,
                           uint32_t aCount) const {
  MOZ_ASSERT(aValueToAdd.mType == aDest.mType, "Trying to add invalid types");
  MOZ_ASSERT(aValueToAdd.mType == this, "Unexpected source type");
  return NS_ERROR_FAILURE;  // enum values can't be added to each other
}

nsresult SMILEnumType::ComputeDistance(const SMILValue& aFrom,
                                       const SMILValue& aTo,
                                       double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType, "Trying to compare different types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected source type");
  return NS_ERROR_FAILURE;  // there is no concept of distance between enum
                            // values
}

nsresult SMILEnumType::Interpolate(const SMILValue& aStartVal,
                                   const SMILValue& aEndVal,
                                   double aUnitDistance,
                                   SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");
  return NS_ERROR_FAILURE;  // enum values do not interpolate
}

}  // namespace mozilla
