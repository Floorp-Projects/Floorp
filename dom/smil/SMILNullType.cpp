/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILNullType.h"
#include "nsSMILValue.h"
#include "nsDebug.h"

namespace mozilla {

/*static*/ SMILNullType* SMILNullType::Singleton() {
  static SMILNullType sSingleton;
  return &sSingleton;
}

nsresult SMILNullType::Assign(nsSMILValue& aDest,
                              const nsSMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aSrc.mType == this, "Unexpected source type");
  aDest.mU = aSrc.mU;
  aDest.mType = Singleton();
  return NS_OK;
}

bool SMILNullType::IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return true;  // All null-typed values are equivalent.
}

nsresult SMILNullType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                           uint32_t aCount) const {
  MOZ_ASSERT_UNREACHABLE("Adding NULL type");
  return NS_ERROR_FAILURE;
}

nsresult SMILNullType::ComputeDistance(const nsSMILValue& aFrom,
                                       const nsSMILValue& aTo,
                                       double& aDistance) const {
  MOZ_ASSERT_UNREACHABLE("Computing distance for NULL type");
  return NS_ERROR_FAILURE;
}

nsresult SMILNullType::Interpolate(const nsSMILValue& aStartVal,
                                   const nsSMILValue& aEndVal,
                                   double aUnitDistance,
                                   nsSMILValue& aResult) const {
  MOZ_ASSERT_UNREACHABLE("Interpolating NULL type");
  return NS_ERROR_FAILURE;
}

}  // namespace mozilla
