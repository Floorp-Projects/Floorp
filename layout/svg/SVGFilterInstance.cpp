/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGFilterInstance.h"

// Keep others in (case-insensitive) order:
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "SVGFilterFrame.h"
#include "FilterSupport.h"
#include "gfx2DGlue.h"

using namespace mozilla::dom;
using namespace mozilla::dom::SVGUnitTypes_Binding;
using namespace mozilla::gfx;

namespace mozilla {

SVGFilterInstance::SVGFilterInstance(
    const StyleFilter& aFilter, SVGFilterFrame* aFilterFrame,
    nsIContent* aTargetContent, const UserSpaceMetrics& aMetrics,
    const gfxRect& aTargetBBox,
    const MatrixScalesDouble& aUserSpaceToFilterSpaceScale)
    : mFilter(aFilter),
      mTargetContent(aTargetContent),
      mMetrics(aMetrics),
      mFilterFrame(aFilterFrame),
      mTargetBBox(aTargetBBox),
      mUserSpaceToFilterSpaceScale(aUserSpaceToFilterSpaceScale),
      mSourceAlphaAvailable(false),
      mInitialized(false) {
  // Get the filter element.
  mFilterElement = mFilterFrame->GetFilterContent();
  if (!mFilterElement) {
    MOZ_ASSERT_UNREACHABLE("filter frame should have a related element");
    return;
  }

  mPrimitiveUnits =
      mFilterFrame->GetEnumValue(SVGFilterElement::PRIMITIVEUNITS);

  if (!ComputeBounds()) {
    return;
  }

  mInitialized = true;
}

bool SVGFilterInstance::ComputeBounds() {
  // XXX if filterUnits is set (or has defaulted) to objectBoundingBox, we
  // should send a warning to the error console if the author has used lengths
  // with units. This is a common mistake and can result in the filter region
  // being *massive* below (because we ignore the units and interpret the number
  // as a factor of the bbox width/height). We should also send a warning if the
  // user uses a number without units (a future SVG spec should really
  // deprecate that, since it's too confusing for a bare number to be sometimes
  // interpreted as a fraction of the bounding box and sometimes as user-space
  // units). So really only percentage values should be used in this case.

  // Set the user space bounds (i.e. the filter region in user space).
  SVGAnimatedLength XYWH[4];
  static_assert(sizeof(mFilterElement->mLengthAttributes) == sizeof(XYWH),
                "XYWH size incorrect");
  memcpy(XYWH, mFilterElement->mLengthAttributes,
         sizeof(mFilterElement->mLengthAttributes));
  XYWH[0] = *mFilterFrame->GetLengthValue(SVGFilterElement::ATTR_X);
  XYWH[1] = *mFilterFrame->GetLengthValue(SVGFilterElement::ATTR_Y);
  XYWH[2] = *mFilterFrame->GetLengthValue(SVGFilterElement::ATTR_WIDTH);
  XYWH[3] = *mFilterFrame->GetLengthValue(SVGFilterElement::ATTR_HEIGHT);
  uint16_t filterUnits =
      mFilterFrame->GetEnumValue(SVGFilterElement::FILTERUNITS);
  gfxRect userSpaceBounds =
      SVGUtils::GetRelativeRect(filterUnits, XYWH, mTargetBBox, mMetrics);

  // Transform the user space bounds to filter space, so we
  // can align them with the pixel boundaries of the offscreen surface.
  // The offscreen surface has the same scale as filter space.
  gfxRect filterSpaceBounds = UserSpaceToFilterSpace(userSpaceBounds);
  filterSpaceBounds.RoundOut();
  if (filterSpaceBounds.width <= 0 || filterSpaceBounds.height <= 0) {
    // 0 disables rendering, < 0 is error. dispatch error console warning
    // or error as appropriate.
    return false;
  }

  // Set the filter space bounds.
  if (!gfxUtils::GfxRectToIntRect(filterSpaceBounds, &mFilterSpaceBounds)) {
    // The filter region is way too big if there is float -> int overflow.
    return false;
  }

  return true;
}

float SVGFilterInstance::GetPrimitiveNumber(uint8_t aCtxType,
                                            float aValue) const {
  SVGAnimatedLength val;
  val.Init(aCtxType, 0xff, aValue, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);

  float value;
  if (mPrimitiveUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    value = SVGUtils::ObjectSpace(mTargetBBox, &val);
  } else {
    value = SVGUtils::UserSpace(mMetrics, &val);
  }

  switch (aCtxType) {
    case SVGContentUtils::X:
      return value * static_cast<float>(mUserSpaceToFilterSpaceScale.xScale);
    case SVGContentUtils::Y:
      return value * static_cast<float>(mUserSpaceToFilterSpaceScale.yScale);
    case SVGContentUtils::XY:
    default:
      return value * SVGContentUtils::ComputeNormalizedHypotenuse(
                         mUserSpaceToFilterSpaceScale.xScale,
                         mUserSpaceToFilterSpaceScale.yScale);
  }
}

Point3D SVGFilterInstance::ConvertLocation(const Point3D& aPoint) const {
  SVGAnimatedLength val[4];
  val[0].Init(SVGContentUtils::X, 0xff, aPoint.x,
              SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);
  val[1].Init(SVGContentUtils::Y, 0xff, aPoint.y,
              SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);
  // Dummy width/height values
  val[2].Init(SVGContentUtils::X, 0xff, 0,
              SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);
  val[3].Init(SVGContentUtils::Y, 0xff, 0,
              SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);

  gfxRect feArea =
      SVGUtils::GetRelativeRect(mPrimitiveUnits, val, mTargetBBox, mMetrics);
  gfxRect r = UserSpaceToFilterSpace(feArea);
  return Point3D(r.x, r.y, GetPrimitiveNumber(SVGContentUtils::XY, aPoint.z));
}

gfxRect SVGFilterInstance::UserSpaceToFilterSpace(
    const gfxRect& aUserSpaceRect) const {
  gfxRect filterSpaceRect = aUserSpaceRect;
  filterSpaceRect.Scale(mUserSpaceToFilterSpaceScale);
  return filterSpaceRect;
}

IntRect SVGFilterInstance::ComputeFilterPrimitiveSubregion(
    SVGFilterPrimitiveElement* aFilterElement,
    const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
    const nsTArray<int32_t>& aInputIndices) {
  SVGFilterPrimitiveElement* fE = aFilterElement;

  IntRect defaultFilterSubregion(0, 0, 0, 0);
  if (fE->SubregionIsUnionOfRegions()) {
    for (const auto& inputIndex : aInputIndices) {
      bool isStandardInput =
          inputIndex < 0 || inputIndex == mSourceGraphicIndex;
      IntRect inputSubregion =
          isStandardInput ? mFilterSpaceBounds
                          : aPrimitiveDescrs[inputIndex].PrimitiveSubregion();

      defaultFilterSubregion = defaultFilterSubregion.Union(inputSubregion);
    }
  } else {
    defaultFilterSubregion = mFilterSpaceBounds;
  }

  gfxRect feArea = SVGUtils::GetRelativeRect(
      mPrimitiveUnits,
      &fE->mLengthAttributes[SVGFilterPrimitiveElement::ATTR_X], mTargetBBox,
      mMetrics);
  Rect region = ToRect(UserSpaceToFilterSpace(feArea));

  if (!fE->mLengthAttributes[SVGFilterPrimitiveElement::ATTR_X]
           .IsExplicitlySet())
    region.x = defaultFilterSubregion.X();
  if (!fE->mLengthAttributes[SVGFilterPrimitiveElement::ATTR_Y]
           .IsExplicitlySet())
    region.y = defaultFilterSubregion.Y();
  if (!fE->mLengthAttributes[SVGFilterPrimitiveElement::ATTR_WIDTH]
           .IsExplicitlySet())
    region.width = defaultFilterSubregion.Width();
  if (!fE->mLengthAttributes[SVGFilterPrimitiveElement::ATTR_HEIGHT]
           .IsExplicitlySet())
    region.height = defaultFilterSubregion.Height();

  // We currently require filter primitive subregions to be pixel-aligned.
  // Following the spec, any pixel partially in the region is included
  // in the region.
  region.RoundOut();
  return RoundedToInt(region);
}

void SVGFilterInstance::GetInputsAreTainted(
    const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
    const nsTArray<int32_t>& aInputIndices, bool aFilterInputIsTainted,
    nsTArray<bool>& aOutInputsAreTainted) {
  for (const auto& inputIndex : aInputIndices) {
    if (inputIndex < 0) {
      aOutInputsAreTainted.AppendElement(aFilterInputIsTainted);
    } else {
      aOutInputsAreTainted.AppendElement(
          aPrimitiveDescrs[inputIndex].IsTainted());
    }
  }
}

static int32_t GetLastResultIndex(
    const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs) {
  uint32_t numPrimitiveDescrs = aPrimitiveDescrs.Length();
  return !numPrimitiveDescrs
             ? FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic
             : numPrimitiveDescrs - 1;
}

int32_t SVGFilterInstance::GetOrCreateSourceAlphaIndex(
    nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs) {
  // If the SourceAlpha index has already been determined or created for this
  // SVG filter, just return it.
  if (mSourceAlphaAvailable) {
    return mSourceAlphaIndex;
  }

  // If this is the first filter in the chain, we can just use the
  // kPrimitiveIndexSourceAlpha keyword to refer to the SourceAlpha of the
  // original image.
  if (mSourceGraphicIndex < 0) {
    mSourceAlphaIndex = FilterPrimitiveDescription::kPrimitiveIndexSourceAlpha;
    mSourceAlphaAvailable = true;
    return mSourceAlphaIndex;
  }

  // Otherwise, create a primitive description to turn the previous filter's
  // output into a SourceAlpha input.
  FilterPrimitiveDescription descr(AsVariant(ToAlphaAttributes()));
  descr.SetInputPrimitive(0, mSourceGraphicIndex);

  const FilterPrimitiveDescription& sourcePrimitiveDescr =
      aPrimitiveDescrs[mSourceGraphicIndex];
  descr.SetPrimitiveSubregion(sourcePrimitiveDescr.PrimitiveSubregion());
  descr.SetIsTainted(sourcePrimitiveDescr.IsTainted());

  ColorSpace colorSpace = sourcePrimitiveDescr.OutputColorSpace();
  descr.SetInputColorSpace(0, colorSpace);
  descr.SetOutputColorSpace(colorSpace);

  aPrimitiveDescrs.AppendElement(std::move(descr));
  mSourceAlphaIndex = aPrimitiveDescrs.Length() - 1;
  mSourceAlphaAvailable = true;
  return mSourceAlphaIndex;
}

nsresult SVGFilterInstance::GetSourceIndices(
    SVGFilterPrimitiveElement* aPrimitiveElement,
    nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
    const nsTHashMap<nsStringHashKey, int32_t>& aImageTable,
    nsTArray<int32_t>& aSourceIndices) {
  AutoTArray<SVGStringInfo, 2> sources;
  aPrimitiveElement->GetSourceImageNames(sources);

  for (const auto& source : sources) {
    nsAutoString str;
    source.mString->GetAnimValue(str, source.mElement);

    int32_t sourceIndex = 0;
    if (str.EqualsLiteral("SourceGraphic")) {
      sourceIndex = mSourceGraphicIndex;
    } else if (str.EqualsLiteral("SourceAlpha")) {
      sourceIndex = GetOrCreateSourceAlphaIndex(aPrimitiveDescrs);
    } else if (str.EqualsLiteral("FillPaint")) {
      sourceIndex = FilterPrimitiveDescription::kPrimitiveIndexFillPaint;
    } else if (str.EqualsLiteral("StrokePaint")) {
      sourceIndex = FilterPrimitiveDescription::kPrimitiveIndexStrokePaint;
    } else if (str.EqualsLiteral("BackgroundImage") ||
               str.EqualsLiteral("BackgroundAlpha")) {
      return NS_ERROR_NOT_IMPLEMENTED;
    } else if (str.EqualsLiteral("")) {
      sourceIndex = GetLastResultIndex(aPrimitiveDescrs);
    } else {
      bool inputExists = aImageTable.Get(str, &sourceIndex);
      if (!inputExists) {
        sourceIndex = GetLastResultIndex(aPrimitiveDescrs);
      }
    }

    aSourceIndices.AppendElement(sourceIndex);
  }
  return NS_OK;
}

nsresult SVGFilterInstance::BuildPrimitives(
    nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
    nsTArray<RefPtr<SourceSurface>>& aInputImages, bool aInputIsTainted) {
  mSourceGraphicIndex = GetLastResultIndex(aPrimitiveDescrs);

  // Clip previous filter's output to this filter's filter region.
  if (mSourceGraphicIndex >= 0) {
    FilterPrimitiveDescription& sourceDescr =
        aPrimitiveDescrs[mSourceGraphicIndex];
    sourceDescr.SetPrimitiveSubregion(
        sourceDescr.PrimitiveSubregion().Intersect(mFilterSpaceBounds));
  }

  // Get the filter primitive elements.
  AutoTArray<RefPtr<SVGFilterPrimitiveElement>, 8> primitives;
  for (nsIContent* child = mFilterElement->nsINode::GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (auto* primitive = SVGFilterPrimitiveElement::FromNode(child)) {
      primitives.AppendElement(primitive);
    }
  }

  // Maps source image name to source index.
  nsTHashMap<nsStringHashKey, int32_t> imageTable(8);

  // The principal that we check principals of any loaded images against.
  nsCOMPtr<nsIPrincipal> principal = mTargetContent->NodePrincipal();

  for (uint32_t primitiveElementIndex = 0;
       primitiveElementIndex < primitives.Length(); ++primitiveElementIndex) {
    SVGFilterPrimitiveElement* filter = primitives[primitiveElementIndex];

    AutoTArray<int32_t, 2> sourceIndices;
    nsresult rv =
        GetSourceIndices(filter, aPrimitiveDescrs, imageTable, sourceIndices);
    if (NS_FAILED(rv)) {
      return rv;
    }

    IntRect primitiveSubregion = ComputeFilterPrimitiveSubregion(
        filter, aPrimitiveDescrs, sourceIndices);

    AutoTArray<bool, 8> sourcesAreTainted;
    GetInputsAreTainted(aPrimitiveDescrs, sourceIndices, aInputIsTainted,
                        sourcesAreTainted);

    FilterPrimitiveDescription descr = filter->GetPrimitiveDescription(
        this, primitiveSubregion, sourcesAreTainted, aInputImages);

    descr.SetIsTainted(filter->OutputIsTainted(sourcesAreTainted, principal));
    descr.SetFilterSpaceBounds(mFilterSpaceBounds);
    descr.SetPrimitiveSubregion(
        primitiveSubregion.Intersect(descr.FilterSpaceBounds()));

    for (uint32_t i = 0; i < sourceIndices.Length(); i++) {
      int32_t inputIndex = sourceIndices[i];
      descr.SetInputPrimitive(i, inputIndex);

      ColorSpace inputColorSpace =
          inputIndex >= 0 ? aPrimitiveDescrs[inputIndex].OutputColorSpace()
                          : ColorSpace(ColorSpace::SRGB);

      ColorSpace desiredInputColorSpace =
          filter->GetInputColorSpace(i, inputColorSpace);
      descr.SetInputColorSpace(i, desiredInputColorSpace);
      if (i == 0) {
        // the output color space is whatever in1 is if there is an in1
        descr.SetOutputColorSpace(desiredInputColorSpace);
      }
    }

    if (sourceIndices.Length() == 0) {
      descr.SetOutputColorSpace(filter->GetOutputColorSpace());
    }

    aPrimitiveDescrs.AppendElement(std::move(descr));
    uint32_t primitiveDescrIndex = aPrimitiveDescrs.Length() - 1;

    nsAutoString str;
    filter->GetResultImageName().GetAnimValue(str, filter);
    imageTable.InsertOrUpdate(str, primitiveDescrIndex);
  }

  return NS_OK;
}

}  // namespace mozilla
