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

static PRBool
ArcFlagsDiffer(SVGPathDataAndOwner::const_iterator aPathData1,
               SVGPathDataAndOwner::const_iterator aPathData2)
{
  NS_ABORT_IF_FALSE
    (SVGPathSegUtils::IsArcType(SVGPathSegUtils::DecodeType(aPathData1[0])),
                                "ArcFlagsDiffer called with non-arc segment");
  NS_ABORT_IF_FALSE
    (SVGPathSegUtils::IsArcType(SVGPathSegUtils::DecodeType(aPathData2[0])),
                                "ArcFlagsDiffer called with non-arc segment");

  return aPathData1[4] != aPathData2[4] ||  // large arc flag
         aPathData1[5] != aPathData2[5];    // sweep flag
}

enum PathInterpolationResult {
  eCannotInterpolate,
  eRequiresConversion,
  eCanInterpolate
};

static PathInterpolationResult
CanInterpolate(const SVGPathDataAndOwner& aStart,
               const SVGPathDataAndOwner& aEnd)
{
  if (aStart.IsEmpty()) {
    return eCanInterpolate;
  }

  if (aStart.Length() != aEnd.Length()) {
    return eCannotInterpolate;
  }

  PathInterpolationResult result = eCanInterpolate;

  SVGPathDataAndOwner::const_iterator pStart = aStart.begin();
  SVGPathDataAndOwner::const_iterator pEnd = aEnd.begin();
  SVGPathDataAndOwner::const_iterator pStartDataEnd = aStart.end();
  SVGPathDataAndOwner::const_iterator pEndDataEnd = aEnd.end();

  while (pStart < pStartDataEnd && pEnd < pEndDataEnd) {
    PRUint32 startType = SVGPathSegUtils::DecodeType(*pStart);
    PRUint32 endType = SVGPathSegUtils::DecodeType(*pEnd);

    if (SVGPathSegUtils::IsArcType(startType) &&
        SVGPathSegUtils::IsArcType(endType) &&
        ArcFlagsDiffer(pStart, pEnd)) {
      return eCannotInterpolate;
    }

    if (startType != endType) {
      if (!SVGPathSegUtils::SameTypeModuloRelativeness(startType, endType)) {
        return eCannotInterpolate;
      }

      result = eRequiresConversion;
    }

    pStart += 1 + SVGPathSegUtils::ArgCountForType(startType);
    pEnd += 1 + SVGPathSegUtils::ArgCountForType(endType);
  }

  NS_ABORT_IF_FALSE(pStart <= pStartDataEnd && pEnd <= pEndDataEnd,
                    "Iterated past end of buffer! (Corrupt path data?)");

  if (pStart != pStartDataEnd || pEnd != pEndDataEnd) {
    return eCannotInterpolate;
  }

  return result;
}

static void
InterpolatePathSegmentData(SVGPathDataAndOwner::const_iterator& aStart,
                           SVGPathDataAndOwner::const_iterator& aEnd,
                           SVGPathDataAndOwner::iterator& aResult,
                           float aUnitDistance)
{
  PRUint32 startType = SVGPathSegUtils::DecodeType(*aStart);
  PRUint32 endType = SVGPathSegUtils::DecodeType(*aEnd);

  NS_ABORT_IF_FALSE
    (startType == endType,
     "InterpolatePathSegmentData expects segment types to be the same!");

  NS_ABORT_IF_FALSE
    (!(SVGPathSegUtils::IsArcType(startType) && ArcFlagsDiffer(aStart, aEnd)),
     "InterpolatePathSegmentData cannot interpolate arc segments with different flag values!");

  PRUint32 argCount = SVGPathSegUtils::ArgCountForType(startType);

  // Copy over segment type.
  *aResult++ = *aStart++;
  ++aEnd;

  // Interpolate the arguments.
  SVGPathDataAndOwner::const_iterator startSegEnd = aStart + argCount;
  while (aStart != startSegEnd) {
    *aResult = *aStart + (*aEnd - *aStart) * aUnitDistance;
    ++aStart;
    ++aEnd;
    ++aResult;
  }
}

enum RelativenessAdjustmentType {
  eAbsoluteToRelative,
  eRelativeToAbsolute
};

static inline void
AdjustSegmentForRelativeness(RelativenessAdjustmentType aAdjustmentType,
                             const SVGPathDataAndOwner::iterator& aSegmentToAdjust,
                             const SVGPathTraversalState& aState)
{
  if (aAdjustmentType == eAbsoluteToRelative) {
    aSegmentToAdjust[0] -= aState.pos.x;
    aSegmentToAdjust[1] -= aState.pos.y;
  } else {
    aSegmentToAdjust[0] += aState.pos.x;
    aSegmentToAdjust[1] += aState.pos.y;
  }
}

static void
ConvertPathSegmentData(SVGPathDataAndOwner::const_iterator& aStart,
                       SVGPathDataAndOwner::const_iterator& aEnd,
                       SVGPathDataAndOwner::iterator& aResult,
                       SVGPathTraversalState& aState)
{
  PRUint32 startType = SVGPathSegUtils::DecodeType(*aStart);
  PRUint32 endType = SVGPathSegUtils::DecodeType(*aEnd);

  PRUint32 segmentLengthIncludingType =
      1 + SVGPathSegUtils::ArgCountForType(startType);

  SVGPathDataAndOwner::const_iterator pResultSegmentBegin = aResult;

  if (startType == endType) {
    // No conversion need, just directly copy aStart.
    aEnd += segmentLengthIncludingType;
    while (segmentLengthIncludingType) {
      *aResult++ = *aStart++;
      --segmentLengthIncludingType;
    }
    SVGPathSegUtils::TraversePathSegment(pResultSegmentBegin, aState);
    return;
  }

  NS_ABORT_IF_FALSE
      (SVGPathSegUtils::SameTypeModuloRelativeness(startType, endType),
       "Incompatible path segment types passed to ConvertPathSegmentData!");

  RelativenessAdjustmentType adjustmentType =
    SVGPathSegUtils::IsRelativeType(startType) ? eRelativeToAbsolute
                                               : eAbsoluteToRelative;

  NS_ABORT_IF_FALSE
    (segmentLengthIncludingType ==
       1 + SVGPathSegUtils::ArgCountForType(endType),
     "Compatible path segment types for interpolation had different lengths!");

  aResult[0] = aEnd[0];

  switch (endType) {
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
      aResult[1] = aStart[1] +
        (adjustmentType == eRelativeToAbsolute ? 1 : -1) * aState.pos.x;
      break;
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
      aResult[1] = aStart[1] +
        (adjustmentType == eRelativeToAbsolute  ? 1 : -1) * aState.pos.y;
      break;
    case nsIDOMSVGPathSeg::PATHSEG_ARC_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_ARC_REL:
      aResult[1] = aStart[1];
      aResult[2] = aStart[2];
      aResult[3] = aStart[3];
      aResult[4] = aStart[4];
      aResult[5] = aStart[5];
      aResult[6] = aStart[6];
      aResult[7] = aStart[7];
      AdjustSegmentForRelativeness(adjustmentType, aResult + 6, aState);
      break;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
      aResult[5] = aStart[5];
      aResult[6] = aStart[6];
      AdjustSegmentForRelativeness(adjustmentType, aResult + 5, aState);
      // fall through
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
      aResult[3] = aStart[3];
      aResult[4] = aStart[4];
      AdjustSegmentForRelativeness(adjustmentType, aResult + 3, aState);
      // fall through
    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL:
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_REL:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
      aResult[1] = aStart[1];
      aResult[2] = aStart[2];
      AdjustSegmentForRelativeness(adjustmentType, aResult + 1, aState);
      break;
  }

  SVGPathSegUtils::TraversePathSegment(pResultSegmentBegin, aState);
  aStart += segmentLengthIncludingType;
  aEnd += segmentLengthIncludingType;
  aResult += segmentLengthIncludingType;
}

static void
ConvertAllPathSegmentData(SVGPathDataAndOwner::const_iterator aStart,
                          SVGPathDataAndOwner::const_iterator aStartDataEnd,
                          SVGPathDataAndOwner::const_iterator aEnd,
                          SVGPathDataAndOwner::const_iterator aEndDataEnd,
                          SVGPathDataAndOwner::iterator aResult)
{
  SVGPathTraversalState state;
  state.mode = SVGPathTraversalState::eUpdateOnlyStartAndCurrentPos;
  while (aStart < aStartDataEnd && aEnd < aEndDataEnd) {
    ConvertPathSegmentData(aStart, aEnd, aResult, state);
  }
  NS_ABORT_IF_FALSE(aStart == aStartDataEnd && aEnd == aEndDataEnd,
                    "Failed to convert all path segment data! (Corrupt?)");
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

  // Allow addition to empty dest.
  if (dest.IsEmpty()) {
    return dest.CopyFrom(valueToAdd);
  }

  PathInterpolationResult check = CanInterpolate(dest, valueToAdd);

  if (check == eCannotInterpolate) {
    // nsSVGUtils::ReportToConsole - can't add path segment lists with different
    // numbers of segments, with arcs with different flag values, or with
    // incompatible segment types.
    return NS_ERROR_FAILURE;
  }

  if (check == eRequiresConversion) {
    ConvertAllPathSegmentData(dest.begin(), dest.end(),
                              valueToAdd.begin(), valueToAdd.end(),
                              dest.begin());
  }

  PRUint32 i = 0;
  while (i < dest.Length()) {
    PRUint32 type = SVGPathSegUtils::DecodeType(dest[i]);
    i++;
    PRUint32 segEnd = i + SVGPathSegUtils::ArgCountForType(type);
    for (; i < segEnd; ++i) {
      dest[i] += valueToAdd[i] * aCount;
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

  PathInterpolationResult check = CanInterpolate(start, end); 

  if (check == eCannotInterpolate) {
    // nsSVGUtils::ReportToConsole - can't interpolate path segment lists with
    // different numbers of segments, with arcs with different flag values, or
    // with incompatible segment types.
    return NS_ERROR_FAILURE;
  }

  if (!result.SetLength(end.Length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (start.IsEmpty()) { // identity path
    PRUint32 i = 0;
    while (i < end.Length()) {
      PRUint32 type = SVGPathSegUtils::DecodeType(end[i]);
      result[i] = end[i];
      i++;
      PRUint32 segEnd = i + SVGPathSegUtils::ArgCountForType(type);
      if (SVGPathSegUtils::IsArcType(type)) {
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
    NS_ABORT_IF_FALSE(i == end.Length() && i == result.Length(),
                      "Very, very bad - path data corrupt");
  } else {
    SVGPathDataAndOwner::const_iterator pStart = start.begin();
    SVGPathDataAndOwner::const_iterator pStartDataEnd = start.end();
    SVGPathDataAndOwner::const_iterator pEnd = end.begin();
    SVGPathDataAndOwner::const_iterator pEndDataEnd = end.end();
    SVGPathDataAndOwner::iterator pResult = result.begin();

    if (check == eRequiresConversion) {
      ConvertAllPathSegmentData(pStart, pStartDataEnd, pEnd, pEndDataEnd,
                                pResult);
      pStart = pResult;
      pStartDataEnd = result.end();
    }

    while (pStart != pStartDataEnd && pEnd != pEndDataEnd) {
      InterpolatePathSegmentData(pStart, pEnd, pResult, aUnitDistance);
    }

    NS_ABORT_IF_FALSE(pStart == pStartDataEnd && pEnd == pEndDataEnd &&
                      pResult == result.end(),
                      "Very, very bad - path data corrupt");
  }

  return NS_OK;
}
