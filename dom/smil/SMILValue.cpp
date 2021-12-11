/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILValue.h"

#include "nsDebug.h"
#include <string.h>

namespace mozilla {

//----------------------------------------------------------------------
// Public methods

SMILValue::SMILValue(const SMILType* aType) : mType(SMILNullType::Singleton()) {
  mU.mBool = false;
  if (!aType) {
    NS_ERROR("Trying to construct SMILValue with null mType pointer");
    return;
  }

  InitAndCheckPostcondition(aType);
}

SMILValue::SMILValue(const SMILValue& aVal) : mType(SMILNullType::Singleton()) {
  InitAndCheckPostcondition(aVal.mType);
  mType->Assign(*this, aVal);
}

const SMILValue& SMILValue::operator=(const SMILValue& aVal) {
  if (&aVal == this) return *this;

  if (mType != aVal.mType) {
    DestroyAndReinit(aVal.mType);
  }

  mType->Assign(*this, aVal);

  return *this;
}

// Move constructor / reassignment operator:
SMILValue::SMILValue(SMILValue&& aVal) noexcept
    : mU(aVal.mU),  // Copying union is only OK because we clear aVal.mType
                    // below.
      mType(aVal.mType) {
  // Leave aVal with a null type, so that it's safely destructible (and won't
  // mess with anything referenced by its union, which we've copied).
  aVal.mType = SMILNullType::Singleton();
}

SMILValue& SMILValue::operator=(SMILValue&& aVal) noexcept {
  if (!IsNull()) {
    // Clean up any data we're currently tracking.
    DestroyAndCheckPostcondition();
  }

  // Copy the union (which could include a pointer to external memory) & mType:
  mU = aVal.mU;
  mType = aVal.mType;

  // Leave aVal with a null type, so that it's safely destructible (and won't
  // mess with anything referenced by its union, which we've now copied).
  aVal.mType = SMILNullType::Singleton();

  return *this;
}

bool SMILValue::operator==(const SMILValue& aVal) const {
  if (&aVal == this) return true;

  return mType == aVal.mType && mType->IsEqual(*this, aVal);
}

nsresult SMILValue::Add(const SMILValue& aValueToAdd, uint32_t aCount) {
  if (aValueToAdd.mType != mType) {
    NS_ERROR("Trying to add incompatible types");
    return NS_ERROR_FAILURE;
  }

  return mType->Add(*this, aValueToAdd, aCount);
}

nsresult SMILValue::SandwichAdd(const SMILValue& aValueToAdd) {
  if (aValueToAdd.mType != mType) {
    NS_ERROR("Trying to add incompatible types");
    return NS_ERROR_FAILURE;
  }

  return mType->SandwichAdd(*this, aValueToAdd);
}

nsresult SMILValue::ComputeDistance(const SMILValue& aTo,
                                    double& aDistance) const {
  if (aTo.mType != mType) {
    NS_ERROR("Trying to calculate distance between incompatible types");
    return NS_ERROR_FAILURE;
  }

  return mType->ComputeDistance(*this, aTo, aDistance);
}

nsresult SMILValue::Interpolate(const SMILValue& aEndVal, double aUnitDistance,
                                SMILValue& aResult) const {
  if (aEndVal.mType != mType) {
    NS_ERROR("Trying to interpolate between incompatible types");
    return NS_ERROR_FAILURE;
  }

  if (aResult.mType != mType) {
    // Outparam has wrong type
    aResult.DestroyAndReinit(mType);
  }

  return mType->Interpolate(*this, aEndVal, aUnitDistance, aResult);
}

//----------------------------------------------------------------------
// Helper methods

// Wrappers for SMILType::Init & ::Destroy that verify their postconditions
void SMILValue::InitAndCheckPostcondition(const SMILType* aNewType) {
  aNewType->Init(*this);
  MOZ_ASSERT(mType == aNewType,
             "Post-condition of Init failed. SMILValue is invalid");
}

void SMILValue::DestroyAndCheckPostcondition() {
  mType->Destroy(*this);
  MOZ_ASSERT(IsNull(),
             "Post-condition of Destroy failed. "
             "SMILValue not null after destroying");
}

void SMILValue::DestroyAndReinit(const SMILType* aNewType) {
  DestroyAndCheckPostcondition();
  InitAndCheckPostcondition(aNewType);
}

}  // namespace mozilla
