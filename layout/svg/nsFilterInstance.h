/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_FILTERINSTANCE_H__
#define __NS_FILTERINSTANCE_H__

#include "gfxMatrix.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "nsCOMPtr.h"
#include "FilterSupport.h"
#include "nsHashKeys.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsTArray.h"
#include "nsIFrame.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/webrender/WebRenderTypes.h"

class gfxContext;
class nsIFrame;
class nsSVGFilterPaintCallback;

namespace mozilla {
namespace dom {
class UserSpaceMetrics;
}  // namespace dom
}  // namespace mozilla

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
class nsFilterInstance {
  template <typename T>
  using Span = mozilla::Span<T>;
  using StyleFilter = mozilla::StyleFilter;

  typedef mozilla::gfx::IntRect IntRect;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::FilterPrimitiveDescription FilterPrimitiveDescription;
  typedef mozilla::gfx::FilterDescription FilterDescription;
  typedef mozilla::dom::UserSpaceMetrics UserSpaceMetrics;
  typedef mozilla::image::imgDrawingParams imgDrawingParams;

 public:
  /**
   * Create a FilterDescription for the supplied filter. All coordinates in
   * the description are in filter space.
   * @param aFilterInputIsTainted Describes whether the SourceImage /
   *   SourceAlpha input is tainted. This affects whether feDisplacementMap
   *   will respect the filter input as its map input, and it affects the
   *   IsTainted() state on the filter primitives in the FilterDescription.
   *   "Tainted" is a term from the filters spec and means security-sensitive
   *   content, i.e. pixels that JS should not be able to read in any way.
   * @param aOutAdditionalImages Will contain additional images needed to
   *   render the filter (from feImage primitives).
   * @return A FilterDescription describing the filter.
   */
  static FilterDescription GetFilterDescription(
      nsIContent* aFilteredElement, Span<const StyleFilter> aFilterChain,
      bool aFilterInputIsTainted, const UserSpaceMetrics& aMetrics,
      const gfxRect& aBBox,
      nsTArray<RefPtr<SourceSurface>>& aOutAdditionalImages);

  /**
   * Paint the given filtered frame.
   * @param aDirtyArea The area than needs to be painted, in aFilteredFrame's
   *   frame space (i.e. relative to its origin, the top-left corner of its
   *   border box).
   */
  static void PaintFilteredFrame(nsIFrame* aFilteredFrame, gfxContext* aCtx,
                                 nsSVGFilterPaintCallback* aPaintCallback,
                                 const nsRegion* aDirtyArea,
                                 imgDrawingParams& aImgParams,
                                 float aOpacity = 1.0f);

  /**
   * Returns the post-filter area that could be dirtied when the given
   * pre-filter area of aFilteredFrame changes.
   * @param aPreFilterDirtyRegion The pre-filter area of aFilteredFrame that
   *   has changed, relative to aFilteredFrame, in app units.
   */
  static nsRegion GetPostFilterDirtyArea(nsIFrame* aFilteredFrame,
                                         const nsRegion& aPreFilterDirtyRegion);

  /**
   * Returns the pre-filter area that is needed from aFilteredFrame when the
   * given post-filter area needs to be repainted.
   * @param aPostFilterDirtyRegion The post-filter area that is dirty, relative
   *   to aFilteredFrame, in app units.
   */
  static nsRegion GetPreFilterNeededArea(
      nsIFrame* aFilteredFrame, const nsRegion& aPostFilterDirtyRegion);

  /**
   * Returns the post-filter visual overflow rect (paint bounds) of
   * aFilteredFrame.
   * @param aOverrideBBox A user space rect, in user units, that should be used
   *   as aFilteredFrame's bbox ('bbox' is a specific SVG term), if non-null.
   * @param aPreFilterBounds The pre-filter visual overflow rect of
   *   aFilteredFrame, if non-null.
   */
  static nsRect GetPostFilterBounds(nsIFrame* aFilteredFrame,
                                    const gfxRect* aOverrideBBox = nullptr,
                                    const nsRect* aPreFilterBounds = nullptr);

  /**
   * Try to build WebRender filters for a frame if the filters applied to it are
   * supported.
   */
  static bool BuildWebRenderFilters(
      nsIFrame* aFilteredFrame,
      mozilla::Span<const mozilla::StyleFilter> aFilters,
      WrFiltersHolder& aWrFilters, mozilla::Maybe<nsRect>& aPostFilterClip);

 private:
  /**
   * @param aTargetFrame The frame of the filtered element under consideration,
   *   may be null.
   * @param aTargetContent The filtered element itself.
   * @param aMetrics The metrics to resolve SVG lengths against.
   * @param aFilterChain The list of filters to apply.
   * @param aFilterInputIsTainted Describes whether the SourceImage /
   *   SourceAlpha input is tainted. This affects whether feDisplacementMap
   *   will respect the filter input as its map input.
   * @param aPaintCallback [optional] The callback that Render() should use to
   *   paint. Only required if you will call Render().
   * @param aPaintTransform The transform to apply to convert to
   *   aTargetFrame's SVG user space. Only used when painting.
   * @param aPostFilterDirtyRegion [optional] The post-filter area
   *   that has to be repainted, in app units. Only required if you will
   *   call ComputeSourceNeededRect() or Render().
   * @param aPreFilterDirtyRegion [optional] The pre-filter area of
   *   the filtered element that changed, in app units. Only required if you
   *   will call ComputePostFilterDirtyRegion().
   * @param aPreFilterVisualOverflowRectOverride [optional] Use a different
   *   visual overflow rect for the target element.
   * @param aOverrideBBox [optional] Use a different SVG bbox for the target
   *   element. Must be non-null if aTargetFrame is null.
   */
  nsFilterInstance(nsIFrame* aTargetFrame, nsIContent* aTargetContent,
                   const UserSpaceMetrics& aMetrics,
                   Span<const StyleFilter> aFilterChain,
                   bool aFilterInputIsTainted,
                   nsSVGFilterPaintCallback* aPaintCallback,
                   const gfxMatrix& aPaintTransform,
                   const nsRegion* aPostFilterDirtyRegion = nullptr,
                   const nsRegion* aPreFilterDirtyRegion = nullptr,
                   const nsRect* aPreFilterVisualOverflowRectOverride = nullptr,
                   const gfxRect* aOverrideBBox = nullptr);

  /**
   * Returns true if the filter instance was created successfully.
   */
  bool IsInitialized() const { return mInitialized; }

  /**
   * Draws the filter output into aDrawTarget. The area that
   * needs to be painted must have been specified before calling this method
   * by passing it as the aPostFilterDirtyRegion argument to the
   * nsFilterInstance constructor.
   */
  void Render(gfxContext* aCtx, imgDrawingParams& aImgParams,
              float aOpacity = 1.0f);

  const FilterDescription& ExtractDescriptionAndAdditionalImages(
      nsTArray<RefPtr<SourceSurface>>& aOutAdditionalImages) {
    mInputImages.SwapElements(aOutAdditionalImages);
    return mFilterDescription;
  }

  /**
   * Sets the aPostFilterDirtyRegion outparam to the post-filter area in frame
   * space that would be dirtied by mTargetFrame when a given
   * pre-filter area of mTargetFrame is dirtied. The pre-filter area must have
   * been specified before calling this method by passing it as the
   * aPreFilterDirtyRegion argument to the nsFilterInstance constructor.
   */
  nsRegion ComputePostFilterDirtyRegion();

  /**
   * Sets the aPostFilterExtents outparam to the post-filter bounds in frame
   * space for the whole filter output. This is not necessarily equivalent to
   * the area that would be dirtied in the result when the entire pre-filter
   * area is dirtied, because some filter primitives can generate output
   * without any input.
   */
  nsRect ComputePostFilterExtents();

  /**
   * Sets the aDirty outparam to the pre-filter bounds in frame space of the
   * area of mTargetFrame that is needed in order to paint the filtered output
   * for a given post-filter dirtied area. The post-filter area must have been
   * specified before calling this method by passing it as the
   * aPostFilterDirtyRegion argument to the nsFilterInstance constructor.
   */
  nsRect ComputeSourceNeededRect();

  struct SourceInfo {
    // Specifies which parts of the source need to be rendered.
    // Set by ComputeNeededBoxes().
    nsIntRect mNeededBounds;

    // The surface that contains the input rendering.
    // Set by BuildSourceImage / BuildSourcePaint.
    RefPtr<SourceSurface> mSourceSurface;

    // The position and size of mSourceSurface in filter space.
    // Set by BuildSourceImage / BuildSourcePaint.
    IntRect mSurfaceRect;
  };

  /**
   * Creates a SourceSurface for either the FillPaint or StrokePaint graph
   * nodes
   */
  void BuildSourcePaint(SourceInfo* aSource, imgDrawingParams& aImgParams);

  /**
   * Creates a SourceSurface for either the FillPaint and StrokePaint graph
   * nodes, fills its contents and assigns it to mFillPaint.mSourceSurface and
   * mStrokePaint.mSourceSurface respectively.
   */
  void BuildSourcePaints(imgDrawingParams& aImgParams);

  /**
   * Creates the SourceSurface for the SourceGraphic graph node, paints its
   * contents, and assigns it to mSourceGraphic.mSourceSurface.
   */
  void BuildSourceImage(DrawTarget* aDest, imgDrawingParams& aImgParams,
                        mozilla::gfx::FilterNode* aFilter,
                        mozilla::gfx::FilterNode* aSource,
                        const mozilla::gfx::Rect& aSourceRect);

  /**
   * Build the list of FilterPrimitiveDescriptions that describes the filter's
   * filter primitives and their connections. This populates
   * mPrimitiveDescriptions and mInputImages. aFilterInputIsTainted describes
   * whether the SourceGraphic is tainted.
   */
  nsresult BuildPrimitives(Span<const StyleFilter> aFilterChain,
                           nsIFrame* aTargetFrame, bool aFilterInputIsTainted);

  /**
   * Add to the list of FilterPrimitiveDescriptions for a particular SVG
   * reference filter or CSS filter. This populates mPrimitiveDescriptions and
   * mInputImages. aInputIsTainted describes whether the input to aFilter is
   * tainted.
   */
  nsresult BuildPrimitivesForFilter(
      const StyleFilter& aFilter, nsIFrame* aTargetFrame, bool aInputIsTainted,
      nsTArray<FilterPrimitiveDescription>& aPrimitiveDescriptions);

  /**
   * Computes the filter space bounds of the areas that we actually *need* from
   * the filter sources, based on the value of mPostFilterDirtyRegion.
   * This sets mNeededBounds on the corresponding SourceInfo structs.
   */
  void ComputeNeededBoxes();

  /**
   * Returns the output bounds of the final FilterPrimitiveDescription.
   */
  nsIntRect OutputFilterSpaceBounds() const;

  /**
   * Compute the scale factors between user space and filter space.
   */
  bool ComputeUserSpaceToFilterSpaceScale();

  /**
   * Transform a rect between user space and filter space.
   */
  gfxRect UserSpaceToFilterSpace(const gfxRect& aUserSpace) const;
  gfxRect FilterSpaceToUserSpace(const gfxRect& aFilterSpaceRect) const;

  /**
   * Converts an nsRect or an nsRegion that is relative to a filtered frame's
   * origin (i.e. the top-left corner of its border box) into filter space,
   * rounding out.
   * Returns the entire filter region if aRect / aRegion is null, or if the
   * result is too large to be stored in an nsIntRect.
   */
  nsIntRect FrameSpaceToFilterSpace(const nsRect* aRect) const;
  nsIntRegion FrameSpaceToFilterSpace(const nsRegion* aRegion) const;

  /**
   * Converts an nsIntRect or an nsIntRegion from filter space into the space
   * that is relative to a filtered frame's origin (i.e. the top-left corner
   * of its border box) in app units, rounding out.
   */
  nsRect FilterSpaceToFrameSpace(const nsIntRect& aRect) const;
  nsRegion FilterSpaceToFrameSpace(const nsIntRegion& aRegion) const;

  /**
   * Returns the transform from frame space to the coordinate space that
   * GetCanvasTM transforms to. "Frame space" is the origin of a frame, aka the
   * top-left corner of its border box, aka the top left corner of its mRect.
   */
  gfxMatrix GetUserSpaceToFrameSpaceInCSSPxTransform() const;

  bool ComputeTargetBBoxInFilterSpace();

  /**
   * The frame for the element that is currently being filtered.
   */
  nsIFrame* mTargetFrame;

  /**
   * The filtered element.
   */
  nsIContent* mTargetContent;

  /**
   * The user space metrics of the filtered frame.
   */
  const UserSpaceMetrics& mMetrics;

  nsSVGFilterPaintCallback* mPaintCallback;

  /**
   * The SVG bbox of the element that is being filtered, in user space.
   */
  gfxRect mTargetBBox;

  /**
   * The SVG bbox of the element that is being filtered, in filter space.
   */
  nsIntRect mTargetBBoxInFilterSpace;

  /**
   * Transform rects between filter space and frame space in CSS pixels.
   */
  gfxMatrix mFilterSpaceToFrameSpaceInCSSPxTransform;
  gfxMatrix mFrameSpaceInCSSPxToFilterSpaceTransform;

  /**
   * The scale factors between user space and filter space.
   */
  gfxSize mUserSpaceToFilterSpaceScale;
  gfxSize mFilterSpaceToUserSpaceScale;

  /**
   * Pre-filter paint bounds of the element that is being filtered, in filter
   * space.
   */
  nsIntRect mTargetBounds;

  /**
   * The dirty area that needs to be repainted, in filter space.
   */
  nsIntRegion mPostFilterDirtyRegion;

  /**
   * The pre-filter area of the filtered element that changed, in filter space.
   */
  nsIntRegion mPreFilterDirtyRegion;

  SourceInfo mSourceGraphic;
  SourceInfo mFillPaint;
  SourceInfo mStrokePaint;

  /**
   * The transform to the SVG user space of mTargetFrame.
   */
  gfxMatrix mPaintTransform;

  nsTArray<RefPtr<SourceSurface>> mInputImages;
  FilterDescription mFilterDescription;
  bool mInitialized;
};

#endif
