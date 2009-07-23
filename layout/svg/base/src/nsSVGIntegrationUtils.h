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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@mozilla.com
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

#ifndef NSSVGINTEGRATIONUTILS_H_
#define NSSVGINTEGRATIONUTILS_H_

#include "nsPoint.h"
#include "nsRect.h"
#include "gfxRect.h"
#include "gfxMatrix.h"

class nsIFrame;
class nsDisplayListBuilder;
class nsDisplayList;
class nsIRenderingContext;

/***** Integration of SVG effects with regular frame painting *****/

class nsSVGIntegrationUtils
{
public:
  /**
   * Returns true if a non-SVG frame has SVG effects.
   */
  static PRBool
  UsingEffectsForFrame(const nsIFrame* aFrame);

  /**
   * Adjust overflow rect for effects.
   * XXX this is a problem. We really need to compute the effects rect for
   * a whole chain of frames for a given element at once. but we have no
   * way to do this effectively with Gecko's current reflow architecture.
   * See http://groups.google.com/group/mozilla.dev.tech.layout/msg/6b179066f3051f65
   */
  static nsRect
  ComputeFrameEffectsRect(nsIFrame* aFrame, const nsRect& aOverflowRect);
  /**
   * Adjust the frame's invalidation area to cover effects
   */
  static nsRect
  GetInvalidAreaForChangedSource(nsIFrame* aFrame, const nsRect& aInvalidRect);
  /**
   * Figure out which area of the source is needed given an area to
   * repaint
   */
  static nsRect
  GetRequiredSourceForInvalidArea(nsIFrame* aFrame, const nsRect& aDamageRect);
  /**
   * Returns true if the given point is not clipped out by effects.
   * @param aPt in appunits relative to aFrame
   */
  static PRBool
  HitTestFrameForEffects(nsIFrame* aFrame, const nsPoint& aPt);

  /**
   * Paint non-SVG frame with SVG effects.
   * @param aOffset the offset in appunits where aFrame should be positioned
   * in aCtx's coordinate system
   */
  static void
  PaintFramesWithEffects(nsIRenderingContext* aCtx,
                         nsIFrame* aEffectsFrame, const nsRect& aDirtyRect,
                         nsDisplayListBuilder* aBuilder,
                         nsDisplayList* aInnerList);

  static gfxMatrix
  GetInitialMatrix(nsIFrame* aNonSVGFrame);
  /**
   * Returns aNonSVGFrame's rect in CSS pixel units. This is the union
   * of all its continuations' rectangles. The top-left is always 0,0
   * since "user space" origin for non-SVG frames is the top-left of the
   * union of all the continuations' rectangles.
   */
  static gfxRect
  GetSVGRectForNonSVGFrame(nsIFrame* aNonSVGFrame);
  /**
   * Returns aNonSVGFrame's bounding box in CSS units. This is the union
   * of all its continuations' overflow areas, relative to the top-left
   * of all the continuations' rectangles.
   */
  static gfxRect
  GetSVGBBoxForNonSVGFrame(nsIFrame* aNonSVGFrame);
};

#endif /*NSSVGINTEGRATIONUTILS_H_*/
