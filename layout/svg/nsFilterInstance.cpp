/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsFilterInstance.h"

// MFBT headers next:
#include "mozilla/UniquePtr.h"

// Keep others in (case-insensitive) order:
#include "FilterSupport.h"
#include "ImgDrawResult.h"
#include "SVGContentUtils.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PatternHelpers.h"
#include "nsCSSFilterInstance.h"
#include "nsSVGDisplayableFrame.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

FilterDescription nsFilterInstance::GetFilterDescription(
    nsIContent* aFilteredElement, const nsTArray<nsStyleFilter>& aFilterChain,
    bool aFilterInputIsTainted, const UserSpaceMetrics& aMetrics,
    const gfxRect& aBBox,
    nsTArray<RefPtr<SourceSurface>>& aOutAdditionalImages) {
  gfxMatrix identity;
  nsFilterInstance instance(nullptr, aFilteredElement, aMetrics, aFilterChain,
                            aFilterInputIsTainted, nullptr, identity, nullptr,
                            nullptr, nullptr, &aBBox);
  if (!instance.IsInitialized()) {
    return FilterDescription();
  }
  return instance.ExtractDescriptionAndAdditionalImages(aOutAdditionalImages);
}

static UniquePtr<UserSpaceMetrics> UserSpaceMetricsForFrame(nsIFrame* aFrame) {
  if (aFrame->GetContent()->IsSVGElement()) {
    SVGElement* element = static_cast<SVGElement*>(aFrame->GetContent());
    return MakeUnique<SVGElementMetrics>(element);
  }
  return MakeUnique<NonSVGFrameUserSpaceMetrics>(aFrame);
}

void nsFilterInstance::PaintFilteredFrame(
    nsIFrame* aFilteredFrame, gfxContext* aCtx,
    nsSVGFilterPaintCallback* aPaintCallback, const nsRegion* aDirtyArea,
    imgDrawingParams& aImgParams, float aOpacity) {
  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics =
      UserSpaceMetricsForFrame(aFilteredFrame);

  gfxContextMatrixAutoSaveRestore autoSR(aCtx);
  gfxSize scaleFactors = aCtx->CurrentMatrixDouble().ScaleFactors(true);
  if (scaleFactors.IsEmpty()) {
    return;
  }

  gfxMatrix scaleMatrix(scaleFactors.width, 0.0f, 0.0f, scaleFactors.height,
                        0.0f, 0.0f);

  gfxMatrix reverseScaleMatrix = scaleMatrix;
  DebugOnly<bool> invertible = reverseScaleMatrix.Invert();
  MOZ_ASSERT(invertible);
  // Pull scale vector out of aCtx's transform, put all scale factors, which
  // includes css and css-to-dev-px scale, into scaleMatrixInDevUnits.
  aCtx->SetMatrixDouble(reverseScaleMatrix * aCtx->CurrentMatrixDouble());

  gfxMatrix scaleMatrixInDevUnits =
      scaleMatrix * nsSVGUtils::GetCSSPxToDevPxMatrix(aFilteredFrame);

  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, /* InputIsTainted */ true,
                            aPaintCallback, scaleMatrixInDevUnits, aDirtyArea,
                            nullptr, nullptr, nullptr);
  if (instance.IsInitialized()) {
    instance.Render(aCtx, aImgParams, aOpacity);
  }
}

static mozilla::wr::ComponentTransferFuncType FuncTypeToWr(uint8_t aFuncType) {
  switch (aFuncType) {
    case SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
      return mozilla::wr::ComponentTransferFuncType::Identity;
    case SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
      return mozilla::wr::ComponentTransferFuncType::Table;
    case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
      return mozilla::wr::ComponentTransferFuncType::Discrete;
    case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
      return mozilla::wr::ComponentTransferFuncType::Linear;
    case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
      return mozilla::wr::ComponentTransferFuncType::Gamma;
    default:
      MOZ_ASSERT(false, "unknown func type?");
  }
  MOZ_ASSERT(false, "unknown func type?");
  return mozilla::wr::ComponentTransferFuncType::Identity;
}

bool nsFilterInstance::BuildWebRenderFilters(nsIFrame* aFilteredFrame,
                                             WrFiltersHolder& aWrFilters,
                                             Maybe<nsRect>& aPostFilterClip) {
  aWrFilters.filters.Clear();
  aWrFilters.filter_datas.Clear();
  aWrFilters.values.Clear();

  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics =
      UserSpaceMetricsForFrame(aFilteredFrame);

  // TODO: simply using an identity matrix here, was pulling the scale from a
  // gfx context for the non-wr path.
  gfxMatrix scaleMatrix;
  gfxMatrix scaleMatrixInDevUnits =
      scaleMatrix * nsSVGUtils::GetCSSPxToDevPxMatrix(aFilteredFrame);

  // Hardcode inputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  bool inputIsTainted = true;
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, inputIsTainted, nullptr,
                            scaleMatrixInDevUnits, nullptr, nullptr, nullptr,
                            nullptr);

  if (!instance.IsInitialized()) {
    return false;
  }

  // If there are too many filters to render, then just pretend that we
  // succeeded, and don't render any of them.
  if (instance.mFilterDescription.mPrimitives.Length() >
      gfxPrefs::WebRenderMaxFilterOpsPerChain()) {
    return true;
  }

  Maybe<IntRect> finalClip;
  bool srgb = true;
  // We currently apply the clip on the stacking context after applying filters,
  // but primitive subregions imply clipping after each filter and not just the
  // end of the chain. For some types of filter it doesn't matter, but for those
  // which sample outside of the location of the destination pixel like blurs,
  // only clipping after could produce incorrect results, so we bail out in this
  // case.
  // We can lift this restriction once we have added support for primitive
  // subregions to WebRender's filters.
  for (uint32_t i = 0; i < instance.mFilterDescription.mPrimitives.Length();
       i++) {
    const auto& primitive = instance.mFilterDescription.mPrimitives[i];

    // WebRender only supports filters with one input.
    if (primitive.NumberOfInputs() != 1) {
      return false;
    }
    // The first primitive must have the source graphic as the input, all
    // other primitives must have the prior primitive as the input, otherwise
    // it's not supported by WebRender.
    if (i == 0) {
      if (primitive.InputPrimitiveIndex(0) !=
          FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic) {
        return false;
      }
    } else if (primitive.InputPrimitiveIndex(0) != int32_t(i - 1)) {
      return false;
    }

    bool previousSrgb = srgb;
    bool primNeedsSrgb = primitive.InputColorSpace(0) == gfx::ColorSpace::SRGB;
    if (srgb && !primNeedsSrgb) {
      aWrFilters.filters.AppendElement(wr::FilterOp::SrgbToLinear());
    } else if (!srgb && primNeedsSrgb) {
      aWrFilters.filters.AppendElement(wr::FilterOp::LinearToSrgb());
    }
    srgb = primitive.OutputColorSpace() == gfx::ColorSpace::SRGB;

    const PrimitiveAttributes& attr = primitive.Attributes();

    bool filterIsNoop = false;

    if (attr.is<OpacityAttributes>()) {
      float opacity = attr.as<OpacityAttributes>().mOpacity;
      aWrFilters.filters.AppendElement(wr::FilterOp::Opacity(
          wr::PropertyBinding<float>::Value(opacity), opacity));
    } else if (attr.is<ColorMatrixAttributes>()) {
      const ColorMatrixAttributes& attributes =
          attr.as<ColorMatrixAttributes>();

      float transposed[20];
      if (!gfx::ComputeColorMatrix(attributes, transposed)) {
        filterIsNoop = true;
        continue;
      }

      auto almostEq = [](float a, float b) -> bool {
        return fabs(a - b) < 0.00001;
      };

      if (!almostEq(transposed[15], 0.0) || !almostEq(transposed[16], 0.0) ||
          !almostEq(transposed[17], 0.0) || !almostEq(transposed[18], 1.0) ||
          !almostEq(transposed[3], 0.0) || !almostEq(transposed[8], 0.0) ||
          !almostEq(transposed[13], 0.0)) {
        // WebRender currently pretends to take the full 4x5 matrix but discards
        // the components related to alpha. So bail out in this case until
        // it is fixed.
        return false;
      }

      float matrix[20] = {
          transposed[0], transposed[5], transposed[10], transposed[15],
          transposed[1], transposed[6], transposed[11], transposed[16],
          transposed[2], transposed[7], transposed[12], transposed[17],
          transposed[3], transposed[8], transposed[13], transposed[18],
          transposed[4], transposed[9], transposed[14], transposed[19]};

      aWrFilters.filters.AppendElement(wr::FilterOp::ColorMatrix(matrix));
    } else if (attr.is<GaussianBlurAttributes>()) {
      if (finalClip) {
        // There's a clip that needs to apply before the blur filter, but
        // WebRender only lets us apply the clip at the end of the filter
        // chain. Clipping after a blur is not equivalent to clipping before
        // a blur, so bail out.
        return false;
      }

      const GaussianBlurAttributes& blur = attr.as<GaussianBlurAttributes>();

      const Size& stdDev = blur.mStdDeviation;
      if (stdDev.width != stdDev.height) {
        return false;
      }

      float radius = stdDev.width;
      if (radius != 0.0) {
        aWrFilters.filters.AppendElement(wr::FilterOp::Blur(radius));
      } else {
        filterIsNoop = true;
      }
    } else if (attr.is<DropShadowAttributes>()) {
      if (finalClip) {
        // We have to bail out for the same reason we would with a blur filter.
        return false;
      }

      const DropShadowAttributes& shadow = attr.as<DropShadowAttributes>();

      const Size& stdDev = shadow.mStdDeviation;
      if (stdDev.width != stdDev.height) {
        return false;
      }

      Color color = shadow.mColor;
      if (!primNeedsSrgb) {
        color = Color(gsRGBToLinearRGBMap[uint8_t(color.r * 255)],
                      gsRGBToLinearRGBMap[uint8_t(color.g * 255)],
                      gsRGBToLinearRGBMap[uint8_t(color.b * 255)], color.a);
      }
      wr::Shadow wrShadow;
      wrShadow.offset = {(float)shadow.mOffset.x, (float)shadow.mOffset.y};
      wrShadow.color = wr::ToColorF(ToDeviceColor(color));
      wrShadow.blur_radius = stdDev.width;
      wr::FilterOp filterOp = wr::FilterOp::DropShadow(wrShadow);

      aWrFilters.filters.AppendElement(filterOp);
    } else if (attr.is<ComponentTransferAttributes>()) {
      const ComponentTransferAttributes& attributes =
          attr.as<ComponentTransferAttributes>();

      size_t numValues =
          attributes.mValues[0].Length() + attributes.mValues[1].Length() +
          attributes.mValues[2].Length() + attributes.mValues[3].Length();
      if (numValues > 1024) {
        // Depending on how the wr shaders are implemented we may need to
        // limit the total number of values.
        return false;
      }

      wr::FilterOp filterOp = {wr::FilterOp::Tag::ComponentTransfer};
      wr::WrFilterData filterData;
      aWrFilters.values.AppendElement(nsTArray<float>());
      nsTArray<float>* values =
          &aWrFilters.values[aWrFilters.values.Length() - 1];
      values->SetCapacity(numValues);

      filterData.funcR_type = FuncTypeToWr(attributes.mTypes[0]);
      size_t R_startindex = values->Length();
      values->AppendElements(attributes.mValues[0]);
      filterData.R_values_count = attributes.mValues[0].Length();

      filterData.funcG_type = FuncTypeToWr(attributes.mTypes[1]);
      size_t G_startindex = values->Length();
      values->AppendElements(attributes.mValues[1]);
      filterData.G_values_count = attributes.mValues[1].Length();

      filterData.funcB_type = FuncTypeToWr(attributes.mTypes[2]);
      size_t B_startindex = values->Length();
      values->AppendElements(attributes.mValues[2]);
      filterData.B_values_count = attributes.mValues[2].Length();

      filterData.funcA_type = FuncTypeToWr(attributes.mTypes[3]);
      size_t A_startindex = values->Length();
      values->AppendElements(attributes.mValues[3]);
      filterData.A_values_count = attributes.mValues[3].Length();

      filterData.R_values =
          filterData.R_values_count > 0 ? &((*values)[R_startindex]) : nullptr;
      filterData.G_values =
          filterData.G_values_count > 0 ? &((*values)[G_startindex]) : nullptr;
      filterData.B_values =
          filterData.B_values_count > 0 ? &((*values)[B_startindex]) : nullptr;
      filterData.A_values =
          filterData.A_values_count > 0 ? &((*values)[A_startindex]) : nullptr;

      aWrFilters.filters.AppendElement(filterOp);
      aWrFilters.filter_datas.AppendElement(filterData);
    } else {
      return false;
    }

    if (filterIsNoop && aWrFilters.filters.Length() > 0 &&
        (aWrFilters.filters.LastElement().tag ==
             wr::FilterOp::Tag::SrgbToLinear ||
         aWrFilters.filters.LastElement().tag ==
             wr::FilterOp::Tag::LinearToSrgb)) {
      // We pushed a color space conversion filter in prevision of applying
      // another filter which turned out to be a no-op, so the conversion is
      // unnecessary. Remove it from the filter list.
      // This is both an optimization and a way to pass the wptest
      // css/filter-effects/filter-scale-001.html for which the needless
      // sRGB->linear->no-op->sRGB roundtrip introduces a slight error and we
      // cannot add fuzziness to the test.
      Unused << aWrFilters.filters.PopLastElement();
      srgb = previousSrgb;
    }

    if (!filterIsNoop) {
      if (finalClip.isNothing()) {
        finalClip = Some(primitive.PrimitiveSubregion());
      } else {
        finalClip =
            Some(primitive.PrimitiveSubregion().Intersect(finalClip.value()));
      }
    }
  }

  if (!srgb) {
    aWrFilters.filters.AppendElement(wr::FilterOp::LinearToSrgb());
  }

  if (finalClip) {
    aPostFilterClip = Some(instance.FilterSpaceToFrameSpace(finalClip.value()));
  }
  return true;
}

nsRegion nsFilterInstance::GetPostFilterDirtyArea(
    nsIFrame* aFilteredFrame, const nsRegion& aPreFilterDirtyRegion) {
  if (aPreFilterDirtyRegion.IsEmpty()) {
    return nsRegion();
  }

  gfxMatrix tm = nsSVGUtils::GetCanvasTM(aFilteredFrame);
  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics =
      UserSpaceMetricsForFrame(aFilteredFrame);
  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, /* InputIsTainted */ true,
                            nullptr, tm, nullptr, &aPreFilterDirtyRegion);
  if (!instance.IsInitialized()) {
    return nsRegion();
  }

  // We've passed in the source's dirty area so the instance knows about it.
  // Now we can ask the instance to compute the area of the filter output
  // that's dirty.
  return instance.ComputePostFilterDirtyRegion();
}

nsRegion nsFilterInstance::GetPreFilterNeededArea(
    nsIFrame* aFilteredFrame, const nsRegion& aPostFilterDirtyRegion) {
  gfxMatrix tm = nsSVGUtils::GetCanvasTM(aFilteredFrame);
  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics =
      UserSpaceMetricsForFrame(aFilteredFrame);
  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, /* InputIsTainted */ true,
                            nullptr, tm, &aPostFilterDirtyRegion);
  if (!instance.IsInitialized()) {
    return nsRect();
  }

  // Now we can ask the instance to compute the area of the source
  // that's needed.
  return instance.ComputeSourceNeededRect();
}

nsRect nsFilterInstance::GetPostFilterBounds(nsIFrame* aFilteredFrame,
                                             const gfxRect* aOverrideBBox,
                                             const nsRect* aPreFilterBounds) {
  MOZ_ASSERT(!(aFilteredFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) ||
                 !(aFilteredFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
             "Non-display SVG do not maintain visual overflow rects");

  nsRegion preFilterRegion;
  nsRegion* preFilterRegionPtr = nullptr;
  if (aPreFilterBounds) {
    preFilterRegion = *aPreFilterBounds;
    preFilterRegionPtr = &preFilterRegion;
  }

  gfxMatrix tm = nsSVGUtils::GetCanvasTM(aFilteredFrame);
  auto& filterChain = aFilteredFrame->StyleEffects()->mFilters;
  UniquePtr<UserSpaceMetrics> metrics =
      UserSpaceMetricsForFrame(aFilteredFrame);
  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  nsFilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                            *metrics, filterChain, /* InputIsTainted */ true,
                            nullptr, tm, nullptr, preFilterRegionPtr,
                            aPreFilterBounds, aOverrideBBox);
  if (!instance.IsInitialized()) {
    return nsRect();
  }

  return instance.ComputePostFilterExtents();
}

nsFilterInstance::nsFilterInstance(
    nsIFrame* aTargetFrame, nsIContent* aTargetContent,
    const UserSpaceMetrics& aMetrics,
    const nsTArray<nsStyleFilter>& aFilterChain, bool aFilterInputIsTainted,
    nsSVGFilterPaintCallback* aPaintCallback, const gfxMatrix& aPaintTransform,
    const nsRegion* aPostFilterDirtyRegion,
    const nsRegion* aPreFilterDirtyRegion,
    const nsRect* aPreFilterVisualOverflowRectOverride,
    const gfxRect* aOverrideBBox)
    : mTargetFrame(aTargetFrame),
      mTargetContent(aTargetContent),
      mMetrics(aMetrics),
      mPaintCallback(aPaintCallback),
      mPaintTransform(aPaintTransform),
      mInitialized(false) {
  if (aOverrideBBox) {
    mTargetBBox = *aOverrideBBox;
  } else {
    MOZ_ASSERT(mTargetFrame,
               "Need to supply a frame when there's no aOverrideBBox");
    mTargetBBox = nsSVGUtils::GetBBox(mTargetFrame,
                                      nsSVGUtils::eUseFrameBoundsForOuterSVG |
                                          nsSVGUtils::eBBoxIncludeFillGeometry);
  }

  // Compute user space to filter space transforms.
  if (!ComputeUserSpaceToFilterSpaceScale()) {
    return;
  }

  if (!ComputeTargetBBoxInFilterSpace()) {
    return;
  }

  // Get various transforms:
  gfxMatrix filterToUserSpace(mFilterSpaceToUserSpaceScale.width, 0.0f, 0.0f,
                              mFilterSpaceToUserSpaceScale.height, 0.0f, 0.0f);

  mFilterSpaceToFrameSpaceInCSSPxTransform =
      filterToUserSpace * GetUserSpaceToFrameSpaceInCSSPxTransform();
  // mFilterSpaceToFrameSpaceInCSSPxTransform is always invertible
  mFrameSpaceInCSSPxToFilterSpaceTransform =
      mFilterSpaceToFrameSpaceInCSSPxTransform;
  mFrameSpaceInCSSPxToFilterSpaceTransform.Invert();

  nsIntRect targetBounds;
  if (aPreFilterVisualOverflowRectOverride) {
    targetBounds =
        FrameSpaceToFilterSpace(aPreFilterVisualOverflowRectOverride);
  } else if (mTargetFrame) {
    nsRect preFilterVOR = mTargetFrame->GetPreEffectsVisualOverflowRect();
    targetBounds = FrameSpaceToFilterSpace(&preFilterVOR);
  }
  mTargetBounds.UnionRect(mTargetBBoxInFilterSpace, targetBounds);

  // Build the filter graph.
  if (NS_FAILED(
          BuildPrimitives(aFilterChain, aTargetFrame, aFilterInputIsTainted))) {
    return;
  }

  // Convert the passed in rects from frame space to filter space:
  mPostFilterDirtyRegion = FrameSpaceToFilterSpace(aPostFilterDirtyRegion);
  mPreFilterDirtyRegion = FrameSpaceToFilterSpace(aPreFilterDirtyRegion);

  mInitialized = true;
}

bool nsFilterInstance::ComputeTargetBBoxInFilterSpace() {
  gfxRect targetBBoxInFilterSpace = UserSpaceToFilterSpace(mTargetBBox);
  targetBBoxInFilterSpace.RoundOut();

  return gfxUtils::GfxRectToIntRect(targetBBoxInFilterSpace,
                                    &mTargetBBoxInFilterSpace);
}

bool nsFilterInstance::ComputeUserSpaceToFilterSpaceScale() {
  if (mTargetFrame) {
    mUserSpaceToFilterSpaceScale = mPaintTransform.ScaleFactors(true);
    if (mUserSpaceToFilterSpaceScale.width <= 0.0f ||
        mUserSpaceToFilterSpaceScale.height <= 0.0f) {
      // Nothing should be rendered.
      return false;
    }
  } else {
    mUserSpaceToFilterSpaceScale = gfxSize(1.0, 1.0);
  }

  mFilterSpaceToUserSpaceScale =
      gfxSize(1.0f / mUserSpaceToFilterSpaceScale.width,
              1.0f / mUserSpaceToFilterSpaceScale.height);

  return true;
}

gfxRect nsFilterInstance::UserSpaceToFilterSpace(
    const gfxRect& aUserSpaceRect) const {
  gfxRect filterSpaceRect = aUserSpaceRect;
  filterSpaceRect.Scale(mUserSpaceToFilterSpaceScale.width,
                        mUserSpaceToFilterSpaceScale.height);
  return filterSpaceRect;
}

gfxRect nsFilterInstance::FilterSpaceToUserSpace(
    const gfxRect& aFilterSpaceRect) const {
  gfxRect userSpaceRect = aFilterSpaceRect;
  userSpaceRect.Scale(mFilterSpaceToUserSpaceScale.width,
                      mFilterSpaceToUserSpaceScale.height);
  return userSpaceRect;
}

nsresult nsFilterInstance::BuildPrimitives(
    const nsTArray<nsStyleFilter>& aFilterChain, nsIFrame* aTargetFrame,
    bool aFilterInputIsTainted) {
  nsTArray<FilterPrimitiveDescription> primitiveDescriptions;

  for (uint32_t i = 0; i < aFilterChain.Length(); i++) {
    bool inputIsTainted = primitiveDescriptions.IsEmpty()
                              ? aFilterInputIsTainted
                              : primitiveDescriptions.LastElement().IsTainted();
    nsresult rv = BuildPrimitivesForFilter(
        aFilterChain[i], aTargetFrame, inputIsTainted, primitiveDescriptions);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mFilterDescription = FilterDescription(std::move(primitiveDescriptions));

  return NS_OK;
}

nsresult nsFilterInstance::BuildPrimitivesForFilter(
    const nsStyleFilter& aFilter, nsIFrame* aTargetFrame, bool aInputIsTainted,
    nsTArray<FilterPrimitiveDescription>& aPrimitiveDescriptions) {
  NS_ASSERTION(mUserSpaceToFilterSpaceScale.width > 0.0f &&
                   mFilterSpaceToUserSpaceScale.height > 0.0f,
               "scale factors between spaces should be positive values");

  if (aFilter.GetType() == NS_STYLE_FILTER_URL) {
    // Build primitives for an SVG filter.
    nsSVGFilterInstance svgFilterInstance(aFilter, aTargetFrame, mTargetContent,
                                          mMetrics, mTargetBBox,
                                          mUserSpaceToFilterSpaceScale);
    if (!svgFilterInstance.IsInitialized()) {
      return NS_ERROR_FAILURE;
    }

    return svgFilterInstance.BuildPrimitives(aPrimitiveDescriptions,
                                             mInputImages, aInputIsTainted);
  }

  // Build primitives for a CSS filter.

  // If we don't have a frame, use opaque black for shadows with unspecified
  // shadow colors.
  nscolor shadowFallbackColor =
      mTargetFrame ? mTargetFrame->StyleColor()->mColor.ToColor()
                   : NS_RGB(0, 0, 0);

  nsCSSFilterInstance cssFilterInstance(
      aFilter, shadowFallbackColor, mTargetBounds,
      mFrameSpaceInCSSPxToFilterSpaceTransform);
  return cssFilterInstance.BuildPrimitives(aPrimitiveDescriptions,
                                           aInputIsTainted);
}

static void UpdateNeededBounds(const nsIntRegion& aRegion, nsIntRect& aBounds) {
  aBounds = aRegion.GetBounds();

  bool overflow;
  IntSize surfaceSize =
      nsSVGUtils::ConvertToSurfaceSize(SizeDouble(aBounds.Size()), &overflow);
  if (overflow) {
    aBounds.SizeTo(surfaceSize);
  }
}

void nsFilterInstance::ComputeNeededBoxes() {
  if (mFilterDescription.mPrimitives.IsEmpty()) {
    return;
  }

  nsIntRegion sourceGraphicNeededRegion;
  nsIntRegion fillPaintNeededRegion;
  nsIntRegion strokePaintNeededRegion;

  FilterSupport::ComputeSourceNeededRegions(
      mFilterDescription, mPostFilterDirtyRegion, sourceGraphicNeededRegion,
      fillPaintNeededRegion, strokePaintNeededRegion);

  sourceGraphicNeededRegion.And(sourceGraphicNeededRegion, mTargetBounds);

  UpdateNeededBounds(sourceGraphicNeededRegion, mSourceGraphic.mNeededBounds);
  UpdateNeededBounds(fillPaintNeededRegion, mFillPaint.mNeededBounds);
  UpdateNeededBounds(strokePaintNeededRegion, mStrokePaint.mNeededBounds);
}

void nsFilterInstance::BuildSourcePaint(SourceInfo* aSource,
                                        imgDrawingParams& aImgParams) {
  MOZ_ASSERT(mTargetFrame);
  nsIntRect neededRect = aSource->mNeededBounds;
  if (neededRect.IsEmpty()) {
    return;
  }

  RefPtr<DrawTarget> offscreenDT =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          neededRect.Size(), SurfaceFormat::B8G8R8A8);
  if (!offscreenDT || !offscreenDT->IsValid()) {
    return;
  }

  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(offscreenDT);
  MOZ_ASSERT(ctx);  // already checked the draw target above
  gfxContextAutoSaveRestore saver(ctx);

  ctx->SetMatrixDouble(mPaintTransform *
                       gfxMatrix::Translation(-neededRect.TopLeft()));
  GeneralPattern pattern;
  if (aSource == &mFillPaint) {
    nsSVGUtils::MakeFillPatternFor(mTargetFrame, ctx, &pattern, aImgParams);
  } else if (aSource == &mStrokePaint) {
    nsSVGUtils::MakeStrokePatternFor(mTargetFrame, ctx, &pattern, aImgParams);
  }

  if (pattern.GetPattern()) {
    offscreenDT->FillRect(
        ToRect(FilterSpaceToUserSpace(ThebesRect(neededRect))), pattern);
  }

  aSource->mSourceSurface = offscreenDT->Snapshot();
  aSource->mSurfaceRect = neededRect;
}

void nsFilterInstance::BuildSourcePaints(imgDrawingParams& aImgParams) {
  if (!mFillPaint.mNeededBounds.IsEmpty()) {
    BuildSourcePaint(&mFillPaint, aImgParams);
  }

  if (!mStrokePaint.mNeededBounds.IsEmpty()) {
    BuildSourcePaint(&mStrokePaint, aImgParams);
  }
}

void nsFilterInstance::BuildSourceImage(DrawTarget* aDest,
                                        imgDrawingParams& aImgParams,
                                        FilterNode* aFilter,
                                        FilterNode* aSource,
                                        const Rect& aSourceRect) {
  MOZ_ASSERT(mTargetFrame);

  nsIntRect neededRect = mSourceGraphic.mNeededBounds;
  if (neededRect.IsEmpty()) {
    return;
  }

  RefPtr<DrawTarget> offscreenDT;
  SurfaceFormat format = SurfaceFormat::B8G8R8A8;
  if (aDest->CanCreateSimilarDrawTarget(neededRect.Size(), format)) {
    offscreenDT = aDest->CreateSimilarDrawTargetForFilter(
        neededRect.Size(), format, aFilter, aSource, aSourceRect, Point(0, 0));
  }
  if (!offscreenDT || !offscreenDT->IsValid()) {
    return;
  }

  gfxRect r = FilterSpaceToUserSpace(ThebesRect(neededRect));
  r.RoundOut();
  nsIntRect dirty;
  if (!gfxUtils::GfxRectToIntRect(r, &dirty)) {
    return;
  }

  // SVG graphics paint to device space, so we need to set an initial device
  // space to filter space transform on the gfxContext that SourceGraphic
  // and SourceAlpha will paint to.
  //
  // (In theory it would be better to minimize error by having filtered SVG
  // graphics temporarily paint to user space when painting the sources and
  // only set a user space to filter space transform on the gfxContext
  // (since that would eliminate the transform multiplications from user
  // space to device space and back again). However, that would make the
  // code more complex while being hard to get right without introducing
  // subtle bugs, and in practice it probably makes no real difference.)
  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(offscreenDT);
  MOZ_ASSERT(ctx);  // already checked the draw target above
  gfxMatrix devPxToCssPxTM = nsSVGUtils::GetCSSPxToDevPxMatrix(mTargetFrame);
  DebugOnly<bool> invertible = devPxToCssPxTM.Invert();
  MOZ_ASSERT(invertible);
  ctx->SetMatrixDouble(devPxToCssPxTM * mPaintTransform *
                       gfxMatrix::Translation(-neededRect.TopLeft()));

  mPaintCallback->Paint(*ctx, mTargetFrame, mPaintTransform, &dirty,
                        aImgParams);

  mSourceGraphic.mSourceSurface = offscreenDT->Snapshot();
  mSourceGraphic.mSurfaceRect = neededRect;
}

void nsFilterInstance::Render(gfxContext* aCtx, imgDrawingParams& aImgParams,
                              float aOpacity) {
  MOZ_ASSERT(mTargetFrame, "Need a frame for rendering");

  if (mFilterDescription.mPrimitives.IsEmpty()) {
    // An filter without any primitive. Treat it as success and paint nothing.
    return;
  }

  nsIntRect filterRect =
      mPostFilterDirtyRegion.GetBounds().Intersect(OutputFilterSpaceBounds());
  if (filterRect.IsEmpty() || mPaintTransform.IsSingular()) {
    return;
  }

  gfxContextMatrixAutoSaveRestore autoSR(aCtx);
  aCtx->SetMatrix(
      aCtx->CurrentMatrix().PreTranslate(filterRect.x, filterRect.y));

  ComputeNeededBoxes();

  Rect renderRect = IntRectToRect(filterRect);
  RefPtr<DrawTarget> dt = aCtx->GetDrawTarget();

  BuildSourcePaints(aImgParams);
  RefPtr<FilterNode> sourceGraphic, fillPaint, strokePaint;
  if (mFillPaint.mSourceSurface) {
    fillPaint = FilterWrappers::ForSurface(dt, mFillPaint.mSourceSurface,
                                           mFillPaint.mSurfaceRect.TopLeft());
  }
  if (mStrokePaint.mSourceSurface) {
    strokePaint = FilterWrappers::ForSurface(
        dt, mStrokePaint.mSourceSurface, mStrokePaint.mSurfaceRect.TopLeft());
  }

  // We make the sourceGraphic filter but don't set its inputs until after so
  // that we can make the sourceGraphic size depend on the filter chain
  sourceGraphic = dt->CreateFilter(FilterType::TRANSFORM);
  if (sourceGraphic) {
    // Make sure we set the translation before calling BuildSourceImage
    // so that CreateSimilarDrawTargetForFilter works properly
    IntPoint offset = mSourceGraphic.mNeededBounds.TopLeft();
    sourceGraphic->SetAttribute(ATT_TRANSFORM_MATRIX,
                                Matrix::Translation(offset.x, offset.y));
  }

  RefPtr<FilterNode> resultFilter = FilterNodeGraphFromDescription(
      aCtx->GetDrawTarget(), mFilterDescription, renderRect, sourceGraphic,
      mSourceGraphic.mSurfaceRect, fillPaint, strokePaint, mInputImages);

  if (!resultFilter) {
    gfxWarning() << "Filter is NULL.";
    return;
  }

  BuildSourceImage(aCtx->GetDrawTarget(), aImgParams, resultFilter,
                   sourceGraphic, renderRect);
  if (sourceGraphic) {
    if (mSourceGraphic.mSourceSurface) {
      sourceGraphic->SetInput(IN_TRANSFORM_IN, mSourceGraphic.mSourceSurface);
    } else {
      RefPtr<FilterNode> clear = FilterWrappers::Clear(aCtx->GetDrawTarget());
      sourceGraphic->SetInput(IN_TRANSFORM_IN, clear);
    }
  }

  aCtx->GetDrawTarget()->DrawFilter(resultFilter, renderRect, Point(0, 0),
                                    DrawOptions(aOpacity));
}

nsRegion nsFilterInstance::ComputePostFilterDirtyRegion() {
  if (mPreFilterDirtyRegion.IsEmpty() ||
      mFilterDescription.mPrimitives.IsEmpty()) {
    return nsRegion();
  }

  nsIntRegion resultChangeRegion = FilterSupport::ComputeResultChangeRegion(
      mFilterDescription, mPreFilterDirtyRegion, nsIntRegion(), nsIntRegion());
  return FilterSpaceToFrameSpace(resultChangeRegion);
}

nsRect nsFilterInstance::ComputePostFilterExtents() {
  if (mFilterDescription.mPrimitives.IsEmpty()) {
    return nsRect();
  }

  nsIntRegion postFilterExtents = FilterSupport::ComputePostFilterExtents(
      mFilterDescription, mTargetBounds);
  return FilterSpaceToFrameSpace(postFilterExtents.GetBounds());
}

nsRect nsFilterInstance::ComputeSourceNeededRect() {
  ComputeNeededBoxes();
  return FilterSpaceToFrameSpace(mSourceGraphic.mNeededBounds);
}

nsIntRect nsFilterInstance::OutputFilterSpaceBounds() const {
  uint32_t numPrimitives = mFilterDescription.mPrimitives.Length();
  if (numPrimitives <= 0) {
    return nsIntRect();
  }

  return mFilterDescription.mPrimitives[numPrimitives - 1].PrimitiveSubregion();
}

nsIntRect nsFilterInstance::FrameSpaceToFilterSpace(const nsRect* aRect) const {
  nsIntRect rect = OutputFilterSpaceBounds();
  if (aRect) {
    if (aRect->IsEmpty()) {
      return nsIntRect();
    }
    gfxRect rectInCSSPx =
        nsLayoutUtils::RectToGfxRect(*aRect, AppUnitsPerCSSPixel());
    gfxRect rectInFilterSpace =
        mFrameSpaceInCSSPxToFilterSpaceTransform.TransformBounds(rectInCSSPx);
    rectInFilterSpace.RoundOut();
    nsIntRect intRect;
    if (gfxUtils::GfxRectToIntRect(rectInFilterSpace, &intRect)) {
      rect = intRect;
    }
  }
  return rect;
}

nsRect nsFilterInstance::FilterSpaceToFrameSpace(const nsIntRect& aRect) const {
  if (aRect.IsEmpty()) {
    return nsRect();
  }
  gfxRect r(aRect.x, aRect.y, aRect.width, aRect.height);
  r = mFilterSpaceToFrameSpaceInCSSPxTransform.TransformBounds(r);
  // nsLayoutUtils::RoundGfxRectToAppRect rounds out.
  return nsLayoutUtils::RoundGfxRectToAppRect(r, AppUnitsPerCSSPixel());
}

nsIntRegion nsFilterInstance::FrameSpaceToFilterSpace(
    const nsRegion* aRegion) const {
  if (!aRegion) {
    return OutputFilterSpaceBounds();
  }
  nsIntRegion result;
  for (auto iter = aRegion->RectIter(); !iter.Done(); iter.Next()) {
    // FrameSpaceToFilterSpace rounds out, so this works.
    nsRect rect = iter.Get();
    result.Or(result, FrameSpaceToFilterSpace(&rect));
  }
  return result;
}

nsRegion nsFilterInstance::FilterSpaceToFrameSpace(
    const nsIntRegion& aRegion) const {
  nsRegion result;
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    // FilterSpaceToFrameSpace rounds out, so this works.
    result.Or(result, FilterSpaceToFrameSpace(iter.Get()));
  }
  return result;
}

gfxMatrix nsFilterInstance::GetUserSpaceToFrameSpaceInCSSPxTransform() const {
  if (!mTargetFrame) {
    return gfxMatrix();
  }
  return gfxMatrix::Translation(
      -nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(mTargetFrame));
}
