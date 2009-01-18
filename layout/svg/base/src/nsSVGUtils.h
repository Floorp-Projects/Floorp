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
#include "nsRect.h"
#include "gfxContext.h"
#include "nsIRenderingContext.h"

class nsIDocument;
class nsPresContext;
class nsIContent;
class nsStyleCoord;
class nsIDOMSVGRect;
class nsFrameList;
class nsIFrame;
struct nsStyleSVGPaint;
class nsIDOMSVGElement;
class nsIDOMSVGLength;
class nsIDOMSVGMatrix;
class nsIURI;
class nsSVGOuterSVGFrame;
class nsIPresShell;
class nsSVGPreserveAspectRatio;
class nsIAtom;
class nsSVGLength2;
class nsSVGElement;
class nsSVGSVGElement;
class nsAttrValue;
class gfxContext;
class gfxASurface;
class gfxPattern;
class gfxImageSurface;
struct gfxRect;
struct gfxMatrix;
struct gfxSize;
struct gfxIntSize;
struct nsStyleFont;
class nsSVGEnum;
class nsISVGChildFrame;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SVG Frame state bits
#define NS_STATE_IS_OUTER_SVG         0x00100000

#define NS_STATE_SVG_DIRTY            0x00200000

/* are we the child of a non-display container? */
#define NS_STATE_SVG_NONDISPLAY_CHILD 0x00400000

#define NS_STATE_SVG_PROPAGATE_TRANSFORM 0x00800000

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

// maximum dimension of an offscreen surface - choose so that
// the surface size doesn't overflow a 32-bit signed int using
// 4 bytes per pixel; in line with gfxASurface::CheckSurfaceSize
#define NS_SVG_OFFSCREEN_MAX_DIMENSION 16384

/*
 * Checks the svg enable preference and if a renderer could
 * successfully be created.  Declared as a function instead of a
 * nsSVGUtil method so that files that can't pull in nsSVGUtils.h (due
 * to cairo.h usage) can still query this information.
 */
PRBool NS_SVGEnabled();

// GRRR WINDOWS HATE HATE HATE
#undef CLIP_MASK

class nsSVGRenderState
{
public:
  enum RenderMode { NORMAL, CLIP, CLIP_MASK };

  /**
   * Render SVG to a legacy rendering context
   */
  nsSVGRenderState(nsIRenderingContext *aContext);
  /**
   * Render SVG to a temporary surface
   */
  nsSVGRenderState(gfxASurface *aSurface);

  nsIRenderingContext *GetRenderingContext(nsIFrame *aFrame);
  gfxContext *GetGfxContext() { return mGfxContext; }

  void SetRenderMode(RenderMode aMode) { mRenderMode = aMode; }
  RenderMode GetRenderMode() { return mRenderMode; }

private:
  RenderMode                    mRenderMode;
  nsCOMPtr<nsIRenderingContext> mRenderingContext;
  nsRefPtr<gfxContext>          mGfxContext;
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

#define NS_ISVGFILTERPROPERTY_IID \
{ 0x9744ee20, 0x1bcf, 0x4c62, \
 { 0x86, 0x7d, 0xd3, 0x7a, 0x91, 0x60, 0x3e, 0xef } }

class nsISVGFilterProperty : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISVGFILTERPROPERTY_IID)
  virtual void Invalidate() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISVGFilterProperty, NS_ISVGFILTERPROPERTY_IID)

class nsSVGUtils
{
public:
  /*
   * Get a font-size (em) of an nsIContent
   */
  static float GetFontSize(nsIContent *aContent);
  static float GetFontSize(nsIFrame *aFrame);
  /*
   * Get an x-height of of an nsIContent
   */
  static float GetFontXHeight(nsIContent *aContent);
  static float GetFontXHeight(nsIFrame *aFrame);

  /*
   * Converts image data from premultipled to unpremultiplied alpha
   */
  static void UnPremultiplyImageDataAlpha(PRUint8 *data, 
                                          PRInt32 stride, 
                                          const nsIntRect &rect);
  /*
   * Converts image data from unpremultipled to premultiplied alpha
   */
  static void PremultiplyImageDataAlpha(PRUint8 *data, 
                                        PRInt32 stride, 
                                        const nsIntRect &rect);
  /*
   * Converts image data from premultiplied sRGB to Linear RGB
   */
  static void ConvertImageDataToLinearRGB(PRUint8 *data, 
                                          PRInt32 stride, 
                                          const nsIntRect &rect);
  /*
   * Converts image data from LinearRGB to premultiplied sRGB
   */
  static void ConvertImageDataFromLinearRGB(PRUint8 *data, 
                                            PRInt32 stride, 
                                            const nsIntRect &rect);

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
   * Return the nearest viewport element
   */
  static nsresult GetNearestViewportElement(nsIContent *aContent,
                                            nsIDOMSVGElement * *aNearestViewportElement);

  /*
   * Get the farthest viewport element
   */
  static nsresult GetFarthestViewportElement(nsIContent *aContent,
                                             nsIDOMSVGElement * *aFarthestViewportElement);

  /*
   * Creates a bounding box by walking the children and doing union.
   */
  static nsresult GetBBox(nsFrameList *aFrames, nsIDOMSVGRect **_retval);

  /**
   * Figures out the worst case invalidation area for a frame, taking
   * filters into account.
   * @param aRect the area in app units that needs to be invalidated in aFrame
   * @return the rect in app units that should be invalidated, taking
   * filters into account. Will return aRect when no filters are present.
   */
  static nsRect FindFilterInvalidation(nsIFrame *aFrame, const nsRect& aRect);

  /**
   * Invalidates the area covered by the frame
   */
  static void InvalidateCoveredRegion(nsIFrame *aFrame);

  /*
   * Update the area covered by the frame
   */
  static void UpdateGraphic(nsISVGChildFrame *aSVGFrame);

  /*
   * Update the filter invalidation region for ancestor frames, if relevant.
   */
  static void NotifyAncestorsOfFilterRegionChange(nsIFrame *aFrame);

  /* enum for specifying coordinate direction for ObjectSpace/UserSpace */
  enum ctxDirection { X, Y, XY };

  /**
   * Computes sqrt((aWidth^2 + aHeight^2)/2);
   */
  static double ComputeNormalizedHypotenuse(double aWidth, double aHeight);

  /* Computes the input length in terms of object space coordinates.
     Input: rect - bounding box
            length - length to be converted
  */
  static float ObjectSpace(nsIDOMSVGRect *aRect, const nsSVGLength2 *aLength);

  /* Computes the input length in terms of user space coordinates.
     Input: content - object to be used for determining user space
     Input: length - length to be converted
  */
  static float UserSpace(nsSVGElement *aSVGElement, const nsSVGLength2 *aLength);

  /* Computes the input length in terms of user space coordinates.
     Input: aFrame - object to be used for determining user space
            length - length to be converted
  */
  static float UserSpace(nsIFrame *aFrame, const nsSVGLength2 *aLength);

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

  /**
   * Get the covered region for a frame. Return null if it's not an SVG frame.
   * @param aRect gets a rectangle in app units
   * @return the outer SVG frame which aRect is relative to
   */
  static nsIFrame*
  GetOuterSVGFrameAndCoveredRegion(nsIFrame* aFrame, nsRect* aRect);

  /* Generate a viewbox to viewport tranformation matrix */
  
  static already_AddRefed<nsIDOMSVGMatrix>
  GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const nsSVGPreserveAspectRatio &aPreserveAspectRatio,
                      PRBool aIgnoreAlign = PR_FALSE);

  /* Paint SVG frame with SVG effects - aDirtyRect is the area being
   * redrawn, in device pixel coordinates relative to the outer svg */
  static void
  PaintFrameWithEffects(nsSVGRenderState *aContext,
                        const nsIntRect *aDirtyRect,
                        nsIFrame *aFrame);

  /* Hit testing - check if point hits the clipPath of indicated
   * frame.  Returns true if no clipPath set. */
  static PRBool
  HitTestClip(nsIFrame *aFrame, const nsPoint &aPoint);
  
  /* Hit testing - check if point hits any children of frame. */

  static nsIFrame *
  HitTestChildren(nsIFrame *aFrame, const nsPoint &aPoint);

  /*
   * Returns the CanvasTM of the indicated frame, whether it's a
   * child SVG frame, container SVG frame, or a regular frame.
   * For regular frames, we just return an identity matrix.
   */
  static already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM(nsIFrame *aFrame);

  /*
   * Tells child frames that something that might affect them has changed
   */
  static void
  NotifyChildrenOfSVGChange(nsIFrame *aFrame, PRUint32 aFlags);

  /*
   * Get frame's covered region by walking the children and doing union.
   */
  static nsRect
  GetCoveredRegion(const nsFrameList &aFrames);

  /*
   * Convert a rect from device pixel units to app pixel units by inflation.
   */
  static nsRect
  ToAppPixelRect(nsPresContext *aPresContext,
                 double xmin, double ymin, double xmax, double ymax);
  static nsRect
  ToAppPixelRect(nsPresContext *aPresContext, const gfxRect& rect);

  /*
   * Convert a surface size to an integer for use by thebes
   * possibly making it smaller in the process so the surface does not
   * use excessive memory.
   * @param aSize the desired surface size
   * @param aResultOverflows true if the desired surface size is too big
   * @return the surface size to use
   */
  static gfxIntSize
  ConvertToSurfaceSize(const gfxSize& aSize, PRBool *aResultOverflows);

  /*
   * Get a pointer to a surface that can be used to create thebes
   * contexts for various measurement purposes.
   */
  static gfxASurface *
  GetThebesComputationalSurface();

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


  static void CompositeSurfaceMatrix(gfxContext *aContext,
                                     gfxASurface *aSurface,
                                     nsIDOMSVGMatrix *aCTM, float aOpacity);

  static void CompositePatternMatrix(gfxContext *aContext,
                                     gfxPattern *aPattern,
                                     nsIDOMSVGMatrix *aCTM, float aWidth, float aHeight, float aOpacity);

  static void SetClipRect(gfxContext *aContext,
                          nsIDOMSVGMatrix *aCTM, float aX, float aY,
                          float aWidth, float aHeight);

  /**
   * If aIn can be represented exactly using an nsIntRect (i.e. integer-aligned edges and
   * coordinates in the PRInt32 range) then we set aOut to that rectangle, otherwise
   * return failure.
   */
  static nsresult GfxRectToIntRect(const gfxRect& aIn, nsIntRect* aOut);

  /**
   * Restricts aRect to pixels that intersect aGfxRect.
   */
  static void ClipToGfxRect(nsIntRect* aRect, const gfxRect& aGfxRect);

  /* Using group opacity instead of fill or stroke opacity on a
   * geometry object seems to be a common authoring mistake.  If we're
   * not applying filters and not both stroking and filling, we can
   * generate the same result without going through the overhead of a
   * push/pop group. */
  static PRBool
  CanOptimizeOpacity(nsIFrame *aFrame);

  /* Calculate the maximum expansion of a matrix */
  static float
  MaxExpansion(nsIDOMSVGMatrix *aMatrix);

  /* Take a CTM and adjust for object bounding box coordinates, if needed */
  static already_AddRefed<nsIDOMSVGMatrix>
  AdjustMatrixForUnits(nsIDOMSVGMatrix *aMatrix,
                       nsSVGEnum *aUnits,
                       nsIFrame *aFrame);

  /**
   * Get bounding-box for aFrame. Matrix propagation is disabled so the
   * bounding box is computed in terms of aFrame's own user space.
   */
  static already_AddRefed<nsIDOMSVGRect>
  GetBBox(nsIFrame *aFrame);
  /**
   * Compute a rectangle in userSpaceOnUse or objectBoundingBoxUnits.
   * @param aXYWH pointer to 4 consecutive nsSVGLength2 objects containing
   * the x, y, width and height values in that order
   * @param aBBox the bounding box of the object the rect is relative to;
   * may be null if aUnits is not SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
   * @param aFrame the object in which to interpret user-space units;
   * may be null if aUnits is SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
   */
  static gfxRect
  GetRelativeRect(PRUint16 aUnits, const nsSVGLength2 *aXYWH, nsIDOMSVGRect *aBBox,
                  nsIFrame *aFrame);

#ifdef DEBUG
  static void
  WritePPM(const char *fname, gfxImageSurface *aSurface);
#endif

private:
  /* Computational (nil) surfaces */
  static gfxASurface *mThebesComputationalSurface;
};

#endif
