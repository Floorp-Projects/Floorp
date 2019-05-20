/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsCSSFilterInstance.h"

// Keep others in (case-insensitive) order:
#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "nsIFrame.h"
#include "nsStyleStruct.h"
#include "nsTArray.h"

using namespace mozilla;
using namespace mozilla::gfx;

static float ClampFactor(float aFactor) {
  if (aFactor > 1) {
    return 1;
  }
  if (aFactor < 0) {
    MOZ_ASSERT_UNREACHABLE("A negative value should not have been parsed.");
    return 0;
  }

  return aFactor;
}

nsCSSFilterInstance::nsCSSFilterInstance(
    const nsStyleFilter& aFilter, nscolor aShadowFallbackColor,
    const nsIntRect& aTargetBoundsInFilterSpace,
    const gfxMatrix& aFrameSpaceInCSSPxToFilterSpaceTransform)
    : mFilter(aFilter),
      mShadowFallbackColor(aShadowFallbackColor),
      mTargetBoundsInFilterSpace(aTargetBoundsInFilterSpace),
      mFrameSpaceInCSSPxToFilterSpaceTransform(
          aFrameSpaceInCSSPxToFilterSpaceTransform) {}

nsresult nsCSSFilterInstance::BuildPrimitives(
    nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
    bool aInputIsTainted) {
  FilterPrimitiveDescription descr =
      CreatePrimitiveDescription(aPrimitiveDescrs, aInputIsTainted);
  nsresult result;
  switch (mFilter.GetType()) {
    case NS_STYLE_FILTER_BLUR:
      result = SetAttributesForBlur(descr);
      break;
    case NS_STYLE_FILTER_BRIGHTNESS:
      result = SetAttributesForBrightness(descr);
      break;
    case NS_STYLE_FILTER_CONTRAST:
      result = SetAttributesForContrast(descr);
      break;
    case NS_STYLE_FILTER_DROP_SHADOW:
      result = SetAttributesForDropShadow(descr);
      break;
    case NS_STYLE_FILTER_GRAYSCALE:
      result = SetAttributesForGrayscale(descr);
      break;
    case NS_STYLE_FILTER_HUE_ROTATE:
      result = SetAttributesForHueRotate(descr);
      break;
    case NS_STYLE_FILTER_INVERT:
      result = SetAttributesForInvert(descr);
      break;
    case NS_STYLE_FILTER_OPACITY:
      result = SetAttributesForOpacity(descr);
      break;
    case NS_STYLE_FILTER_SATURATE:
      result = SetAttributesForSaturate(descr);
      break;
    case NS_STYLE_FILTER_SEPIA:
      result = SetAttributesForSepia(descr);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("not a valid CSS filter type");
      return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(result)) {
    return result;
  }

  // Compute the primitive's bounds now that we've determined its attributes.
  // Some attributes like blur radius can influence the bounds.
  SetBounds(descr, aPrimitiveDescrs);

  // Add this primitive to the filter chain.
  aPrimitiveDescrs.AppendElement(std::move(descr));
  return NS_OK;
}

FilterPrimitiveDescription nsCSSFilterInstance::CreatePrimitiveDescription(
    const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
    bool aInputIsTainted) {
  FilterPrimitiveDescription descr;
  int32_t inputIndex = GetLastResultIndex(aPrimitiveDescrs);
  descr.SetInputPrimitive(0, inputIndex);
  descr.SetIsTainted(inputIndex < 0 ? aInputIsTainted
                                    : aPrimitiveDescrs[inputIndex].IsTainted());
  descr.SetInputColorSpace(0, ColorSpace::SRGB);
  descr.SetOutputColorSpace(ColorSpace::SRGB);
  return descr;
}

nsresult nsCSSFilterInstance::SetAttributesForBlur(
    FilterPrimitiveDescription& aDescr) {
  const nsStyleCoord& radiusInFrameSpace = mFilter.GetFilterParameter();
  if (radiusInFrameSpace.GetUnit() != eStyleUnit_Coord) {
    MOZ_ASSERT_UNREACHABLE("unexpected unit");
    return NS_ERROR_FAILURE;
  }

  Size radiusInFilterSpace =
      BlurRadiusToFilterSpace(radiusInFrameSpace.GetCoordValue());
  GaussianBlurAttributes atts;
  atts.mStdDeviation = radiusInFilterSpace;
  aDescr.Attributes() = AsVariant(atts);
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForBrightness(
    FilterPrimitiveDescription& aDescr) {
  const nsStyleCoord& styleValue = mFilter.GetFilterParameter();
  float value = styleValue.GetFactorOrPercentValue();
  float intercept = 0.0f;
  ComponentTransferAttributes atts;

  // Set transfer functions for RGB.
  atts.mTypes[kChannelROrRGB] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_LINEAR;
  atts.mTypes[kChannelG] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN;
  atts.mTypes[kChannelB] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN;
  float slopeIntercept[2];
  slopeIntercept[kComponentTransferSlopeIndex] = value;
  slopeIntercept[kComponentTransferInterceptIndex] = intercept;
  atts.mValues[kChannelROrRGB].AppendElements(slopeIntercept, 2);

  atts.mTypes[kChannelA] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY;

  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForContrast(
    FilterPrimitiveDescription& aDescr) {
  const nsStyleCoord& styleValue = mFilter.GetFilterParameter();
  float value = styleValue.GetFactorOrPercentValue();
  float intercept = -(0.5 * value) + 0.5;
  ComponentTransferAttributes atts;

  // Set transfer functions for RGB.
  atts.mTypes[kChannelROrRGB] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_LINEAR;
  atts.mTypes[kChannelG] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN;
  atts.mTypes[kChannelB] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN;
  float slopeIntercept[2];
  slopeIntercept[kComponentTransferSlopeIndex] = value;
  slopeIntercept[kComponentTransferInterceptIndex] = intercept;
  atts.mValues[kChannelROrRGB].AppendElements(slopeIntercept, 2);

  atts.mTypes[kChannelA] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY;

  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForDropShadow(
    FilterPrimitiveDescription& aDescr) {
  const auto& shadow = mFilter.GetDropShadow();

  DropShadowAttributes atts;

  // Set drop shadow blur radius.
  Size radiusInFilterSpace = BlurRadiusToFilterSpace(shadow.blur.ToAppUnits());
  atts.mStdDeviation = radiusInFilterSpace;

  // Set offset.
  IntPoint offsetInFilterSpace = OffsetToFilterSpace(
      shadow.horizontal.ToAppUnits(), shadow.vertical.ToAppUnits());
  atts.mOffset = offsetInFilterSpace;

  // Set color. If unspecified, use the CSS color property.
  nscolor shadowColor = shadow.color.CalcColor(mShadowFallbackColor);
  atts.mColor = ToAttributeColor(shadowColor);

  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForGrayscale(
    FilterPrimitiveDescription& aDescr) {
  ColorMatrixAttributes atts;
  // Set color matrix type.
  atts.mType = (uint32_t)SVG_FECOLORMATRIX_TYPE_SATURATE;

  // Set color matrix values.
  const nsStyleCoord& styleValue = mFilter.GetFilterParameter();
  float value = 1 - ClampFactor(styleValue.GetFactorOrPercentValue());
  atts.mValues.AppendElements(&value, 1);

  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForHueRotate(
    FilterPrimitiveDescription& aDescr) {
  ColorMatrixAttributes atts;
  // Set color matrix type.
  atts.mType = (uint32_t)SVG_FECOLORMATRIX_TYPE_HUE_ROTATE;

  // Set color matrix values.
  const nsStyleCoord& styleValue = mFilter.GetFilterParameter();
  float value = styleValue.GetAngleValueInDegrees();
  atts.mValues.AppendElements(&value, 1);

  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForInvert(
    FilterPrimitiveDescription& aDescr) {
  ComponentTransferAttributes atts;
  const nsStyleCoord& styleValue = mFilter.GetFilterParameter();
  float value = ClampFactor(styleValue.GetFactorOrPercentValue());

  // Set transfer functions for RGB.
  float invertTableValues[2];
  invertTableValues[0] = value;
  invertTableValues[1] = 1 - value;

  // Set transfer functions for RGB.
  atts.mTypes[kChannelROrRGB] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_TABLE;
  atts.mTypes[kChannelG] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN;
  atts.mTypes[kChannelB] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN;
  atts.mValues[kChannelROrRGB].AppendElements(invertTableValues, 2);

  atts.mTypes[kChannelA] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY;

  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForOpacity(
    FilterPrimitiveDescription& aDescr) {
  OpacityAttributes atts;
  const nsStyleCoord& styleValue = mFilter.GetFilterParameter();
  float value = ClampFactor(styleValue.GetFactorOrPercentValue());

  atts.mOpacity = value;
  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForSaturate(
    FilterPrimitiveDescription& aDescr) {
  ColorMatrixAttributes atts;
  // Set color matrix type.
  atts.mType = (uint32_t)SVG_FECOLORMATRIX_TYPE_SATURATE;

  // Set color matrix values.
  const nsStyleCoord& styleValue = mFilter.GetFilterParameter();
  float value = styleValue.GetFactorOrPercentValue();
  atts.mValues.AppendElements(&value, 1);

  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

nsresult nsCSSFilterInstance::SetAttributesForSepia(
    FilterPrimitiveDescription& aDescr) {
  ColorMatrixAttributes atts;
  // Set color matrix type.
  atts.mType = (uint32_t)SVG_FECOLORMATRIX_TYPE_SEPIA;

  // Set color matrix values.
  const nsStyleCoord& styleValue = mFilter.GetFilterParameter();
  float value = ClampFactor(styleValue.GetFactorOrPercentValue());
  atts.mValues.AppendElements(&value, 1);

  aDescr.Attributes() = AsVariant(std::move(atts));
  return NS_OK;
}

Size nsCSSFilterInstance::BlurRadiusToFilterSpace(nscoord aRadiusInFrameSpace) {
  float radiusInFrameSpaceInCSSPx =
      nsPresContext::AppUnitsToFloatCSSPixels(aRadiusInFrameSpace);

  // Convert the radius to filter space.
  Size radiusInFilterSpace(radiusInFrameSpaceInCSSPx,
                           radiusInFrameSpaceInCSSPx);
  gfxSize frameSpaceInCSSPxToFilterSpaceScale =
      mFrameSpaceInCSSPxToFilterSpaceTransform.ScaleFactors(true);
  radiusInFilterSpace.Scale(frameSpaceInCSSPxToFilterSpaceScale.width,
                            frameSpaceInCSSPxToFilterSpaceScale.height);

  // Check the radius limits.
  if (radiusInFilterSpace.width < 0 || radiusInFilterSpace.height < 0) {
    MOZ_ASSERT_UNREACHABLE(
        "we shouldn't have parsed a negative radius in the "
        "style");
    return Size();
  }

  Float maxStdDeviation = (Float)kMaxStdDeviation;
  radiusInFilterSpace.width =
      std::min(radiusInFilterSpace.width, maxStdDeviation);
  radiusInFilterSpace.height =
      std::min(radiusInFilterSpace.height, maxStdDeviation);

  return radiusInFilterSpace;
}

IntPoint nsCSSFilterInstance::OffsetToFilterSpace(
    nscoord aXOffsetInFrameSpace, nscoord aYOffsetInFrameSpace) {
  gfxPoint offsetInFilterSpace(
      nsPresContext::AppUnitsToFloatCSSPixels(aXOffsetInFrameSpace),
      nsPresContext::AppUnitsToFloatCSSPixels(aYOffsetInFrameSpace));

  // Convert the radius to filter space.
  gfxSize frameSpaceInCSSPxToFilterSpaceScale =
      mFrameSpaceInCSSPxToFilterSpaceTransform.ScaleFactors(true);
  offsetInFilterSpace.x *= frameSpaceInCSSPxToFilterSpaceScale.width;
  offsetInFilterSpace.y *= frameSpaceInCSSPxToFilterSpaceScale.height;

  return IntPoint(int32_t(offsetInFilterSpace.x),
                  int32_t(offsetInFilterSpace.y));
}

Color nsCSSFilterInstance::ToAttributeColor(nscolor aColor) {
  return Color(NS_GET_R(aColor) / 255.0, NS_GET_G(aColor) / 255.0,
               NS_GET_B(aColor) / 255.0, NS_GET_A(aColor) / 255.0);
}

int32_t nsCSSFilterInstance::GetLastResultIndex(
    const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs) {
  uint32_t numPrimitiveDescrs = aPrimitiveDescrs.Length();
  return !numPrimitiveDescrs
             ? FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic
             : numPrimitiveDescrs - 1;
}

void nsCSSFilterInstance::SetBounds(
    FilterPrimitiveDescription& aDescr,
    const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs) {
  int32_t inputIndex = GetLastResultIndex(aPrimitiveDescrs);
  nsIntRect inputBounds =
      (inputIndex < 0) ? mTargetBoundsInFilterSpace
                       : aPrimitiveDescrs[inputIndex].PrimitiveSubregion();

  nsTArray<nsIntRegion> inputExtents;
  inputExtents.AppendElement(inputBounds);

  nsIntRegion outputExtents =
      FilterSupport::PostFilterExtentsForPrimitive(aDescr, inputExtents);
  IntRect outputBounds = outputExtents.GetBounds();

  aDescr.SetPrimitiveSubregion(outputBounds);
  aDescr.SetFilterSpaceBounds(outputBounds);
}
