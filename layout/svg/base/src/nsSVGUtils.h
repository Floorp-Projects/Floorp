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

#ifndef NS_SVGUTILS_H
#define NS_SVGUTILS_H

// Need this to get nsPresContext
#include "nsContentUtils.h"

class nsPresContext;
class nsIContent;
class nsStyleCoord;
class nsIDOMSVGRect;
class nsFrameList;
class nsIFrame;
struct nsStyleSVGPaint;
class nsISVGRendererRegion;
class nsIDOMSVGLength;
class nsIDOMSVGMatrix;
class nsIURI;

class nsSVGUtils
{
public:
  /* Checks the svg enable preference and if a renderer could
   * successfully be created.
   */
  static PRBool SVGEnabled();

  /*
   * Converts a nsStyleCoord into a userspace value.  Handles units
   * Factor (straight userspace), Coord (dimensioned), and Percent (of
   * the current SVG viewport)
   */
  static float CoordToFloat(nsPresContext *aPresContext, nsIContent *aContent,
                            const nsStyleCoord &aCoord);
  /*
   * Gets an internal frame for an element referenced by a URI.  Note that this
   * only works for URIs that reference elements within the same document.
   */
  static nsresult GetReferencedFrame(nsIFrame **aRefFrame, nsIURI* aURI,
                                     nsIContent *aContent, nsIPresShell *aPresShell);
  /*
   * For SVGPaint attributes (fills, strokes), return the type of the Paint.  This
   * is an expanded type that includes whether this is a solid fill, a gradient, or
   * a pattern.
   */
  static nsresult GetPaintType(PRUint16 *aPaintType, const nsStyleSVGPaint& aPaint, 
                               nsIContent *aContent, nsIPresShell *aPresShell);

  /*
   * Creates a bounding box by walking the children and doing union.
   */
  static nsresult GetBBox(nsFrameList *aFrames, nsIDOMSVGRect **_retval);

  /*
   * Figures out the worst case invalidation area for a frame, taking
   * into account filters.  Null return if no filter in the hierarcy.
   */
  static void FindFilterInvalidation(nsIFrame *aFrame,
                                     nsISVGRendererRegion **aRegion);

  /* enum for specifying coordinate direction for ObjectSpace/UserSpace */
  enum ctxDirection { X, Y, XY };

  /* Computes the input length in terms of object space coordinates.
     Input: rect - bounding box
            length - length to be converted
            direction - direction of length
  */
  static float ObjectSpace(nsIDOMSVGRect *rect,
                           nsIDOMSVGLength *length,
                           ctxDirection direction);

  /* Computes the input length in terms of user space coordinates.
     Input: content - object to be used for determining user space
            length - length to be converted
            direction - direction of length
  */
  static float UserSpace(nsIContent *content,
                         nsIDOMSVGLength *length,
                         ctxDirection direction);

  /* Tranforms point by the matrix.  In/out: x,y */
  static void
  TransformPoint(nsIDOMSVGMatrix *matrix,
                 float *x, float *y);
};

#endif
