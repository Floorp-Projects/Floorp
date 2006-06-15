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

// include math.h to pick up definition of M_PI if the platform defines it
#define _USE_MATH_DEFINES
#include <math.h>

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsISVGValue.h"
#include "nsRect.h"

class nsPresContext;
class nsIContent;
class nsStyleCoord;
class nsIDOMSVGRect;
class nsIDOMSVGPoint;
class nsFrameList;
class nsIFrame;
struct nsStyleSVGPaint;
class nsISVGGlyphFragmentLeaf;
class nsISVGGlyphFragmentNode;
class nsIDOMSVGLength;
class nsIDOMSVGMatrix;
class nsIURI;
class nsSVGOuterSVGFrame;
class nsISVGRendererSurface;
class nsIPresShell;
class nsISVGRendererCanvas;
class nsIDOMSVGAnimatedPreserveAspectRatio;
class nsISVGValueObserver;
class nsIAtom;
class nsSVGLength2;
class nsSVGElement;
class nsSVGCoordCtxProvider;
class nsAttrValue;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SVG Frame state bits
#define NS_STATE_IS_OUTER_SVG         0x00100000

#define NS_STATE_SVG_CLIPPED_TRIVIAL  0x00200000
#define NS_STATE_SVG_CLIPPED_COMPLEX  0x00400000
#define NS_STATE_SVG_CLIPPED_MASK     0x00600000

#define NS_STATE_SVG_FILTERED         0x00800000
#define NS_STATE_SVG_MASKED           0x01000000

#define NS_STATE_SVG_HAS_MARKERS      0x02000000

#define NS_STATE_SVG_DIRTY            0x04000000
#define NS_STATE_SVG_METRICS_DIRTY    0x08000000

#define NS_STATE_SVG_FILL_PSERVER     0x10000000
#define NS_STATE_SVG_STROKE_PSERVER   0x20000000
#define NS_STATE_SVG_PSERVER_MASK     0x30000000

/* are we the child of a non-display container? */
#define NS_STATE_SVG_NONDISPLAY_CHILD 0x40000000

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
   * Creates a bounding box by walking the children and doing union.
   */
  static nsresult GetBBox(nsFrameList *aFrames, nsIDOMSVGRect **_retval);

  /*
   * Returns the first child node for a frame
   */
  static nsISVGGlyphFragmentNode *
  GetFirstGlyphFragmentChildNode(nsFrameList *aFrames);

  /*
   * Returns the next child node for a frame
   */
  static nsISVGGlyphFragmentNode *
  GetNextGlyphFragmentChildNode(nsISVGGlyphFragmentNode *node);

  /*
   * Build the glyph fragment tree
   */
  static PRUint32
  BuildGlyphFragmentTree(nsFrameList *aFrames,
                         PRUint32 charNum,
                         PRBool lastBranch);

  /*
   * Returns the number of characters in a string
   */
  static PRUint32 GetNumberOfChars(nsFrameList *aFrames);

  /*
   * Determines the length of a string
   */
  static float GetComputedTextLength(nsFrameList *aFrames);

  /*
   * Determines the length of a substring
   */
  static nsresult GetSubStringLength(nsFrameList *aFrames,
                                     PRUint32 charnum,
                                     PRUint32 nchars,
                                     float *_retval);

  /*
   * Determines the length of a substring
   */
  static float GetSubStringLengthNoValidation(nsFrameList *aFrames,
                                              PRUint32 charnum,
                                              PRUint32 nchars);

  /*
   * Determines the start position of a character
   */
  static nsresult GetStartPositionOfChar(nsFrameList *aFrames,
                                         PRUint32 charnum,
                                         nsIDOMSVGPoint **_retval);

  /*
   * Determines the end position of a character
   */
  static nsresult GetEndPositionOfChar(nsFrameList *aFrames,
                                       PRUint32 charnum,
                                       nsIDOMSVGPoint **_retval);

  /*
   * Determines the bounds of a character
   */
  static nsresult GetExtentOfChar(nsFrameList *aFrames,
                                  PRUint32 charnum,
                                  nsIDOMSVGRect **_retval);

  /*
   * Determines the rotation of a character
   */
  static nsresult GetRotationOfChar(nsFrameList *aFrames,
                                    PRUint32 charnum,
                                    float *_retval);

  /*
   * Get the character at the specified position
   */
  static PRInt32 GetCharNumAtPosition(nsFrameList *aFrames,
                                      nsIDOMSVGPoint *point);

  /*
   * Figures out the worst case invalidation area for a frame, taking
   * into account filters.  Empty return if no filter in the hierarchy.
   */
  static nsRect FindFilterInvalidation(nsIFrame *aFrame);

  /* enum for specifying coordinate direction for ObjectSpace/UserSpace */
  enum ctxDirection { X, Y, XY };

  /* Computes the input length in terms of object space coordinates.
     Input: rect - bounding box
            length - length to be converted
  */
  static float ObjectSpace(nsIDOMSVGRect *aRect, nsSVGLength2 *aLength);

  /* Computes the input length in terms of user space coordinates.
     Input: content - object to be used for determining user space
            length - length to be converted
  */
  static float UserSpace(nsSVGElement *aSVGElement, nsSVGLength2 *aLength);

  /* Tranforms point by the matrix.  In/out: x,y */
  static void
  TransformPoint(nsIDOMSVGMatrix *matrix,
                 float *x, float *y);

  /* Returns the angle halfway between the two specified angles */
  static float
  AngleBisect(float a1, float a2);

  /* Generate a new rendering surface the size of the current surface */
  static nsresult GetSurface(nsSVGOuterSVGFrame *aOuterSVGFrame,
                             nsISVGRendererCanvas *aCanvas,
                             nsISVGRendererSurface **aSurface);

  /* Find the outermost SVG frame of the passed frame */
  static nsSVGOuterSVGFrame *
  GetOuterSVGFrame(nsIFrame *aFrame);

  /* Generate a viewbox to viewport tranformation matrix */
  
  static already_AddRefed<nsIDOMSVGMatrix>
  GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      nsIDOMSVGAnimatedPreserveAspectRatio *aPreserveAspectRatio,
                      PRBool aIgnoreAlign = PR_FALSE);

  /* Paint frame with SVG effects */
  static void
  PaintChildWithEffects(nsISVGRendererCanvas *aCanvas, nsIFrame *aFrame);

  /* Style change for effects (filter/clip/mask/opacity) - call when
   * the frame's style has changed to make sure the effects properties
   * stay in sync. */
  static void
  StyleEffects(nsIFrame *aFrame);

  /* Modification events for effects (filter/clip/mask/opacity) - call
   * when observers on effects get called to make sure properties stay
   * in sync. */
  static void
  WillModifyEffects(nsIFrame *aFrame, nsISVGValue *observable,
                    nsISVGValue::modificationType aModType);
  static void
  DidModifyEffects(nsIFrame *aFrame, nsISVGValue *observable,
                   nsISVGValue::modificationType aModType);

  /* Hit testing - check if point hits the clipPath of indicated
   * frame.  Returns true of no clipPath set. */

  static PRBool
  HitTestClip(nsIFrame *aFrame, float x, float y);

  /* Hit testing - check if point hits any children of frame. */
  
  static void
  HitTestChildren(nsIFrame *aFrame, float x, float y, nsIFrame **aResult);

  /* Add observation of an nsISVGValue to an nsISVGValueObserver */
  static void
  AddObserver(nsISupports *aObserver, nsISupports *aTarget);

  /* Remove observation of an nsISVGValue from an nsISVGValueObserver */
  static void
  RemoveObserver(nsISupports *aObserver, nsISupports *aTarget);

  /*
   * Returns the CanvasTM of the indicated frame, whether it's a
   * child or container SVG frame.
   */
  static already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM(nsIFrame *aFrame);

  /*
   * Get element's coordinate context provider.
   */
  static already_AddRefed<nsSVGCoordCtxProvider>
  GetCoordContextProvider(nsSVGElement *aElement);

  /*
   * Get frame's covered region by walking the children and doing union.
   */
  static nsRect
  GetCoveredRegion(const nsFrameList &aFrames);

  /*
   * Inflate a floating-point rect to a nsRect
   */
  static nsRect
  ToBoundingPixelRect(double xmin, double ymin, double xmax, double ymax);

private:
  /*
   * Returns the glyph fragment containing a particular character
   */
  static nsISVGGlyphFragmentLeaf *
  GetGlyphFragmentAtCharNum(nsISVGGlyphFragmentNode* node,
                            PRUint32 charnum,
                            PRUint32 *offset);
};

#endif
