/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGGradientFrame.h"
#include <algorithm>

// Keep others in (case-insensitive) order:
#include "AutoReferenceChainGuard.h"
#include "gfxPattern.h"
#include "gfxUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/SVGGradientElement.h"
#include "mozilla/dom/SVGGradientElementBinding.h"
#include "mozilla/dom/SVGStopElement.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "nsContentUtils.h"
#include "SVGAnimatedTransformList.h"

// XXX Tight coupling with content classes ahead!

using namespace mozilla::dom;
using namespace mozilla::dom::SVGGradientElement_Binding;
using namespace mozilla::dom::SVGUnitTypes_Binding;
using namespace mozilla::gfx;

namespace mozilla {

//----------------------------------------------------------------------
// Implementation

SVGGradientFrame::SVGGradientFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext, ClassID aID)
    : SVGPaintServerFrame(aStyle, aPresContext, aID),
      mSource(nullptr),
      mLoopFlag(false),
      mNoHRefURI(false) {}

NS_QUERYFRAME_HEAD(SVGGradientFrame)
  NS_QUERYFRAME_ENTRY(SVGGradientFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGPaintServerFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

nsresult SVGGradientFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::gradientUnits ||
       aAttribute == nsGkAtoms::gradientTransform ||
       aAttribute == nsGkAtoms::spreadMethod)) {
    SVGObserverUtils::InvalidateRenderingObservers(this);
  } else if ((aNameSpaceID == kNameSpaceID_XLink ||
              aNameSpaceID == kNameSpaceID_None) &&
             aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    SVGObserverUtils::RemoveTemplateObserver(this);
    mNoHRefURI = false;
    // And update whoever references us
    SVGObserverUtils::InvalidateRenderingObservers(this);
  }

  return SVGPaintServerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                               aModType);
}

//----------------------------------------------------------------------

uint16_t SVGGradientFrame::GetEnumValue(uint32_t aIndex, nsIContent* aDefault) {
  const SVGAnimatedEnumeration& thisEnum =
      static_cast<dom::SVGGradientElement*>(GetContent())
          ->mEnumAttributes[aIndex];

  if (thisEnum.IsExplicitlySet()) {
    return thisEnum.GetAnimValue();
  }

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<dom::SVGGradientElement*>(aDefault)
        ->mEnumAttributes[aIndex]
        .GetAnimValue();
  }

  SVGGradientFrame* next = GetReferencedGradient();

  return next ? next->GetEnumValue(aIndex, aDefault)
              : static_cast<dom::SVGGradientElement*>(aDefault)
                    ->mEnumAttributes[aIndex]
                    .GetAnimValue();
}

uint16_t SVGGradientFrame::GetGradientUnits() {
  // This getter is called every time the others are called - maybe cache it?
  return GetEnumValue(dom::SVGGradientElement::GRADIENTUNITS);
}

uint16_t SVGGradientFrame::GetSpreadMethod() {
  return GetEnumValue(dom::SVGGradientElement::SPREADMETHOD);
}

const SVGAnimatedTransformList* SVGGradientFrame::GetGradientTransformList(
    nsIContent* aDefault) {
  SVGAnimatedTransformList* thisTransformList =
      static_cast<dom::SVGGradientElement*>(GetContent())
          ->GetAnimatedTransformList();

  if (thisTransformList && thisTransformList->IsExplicitlySet())
    return thisTransformList;

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return static_cast<const dom::SVGGradientElement*>(aDefault)
        ->mGradientTransform.get();
  }

  SVGGradientFrame* next = GetReferencedGradient();

  return next ? next->GetGradientTransformList(aDefault)
              : static_cast<const dom::SVGGradientElement*>(aDefault)
                    ->mGradientTransform.get();
}

gfxMatrix SVGGradientFrame::GetGradientTransform(
    nsIFrame* aSource, const gfxRect* aOverrideBounds) {
  gfxMatrix bboxMatrix;

  uint16_t gradientUnits = GetGradientUnits();
  if (gradientUnits != SVG_UNIT_TYPE_USERSPACEONUSE) {
    NS_ASSERTION(gradientUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX,
                 "Unknown gradientUnits type");
    // objectBoundingBox is the default anyway

    gfxRect bbox = aOverrideBounds
                       ? *aOverrideBounds
                       : SVGUtils::GetBBox(
                             aSource, SVGUtils::eUseFrameBoundsForOuterSVG |
                                          SVGUtils::eBBoxIncludeFillGeometry);
    bboxMatrix =
        gfxMatrix(bbox.Width(), 0, 0, bbox.Height(), bbox.X(), bbox.Y());
  }

  const SVGAnimatedTransformList* animTransformList =
      GetGradientTransformList(GetContent());
  if (!animTransformList) {
    return bboxMatrix;
  }

  gfxMatrix gradientTransform =
      animTransformList->GetAnimValue().GetConsolidationMatrix();
  return bboxMatrix.PreMultiply(gradientTransform);
}

dom::SVGLinearGradientElement* SVGGradientFrame::GetLinearGradientWithLength(
    uint32_t aIndex, dom::SVGLinearGradientElement* aDefault) {
  // If this was a linear gradient with the required length, we would have
  // already found it in SVGLinearGradientFrame::GetLinearGradientWithLength.
  // Since we didn't find the length, continue looking down the chain.

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return aDefault;
  }

  SVGGradientFrame* next = GetReferencedGradient();
  return next ? next->GetLinearGradientWithLength(aIndex, aDefault) : aDefault;
}

dom::SVGRadialGradientElement* SVGGradientFrame::GetRadialGradientWithLength(
    uint32_t aIndex, dom::SVGRadialGradientElement* aDefault) {
  // If this was a radial gradient with the required length, we would have
  // already found it in SVGRadialGradientFrame::GetRadialGradientWithLength.
  // Since we didn't find the length, continue looking down the chain.

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return aDefault;
  }

  SVGGradientFrame* next = GetReferencedGradient();
  return next ? next->GetRadialGradientWithLength(aIndex, aDefault) : aDefault;
}

//----------------------------------------------------------------------
// SVGPaintServerFrame methods:

// helper
static void GetStopInformation(nsIFrame* aStopFrame, float* aOffset,
                               nscolor* aStopColor, float* aStopOpacity) {
  nsIContent* stopContent = aStopFrame->GetContent();
  MOZ_ASSERT(stopContent && stopContent->IsSVGElement(nsGkAtoms::stop));

  static_cast<SVGStopElement*>(stopContent)
      ->GetAnimatedNumberValues(aOffset, nullptr);

  const nsStyleSVGReset* styleSVGReset = aStopFrame->StyleSVGReset();
  *aOffset = mozilla::clamped(*aOffset, 0.0f, 1.0f);
  *aStopColor = styleSVGReset->mStopColor.CalcColor(aStopFrame);
  *aStopOpacity = styleSVGReset->mStopOpacity;
}

already_AddRefed<gfxPattern> SVGGradientFrame::GetPaintServerPattern(
    nsIFrame* aSource, const DrawTarget* aDrawTarget,
    const gfxMatrix& aContextMatrix, StyleSVGPaint nsStyleSVG::*aFillOrStroke,
    float aGraphicOpacity, imgDrawingParams& aImgParams,
    const gfxRect* aOverrideBounds) {
  uint16_t gradientUnits = GetGradientUnits();
  MOZ_ASSERT(gradientUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX ||
             gradientUnits == SVG_UNIT_TYPE_USERSPACEONUSE);
  if (gradientUnits == SVG_UNIT_TYPE_USERSPACEONUSE) {
    // Set mSource for this consumer.
    // If this gradient is applied to text, our caller will be the glyph, which
    // is not an element, so we need to get the parent
    mSource = aSource->IsTextFrame() ? aSource->GetParent() : aSource;
  }

  AutoTArray<nsIFrame*, 8> stopFrames;
  GetStopFrames(&stopFrames);

  uint32_t nStops = stopFrames.Length();

  // SVG specification says that no stops should be treated like
  // the corresponding fill or stroke had "none" specified.
  if (nStops == 0) {
    RefPtr<gfxPattern> pattern = new gfxPattern(DeviceColor());
    return do_AddRef(new gfxPattern(DeviceColor()));
  }

  if (nStops == 1 || GradientVectorLengthIsZero()) {
    auto* lastStopFrame = stopFrames[nStops - 1];
    const auto* svgReset = lastStopFrame->StyleSVGReset();
    // The gradient paints a single colour, using the stop-color of the last
    // gradient step if there are more than one.
    float stopOpacity = svgReset->mStopOpacity;
    nscolor stopColor = svgReset->mStopColor.CalcColor(lastStopFrame);

    sRGBColor stopColor2 = sRGBColor::FromABGR(stopColor);
    stopColor2.a *= stopOpacity * aGraphicOpacity;
    return do_AddRef(new gfxPattern(ToDeviceColor(stopColor2)));
  }

  // Get the transform list (if there is one). We do this after the returns
  // above since this call can be expensive when "gradientUnits" is set to
  // "objectBoundingBox" (since that requiring a GetBBox() call).
  gfxMatrix patternMatrix = GetGradientTransform(aSource, aOverrideBounds);

  if (patternMatrix.IsSingular()) {
    return nullptr;
  }

  // revert any vector effect transform so that the gradient appears unchanged
  if (aFillOrStroke == &nsStyleSVG::mStroke) {
    gfxMatrix userToOuterSVG;
    if (SVGUtils::GetNonScalingStrokeTransform(aSource, &userToOuterSVG)) {
      patternMatrix *= userToOuterSVG;
    }
  }

  if (!patternMatrix.Invert()) {
    return nullptr;
  }

  RefPtr<gfxPattern> gradient = CreateGradient();
  if (!gradient) {
    return nullptr;
  }

  uint16_t aSpread = GetSpreadMethod();
  if (aSpread == SVG_SPREADMETHOD_PAD)
    gradient->SetExtend(ExtendMode::CLAMP);
  else if (aSpread == SVG_SPREADMETHOD_REFLECT)
    gradient->SetExtend(ExtendMode::REFLECT);
  else if (aSpread == SVG_SPREADMETHOD_REPEAT)
    gradient->SetExtend(ExtendMode::REPEAT);

  gradient->SetMatrix(patternMatrix);

  // setup stops
  float lastOffset = 0.0f;

  for (uint32_t i = 0; i < nStops; i++) {
    float offset, stopOpacity;
    nscolor stopColor;

    GetStopInformation(stopFrames[i], &offset, &stopColor, &stopOpacity);

    if (offset < lastOffset)
      offset = lastOffset;
    else
      lastOffset = offset;

    sRGBColor stopColor2 = sRGBColor::FromABGR(stopColor);
    stopColor2.a *= stopOpacity * aGraphicOpacity;
    gradient->AddColorStop(offset, ToDeviceColor(stopColor2));
  }

  return gradient.forget();
}

// Private (helper) methods

SVGGradientFrame* SVGGradientFrame::GetReferencedGradient() {
  if (mNoHRefURI) {
    return nullptr;
  }

  auto GetHref = [this](nsAString& aHref) {
    dom::SVGGradientElement* grad =
        static_cast<dom::SVGGradientElement*>(this->GetContent());
    if (grad->mStringAttributes[dom::SVGGradientElement::HREF]
            .IsExplicitlySet()) {
      grad->mStringAttributes[dom::SVGGradientElement::HREF].GetAnimValue(aHref,
                                                                          grad);
    } else {
      grad->mStringAttributes[dom::SVGGradientElement::XLINK_HREF].GetAnimValue(
          aHref, grad);
    }
    this->mNoHRefURI = aHref.IsEmpty();
  };

  // We don't call SVGObserverUtils::RemoveTemplateObserver and set
  // `mNoHRefURI = false` on failure since we want to be invalidated if the ID
  // specified by our href starts resolving to a different/valid element.

  return do_QueryFrame(SVGObserverUtils::GetAndObserveTemplate(this, GetHref));
}

void SVGGradientFrame::GetStopFrames(nsTArray<nsIFrame*>* aStopFrames) {
  nsIFrame* stopFrame = nullptr;
  for (stopFrame = mFrames.FirstChild(); stopFrame;
       stopFrame = stopFrame->GetNextSibling()) {
    if (stopFrame->IsSVGStopFrame()) {
      aStopFrames->AppendElement(stopFrame);
    }
  }
  if (aStopFrames->Length() > 0) {
    return;
  }

  // Our gradient element doesn't have stops - try to "inherit" them

  // Before we recurse, make sure we'll break reference loops and over long
  // reference chains:
  static int16_t sRefChainLengthCounter = AutoReferenceChainGuard::noChain;
  AutoReferenceChainGuard refChainGuard(this, &mLoopFlag,
                                        &sRefChainLengthCounter);
  if (MOZ_UNLIKELY(!refChainGuard.Reference())) {
    // Break reference chain
    return;
  }

  SVGGradientFrame* next = GetReferencedGradient();
  if (next) {
    next->GetStopFrames(aStopFrames);
  }
}

// -------------------------------------------------------------------------
// Linear Gradients
// -------------------------------------------------------------------------

NS_QUERYFRAME_HEAD(SVGLinearGradientFrame)
  NS_QUERYFRAME_ENTRY(SVGLinearGradientFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGGradientFrame)

#ifdef DEBUG
void SVGLinearGradientFrame::Init(nsIContent* aContent,
                                  nsContainerFrame* aParent,
                                  nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::linearGradient),
               "Content is not an SVG linearGradient");

  SVGGradientFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsresult SVGLinearGradientFrame::AttributeChanged(int32_t aNameSpaceID,
                                                  nsAtom* aAttribute,
                                                  int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x1 || aAttribute == nsGkAtoms::y1 ||
       aAttribute == nsGkAtoms::x2 || aAttribute == nsGkAtoms::y2)) {
    SVGObserverUtils::InvalidateRenderingObservers(this);
  }

  return SVGGradientFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

//----------------------------------------------------------------------

float SVGLinearGradientFrame::GetLengthValue(uint32_t aIndex) {
  dom::SVGLinearGradientElement* lengthElement = GetLinearGradientWithLength(
      aIndex, static_cast<dom::SVGLinearGradientElement*>(GetContent()));
  // We passed in mContent as a fallback, so, assuming mContent is non-null, the
  // return value should also be non-null.
  MOZ_ASSERT(lengthElement,
             "Got unexpected null element from GetLinearGradientWithLength");
  const SVGAnimatedLength& length = lengthElement->mLengthAttributes[aIndex];

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransform, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  uint16_t gradientUnits = GetGradientUnits();
  if (gradientUnits == SVG_UNIT_TYPE_USERSPACEONUSE) {
    return SVGUtils::UserSpace(mSource, &length);
  }

  NS_ASSERTION(gradientUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length.GetAnimValue(static_cast<SVGViewportElement*>(nullptr));
}

dom::SVGLinearGradientElement*
SVGLinearGradientFrame::GetLinearGradientWithLength(
    uint32_t aIndex, dom::SVGLinearGradientElement* aDefault) {
  dom::SVGLinearGradientElement* thisElement =
      static_cast<dom::SVGLinearGradientElement*>(GetContent());
  const SVGAnimatedLength& length = thisElement->mLengthAttributes[aIndex];

  if (length.IsExplicitlySet()) {
    return thisElement;
  }

  return SVGGradientFrame::GetLinearGradientWithLength(aIndex, aDefault);
}

bool SVGLinearGradientFrame::GradientVectorLengthIsZero() {
  return GetLengthValue(dom::SVGLinearGradientElement::ATTR_X1) ==
             GetLengthValue(dom::SVGLinearGradientElement::ATTR_X2) &&
         GetLengthValue(dom::SVGLinearGradientElement::ATTR_Y1) ==
             GetLengthValue(dom::SVGLinearGradientElement::ATTR_Y2);
}

already_AddRefed<gfxPattern> SVGLinearGradientFrame::CreateGradient() {
  float x1 = GetLengthValue(dom::SVGLinearGradientElement::ATTR_X1);
  float y1 = GetLengthValue(dom::SVGLinearGradientElement::ATTR_Y1);
  float x2 = GetLengthValue(dom::SVGLinearGradientElement::ATTR_X2);
  float y2 = GetLengthValue(dom::SVGLinearGradientElement::ATTR_Y2);

  RefPtr<gfxPattern> pattern = new gfxPattern(x1, y1, x2, y2);
  return pattern.forget();
}

// -------------------------------------------------------------------------
// Radial Gradients
// -------------------------------------------------------------------------

NS_QUERYFRAME_HEAD(SVGRadialGradientFrame)
  NS_QUERYFRAME_ENTRY(SVGRadialGradientFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGGradientFrame)

#ifdef DEBUG
void SVGRadialGradientFrame::Init(nsIContent* aContent,
                                  nsContainerFrame* aParent,
                                  nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::radialGradient),
               "Content is not an SVG radialGradient");

  SVGGradientFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsresult SVGRadialGradientFrame::AttributeChanged(int32_t aNameSpaceID,
                                                  nsAtom* aAttribute,
                                                  int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::r || aAttribute == nsGkAtoms::cx ||
       aAttribute == nsGkAtoms::cy || aAttribute == nsGkAtoms::fx ||
       aAttribute == nsGkAtoms::fy)) {
    SVGObserverUtils::InvalidateRenderingObservers(this);
  }

  return SVGGradientFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

//----------------------------------------------------------------------

float SVGRadialGradientFrame::GetLengthValue(uint32_t aIndex) {
  dom::SVGRadialGradientElement* lengthElement = GetRadialGradientWithLength(
      aIndex, static_cast<dom::SVGRadialGradientElement*>(GetContent()));
  // We passed in mContent as a fallback, so, assuming mContent is non-null,
  // the return value should also be non-null.
  MOZ_ASSERT(lengthElement,
             "Got unexpected null element from GetRadialGradientWithLength");
  return GetLengthValueFromElement(aIndex, *lengthElement);
}

float SVGRadialGradientFrame::GetLengthValue(uint32_t aIndex,
                                             float aDefaultValue) {
  dom::SVGRadialGradientElement* lengthElement =
      GetRadialGradientWithLength(aIndex, nullptr);

  return lengthElement ? GetLengthValueFromElement(aIndex, *lengthElement)
                       : aDefaultValue;
}

float SVGRadialGradientFrame::GetLengthValueFromElement(
    uint32_t aIndex, dom::SVGRadialGradientElement& aElement) {
  const SVGAnimatedLength& length = aElement.mLengthAttributes[aIndex];

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransform, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  uint16_t gradientUnits = GetGradientUnits();
  if (gradientUnits == SVG_UNIT_TYPE_USERSPACEONUSE) {
    return SVGUtils::UserSpace(mSource, &length);
  }

  NS_ASSERTION(gradientUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length.GetAnimValue(static_cast<SVGViewportElement*>(nullptr));
}

dom::SVGRadialGradientElement*
SVGRadialGradientFrame::GetRadialGradientWithLength(
    uint32_t aIndex, dom::SVGRadialGradientElement* aDefault) {
  dom::SVGRadialGradientElement* thisElement =
      static_cast<dom::SVGRadialGradientElement*>(GetContent());
  const SVGAnimatedLength& length = thisElement->mLengthAttributes[aIndex];

  if (length.IsExplicitlySet()) {
    return thisElement;
  }

  return SVGGradientFrame::GetRadialGradientWithLength(aIndex, aDefault);
}

bool SVGRadialGradientFrame::GradientVectorLengthIsZero() {
  float cx = GetLengthValue(dom::SVGRadialGradientElement::ATTR_CX);
  float cy = GetLengthValue(dom::SVGRadialGradientElement::ATTR_CY);
  float r = GetLengthValue(dom::SVGRadialGradientElement::ATTR_R);
  // If fx or fy are not set, use cx/cy instead
  float fx = GetLengthValue(dom::SVGRadialGradientElement::ATTR_FX, cx);
  float fy = GetLengthValue(dom::SVGRadialGradientElement::ATTR_FY, cy);
  float fr = GetLengthValue(dom::SVGRadialGradientElement::ATTR_FR);
  return cx == fx && cy == fy && r == fr;
}

already_AddRefed<gfxPattern> SVGRadialGradientFrame::CreateGradient() {
  float cx = GetLengthValue(dom::SVGRadialGradientElement::ATTR_CX);
  float cy = GetLengthValue(dom::SVGRadialGradientElement::ATTR_CY);
  float r = GetLengthValue(dom::SVGRadialGradientElement::ATTR_R);
  // If fx or fy are not set, use cx/cy instead
  float fx = GetLengthValue(dom::SVGRadialGradientElement::ATTR_FX, cx);
  float fy = GetLengthValue(dom::SVGRadialGradientElement::ATTR_FY, cy);
  float fr = GetLengthValue(dom::SVGRadialGradientElement::ATTR_FR);

  RefPtr<gfxPattern> pattern = new gfxPattern(fx, fy, fr, cx, cy, r);
  return pattern.forget();
}

}  // namespace mozilla

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* NS_NewSVGLinearGradientFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGLinearGradientFrame(aStyle, aPresShell->GetPresContext());
}

nsIFrame* NS_NewSVGRadialGradientFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGRadialGradientFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGLinearGradientFrame)
NS_IMPL_FRAMEARENA_HELPERS(SVGRadialGradientFrame)

}  // namespace mozilla
