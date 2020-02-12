/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGMaskFrame.h"

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "mozilla/PresShell.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/SVGMaskElement.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "mozilla/gfx/2D.h"
#include "SVGObserverUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::SVGUnitTypes_Binding;
using namespace mozilla::gfx;
using namespace mozilla::image;

nsIFrame* NS_NewSVGMaskFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsSVGMaskFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGMaskFrame)

already_AddRefed<SourceSurface> nsSVGMaskFrame::GetMaskForMaskedFrame(
    MaskParams& aParams) {
  // Make sure we break reference loops and over long reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mInUse, &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return nullptr;
  }

  gfxRect maskArea = GetMaskArea(aParams.maskedFrame);
  if (maskArea.IsEmpty()) {
    return nullptr;
  }
  gfxContext* context = aParams.ctx;
  // Get the clip extents in device space:
  // Minimizing the mask surface extents (using both the current clip extents
  // and maskArea) is important for performance.
  //
  gfxRect maskSurfaceRectDouble = aParams.toUserSpace.TransformBounds(maskArea);
  Rect maskSurfaceRect = ToRect(maskSurfaceRectDouble);
  maskSurfaceRect.RoundOut();

  StyleMaskType maskType;
  if (aParams.maskMode == StyleMaskMode::MatchSource) {
    maskType = StyleSVGReset()->mMaskType;
  } else {
    maskType = aParams.maskMode == StyleMaskMode::Luminance
                   ? StyleMaskType::Luminance
                   : StyleMaskType::Alpha;
  }

  RefPtr<DrawTarget> maskDT;
  if (maskType == StyleMaskType::Luminance) {
    maskDT = context->GetDrawTarget()->CreateClippedDrawTarget(
        maskSurfaceRect, SurfaceFormat::B8G8R8A8);
  } else {
    maskDT = context->GetDrawTarget()->CreateClippedDrawTarget(
        maskSurfaceRect, SurfaceFormat::A8);
  }

  if (!maskDT || !maskDT->IsValid()) {
    return nullptr;
  }

  RefPtr<gfxContext> tmpCtx =
      gfxContext::CreatePreservingTransformOrNull(maskDT);
  MOZ_ASSERT(tmpCtx);  // already checked the draw target above

  mMatrixForChildren =
      GetMaskTransform(aParams.maskedFrame) * aParams.toUserSpace;

  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    gfxMatrix m = mMatrixForChildren;

    // The CTM of each frame referencing us can be different
    nsSVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      SVGFrame->NotifySVGChanged(nsSVGDisplayableFrame::TRANSFORM_CHANGED);
      m = nsSVGUtils::GetTransformMatrixInUserSpace(kid) * m;
    }

    nsSVGUtils::PaintFrameWithEffects(kid, *tmpCtx, m, aParams.imgParams);
  }

  RefPtr<SourceSurface> surface;
  if (maskType == StyleMaskType::Luminance) {
    auto luminanceType = LuminanceType::LUMINANCE;
    if (StyleSVG()->mColorInterpolation ==
        NS_STYLE_COLOR_INTERPOLATION_LINEARRGB) {
      luminanceType = LuminanceType::LINEARRGB;
    }

    RefPtr<SourceSurface> maskSnapshot =
        maskDT->IntoLuminanceSource(luminanceType, aParams.opacity);
    if (!maskSnapshot) {
      return nullptr;
    }
    surface = maskSnapshot.forget();
  } else {
    maskDT->FillRect(maskSurfaceRect,
                     ColorPattern(Color(1.0f, 1.0f, 1.0f, aParams.opacity)),
                     DrawOptions(1, CompositionOp::OP_IN));
    RefPtr<SourceSurface> maskSnapshot = maskDT->Snapshot();
    if (!maskSnapshot) {
      return nullptr;
    }
    surface = maskSnapshot.forget();
  }

  return surface.forget();
}

gfxRect nsSVGMaskFrame::GetMaskArea(nsIFrame* aMaskedFrame) {
  SVGMaskElement* maskElem = static_cast<SVGMaskElement*>(GetContent());

  uint16_t units =
      maskElem->mEnumAttributes[SVGMaskElement::MASKUNITS].GetAnimValue();
  gfxRect bbox;
  if (units == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    bbox = nsSVGUtils::GetBBox(aMaskedFrame,
                               nsSVGUtils::eUseFrameBoundsForOuterSVG |
                                   nsSVGUtils::eBBoxIncludeFillGeometry);
  }

  // Bounds in the user space of aMaskedFrame
  gfxRect maskArea = nsSVGUtils::GetRelativeRect(
      units, &maskElem->mLengthAttributes[SVGMaskElement::ATTR_X], bbox,
      aMaskedFrame);

  return maskArea;
}

nsresult nsSVGMaskFrame::AttributeChanged(int32_t aNameSpaceID,
                                          nsAtom* aAttribute,
                                          int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::maskUnits ||
       aAttribute == nsGkAtoms::maskContentUnits)) {
    SVGObserverUtils::InvalidateDirectRenderingObservers(this);
  }

  return nsSVGContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                               aModType);
}

#ifdef DEBUG
void nsSVGMaskFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                          nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::mask),
               "Content is not an SVG mask");

  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

gfxMatrix nsSVGMaskFrame::GetCanvasTM() { return mMatrixForChildren; }

gfxMatrix nsSVGMaskFrame::GetMaskTransform(nsIFrame* aMaskedFrame) {
  SVGMaskElement* content = static_cast<SVGMaskElement*>(GetContent());

  SVGAnimatedEnumeration* maskContentUnits =
      &content->mEnumAttributes[SVGMaskElement::MASKCONTENTUNITS];

  uint32_t flags = nsSVGUtils::eBBoxIncludeFillGeometry |
                   (aMaskedFrame->StyleBorder()->mBoxDecorationBreak ==
                            StyleBoxDecorationBreak::Clone
                        ? nsSVGUtils::eIncludeOnlyCurrentFrameForNonSVGElement
                        : 0);

  return nsSVGUtils::AdjustMatrixForUnits(gfxMatrix(), maskContentUnits,
                                          aMaskedFrame, flags);
}
