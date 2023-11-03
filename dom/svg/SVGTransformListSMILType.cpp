/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGTransformListSMILType.h"

#include "mozilla/SMILValue.h"
#include "nsCRT.h"
#include "SVGTransformList.h"
#include "SVGTransform.h"
#include <math.h>

using namespace mozilla::dom::SVGTransform_Binding;

namespace mozilla {

using TransformArray = FallibleTArray<SVGTransformSMILData>;

//----------------------------------------------------------------------
// nsISMILType implementation

void SVGTransformListSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");

  TransformArray* transforms = new TransformArray(1);
  aValue.mU.mPtr = transforms;
  aValue.mType = this;
}

void SVGTransformListSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value type");
  TransformArray* params = static_cast<TransformArray*>(aValue.mU.mPtr);
  delete params;
  aValue.mU.mPtr = nullptr;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGTransformListSMILType::Assign(SMILValue& aDest,
                                          const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");

  const TransformArray* srcTransforms =
      static_cast<const TransformArray*>(aSrc.mU.mPtr);
  TransformArray* dstTransforms = static_cast<TransformArray*>(aDest.mU.mPtr);
  if (!dstTransforms->Assign(*srcTransforms, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

bool SVGTransformListSMILType::IsEqual(const SMILValue& aLeft,
                                       const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected SMIL type");

  const TransformArray& leftArr(
      *static_cast<const TransformArray*>(aLeft.mU.mPtr));
  const TransformArray& rightArr(
      *static_cast<const TransformArray*>(aRight.mU.mPtr));

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

  // Found no differences.
  return true;
}

nsresult SVGTransformListSMILType::Add(SMILValue& aDest,
                                       const SMILValue& aValueToAdd,
                                       uint32_t aCount) const {
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL type");
  MOZ_ASSERT(aDest.mType == aValueToAdd.mType, "Incompatible SMIL types");

  TransformArray& dstTransforms(*static_cast<TransformArray*>(aDest.mU.mPtr));
  const TransformArray& srcTransforms(
      *static_cast<const TransformArray*>(aValueToAdd.mU.mPtr));

  // We're doing a simple add here (as opposed to a sandwich add below).
  // We only do this when we're accumulating a repeat result or calculating
  // a by-animation value.
  //
  // In either case we should have 1 transform in the source array.
  NS_ASSERTION(srcTransforms.Length() == 1,
               "Invalid source transform list to add");

  // And we should have 0 or 1 transforms in the dest array.
  // (We can have 0 transforms in the case of by-animation when we are
  // calculating the by-value as "0 + by". Zero being represented by a
  // SMILValue with an empty transform array.)
  NS_ASSERTION(dstTransforms.Length() < 2,
               "Invalid dest transform list to add to");

  // Get the individual transforms to add
  const SVGTransformSMILData& srcTransform = srcTransforms[0];
  if (dstTransforms.IsEmpty()) {
    SVGTransformSMILData* result = dstTransforms.AppendElement(
        SVGTransformSMILData(srcTransform.mTransformType), fallible);
    NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);
  }
  SVGTransformSMILData& dstTransform = dstTransforms[0];

  // The types must be the same
  NS_ASSERTION(srcTransform.mTransformType == dstTransform.mTransformType,
               "Trying to perform simple add of different transform types");

  // And it should be impossible that one of them is of matrix type
  NS_ASSERTION(srcTransform.mTransformType != SVG_TRANSFORM_MATRIX,
               "Trying to perform simple add with matrix transform");

  // Add the parameters
  for (int i = 0; i <= 2; ++i) {
    dstTransform.mParams[i] += srcTransform.mParams[i] * aCount;
  }

  return NS_OK;
}

nsresult SVGTransformListSMILType::SandwichAdd(
    SMILValue& aDest, const SMILValue& aValueToAdd) const {
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL type");
  MOZ_ASSERT(aDest.mType == aValueToAdd.mType, "Incompatible SMIL types");

  // For <animateTransform> a sandwich add means a matrix post-multiplication
  // which just means to put the additional transform on the end of the array

  TransformArray& dstTransforms(*static_cast<TransformArray*>(aDest.mU.mPtr));
  const TransformArray& srcTransforms(
      *static_cast<const TransformArray*>(aValueToAdd.mU.mPtr));

  // We should have 0 or 1 transforms in the src list.
  NS_ASSERTION(srcTransforms.Length() < 2,
               "Trying to do sandwich add of more than one value");

  // The empty src transform list case only occurs in some limited circumstances
  // where we create an empty 'from' value to interpolate from (e.g.
  // by-animation) but then skip the interpolation step for some reason (e.g.
  // because we have an indefinite duration which means we'll never get past the
  // first value) and instead attempt to add that empty value to the underlying
  // value.
  // In any case, the expected result is that nothing is added.
  if (srcTransforms.IsEmpty()) return NS_OK;

  // Stick the src on the end of the array
  const SVGTransformSMILData& srcTransform = srcTransforms[0];
  SVGTransformSMILData* result =
      dstTransforms.AppendElement(srcTransform, fallible);
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsresult SVGTransformListSMILType::ComputeDistance(const SMILValue& aFrom,
                                                   const SMILValue& aTo,
                                                   double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType,
             "Can't compute difference between different SMIL types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected SMIL type");

  const TransformArray* fromTransforms =
      static_cast<const TransformArray*>(aFrom.mU.mPtr);
  const TransformArray* toTransforms =
      static_cast<const TransformArray*>(aTo.mU.mPtr);

  // ComputeDistance is only used for calculating distances between single
  // values in a values array which necessarily have the same type
  //
  // So we should only have one transform in each array and they should be of
  // the same type
  NS_ASSERTION(fromTransforms->Length() == 1,
               "Wrong number of elements in from value");
  NS_ASSERTION(toTransforms->Length() == 1,
               "Wrong number of elements in to value");

  const SVGTransformSMILData& fromTransform = (*fromTransforms)[0];
  const SVGTransformSMILData& toTransform = (*toTransforms)[0];
  NS_ASSERTION(fromTransform.mTransformType == toTransform.mTransformType,
               "Incompatible transform types to calculate distance between");

  switch (fromTransform.mTransformType) {
    // We adopt the SVGT1.2 notions of distance here
    // See: http://www.w3.org/TR/SVGTiny12/animate.html#complexDistances
    // (As discussed in bug #469040)
    case SVG_TRANSFORM_TRANSLATE:
    case SVG_TRANSFORM_SCALE: {
      const float& a_tx = fromTransform.mParams[0];
      const float& a_ty = fromTransform.mParams[1];
      const float& b_tx = toTransform.mParams[0];
      const float& b_ty = toTransform.mParams[1];
      aDistance = sqrt(pow(a_tx - b_tx, 2) + (pow(a_ty - b_ty, 2)));
    } break;

    case SVG_TRANSFORM_ROTATE:
    case SVG_TRANSFORM_SKEWX:
    case SVG_TRANSFORM_SKEWY: {
      const float& a = fromTransform.mParams[0];
      const float& b = toTransform.mParams[0];
      aDistance = std::fabs(a - b);
    } break;

    default:
      NS_ERROR("Got bad transform types for calculating distances");
      aDistance = 1.0;
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult SVGTransformListSMILType::Interpolate(const SMILValue& aStartVal,
                                               const SMILValue& aEndVal,
                                               double aUnitDistance,
                                               SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Can't interpolate between different SMIL types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected type for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  const TransformArray& startTransforms =
      (*static_cast<const TransformArray*>(aStartVal.mU.mPtr));
  const TransformArray& endTransforms(
      *static_cast<const TransformArray*>(aEndVal.mU.mPtr));

  // We may have 0..n transforms in the start transform array (the base
  // value) but we should only have 1 transform in the end transform array
  NS_ASSERTION(endTransforms.Length() == 1,
               "Invalid end-point for interpolating between transform values");

  // The end point should never be a matrix transform
  const SVGTransformSMILData& endTransform = endTransforms[0];
  NS_ASSERTION(endTransform.mTransformType != SVG_TRANSFORM_MATRIX,
               "End point for interpolation should not be a matrix transform");

  // If we have 0 or more than 1 transform in the start transform array then we
  // just interpolate from 0, 0, 0
  // Likewise, even if there's only 1 transform in the start transform array
  // then if the type of the start transform doesn't match the end then we
  // can't interpolate and should just use 0, 0, 0
  static float identityParams[3] = {0.f};
  const float* startParams = nullptr;
  if (startTransforms.Length() == 1) {
    const SVGTransformSMILData& startTransform = startTransforms[0];
    if (startTransform.mTransformType == endTransform.mTransformType) {
      startParams = startTransform.mParams;
    }
  }
  if (!startParams) {
    startParams = identityParams;
  }

  const float* endParams = endTransform.mParams;

  // Interpolate between the params
  float newParams[3];
  for (int i = 0; i <= 2; ++i) {
    const float& a = startParams[i];
    const float& b = endParams[i];
    newParams[i] = static_cast<float>(a + (b - a) * aUnitDistance);
  }

  // Make the result
  SVGTransformSMILData resultTransform(endTransform.mTransformType, newParams);

  // Clear the way for it in the result array
  TransformArray& dstTransforms =
      (*static_cast<TransformArray*>(aResult.mU.mPtr));
  dstTransforms.Clear();

  // Assign the result
  SVGTransformSMILData* transform =
      dstTransforms.AppendElement(resultTransform, fallible);
  NS_ENSURE_TRUE(transform, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

//----------------------------------------------------------------------
// Transform array accessors

// static
nsresult SVGTransformListSMILType::AppendTransform(
    const SVGTransformSMILData& aTransform, SMILValue& aValue) {
  MOZ_ASSERT(aValue.mType == Singleton(), "Unexpected SMIL value type");

  TransformArray& transforms = *static_cast<TransformArray*>(aValue.mU.mPtr);
  return transforms.AppendElement(aTransform, fallible)
             ? NS_OK
             : NS_ERROR_OUT_OF_MEMORY;
}

// static
bool SVGTransformListSMILType::AppendTransforms(const SVGTransformList& aList,
                                                SMILValue& aValue) {
  MOZ_ASSERT(aValue.mType == Singleton(), "Unexpected SMIL value type");

  TransformArray& transforms = *static_cast<TransformArray*>(aValue.mU.mPtr);

  if (!transforms.SetCapacity(transforms.Length() + aList.Length(), fallible))
    return false;

  for (uint32_t i = 0; i < aList.Length(); ++i) {
    // No need to check the return value below since we have already allocated
    // the necessary space
    MOZ_ALWAYS_TRUE(
        transforms.AppendElement(SVGTransformSMILData(aList[i]), fallible));
  }
  return true;
}

// static
bool SVGTransformListSMILType::GetTransforms(
    const SMILValue& aValue, FallibleTArray<SVGTransform>& aTransforms) {
  MOZ_ASSERT(aValue.mType == Singleton(), "Unexpected SMIL value type");

  const TransformArray& smilTransforms =
      *static_cast<const TransformArray*>(aValue.mU.mPtr);

  aTransforms.Clear();
  if (!aTransforms.SetCapacity(smilTransforms.Length(), fallible)) return false;

  for (const auto& smilTransform : smilTransforms) {
    // No need to check the return value below since we have already allocated
    // the necessary space
    (void)aTransforms.AppendElement(smilTransform.ToSVGTransform(), fallible);
  }
  return true;
}

}  // namespace mozilla
