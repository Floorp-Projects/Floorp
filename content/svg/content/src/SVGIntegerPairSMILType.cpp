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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is Robert Longson.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "SVGIntegerPairSMILType.h"
#include "nsSMILValue.h"
#include "nsMathUtils.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {
  
/*static*/ SVGIntegerPairSMILType SVGIntegerPairSMILType::sSingleton;

void
SVGIntegerPairSMILType::Init(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mIntPair[0] = 0;
  aValue.mU.mIntPair[1] = 0;
  aValue.mType = this;
}

void
SVGIntegerPairSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mIntPair[0] = 0;
  aValue.mU.mIntPair[1] = 0;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
SVGIntegerPairSMILType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");

  aDest.mU.mIntPair[0] = aSrc.mU.mIntPair[0];
  aDest.mU.mIntPair[1] = aSrc.mU.mIntPair[1];
  return NS_OK;
}

bool
SVGIntegerPairSMILType::IsEqual(const nsSMILValue& aLeft,
                                const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mIntPair[0] == aRight.mU.mIntPair[0] &&
         aLeft.mU.mIntPair[1] == aRight.mU.mIntPair[1];
}

nsresult
SVGIntegerPairSMILType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                            PRUint32 aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");

  aDest.mU.mIntPair[0] += aValueToAdd.mU.mIntPair[0] * aCount;
  aDest.mU.mIntPair[1] += aValueToAdd.mU.mIntPair[1] * aCount;

  return NS_OK;
}

nsresult
SVGIntegerPairSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                        const nsSMILValue& aTo,
                                        double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");

  double delta[2];
  delta[0] = aTo.mU.mIntPair[0] - aFrom.mU.mIntPair[0];
  delta[1] = aTo.mU.mIntPair[1] - aFrom.mU.mIntPair[1];

  aDistance = NS_hypot(delta[0], delta[1]);
  return NS_OK;
}

nsresult
SVGIntegerPairSMILType::Interpolate(const nsSMILValue& aStartVal,
                                    const nsSMILValue& aEndVal,
                                    double aUnitDistance,
                                    nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType == this, "Unexpected result type");

  double currentVal[2];
  currentVal[0] = aStartVal.mU.mIntPair[0] +
                  (aEndVal.mU.mIntPair[0] - aStartVal.mU.mIntPair[0]) * aUnitDistance;
  currentVal[1] = aStartVal.mU.mIntPair[1] +
                  (aEndVal.mU.mIntPair[1] - aStartVal.mU.mIntPair[1]) * aUnitDistance;

  aResult.mU.mIntPair[0] = NS_lround(currentVal[0]);
  aResult.mU.mIntPair[1] = NS_lround(currentVal[1]);
  return NS_OK;
}

} // namespace mozilla
