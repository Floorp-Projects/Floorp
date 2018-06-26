/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsSVGTransform.h"
#include "nsContentUtils.h" // for NS_ENSURE_FINITE
#include "nsTextFormatter.h"

namespace {
  const double kRadPerDegree = 2.0 * M_PI / 360.0;
} // namespace

namespace mozilla {

using namespace dom::SVGTransformBinding;

void
nsSVGTransform::GetValueAsString(nsAString& aValue) const
{
  switch (mType) {
    case SVG_TRANSFORM_TRANSLATE:
      // The spec say that if Y is not provided, it is assumed to be zero.
      if (mMatrix._32 != 0)
        nsTextFormatter::ssprintf(aValue,
            u"translate(%g, %g)",
            mMatrix._31, mMatrix._32);
      else
        nsTextFormatter::ssprintf(aValue,
            u"translate(%g)",
            mMatrix._31);
      break;
    case SVG_TRANSFORM_ROTATE:
      if (mOriginX != 0.0f || mOriginY != 0.0f)
        nsTextFormatter::ssprintf(aValue,
            u"rotate(%g, %g, %g)",
            mAngle, mOriginX, mOriginY);
      else
        nsTextFormatter::ssprintf(aValue,
            u"rotate(%g)", mAngle);
      break;
    case SVG_TRANSFORM_SCALE:
      if (mMatrix._11 != mMatrix._22)
        nsTextFormatter::ssprintf(aValue,
            u"scale(%g, %g)", mMatrix._11, mMatrix._22);
      else
        nsTextFormatter::ssprintf(aValue,
            u"scale(%g)", mMatrix._11);
      break;
    case SVG_TRANSFORM_SKEWX:
      nsTextFormatter::ssprintf(aValue,
                                u"skewX(%g)", mAngle);
      break;
    case SVG_TRANSFORM_SKEWY:
      nsTextFormatter::ssprintf(aValue,
                                u"skewY(%g)", mAngle);
      break;
    case SVG_TRANSFORM_MATRIX:
      nsTextFormatter::ssprintf(aValue,
          u"matrix(%g, %g, %g, %g, %g, %g)",
                            mMatrix._11, mMatrix._12,
                            mMatrix._21, mMatrix._22,
                            mMatrix._31, mMatrix._32);
      break;
    default:
      aValue.Truncate();
      NS_ERROR("unknown transformation type");
      break;
  }
}

void
nsSVGTransform::SetMatrix(const gfxMatrix& aMatrix)
{
  mType    = SVG_TRANSFORM_MATRIX;
  mMatrix  = aMatrix;
  // We set the other members here too, since operator== requires it and
  // the DOM requires it for mAngle.
  mAngle   = 0.f;
  mOriginX = 0.f;
  mOriginY = 0.f;
}

void
nsSVGTransform::SetTranslate(float aTx, float aTy)
{
  mType    = SVG_TRANSFORM_TRANSLATE;
  mMatrix  = gfxMatrix::Translation(aTx, aTy);
  mAngle   = 0.f;
  mOriginX = 0.f;
  mOriginY = 0.f;
}

void
nsSVGTransform::SetScale(float aSx, float aSy)
{
  mType    = SVG_TRANSFORM_SCALE;
  mMatrix  = gfxMatrix::Scaling(aSx, aSy);
  mAngle   = 0.f;
  mOriginX = 0.f;
  mOriginY = 0.f;
}

void
nsSVGTransform::SetRotate(float aAngle, float aCx, float aCy)
{
  mType    = SVG_TRANSFORM_ROTATE;
  mMatrix  = gfxMatrix::Translation(aCx, aCy)
                       .PreRotate(aAngle*kRadPerDegree)
                       .PreTranslate(-aCx, -aCy);
  mAngle   = aAngle;
  mOriginX = aCx;
  mOriginY = aCy;
}

nsresult
nsSVGTransform::SetSkewX(float aAngle)
{
  double ta = tan(aAngle*kRadPerDegree);
  NS_ENSURE_FINITE(ta, NS_ERROR_RANGE_ERR);

  mType    = SVG_TRANSFORM_SKEWX;
  mMatrix  = gfxMatrix();
  mMatrix._21 = ta;
  mAngle   = aAngle;
  mOriginX = 0.f;
  mOriginY = 0.f;
  return NS_OK;
}

nsresult
nsSVGTransform::SetSkewY(float aAngle)
{
  double ta = tan(aAngle*kRadPerDegree);
  NS_ENSURE_FINITE(ta, NS_ERROR_RANGE_ERR);

  mType    = SVG_TRANSFORM_SKEWY;
  mMatrix  = gfxMatrix();
  mMatrix._12 = ta;
  mAngle   = aAngle;
  mOriginX = 0.f;
  mOriginY = 0.f;
  return NS_OK;
}

SVGTransformSMILData::SVGTransformSMILData(const nsSVGTransform& aTransform)
  : mTransformType(aTransform.Type())
{
  MOZ_ASSERT(mTransformType >= SVG_TRANSFORM_MATRIX &&
             mTransformType <= SVG_TRANSFORM_SKEWY,
             "Unexpected transform type");

  for (uint32_t i = 0; i < NUM_STORED_PARAMS; ++i) {
    mParams[i] = 0.f;
  }

  switch (mTransformType) {
    case SVG_TRANSFORM_MATRIX: {
      const gfxMatrix& mx = aTransform.GetMatrix();
      mParams[0] = static_cast<float>(mx._11);
      mParams[1] = static_cast<float>(mx._12);
      mParams[2] = static_cast<float>(mx._21);
      mParams[3] = static_cast<float>(mx._22);
      mParams[4] = static_cast<float>(mx._31);
      mParams[5] = static_cast<float>(mx._32);
      break;
    }
    case SVG_TRANSFORM_TRANSLATE: {
      const gfxMatrix& mx = aTransform.GetMatrix();
      mParams[0] = static_cast<float>(mx._31);
      mParams[1] = static_cast<float>(mx._32);
      break;
    }
    case SVG_TRANSFORM_SCALE: {
      const gfxMatrix& mx = aTransform.GetMatrix();
      mParams[0] = static_cast<float>(mx._11);
      mParams[1] = static_cast<float>(mx._22);
      break;
    }
    case SVG_TRANSFORM_ROTATE:
      mParams[0] = aTransform.Angle();
      aTransform.GetRotationOrigin(mParams[1], mParams[2]);
      break;

    case SVG_TRANSFORM_SKEWX:
    case SVG_TRANSFORM_SKEWY:
      mParams[0] = aTransform.Angle();
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected transform type");
      break;
  }
}

nsSVGTransform
SVGTransformSMILData::ToSVGTransform() const
{
  nsSVGTransform result;

  switch (mTransformType) {
    case SVG_TRANSFORM_MATRIX:
      result.SetMatrix(gfxMatrix(mParams[0], mParams[1],
                                 mParams[2], mParams[3],
                                 mParams[4], mParams[5]));
      break;

    case SVG_TRANSFORM_TRANSLATE:
      result.SetTranslate(mParams[0], mParams[1]);
      break;

    case SVG_TRANSFORM_SCALE:
      result.SetScale(mParams[0], mParams[1]);
      break;

    case SVG_TRANSFORM_ROTATE:
      result.SetRotate(mParams[0], mParams[1], mParams[2]);
      break;

    case SVG_TRANSFORM_SKEWX:
      result.SetSkewX(mParams[0]);
      break;

    case SVG_TRANSFORM_SKEWY:
      result.SetSkewY(mParams[0]);
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected transform type");
      break;
  }
  return result;
}

} // namespace mozilla
