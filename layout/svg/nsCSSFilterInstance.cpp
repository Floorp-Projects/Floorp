/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsCSSFilterInstance.h"

// Keep others in (case-insensitive) order:
#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "nsStyleStruct.h"
#include "nsTArray.h"

using namespace mozilla;
using namespace mozilla::gfx;

nsCSSFilterInstance::nsCSSFilterInstance(const nsStyleFilter& aFilter,
                                         const nsIntRect& aTargetBBoxInFilterSpace,
                                         const gfxMatrix& aFrameSpaceInCSSPxToFilterSpaceTransform)
  : mFilter(aFilter)
  , mTargetBBoxInFilterSpace(aTargetBBoxInFilterSpace)
  , mFrameSpaceInCSSPxToFilterSpaceTransform(aFrameSpaceInCSSPxToFilterSpaceTransform)
{
}

nsresult
nsCSSFilterInstance::BuildPrimitives(nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs)
{
  FilterPrimitiveDescription descr;
  nsresult result;

  switch(mFilter.GetType()) {
    case NS_STYLE_FILTER_BLUR:
      descr = CreatePrimitiveDescription(PrimitiveType::GaussianBlur, aPrimitiveDescrs);
      result = SetAttributesForBlur(descr);
      break;
    case NS_STYLE_FILTER_BRIGHTNESS:
    case NS_STYLE_FILTER_CONTRAST:
    case NS_STYLE_FILTER_DROP_SHADOW:
    case NS_STYLE_FILTER_GRAYSCALE:
    case NS_STYLE_FILTER_HUE_ROTATE:
    case NS_STYLE_FILTER_INVERT:
    case NS_STYLE_FILTER_OPACITY:
    case NS_STYLE_FILTER_SATURATE:
    case NS_STYLE_FILTER_SEPIA:
      return NS_ERROR_NOT_IMPLEMENTED;
    default:
      NS_NOTREACHED("not a valid CSS filter type");
      return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(result)) {
    return result;
  }

  // Compute the primitive's bounds now that we've determined its attributes.
  // Some attributes like blur radius can influence the bounds.
  SetBounds(descr, aPrimitiveDescrs);

  // Add this primitive to the filter chain.
  aPrimitiveDescrs.AppendElement(descr);
  return NS_OK;
}

FilterPrimitiveDescription
nsCSSFilterInstance::CreatePrimitiveDescription(PrimitiveType aType,
                                                const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs) {
  FilterPrimitiveDescription descr(aType);
  int32_t inputIndex = GetLastResultIndex(aPrimitiveDescrs);
  descr.SetInputPrimitive(0, inputIndex);
  descr.SetIsTainted(inputIndex < 0 ? true : aPrimitiveDescrs[inputIndex].IsTainted());
  descr.SetInputColorSpace(0, ColorSpace::SRGB);
  descr.SetOutputColorSpace(ColorSpace::SRGB);
  return descr;
}

nsresult
nsCSSFilterInstance::SetAttributesForBlur(FilterPrimitiveDescription& aDescr)
{
  // Get the radius from the style.
  nsStyleCoord radiusStyleCoord = mFilter.GetFilterParameter();
  if (radiusStyleCoord.GetUnit() != eStyleUnit_Coord) {
    NS_NOTREACHED("unexpected unit");
    return NS_ERROR_FAILURE;
  }

  // Get the radius in frame space.
  nscoord radiusInFrameSpace = radiusStyleCoord.GetCoordValue();
  float radiusInFrameSpaceInCSSPx =
    nsPresContext::AppUnitsToFloatCSSPixels(radiusInFrameSpace);

  // Convert the radius to filter space.
  gfxSize radiusInFilterSpace(radiusInFrameSpaceInCSSPx,
                              radiusInFrameSpaceInCSSPx);
  gfxSize frameSpaceInCSSPxToFilterSpaceScale =
    mFrameSpaceInCSSPxToFilterSpaceTransform.ScaleFactors(true);
  radiusInFilterSpace.Scale(frameSpaceInCSSPxToFilterSpaceScale.width,
                            frameSpaceInCSSPxToFilterSpaceScale.height);

  // Check the radius limits.
  if (radiusInFilterSpace.width < 0 || radiusInFilterSpace.height < 0) {
    NS_NOTREACHED("we shouldn't have parsed a negative radius in the style");
    return NS_ERROR_FAILURE;
  }
  gfxFloat maxStdDeviation = (gfxFloat)kMaxStdDeviation;
  radiusInFilterSpace.width = std::min(radiusInFilterSpace.width, maxStdDeviation);
  radiusInFilterSpace.height = std::min(radiusInFilterSpace.height, maxStdDeviation);

  // Set the radius parameter.
  aDescr.Attributes().Set(eGaussianBlurStdDeviation, ToSize(radiusInFilterSpace));
  return NS_OK;
}

int32_t
nsCSSFilterInstance::GetLastResultIndex(const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs)
{
  uint32_t numPrimitiveDescrs = aPrimitiveDescrs.Length();
  return !numPrimitiveDescrs ?
    FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic :
    numPrimitiveDescrs - 1;
}

void
nsCSSFilterInstance::SetBounds(FilterPrimitiveDescription& aDescr,
                               const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs) {
  int32_t inputIndex = GetLastResultIndex(aPrimitiveDescrs);
  nsIntRect inputBounds = (inputIndex < 0) ?
    mTargetBBoxInFilterSpace : ThebesIntRect(aPrimitiveDescrs[inputIndex].PrimitiveSubregion());

  nsTArray<nsIntRegion> inputExtents;
  inputExtents.AppendElement(inputBounds);

  nsIntRegion outputExtents =
    FilterSupport::PostFilterExtentsForPrimitive(aDescr, inputExtents);
  IntRect outputBounds = ToIntRect(outputExtents.GetBounds());

  aDescr.SetPrimitiveSubregion(outputBounds);
  aDescr.SetFilterSpaceBounds(outputBounds);
}
