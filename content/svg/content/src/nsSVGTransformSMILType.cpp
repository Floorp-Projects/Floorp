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
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSVGTransformSMILType.h"
#include "nsSMILValue.h"
#include "nsCRT.h"
#include <math.h>

/*static*/ nsSVGTransformSMILType nsSVGTransformSMILType::sSingleton;

//----------------------------------------------------------------------
// nsISMILType implementation

nsresult
nsSVGTransformSMILType::Init(nsSMILValue &aValue) const
{
  NS_PRECONDITION(aValue.mType == this || aValue.IsNull(),
                  "Unexpected value type");
  NS_ASSERTION(aValue.mType != this || aValue.mU.mPtr,
               "Invalid nsSMILValue of SVG transform type: NULL data member.");

  if (aValue.mType != this || !aValue.mU.mPtr) {
    // Different type, or no data member: allocate memory and set type 
    TransformArray* transforms = new TransformArray(1);
    NS_ENSURE_TRUE(transforms, NS_ERROR_OUT_OF_MEMORY);
    aValue.mU.mPtr = transforms;
    aValue.mType = this;
  } else {
    // Same type, just set clear
    TransformArray* transforms = static_cast<TransformArray*>(aValue.mU.mPtr);
    transforms->Clear();
  }

  return NS_OK;
}

void
nsSVGTransformSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value type.");
  TransformArray* params = static_cast<TransformArray*>(aValue.mU.mPtr);
  delete params;
  aValue.mU.mPtr = nsnull;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
nsSVGTransformSMILType::Assign(nsSMILValue& aDest,
                               const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types.");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value.");

  const TransformArray* srcTransforms = 
    static_cast<const TransformArray*>(aSrc.mU.mPtr);
  TransformArray* dstTransforms = static_cast<TransformArray*>(aDest.mU.mPtr);

  // Before we assign, ensure we have sufficient memory
  PRBool result = dstTransforms->SetCapacity(srcTransforms->Length());
  NS_ENSURE_TRUE(result,NS_ERROR_OUT_OF_MEMORY);

  *dstTransforms = *srcTransforms;

  return NS_OK;
}

nsresult
nsSVGTransformSMILType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                            PRUint32 aCount) const
{
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL type.");
  NS_PRECONDITION(aDest.mType == aValueToAdd.mType, "Incompatible SMIL types.");

  TransformArray& dstTransforms(*static_cast<TransformArray*>(aDest.mU.mPtr));
  const TransformArray& srcTransforms
    (*static_cast<const TransformArray*>(aValueToAdd.mU.mPtr));

  // We're doing a simple add here (as opposed to a sandwich add below).
  // We only do this when we're accumulating a repeat result or calculating
  // a by-animation value.
  //
  // In either case we should have 1 transform in the source array.
  NS_ASSERTION(srcTransforms.Length() == 1,
    "Invalid source transform list to add.");

  // And we should have 0 or 1 transforms in the dest array.
  // (We can have 0 transforms in the case of by-animation when we are
  // calculating the by-value as "0 + by". Zero being represented by an
  // nsSMILValue with an empty transform array.)
  NS_ASSERTION(dstTransforms.Length() < 2,
    "Invalid dest transform list to add to.");

  // Get the individual transforms to add
  const nsSVGSMILTransform& srcTransform = srcTransforms[0];
  if (dstTransforms.IsEmpty()) {
    nsSVGSMILTransform* result = dstTransforms.AppendElement(
      nsSVGSMILTransform(srcTransform.mTransformType));
    NS_ENSURE_TRUE(result,NS_ERROR_OUT_OF_MEMORY);
  }
  nsSVGSMILTransform& dstTransform = dstTransforms[0];

  // The types must be the same
  NS_ASSERTION(srcTransform.mTransformType == dstTransform.mTransformType,
    "Trying to perform simple add of different transform types.");

  // And it should be impossible that one of them is of matrix type
  NS_ASSERTION(
    srcTransform.mTransformType != nsSVGSMILTransform::TRANSFORM_MATRIX,
    "Trying to perform simple add with matrix transform.");

  // Add the parameters
  for (int i = 0; i <= 2; ++i) {
    dstTransform.mParams[i] += srcTransform.mParams[i] * aCount;
  }

  return NS_OK;
}

nsresult
nsSVGTransformSMILType::SandwichAdd(nsSMILValue& aDest,
                                    const nsSMILValue& aValueToAdd) const
{
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL type.");
  NS_PRECONDITION(aDest.mType == aValueToAdd.mType, "Incompatible SMIL types.");

  // For <animateTransform> a sandwich add means a matrix post-multiplication
  // which just means to put the additional transform on the end of the array

  TransformArray& dstTransforms(*static_cast<TransformArray*>(aDest.mU.mPtr));
  const TransformArray& srcTransforms
    (*static_cast<const TransformArray*>(aValueToAdd.mU.mPtr));

  // We're only expecting to be adding 1 src transform on to the list
  NS_ASSERTION(srcTransforms.Length() == 1,
    "Trying to do sandwich add of more than one value.");

  // Stick the src on the end of the array
  const nsSVGSMILTransform& srcTransform = srcTransforms[0];
  nsSVGSMILTransform* result = dstTransforms.AppendElement(srcTransform);
  NS_ENSURE_TRUE(result,NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsresult
nsSVGTransformSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                        const nsSMILValue& aTo,
                                        double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType, 
      "Can't compute difference between different SMIL types.");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected SMIL type.");

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
    "Wrong number of elements in from value.");
  NS_ASSERTION(toTransforms->Length() == 1,
    "Wrong number of elements in to value.");

  const nsSVGSMILTransform& fromTransform = (*fromTransforms)[0];
  const nsSVGSMILTransform& toTransform = (*toTransforms)[0];
  NS_ASSERTION(fromTransform.mTransformType == toTransform.mTransformType,
    "Incompatible transform types to calculate distance between.");

  switch (fromTransform.mTransformType)
  {
    // We adopt the SVGT1.2 notions of distance here
    // See: http://www.w3.org/TR/SVGTiny12/animate.html#complexDistances
    // (As discussed in bug #469040)
    case nsSVGSMILTransform::TRANSFORM_TRANSLATE:
    case nsSVGSMILTransform::TRANSFORM_SCALE:
      {
        const float& a_tx = fromTransform.mParams[0];
        const float& a_ty = fromTransform.mParams[1];
        const float& b_tx = toTransform.mParams[0];
        const float& b_ty = toTransform.mParams[1];
        aDistance = sqrt(pow(a_tx - b_tx, 2) + (pow(a_ty - b_ty, 2)));
      }
      break;

    case nsSVGSMILTransform::TRANSFORM_ROTATE:
    case nsSVGSMILTransform::TRANSFORM_SKEWX:
    case nsSVGSMILTransform::TRANSFORM_SKEWY:
      {
        const float& a = fromTransform.mParams[0];
        const float& b = toTransform.mParams[0];
        aDistance = fabs(a-b);
      }
      break;

    default:
      NS_ERROR("Got bad transform types for calculating distances.");
      aDistance = 1.0;
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsSVGTransformSMILType::Interpolate(const nsSMILValue& aStartVal,
                                    const nsSMILValue& aEndVal,
                                    double aUnitDistance,
                                    nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
      "Can't interpolate between different SMIL types.");
  NS_PRECONDITION(aStartVal.mType == this,
      "Unexpected type for interpolation.");
  NS_PRECONDITION(aResult.mType == this, "Unexpected result type.");

  const TransformArray& startTransforms = 
    (*static_cast<const TransformArray*>(aStartVal.mU.mPtr));
  const TransformArray& endTransforms 
    (*static_cast<const TransformArray*>(aEndVal.mU.mPtr));

  // We may have 0..n transforms in the start transform array (the base
  // value) but we should only have 1 transform in the end transform array
  NS_ASSERTION(endTransforms.Length() == 1,
    "Invalid end-point for interpolating between transform values.");

  // The end point should never be a matrix transform
  const nsSVGSMILTransform& endTransform = endTransforms[0];
  NS_ASSERTION(
    endTransform.mTransformType != nsSVGSMILTransform::TRANSFORM_MATRIX,
    "End point for interpolation should not be a matrix transform.");

  // If we have 0 or more than 1 transform in the start transform array then we
  // just interpolate from 0, 0, 0
  // Likewise, even if there's only 1 transform in the start transform array
  // then if the type of the start transform doesn't match the end then we
  // can't interpolate and should just use 0, 0, 0
  static float identityParams[3] = { 0.f };
  const float* startParams = nsnull;
  if (startTransforms.Length() == 1) {
    const nsSVGSMILTransform& startTransform = startTransforms[0];
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
    newParams[i] = a + (b - a) * aUnitDistance;
  }

  // Make the result
  nsSVGSMILTransform resultTransform(endTransform.mTransformType, newParams);

  // Clear the way for it in the result array
  TransformArray& dstTransforms = 
    (*static_cast<TransformArray*>(aResult.mU.mPtr));
  dstTransforms.Clear();

  // Assign the result
  nsSVGSMILTransform* transform = dstTransforms.AppendElement(resultTransform);
  NS_ENSURE_TRUE(transform,NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

//----------------------------------------------------------------------
// Transform array accessors

PRUint32
nsSVGTransformSMILType::GetNumTransforms(const nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value.");

  const TransformArray& transforms = 
    *static_cast<const TransformArray*>(aValue.mU.mPtr);
  
  return transforms.Length();
}

const nsSVGSMILTransform*
nsSVGTransformSMILType::GetTransformAt(PRUint32 aIndex,
                                       const nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value.");

  const TransformArray& transforms = 
    *static_cast<const TransformArray*>(aValue.mU.mPtr);

  if (aIndex >= transforms.Length()) {
    NS_ERROR("Attempting to access invalid transform.");
    return nsnull;
  }
  
  return &transforms[aIndex];
}

nsresult
nsSVGTransformSMILType::AppendTransform(const nsSVGSMILTransform& aTransform,
                                        nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value.");

  TransformArray& transforms = *static_cast<TransformArray*>(aValue.mU.mPtr);

  nsSVGSMILTransform* transform = transforms.AppendElement(aTransform);
  NS_ENSURE_TRUE(transform,NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}
