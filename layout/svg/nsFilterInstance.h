/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_FILTERINSTANCE_H__
#define __NS_FILTERINSTANCE_H__

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
class nsIFrame;
class nsSVGFilterPaintCallback;

/**
 * This class performs all filter processing.
 * 
 * We build a graph of the filter image data flow, essentially
 * converting the filter graph to SSA. This lets us easily propagate
 * analysis data (such as bounding-boxes) over the filter primitive graph.
 *
 * Definition of "filter space": filter space is a coordinate system that is
 * aligned with the user space of the filtered element, with its origin located
 * at the top left of the filter region, and with one unit equal in size to one
 * pixel of the offscreen surface into which the filter output would/will be
 * painted.
 *
 * The definition of "filter region" can be found here:
 * http://www.w3.org/TR/SVG11/filters.html#FilterEffectsRegion
 */
class nsFilterInstance
{
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
  static nsresult PaintFilteredFrame(nsRenderingContext *aContext,
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
  static nsRect GetPostFilterDirtyArea(nsIFrame *aFilteredFrame,
                                       const nsRect& aPreFilterDirtyRect);

  /**
   * Returns the pre-filter area that is needed from aFilteredFrame when the
   * given post-filter area needs to be repainted.
   * @param aPostFilterDirtyRect The post-filter area that is dirty, relative
   *   to aFilteredFrame, in app units.
   */
  static nsRect GetPreFilterNeededArea(nsIFrame *aFilteredFrame,
                                       const nsRect& aPostFilterDirtyRect);

  /**
   * Returns the post-filter visual overflow rect (paint bounds) of
   * aFilteredFrame.
   * @param aOverrideBBox A user space rect, in user units, that should be used
   *   as aFilteredFrame's bbox ('bbox' is a specific SVG term), if non-null.
   * @param aPreFilterBounds The pre-filter visual overflow rect of
   *   aFilteredFrame, if non-null.
   */
  static nsRect GetPostFilterBounds(nsIFrame *aFilteredFrame,
                                    const gfxRect *aOverrideBBox = nullptr,
                                    const nsRect *aPreFilterBounds = nullptr);

  /**
   * @param aTargetFrame The frame of the filtered element under consideration.
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
  nsFilterInstance(nsIFrame *aTargetFrame,
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
   * Draws the filter output into aContext. The area that
   * needs to be painted must have been specified before calling this method
   * by passing it as the aPostFilterDirtyRect argument to the
   * nsFilterInstance constructor.
   */
  nsresult Render(gfxContext* aContext);

  /**
   * Sets the aPostFilterDirtyRect outparam to the post-filter bounds in frame
   * space of the area that would be dirtied by mTargetFrame when a given
   * pre-filter area of mTargetFrame is dirtied. The pre-filter area must have
   * been specified before calling this method by passing it as the
   * aPreFilterDirtyRect argument to the nsFilterInstance constructor.
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
   * argument to the nsFilterInstance constructor.
   */
  nsresult ComputeSourceNeededRect(nsRect* aDirty);


  /**
   * Returns the transform from filter space to outer-<svg> device space.
   */
  gfxMatrix GetFilterSpaceToDeviceSpaceTransform() const {
    return mFilterSpaceToDeviceSpaceTransform;
  }

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
   * Add to the list of FilterPrimitiveDescriptions for a particular SVG
   * reference filter or CSS filter. This populates mPrimitiveDescrs and
   * mInputImages.
   */
  nsresult BuildPrimitivesForFilter(const nsStyleFilter& aFilter);

  /**
   * Computes the filter space bounds of the areas that we actually *need* from
   * the filter sources, based on the value of mPostFilterDirtyRect.
   * This sets mNeededBounds on the corresponding SourceInfo structs.
   */
  void ComputeNeededBoxes();

  /**
   * Compute the scale factors between user space and filter space.
   */
  nsresult ComputeUserSpaceToFilterSpaceScale();

  /**
   * Transform a rect between user space and filter space.
   */
  gfxRect UserSpaceToFilterSpace(const gfxRect& aUserSpace) const;
  gfxRect FilterSpaceToUserSpace(const gfxRect& aFilterSpaceRect) const;

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
  gfxRect                 mUserSpaceBounds;
  nsIntRect               mFilterSpaceBounds;

  /**
   * The scale factors between user space and filter space.
   */
  gfxSize                 mUserSpaceToFilterSpaceScale;
  gfxSize                 mFilterSpaceToUserSpaceScale;

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
