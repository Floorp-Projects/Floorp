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

#include "SVGPathSegListSMILType.h"
#include "nsSMILValue.h"
#include "SVGPathData.h"
#include <math.h>

using namespace mozilla;

/*static*/ SVGPathSegListSMILType SVGPathSegListSMILType::sSingleton;

//----------------------------------------------------------------------
// nsISMILType implementation

void
SVGPathSegListSMILType::Init(nsSMILValue &aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mPtr = new SVGPathDataAndOwner();
  aValue.mType = this;
}

void
SVGPathSegListSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<SVGPathDataAndOwner*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nsnull;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
SVGPathSegListSMILType::Assign(nsSMILValue& aDest,
                               const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");

  const SVGPathDataAndOwner* src =
    static_cast<const SVGPathDataAndOwner*>(aSrc.mU.mPtr);
  SVGPathDataAndOwner* dest =
    static_cast<SVGPathDataAndOwner*>(aDest.mU.mPtr);

  return dest->CopyFrom(*src);
}

PRBool
SVGPathSegListSMILType::IsEqual(const nsSMILValue& aLeft,
                                const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return *static_cast<const SVGPathDataAndOwner*>(aLeft.mU.mPtr) ==
         *static_cast<const SVGPathDataAndOwner*>(aRight.mU.mPtr);
}

nsresult
SVGPathSegListSMILType::Add(nsSMILValue& aDest,
                            const nsSMILValue& aValueToAdd,
                            PRUint32 aCount) const
{
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL type");
  NS_PRECONDITION(aValueToAdd.mType == this, "Incompatible SMIL type");

  SVGPathDataAndOwner& dest =
    *static_cast<SVGPathDataAndOwner*>(aDest.mU.mPtr);
  const SVGPathDataAndOwner& valueToAdd =
    *static_cast<const SVGPathDataAndOwner*>(aValueToAdd.mU.mPtr);

  if (dest.Length() != valueToAdd.Length()) {
    // Allow addition to empty dest:
    if (dest.Length() == 0) {
      return dest.CopyFrom(valueToAdd);
    }
    // For now we only support animation to a list with the same number of
    // items (and with the same segment types).
    // nsSVGUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }

  PRUint32 i = 0;
  while (i < dest.Length()) {
    PRUint32 type = SVGPathSegUtils::DecodeType(dest[i]);
    if (type != SVGPathSegUtils::DecodeType(valueToAdd[i])) {
      // nsSVGUtils::ReportToConsole - can't yet animate between different
      // types, although it would make sense to allow animation between
      // some.
      return NS_ERROR_FAILURE;
    }
    i++;
    if ((type == nsIDOMSVGPathSeg::PATHSEG_ARC_ABS ||
         type == nsIDOMSVGPathSeg::PATHSEG_ARC_REL) &&
        (dest[i+3] != valueToAdd[i+3] || dest[i+4] != valueToAdd[i+4])) {
      // boolean args largeArcFlag and sweepFlag must be the same
      return NS_ERROR_FAILURE;
    }
    PRUint32 segEnd = i + SVGPathSegUtils::ArgCountForType(type);
    for (; i < segEnd; ++i) {
      dest[i] += valueToAdd[i];
    }
  }

  NS_ABORT_IF_FALSE(i == dest.Length(), "Very, very bad - path data corrupt");

  // For now we only support pure 'to' animation.
  // nsSVGUtils::ReportToConsole
  return NS_OK;
}

nsresult
SVGPathSegListSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                        const nsSMILValue& aTo,
                                        double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == this, "Unexpected SMIL type");
  NS_PRECONDITION(aTo.mType == this, "Incompatible SMIL type");

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=522306#c18

  // nsSVGUtils::ReportToConsole
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
SVGPathSegListSMILType::Interpolate(const nsSMILValue& aStartVal,
                                    const nsSMILValue& aEndVal,
                                    double aUnitDistance,
                                    nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType == this, "Unexpected result type");

  const SVGPathDataAndOwner& start =
    *static_cast<const SVGPathDataAndOwner*>(aStartVal.mU.mPtr);
  const SVGPathDataAndOwner& end =
    *static_cast<const SVGPathDataAndOwner*>(aEndVal.mU.mPtr);
  SVGPathDataAndOwner& result =
    *static_cast<SVGPathDataAndOwner*>(aResult.mU.mPtr);

  if (start.Length() != end.Length() && start.Length() != 0) {
    // For now we only support animation to a list with the same number of
    // items (and with the same segment types).
    // nsSVGUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }

  if (!result.SetLength(end.Length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 i = 0;

  if (start.Length() == 0) { // identity path
    while (i < end.Length()) {
      PRUint32 type = SVGPathSegUtils::DecodeType(end[i]);
      result[i] = end[i];
      i++;
      PRUint32 segEnd = i + SVGPathSegUtils::ArgCountForType(type);
      if ((type == nsIDOMSVGPathSeg::PATHSEG_ARC_ABS ||
           type == nsIDOMSVGPathSeg::PATHSEG_ARC_REL)) {
        result[i] = end[i] * aUnitDistance;
        result[i+1] = end[i+1] * aUnitDistance;
        result[i+2] = end[i+2] * aUnitDistance;
        // boolean args largeArcFlag and sweepFlag must be the same
        result[i+3] = end[i+3];
        result[i+4] = end[i+4];
        result[i+5] = end[i+5] * aUnitDistance;
        result[i+6] = end[i+6] * aUnitDistance;
        i = segEnd;
      } else {
        for (; i < segEnd; ++i) {
          result[i] = end[i] * aUnitDistance;
        }
      }
    }
  } else {
    while (i < end.Length()) {
      PRUint32 type = SVGPathSegUtils::DecodeType(end[i]);
      if (type != SVGPathSegUtils::DecodeType(start[i])) {
        // nsSVGUtils::ReportToConsole - can't yet interpolate between different
        // types, although it would make sense to allow interpolation between
        // some.
        return NS_ERROR_FAILURE;
      }
      result[i] = end[i];
      i++;
      if ((type == nsIDOMSVGPathSeg::PATHSEG_ARC_ABS ||
           type == nsIDOMSVGPathSeg::PATHSEG_ARC_REL) &&
          (start[i+3] != end[i+3] || start[i+4] != end[i+4])) {
        // boolean args largeArcFlag and sweepFlag must be the same
        return NS_ERROR_FAILURE;
      }
      PRUint32 segEnd = i + SVGPathSegUtils::ArgCountForType(type);
      for (; i < segEnd; ++i) {
        result[i] = start[i] + (end[i] - start[i]) * aUnitDistance;
      }
    }
  }

  NS_ABORT_IF_FALSE(i == end.Length(), "Very, very bad - path data corrupt");

  return NS_OK;
}

