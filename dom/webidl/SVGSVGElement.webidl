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

interface SVGAnimatedString;
interface SVGViewSpec;

interface SVGSVGElement : SVGGraphicsElement {

  readonly attribute SVGAnimatedLength x;
  readonly attribute SVGAnimatedLength y;
  readonly attribute SVGAnimatedLength width;
  readonly attribute SVGAnimatedLength height;
  // readonly attribute SVGRect viewport;
  readonly attribute float pixelUnitToMillimeterX;
  readonly attribute float pixelUnitToMillimeterY;
  readonly attribute float screenPixelToMillimeterX;
  readonly attribute float screenPixelToMillimeterY;
  readonly attribute boolean useCurrentView;
  // readonly attribute SVGViewSpec currentView;
           attribute float currentScale;
  readonly attribute SVGPoint currentTranslate;

  unsigned long suspendRedraw(unsigned long maxWaitMilliseconds);
  void unsuspendRedraw(unsigned long suspendHandleID);
  void unsuspendRedrawAll();
  [Throws]
  void forceRedraw();
  void pauseAnimations();
  void unpauseAnimations();
  boolean animationsPaused();
  float getCurrentTime();
  void setCurrentTime(float seconds);
  // NodeList getIntersectionList(SVGRect rect, SVGElement referenceElement);
  // NodeList getEnclosureList(SVGRect rect, SVGElement referenceElement);
  // boolean checkIntersection(SVGElement element, SVGRect rect);
  // boolean checkEnclosure(SVGElement element, SVGRect rect);
  // void deselectAll();
  [Creator]
  SVGNumber createSVGNumber();
  [Creator]
  SVGLength createSVGLength();
  [Creator]
  SVGAngle createSVGAngle();
  [Creator]
  SVGPoint createSVGPoint();
  [Creator]
  SVGMatrix createSVGMatrix();
  [Creator]
  SVGRect createSVGRect();
  [Creator]
  SVGTransform createSVGTransform();
  [Creator]
  SVGTransform createSVGTransformFromMatrix(SVGMatrix matrix);
  [Throws]
  Element? getElementById(DOMString elementId);
};

/*SVGSVGElement implements ViewCSS;
SVGSVGElement implements DocumentCSS;*/
SVGSVGElement implements SVGFitToViewBox;
SVGSVGElement implements SVGZoomAndPan;

