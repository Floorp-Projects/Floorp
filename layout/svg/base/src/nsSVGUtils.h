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
#include "nsRenderingContext.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "nsStyleStruct.h"

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
class nsIURI;
class nsSVGOuterSVGFrame;
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
struct nsStyleFont;
class nsSVGEnum;
class nsISVGChildFrame;
class nsSVGGeometryFrame;
class nsSVGPathGeometryFrame;
class nsSVGDisplayContainerFrame;

namespace mozilla {
class SVGAnimatedPreserveAspectRatio;
class SVGPreserveAspectRatio;
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SVG Frame state bits
#define NS_STATE_IS_OUTER_SVG                    NS_FRAME_STATE_BIT(20)

/* are we the child of a non-display container? */
#define NS_STATE_SVG_NONDISPLAY_CHILD            NS_FRAME_STATE_BIT(22)

// If this bit is set, we are a <clipPath> element or descendant.
#define NS_STATE_SVG_CLIPPATH_CHILD              NS_FRAME_STATE_BIT(23)

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

inline bool
IsSVGWhitespace(char aChar)
{
  return aChar == '\x20' || aChar == '\x9' ||
         aChar == '\xD'  || aChar == '\xA';
}

inline bool
IsSVGWhitespace(PRUnichar aChar)
{
  return aChar == PRUnichar('\x20') || aChar == PRUnichar('\x9') ||
         aChar == PRUnichar('\xD')  || aChar == PRUnichar('\xA');
}

/*
 * Checks the smil enabled preference.  Declared as a function to match
 * NS_SVGEnabled().
 */
bool NS_SMILEnabled();


// GRRR WINDOWS HATE HATE HATE
#undef CLIP_MASK

class NS_STACK_CLASS SVGAutoRenderState
{
public:
  enum RenderMode { NORMAL, CLIP, CLIP_MASK };

  SVGAutoRenderState(nsRenderingContext *aContext, RenderMode aMode);
  ~SVGAutoRenderState();

  void SetPaintingToWindow(bool aPaintingToWindow);

  static RenderMode GetRenderMode(nsRenderingContext *aContext);
  static bool IsPaintingToWindow(nsRenderingContext *aContext);

private:
  nsRenderingContext *mContext;
  void *mOriginalRenderState;
  RenderMode mMode;
  bool mPaintingToWindow;
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
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;
  typedef mozilla::SVGPreserveAspectRatio SVGPreserveAspectRatio;

  /*
   * Get the parent element of an nsIContent
   */
  static mozilla::dom::Element *GetParentElement(nsIContent *aContent);

  /*
   * Get the outer SVG element of an nsIContent
   */
  static nsSVGSVGElement *GetOuterSVGElement(nsSVGElement *aSVGElement);

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

  static gfxMatrix GetCTM(nsSVGElement *aElement, bool aScreenCTM);

  /**
   * Check if this is one of the SVG elements that SVG 1.1 Full says
   * establishes a viewport: svg, symbol, image or foreignObject.
   */
  static bool EstablishesViewport(nsIContent *aContent);

  static already_AddRefed<nsIDOMSVGElement>
  GetNearestViewportElement(nsIContent *aContent);

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
   * Invalidates the area that is painted by the frame without updating its
   * bounds.
   *
   * This is similar to InvalidateOverflowRect(). It will go away when we
   * support display list based invalidation of SVG.
   */
  static void InvalidateBounds(nsIFrame *aFrame, bool aDuringUpdate = false);

  /**
   * Schedules an update of the frame's bounds (which will in turn invalidate
   * the new area that the frame should paint to).
   *
   * This does nothing when passed an NS_STATE_SVG_NONDISPLAY_CHILD frame.
   * In future we may want to allow UpdateBounds to be called on such frames,
   * but that would be better implemented as a ForceUpdateBounds function to
   * be called synchronously while painting them without marking or paying
   * attention to dirty bits like this function.
   *
   * This is very similar to PresShell::FrameNeedsReflow. The main reason that
   * we have this function instead of using FrameNeedsReflow is because we need
   * to be able to call it under nsSVGOuterSVGFrame::NotifyViewportChange when
   * that function is called by nsSVGOuterSVGFrame::Reflow. FrameNeedsReflow
   * is not suitable for calling during reflow though, and it asserts as much.
   * The reason that we want to be callable under NotifyViewportChange is
   * because we want to synchronously notify and dirty the nsSVGOuterSVGFrame's
   * children so that when nsSVGOuterSVGFrame::DidReflow is called its children
   * will be updated for the new size as appropriate. Otherwise we'd have to
   * post an event to the event loop to mark dirty flags and request an update.
   *
   * Another reason that we don't currently want to call
   * PresShell::FrameNeedsReflow is because passing eRestyle to it to get it to
   * mark descendants dirty would cause it to descend through
   * nsSVGForeignObjectFrame frames to mark their children dirty, but we want to
   * handle nsSVGForeignObjectFrame specially. It would also do unnecessary work
   * descending into NS_STATE_SVG_NONDISPLAY_CHILD frames.
   */
  static void ScheduleBoundsUpdate(nsIFrame *aFrame);

  /**
   * Invalidates the area that the frame last painted to, then schedules an
   * update of the frame's bounds (which will in turn invalidate the new area
   * that the frame should paint to).
   */
  static void InvalidateAndScheduleBoundsUpdate(nsIFrame *aFrame);

  /**
   * Returns true if the frame or any of its children need UpdateBounds
   * to be called on them.
   */
  static bool NeedsUpdatedBounds(nsIFrame *aFrame);

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
  GetViewBoxTransform(const nsSVGElement* aElement,
                      float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const SVGAnimatedPreserveAspectRatio &aPreserveAspectRatio);

  static gfxMatrix
  GetViewBoxTransform(const nsSVGElement* aElement,
                      float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const SVGPreserveAspectRatio &aPreserveAspectRatio);

  /* Paint SVG frame with SVG effects - aDirtyRect is the area being
   * redrawn, in device pixel coordinates relative to the outer svg */
  static void
  PaintFrameWithEffects(nsRenderingContext *aContext,
                        const nsIntRect *aDirtyRect,
                        nsIFrame *aFrame);

  /* Hit testing - check if point hits the clipPath of indicated
   * frame.  Returns true if no clipPath set. */
  static bool
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

  // Converts aPoint from an app unit point in outer-<svg> content rect space
  // to an app unit point in a frame's SVG userspace. 
  // This is a temporary helper we should no longer need after bug 614732 is
  // fixed.
  static nsPoint
  TransformOuterSVGPointToChildFrame(nsPoint aPoint,
                                     const gfxMatrix& aFrameToCanvasTM,
                                     nsPresContext* aPresContext);

  static nsRect
  TransformFrameRectToOuterSVG(const nsRect& aRect,
                               const gfxMatrix& aMatrix,
                               nsPresContext* aPresContext);

  /*
   * Convert a surface size to an integer for use by thebes
   * possibly making it smaller in the process so the surface does not
   * use excessive memory.
   *
   * @param aSize the desired surface size
   * @param aResultOverflows true if the desired surface size is too big
   * @return the surface size to use
   */
  static gfxIntSize ConvertToSurfaceSize(const gfxSize& aSize,
                                         bool *aResultOverflows);

  /*
   * Hit test a given rectangle/matrix.
   */
  static bool
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
  static bool
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

  enum BBoxFlags {
    eBBoxIncludeFill          = 1 << 0,
    eBBoxIgnoreFillIfNone     = 1 << 1,
    eBBoxIncludeStroke        = 1 << 2,
    eBBoxIgnoreStrokeIfNone   = 1 << 3,
    eBBoxIncludeMarkers       = 1 << 4
  };
  /**
   * Get the SVG bbox (the SVG spec's simplified idea of bounds) of aFrame in
   * aFrame's userspace.
   */
  static gfxRect GetBBox(nsIFrame *aFrame, PRUint32 aFlags = eBBoxIncludeFill);

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

  static bool OuterSVGIsCallingUpdateBounds(nsIFrame *aFrame);
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
                                               nsSVGGeometryFrame* aFrame,
                                               const gfxMatrix& aMatrix);
  static gfxRect PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                                               nsSVGPathGeometryFrame* aFrame,
                                               const gfxMatrix& aMatrix);

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
   * Given a nsIContent* that is actually an nsSVGSVGElement*, this method
   * checks whether it currently has a valid viewBox, and returns true if so.
   *
   * No other type of element should be passed to this method.
   * (In debug builds, anything non-<svg> will trigger an abort; in non-debug
   * builds, it will trigger a false return-value as a safe fallback.)
   */
  static bool RootSVGElementHasViewbox(const nsIContent *aRootSVGElem);

  static void GetFallbackOrPaintColor(gfxContext *aContext,
                                      nsStyleContext *aStyleContext,
                                      nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                      float *aOpacity, nscolor *color);
};

#endif
