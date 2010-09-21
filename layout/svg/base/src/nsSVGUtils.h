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

// include math.h to pick up definition of M_SQRT1_2 if the platform defines it
#define _USE_MATH_DEFINES
#include <math.h>

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "gfxContext.h"
#include "nsIRenderingContext.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "nsSVGMatrix.h"

class nsIDocument;
class nsPresContext;
class nsIContent;
class nsStyleContext;
class nsStyleCoord;
class nsFrameList;
class nsIFrame;
struct nsStyleSVGPaint;
class nsIDOMSVGElement;
class nsIDOMSVGLength;
class nsIDOMSVGNumberList;
class nsIURI;
class nsSVGOuterSVGFrame;
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
struct gfxSize;
struct gfxIntSize;
struct nsStyleFont;
class nsSVGEnum;
class nsISVGChildFrame;
class nsSVGGeometryFrame;
class nsSVGDisplayContainerFrame;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SVG Frame state bits
#define NS_STATE_IS_OUTER_SVG         NS_FRAME_STATE_BIT(20)

#define NS_STATE_SVG_DIRTY            NS_FRAME_STATE_BIT(21)

/* are we the child of a non-display container? */
#define NS_STATE_SVG_NONDISPLAY_CHILD NS_FRAME_STATE_BIT(22)

#define NS_STATE_SVG_PROPAGATE_TRANSFORM NS_FRAME_STATE_BIT(23)

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
// In fact Macs can't even manage that
#define NS_SVG_OFFSCREEN_MAX_DIMENSION 4096

#define SVG_WSP_DELIM       "\x20\x9\xD\xA"
#define SVG_COMMA_WSP_DELIM "," SVG_WSP_DELIM

inline PRBool
IsSVGWhitespace(char aChar)
{
  return aChar == '\x20' || aChar == '\x9' ||
         aChar == '\xD'  || aChar == '\xA';
}

/*
 * Checks the svg enable preference and if a renderer could
 * successfully be created.  Declared as a function instead of a
 * nsSVGUtil method so that files that can't pull in nsSVGUtils.h (due
 * to cairo.h usage) can still query this information.
 */
PRBool NS_SVGEnabled();

#ifdef MOZ_SMIL
/*
 * Checks the smil enabled preference.  Declared as a function to match
 * NS_SVGEnabled().
 */
PRBool NS_SMILEnabled();
#endif // MOZ_SMIL

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
   * Render SVG to a modern rendering context
   */
  nsSVGRenderState(gfxContext *aContext);
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
   * Get the parent element of an nsIContent
   */
  static mozilla::dom::Element *GetParentElement(nsIContent *aContent);

  /*
   * Get the number of CSS px (user units) per em (i.e. the em-height in user
   * units) for an nsIContent
   *
   * XXX document the conditions under which these may fail, and what they
   * return in those cases.
   */
  static float GetFontSize(mozilla::dom::Element *aElement);
  static float GetFontSize(nsIFrame *aFrame);
  static float GetFontSize(nsStyleContext *aStyleContext);
  /*
   * Get the number of CSS px (user units) per ex (i.e. the x-height in user
   * units) for an nsIContent
   *
   * XXX document the conditions under which these may fail, and what they
   * return in those cases.
   */
  static float GetFontXHeight(mozilla::dom::Element *aElement);
  static float GetFontXHeight(nsIFrame *aFrame);
  static float GetFontXHeight(nsStyleContext *aStyleContext);

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

  static gfxMatrix GetCTM(nsSVGElement *aElement, PRBool aScreenCTM);

  /**
   * Check if this is one of the SVG elements that SVG 1.1 Full says
   * establishes a viewport: svg, symbol, image or foreignObject.
   */
  static PRBool EstablishesViewport(nsIContent *aContent);

  static already_AddRefed<nsIDOMSVGElement>
  GetNearestViewportElement(nsIContent *aContent);

  static already_AddRefed<nsIDOMSVGElement>
  GetFarthestViewportElement(nsIContent *aContent);

  /**
   * Gets the nearest nsSVGInnerSVGFrame or nsSVGOuterSVGFrame frame. aFrame
   * must be an SVG frame. If aFrame is of type nsGkAtoms::svgOuterSVGFrame,
   * returns nsnull.
   */
  static nsSVGDisplayContainerFrame* GetNearestSVGViewport(nsIFrame *aFrame);
  
  /**
   * Figures out the worst case invalidation area for a frame, taking
   * filters into account.
   * Note that the caller is responsible for making sure that any cached
   * covered regions in the frame tree rooted at aFrame are up to date.
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
  static float ObjectSpace(const gfxRect &aRect, const nsSVGLength2 *aLength);

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
  
  static gfxMatrix
  GetViewBoxTransform(nsSVGElement* aElement,
                      float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const nsSVGPreserveAspectRatio &aPreserveAspectRatio);

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
  static gfxMatrix GetCanvasTM(nsIFrame* aFrame);

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
   *
   * XXXdholbert Putting impl in header file so that imagelib can call this
   * method.  Once we switch to a libxul-only world, this can go back into
   * the .cpp file.
   *
   * @param aSize the desired surface size
   * @param aResultOverflows true if the desired surface size is too big
   * @return the surface size to use
   */
  static gfxIntSize ConvertToSurfaceSize(const gfxSize& aSize,
                                  PRBool *aResultOverflows)
  {
    gfxIntSize surfaceSize(ClampToInt(aSize.width), ClampToInt(aSize.height));

    *aResultOverflows = surfaceSize.width != NS_round(aSize.width) ||
      surfaceSize.height != NS_round(aSize.height);

    if (!gfxASurface::CheckSurfaceSize(surfaceSize)) {
      surfaceSize.width = NS_MIN(NS_SVG_OFFSCREEN_MAX_DIMENSION,
                                 surfaceSize.width);
      surfaceSize.height = NS_MIN(NS_SVG_OFFSCREEN_MAX_DIMENSION,
                                  surfaceSize.height);
      *aResultOverflows = PR_TRUE;
    }

    return surfaceSize;
  }

  /*
   * Convert a nsIDOMSVGMatrix to a gfxMatrix.
   */
  static gfxMatrix
  ConvertSVGMatrixToThebes(nsIDOMSVGMatrix *aMatrix);

  /*
   * Hit test a given rectangle/matrix.
   */
  static PRBool
  HitTestRect(const gfxMatrix &aMatrix,
              float aRX, float aRY, float aRWidth, float aRHeight,
              float aX, float aY);


  /**
   * Get the clip rect for the given frame, taking into account the CSS 'clip'
   * property. See:
   * http://www.w3.org/TR/SVG11/masking.html#OverflowAndClipProperties
   * The arguments for aX, aY, aWidth and aHeight should be the dimensions of
   * the viewport established by aFrame.
   */
  static gfxRect
  GetClipRectForFrame(nsIFrame *aFrame,
                      float aX, float aY, float aWidth, float aHeight);

  static void CompositeSurfaceMatrix(gfxContext *aContext,
                                     gfxASurface *aSurface,
                                     const gfxMatrix &aCTM, float aOpacity);

  static void CompositePatternMatrix(gfxContext *aContext,
                                     gfxPattern *aPattern,
                                     const gfxMatrix &aCTM, float aWidth, float aHeight, float aOpacity);

  static void SetClipRect(gfxContext *aContext,
                          const gfxMatrix &aCTM,
                          const gfxRect &aRect);

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
  MaxExpansion(const gfxMatrix &aMatrix);

  /**
   * Take the CTM to userspace for an element, and adjust it to a CTM to its
   * object bounding box space if aUnits is SVG_UNIT_TYPE_OBJECTBOUNDINGBOX.
   * (I.e. so that [0,0] is at the top left of its bbox, and [1,1] is at the
   * bottom right of its bbox).
   *
   * If the bbox is empty, this will return a singular matrix.
   */
  static gfxMatrix
  AdjustMatrixForUnits(const gfxMatrix &aMatrix,
                       nsSVGEnum *aUnits,
                       nsIFrame *aFrame);

  /**
   * Get bounding-box for aFrame. Matrix propagation is disabled so the
   * bounding box is computed in terms of aFrame's own user space.
   */
  static gfxRect GetBBox(nsIFrame *aFrame);
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
  GetRelativeRect(PRUint16 aUnits, const nsSVGLength2 *aXYWH,
                  const gfxRect &aBBox, nsIFrame *aFrame);

  /**
   * Find the first frame, starting with aStartFrame and going up its
   * parent chain, that is not an svgAFrame.
   */
  static nsIFrame* GetFirstNonAAncestorFrame(nsIFrame* aStartFrame);

#ifdef DEBUG
  static void
  WritePPM(const char *fname, gfxImageSurface *aSurface);
#endif

  /**
   * Compute the maximum possible device space stroke extents of a path given
   * the path's device space path extents, its stroke style and its ctm.
   *
   * This is a workaround for the lack of suitable cairo API for getting the
   * tight device space stroke extents of a path. This basically gives us the
   * tightest extents that we can guarantee fully enclose the inked stroke
   * without doing the calculations for the actual tight extents. We exploit
   * the fact that cairo does have an API for getting the tight device space
   * fill/path extents.
   *
   * This should die once bug 478152 is fixed.
   */
  static gfxRect PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                                               nsSVGGeometryFrame* aFrame);

  /**
   * Returns true if aContent is an SVG <svg> element that is the child of
   * another non-foreignObject SVG element.
   */
  static PRBool IsInnerSVG(nsIContent* aContent);

  /**
   * Parse a string that may contain either a CSS <number> or, if
   * aAllowPercentages is set to true, a CSS <percentage>, and return the
   * number as a float.
   *
   * This helper returns PR_TRUE if a number was successfully parsed from the
   * string and no characters were left, else it returns PR_FALSE.
   */
  static PRBool NumberFromString(const nsAString& aString, float* aValue,
                                 PRBool aAllowPercentages = PR_FALSE);

  /**
   * Convert a floating-point value to a 32-bit integer value, clamping to
   * the range of valid integers.
   */
  static PRInt32 ClampToInt(double aVal)
  {
    return NS_lround(NS_MAX(double(PR_INT32_MIN),
                            NS_MIN(double(PR_INT32_MAX), aVal)));
  }

  /**
   * Returns aIndex-th item of nsIDOMSVGNumberList
   */
  static float GetNumberListValue(nsIDOMSVGNumberList *aList, PRUint32 aIndex);

private:
  /* Computational (nil) surfaces */
  static gfxASurface *gThebesComputationalSurface;
};

#endif
