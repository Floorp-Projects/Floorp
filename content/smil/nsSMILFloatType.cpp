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

#include "nsSMILFloatType.h"
#include "nsSMILValue.h"
#include "nsDebug.h"
#include <math.h>

/*static*/ nsSMILFloatType nsSMILFloatType::sSingleton;

void
nsSMILFloatType::Init(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mDouble = 0.0;
  aValue.mType = this;
}

void
nsSMILFloatType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mDouble = 0.0;
  aValue.mType      = &nsSMILNullType::sSingleton;
}

nsresult
nsSMILFloatType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");
  aDest.mU.mDouble = aSrc.mU.mDouble;
  return NS_OK;
}

bool
nsSMILFloatType::IsEqual(const nsSMILValue& aLeft,
                         const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mDouble == aRight.mU.mDouble;
}

nsresult
nsSMILFloatType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                     PRUint32 aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");
  aDest.mU.mDouble += aValueToAdd.mU.mDouble * aCount;
  return NS_OK;
}

nsresult
nsSMILFloatType::ComputeDistance(const nsSMILValue& aFrom,
                                 const nsSMILValue& aTo,
                                 double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");

  const double &from = aFrom.mU.mDouble;
  const double &to   = aTo.mU.mDouble;

  aDistance = fabs(to - from);

  return NS_OK;
}

nsresult
nsSMILFloatType::Interpolate(const nsSMILValue& aStartVal,
                             const nsSMILValue& aEndVal,
                             double aUnitDistance,
                             nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
      "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
      "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType   == this, "Unexpected result type");

  const double &startVal = aStartVal.mU.mDouble;
  const double &endVal   = aEndVal.mU.mDouble;

  aResult.mU.mDouble = (startVal + (endVal - startVal) * aUnitDistance);

  return NS_OK;
}
