/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGATTRVALUEWRAPPER_H__
#define MOZILLA_SVGATTRVALUEWRAPPER_H__

/**
 * Utility wrapper for handling SVG types used inside nsAttrValue so that these
 * types don't need to be exported outside the SVG module.
 */

#include "nsString.h"

class nsSVGAngle;
class nsSVGIntegerPair;
class nsSVGLength2;
class nsSVGNumberPair;
class nsSVGViewBox;

namespace mozilla {
class SVGLengthList;
class SVGNumberList;
class SVGPathData;
class SVGPointList;
class SVGAnimatedPreserveAspectRatio;
class SVGStringList;
class SVGTransformList;
} // namespace mozilla

namespace mozilla {

class SVGAttrValueWrapper
{
public:
  static void ToString(const nsSVGAngle* aAngle, nsAString& aResult);
  static void ToString(const nsSVGIntegerPair* aIntegerPair,
                       nsAString& aResult);
  static void ToString(const nsSVGLength2* aLength, nsAString& aResult);
  static void ToString(const mozilla::SVGLengthList* aLengthList,
                       nsAString& aResult);
  static void ToString(const mozilla::SVGNumberList* aNumberList,
                       nsAString& aResult);
  static void ToString(const nsSVGNumberPair* aNumberPair, nsAString& aResult);
  static void ToString(const mozilla::SVGPathData* aPathData,
                       nsAString& aResult);
  static void ToString(const mozilla::SVGPointList* aPointList,
                       nsAString& aResult);
  static void ToString(
    const mozilla::SVGAnimatedPreserveAspectRatio* aPreserveAspectRatio,
    nsAString& aResult);
  static void ToString(const mozilla::SVGStringList* aStringList,
                       nsAString& aResult);
  static void ToString(const mozilla::SVGTransformList* aTransformList,
                       nsAString& aResult);
  static void ToString(const nsSVGViewBox* aViewBox, nsAString& aResult);
};

} /* namespace mozilla */

#endif // MOZILLA_SVGATTRVALUEWRAPPER_H__
