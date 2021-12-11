/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAttrValueWrapper.h"

#include "SVGAnimatedIntegerPair.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedNumberPair.h"
#include "SVGAnimatedOrient.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedViewBox.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPathData.h"
#include "SVGPointList.h"
#include "SVGStringList.h"
#include "SVGTransformList.h"

namespace mozilla {

/*static*/
void SVGAttrValueWrapper::ToString(const SVGAnimatedOrient* aOrient,
                                   nsAString& aResult) {
  aOrient->GetBaseValueString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGAnimatedIntegerPair* aIntegerPair,
                                   nsAString& aResult) {
  aIntegerPair->GetBaseValueString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGAnimatedLength* aLength,
                                   nsAString& aResult) {
  aLength->GetBaseValueString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGLengthList* aLengthList,
                                   nsAString& aResult) {
  aLengthList->GetValueAsString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGNumberList* aNumberList,
                                   nsAString& aResult) {
  aNumberList->GetValueAsString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGAnimatedNumberPair* aNumberPair,
                                   nsAString& aResult) {
  aNumberPair->GetBaseValueString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGPathData* aPathData,
                                   nsAString& aResult) {
  aPathData->GetValueAsString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGPointList* aPointList,
                                   nsAString& aResult) {
  aPointList->GetValueAsString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(
    const SVGAnimatedPreserveAspectRatio* aPreserveAspectRatio,
    nsAString& aResult) {
  aPreserveAspectRatio->GetBaseValueString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGStringList* aStringList,
                                   nsAString& aResult) {
  aStringList->GetValue(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGTransformList* aTransformList,
                                   nsAString& aResult) {
  aTransformList->GetValueAsString(aResult);
}

/*static*/
void SVGAttrValueWrapper::ToString(const SVGAnimatedViewBox* aViewBox,
                                   nsAString& aResult) {
  aViewBox->GetBaseValueString(aResult);
}

}  // namespace mozilla
