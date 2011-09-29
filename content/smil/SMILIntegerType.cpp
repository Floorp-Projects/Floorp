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
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "SMILIntegerType.h"
#include "nsSMILValue.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {

/*static*/ SMILIntegerType SMILIntegerType::sSingleton;

void
SMILIntegerType::Init(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mInt = 0;
  aValue.mType = this;
}

void
SMILIntegerType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  aValue.mU.mInt = 0;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
SMILIntegerType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");
  aDest.mU.mInt = aSrc.mU.mInt;
  return NS_OK;
}

bool
SMILIntegerType::IsEqual(const nsSMILValue& aLeft,
                         const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return aLeft.mU.mInt == aRight.mU.mInt;
}

nsresult
SMILIntegerType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                     PRUint32 aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");
  aDest.mU.mInt += aValueToAdd.mU.mInt * aCount;
  return NS_OK;
}

nsresult
SMILIntegerType::ComputeDistance(const nsSMILValue& aFrom,
                                 const nsSMILValue& aTo,
                                 double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");
  aDistance = fabs(double(aTo.mU.mInt - aFrom.mU.mInt));
  return NS_OK;
}

nsresult
SMILIntegerType::Interpolate(const nsSMILValue& aStartVal,
                             const nsSMILValue& aEndVal,
                             double aUnitDistance,
                             nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType   == this, "Unexpected result type");

  const double startVal   = double(aStartVal.mU.mInt);
  const double endVal     = double(aEndVal.mU.mInt);
  const double currentVal = startVal + (endVal - startVal) * aUnitDistance;

  // When currentVal is exactly midway between its two nearest integers, we
  // jump to the "next" integer to provide simple, easy to remember and
  // consistent behaviour (from the SMIL author's point of view).

  if (startVal < endVal) {
    aResult.mU.mInt = PRInt64(floor(currentVal + 0.5)); // round mid up
  } else {
    aResult.mU.mInt = PRInt64(ceil(currentVal - 0.5)); // round mid down
  }

  return NS_OK;
}

} // namespace mozilla
