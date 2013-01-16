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

  readonly attribute SVGAnimatedNumber pathLength;

  [Throws]
  float getTotalLength();
  [Creator, Throws]
  SVGPoint getPointAtLength(float distance);
  unsigned long getPathSegAtLength(float distance);
  [Creator]
  SVGPathSegClosePath createSVGPathSegClosePath();
  [Creator]
  SVGPathSegMovetoAbs createSVGPathSegMovetoAbs(float x, float y);
  [Creator]
  SVGPathSegMovetoRel createSVGPathSegMovetoRel(float x, float y);
  [Creator]
  SVGPathSegLinetoAbs createSVGPathSegLinetoAbs(float x, float y);
  [Creator]
  SVGPathSegLinetoRel createSVGPathSegLinetoRel(float x, float y);
  [Creator]
  SVGPathSegCurvetoCubicAbs createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2);
  [Creator]
  SVGPathSegCurvetoCubicRel createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2);
  [Creator]
  SVGPathSegCurvetoQuadraticAbs createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1);
  [Creator]
  SVGPathSegCurvetoQuadraticRel createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1);
  [Creator]
  SVGPathSegArcAbs createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, boolean largeArcFlag, boolean sweepFlag);
  [Creator]
  SVGPathSegArcRel createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, boolean largeArcFlag, boolean sweepFlag);
  [Creator]
  SVGPathSegLinetoHorizontalAbs createSVGPathSegLinetoHorizontalAbs(float x);
  [Creator]
  SVGPathSegLinetoHorizontalRel createSVGPathSegLinetoHorizontalRel(float x);
  [Creator]
  SVGPathSegLinetoVerticalAbs createSVGPathSegLinetoVerticalAbs(float y);
  [Creator]
  SVGPathSegLinetoVerticalRel createSVGPathSegLinetoVerticalRel(float y);
  [Creator]
  SVGPathSegCurvetoCubicSmoothAbs createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2);
  [Creator]
  SVGPathSegCurvetoCubicSmoothRel createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2);
  [Creator]
  SVGPathSegCurvetoQuadraticSmoothAbs createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);
  [Creator]
  SVGPathSegCurvetoQuadraticSmoothRel createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);
};

SVGPathElement implements SVGAnimatedPathData;

