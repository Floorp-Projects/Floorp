/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGMaskFrame.h"

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsSVGEffects.h"
#include "mozilla/dom/SVGMaskElement.h"
#ifdef BUILD_ARM_NEON
#include "mozilla/arm.h"
#include "nsSVGMaskFrameNEON.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

// c = n / 255
// c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4)) * 255 + 0.5
static const uint8_t gsRGBToLinearRGBMap[256] = {
  0,   0,   0,   0,   0,   0,   0,   1,
  1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   2,   2,   2,   2,   2,   2,
  2,   2,   3,   3,   3,   3,   3,   3,
  4,   4,   4,   4,   4,   5,   5,   5,
  5,   6,   6,   6,   6,   7,   7,   7,
  8,   8,   8,   8,   9,   9,   9,  10,
 10,  10,  11,  11,  12,  12,  12,  13,
 13,  13,  14,  14,  15,  15,  16,  16,
 17,  17,  17,  18,  18,  19,  19,  20,
 20,  21,  22,  22,  23,  23,  24,  24,
 25,  25,  26,  27,  27,  28,  29,  29,
 30,  30,  31,  32,  32,  33,  34,  35,
 35,  36,  37,  37,  38,  39,  40,  41,
 41,  42,  43,  44,  45,  45,  46,  47,
 48,  49,  50,  51,  51,  52,  53,  54,
 55,  56,  57,  58,  59,  60,  61,  62,
 63,  64,  65,  66,  67,  68,  69,  70,
 71,  72,  73,  74,  76,  77,  78,  79,
 80,  81,  82,  84,  85,  86,  87,  88,
 90,  91,  92,  93,  95,  96,  97,  99,
100, 101, 103, 104, 105, 107, 108, 109,
111, 112, 114, 115, 116, 118, 119, 121,
122, 124, 125, 127, 128, 130, 131, 133,
134, 136, 138, 139, 141, 142, 144, 146,
147, 149, 151, 152, 154, 156, 157, 159,
161, 163, 164, 166, 168, 170, 171, 173,
175, 177, 179, 181, 183, 184, 186, 188,
190, 192, 194, 196, 198, 200, 202, 204,
206, 208, 210, 212, 214, 216, 218, 220,
222, 224, 226, 229, 231, 233, 235, 237,
239, 242, 244, 246, 248, 250, 253, 255
};

static void
ComputesRGBLuminanceMask(const uint8_t *aSourceData,
                         int32_t aSourceStride,
                         uint8_t *aDestData,
                         int32_t aDestStride,
                         const IntSize &aSize,
                         float aOpacity)
{
#ifdef BUILD_ARM_NEON
  if (mozilla::supports_neon()) {
    ComputesRGBLuminanceMask_NEON(aSourceData, aSourceStride,
                                  aDestData, aDestStride,
                                  aSize, aOpacity);
    return;
  }
#endif

  int32_t redFactor = 55 * aOpacity; // 255 * 0.2125 * opacity
  int32_t greenFactor = 183 * aOpacity; // 255 * 0.7154 * opacity
  int32_t blueFactor = 18 * aOpacity; // 255 * 0.0721
  int32_t sourceOffset = aSourceStride - 4 * aSize.width;
  const uint8_t *sourcePixel = aSourceData;
  int32_t destOffset = aDestStride - aSize.width;
  uint8_t *destPixel = aDestData;

  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      uint8_t a = sourcePixel[GFX_ARGB32_OFFSET_A];

      if (a) {
        *destPixel = (redFactor * sourcePixel[GFX_ARGB32_OFFSET_R] +
                      greenFactor * sourcePixel[GFX_ARGB32_OFFSET_G] +
                      blueFactor * sourcePixel[GFX_ARGB32_OFFSET_B]) >> 8;
      } else {
        *destPixel = 0;
      }
      sourcePixel += 4;
      destPixel++;
    }
    sourcePixel += sourceOffset;
    destPixel += destOffset;
  }
}

static void
ComputeLinearRGBLuminanceMask(const uint8_t *aSourceData,
                              int32_t aSourceStride,
                              uint8_t *aDestData,
                              int32_t aDestStride,
                              const IntSize &aSize,
                              float aOpacity)
{
  int32_t redFactor = 55 * aOpacity; // 255 * 0.2125 * opacity
  int32_t greenFactor = 183 * aOpacity; // 255 * 0.7154 * opacity
  int32_t blueFactor = 18 * aOpacity; // 255 * 0.0721
  int32_t sourceOffset = aSourceStride - 4 * aSize.width;
  const uint8_t *sourcePixel = aSourceData;
  int32_t destOffset = aDestStride - aSize.width;
  uint8_t *destPixel = aDestData;

  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      uint8_t a = sourcePixel[GFX_ARGB32_OFFSET_A];

      // unpremultiply
      if (a) {
        if (a == 255) {
          /* sRGB -> linearRGB -> intensity */
          *destPixel =
            static_cast<uint8_t>
                       ((gsRGBToLinearRGBMap[sourcePixel[GFX_ARGB32_OFFSET_R]] *
                         redFactor +
                         gsRGBToLinearRGBMap[sourcePixel[GFX_ARGB32_OFFSET_G]] *
                         greenFactor +
                         gsRGBToLinearRGBMap[sourcePixel[GFX_ARGB32_OFFSET_B]] *
                         blueFactor) >> 8);
        } else {
          uint8_t tempPixel[4];
          tempPixel[GFX_ARGB32_OFFSET_B] =
            (255 * sourcePixel[GFX_ARGB32_OFFSET_B]) / a;
          tempPixel[GFX_ARGB32_OFFSET_G] =
            (255 * sourcePixel[GFX_ARGB32_OFFSET_G]) / a;
          tempPixel[GFX_ARGB32_OFFSET_R] =
            (255 * sourcePixel[GFX_ARGB32_OFFSET_R]) / a;

          /* sRGB -> linearRGB -> intensity */
          *destPixel =
            static_cast<uint8_t>
                       (((gsRGBToLinearRGBMap[tempPixel[GFX_ARGB32_OFFSET_R]] *
                          redFactor +
                          gsRGBToLinearRGBMap[tempPixel[GFX_ARGB32_OFFSET_G]] *
                          greenFactor +
                          gsRGBToLinearRGBMap[tempPixel[GFX_ARGB32_OFFSET_B]] *
                          blueFactor) >> 8) * (a / 255.0f));
        }
      } else {
        *destPixel = 0;
      }
      sourcePixel += 4;
      destPixel++;
    }
    sourcePixel += sourceOffset;
    destPixel += destOffset;
  }
}

static void
ComputeAlphaMask(const uint8_t *aSourceData,
                 int32_t aSourceStride,
                 uint8_t *aDestData,
                 int32_t aDestStride,
                 const IntSize &aSize,
                 float aOpacity)
{
  int32_t sourceOffset = aSourceStride - 4 * aSize.width;
  const uint8_t *sourcePixel = aSourceData;
  int32_t destOffset = aDestStride - aSize.width;
  uint8_t *destPixel = aDestData;

  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      *destPixel = sourcePixel[GFX_ARGB32_OFFSET_A] * aOpacity;
      sourcePixel += 4;
      destPixel++;
    }
    sourcePixel += sourceOffset;
    destPixel += destOffset;
  }
}

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGMaskFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGMaskFrame)

mozilla::Pair<DrawResult, RefPtr<SourceSurface>>
nsSVGMaskFrame::GetMaskForMaskedFrame(MaskParams& aParams)
{
  // Make sure we break reference loops and over long reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mInUse,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return MakePair(DrawResult::SUCCESS, RefPtr<SourceSurface>());
  }

  gfxRect maskArea = GetMaskArea(aParams.maskedFrame);
  gfxContext* context = aParams.ctx;
  // Get the clip extents in device space:
  // Minimizing the mask surface extents (using both the current clip extents
  // and maskArea) is important for performance.
  context->Save();
  nsSVGUtils::SetClipRect(context, aParams.toUserSpace, maskArea);
  context->SetMatrix(gfxMatrix());
  gfxRect maskSurfaceRect = context->GetClipExtents();
  maskSurfaceRect.RoundOut();
  context->Restore();

  bool resultOverflows;
  IntSize maskSurfaceSize =
    nsSVGUtils::ConvertToSurfaceSize(maskSurfaceRect.Size(), &resultOverflows);

  if (resultOverflows || maskSurfaceSize.IsEmpty()) {
    // Return value other then DrawResult::SUCCESS, so the caller can skip
    // painting the masked frame(aParams.maskedFrame).
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }

  RefPtr<DrawTarget> maskDT =
    gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
      maskSurfaceSize, SurfaceFormat::B8G8R8A8);

  if (!maskDT || !maskDT->IsValid()) {
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }

  gfxMatrix maskSurfaceMatrix =
    context->CurrentMatrix() * gfxMatrix::Translation(-maskSurfaceRect.TopLeft());

  RefPtr<gfxContext> tmpCtx = gfxContext::CreateOrNull(maskDT);
  MOZ_ASSERT(tmpCtx); // already checked the draw target above
  tmpCtx->SetMatrix(maskSurfaceMatrix);

  mMatrixForChildren = GetMaskTransform(aParams.maskedFrame) *
                       aParams.toUserSpace;
  DrawResult result;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    // The CTM of each frame referencing us can be different
    nsSVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      SVGFrame->NotifySVGChanged(nsSVGDisplayableFrame::TRANSFORM_CHANGED);
    }
    gfxMatrix m = mMatrixForChildren;
    if (kid->GetContent()->IsSVGElement()) {
      m = static_cast<nsSVGElement*>(kid->GetContent())->
            PrependLocalTransformsTo(m, eUserSpaceToParent);
    }
    result = nsSVGUtils::PaintFrameWithEffects(kid, *tmpCtx, m);
    if (result != DrawResult::SUCCESS) {
      return MakePair(result, RefPtr<SourceSurface>());
    }
  }

  RefPtr<SourceSurface> maskSnapshot = maskDT->Snapshot();
  if (!maskSnapshot) {
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }
  RefPtr<DataSourceSurface> maskSurface = maskSnapshot->GetDataSurface();
  DataSourceSurface::MappedSurface map;
  if (!maskSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }

  // Create alpha channel mask for output
  RefPtr<DataSourceSurface> destMaskSurface =
    Factory::CreateDataSourceSurface(maskSurfaceSize, SurfaceFormat::A8);
  if (!destMaskSurface) {
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }
  DataSourceSurface::MappedSurface destMap;
  if (!destMaskSurface->Map(DataSourceSurface::MapType::WRITE, &destMap)) {
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }

  uint8_t maskType;
  if (aParams.maskMode == NS_STYLE_MASK_MODE_MATCH_SOURCE) {
    maskType = StyleSVGReset()->mMaskType;
  } else {
    maskType = aParams.maskMode == NS_STYLE_MASK_MODE_LUMINANCE
               ? NS_STYLE_MASK_TYPE_LUMINANCE : NS_STYLE_MASK_TYPE_ALPHA;
  }

  if (maskType == NS_STYLE_MASK_TYPE_LUMINANCE) {
    if (StyleSVG()->mColorInterpolation ==
        NS_STYLE_COLOR_INTERPOLATION_LINEARRGB) {
      ComputeLinearRGBLuminanceMask(map.mData, map.mStride,
                                    destMap.mData, destMap.mStride,
                                    maskSurfaceSize, aParams.opacity);
    } else {
      ComputesRGBLuminanceMask(map.mData, map.mStride,
                               destMap.mData, destMap.mStride,
                               maskSurfaceSize, aParams.opacity);
    }
  } else {
    ComputeAlphaMask(map.mData, map.mStride,
                     destMap.mData, destMap.mStride,
                     maskSurfaceSize, aParams.opacity);
  }

  maskSurface->Unmap();
  destMaskSurface->Unmap();

  // Moz2D transforms in the opposite direction to Thebes
  if (!maskSurfaceMatrix.Invert()) {
    return MakePair(DrawResult::TEMPORARY_ERROR, RefPtr<SourceSurface>());
  }

  *aParams.maskTransform = ToMatrix(maskSurfaceMatrix);
  RefPtr<SourceSurface> surface = destMaskSurface.forget();
  return MakePair(DrawResult::SUCCESS, Move(surface));
}

gfxRect
nsSVGMaskFrame::GetMaskArea(nsIFrame* aMaskedFrame)
{
  SVGMaskElement *maskElem = static_cast<SVGMaskElement*>(mContent);

  uint16_t units =
    maskElem->mEnumAttributes[SVGMaskElement::MASKUNITS].GetAnimValue();
  gfxRect bbox;
  if (units == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    bbox =
      nsSVGUtils::GetBBox(aMaskedFrame,
                          nsSVGUtils::eUseFrameBoundsForOuterSVG |
                          nsSVGUtils::eBBoxIncludeFillGeometry);
  }

  // Bounds in the user space of aMaskedFrame
  gfxRect maskArea = nsSVGUtils::GetRelativeRect(units,
                       &maskElem->mLengthAttributes[SVGMaskElement::ATTR_X],
                       bbox, aMaskedFrame);

  return maskArea;
}

nsresult
nsSVGMaskFrame::AttributeChanged(int32_t  aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height||
       aAttribute == nsGkAtoms::maskUnits ||
       aAttribute == nsGkAtoms::maskContentUnits)) {
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }

  return nsSVGContainerFrame::AttributeChanged(aNameSpaceID,
                                               aAttribute, aModType);
}

#ifdef DEBUG
void
nsSVGMaskFrame::Init(nsIContent*       aContent,
                     nsContainerFrame* aParent,
                     nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::mask),
               "Content is not an SVG mask");

  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGMaskFrame::GetType() const
{
  return nsGkAtoms::svgMaskFrame;
}

gfxMatrix
nsSVGMaskFrame::GetCanvasTM()
{
  return mMatrixForChildren;
}

gfxMatrix
nsSVGMaskFrame::GetMaskTransform(nsIFrame* aMaskedFrame)
{
  SVGMaskElement *content = static_cast<SVGMaskElement*>(mContent);

  nsSVGEnum* maskContentUnits =
    &content->mEnumAttributes[SVGMaskElement::MASKCONTENTUNITS];

  return nsSVGUtils::AdjustMatrixForUnits(gfxMatrix(), maskContentUnits,
                                          aMaskedFrame);
}
