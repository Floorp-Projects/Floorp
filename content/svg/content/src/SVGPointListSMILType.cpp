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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "SVGPointListSMILType.h"
#include "nsSMILValue.h"
#include "SVGPointList.h"
#include "nsMathUtils.h"
#include <math.h>

namespace mozilla {

/*static*/ SVGPointListSMILType SVGPointListSMILType::sSingleton;

//----------------------------------------------------------------------
// nsISMILType implementation

void
SVGPointListSMILType::Init(nsSMILValue &aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");

  SVGPointListAndInfo* pointList = new SVGPointListAndInfo();

  aValue.mU.mPtr = pointList;
  aValue.mType = this;
}

void
SVGPointListSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<SVGPointListAndInfo*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nsnull;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
SVGPointListSMILType::Assign(nsSMILValue& aDest,
                              const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");

  const SVGPointListAndInfo* src =
    static_cast<const SVGPointListAndInfo*>(aSrc.mU.mPtr);
  SVGPointListAndInfo* dest =
    static_cast<SVGPointListAndInfo*>(aDest.mU.mPtr);

  return dest->CopyFrom(*src);
}

bool
SVGPointListSMILType::IsEqual(const nsSMILValue& aLeft,
                               const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return *static_cast<const SVGPointListAndInfo*>(aLeft.mU.mPtr) ==
         *static_cast<const SVGPointListAndInfo*>(aRight.mU.mPtr);
}

nsresult
SVGPointListSMILType::Add(nsSMILValue& aDest,
                          const nsSMILValue& aValueToAdd,
                          PRUint32 aCount) const
{
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL type");
  NS_PRECONDITION(aValueToAdd.mType == this, "Incompatible SMIL type");

  SVGPointListAndInfo& dest =
    *static_cast<SVGPointListAndInfo*>(aDest.mU.mPtr);
  const SVGPointListAndInfo& valueToAdd =
    *static_cast<const SVGPointListAndInfo*>(aValueToAdd.mU.mPtr);

  NS_ABORT_IF_FALSE(dest.Element() || valueToAdd.Element(),
                    "Target element propagation failure");

  if (!valueToAdd.Element()) {
    NS_ABORT_IF_FALSE(valueToAdd.Length() == 0,
                      "Not identity value - target element propagation failure");
    return NS_OK;
  }
  if (!dest.Element()) {
    NS_ABORT_IF_FALSE(dest.Length() == 0,
                      "Not identity value - target element propagation failure");
    if (!dest.SetLength(valueToAdd.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (PRUint32 i = 0; i < dest.Length(); ++i) {
      dest[i] = aCount * valueToAdd[i];
    }
    dest.SetInfo(valueToAdd.Element()); // propagate target element info!
    return NS_OK;
  }
  NS_ABORT_IF_FALSE(dest.Element() == valueToAdd.Element(),
                    "adding values from different elements...?");
  if (dest.Length() != valueToAdd.Length()) {
    // For now we only support animation between lists with the same number of
    // items. nsSVGUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }
  for (PRUint32 i = 0; i < dest.Length(); ++i) {
    dest[i] += aCount * valueToAdd[i];
  }
  dest.SetInfo(valueToAdd.Element()); // propagate target element info!
  return NS_OK;
}

nsresult
SVGPointListSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                      const nsSMILValue& aTo,
                                      double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == this, "Unexpected SMIL type");
  NS_PRECONDITION(aTo.mType == this, "Incompatible SMIL type");

  const SVGPointListAndInfo& from =
    *static_cast<const SVGPointListAndInfo*>(aFrom.mU.mPtr);
  const SVGPointListAndInfo& to =
    *static_cast<const SVGPointListAndInfo*>(aTo.mU.mPtr);

  if (from.Length() != to.Length()) {
    // Lists in the 'values' attribute must have the same length.
    // nsSVGUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }

  // We return the root of the sum of the squares of the distances between the
  // points at each corresponding index.

  double total = 0.0;

  for (PRUint32 i = 0; i < to.Length(); ++i) {
    double dx = to[i].mX - from[i].mX;
    double dy = to[i].mY - from[i].mY;
    total += dx * dx + dy * dy;
  }
  double distance = sqrt(total);
  if (!NS_finite(distance)) {
    return NS_ERROR_FAILURE;
  }
  aDistance = distance;

  return NS_OK;
}

nsresult
SVGPointListSMILType::Interpolate(const nsSMILValue& aStartVal,
                                  const nsSMILValue& aEndVal,
                                  double aUnitDistance,
                                  nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType == this, "Unexpected result type");

  const SVGPointListAndInfo& start =
    *static_cast<const SVGPointListAndInfo*>(aStartVal.mU.mPtr);
  const SVGPointListAndInfo& end =
    *static_cast<const SVGPointListAndInfo*>(aEndVal.mU.mPtr);
  SVGPointListAndInfo& result =
    *static_cast<SVGPointListAndInfo*>(aResult.mU.mPtr);

  NS_ABORT_IF_FALSE(end.Element(), "Can't propagate target element");
  NS_ABORT_IF_FALSE(start.Element() == end.Element() || !start.Element(),
                    "Different target elements");

  if (start.Element() && // 'start' is not an "identity" value
      start.Length() != end.Length()) {
    // For now we only support animation between lists of the same length.
    // nsSVGUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }
  if (!result.SetLength(end.Length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  result.SetInfo(end.Element()); // propagate target element info!

  if (start.Length() != end.Length()) {
    NS_ABORT_IF_FALSE(start.Length() == 0, "Not an identity value");
    for (PRUint32 i = 0; i < end.Length(); ++i) {
      result[i] = aUnitDistance * end[i];
    }
    return NS_OK;
  }
  for (PRUint32 i = 0; i < end.Length(); ++i) {
    result[i] = start[i] + (end[i] - start[i]) * aUnitDistance;
  }
  return NS_OK;
}

} // namespace mozilla
