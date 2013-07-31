/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGUTILS_H
#define NS_SVGUTILS_H

// include math.h to pick up definition of M_ maths defines e.g. M_PI
#define _USE_MATH_DEFINES
#include <math.h>

#include "gfxFont.h"
#include "gfxMatrix.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "nsAlgorithm.h"
#include "nsChangeHint.h"
#include "nsColor.h"
#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsISupportsBase.h"
#include "nsMathUtils.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsStyleStruct.h"
#include "mozilla/Constants.h"
#include <algorithm>

class gfxASurface;
class gfxContext;
class gfxImageSurface;
class gfxPattern;
class nsFrameList;
class nsIContent;
class nsIDocument;
class nsIFrame;
class nsPresContext;
class nsRenderingContext;
class nsStyleContext;
class nsStyleCoord;
class nsSVGDisplayContainerFrame;
class nsSVGElement;
class nsSVGEnum;
class nsSVGGeometryFrame;
class nsSVGLength2;
class nsSVGOuterSVGFrame;
class nsSVGPathGeometryFrame;
class nsTextFrame;
class gfxTextObjectPaint;

struct nsStyleSVG;
struct nsStyleSVGPaint;

namespace mozilla {
class SVGAnimatedPreserveAspectRatio;
class SVGPreserveAspectRatio;
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

// SVG Frame state bits
#define NS_STATE_IS_OUTER_SVG                    NS_FRAME_STATE_BIT(20)

// If this bit is set, we are a <clipPath> element or descendant.
#define NS_STATE_SVG_CLIPPATH_CHILD              NS_FRAME_STATE_BIT(21)

/**
 * For text, the NS_FRAME_IS_DIRTY and NS_FRAME_HAS_DIRTY_CHILDREN bits indicate
 * that our anonymous block child needs to be reflowed, and that mPositions
 * will likely need to be updated as a consequence. These are set, for
 * example, when the font-family changes. Sometimes we only need to
 * update mPositions though. For example if the x/y attributes change.
 * mPositioningDirty is used to indicate this latter "things are dirty" case
 * to allow us to avoid reflowing the anonymous block when it is not
 * necessary.
 */
#define NS_STATE_SVG_POSITIONING_DIRTY           NS_FRAME_STATE_BIT(22)

/**
 * For text, whether the values from x/y/dx/dy attributes have any percentage values
 * that are used in determining the positions of glyphs.  The value will
 * be true even if a positioning value is overridden by a descendant element's
 * attribute with a non-percentage length.  For example,
 * NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES would be set for:
 *
 *   <text x="10%"><tspan x="0">abc</tspan></text>
 *
 * Percentage values beyond the number of addressable characters, however, do
 * not influence NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES.  For example,
 * NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES would be false for:
 *
 *   <text x="10 20 30 40%">abc</text>
 *
 * NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES is used to determine whether
 * to recompute mPositions when the viewport size changes.  So although the 
 * first example above shows that NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES
 * can be true even if a viewport size change will not affect mPositions,
 * determining a completley accurate value for
 * NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES would require extra work that is
 * probably not worth it.
 */
#define NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES NS_FRAME_STATE_BIT(23)

#define NS_STATE_SVG_TEXT_IN_REFLOW              NS_FRAME_STATE_BIT(24)

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

#define SVG_HIT_TEST_FILL        0x01
#define SVG_HIT_TEST_STROKE      0x02
#define SVG_HIT_TEST_CHECK_MRECT 0x04


bool NS_SVGDisplayListHitTestingEnabled();
bool NS_SVGDisplayListPaintingEnabled();
bool NS_SVGTextCSSFramesEnabled();

/**
 * Sometimes we need to distinguish between an empty box and a box
 * that contains an element that has no size e.g. a point at the origin.
 */
class SVGBBox {
public:
  SVGBBox() 
    : mIsEmpty(true) {}

  SVGBBox(const gfxRect& aRect) 
    : mBBox(aRect), mIsEmpty(false) {}

  SVGBBox& operator=(const gfxRect& aRect) {
    mBBox = aRect;
    mIsEmpty = false;
    return *this;
  }

  operator const gfxRect& () const {
    return mBBox;
  }

  bool IsEmpty() const {
    return mIsEmpty;
  }

  void UnionEdges(const SVGBBox& aSVGBBox) {
    if (aSVGBBox.mIsEmpty) {
      return;
    }
    mBBox = mIsEmpty ? aSVGBBox.mBBox : mBBox.UnionEdges(aSVGBBox.mBBox);
    mIsEmpty = false;
  }

private:
  gfxRect mBBox;
  bool    mIsEmpty;
};

// GRRR WINDOWS HATE HATE HATE
#undef CLIP_MASK

class MOZ_STACK_CLASS SVGAutoRenderState
{
public:
  enum RenderMode {
    /**
     * Used to inform SVG frames that they should paint as normal.
     */
    NORMAL, 
    /** 
     * Used to inform SVG frames when they are painting as the child of a
     * simple clipPath. In this case they should only draw their basic geometry
     * as a path. They should not fill, stroke, or paint anything else.
     */
    CLIP, 
    /** 
     * Used to inform SVG frames when they are painting as the child of a
     * complex clipPath that requires the use of a clip mask. In this case they
     * should only draw their basic geometry as a path and then fill it using
     * fully opaque white. They should not stroke, or paint anything else.
     */
    CLIP_MASK 
  };

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

/**
 * General functions used by all of SVG layout and possibly content code.
 * If a method is used by content and depends only on other content methods
 * it should go in SVGContentUtils instead.
 */
class nsSVGUtils
{
public:
  typedef mozilla::dom::Element Element;

  static void Init();

  /*
   * Converts image data from premultipled to unpremultiplied alpha
   */
  static void UnPremultiplyImageDataAlpha(uint8_t *data, 
                                          int32_t stride, 
                                          const nsIntRect &rect);
  /*
   * Converts image data from unpremultipled to premultiplied alpha
   */
  static void PremultiplyImageDataAlpha(uint8_t *data, 
                                        int32_t stride, 
                                        const nsIntRect &rect);
  /*
   * Converts image data from premultiplied sRGB to Linear RGB
   */
  static void ConvertImageDataToLinearRGB(uint8_t *data, 
                                          int32_t stride, 
                                          const nsIntRect &rect);
  /*
   * Converts image data from LinearRGB to premultiplied sRGB
   */
  static void ConvertImageDataFromLinearRGB(uint8_t *data, 
                                            int32_t stride, 
                                            const nsIntRect &rect);

  /*
   * Converts image data from sRGB to luminance
   */
  static void ComputesRGBLuminanceMask(uint8_t *aData,
                                       int32_t aStride,
                                       const nsIntRect &aRect,
                                       float aOpacity);

  /*
   * Converts image data from sRGB to luminance assuming
   * Linear RGB Interpolation
   */
  static void ComputeLinearRGBLuminanceMask(uint8_t *aData,
                                            int32_t aStride,
                                            const nsIntRect &aRect,
                                            float aOpacity);
  /*
   * Converts image data to luminance using the value of alpha as luminance
   */
  static void ComputeAlphaMask(uint8_t *aData,
                               int32_t aStride,
                               const nsIntRect &aRect,
                               float aOpacity);

  /*
   * Converts a nsStyleCoord into a userspace value.  Handles units
   * Factor (straight userspace), Coord (dimensioned), and Percent (of
   * the current SVG viewport)
   */
  static float CoordToFloat(nsPresContext *aPresContext,
                            nsSVGElement *aContent,
                            const nsStyleCoord &aCoord);

  /**
   * Gets the nearest nsSVGInnerSVGFrame or nsSVGOuterSVGFrame frame. aFrame
   * must be an SVG frame. If aFrame is of type nsGkAtoms::svgOuterSVGFrame,
   * returns nullptr.
   */
  static nsSVGDisplayContainerFrame* GetNearestSVGViewport(nsIFrame *aFrame);

  /**
   * Returns the frame's post-filter visual overflow rect when passed the
   * frame's pre-filter visual overflow rect. If the frame is not currently
   * being filtered, this function simply returns aUnfilteredRect.
   */
  static nsRect GetPostFilterVisualOverflowRect(nsIFrame *aFrame,
                                                const nsRect &aUnfilteredRect);

  /**
   * Schedules an update of the frame's bounds (which will in turn invalidate
   * the new area that the frame should paint to).
   *
   * This does nothing when passed an NS_FRAME_IS_NONDISPLAY frame.
   * In future we may want to allow ReflowSVG to be called on such frames,
   * but that would be better implemented as a ForceReflowSVG function to
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
   * descending into NS_FRAME_IS_NONDISPLAY frames.
   */
  static void ScheduleReflowSVG(nsIFrame *aFrame);

  /**
   * Returns true if the frame or any of its children need ReflowSVG
   * to be called on them.
   */
  static bool NeedsReflowSVG(nsIFrame *aFrame);

  /*
   * Update the filter invalidation region for ancestor frames, if relevant.
   */
  static void NotifyAncestorsOfFilterRegionChange(nsIFrame *aFrame);

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
  static gfxMatrix GetCanvasTM(nsIFrame* aFrame, uint32_t aFor);

  /**
   * Returns the transform from aFrame's user space to canvas space. Only call
   * with SVG frames. This is like GetCanvasTM, except that it only includes
   * the transforms from aFrame's user space (i.e. the coordinate context
   * established by its 'transform' attribute, or else the coordinate context
   * that its _parent_ establishes for its children) to outer-<svg> device
   * space. Specifically, it does not include any other transforms introduced
   * by the frame such as x/y offsets and viewBox attributes.
   */
  static gfxMatrix GetUserToCanvasTM(nsIFrame* aFrame, uint32_t aFor);

  /**
   * Notify the descendants of aFrame of a change to one of their ancestors
   * that might affect them.
   */
  static void
  NotifyChildrenOfSVGChange(nsIFrame *aFrame, uint32_t aFlags);

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
    eBBoxIncludeFill           = 1 << 0,
    eBBoxIncludeFillGeometry   = 1 << 1,
    eBBoxIncludeStroke         = 1 << 2,
    eBBoxIncludeStrokeGeometry = 1 << 3,
    eBBoxIncludeMarkers        = 1 << 4
  };
  /**
   * Get the SVG bbox (the SVG spec's simplified idea of bounds) of aFrame in
   * aFrame's userspace.
   */
  static gfxRect GetBBox(nsIFrame *aFrame,
                         uint32_t aFlags = eBBoxIncludeFillGeometry);

  /**
   * Convert a userSpaceOnUse/objectBoundingBoxUnits rectangle that's specified
   * using four nsSVGLength2 values into a user unit rectangle in user space.
   *
   * @param aXYWH pointer to 4 consecutive nsSVGLength2 objects containing
   * the x, y, width and height values in that order
   * @param aBBox the bounding box of the object the rect is relative to;
   * may be null if aUnits is not SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
   * @param aFrame the object in which to interpret user-space units;
   * may be null if aUnits is SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
   */
  static gfxRect
  GetRelativeRect(uint16_t aUnits, const nsSVGLength2 *aXYWH,
                  const gfxRect &aBBox, nsIFrame *aFrame);

  /**
   * Find the first frame, starting with aStartFrame and going up its
   * parent chain, that is not an svgAFrame.
   */
  static nsIFrame* GetFirstNonAAncestorFrame(nsIFrame* aStartFrame);

  static bool OuterSVGIsCallingReflowSVG(nsIFrame *aFrame);
  static bool AnyOuterSVGIsCallingReflowSVG(nsIFrame *aFrame);

  /*
   * Get any additional transforms that apply only to stroking
   * e.g. non-scaling-stroke
   */
  static gfxMatrix GetStrokeTransform(nsIFrame *aFrame);

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
                                               nsTextFrame* aFrame,
                                               const gfxMatrix& aMatrix);
  static gfxRect PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                                               nsSVGPathGeometryFrame* aFrame,
                                               const gfxMatrix& aMatrix);

  /**
   * Convert a floating-point value to a 32-bit integer value, clamping to
   * the range of valid integers.
   */
  static int32_t ClampToInt(double aVal)
  {
    return NS_lround(std::max(double(INT32_MIN),
                            std::min(double(INT32_MAX), aVal)));
  }

  static nscolor GetFallbackOrPaintColor(gfxContext *aContext,
                                         nsStyleContext *aStyleContext,
                                         nsStyleSVGPaint nsStyleSVG::*aFillOrStroke);

  /**
   * Set up cairo context with an object pattern
   */
  static bool SetupObjectPaint(gfxContext *aContext,
                               gfxTextObjectPaint *aObjectPaint,
                               const nsStyleSVGPaint& aPaint,
                               float aOpacity);

  /**
   * Sets the current paint on the specified gfxContent to be the SVG 'fill'
   * for the given frame.
   */
  static bool SetupCairoFillPaint(nsIFrame* aFrame, gfxContext* aContext,
                                  gfxTextObjectPaint *aObjectPaint = nullptr);

  /**
   * Sets the current paint on the specified gfxContent to be the SVG 'stroke'
   * for the given frame.
   */
  static bool SetupCairoStrokePaint(nsIFrame* aFrame, gfxContext* aContext,
                                    gfxTextObjectPaint *aObjectPaint = nullptr);

  static float GetOpacity(nsStyleSVGOpacitySource aOpacityType,
                          const float& aOpacity,
                          gfxTextObjectPaint *aOuterObjectPaint);

  /*
   * @return false if there is no stroke
   */
  static bool HasStroke(nsIFrame* aFrame,
                        gfxTextObjectPaint *aObjectPaint = nullptr);

  static float GetStrokeWidth(nsIFrame* aFrame,
                              gfxTextObjectPaint *aObjectPaint = nullptr);

  /*
   * Set up a cairo context for measuring the bounding box of a stroked path.
   */
  static void SetupCairoStrokeBBoxGeometry(nsIFrame* aFrame,
                                           gfxContext *aContext,
                                           gfxTextObjectPaint *aObjectPaint = nullptr);

  /*
   * Set up a cairo context for a stroked path (including any dashing that
   * applies).
   */
  static void SetupCairoStrokeGeometry(nsIFrame* aFrame, gfxContext *aContext,
                                       gfxTextObjectPaint *aObjectPaint = nullptr);

  /*
   * Set up a cairo context for stroking, including setting up any stroke-related
   * properties such as dashing and setting the current paint on the gfxContext.
   */
  static bool SetupCairoStroke(nsIFrame* aFrame, gfxContext *aContext,
                               gfxTextObjectPaint *aObjectPaint = nullptr);

  /**
   * This function returns a set of bit flags indicating which parts of the
   * element (fill, stroke, bounds) should intercept pointer events. It takes
   * into account the type of element and the value of the 'pointer-events'
   * property on the element.
   */
  static uint16_t GetGeometryHitTestFlags(nsIFrame* aFrame);

  /**
   * Render a SVG glyph.
   * @param aElement the SVG glyph element to render
   * @param aContext the thebes aContext to draw to
   * @param aDrawMode fill or stroke or both (see gfxFont::DrawMode)
   * @return true if rendering succeeded
   */
  static bool PaintSVGGlyph(Element* aElement, gfxContext* aContext,
                            gfxFont::DrawMode aDrawMode,
                            gfxTextObjectPaint* aObjectPaint);
  /**
   * Get the extents of a SVG glyph.
   * @param aElement the SVG glyph element
   * @param aSVGToAppSpace the matrix mapping the SVG glyph space to the
   *   target context space
   * @param aResult the result (valid when true is returned)
   * @return true if calculating the extents succeeded
   */
  static bool GetSVGGlyphExtents(Element* aElement,
                                 const gfxMatrix& aSVGToAppSpace,
                                 gfxRect* aResult);

  /**
   * Returns the app unit canvas bounds of a userspace rect.
   *
   * @param aToCanvas Transform from userspace to canvas device space.
   */
  static nsRect
  ToCanvasBounds(const gfxRect &aUserspaceRect,
                 const gfxMatrix &aToCanvas,
                 const nsPresContext *presContext);
};

#endif
