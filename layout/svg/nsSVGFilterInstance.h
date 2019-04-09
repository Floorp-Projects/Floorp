/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERINSTANCE_H__
#define __NS_SVGFILTERINSTANCE_H__

#include "gfxMatrix.h"
#include "gfxRect.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberPair.h"
#include "SVGFilters.h"
#include "nsTArray.h"

class nsSVGFilterFrame;
struct nsStyleFilter;

namespace mozilla {
namespace dom {
class SVGFilterElement;
}  // namespace dom
}  // namespace mozilla

/**
 * This class helps nsFilterInstance build its filter graph by processing a
 * single SVG reference filter.
 *
 * In BuildPrimitives, this class iterates through the referenced <filter>
 * element's primitive elements, creating a FilterPrimitiveDescription for
 * each one.
 *
 * This class uses several different coordinate spaces, defined as follows:
 *
 * "user space"
 *   The filtered SVG element's user space or the filtered HTML element's
 *   CSS pixel space. The origin for an HTML element is the top left corner of
 *   its border box.
 *
 * "filter space"
 *   User space scaled to device pixels. Shares the same origin as user space.
 *   This space is the same across chained SVG and CSS filters. To compute the
 *   overall filter space for a chain, we first need to build each filter's
 *   FilterPrimitiveDescriptions in some common space. That space is
 *   filter space.
 *
 * To understand the spaces better, let's take an example filter:
 *   <filter id="f">...</filter>
 *
 * And apply the filter to a div element:
 *   <div style="filter: url(#f); ...">...</div>
 *
 * And let's say there are 2 device pixels for every 1 CSS pixel.
 *
 * Finally, let's define an arbitrary point in user space:
 *   "user space point" = (10, 10)
 *
 * The point will be inset 10 CSS pixels from both the top and left edges of the
 * div element's border box.
 *
 * Now, let's transform the point from user space to filter space:
 *   "filter space point" = "user space point" * "device pixels per CSS pixel"
 *   "filter space point" = (10, 10) * 2
 *   "filter space point" = (20, 20)
 */
class nsSVGFilterInstance {
  typedef mozilla::SVGAnimatedNumber SVGAnimatedNumber;
  typedef mozilla::SVGAnimatedNumberPair SVGAnimatedNumberPair;
  typedef mozilla::gfx::Point3D Point3D;
  typedef mozilla::gfx::IntRect IntRect;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::FilterPrimitiveDescription FilterPrimitiveDescription;
  typedef mozilla::dom::SVGFE SVGFE;
  typedef mozilla::dom::UserSpaceMetrics UserSpaceMetrics;

 public:
  /**
   * @param aFilter The SVG filter reference from the style system. This class
   *   stores aFilter by reference, so callers should avoid modifying or
   *   deleting aFilter during the lifetime of nsSVGFilterInstance.
   * @param aTargetContent The filtered element.
   * @param aTargetBBox The SVG bbox to use for the target frame, computed by
   *   the caller. The caller may decide to override the actual SVG bbox.
   */
  nsSVGFilterInstance(const nsStyleFilter& aFilter, nsIFrame* aTargetFrame,
                      nsIContent* aTargetContent,
                      const UserSpaceMetrics& aMetrics,
                      const gfxRect& aTargetBBox,
                      const gfxSize& aUserSpaceToFilterSpaceScale);

  /**
   * Returns true if the filter instance was created successfully.
   */
  bool IsInitialized() const { return mInitialized; }

  /**
   * Iterates through the <filter> element's primitive elements, creating a
   * FilterPrimitiveDescription for each one. Appends the new
   * FilterPrimitiveDescription(s) to the aPrimitiveDescrs list. Also, appends
   * new images from feImage filter primitive elements to the aInputImages list.
   * aInputIsTainted describes whether the input to this filter is tainted, i.e.
   * whether it contains security-sensitive content. This is needed to propagate
   * taintedness to the FilterPrimitive that take tainted inputs. Something
   * being tainted means that it contains security sensitive content. The input
   * to this filter is the previous filter's output, i.e. the last element in
   * aPrimitiveDescrs, or the SourceGraphic input if this is the first filter in
   * the filter chain.
   */
  nsresult BuildPrimitives(
      nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
      nsTArray<RefPtr<SourceSurface>>& aInputImages, bool aInputIsTainted);

  float GetPrimitiveNumber(uint8_t aCtxType,
                           const SVGAnimatedNumber* aNumber) const {
    return GetPrimitiveNumber(aCtxType, aNumber->GetAnimValue());
  }
  float GetPrimitiveNumber(uint8_t aCtxType,
                           const SVGAnimatedNumberPair* aNumberPair,
                           SVGAnimatedNumberPair::PairIndex aIndex) const {
    return GetPrimitiveNumber(aCtxType, aNumberPair->GetAnimValue(aIndex));
  }

  /**
   * Converts a userSpaceOnUse/objectBoundingBoxUnits unitless point
   * into filter space, depending on the value of mPrimitiveUnits. (For
   * objectBoundingBoxUnits, the bounding box offset is applied to the point.)
   */
  Point3D ConvertLocation(const Point3D& aPoint) const;

  /**
   * Transform a rect between user space and filter space.
   */
  gfxRect UserSpaceToFilterSpace(const gfxRect& aUserSpaceRect) const;

 private:
  /**
   * Finds the filter frame associated with this SVG filter.
   */
  nsSVGFilterFrame* GetFilterFrame(nsIFrame* aTargetFrame);

  /**
   * Computes the filter primitive subregion for the given primitive.
   */
  IntRect ComputeFilterPrimitiveSubregion(
      SVGFE* aFilterElement,
      const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
      const nsTArray<int32_t>& aInputIndices);

  /**
   * Takes the input indices of a filter primitive and returns for each input
   * whether the input's output is tainted.
   */
  void GetInputsAreTainted(
      const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
      const nsTArray<int32_t>& aInputIndices, bool aFilterInputIsTainted,
      nsTArray<bool>& aOutInputsAreTainted);

  /**
   * Scales a numeric filter primitive length in the X, Y or "XY" directions
   * into a length in filter space (no offset is applied).
   */
  float GetPrimitiveNumber(uint8_t aCtxType, float aValue) const;

  /**
   * Returns the transform from frame space to the coordinate space that
   * GetCanvasTM transforms to. "Frame space" is the origin of a frame, aka the
   * top-left corner of its border box, aka the top left corner of its mRect.
   */
  gfxMatrix GetUserSpaceToFrameSpaceInCSSPxTransform() const;

  /**
   * Appends a new FilterPrimitiveDescription to aPrimitiveDescrs that
   * converts the FilterPrimitiveDescription at mSourceGraphicIndex into
   * a SourceAlpha input for the next FilterPrimitiveDescription.
   *
   * The new FilterPrimitiveDescription zeros out the SourceGraphic's RGB
   * channels and keeps the alpha channel intact.
   */
  int32_t GetOrCreateSourceAlphaIndex(
      nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs);

  /**
   * Finds the index in aPrimitiveDescrs of each input to aPrimitiveElement.
   * For example, if aPrimitiveElement is:
   *   <feGaussianBlur in="another-primitive" .../>
   * Then, the resulting aSourceIndices will contain the index of the
   * FilterPrimitiveDescription representing "another-primitive".
   */
  nsresult GetSourceIndices(
      SVGFE* aPrimitiveElement,
      nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
      const nsDataHashtable<nsStringHashKey, int32_t>& aImageTable,
      nsTArray<int32_t>& aSourceIndices);

  /**
   * Compute the filter region in user space, filter space, and filter
   * space.
   */
  bool ComputeBounds();

  /**
   * The SVG reference filter originally from the style system.
   */
  const nsStyleFilter& mFilter;

  /**
   * The filtered element.
   */
  nsIContent* mTargetContent;

  /**
   * The SVG user space metrics that SVG lengths are resolved against.
   */
  const UserSpaceMetrics& mMetrics;

  /**
   * The filter element referenced by mTargetFrame's element.
   */
  const mozilla::dom::SVGFilterElement* mFilterElement;

  /**
   * The frame for the SVG filter element.
   */
  nsSVGFilterFrame* mFilterFrame;

  /**
   * The SVG bbox of the element that is being filtered, in user space.
   */
  gfxRect mTargetBBox;

  /**
   * The "filter region" in various spaces.
   */
  nsIntRect mFilterSpaceBounds;

  /**
   * The scale factors between user space and filter space.
   */
  gfxSize mUserSpaceToFilterSpaceScale;

  /**
   * The 'primitiveUnits' attribute value (objectBoundingBox or userSpaceOnUse).
   */
  uint16_t mPrimitiveUnits;

  /**
   * The index of the FilterPrimitiveDescription that this SVG filter should use
   * as its SourceGraphic, or the SourceGraphic keyword index if this is the
   * first filter in a chain. Initialized in BuildPrimitives
   */
  MOZ_INIT_OUTSIDE_CTOR int32_t mSourceGraphicIndex;

  /**
   * The index of the FilterPrimitiveDescription that this SVG filter should use
   * as its SourceAlpha, or the SourceAlpha keyword index if this is the first
   * filter in a chain. Initialized in BuildPrimitives
   */
  MOZ_INIT_OUTSIDE_CTOR int32_t mSourceAlphaIndex;

  /**
   * SourceAlpha is available if GetOrCreateSourceAlphaIndex has been called.
   */
  int32_t mSourceAlphaAvailable;

  bool mInitialized;
};

#endif
