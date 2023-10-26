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

[Exposed=Window]
interface SVGSVGElement : SVGGraphicsElement {

  [Constant]
  readonly attribute SVGAnimatedLength x;
  [Constant]
  readonly attribute SVGAnimatedLength y;
  [Constant]
  readonly attribute SVGAnimatedLength width;
  [Constant]
  readonly attribute SVGAnimatedLength height;
  [UseCounter]
           attribute float currentScale;
  readonly attribute SVGPoint currentTranslate;

  [DependsOn=Nothing, Affects=Nothing]
  unsigned long suspendRedraw(unsigned long maxWaitMilliseconds);
  [DependsOn=Nothing, Affects=Nothing]
  undefined unsuspendRedraw(unsigned long suspendHandleID);
  [DependsOn=Nothing, Affects=Nothing]
  undefined unsuspendRedrawAll();
  [DependsOn=Nothing, Affects=Nothing]
  undefined forceRedraw();
  undefined pauseAnimations();
  undefined unpauseAnimations();
  boolean animationsPaused();
  [BinaryName="getCurrentTimeAsFloat"]
  float getCurrentTime();
  undefined setCurrentTime(float seconds);
  // NodeList getIntersectionList(SVGRect rect, SVGElement referenceElement);
  // NodeList getEnclosureList(SVGRect rect, SVGElement referenceElement);
  // boolean checkIntersection(SVGElement element, SVGRect rect);
  // boolean checkEnclosure(SVGElement element, SVGRect rect);
  [Deprecated="SVGDeselectAll"]
  undefined deselectAll();
  [NewObject]
  SVGNumber createSVGNumber();
  [NewObject]
  SVGLength createSVGLength();
  [NewObject]
  SVGAngle createSVGAngle();
  [NewObject]
  SVGPoint createSVGPoint();
  [NewObject]
  SVGMatrix createSVGMatrix();
  [NewObject]
  SVGRect createSVGRect();
  [NewObject]
  SVGTransform createSVGTransform();
  [NewObject, Throws]
  SVGTransform createSVGTransformFromMatrix(optional DOMMatrix2DInit matrix = {});
  [UseCounter]
  Element? getElementById(DOMString elementId);
};

SVGSVGElement includes SVGFitToViewBox;
SVGSVGElement includes SVGZoomAndPan;
