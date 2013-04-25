/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEGaussianBlurElement.h"
#include "mozilla/dom/SVGFEGaussianBlurElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEGaussianBlur)

namespace mozilla {
namespace dom {

JSObject*
SVGFEGaussianBlurElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEGaussianBlurElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberPairInfo SVGFEGaussianBlurElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::stdDeviation, 0, 0 }
};

nsSVGElement::StringInfo SVGFEGaussianBlurElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEGaussianBlurElement)

//----------------------------------------------------------------------

already_AddRefed<nsIDOMSVGAnimatedString>
SVGFEGaussianBlurElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEGaussianBlurElement::StdDeviationX()
{
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(nsSVGNumberPair::eFirst, this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEGaussianBlurElement::StdDeviationY()
{
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(nsSVGNumberPair::eSecond, this);
}

void
SVGFEGaussianBlurElement::SetStdDeviation(float stdDeviationX, float stdDeviationY)
{
  mNumberPairAttributes[STD_DEV].SetBaseValues(stdDeviationX, stdDeviationY, this);
}

/**
 * We want to speed up 1/N integer divisions --- integer division is
 * often rather slow.
 * We know that our input numerators V are constrained to be <= 255*N,
 * so the result of dividing by N always fits in 8 bits.
 * So we can try approximating the division V/N as V*K/(2^24) (integer
 * division, 32-bit multiply). Dividing by 2^24 is a simple shift so it's
 * fast. The main problem is choosing a value for K; this function returns
 * K's value.
 *
 * If the result is correct for the extrema, V=0 and V=255*N, then we'll
 * be in good shape since both the original function and our approximation
 * are linear. V=0 always gives 0 in both cases, no problem there.
 * For V=255*N, let's choose the largest K that doesn't cause overflow
 * and ensure that it gives the right answer. The constraints are
 *     (1)   255*N*K < 2^32
 * and (2)   255*N*K >= 255*(2^24)
 *
 * From (1) we find the best value of K is floor((2^32 - 1)/(255*N)).
 * (2) tells us when this will be valid:
 *    N*floor((2^32 - 1)/(255*N)) >= 2^24
 * Now, floor(X) > X - 1, so (2) holds if
 *    N*((2^32 - 1)/(255*N) - 1) >= 2^24
 *         (2^32 - 1)/255 - 2^24 >= N
 *                             N <= 65793
 *
 * If all that math confuses you, this should convince you:
 * > perl -e 'for($N=1;(255*$N*int(0xFFFFFFFF/(255*$N)))>>24==255;++$N){}print"$N\n"'
 * 66052
 *
 * So this is fine for all reasonable values of N. For larger values of N
 * we may as well just use the same approximation and accept the fact that
 * the output channel values will be a little low.
 */
static uint32_t ComputeScaledDivisor(uint32_t aDivisor)
{
  return UINT32_MAX/(255*aDivisor);
}

static void
BoxBlur(const uint8_t *aInput, uint8_t *aOutput,
        int32_t aStrideMinor, int32_t aStartMinor, int32_t aEndMinor,
        int32_t aLeftLobe, int32_t aRightLobe, bool aAlphaOnly)
{
  int32_t boxSize = aLeftLobe + aRightLobe + 1;
  int32_t scaledDivisor = ComputeScaledDivisor(boxSize);
  int32_t sums[4] = {0, 0, 0, 0};

  for (int32_t i=0; i < boxSize; i++) {
    int32_t pos = aStartMinor - aLeftLobe + i;
    pos = std::max(pos, aStartMinor);
    pos = std::min(pos, aEndMinor - 1);
#define SUM(j)     sums[j] += aInput[aStrideMinor*pos + j];
    SUM(0); SUM(1); SUM(2); SUM(3);
#undef SUM
  }

  aOutput += aStrideMinor*aStartMinor;
  if (aStartMinor + int32_t(boxSize) <= aEndMinor) {
    const uint8_t *lastInput = aInput + aStartMinor*aStrideMinor;
    const uint8_t *nextInput = aInput + (aStartMinor + aRightLobe + 1)*aStrideMinor;
#define OUTPUT(j)     aOutput[j] = (sums[j]*scaledDivisor) >> 24;
#define SUM(j)        sums[j] += nextInput[j] - lastInput[j];
    // process pixels in B, G, R, A order because that's 0, 1, 2, 3 for x86
#define OUTPUT_PIXEL() \
        if (!aAlphaOnly) { OUTPUT(GFX_ARGB32_OFFSET_B); \
                           OUTPUT(GFX_ARGB32_OFFSET_G); \
                           OUTPUT(GFX_ARGB32_OFFSET_R); } \
        OUTPUT(GFX_ARGB32_OFFSET_A);
#define SUM_PIXEL() \
        if (!aAlphaOnly) { SUM(GFX_ARGB32_OFFSET_B); \
                           SUM(GFX_ARGB32_OFFSET_G); \
                           SUM(GFX_ARGB32_OFFSET_R); } \
        SUM(GFX_ARGB32_OFFSET_A);
    for (int32_t minor = aStartMinor;
         minor < aStartMinor + aLeftLobe;
         minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      nextInput += aStrideMinor;
      aOutput += aStrideMinor;
    }
    for (int32_t minor = aStartMinor + aLeftLobe;
         minor < aEndMinor - aRightLobe - 1;
         minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      lastInput += aStrideMinor;
      nextInput += aStrideMinor;
      aOutput += aStrideMinor;
    }
    // nextInput is now aInput + aEndMinor*aStrideMinor. Set it back to
    // aInput + (aEndMinor - 1)*aStrideMinor so we read the last pixel in every
    // iteration of the next loop.
    nextInput -= aStrideMinor;
    for (int32_t minor = aEndMinor - aRightLobe - 1; minor < aEndMinor; minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      lastInput += aStrideMinor;
      aOutput += aStrideMinor;
#undef SUM_PIXEL
#undef SUM
    }
  } else {
    for (int32_t minor = aStartMinor; minor < aEndMinor; minor++) {
      int32_t tmp = minor - aLeftLobe;
      int32_t last = std::max(tmp, aStartMinor);
      int32_t next = std::min(tmp + int32_t(boxSize), aEndMinor - 1);

      OUTPUT_PIXEL();
#define SUM(j)     sums[j] += aInput[aStrideMinor*next + j] - \
                              aInput[aStrideMinor*last + j];
      if (!aAlphaOnly) { SUM(GFX_ARGB32_OFFSET_B);
                         SUM(GFX_ARGB32_OFFSET_G);
                         SUM(GFX_ARGB32_OFFSET_R); }
      SUM(GFX_ARGB32_OFFSET_A);
      aOutput += aStrideMinor;
#undef SUM
#undef OUTPUT_PIXEL
#undef OUTPUT
    }
  }
}

static uint32_t
GetBlurBoxSize(double aStdDev)
{
  NS_ASSERTION(aStdDev >= 0, "Negative standard deviations not allowed");

  double size = aStdDev*3*sqrt(2*M_PI)/4;
  // Doing super-large blurs accurately isn't very important.
  uint32_t max = 1024;
  if (size > max)
    return max;
  return uint32_t(floor(size + 0.5));
}

nsresult
SVGFEGaussianBlurElement::GetDXY(uint32_t *aDX, uint32_t *aDY,
                                 const nsSVGFilterInstance& aInstance)
{
  float stdX = aInstance.GetPrimitiveNumber(SVGContentUtils::X,
                                            &mNumberPairAttributes[STD_DEV],
                                            nsSVGNumberPair::eFirst);
  float stdY = aInstance.GetPrimitiveNumber(SVGContentUtils::Y,
                                            &mNumberPairAttributes[STD_DEV],
                                            nsSVGNumberPair::eSecond);
  if (stdX < 0 || stdY < 0)
    return NS_ERROR_FAILURE;

  // If the box size is greater than twice the temporary surface size
  // in an axis, then each pixel will be set to the average of all the
  // other pixel values.
  *aDX = GetBlurBoxSize(stdX);
  *aDY = GetBlurBoxSize(stdY);
  return NS_OK;
}

static bool
AreAllColorChannelsZero(const nsSVGFE::Image* aTarget)
{
  return aTarget->mConstantColorChannels &&
         aTarget->mImage->GetDataSize() >= 4 &&
         (*reinterpret_cast<uint32_t*>(aTarget->mImage->Data()) & 0x00FFFFFF) == 0;
}

void
SVGFEGaussianBlurElement::GaussianBlur(const Image* aSource,
                                       const Image* aTarget,
                                       const nsIntRect& aDataRect,
                                       uint32_t aDX, uint32_t aDY)
{
  NS_ASSERTION(nsIntRect(0, 0, aTarget->mImage->Width(), aTarget->mImage->Height()).Contains(aDataRect),
               "aDataRect out of bounds");

  nsAutoArrayPtr<uint8_t> tmp(new uint8_t[aTarget->mImage->GetDataSize()]);
  if (!tmp)
    return;
  memset(tmp, 0, aTarget->mImage->GetDataSize());

  bool alphaOnly = AreAllColorChannelsZero(aTarget);

  const uint8_t* sourceData = aSource->mImage->Data();
  uint8_t* targetData = aTarget->mImage->Data();
  uint32_t stride = aTarget->mImage->Stride();

  if (aDX == 0) {
    CopyDataRect(tmp, sourceData, stride, aDataRect);
  } else {
    int32_t longLobe = aDX/2;
    int32_t shortLobe = (aDX & 1) ? longLobe : longLobe - 1;
    for (int32_t major = aDataRect.y; major < aDataRect.YMost(); ++major) {
      int32_t ms = major*stride;
      BoxBlur(sourceData + ms, tmp + ms, 4, aDataRect.x, aDataRect.XMost(), longLobe, shortLobe, alphaOnly);
      BoxBlur(tmp + ms, targetData + ms, 4, aDataRect.x, aDataRect.XMost(), shortLobe, longLobe, alphaOnly);
      BoxBlur(targetData + ms, tmp + ms, 4, aDataRect.x, aDataRect.XMost(), longLobe, longLobe, alphaOnly);
    }
  }

  if (aDY == 0) {
    CopyDataRect(targetData, tmp, stride, aDataRect);
  } else {
    int32_t longLobe = aDY/2;
    int32_t shortLobe = (aDY & 1) ? longLobe : longLobe - 1;
    for (int32_t major = aDataRect.x; major < aDataRect.XMost(); ++major) {
      int32_t ms = major*4;
      BoxBlur(tmp + ms, targetData + ms, stride, aDataRect.y, aDataRect.YMost(), longLobe, shortLobe, alphaOnly);
      BoxBlur(targetData + ms, tmp + ms, stride, aDataRect.y, aDataRect.YMost(), shortLobe, longLobe, alphaOnly);
      BoxBlur(tmp + ms, targetData + ms, stride, aDataRect.y, aDataRect.YMost(), longLobe, longLobe, alphaOnly);
    }
  }
}

static void
InflateRectForBlurDXY(nsIntRect* aRect, uint32_t aDX, uint32_t aDY)
{
  aRect->Inflate(3*(aDX/2), 3*(aDY/2));
}

static void
ClearRect(gfxImageSurface* aSurface, int32_t aX, int32_t aY,
          int32_t aXMost, int32_t aYMost)
{
  NS_ASSERTION(aX <= aXMost && aY <= aYMost, "Invalid rectangle");
  NS_ASSERTION(aX >= 0 && aY >= 0 && aXMost <= aSurface->Width() && aYMost <= aSurface->Height(),
               "Rectangle out of bounds");

  if (aX == aXMost || aY == aYMost)
    return;
  for (int32_t y = aY; y < aYMost; ++y) {
    memset(aSurface->Data() + aSurface->Stride()*y + aX*4, 0, (aXMost - aX)*4);
  }
}

// Clip aTarget's image to its filter primitive subregion.
// aModifiedRect contains all the pixels which might not be RGBA(0,0,0,0),
// it's relative to the surface data.
static void
ClipTarget(nsSVGFilterInstance* aInstance, const nsSVGFE::Image* aTarget,
           const nsIntRect& aModifiedRect)
{
  nsIntPoint surfaceTopLeft = aInstance->GetSurfaceRect().TopLeft();

  NS_ASSERTION(aInstance->GetSurfaceRect().Contains(aModifiedRect + surfaceTopLeft),
               "Modified data area overflows the surface?");

  nsIntRect clip = aModifiedRect;
  nsSVGUtils::ClipToGfxRect(&clip,
    aTarget->mFilterPrimitiveSubregion - gfxPoint(surfaceTopLeft.x, surfaceTopLeft.y));

  ClearRect(aTarget->mImage, aModifiedRect.x, aModifiedRect.y, aModifiedRect.XMost(), clip.y);
  ClearRect(aTarget->mImage, aModifiedRect.x, clip.y, clip.x, clip.YMost());
  ClearRect(aTarget->mImage, clip.XMost(), clip.y, aModifiedRect.XMost(), clip.YMost());
  ClearRect(aTarget->mImage, aModifiedRect.x, clip.YMost(), aModifiedRect.XMost(), aModifiedRect.YMost());
}

static void
ClipComputationRectToSurface(nsSVGFilterInstance* aInstance,
                             nsIntRect* aDataRect)
{
  aDataRect->IntersectRect(*aDataRect,
          nsIntRect(nsIntPoint(0, 0), aInstance->GetSurfaceRect().Size()));
}

nsresult
SVGFEGaussianBlurElement::Filter(nsSVGFilterInstance* aInstance,
                                 const nsTArray<const Image*>& aSources,
                                 const Image* aTarget,
                                 const nsIntRect& rect)
{
  uint32_t dx, dy;
  nsresult rv = GetDXY(&dx, &dy, *aInstance);
  if (NS_FAILED(rv))
    return rv;

  nsIntRect computationRect = rect;
  InflateRectForBlurDXY(&computationRect, dx, dy);
  ClipComputationRectToSurface(aInstance, &computationRect);
  GaussianBlur(aSources[0], aTarget, computationRect, dx, dy);
  ClipTarget(aInstance, aTarget, computationRect);
  return NS_OK;
}

bool
SVGFEGaussianBlurElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                    nsIAtom* aAttribute) const
{
  return SVGFEGaussianBlurElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::stdDeviation));
}

void
SVGFEGaussianBlurElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
SVGFEGaussianBlurElement::InflateRectForBlur(const nsIntRect& aRect,
                                             const nsSVGFilterInstance& aInstance)
{
  uint32_t dX, dY;
  nsresult rv = GetDXY(&dX, &dY, aInstance);
  nsIntRect result = aRect;
  if (NS_SUCCEEDED(rv)) {
    InflateRectForBlurDXY(&result, dX, dY);
  }
  return result;
}

nsIntRect
SVGFEGaussianBlurElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return InflateRectForBlur(aSourceBBoxes[0], aInstance);
}

void
SVGFEGaussianBlurElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  aSourceBBoxes[0] = InflateRectForBlur(aTargetBBox, aInstance);
}

nsIntRect
SVGFEGaussianBlurElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                            const nsSVGFilterInstance& aInstance)
{
  return InflateRectForBlur(aSourceChangeBoxes[0], aInstance);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberPairAttributesInfo
SVGFEGaussianBlurElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEGaussianBlurElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
