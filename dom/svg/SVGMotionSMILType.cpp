/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of nsISMILType for use by <animateMotion> element */

#include "SVGMotionSMILType.h"

#include "mozilla/SMILValue.h"
#include "mozilla/gfx/Point.h"
#include "gfx2DGlue.h"
#include "nsDebug.h"
#include "nsMathUtils.h"
#include "nsISupportsUtils.h"
#include "nsTArray.h"
#include <math.h>

using namespace mozilla::gfx;

namespace mozilla {

/*static*/
SVGMotionSMILType SVGMotionSMILType::sSingleton;

// Helper enum, for distinguishing between types of MotionSegment structs
enum SegmentType { eSegmentType_Translation, eSegmentType_PathPoint };

// Helper Structs: containers for params to define our MotionSegment
// (either simple translation or point-on-a-path)
struct TranslationParams {  // Simple translation
  float mX;
  float mY;
};
struct PathPointParams {  // Point along a path
  // Refcounted: need to AddRef/Release.  This can't be an nsRefPtr because
  // this struct is used inside a union so it can't have a default constructor.
  Path* MOZ_OWNING_REF mPath;
  float mDistToPoint;  // Distance from path start to the point on the path that
                       // we're interested in.
};

/**
 * Helper Struct: MotionSegment
 *
 * Instances of this class represent the points that we move between during
 * <animateMotion>.  Each SMILValue will get a nsTArray of these (generally
 * with at most 1 entry in the array, except for in SandwichAdd).  (This
 * matches our behavior in SVGTransformListSMILType.)
 *
 * NOTE: In general, MotionSegments are represented as points on a path
 * (eSegmentType_PathPoint), so that we can easily interpolate and compute
 * distance *along their path*.  However, Add() outputs MotionSegments as
 * simple translations (eSegmentType_Translation), because adding two points
 * from a path (e.g. when accumulating a repeated animation) will generally
 * take you to an arbitrary point *off* of the path.
 */
struct MotionSegment {
  // Default constructor just locks us into being a Translation, and leaves
  // other fields uninitialized (since client is presumably about to set them)
  MotionSegment()
      : mRotateType(eRotateType_Auto),
        mRotateAngle(0.0),
        mSegmentType(eSegmentType_Translation),
        mU{} {}

  // Constructor for a translation
  MotionSegment(float aX, float aY, float aRotateAngle)
      : mRotateType(eRotateType_Explicit),
        mRotateAngle(aRotateAngle),
        mSegmentType(eSegmentType_Translation) {
    mU.mTranslationParams.mX = aX;
    mU.mTranslationParams.mY = aY;
  }

  // Constructor for a point on a path (NOTE: AddRef's)
  MotionSegment(Path* aPath, float aDistToPoint, RotateType aRotateType,
                float aRotateAngle)
      : mRotateType(aRotateType),
        mRotateAngle(aRotateAngle),
        mSegmentType(eSegmentType_PathPoint) {
    mU.mPathPointParams.mPath = aPath;
    mU.mPathPointParams.mDistToPoint = aDistToPoint;

    NS_ADDREF(mU.mPathPointParams.mPath);  // Retain a reference to path
  }

  // Copy constructor (NOTE: AddRef's if we're eSegmentType_PathPoint)
  MotionSegment(const MotionSegment& aOther)
      : mRotateType(aOther.mRotateType),
        mRotateAngle(aOther.mRotateAngle),
        mSegmentType(aOther.mSegmentType) {
    if (mSegmentType == eSegmentType_Translation) {
      mU.mTranslationParams = aOther.mU.mTranslationParams;
    } else {  // mSegmentType == eSegmentType_PathPoint
      mU.mPathPointParams = aOther.mU.mPathPointParams;
      NS_ADDREF(mU.mPathPointParams.mPath);  // Retain a reference to path
    }
  }

  // Destructor (releases any reference we were holding onto)
  ~MotionSegment() {
    if (mSegmentType == eSegmentType_PathPoint) {
      NS_RELEASE(mU.mPathPointParams.mPath);
    }
  }

  // Comparison operators
  bool operator==(const MotionSegment& aOther) const {
    // Compare basic params
    if (mSegmentType != aOther.mSegmentType ||
        mRotateType != aOther.mRotateType ||
        (mRotateType == eRotateType_Explicit &&   // Technically, angle mismatch
         mRotateAngle != aOther.mRotateAngle)) {  // only matters for Explicit.
      return false;
    }

    // Compare translation params, if we're a translation.
    if (mSegmentType == eSegmentType_Translation) {
      return mU.mTranslationParams.mX == aOther.mU.mTranslationParams.mX &&
             mU.mTranslationParams.mY == aOther.mU.mTranslationParams.mY;
    }

    // Else, compare path-point params, if we're a path point.
    return (mU.mPathPointParams.mPath == aOther.mU.mPathPointParams.mPath) &&
           (mU.mPathPointParams.mDistToPoint ==
            aOther.mU.mPathPointParams.mDistToPoint);
  }

  bool operator!=(const MotionSegment& aOther) const {
    return !(*this == aOther);
  }

  // Member Data
  // -----------
  RotateType mRotateType;  // Explicit angle vs. auto vs. auto-reverse.
  float mRotateAngle;      // Only used if mRotateType == eRotateType_Explicit.
  const SegmentType mSegmentType;  // This determines how we interpret
                                   // mU. (const for safety/sanity)

  union {  // Union to let us hold the params for either segment-type.
    TranslationParams mTranslationParams;
    PathPointParams mPathPointParams;
  } mU;
};

using MotionSegmentArray = FallibleTArray<MotionSegment>;

// Helper methods to cast SMILValue.mU.mPtr to the right pointer-type
static MotionSegmentArray& ExtractMotionSegmentArray(SMILValue& aValue) {
  return *static_cast<MotionSegmentArray*>(aValue.mU.mPtr);
}

static const MotionSegmentArray& ExtractMotionSegmentArray(
    const SMILValue& aValue) {
  return *static_cast<const MotionSegmentArray*>(aValue.mU.mPtr);
}

// nsISMILType Methods
// -------------------

void SVGMotionSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected SMIL type");

  aValue.mType = this;
  aValue.mU.mPtr = new MotionSegmentArray(1);
}

void SVGMotionSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL type");

  MotionSegmentArray* arr = static_cast<MotionSegmentArray*>(aValue.mU.mPtr);
  delete arr;

  aValue.mU.mPtr = nullptr;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGMotionSMILType::Assign(SMILValue& aDest,
                                   const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL type");

  const MotionSegmentArray& srcArr = ExtractMotionSegmentArray(aSrc);
  MotionSegmentArray& dstArr = ExtractMotionSegmentArray(aDest);
  if (!dstArr.Assign(srcArr, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

bool SVGMotionSMILType::IsEqual(const SMILValue& aLeft,
                                const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected SMIL type");

  const MotionSegmentArray& leftArr = ExtractMotionSegmentArray(aLeft);
  const MotionSegmentArray& rightArr = ExtractMotionSegmentArray(aRight);

  // If array-lengths don't match, we're trivially non-equal.
  if (leftArr.Length() != rightArr.Length()) {
    return false;
  }

  // Array-lengths match -- check each array-entry for equality.
  uint32_t length = leftArr.Length();  // == rightArr->Length(), if we get here
  for (uint32_t i = 0; i < length; ++i) {
    if (leftArr[i] != rightArr[i]) {
      return false;
    }
  }

  return true;  // If we get here, we found no differences.
}

// Helper method for Add & CreateMatrix
inline static void GetAngleAndPointAtDistance(
    Path* aPath, float aDistance, RotateType aRotateType,
    float& aRotateAngle,  // in & out-param.
    Point& aPoint)        // out-param.
{
  if (aRotateType == eRotateType_Explicit) {
    // Leave aRotateAngle as-is.
    aPoint = aPath->ComputePointAtLength(aDistance);
  } else {
    Point tangent;  // Unit vector tangent to the point we find.
    aPoint = aPath->ComputePointAtLength(aDistance, &tangent);
    float tangentAngle = atan2(tangent.y, tangent.x);
    if (aRotateType == eRotateType_Auto) {
      aRotateAngle = tangentAngle;
    } else {
      MOZ_ASSERT(aRotateType == eRotateType_AutoReverse);
      aRotateAngle = M_PI + tangentAngle;
    }
  }
}

nsresult SVGMotionSMILType::Add(SMILValue& aDest, const SMILValue& aValueToAdd,
                                uint32_t aCount) const {
  MOZ_ASSERT(aDest.mType == aValueToAdd.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL type");

  MotionSegmentArray& dstArr = ExtractMotionSegmentArray(aDest);
  const MotionSegmentArray& srcArr = ExtractMotionSegmentArray(aValueToAdd);

  // We're doing a simple add here (as opposed to a sandwich add below).  We
  // only do this when we're accumulating a repeat result.
  // NOTE: In other nsISMILTypes, we use this method with a barely-initialized
  // |aDest| value to assist with "by" animation.  (In this case,
  // "barely-initialized" would mean dstArr.Length() would be empty.)  However,
  // we don't do this for <animateMotion>, because we instead use our "by"
  // value to construct an equivalent "path" attribute, and we use *that* for
  // our actual animation.
  MOZ_ASSERT(srcArr.Length() == 1, "Invalid source segment arr to add");
  MOZ_ASSERT(dstArr.Length() == 1, "Invalid dest segment arr to add to");
  const MotionSegment& srcSeg = srcArr[0];
  const MotionSegment& dstSeg = dstArr[0];
  MOZ_ASSERT(srcSeg.mSegmentType == eSegmentType_PathPoint,
             "expecting to be adding points from a motion path");
  MOZ_ASSERT(dstSeg.mSegmentType == eSegmentType_PathPoint,
             "expecting to be adding points from a motion path");

  const PathPointParams& srcParams = srcSeg.mU.mPathPointParams;
  const PathPointParams& dstParams = dstSeg.mU.mPathPointParams;

  MOZ_ASSERT(srcSeg.mRotateType == dstSeg.mRotateType &&
                 srcSeg.mRotateAngle == dstSeg.mRotateAngle,
             "unexpected angle mismatch");
  MOZ_ASSERT(srcParams.mPath == dstParams.mPath, "unexpected path mismatch");
  Path* path = srcParams.mPath;

  // Use destination to get our rotate angle.
  float rotateAngle = dstSeg.mRotateAngle;
  Point dstPt;
  GetAngleAndPointAtDistance(path, dstParams.mDistToPoint, dstSeg.mRotateType,
                             rotateAngle, dstPt);

  Point srcPt = path->ComputePointAtLength(srcParams.mDistToPoint);

  float newX = dstPt.x + srcPt.x * aCount;
  float newY = dstPt.y + srcPt.y * aCount;

  // Replace destination's current value -- a point-on-a-path -- with the
  // translation that results from our addition.
  dstArr.ReplaceElementAt(0, MotionSegment(newX, newY, rotateAngle));
  return NS_OK;
}

nsresult SVGMotionSMILType::SandwichAdd(SMILValue& aDest,
                                        const SMILValue& aValueToAdd) const {
  MOZ_ASSERT(aDest.mType == aValueToAdd.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL type");
  MotionSegmentArray& dstArr = ExtractMotionSegmentArray(aDest);
  const MotionSegmentArray& srcArr = ExtractMotionSegmentArray(aValueToAdd);

  // We're only expecting to be adding 1 segment on to the list
  MOZ_ASSERT(srcArr.Length() == 1,
             "Trying to do sandwich add of more than one value");

  if (!dstArr.AppendElement(srcArr[0], fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult SVGMotionSMILType::ComputeDistance(const SMILValue& aFrom,
                                            const SMILValue& aTo,
                                            double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected SMIL type");
  const MotionSegmentArray& fromArr = ExtractMotionSegmentArray(aFrom);
  const MotionSegmentArray& toArr = ExtractMotionSegmentArray(aTo);

  // ComputeDistance is only used for calculating distances between single
  // values in a values array. So we should only have one entry in each array.
  MOZ_ASSERT(fromArr.Length() == 1, "Wrong number of elements in from value");
  MOZ_ASSERT(toArr.Length() == 1, "Wrong number of elements in to value");

  const MotionSegment& from = fromArr[0];
  const MotionSegment& to = toArr[0];

  MOZ_ASSERT(from.mSegmentType == to.mSegmentType,
             "Mismatched MotionSegment types");
  if (from.mSegmentType == eSegmentType_PathPoint) {
    const PathPointParams& fromParams = from.mU.mPathPointParams;
    const PathPointParams& toParams = to.mU.mPathPointParams;
    MOZ_ASSERT(fromParams.mPath == toParams.mPath,
               "Interpolation endpoints should be from same path");
    aDistance = std::fabs(toParams.mDistToPoint - fromParams.mDistToPoint);
  } else {
    const TranslationParams& fromParams = from.mU.mTranslationParams;
    const TranslationParams& toParams = to.mU.mTranslationParams;
    float dX = toParams.mX - fromParams.mX;
    float dY = toParams.mY - fromParams.mY;
    aDistance = NS_hypot(dX, dY);
  }

  return NS_OK;
}

// Helper method for Interpolate()
static inline float InterpolateFloat(const float& aStartFlt,
                                     const float& aEndFlt,
                                     const double& aUnitDistance) {
  return aStartFlt + aUnitDistance * (aEndFlt - aStartFlt);
}

nsresult SVGMotionSMILType::Interpolate(const SMILValue& aStartVal,
                                        const SMILValue& aEndVal,
                                        double aUnitDistance,
                                        SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");
  MOZ_ASSERT(aUnitDistance >= 0.0 && aUnitDistance <= 1.0,
             "unit distance value out of bounds");

  const MotionSegmentArray& startArr = ExtractMotionSegmentArray(aStartVal);
  const MotionSegmentArray& endArr = ExtractMotionSegmentArray(aEndVal);
  MotionSegmentArray& resultArr = ExtractMotionSegmentArray(aResult);

  MOZ_ASSERT(endArr.Length() == 1,
             "Invalid end-point for animateMotion interpolation");
  MOZ_ASSERT(resultArr.IsEmpty(),
             "Expecting result to be just-initialized w/ empty array");

  const MotionSegment& endSeg = endArr[0];
  MOZ_ASSERT(endSeg.mSegmentType == eSegmentType_PathPoint,
             "Expecting to be interpolating along a path");

  const PathPointParams& endParams = endSeg.mU.mPathPointParams;
  // NOTE: Ususally, path & angle should match between start & end (since
  // presumably start & end came from the same <animateMotion> element),
  // unless start is empty. (as it would be for pure 'to' animation)
  // Notable exception: when a to-animation layers on top of lower priority
  // animation(s) -- see comment below.
  Path* path = endParams.mPath;
  RotateType rotateType = endSeg.mRotateType;
  float rotateAngle = endSeg.mRotateAngle;

  float startDist;
  if (startArr.IsEmpty() ||
      startArr[0].mU.mPathPointParams.mPath != endParams.mPath) {
    // When a to-animation follows other lower priority animation(s),
    // the endParams will contain a different path from the animation(s)
    // that it layers on top of.
    // Per SMIL spec, we should interpolate from the value at startArr.
    // However, neither Chrome nor Safari implements to-animation that way.
    // For simplicity, we use the same behavior as other browsers: override
    // previous animations and start at the initial underlying value.
    startDist = 0.0f;
  } else {
    MOZ_ASSERT(startArr.Length() <= 1,
               "Invalid start-point for animateMotion interpolation");
    const MotionSegment& startSeg = startArr[0];
    MOZ_ASSERT(startSeg.mSegmentType == eSegmentType_PathPoint,
               "Expecting to be interpolating along a path");
    const PathPointParams& startParams = startSeg.mU.mPathPointParams;
    MOZ_ASSERT(startSeg.mRotateType == endSeg.mRotateType &&
                   startSeg.mRotateAngle == endSeg.mRotateAngle,
               "unexpected angle mismatch");
    startDist = startParams.mDistToPoint;
  }

  // Get the interpolated distance along our path.
  float resultDist =
      InterpolateFloat(startDist, endParams.mDistToPoint, aUnitDistance);

  // Construct the intermediate result segment, and put it in our outparam.
  // AppendElement has guaranteed success here, since Init() allocates 1 slot.
  MOZ_ALWAYS_TRUE(resultArr.AppendElement(
      MotionSegment(path, resultDist, rotateType, rotateAngle), fallible));
  return NS_OK;
}

/* static */ gfx::Matrix SVGMotionSMILType::CreateMatrix(
    const SMILValue& aSMILVal) {
  const MotionSegmentArray& arr = ExtractMotionSegmentArray(aSMILVal);

  gfx::Matrix matrix;
  uint32_t length = arr.Length();
  for (uint32_t i = 0; i < length; i++) {
    Point point;                              // initialized below
    float rotateAngle = arr[i].mRotateAngle;  // might get updated below
    if (arr[i].mSegmentType == eSegmentType_Translation) {
      point.x = arr[i].mU.mTranslationParams.mX;
      point.y = arr[i].mU.mTranslationParams.mY;
      MOZ_ASSERT(arr[i].mRotateType == eRotateType_Explicit,
                 "'auto'/'auto-reverse' should have been converted to "
                 "explicit angles when we generated this translation");
    } else {
      GetAngleAndPointAtDistance(arr[i].mU.mPathPointParams.mPath,
                                 arr[i].mU.mPathPointParams.mDistToPoint,
                                 arr[i].mRotateType, rotateAngle, point);
    }
    matrix.PreTranslate(point.x, point.y);
    matrix.PreRotate(rotateAngle);
  }
  return matrix;
}

/* static */
SMILValue SVGMotionSMILType::ConstructSMILValue(Path* aPath, float aDist,
                                                RotateType aRotateType,
                                                float aRotateAngle) {
  SMILValue smilVal(&SVGMotionSMILType::sSingleton);
  MotionSegmentArray& arr = ExtractMotionSegmentArray(smilVal);

  // AppendElement has guaranteed success here, since Init() allocates 1 slot.
  MOZ_ALWAYS_TRUE(arr.AppendElement(
      MotionSegment(aPath, aDist, aRotateType, aRotateAngle), fallible));
  return smilVal;
}

}  // namespace mozilla
