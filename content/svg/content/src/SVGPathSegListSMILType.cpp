/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "mozilla/DebugOnly.h"
#include "mozilla/Util.h"

#include "SVGPathSegListSMILType.h"
#include "nsSMILValue.h"
#include "SVGPathSegUtils.h"
#include "SVGPathData.h"

// Indices of boolean flags within 'arc' segment chunks in path-data arrays
// (where '0' would correspond to the index of the encoded segment type):
#define LARGE_ARC_FLAG_IDX 4
#define SWEEP_FLAG_IDX     5

namespace mozilla {

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
  aValue.mU.mPtr = nullptr;
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

bool
SVGPathSegListSMILType::IsEqual(const nsSMILValue& aLeft,
                                const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return *static_cast<const SVGPathDataAndOwner*>(aLeft.mU.mPtr) ==
         *static_cast<const SVGPathDataAndOwner*>(aRight.mU.mPtr);
}

static bool
ArcFlagsDiffer(SVGPathDataAndOwner::const_iterator aPathData1,
               SVGPathDataAndOwner::const_iterator aPathData2)
{
  NS_ABORT_IF_FALSE
    (SVGPathSegUtils::IsArcType(SVGPathSegUtils::DecodeType(aPathData1[0])),
                                "ArcFlagsDiffer called with non-arc segment");
  NS_ABORT_IF_FALSE
    (SVGPathSegUtils::IsArcType(SVGPathSegUtils::DecodeType(aPathData2[0])),
                                "ArcFlagsDiffer called with non-arc segment");

  return aPathData1[LARGE_ARC_FLAG_IDX] != aPathData2[LARGE_ARC_FLAG_IDX] ||
         aPathData1[SWEEP_FLAG_IDX]     != aPathData2[SWEEP_FLAG_IDX];
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
  if (aStart.IsIdentity()) {
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
    uint32_t startType = SVGPathSegUtils::DecodeType(*pStart);
    uint32_t endType = SVGPathSegUtils::DecodeType(*pEnd);

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

/**
 * Helper function for AddWeightedPathSegLists, to add multiples of two
 * path-segments of the same type.
 *
 * NOTE: |aSeg1| is allowed to be nullptr, so we use |aSeg2| as the
 * authoritative source of things like segment-type and boolean arc flags.
 *
 * @param aCoeff1    The coefficient to use on the first segment.
 * @param aSeg1      An iterator pointing to the first segment.  This can be
 *                   null, which is treated as identity (zero).
 * @param aCoeff2    The coefficient to use on the second segment.
 * @param aSeg2      An iterator pointing to the second segment.
 * @param [out] aResultSeg An iterator pointing to where we should write the
 *                         result of this operation.
 */
static inline void
AddWeightedPathSegs(double aCoeff1,
                    SVGPathDataAndOwner::const_iterator& aSeg1,
                    double aCoeff2,
                    SVGPathDataAndOwner::const_iterator& aSeg2,
                    SVGPathDataAndOwner::iterator& aResultSeg)
{
  NS_ABORT_IF_FALSE(aSeg2, "2nd segment must be non-null");
  NS_ABORT_IF_FALSE(aResultSeg, "result segment must be non-null");

  uint32_t segType = SVGPathSegUtils::DecodeType(aSeg2[0]);
  NS_ABORT_IF_FALSE(!aSeg1 || SVGPathSegUtils::DecodeType(*aSeg1) == segType,
                    "unexpected segment type");

  // FIRST: Directly copy the arguments that don't make sense to add.
  aResultSeg[0] = aSeg2[0];  // encoded segment type

  bool isArcType = SVGPathSegUtils::IsArcType(segType);
  if (isArcType) {
    // Copy boolean arc flags.
    NS_ABORT_IF_FALSE(!aSeg1 || !ArcFlagsDiffer(aSeg1, aSeg2),
                      "Expecting arc flags to match");
    aResultSeg[LARGE_ARC_FLAG_IDX] = aSeg2[LARGE_ARC_FLAG_IDX];
    aResultSeg[SWEEP_FLAG_IDX]     = aSeg2[SWEEP_FLAG_IDX];
  }

  // SECOND: Add the arguments that are supposed to be added.
  // (The 1's below are to account for segment type)
  uint32_t numArgs = SVGPathSegUtils::ArgCountForType(segType);
  for (uint32_t i = 1; i < 1 + numArgs; ++i) {
     // Need to skip arc flags for arc-type segments. (already handled them)
    if (!(isArcType && (i == LARGE_ARC_FLAG_IDX || i == SWEEP_FLAG_IDX))) {
      aResultSeg[i] = (aSeg1 ? aCoeff1 * aSeg1[i] : 0.0) + aCoeff2 * aSeg2[i];
    }
  }

  // FINALLY: Shift iterators forward. ("1+" is to include seg-type)
  if (aSeg1) {
    aSeg1 += 1 + numArgs;
  }
  aSeg2 += 1 + numArgs;
  aResultSeg += 1 + numArgs;
}

/**
 * Helper function for Add & Interpolate, to add multiples of two path-segment
 * lists.
 *
 * NOTE: aList1 and aList2 are assumed to have their segment-types and
 * segment-count match exactly (unless aList1 is an identity value).
 *
 * NOTE: aResult, the output list, is expected to either be an identity value
 * (in which case we'll grow it) *or* to already have the exactly right length
 * (e.g. in cases where aList1 and aResult are actually the same list).
 *
 * @param aCoeff1    The coefficient to use on the first path segment list.
 * @param aList1     The first path segment list. Allowed to be identity.
 * @param aCoeff2    The coefficient to use on the second path segment list.
 * @param aList2     The second path segment list.
 * @param [out] aResultSeg The resulting path segment list. Allowed to be
 *                         identity, in which case we'll grow it to the right
 *                         size. Also allowed to be the same list as aList1.
 */
static void
AddWeightedPathSegLists(double aCoeff1, const SVGPathDataAndOwner& aList1,
                        double aCoeff2, const SVGPathDataAndOwner& aList2,
                        SVGPathDataAndOwner& aResult)
{
  NS_ABORT_IF_FALSE(aCoeff1 >= 0.0 && aCoeff2 >= 0.0,
                    "expecting non-negative coefficients");
  NS_ABORT_IF_FALSE(!aList2.IsIdentity(),
                    "expecting 2nd list to be non-identity");
  NS_ABORT_IF_FALSE(aList1.IsIdentity() || aList1.Length() == aList2.Length(),
                    "expecting 1st list to be identity or to have same "
                    "length as 2nd list");
  NS_ABORT_IF_FALSE(aResult.IsIdentity() || aResult.Length() == aList2.Length(),
                    "expecting result list to be identity or to have same "
                    "length as 2nd list");

  SVGPathDataAndOwner::const_iterator iter1, end1;
  if (aList1.IsIdentity()) {
    iter1 = end1 = nullptr; // indicate that this is an identity list
  } else {
    iter1 = aList1.begin();
    end1 = aList1.end();
  }
  SVGPathDataAndOwner::const_iterator iter2 = aList2.begin();
  SVGPathDataAndOwner::const_iterator end2 = aList2.end();

  // Grow |aResult| if necessary. (NOTE: It's possible that aResult and aList1
  // are the same list, so this may implicitly resize aList1. That's fine,
  // because in that case, we will have already set iter1 to nullptr above, to
  // record that our first operand is an identity value.)
  if (aResult.IsIdentity()) {
    DebugOnly<bool> success = aResult.SetLength(aList2.Length());
    NS_ABORT_IF_FALSE(success, "infallible nsTArray::SetLength should succeed");
    aResult.SetElement(aList2.Element()); // propagate target element info!
  }

  SVGPathDataAndOwner::iterator resultIter = aResult.begin();

  while ((!iter1 || iter1 != end1) &&
         iter2 != end2) {
    AddWeightedPathSegs(aCoeff1, iter1,
                        aCoeff2, iter2,
                        resultIter);
  }
  NS_ABORT_IF_FALSE((!iter1 || iter1 == end1) &&
                    iter2 == end2 &&
                    resultIter == aResult.end(),
                    "Very, very bad - path data corrupt");
}

static void
ConvertPathSegmentData(SVGPathDataAndOwner::const_iterator& aStart,
                       SVGPathDataAndOwner::const_iterator& aEnd,
                       SVGPathDataAndOwner::iterator& aResult,
                       SVGPathTraversalState& aState)
{
  uint32_t startType = SVGPathSegUtils::DecodeType(*aStart);
  uint32_t endType = SVGPathSegUtils::DecodeType(*aEnd);

  uint32_t segmentLengthIncludingType =
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
                            uint32_t aCount) const
{
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL type");
  NS_PRECONDITION(aValueToAdd.mType == this, "Incompatible SMIL type");

  SVGPathDataAndOwner& dest =
    *static_cast<SVGPathDataAndOwner*>(aDest.mU.mPtr);
  const SVGPathDataAndOwner& valueToAdd =
    *static_cast<const SVGPathDataAndOwner*>(aValueToAdd.mU.mPtr);

  if (valueToAdd.IsIdentity()) { // Adding identity value - no-op
    return NS_OK;
  }

  if (!dest.IsIdentity()) {
    // Neither value is identity; make sure they're compatible.
    NS_ABORT_IF_FALSE(dest.Element() == valueToAdd.Element(),
                      "adding values from different elements...?");

    PathInterpolationResult check = CanInterpolate(dest, valueToAdd);
    if (check == eCannotInterpolate) {
      // SVGContentUtils::ReportToConsole - can't add path segment lists with
      // different numbers of segments, with arcs that have different flag
      // values, or with incompatible segment types.
      return NS_ERROR_FAILURE;
    }
    if (check == eRequiresConversion) {
      // Convert dest, in-place, to match the types in valueToAdd:
      ConvertAllPathSegmentData(dest.begin(), dest.end(),
                                valueToAdd.begin(), valueToAdd.end(),
                                dest.begin());
    }
  }

  AddWeightedPathSegLists(1.0, dest, aCount, valueToAdd, dest);

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

  // SVGContentUtils::ReportToConsole
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
  NS_ABORT_IF_FALSE(result.IsIdentity(),
                    "expecting outparam to start out as identity");

  PathInterpolationResult check = CanInterpolate(start, end); 

  if (check == eCannotInterpolate) {
    // SVGContentUtils::ReportToConsole - can't interpolate path segment lists with
    // different numbers of segments, with arcs with different flag values, or
    // with incompatible segment types.
    return NS_ERROR_FAILURE;
  }

  const SVGPathDataAndOwner* startListToUse = &start;
  if (check == eRequiresConversion) {
    // Can't convert |start| in-place, since it's const. Instead, we copy it
    // into |result|, converting the types as we go, and use that as our start.
    DebugOnly<bool> success = result.SetLength(end.Length());
    NS_ABORT_IF_FALSE(success, "infallible nsTArray::SetLength should succeed");
    result.SetElement(end.Element()); // propagate target element info!

    ConvertAllPathSegmentData(start.begin(), start.end(),
                              end.begin(), end.end(),
                              result.begin());
    startListToUse = &result;
  }

  AddWeightedPathSegLists(1.0 - aUnitDistance, *startListToUse,
                          aUnitDistance, end, result);

  return NS_OK;
}

} // namespace mozilla
