/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILValue.h"
#include "nsDebug.h"
#include <string.h>

//----------------------------------------------------------------------
// Public methods

nsSMILValue::nsSMILValue(const nsISMILType* aType)
  : mType(nsSMILNullType::Singleton())
{
  if (!aType) {
    NS_ERROR("Trying to construct nsSMILValue with null mType pointer");
    return;
  }

  InitAndCheckPostcondition(aType);
}

nsSMILValue::nsSMILValue(const nsSMILValue& aVal)
  : mType(nsSMILNullType::Singleton())
{
  InitAndCheckPostcondition(aVal.mType);
  mType->Assign(*this, aVal);
}

const nsSMILValue&
nsSMILValue::operator=(const nsSMILValue& aVal)
{
  if (&aVal == this)
    return *this;

  if (mType != aVal.mType) {
    DestroyAndReinit(aVal.mType);
  }

  mType->Assign(*this, aVal);

  return *this;
}

// Move constructor / reassignment operator:
nsSMILValue::nsSMILValue(nsSMILValue&& aVal)
  : mU(aVal.mU), // Copying union is only OK because we clear aVal.mType below.
    mType(aVal.mType)
{
  // Leave aVal with a null type, so that it's safely destructible (and won't
  // mess with anything referenced by its union, which we've copied).
  aVal.mType = nsSMILNullType::Singleton();
}

nsSMILValue&
nsSMILValue::operator=(nsSMILValue&& aVal)
{
  if (!IsNull()) {
    // Clean up any data we're currently tracking.
    DestroyAndCheckPostcondition();
  }

  // Copy the union (which could include a pointer to external memory) & mType:
  mU = aVal.mU;
  mType = aVal.mType;

  // Leave aVal with a null type, so that it's safely destructible (and won't
  // mess with anything referenced by its union, which we've now copied).
  aVal.mType = nsSMILNullType::Singleton();

  return *this;
}

bool
nsSMILValue::operator==(const nsSMILValue& aVal) const
{
  if (&aVal == this)
    return true;

  return mType == aVal.mType && mType->IsEqual(*this, aVal);
}

nsresult
nsSMILValue::Add(const nsSMILValue& aValueToAdd, uint32_t aCount)
{
  if (aValueToAdd.mType != mType) {
    NS_ERROR("Trying to add incompatible types");
    return NS_ERROR_FAILURE;
  }

  return mType->Add(*this, aValueToAdd, aCount);
}

nsresult
nsSMILValue::SandwichAdd(const nsSMILValue& aValueToAdd)
{
  if (aValueToAdd.mType != mType) {
    NS_ERROR("Trying to add incompatible types");
    return NS_ERROR_FAILURE;
  }

  return mType->SandwichAdd(*this, aValueToAdd);
}

nsresult
nsSMILValue::ComputeDistance(const nsSMILValue& aTo, double& aDistance) const
{
  if (aTo.mType != mType) {
    NS_ERROR("Trying to calculate distance between incompatible types");
    return NS_ERROR_FAILURE;
  }

  return mType->ComputeDistance(*this, aTo, aDistance);
}

nsresult
nsSMILValue::Interpolate(const nsSMILValue& aEndVal,
                         double aUnitDistance,
                         nsSMILValue& aResult) const
{
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

// Wrappers for nsISMILType::Init & ::Destroy that verify their postconditions
void
nsSMILValue::InitAndCheckPostcondition(const nsISMILType* aNewType)
{
  aNewType->Init(*this);
  MOZ_ASSERT(mType == aNewType,
             "Post-condition of Init failed. nsSMILValue is invalid");
}
                
void
nsSMILValue::DestroyAndCheckPostcondition()
{
  mType->Destroy(*this);
  MOZ_ASSERT(IsNull(),
             "Post-condition of Destroy failed. "
             "nsSMILValue not null after destroying");
}

void
nsSMILValue::DestroyAndReinit(const nsISMILType* aNewType)
{
  DestroyAndCheckPostcondition();
  InitAndCheckPostcondition(aNewType);
}
