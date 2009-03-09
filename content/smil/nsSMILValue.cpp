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

nsSMILValue::nsSMILValue(const nsISMILType* aType)
: mU(),
  mType(&nsSMILNullType::sSingleton)
{
  if (!aType) return;

  nsresult rv = aType->Init(*this);
  NS_POSTCONDITION(mType == aType || (NS_FAILED(rv) && IsNull()),
    "Post-condition of Init failed. nsSMILValue is invalid.");
}

nsSMILValue::nsSMILValue(const nsSMILValue& aVal)
:
  mU(),
  mType(&nsSMILNullType::sSingleton)
{
  nsresult rv = aVal.mType->Init(*this);
  NS_POSTCONDITION(mType == aVal.mType || (NS_FAILED(rv) && IsNull()),
    "Post-condition of Init failed. nsSMILValue is invalid.");
  if (NS_FAILED(rv)) return;
  mType->Assign(*this, aVal);
}

const nsSMILValue&
nsSMILValue::operator=(const nsSMILValue& aVal)
{
  if (&aVal == this)
    return *this;

  if (mType != aVal.mType) {
    mType->Destroy(*this);
    NS_POSTCONDITION(IsNull(), "nsSMILValue not null after destroying");
    nsresult rv = aVal.mType->Init(*this);
    NS_POSTCONDITION(mType == aVal.mType || (NS_FAILED(rv) && IsNull()),
      "Post-condition of Init failed. nsSMILValue is invalid.");
    if (NS_FAILED(rv)) return *this;
  }

  mType->Assign(*this, aVal);

  return *this;
}

nsresult
nsSMILValue::Add(const nsSMILValue& aValueToAdd, PRUint32 aCount)
{
  if (aValueToAdd.IsNull()) return NS_OK;

  if (aValueToAdd.mType != mType) {
    NS_ERROR("Trying to add incompatible types.");
    return NS_ERROR_FAILURE;
  }

  return mType->Add(*this, aValueToAdd, aCount);
}

nsresult
nsSMILValue::SandwichAdd(const nsSMILValue& aValueToAdd)
{
  if (aValueToAdd.IsNull())
    return NS_OK;

  if (aValueToAdd.mType != mType) {
    NS_ERROR("Trying to add incompatible types.");
    return NS_ERROR_FAILURE;
  }

  return mType->SandwichAdd(*this, aValueToAdd);
}

nsresult
nsSMILValue::ComputeDistance(const nsSMILValue& aTo, double& aDistance) const
{
  if (aTo.mType != mType) {
    NS_ERROR("Trying to calculate distance between incompatible types.");
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
    NS_ERROR("Trying to interpolate between incompatible types.");
    return NS_ERROR_FAILURE;
  }

  if (aResult.mType != mType) {
    aResult.mType->Destroy(aResult);
    NS_POSTCONDITION(aResult.IsNull(), "nsSMILValue not null after destroying");
    nsresult rv = mType->Init(aResult);
    NS_POSTCONDITION(aResult.mType == mType
      || (NS_FAILED(rv) && aResult.IsNull()),
      "Post-condition of Init failed. nsSMILValue is invalid.");
    if (NS_FAILED(rv)) return rv;
  }

  return mType->Interpolate(*this, aEndVal, aUnitDistance, aResult);
}
