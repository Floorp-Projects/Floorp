/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Brian Birtles <birtles@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSMILValue.h"
#include "nsDebug.h"
#include <string.h>

//----------------------------------------------------------------------
// Public methods

nsSMILValue::nsSMILValue(const nsISMILType* aType)
  : mType(&nsSMILNullType::sSingleton)
{
  if (!aType) {
    NS_ERROR("Trying to construct nsSMILValue with null mType pointer");
    return;
  }

  InitAndCheckPostcondition(aType);
}

nsSMILValue::nsSMILValue(const nsSMILValue& aVal)
  : mType(&nsSMILNullType::sSingleton)
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

bool
nsSMILValue::operator==(const nsSMILValue& aVal) const
{
  if (&aVal == this)
    return PR_TRUE;

  return mType == aVal.mType && mType->IsEqual(*this, aVal);
}

void
nsSMILValue::Swap(nsSMILValue& aOther)
{
  nsSMILValue tmp;
  memcpy(&tmp,    &aOther, sizeof(nsSMILValue));  // tmp    = aOther
  memcpy(&aOther, this,    sizeof(nsSMILValue));  // aOther = this
  memcpy(this,    &tmp,    sizeof(nsSMILValue));  // this   = tmp

  // |tmp| is about to die -- we need to clear its mType, so that its
  // destructor doesn't muck with the data we just transferred out of it.
  tmp.mType = &nsSMILNullType::sSingleton;
}

nsresult
nsSMILValue::Add(const nsSMILValue& aValueToAdd, PRUint32 aCount)
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
  NS_ABORT_IF_FALSE(mType == aNewType,
                    "Post-condition of Init failed. nsSMILValue is invalid");
}
                
void
nsSMILValue::DestroyAndCheckPostcondition()
{
  mType->Destroy(*this);
  NS_ABORT_IF_FALSE(IsNull(), "Post-condition of Destroy failed. "
                    "nsSMILValue not null after destroying");
}

void
nsSMILValue::DestroyAndReinit(const nsISMILType* aNewType)
{
  DestroyAndCheckPostcondition();
  InitAndCheckPostcondition(aNewType);
}
