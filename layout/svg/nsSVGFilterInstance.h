/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERINSTANCE_H__
#define __NS_SVGFILTERINSTANCE_H__

#include "gfxMatrix.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"
#include "nsTArray.h"
#include "nsIFrame.h"
#include "mozilla/gfx/2D.h"

class gfxASurface;
class gfxImageSurface;
class nsIFrame;
class nsSVGFilterFrame;
class nsSVGFilterPaintCallback;

namespace mozilla {
namespace dom {
class SVGFilterElement;
}
}

/**
 * This class performs all filter processing.
 * 
 * We build a graph of the filter image data flow, essentially
 * converting the filter graph to SSA. This lets us easily propagate
 * analysis data (such as bounding-boxes) over the filter primitive graph.
 *
 * Definition of "filter space": filter space is a coordinate system that is
 * aligned with the user space of the filtered element, with its origin located
 * at the top left of the filter region (as specified by our ctor's
 * aFilterRegion, and returned by our GetFilterRegion, specifically), and with
 * one unit equal in size to one pixel of the offscreen surface into which the
 * filter output would/will be painted.
 *
 * The definition of "filter region" can be found here:
 * http://www.w3.org/TR/SVG11/filters.html#FilterEffectsRegion
 */
class nsSVGFilterInstance
{
  typedef mozilla::gfx::Point3D Point3D;
  typedef mozilla::gfx::IntRect IntRect;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::FilterPrimitiveDescription FilterPrimitiveDescription;

public:
  /**
   * Paint the given filtered frame.
   * @param aDirtyArea The area than needs to be painted, in aFilteredFrame's
   *   frame space (i.e. relative to its origin, the top-left corner of its
   *   border box).
   */
  static nsresult PaintFilteredFrame(nsSVGFilterFrame* aFilterFrame,
                                     nsRenderingContext *aContext,
                                     nsIFrame *aFilteredFrame,
                                     nsSVGFilterPaintCallback *aPaintCallback,
                                     const nsRect* aDirtyArea,
                                     nsIFrame* aTransformRoot = nullptr);

  /**
   * Returns the post-filter area that could be dirtied when the given
   * pre-filter area of aFilteredFrame changes.
   * @param aPreFilterDirtyRect The pre-filter area of aFilteredFrame that has
   *   changed, relative to aFilteredFrame, in app units.
   */
  static nsRect GetPostFilterDirtyArea(nsSVGFilterFrame* aFilterFrame,
                                       nsIFrame *aFilteredFrame,
                                       const nsRect& aPreFilterDirtyRect);

  /**
   * Returns the pre-filter area that is needed from aFilteredFrame when the
   * given post-filter area needs to be repainted.
   * @param aPostFilterDirtyRect The post-filter area that is dirty, relative
   *   to aFilteredFrame, in app units.
   */
  static nsRect GetPreFilterNeededArea(nsSVGFilterFrame* aFilterFrame,
                                       nsIFrame *aFilteredFrame,
                                       const nsRect& aPostFilterDirtyRect);

  /**
   * Returns the post-filter visual overflow rect (paint bounds) of
   * aFilteredFrame.
   * @param aOverrideBBox A user space rect, in user units, that should be used
   *   as aFilteredFrame's bbox ('bbox' is a specific SVG term), if non-null.
   * @param aPreFilterBounds The pre-filter visual overflow rect of
   *   aFilteredFrame, if non-null.
   */
  static nsRect GetPostFilterBounds(nsSVGFilterFrame* aFilterFrame,
                                    nsIFrame *aFilteredFrame,
                                    const gfxRect *aOverrideBBox = nullptr,
                                    const nsRect *aPreFilterBounds = nullptr);

  /**
   * @param aTargetFrame The frame of the filtered element under consideration.
   * @param aFilterFrame The frame of the SVG filter element.
   * @param aPaintCallback [optional] The callback that Render() should use to
   *   paint. Only required if you will call Render().
   * @param aPostFilterDirtyRect [optional] The bounds of the post-filter area
   *   that has to be repainted, in filter space. Only required if you will
   *   call ComputeSourceNeededRect() or Render().
   * @param aPreFilterDirtyRect [optional] The bounds of the pre-filter area of
   *   the filtered element that changed, in filter space. Only required if you
   *   will call ComputePostFilterDirtyRect().
   * @param aOverridePreFilterVisualOverflowRect [optional] Use a different
   *   visual overflow rect for the target element.
   * @param aOverrideBBox [optional] Use a different SVG bbox for the target
   *   element.
   * @param aTransformRoot [optional] The transform root frame for painting.
   */
  nsSVGFilterInstance(nsIFrame *aTargetFrame,
                      nsSVGFilterFrame *aFilterFrame,
                      nsSVGFilterPaintCallback *aPaintCallback,
                      const nsRect *aPostFilterDirtyRect = nullptr,
                      const nsRect *aPreFilterDirtyRect = nullptr,
                      const nsRect *aOverridePreFilterVisualOverflowRect = nullptr,
                      const gfxRect *aOverrideBBox = nullptr,
                      nsIFrame* aTransformRoot = nullptr);

  /**
   * Returns true if the filter instance was created successfully.
   */
  bool IsInitialized() const { return mInitialized; }

  /**
   * Returns the user specified "filter region", in the filtered element's user
   * space, after it has been adjusted out (if necessary) so that its edges
   * coincide with pixel boundaries of the offscreen surface into which the
   * filtered output would/will be painted.
   */
  gfxRect GetFilterRegion() const { return mFilterRegion; }

  /**
   * Returns the size of the user specified "filter region", in filter space.
   * The size will be {filterRes.x by filterRes.y}, whether the user specified
   * the filter's filterRes attribute explicitly, or the implementation chose
   * the filterRes values. (The top-left of the filter region is the origin of
   * filter space, which is why this method returns an nsIntSize and not an
   * nsIntRect.)
   */
  uint32_t GetFilterResX() const { return mFilterSpaceBounds.width; }
  uint32_t GetFilterResY() const { return mFilterSpaceBounds.height; }

  /**
   * Draws the filter output into aContext. The area that
   * needs to be painted must have been specified before calling this method
   * by passing it as the aPostFilterDirtyRect argument to the
   * nsSVGFilterInstance constructor.
   */
  nsresult Render(gfxContext* aContext);

  /**
   * Sets the aPostFilterDirtyRect outparam to the post-filter bounds in frame
   * space of the area that would be dirtied by mTargetFrame when a given
   * pre-filter area of mTargetFrame is dirtied. The pre-filter area must have
   * been specified before calling this method by passing it as the
   * aPreFilterDirtyRect argument to the nsSVGFilterInstance constructor.
   */
  nsresult ComputePostFilterDirtyRect(nsRect* aPostFilterDirtyRect);

  /**
   * Sets the aPostFilterExtents outparam to the post-filter bounds in frame
   * space for the whole filter output. This is not necessarily equivalent to
   * the area that would be dirtied in the result when the entire pre-filter
   * area is dirtied, because some filter primitives can generate output
   * without any input.
   */
  nsresult ComputePostFilterExtents(nsRect* aPostFilterExtents);

  /**
   * Sets the aDirty outparam to the pre-filter bounds in frame space of the
   * area of mTargetFrame that is needed in order to paint the filtered output
   * for a given post-filter dirtied area. The post-filter area must have been
   * specified before calling this method by passing it as the aPostFilterDirtyRect
   * argument to the nsSVGFilterInstance constructor.
   */
  nsresult ComputeSourceNeededRect(nsRect* aDirty);

  float GetPrimitiveNumber(uint8_t aCtxType, const nsSVGNumber2 *aNumber) const
  {
    return GetPrimitiveNumber(aCtxType, aNumber->GetAnimValue());
  }
  float GetPrimitiveNumber(uint8_t aCtxType, const nsSVGNumberPair *aNumberPair,
                           nsSVGNumberPair::PairIndex aIndex) const
  {
    return GetPrimitiveNumber(aCtxType, aNumberPair->GetAnimValue(aIndex));
  }

  /**
   * Converts a userSpaceOnUse/objectBoundingBoxUnits unitless point
   * into filter space, depending on the value of mPrimitiveUnits. (For
   * objectBoundingBoxUnits, the bounding box offset is applied to the point.)
   */
  Point3D ConvertLocation(const Point3D& aPoint) const;

  /**
   * Returns the transform from the filtered element's user space to filter
   * space. This will be a simple translation and/or scale.
   */
  gfxMatrix GetUserSpaceToFilterSpaceTransform() const;

  /**
   * Returns the transform from filter space to outer-<svg> device space.
   */
  gfxMatrix GetFilterSpaceToDeviceSpaceTransform() const {
    return mFilterSpaceToDeviceSpaceTransform;
  }

  gfxPoint FilterSpaceToUserSpace(const gfxPoint& aPt) const;

  /**
   * Returns the transform from filter space to frame space, in CSS px. This
   * transform does not transform to frame space in its normal app units, since
   * app units are ints, requiring appropriate rounding which can't be done by
   * a transform matrix. Callers have to do that themselves as appropriate for
   * their needs.
   */
  gfxMatrix GetFilterSpaceToFrameSpaceInCSSPxTransform() const {
    return mFilterSpaceToFrameSpaceInCSSPxTransform;
  }

  int32_t AppUnitsPerCSSPixel() const { return mAppUnitsPerCSSPx; }

private:
  struct SourceInfo {
    // Specifies which parts of the source need to be rendered.
    // Set by ComputeNeededBoxes().
    nsIntRect mNeededBounds;

    // The surface that contains the input rendering.
    // Set by BuildSourceImage / BuildSourcePaint.
    mozilla::RefPtr<SourceSurface> mSourceSurface;

    // The position and size of mSourceSurface in filter space.
    // Set by BuildSourceImage / BuildSourcePaint.
    IntRect mSurfaceRect;
  };

  /**
   * Creates a SourceSurface for either the FillPaint or StrokePaint graph
   * nodes
   */
  nsresult BuildSourcePaint(SourceInfo *aPrimitive,
                            gfxASurface* aTargetSurface,
                            DrawTarget* aTargetDT);

  /**
   * Creates a SourceSurface for either the FillPaint and StrokePaint graph
   * nodes, fills its contents and assigns it to mFillPaint.mSourceSurface and
   * mStrokePaint.mSourceSurface respectively.
   */
  nsresult BuildSourcePaints(gfxASurface* aTargetSurface,
                             DrawTarget* aTargetDT);

  /**
   * Creates the SourceSurface for the SourceGraphic graph node, paints its
   * contents, and assigns it to mSourceGraphic.mSourceSurface.
   */
  nsresult BuildSourceImage(gfxASurface* aTargetSurface,
                            DrawTarget* aTargetDT);

  /**
   * Build the list of FilterPrimitiveDescriptions that describes the filter's
   * filter primitives and their connections. This populates
   * mPrimitiveDescriptions and mInputImages.
   */
  nsresult BuildPrimitives();

  /**
   * Computes the filter space bounds of the areas that we actually *need* from
   * the filter sources, based on the value of mPostFilterDirtyRect.
   * This sets mNeededBounds on the corresponding SourceInfo structs.
   */
   void ComputeNeededBoxes();

  /**
   * Computes the filter primitive subregion for the given primitive.
   */
  IntRect ComputeFilterPrimitiveSubregion(nsSVGFE* aFilterElement,
                                          const nsTArray<int32_t>& aInputIndices);

  /**
   * Takes the input indices of a filter primitive and returns for each input
   * whether the input's output is tainted.
   */
  void GetInputsAreTainted(const nsTArray<int32_t>& aInputIndices,
                           nsTArray<bool>& aOutInputsAreTainted);

  /**
   * Scales a numeric filter primitive length in the X, Y or "XY" directions
   * into a length in filter space (no offset is applied).
   */
  float GetPrimitiveNumber(uint8_t aCtxType, float aValue) const;

  gfxRect UserSpaceToFilterSpace(const gfxRect& aUserSpace) const;

  /**
   * Converts an nsRect that is relative to a filtered frame's origin (i.e. the
   * top-left corner of its border box) into filter space.
   * Returns the entire filter region if aRect is null, or if the result is too
   * large to be stored in an nsIntRect.
   */
  nsIntRect FrameSpaceToFilterSpace(const nsRect* aRect) const;
  nsRect FilterSpaceToFrameSpace(const nsIntRect& aRect) const;

  /**
   * Returns the transform from frame space to the coordinate space that
   * GetCanvasTM transforms to. "Frame space" is the origin of a frame, aka the
   * top-left corner of its border box, aka the top left corner of its mRect.
   */
  gfxMatrix GetUserSpaceToFrameSpaceInCSSPxTransform() const;

  /**
   * The frame for the element that is currently being filtered.
   */
  nsIFrame*               mTargetFrame;

  nsSVGFilterPaintCallback* mPaintCallback;

  /**
   * The filter element referenced by mTargetFrame's element.
   */
  const mozilla::dom::SVGFilterElement* mFilterElement;

  /**
   * The SVG bbox of the element that is being filtered, in user space.
   */
  gfxRect                 mTargetBBox;

  /**
   * The transform from filter space to outer-<svg> device space.
   */
  gfxMatrix               mFilterSpaceToDeviceSpaceTransform;

  /**
   * Transform rects between filter space and frame space in CSS pixels.
   */
  gfxMatrix               mFilterSpaceToFrameSpaceInCSSPxTransform;
  gfxMatrix               mFrameSpaceInCSSPxToFilterSpaceTransform;

  /**
   * The "filter region", in the filtered element's user space.
   */
  gfxRect                 mFilterRegion;
  nsIntRect               mFilterSpaceBounds;

  /**
   * Pre-filter paint bounds of the element that is being filtered, in filter
   * space.
   */
  nsIntRect               mTargetBounds;

  /**
   * If set, this is the filter space bounds of the outer-<svg> device space
   * bounds of the dirty area that needs to be repainted. (As bounds-of-bounds,
   * this may be a fair bit bigger than we actually need, unfortunately.)
   */
  nsIntRect               mPostFilterDirtyRect;

  /**
   * If set, this is the filter space bounds of the outer-<svg> device bounds
   * of the pre-filter area of the filtered element that changed. (As
   * bounds-of-bounds, this may be a fair bit bigger than we actually need,
   * unfortunately.)
   */
  nsIntRect               mPreFilterDirtyRect;

  /**
   * The 'primitiveUnits' attribute value (objectBoundingBox or userSpaceOnUse).
   */
  uint16_t                mPrimitiveUnits;

  SourceInfo              mSourceGraphic;
  SourceInfo              mFillPaint;
  SourceInfo              mStrokePaint;
  nsIFrame*               mTransformRoot;
  nsTArray<mozilla::RefPtr<SourceSurface>> mInputImages;
  nsTArray<FilterPrimitiveDescription> mPrimitiveDescriptions;
  int32_t                 mAppUnitsPerCSSPx;
  bool                    mInitialized;
};

#endif
