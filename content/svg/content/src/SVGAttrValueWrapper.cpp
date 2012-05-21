/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAttrValueWrapper.h"
#include "nsSVGAngle.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGLength2.h"
#include "nsSVGNumberPair.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPathData.h"
#include "SVGPointList.h"
#include "SVGStringList.h"
#include "SVGTransformList.h"

using namespace mozilla;

/*static*/ void
SVGAttrValueWrapper::ToString(const nsSVGAngle* aAngle, nsAString& aResult)
{
  aAngle->GetBaseValueString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const nsSVGIntegerPair* aIntegerPair,
                              nsAString& aResult)
{
  aIntegerPair->GetBaseValueString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const nsSVGLength2* aLength, nsAString& aResult)
{
  aLength->GetBaseValueString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const SVGLengthList* aLengthList,
                              nsAString& aResult)
{
  aLengthList->GetValueAsString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const SVGNumberList* aNumberList,
                              nsAString& aResult)
{
  aNumberList->GetValueAsString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const nsSVGNumberPair* aNumberPair,
                              nsAString& aResult)
{
  aNumberPair->GetBaseValueString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const SVGPathData* aPathData, nsAString& aResult)
{
  aPathData->GetValueAsString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const SVGPointList* aPointList,
                              nsAString& aResult)
{
  aPointList->GetValueAsString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(
  const SVGAnimatedPreserveAspectRatio* aPreserveAspectRatio,
  nsAString& aResult)
{
  aPreserveAspectRatio->GetBaseValueString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const SVGStringList* aStringList,
                              nsAString& aResult)
{
  aStringList->GetValue(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const SVGTransformList* aTransformList,
                              nsAString& aResult)
{
  aTransformList->GetValueAsString(aResult);
}

/*static*/ void
SVGAttrValueWrapper::ToString(const nsSVGViewBox* aViewBox, nsAString& aResult)
{
  aViewBox->GetBaseValueString(aResult);
}
