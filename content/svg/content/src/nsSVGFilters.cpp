/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGElement.h"
#include "nsGkAtoms.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"
#include "nsSVGInteger.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGBoolean.h"
#include "nsCOMPtr.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGEnum.h"
#include "SVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "nsSVGFilters.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "nsStyleContext.h"
#include "nsIFrame.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "imgIContainer.h"
#include "nsNetUtil.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "nsSVGString.h"
#include "gfxUtils.h"
#include "SVGContentUtils.h"
#include <algorithm>
#include "nsContentUtils.h"
#include "mozilla/dom/SVGAnimatedLength.h"
#include "mozilla/dom/SVGComponentTransferFunctionElement.h"
#include "mozilla/dom/SVGFEDistantLightElement.h"
#include "mozilla/dom/SVGFEFuncAElementBinding.h"
#include "mozilla/dom/SVGFEFuncBElementBinding.h"
#include "mozilla/dom/SVGFEFuncGElementBinding.h"
#include "mozilla/dom/SVGFEFuncRElementBinding.h"
#include "mozilla/dom/SVGFEPointLightElement.h"
#include "mozilla/dom/SVGFESpotLightElement.h"

#if defined(XP_WIN)
// Prevent Windows redefining LoadImage
#undef LoadImage
#endif

using namespace mozilla;
using namespace mozilla::dom;

static const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN  = 0;
static const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY = 1;
static const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_TABLE    = 2;
static const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE = 3;
static const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_LINEAR   = 4;
static const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_GAMMA    = 5;

void
CopyDataRect(uint8_t *aDest, const uint8_t *aSrc, uint32_t aStride,
             const nsIntRect& aDataRect)
{
  for (int32_t y = aDataRect.y; y < aDataRect.YMost(); y++) {
    memcpy(aDest + y * aStride + 4 * aDataRect.x,
           aSrc + y * aStride + 4 * aDataRect.x,
           4 * aDataRect.width);
  }
}

static void
CopyAndScaleDeviceOffset(const gfxImageSurface *aImage, gfxImageSurface *aResult,
                         gfxFloat kernelX, gfxFloat kernelY)
{
  gfxPoint deviceOffset = aImage->GetDeviceOffset();
  deviceOffset.x /= kernelX;
  deviceOffset.y /= kernelY;
  aResult->SetDeviceOffset(deviceOffset);
}

//--------------------Filter Element Base Class-----------------------

nsSVGElement::LengthInfo nsSVGFE::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
  { &nsGkAtoms::width, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::height, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFE,nsSVGFEBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFE,nsSVGFEBase)

NS_DEFINE_STATIC_IID_ACCESSOR(nsSVGFE, NS_SVG_FE_CID)

NS_INTERFACE_MAP_BEGIN(nsSVGFE)
   // nsISupports is an ambiguous base of nsSVGFE so we have to work
   // around that
   if ( aIID.Equals(NS_GET_IID(nsSVGFE)) )
     foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
   else
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFE::ScaleInfo
nsSVGFE::SetupScalingFilter(nsSVGFilterInstance *aInstance,
                            const Image *aSource, const Image *aTarget,
                            const nsIntRect& aDataRect,
                            nsSVGNumberPair *aKernelUnitLength)
{
  ScaleInfo result;
  result.mRescaling = aKernelUnitLength->IsExplicitlySet();
  if (!result.mRescaling) {
    result.mSource = aSource->mImage;
    result.mTarget = aTarget->mImage;
    result.mDataRect = aDataRect;
    return result;
  }

  gfxFloat kernelX = aInstance->GetPrimitiveNumber(SVGContentUtils::X,
                                                   aKernelUnitLength,
                                                   nsSVGNumberPair::eFirst);
  gfxFloat kernelY = aInstance->GetPrimitiveNumber(SVGContentUtils::Y,
                                                   aKernelUnitLength,
                                                   nsSVGNumberPair::eSecond);
  if (kernelX <= 0 || kernelY <= 0)
    return result;

  bool overflow = false;
  gfxIntSize scaledSize =
    nsSVGUtils::ConvertToSurfaceSize(gfxSize(aTarget->mImage->Width() / kernelX,
                                             aTarget->mImage->Height() / kernelY),
                                     &overflow);
  // If the requested size based on the kernel unit is too big, we
  // need to bail because the effect is pixel size dependent.  Also
  // need to check if we ended up with a negative size (arithmetic
  // overflow) or zero size (large kernel unit)
  if (overflow || scaledSize.width <= 0 || scaledSize.height <= 0)
    return result;

  gfxRect r(aDataRect.x, aDataRect.y, aDataRect.width, aDataRect.height);
  r.Scale(1 / kernelX, 1 / kernelY);
  r.RoundOut();
  if (!gfxUtils::GfxRectToIntRect(r, &result.mDataRect))
    return result;

  // Rounding in the code above can mean that result.mDataRect is not contained
  // within the bounds of the surfaces that we're about to create. We must
  // clamp to these bounds to prevent out-of-bounds reads and writes:
  result.mDataRect.IntersectRect(result.mDataRect,
                                 nsIntRect(nsIntPoint(), scaledSize));

  result.mSource = new gfxImageSurface(scaledSize,
                                       gfxASurface::ImageFormatARGB32);
  result.mTarget = new gfxImageSurface(scaledSize,
                                       gfxASurface::ImageFormatARGB32);
  if (!result.mSource || result.mSource->CairoStatus() ||
      !result.mTarget || result.mTarget->CairoStatus()) {
    result.mSource = nullptr;
    result.mTarget = nullptr;
    return result;
  }

  CopyAndScaleDeviceOffset(aSource->mImage, result.mSource, kernelX, kernelY);
  CopyAndScaleDeviceOffset(aTarget->mImage, result.mTarget, kernelX, kernelY);

  result.mRealTarget = aTarget->mImage;

  gfxContext ctx(result.mSource);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.Scale(double(scaledSize.width) / aTarget->mImage->Width(),
            double(scaledSize.height) / aTarget->mImage->Height());
  ctx.SetSource(aSource->mImage);
  ctx.Paint();

  // mTarget was already cleared when it was created

  return result;
}

void
nsSVGFE::FinishScalingFilter(ScaleInfo *aScaleInfo)
{
  if (!aScaleInfo->mRescaling)
    return;

  gfxIntSize scaledSize = aScaleInfo->mTarget->GetSize();

  gfxContext ctx(aScaleInfo->mRealTarget);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.Scale(double(aScaleInfo->mRealTarget->Width()) / scaledSize.width,
            double(aScaleInfo->mRealTarget->Height()) / scaledSize.height);
  ctx.SetSource(aScaleInfo->mTarget);
  ctx.Paint();
}

nsIntRect
nsSVGFE::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
                           const nsSVGFilterInstance& aInstance)
{
  nsIntRect r;
  for (uint32_t i = 0; i < aSourceBBoxes.Length(); ++i) {
    r.UnionRect(r, aSourceBBoxes[i]);
  }
  return r;
}

void
nsSVGFE::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
                                   nsTArray<nsIntRect>& aSourceBBoxes,
                                   const nsSVGFilterInstance& aInstance)
{
  for (uint32_t i = 0; i < aSourceBBoxes.Length(); ++i) {
    aSourceBBoxes[i] = aTargetBBox;
  }
}

nsIntRect
nsSVGFE::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                           const nsSVGFilterInstance& aInstance)
{
  nsIntRect r;
  for (uint32_t i = 0; i < aSourceChangeBoxes.Length(); ++i) {
    r.UnionRect(r, aSourceChangeBoxes[i]);
  }
  return r;
}

void
nsSVGFE::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
}

bool
nsSVGFE::AttributeAffectsRendering(int32_t aNameSpaceID,
                                   nsIAtom* aAttribute) const
{
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::x ||
          aAttribute == nsGkAtoms::y ||
          aAttribute == nsGkAtoms::width ||
          aAttribute == nsGkAtoms::height ||
          aAttribute == nsGkAtoms::result);
}

already_AddRefed<SVGAnimatedLength>
nsSVGFE::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
nsSVGFE::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
nsSVGFE::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
nsSVGFE::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedString>
nsSVGFE::Result()
{
  return GetResultImageName().ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGFE::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFiltersMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGFEBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
nsSVGFE::HasValidDimensions() const
{
  return (!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
           mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
           mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

nsSVGElement::LengthAttributesInfo
nsSVGFE::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

namespace mozilla {
namespace dom {

nsSVGElement::NumberListInfo SVGComponentTransferFunctionElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::tableValues }
};

nsSVGElement::NumberInfo SVGComponentTransferFunctionElement::sNumberInfo[5] =
{
  { &nsGkAtoms::slope,     1, false },
  { &nsGkAtoms::intercept, 0, false },
  { &nsGkAtoms::amplitude, 1, false },
  { &nsGkAtoms::exponent,  1, false },
  { &nsGkAtoms::offset,    0, false }
};

nsSVGEnumMapping SVGComponentTransferFunctionElement::sTypeMap[] = {
  {&nsGkAtoms::identity,
   SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY},
  {&nsGkAtoms::table,
   SVG_FECOMPONENTTRANSFER_TYPE_TABLE},
  {&nsGkAtoms::discrete,
   SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE},
  {&nsGkAtoms::linear,
   SVG_FECOMPONENTTRANSFER_TYPE_LINEAR},
  {&nsGkAtoms::gamma,
   SVG_FECOMPONENTTRANSFER_TYPE_GAMMA},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGComponentTransferFunctionElement::sEnumInfo[1] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY
  }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGComponentTransferFunctionElement,SVGComponentTransferFunctionElementBase)
NS_IMPL_RELEASE_INHERITED(SVGComponentTransferFunctionElement,SVGComponentTransferFunctionElementBase)

NS_DEFINE_STATIC_IID_ACCESSOR(SVGComponentTransferFunctionElement, NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID)

NS_INTERFACE_MAP_BEGIN(SVGComponentTransferFunctionElement)
   // nsISupports is an ambiguous base of nsSVGFE so we have to work
   // around that
   if ( aIID.Equals(NS_GET_IID(SVGComponentTransferFunctionElement)) )
     foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
   else
NS_INTERFACE_MAP_END_INHERITING(SVGComponentTransferFunctionElementBase)


//----------------------------------------------------------------------
// nsFEUnstyledElement methods

bool
SVGComponentTransferFunctionElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                               nsIAtom* aAttribute) const
{
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::tableValues ||
          aAttribute == nsGkAtoms::slope ||
          aAttribute == nsGkAtoms::intercept ||
          aAttribute == nsGkAtoms::amplitude ||
          aAttribute == nsGkAtoms::exponent ||
          aAttribute == nsGkAtoms::offset ||
          aAttribute == nsGkAtoms::type);
}

//----------------------------------------------------------------------

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGComponentTransferFunctionElement::Type()
{
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedNumberList>
SVGComponentTransferFunctionElement::TableValues()
{
  return DOMSVGAnimatedNumberList::GetDOMWrapper(
    &mNumberListAttributes[TABLEVALUES], this, TABLEVALUES);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Slope()
{
  return mNumberAttributes[SLOPE].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Intercept()
{
  return mNumberAttributes[INTERCEPT].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Amplitude()
{
  return mNumberAttributes[AMPLITUDE].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Exponent()
{
  return mNumberAttributes[EXPONENT].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Offset()
{
  return mNumberAttributes[OFFSET].ToDOMAnimatedNumber(this);
}

bool
SVGComponentTransferFunctionElement::GenerateLookupTable(uint8_t *aTable)
{
  uint16_t type = mEnumAttributes[TYPE].GetAnimValue();

  float slope, intercept, amplitude, exponent, offset;
  GetAnimatedNumberValues(&slope, &intercept, &amplitude, 
                          &exponent, &offset, nullptr);

  const SVGNumberList &tableValues =
    mNumberListAttributes[TABLEVALUES].GetAnimValue();
  uint32_t tvLength = tableValues.Length();

  uint32_t i;

  switch (type) {
  case SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
  {
    if (tableValues.Length() < 2)
      return false;

    for (i = 0; i < 256; i++) {
      uint32_t k = (i * (tvLength - 1)) / 255;
      float v1 = tableValues[k];
      float v2 = tableValues[std::min(k + 1, tvLength - 1)];
      int32_t val =
        int32_t(255 * (v1 + (i/255.0f - k/float(tvLength-1))*(tvLength - 1)*(v2 - v1)));
      val = std::min(255, val);
      val = std::max(0, val);
      aTable[i] = val;
    }
    break;
  }

  case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
  {
    if (tableValues.Length() < 1)
      return false;

    for (i = 0; i < 256; i++) {
      uint32_t k = (i * tvLength) / 255;
      k = std::min(k, tvLength - 1);
      float v = tableValues[k];
      int32_t val = int32_t(255 * v);
      val = std::min(255, val);
      val = std::max(0, val);
      aTable[i] = val;
    }
    break;
  }

  case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
  {
    for (i = 0; i < 256; i++) {
      int32_t val = int32_t(slope * i + 255 * intercept);
      val = std::min(255, val);
      val = std::max(0, val);
      aTable[i] = val;
    }
    break;
  }

  case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
  {
    for (i = 0; i < 256; i++) {
      int32_t val = int32_t(255 * (amplitude * pow(i / 255.0f, exponent) + offset));
      val = std::min(255, val);
      val = std::max(0, val);
      aTable[i] = val;
    }
    break;
  }

  case SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
  default:
    break;
  }
  return true;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberListAttributesInfo
SVGComponentTransferFunctionElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

nsSVGElement::EnumAttributesInfo
SVGComponentTransferFunctionElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::NumberAttributesInfo
SVGComponentTransferFunctionElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

/* virtual */ JSObject*
SVGFEFuncRElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEFuncRElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFuncR)

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncRElement)

/* virtual */ JSObject*
SVGFEFuncGElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEFuncGElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFuncG)

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncGElement)

/* virtual */ JSObject*
SVGFEFuncBElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEFuncBElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFuncB)

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncBElement)

/* virtual */ JSObject*
SVGFEFuncAElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEFuncAElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFuncA)

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncAElement)

} // namespace dom
} // namespace mozilla

//--------------------------------------------------------------------
//
nsSVGElement::NumberInfo nsSVGFELightingElement::sNumberInfo[4] =
{
  { &nsGkAtoms::surfaceScale, 1, false },
  { &nsGkAtoms::diffuseConstant, 1, false },
  { &nsGkAtoms::specularConstant, 1, false },
  { &nsGkAtoms::specularExponent, 1, false }
};

nsSVGElement::NumberPairInfo nsSVGFELightingElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::kernelUnitLength, 0, 0 }
};

nsSVGElement::StringInfo nsSVGFELightingElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFELightingElement,nsSVGFELightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFELightingElement,nsSVGFELightingElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFELightingElement) 
NS_INTERFACE_MAP_END_INHERITING(nsSVGFELightingElementBase)

//----------------------------------------------------------------------
// Implementation

NS_IMETHODIMP_(bool)
nsSVGFELightingElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sLightingEffectsMap
  };

  return FindAttributeDependence(name, map) ||
    nsSVGFELightingElementBase::IsAttributeMapped(name);
}

void
nsSVGFELightingElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

void
nsSVGFELightingElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // XXX lighting can depend on more than the target area, because
  // of the kernels it uses. We could compute something precise here
  // but just leave it and assume we use the entire source bounding box.
}

nsIntRect
nsSVGFELightingElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                          const nsSVGFilterInstance& aInstance)
{
  // XXX be conservative for now
  return GetMaxRect();
}

static int32_t
Convolve3x3(const uint8_t *index, int32_t stride,
            const int8_t kernel[3][3]
#ifdef DEBUG
            , const uint8_t *minData, const uint8_t *maxData
#endif // DEBUG
)
{
  int32_t sum = 0;
  for (int32_t y = 0; y < 3; y++) {
    for (int32_t x = 0; x < 3; x++) {
      int8_t k = kernel[y][x];
      if (k) {
        const uint8_t *valPtr = index + (4 * (x - 1) + stride * (y - 1));
        NS_ASSERTION(valPtr >= minData, "out of bounds read (before buffer)");
        NS_ASSERTION(valPtr < maxData,  "out of bounds read (after buffer)");
        sum += k * (*valPtr);
      }
    }
  }
  return sum;
}

static void
GenerateNormal(float *N, const uint8_t *data, int32_t stride,
               int32_t surfaceWidth, int32_t surfaceHeight,
               int32_t x, int32_t y, float surfaceScale)
{
  // See this for source of constants:
  //   http://www.w3.org/TR/SVG11/filters.html#feDiffuseLightingElement
  static const int8_t Kx[3][3][3][3] =
    { { { {  0,  0,  0}, { 0, -2,  2}, { 0, -1,  1} },
        { {  0,  0,  0}, {-2,  0,  2}, {-1,  0,  1} },
        { {  0,  0,  0}, {-2,  2,  0}, {-1,  1,  0} } },
      { { {  0, -1,  1}, { 0, -2,  2}, { 0, -1,  1} },
        { { -1,  0,  1}, {-2,  0,  2}, {-1,  0,  1} },
        { { -1,  1,  0}, {-2,  2,  0}, {-1,  1,  0} } },
      { { {  0, -1,  1}, { 0, -2,  2}, { 0,  0,  0} },
        { { -1,  0,  1}, {-2,  0,  2}, { 0,  0,  0} },
        { { -1,  1,  0}, {-2,  2,  0}, { 0,  0,  0} } } };
  static const int8_t Ky[3][3][3][3] =
    { { { {  0,  0,  0}, { 0, -2, -1}, { 0,  2,  1} },
        { {  0,  0,  0}, {-1, -2, -1}, { 1,  2,  1} },
        { {  0,  0,  0}, {-1, -2,  1}, { 1,  2,  0} } },
      { { {  0, -2, -1}, { 0,  0,  0}, { 0,  2,  1} },
        { { -1, -2, -1}, { 0,  0,  0}, { 1,  2,  1} },
        { { -1, -2,  0}, { 0,  0,  0}, { 1,  2,  0} } },
      { { {  0, -2, -1}, { 0,  2,  1}, { 0,  0,  0} },
        { { -1, -2, -1}, { 1,  2,  1}, { 0,  0,  0} },
        { { -1, -2,  0}, { 1,  2,  0}, { 0,  0,  0} } } };
  static const float FACTORx[3][3] =
    { { 2.0f / 3.0f, 1.0f / 3.0f, 2.0f / 3.0f },
      { 1.0f / 2.0f, 1.0f / 4.0f, 1.0f / 2.0f },
      { 2.0f / 3.0f, 1.0f / 3.0f, 2.0f / 3.0f } };
  static const float FACTORy[3][3] =
    { { 2.0f / 3.0f, 1.0f / 2.0f, 2.0f / 3.0f },
      { 1.0f / 3.0f, 1.0f / 4.0f, 1.0f / 3.0f },
      { 2.0f / 3.0f, 1.0f / 2.0f, 2.0f / 3.0f } };

  // degenerate cases
  if (surfaceWidth == 1 || surfaceHeight == 1) {
    // just return a unit vector pointing towards the viewer
    N[0] = 0;
    N[1] = 0;
    N[2] = 255;
    return;
  }

  int8_t xflag, yflag;
  if (x == 0) {
    xflag = 0;
  } else if (x == surfaceWidth - 1) {
    xflag = 2;
  } else {
    xflag = 1;
  }
  if (y == 0) {
    yflag = 0;
  } else if (y == surfaceHeight - 1) {
    yflag = 2;
  } else {
    yflag = 1;
  }

  const uint8_t *index = data + y * stride + 4 * x + GFX_ARGB32_OFFSET_A;

#ifdef DEBUG
  // For sanity-checking, to be sure we're not reading outside source buffer:
  const uint8_t* minData = data;
  const uint8_t* maxData = minData + (surfaceHeight * surfaceWidth * stride);

  // We'll sanity-check each value we read inside of Convolve3x3, but we
  // might as well ensure we're passing it a valid pointer to start with, too:
  NS_ASSERTION(index >= minData, "index points before buffer start");
  NS_ASSERTION(index < maxData, "index points after buffer end");
#endif // DEBUG

  N[0] = -surfaceScale * FACTORx[yflag][xflag] *
    Convolve3x3(index, stride, Kx[yflag][xflag]
#ifdef DEBUG
                , minData, maxData
#endif // DEBUG
                );

  N[1] = -surfaceScale * FACTORy[yflag][xflag] *
    Convolve3x3(index, stride, Ky[yflag][xflag]
#ifdef DEBUG
                , minData, maxData
#endif // DEBUG
                );
  N[2] = 255;
  NORMALIZE(N);
}

nsresult
nsSVGFELightingElement::Filter(nsSVGFilterInstance *instance,
                               const nsTArray<const Image*>& aSources,
                               const Image* aTarget,
                               const nsIntRect& rect)
{
  ScaleInfo info = SetupScalingFilter(instance, aSources[0], aTarget, rect,
                                      &mNumberPairAttributes[KERNEL_UNIT_LENGTH]);
  if (!info.mTarget)
    return NS_ERROR_FAILURE;

  SVGFEDistantLightElement* distantLight = nullptr;
  SVGFEPointLightElement* pointLight = nullptr;
  SVGFESpotLightElement* spotLight = nullptr;

  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return NS_ERROR_FAILURE;
  nsStyleContext* style = frame->StyleContext();

  nscolor lightColor = style->StyleSVGReset()->mLightingColor;

  // find specified light
  for (nsCOMPtr<nsIContent> child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    distantLight = child->IsSVG(nsGkAtoms::feDistantLight) ?
                     static_cast<SVGFEDistantLightElement*>(child.get()) : nullptr;
    pointLight = child->IsSVG(nsGkAtoms::fePointLight) ?
                   static_cast<SVGFEPointLightElement*>(child.get()) : nullptr;
    spotLight = child->IsSVG(nsGkAtoms::feSpotLight) ?
                   static_cast<SVGFESpotLightElement*>(child.get()) : nullptr;
    if (distantLight || pointLight || spotLight)
      break;
  }

  if (!distantLight && !pointLight && !spotLight)
    return NS_ERROR_FAILURE;

  const float radPerDeg = static_cast<float>(M_PI/180.0);

  float L[3];
  if (distantLight) {
    float azimuth, elevation;
    distantLight->GetAnimatedNumberValues(&azimuth,
                                          &elevation,
                                          nullptr);
    L[0] = cos(azimuth * radPerDeg) * cos(elevation * radPerDeg);
    L[1] = sin(azimuth * radPerDeg) * cos(elevation * radPerDeg);
    L[2] = sin(elevation * radPerDeg);
  }
  float lightPos[3], pointsAt[3], specularExponent;
  float cosConeAngle = 0;
  if (pointLight) {
    pointLight->GetAnimatedNumberValues(lightPos,
                                        lightPos + 1,
                                        lightPos + 2,
                                        nullptr);
    instance->ConvertLocation(lightPos);
  }
  if (spotLight) {
    float limitingConeAngle;
    spotLight->GetAnimatedNumberValues(lightPos,
                                       lightPos + 1,
                                       lightPos + 2,
                                       pointsAt,
                                       pointsAt + 1,
                                       pointsAt + 2,
                                       &specularExponent,
                                       &limitingConeAngle,
                                       nullptr);
    instance->ConvertLocation(lightPos);
    instance->ConvertLocation(pointsAt);

    if (spotLight->mNumberAttributes[SVGFESpotLightElement::LIMITING_CONE_ANGLE].
                                       IsExplicitlySet()) {
      cosConeAngle = std::max<double>(cos(limitingConeAngle * radPerDeg), 0.0);
    }
  }

  float surfaceScale = mNumberAttributes[SURFACE_SCALE].GetAnimValue();

  const nsIntRect& dataRect = info.mDataRect;
  int32_t stride = info.mSource->Stride();
  uint8_t *sourceData = info.mSource->Data();
  uint8_t *targetData = info.mTarget->Data();
  int32_t surfaceWidth = info.mSource->Width();
  int32_t surfaceHeight = info.mSource->Height();

  for (int32_t y = dataRect.y; y < dataRect.YMost(); y++) {
    for (int32_t x = dataRect.x; x < dataRect.XMost(); x++) {
      int32_t index = y * stride + x * 4;

      float N[3];
      GenerateNormal(N, sourceData, stride, surfaceWidth, surfaceHeight,
                     x, y, surfaceScale);

      if (pointLight || spotLight) {
        gfxPoint pt = instance->FilterSpaceToUserSpace(
                gfxPoint(x + instance->GetSurfaceRect().x,
                         y + instance->GetSurfaceRect().y));
        float Z = surfaceScale * sourceData[index + GFX_ARGB32_OFFSET_A] / 255;

        L[0] = lightPos[0] - pt.x;
        L[1] = lightPos[1] - pt.y;
        L[2] = lightPos[2] - Z;
        NORMALIZE(L);
      }

      nscolor color;

      if (spotLight) {
        float S[3];
        S[0] = pointsAt[0] - lightPos[0];
        S[1] = pointsAt[1] - lightPos[1];
        S[2] = pointsAt[2] - lightPos[2];
        NORMALIZE(S);
        float dot = -DOT(L, S);
        float tmp = pow(dot, specularExponent);
        if (dot < cosConeAngle) tmp = 0;
        color = NS_RGB(uint8_t(NS_GET_R(lightColor) * tmp),
                       uint8_t(NS_GET_G(lightColor) * tmp),
                       uint8_t(NS_GET_B(lightColor) * tmp));
      } else {
        color = lightColor;
      }

      LightPixel(N, L, color, targetData + index);
    }
  }

  FinishScalingFilter(&info);

  return NS_OK;
}

bool
nsSVGFELightingElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                  nsIAtom* aAttribute) const
{
  return nsSVGFELightingElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::surfaceScale ||
           aAttribute == nsGkAtoms::kernelUnitLength));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFELightingElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
nsSVGFELightingElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFELightingElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}
