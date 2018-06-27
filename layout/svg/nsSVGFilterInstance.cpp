/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGFilterInstance.h"

// Keep others in (case-insensitive) order:
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "nsSVGDisplayableFrame.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/IDTracker.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "SVGObserverUtils.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGUtils.h"
#include "SVGContentUtils.h"
#include "FilterSupport.h"
#include "gfx2DGlue.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::SVGUnitTypes_Binding;
using namespace mozilla::gfx;

nsSVGFilterInstance::nsSVGFilterInstance(const nsStyleFilter& aFilter,
                                         nsIFrame* aTargetFrame,
                                         nsIContent* aTargetContent,
                                         const UserSpaceMetrics& aMetrics,
                                         const gfxRect& aTargetBBox,
                                         const gfxSize& aUserSpaceToFilterSpaceScale) :
  mFilter(aFilter),
  mTargetContent(aTargetContent),
  mMetrics(aMetrics),
  mTargetBBox(aTargetBBox),
  mUserSpaceToFilterSpaceScale(aUserSpaceToFilterSpaceScale),
  mSourceAlphaAvailable(false),
  mInitialized(false) {

  // Get the filter frame.
  mFilterFrame = GetFilterFrame(aTargetFrame);
  if (!mFilterFrame) {
    return;
  }

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

bool
nsSVGFilterInstance::ComputeBounds()
{
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
  nsSVGLength2 XYWH[4];
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
    nsSVGUtils::GetRelativeRect(filterUnits, XYWH, mTargetBBox, mMetrics);

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

nsSVGFilterFrame*
nsSVGFilterInstance::GetFilterFrame(nsIFrame* aTargetFrame)
{
  if (mFilter.GetType() != NS_STYLE_FILTER_URL) {
    // The filter is not an SVG reference filter.
    return nullptr;
  }

  // Get the target element to use as a point of reference for looking up the
  // filter element.
  if (!mTargetContent) {
    return nullptr;
  }

  // aTargetFrame can be null if this filter belongs to a
  // CanvasRenderingContext2D.
  nsCOMPtr<nsIURI> url = aTargetFrame
    ? SVGObserverUtils::GetFilterURI(aTargetFrame, mFilter)
    : mFilter.GetURL()->ResolveLocalRef(mTargetContent);

  if (!url) {
    MOZ_ASSERT_UNREACHABLE("an nsStyleFilter of type URL should have a non-null URL");
    return nullptr;
  }

  // Look up the filter element by URL.
  IDTracker filterElement;
  bool watch = false;
  filterElement.Reset(mTargetContent, url, watch);
  Element* element = filterElement.get();
  if (!element) {
    // The URL points to no element.
    return nullptr;
  }

  // Get the frame of the filter element.
  nsIFrame* frame = element->GetPrimaryFrame();
  if (!frame || !frame->IsSVGFilterFrame()) {
    // The URL points to an element that's not an SVG filter element, or to an
    // element that hasn't had its frame constructed yet.
    return nullptr;
  }

  return static_cast<nsSVGFilterFrame*>(frame);
}

float
nsSVGFilterInstance::GetPrimitiveNumber(uint8_t aCtxType, float aValue) const
{
  nsSVGLength2 val;
  val.Init(aCtxType, 0xff, aValue,
           SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);

  float value;
  if (mPrimitiveUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    value = nsSVGUtils::ObjectSpace(mTargetBBox, &val);
  } else {
    value = nsSVGUtils::UserSpace(mMetrics, &val);
  }

  switch (aCtxType) {
  case SVGContentUtils::X:
    return value * mUserSpaceToFilterSpaceScale.width;
  case SVGContentUtils::Y:
    return value * mUserSpaceToFilterSpaceScale.height;
  case SVGContentUtils::XY:
  default:
    return value * SVGContentUtils::ComputeNormalizedHypotenuse(
                     mUserSpaceToFilterSpaceScale.width,
                     mUserSpaceToFilterSpaceScale.height);
  }
}

Point3D
nsSVGFilterInstance::ConvertLocation(const Point3D& aPoint) const
{
  nsSVGLength2 val[4];
  val[0].Init(SVGContentUtils::X, 0xff, aPoint.x,
              SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);
  val[1].Init(SVGContentUtils::Y, 0xff, aPoint.y,
              SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);
  // Dummy width/height values
  val[2].Init(SVGContentUtils::X, 0xff, 0,
              SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);
  val[3].Init(SVGContentUtils::Y, 0xff, 0,
              SVGLength_Binding::SVG_LENGTHTYPE_NUMBER);

  gfxRect feArea = nsSVGUtils::GetRelativeRect(mPrimitiveUnits,
    val, mTargetBBox, mMetrics);
  gfxRect r = UserSpaceToFilterSpace(feArea);
  return Point3D(r.x, r.y, GetPrimitiveNumber(SVGContentUtils::XY, aPoint.z));
}

gfxRect
nsSVGFilterInstance::UserSpaceToFilterSpace(const gfxRect& aUserSpaceRect) const
{
  gfxRect filterSpaceRect = aUserSpaceRect;
  filterSpaceRect.Scale(mUserSpaceToFilterSpaceScale.width,
                        mUserSpaceToFilterSpaceScale.height);
  return filterSpaceRect;
}

IntRect
nsSVGFilterInstance::ComputeFilterPrimitiveSubregion(nsSVGFE* aFilterElement,
                                                     const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
                                                     const nsTArray<int32_t>& aInputIndices)
{
  nsSVGFE* fE = aFilterElement;

  IntRect defaultFilterSubregion(0,0,0,0);
  if (fE->SubregionIsUnionOfRegions()) {
    for (uint32_t i = 0; i < aInputIndices.Length(); ++i) {
      int32_t inputIndex = aInputIndices[i];
      bool isStandardInput = inputIndex < 0 || inputIndex == mSourceGraphicIndex;
      IntRect inputSubregion = isStandardInput ?
        mFilterSpaceBounds :
        aPrimitiveDescrs[inputIndex].PrimitiveSubregion();

      defaultFilterSubregion = defaultFilterSubregion.Union(inputSubregion);
    }
  } else {
    defaultFilterSubregion = mFilterSpaceBounds;
  }

  gfxRect feArea = nsSVGUtils::GetRelativeRect(mPrimitiveUnits,
    &fE->mLengthAttributes[nsSVGFE::ATTR_X], mTargetBBox, mMetrics);
  Rect region = ToRect(UserSpaceToFilterSpace(feArea));

  if (!fE->mLengthAttributes[nsSVGFE::ATTR_X].IsExplicitlySet())
    region.x = defaultFilterSubregion.X();
  if (!fE->mLengthAttributes[nsSVGFE::ATTR_Y].IsExplicitlySet())
    region.y = defaultFilterSubregion.Y();
  if (!fE->mLengthAttributes[nsSVGFE::ATTR_WIDTH].IsExplicitlySet())
    region.width = defaultFilterSubregion.Width();
  if (!fE->mLengthAttributes[nsSVGFE::ATTR_HEIGHT].IsExplicitlySet())
    region.height = defaultFilterSubregion.Height();

  // We currently require filter primitive subregions to be pixel-aligned.
  // Following the spec, any pixel partially in the region is included
  // in the region.
  region.RoundOut();
  return RoundedToInt(region);
}

void
nsSVGFilterInstance::GetInputsAreTainted(const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
                                         const nsTArray<int32_t>& aInputIndices,
                                         bool aFilterInputIsTainted,
                                         nsTArray<bool>& aOutInputsAreTainted)
{
  for (uint32_t i = 0; i < aInputIndices.Length(); i++) {
    int32_t inputIndex = aInputIndices[i];
    if (inputIndex < 0) {
      aOutInputsAreTainted.AppendElement(aFilterInputIsTainted);
    } else {
      aOutInputsAreTainted.AppendElement(aPrimitiveDescrs[inputIndex].IsTainted());
    }
  }
}

static int32_t
GetLastResultIndex(const nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs)
{
  uint32_t numPrimitiveDescrs = aPrimitiveDescrs.Length();
  return !numPrimitiveDescrs ?
    FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic :
    numPrimitiveDescrs - 1;
}

int32_t
nsSVGFilterInstance::GetOrCreateSourceAlphaIndex(nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs)
{
  // If the SourceAlpha index has already been determined or created for this
  // SVG filter, just return it.
  if (mSourceAlphaAvailable)
    return mSourceAlphaIndex;

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
  FilterPrimitiveDescription descr(PrimitiveType::ToAlpha);
  descr.SetInputPrimitive(0, mSourceGraphicIndex);

  const FilterPrimitiveDescription& sourcePrimitiveDescr =
    aPrimitiveDescrs[mSourceGraphicIndex];
  descr.SetPrimitiveSubregion(sourcePrimitiveDescr.PrimitiveSubregion());
  descr.SetIsTainted(sourcePrimitiveDescr.IsTainted());

  ColorSpace colorSpace = sourcePrimitiveDescr.OutputColorSpace();
  descr.SetInputColorSpace(0, colorSpace);
  descr.SetOutputColorSpace(colorSpace);

  aPrimitiveDescrs.AppendElement(descr);
  mSourceAlphaIndex = aPrimitiveDescrs.Length() - 1;
  mSourceAlphaAvailable = true;
  return mSourceAlphaIndex;
}

nsresult
nsSVGFilterInstance::GetSourceIndices(nsSVGFE* aPrimitiveElement,
                                      nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
                                      const nsDataHashtable<nsStringHashKey, int32_t>& aImageTable,
                                      nsTArray<int32_t>& aSourceIndices)
{
  AutoTArray<nsSVGStringInfo,2> sources;
  aPrimitiveElement->GetSourceImageNames(sources);

  for (uint32_t j = 0; j < sources.Length(); j++) {
    nsAutoString str;
    sources[j].mString->GetAnimValue(str, sources[j].mElement);

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
      if (!inputExists)
        return NS_ERROR_FAILURE;
    }

    aSourceIndices.AppendElement(sourceIndex);
  }
  return NS_OK;
}

nsresult
nsSVGFilterInstance::BuildPrimitives(nsTArray<FilterPrimitiveDescription>& aPrimitiveDescrs,
                                     nsTArray<RefPtr<SourceSurface>>& aInputImages,
                                     bool aInputIsTainted)
{
  mSourceGraphicIndex = GetLastResultIndex(aPrimitiveDescrs);

  // Clip previous filter's output to this filter's filter region.
  if (mSourceGraphicIndex >= 0) {
    FilterPrimitiveDescription& sourceDescr = aPrimitiveDescrs[mSourceGraphicIndex];
    sourceDescr.SetPrimitiveSubregion(sourceDescr.PrimitiveSubregion().Intersect(mFilterSpaceBounds));
  }

  // Get the filter primitive elements.
  nsTArray<RefPtr<nsSVGFE> > primitives;
  for (nsIContent* child = mFilterElement->nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    RefPtr<nsSVGFE> primitive;
    CallQueryInterface(child, (nsSVGFE**)getter_AddRefs(primitive));
    if (primitive) {
      primitives.AppendElement(primitive);
    }
  }

  // Maps source image name to source index.
  nsDataHashtable<nsStringHashKey, int32_t> imageTable(8);

  // The principal that we check principals of any loaded images against.
  nsCOMPtr<nsIPrincipal> principal = mTargetContent->NodePrincipal();

  for (uint32_t primitiveElementIndex = 0;
       primitiveElementIndex < primitives.Length();
       ++primitiveElementIndex) {
    nsSVGFE* filter = primitives[primitiveElementIndex];

    AutoTArray<int32_t,2> sourceIndices;
    nsresult rv = GetSourceIndices(filter, aPrimitiveDescrs, imageTable, sourceIndices);
    if (NS_FAILED(rv)) {
      return rv;
    }

    IntRect primitiveSubregion =
      ComputeFilterPrimitiveSubregion(filter, aPrimitiveDescrs, sourceIndices);

    nsTArray<bool> sourcesAreTainted;
    GetInputsAreTainted(aPrimitiveDescrs, sourceIndices, aInputIsTainted, sourcesAreTainted);

    FilterPrimitiveDescription descr =
      filter->GetPrimitiveDescription(this, primitiveSubregion, sourcesAreTainted, aInputImages);

    descr.SetIsTainted(filter->OutputIsTainted(sourcesAreTainted, principal));
    descr.SetFilterSpaceBounds(mFilterSpaceBounds);
    descr.SetPrimitiveSubregion(primitiveSubregion.Intersect(descr.FilterSpaceBounds()));

    for (uint32_t i = 0; i < sourceIndices.Length(); i++) {
      int32_t inputIndex = sourceIndices[i];
      descr.SetInputPrimitive(i, inputIndex);

      ColorSpace inputColorSpace = inputIndex >= 0
        ? aPrimitiveDescrs[inputIndex].OutputColorSpace()
        : ColorSpace(ColorSpace::SRGB);

      ColorSpace desiredInputColorSpace = filter->GetInputColorSpace(i, inputColorSpace);
      descr.SetInputColorSpace(i, desiredInputColorSpace);
      if (i == 0) {
        // the output color space is whatever in1 is if there is an in1
        descr.SetOutputColorSpace(desiredInputColorSpace);
      }
    }

    if (sourceIndices.Length() == 0) {
      descr.SetOutputColorSpace(filter->GetOutputColorSpace());
    }

    aPrimitiveDescrs.AppendElement(descr);
    uint32_t primitiveDescrIndex = aPrimitiveDescrs.Length() - 1;

    nsAutoString str;
    filter->GetResultImageName().GetAnimValue(str, filter);
    imageTable.Put(str, primitiveDescrIndex);
  }

  return NS_OK;
}
