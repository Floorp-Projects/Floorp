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
#include "cairo.h"

class nsIDocument;
class nsPresContext;
class nsIContent;
class nsStyleCoord;
class nsIDOMSVGRect;
class nsFrameList;
class nsIFrame;
struct nsStyleSVGPaint;
class nsIDOMSVGLength;
class nsIDOMSVGMatrix;
class nsIURI;
class nsSVGOuterSVGFrame;
class nsIPresShell;
class nsIDOMSVGAnimatedPreserveAspectRatio;
class nsISVGValueObserver;
class nsIAtom;
class nsSVGLength2;
class nsSVGElement;
class nsSVGSVGElement;
class nsAttrValue;
class gfxContext;
class gfxASurface;
class nsIRenderingContext;
struct gfxRect;
struct gfxMatrix;

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

#define NS_STATE_SVG_FILL_PSERVER     0x08000000
#define NS_STATE_SVG_STROKE_PSERVER   0x10000000
#define NS_STATE_SVG_PSERVER_MASK     0x18000000

/* are we the child of a non-display container? */
#define NS_STATE_SVG_NONDISPLAY_CHILD 0x20000000

/**
 * Byte offsets of channels in a native packed gfxColor or cairo image surface.
 */
#ifdef IS_BIG_ENDIAN
#define GFX_ARGB32_OFFSET_A 0
#define GFX_ARGB32_OFFSET_R 1
#define GFX_ARGB32_OFFSET_G 2
#define GFX_ARGB32_OFFSET_B 3
#else
#define GFX_ARGB32_OFFSET_A 3
#define GFX_ARGB32_OFFSET_R 2
#define GFX_ARGB32_OFFSET_G 1
#define GFX_ARGB32_OFFSET_B 0
#endif

/*
 * Checks the svg enable preference and if a renderer could
 * successfully be created.  Declared as a function instead of a
 * nsSVGUtil method so that files that can't pull in nsSVGUtils.h (due
 * to cairo.h usage) can still query this information.
 */
PRBool NS_SVGEnabled();

class nsSVGRenderState
{
public:
  enum RenderMode { NORMAL, CLIP, CLIP_MASK };

  nsSVGRenderState(nsIRenderingContext *aContext);
  nsSVGRenderState(gfxContext *aContext);

  nsIRenderingContext *GetRenderingContext() { return mRenderingContext; }
  gfxContext *GetGfxContext() { return mGfxContext; }

  void SetRenderMode(RenderMode aMode) { mRenderMode = aMode; }
  RenderMode GetRenderMode() { return mRenderMode; }

private:
  RenderMode           mRenderMode;
  nsIRenderingContext *mRenderingContext;
  gfxContext          *mGfxContext;
};

class nsAutoSVGRenderMode
{
public:
  nsAutoSVGRenderMode(nsSVGRenderState *aState,
                      nsSVGRenderState::RenderMode aMode) : mState(aState) {
    mOriginalMode = aState->GetRenderMode();
    aState->SetRenderMode(aMode);
  }
  ~nsAutoSVGRenderMode() { mState->SetRenderMode(mOriginalMode); }

private:
  nsSVGRenderState            *mState;
  nsSVGRenderState::RenderMode mOriginalMode;
};

class nsSVGUtils
{
public:
  /*
   * Converts image data from premultipled to unpremultiplied alpha
   */
  static void UnPremultiplyImageDataAlpha(PRUint8 *data, 
                                          PRInt32 stride, 
                                          const nsRect &rect);
  /*
   * Converts image data from unpremultipled to premultiplied alpha
   */
  static void PremultiplyImageDataAlpha(PRUint8 *data, 
                                        PRInt32 stride, 
                                        const nsRect &rect);
  /*
   * Converts image data from premultiplied sRGB to Linear RGB
   */
  static void ConvertImageDataToLinearRGB(PRUint8 *data, 
                                          PRInt32 stride, 
                                          const nsRect &rect);
  /*
   * Converts image data from LinearRGB to premultiplied sRGB
   */
  static void ConvertImageDataFromLinearRGB(PRUint8 *data, 
                                            PRInt32 stride, 
                                            const nsRect &rect);

  /*
   * Report a localized error message to the error console.
   */
  static nsresult ReportToConsole(nsIDocument* doc,
                                  const char* aWarning,
                                  const PRUnichar **aParams,
                                  PRUint32 aParamsLength);

  /*
   * Converts a nsStyleCoord into a userspace value.  Handles units
   * Factor (straight userspace), Coord (dimensioned), and Percent (of
   * the current SVG viewport)
   */
  static float CoordToFloat(nsPresContext *aPresContext,
                            nsSVGElement *aContent,
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

  /* Paint frame with SVG effects - aDirtyRect is the area being
   * redrawn, in frame offset pixel coordinates */
  static void
  PaintChildWithEffects(nsSVGRenderState *aContext,
                        nsRect *aDirtyRect,
                        nsIFrame *aFrame);

  /* Style change for effects (filter/clip/mask/opacity) - call when
   * the frame's style has changed to make sure the effects properties
   * stay in sync. */
  static void
  StyleEffects(nsIFrame *aFrame);

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
   * Get frame's covered region by walking the children and doing union.
   */
  static nsRect
  GetCoveredRegion(const nsFrameList &aFrames);

  /*
   * Inflate a floating-point rect to a nsRect
   */
  static nsRect
  ToBoundingPixelRect(double xmin, double ymin, double xmax, double ymax);
  static nsRect
  ToBoundingPixelRect(const gfxRect& rect);

  /*
   * Get a pointer to a surface that can be used to create cairo
   * contexts for various measurement purposes.
   */
  static cairo_surface_t *
  GetCairoComputationalSurface();
  static gfxASurface *
  GetThebesComputationalSurface();

  /*
   * Convert a nsIDOMSVGMatrix to a cairo_matrix_t.
   */
  static cairo_matrix_t
  ConvertSVGMatrixToCairo(nsIDOMSVGMatrix *aMatrix);

  /*
   * Convert a nsIDOMSVGMatrix to a gfxMatrix.
   */
  static gfxMatrix
  ConvertSVGMatrixToThebes(nsIDOMSVGMatrix *aMatrix);

  /*
   * Hit test a given rectangle/matrix.
   */
  static PRBool
  HitTestRect(nsIDOMSVGMatrix *aMatrix,
              float aRX, float aRY, float aRWidth, float aRHeight,
              float aX, float aY);

  /*
   * Convert a rectangle from cairo user space to device space.
   */
  static void
  UserToDeviceBBox(cairo_t *ctx,
                   double *xmin, double *ymin,
                   double *xmax, double *ymax);

  static void CompositeSurfaceMatrix(gfxContext *aContext,
                                     gfxASurface *aSurface,
                                     nsIDOMSVGMatrix *aCTM, float aOpacity);

  static void SetClipRect(gfxContext *aContext,
                          nsIDOMSVGMatrix *aCTM, float aX, float aY,
                          float aWidth, float aHeight);

  /* Using group opacity instead of fill or stroke opacity on a
   * geometry object seems to be a common authoring mistake.  If we're
   * not applying filters and not both stroking and filling, we can
   * generate the same result without going through the overhead of a
   * push/pop group. */
  static PRBool
  CanOptimizeOpacity(nsIFrame *aFrame);

private:
  /* Computational (nil) surfaces */
  static cairo_surface_t *mCairoComputationalSurface;
  static gfxASurface *mThebesComputationalSurface;
};

#endif
