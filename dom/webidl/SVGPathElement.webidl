/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/SVG2/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */
interface SVGPathElement : SVGGraphicsElement {

  [Constant]
  readonly attribute SVGAnimatedNumber pathLength;

  float getTotalLength();
  [NewObject, Throws]
  SVGPoint getPointAtLength(float distance);
  unsigned long getPathSegAtLength(float distance);
  [NewObject]
  SVGPathSegClosePath createSVGPathSegClosePath();
  [NewObject]
  SVGPathSegMovetoAbs createSVGPathSegMovetoAbs(float x, float y);
  [NewObject]
  SVGPathSegMovetoRel createSVGPathSegMovetoRel(float x, float y);
  [NewObject]
  SVGPathSegLinetoAbs createSVGPathSegLinetoAbs(float x, float y);
  [NewObject]
  SVGPathSegLinetoRel createSVGPathSegLinetoRel(float x, float y);
  [NewObject]
  SVGPathSegCurvetoCubicAbs createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2);
  [NewObject]
  SVGPathSegCurvetoCubicRel createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2);
  [NewObject]
  SVGPathSegCurvetoQuadraticAbs createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1);
  [NewObject]
  SVGPathSegCurvetoQuadraticRel createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1);
  [NewObject]
  SVGPathSegArcAbs createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, boolean largeArcFlag, boolean sweepFlag);
  [NewObject]
  SVGPathSegArcRel createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, boolean largeArcFlag, boolean sweepFlag);
  [NewObject]
  SVGPathSegLinetoHorizontalAbs createSVGPathSegLinetoHorizontalAbs(float x);
  [NewObject]
  SVGPathSegLinetoHorizontalRel createSVGPathSegLinetoHorizontalRel(float x);
  [NewObject]
  SVGPathSegLinetoVerticalAbs createSVGPathSegLinetoVerticalAbs(float y);
  [NewObject]
  SVGPathSegLinetoVerticalRel createSVGPathSegLinetoVerticalRel(float y);
  [NewObject]
  SVGPathSegCurvetoCubicSmoothAbs createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2);
  [NewObject]
  SVGPathSegCurvetoCubicSmoothRel createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2);
  [NewObject]
  SVGPathSegCurvetoQuadraticSmoothAbs createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);
  [NewObject]
  SVGPathSegCurvetoQuadraticSmoothRel createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);
};

SVGPathElement implements SVGAnimatedPathData;

