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

namespace mozilla {
class SVGAnimatedIntegerPair;
class SVGAnimatedLength;
class SVGAnimatedNumberPair;
class SVGAnimatedOrient;
class SVGAnimatedPreserveAspectRatio;
class SVGAnimatedViewBox;
class SVGLengthList;
class SVGNumberList;
class SVGPathData;
class SVGPointList;
class SVGStringList;
class SVGTransformList;

class SVGAttrValueWrapper {
 public:
  static void ToString(const SVGAnimatedIntegerPair* aIntegerPair,
                       nsAString& aResult);
  static void ToString(const SVGAnimatedLength* aLength, nsAString& aResult);
  static void ToString(const SVGAnimatedNumberPair* aNumberPair,
                       nsAString& aResult);
  static void ToString(const SVGAnimatedOrient* aOrient, nsAString& aResult);
  static void ToString(
      const SVGAnimatedPreserveAspectRatio* aPreserveAspectRatio,
      nsAString& aResult);
  static void ToString(const SVGAnimatedViewBox* aViewBox, nsAString& aResult);
  static void ToString(const SVGLengthList* aLengthList, nsAString& aResult);
  static void ToString(const SVGNumberList* aNumberList, nsAString& aResult);
  static void ToString(const SVGPathData* aPathData, nsAString& aResult);
  static void ToString(const SVGPointList* aPointList, nsAString& aResult);
  static void ToString(const SVGStringList* aStringList, nsAString& aResult);
  static void ToString(const SVGTransformList* aTransformList,
                       nsAString& aResult);
};

} /* namespace mozilla */

#endif  // MOZILLA_SVGATTRVALUEWRAPPER_H__
