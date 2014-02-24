/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERINSTANCE_H__
#define __NS_SVGFILTERINSTANCE_H__

#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"
#include "nsTArray.h"
#include "nsIFrame.h"

class nsIFrame;
class nsSVGFilterFrame;
class nsSVGFilterPaintCallback;

namespace mozilla {
namespace dom {
class SVGFilterElement;
}
}

/**
 * This class helps nsFilterInstance build its filter graph by processing a
 * single SVG reference filter.
 *
 * In BuildPrimitives, this class iterates through the referenced <filter>
 * element's primitive elements, creating a FilterPrimitiveDescription for
 * each one.
 */
class nsSVGFilterInstance
{
  typedef mozilla::gfx::Point3D Point3D;
  typedef mozilla::gfx::IntRect IntRect;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::FilterPrimitiveDescription FilterPrimitiveDescription;

public:
  /**
   * @param aFilter The SVG reference filter to process.
   * @param aTargetFrame The frame of the filtered element under consideration.
   * @param aTargetBBox The SVG bbox to use for the target frame, computed by
   *   the caller. The caller may decide to override the actual SVG bbox.
   */
  nsSVGFilterInstance(const nsStyleFilter& aFilter,
                      nsIFrame *aTargetFrame,
                      const gfxRect& aTargetBBox);

  /**
   * Returns true if the filter instance was created successfully.
   */
  bool IsInitialized() const { return mInitialized; }

  /**
   * Iterates through the <filter> element's primitive elements, creating a
   * FilterPrimitiveDescription for each one. Appends the new
   * FilterPrimitiveDescription(s) to the aPrimitiveDescrs list. Also, appends
   * new images from feImage filter primitive elements to the aInputImages list.
   */
  nsresult BuildPrimitives(nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
                           nsTArray<mozilla::RefPtr<SourceSurface>>& aInputImages);

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
   * the filterRes values.
   */
  nsIntRect GetFilterSpaceBounds() const { return mFilterSpaceBounds; }

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

private:
  /**
   * Finds the filter frame associated with this SVG filter.
   */
  nsSVGFilterFrame* GetFilterFrame();

  /**
   * Computes the filter primitive subregion for the given primitive.
   */
  IntRect ComputeFilterPrimitiveSubregion(nsSVGFE* aFilterElement,
                                          const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
                                          const nsTArray<int32_t>& aInputIndices);

  /**
   * Takes the input indices of a filter primitive and returns for each input
   * whether the input's output is tainted.
   */
  void GetInputsAreTainted(const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
                           const nsTArray<int32_t>& aInputIndices,
                           nsTArray<bool>& aOutInputsAreTainted);

  /**
   * Scales a numeric filter primitive length in the X, Y or "XY" directions
   * into a length in filter space (no offset is applied).
   */
  float GetPrimitiveNumber(uint8_t aCtxType, float aValue) const;

  gfxRect UserSpaceToFilterSpace(const gfxRect& aUserSpace) const;

  /**
   * Returns the transform from frame space to the coordinate space that
   * GetCanvasTM transforms to. "Frame space" is the origin of a frame, aka the
   * top-left corner of its border box, aka the top left corner of its mRect.
   */
  gfxMatrix GetUserSpaceToFrameSpaceInCSSPxTransform() const;

  /**
   * The SVG reference filter originally from the style system.
   */
  const nsStyleFilter mFilter;

  /**
   * The frame for the element that is currently being filtered.
   */
  nsIFrame*               mTargetFrame;

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
  gfxRect                 mTargetBBox;

  /**
   * The "filter region", in the filtered element's user space.
   */
  gfxRect                 mFilterRegion;
  nsIntRect               mFilterSpaceBounds;

  /**
   * The 'primitiveUnits' attribute value (objectBoundingBox or userSpaceOnUse).
   */
  uint16_t                mPrimitiveUnits;

  bool                    mInitialized;
};

#endif
