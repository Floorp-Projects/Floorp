/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGGradientFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxPattern.h"
#include "nsContentUtils.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsIDOMSVGStopElement.h"
#include "nsSVGEffects.h"
#include "nsSVGGradientElement.h"
#include "SVGAnimatedTransformList.h"

using mozilla::SVGAnimatedTransformList;

//----------------------------------------------------------------------
// Helper classes

class nsSVGGradientFrame::AutoGradientReferencer
{
public:
  AutoGradientReferencer(nsSVGGradientFrame *aFrame)
    : mFrame(aFrame)
  {
    // Reference loops should normally be detected in advance and handled, so
    // we're not expecting to encounter them here
    NS_ABORT_IF_FALSE(!mFrame->mLoopFlag, "Undetected reference loop!");
    mFrame->mLoopFlag = true;
  }
  ~AutoGradientReferencer() {
    mFrame->mLoopFlag = false;
  }
private:
  nsSVGGradientFrame *mFrame;
};

//----------------------------------------------------------------------
// Implementation

nsSVGGradientFrame::nsSVGGradientFrame(nsStyleContext* aContext) :
  nsSVGGradientFrameBase(aContext),
  mLoopFlag(false),
  mNoHRefURI(false)
{
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGGradientFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

/* virtual */ void
nsSVGGradientFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsSVGEffects::InvalidateDirectRenderingObservers(this);
  nsSVGGradientFrameBase::DidSetStyleContext(aOldStyleContext);
}

NS_IMETHODIMP
nsSVGGradientFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::gradientUnits ||
       aAttribute == nsGkAtoms::gradientTransform ||
       aAttribute == nsGkAtoms::spreadMethod)) {
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  } else if (aNameSpaceID == kNameSpaceID_XLink &&
             aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    Properties().Delete(nsSVGEffects::HrefProperty());
    mNoHRefURI = false;
    // And update whoever references us
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }

  return nsSVGGradientFrameBase::AttributeChanged(aNameSpaceID,
                                                  aAttribute, aModType);
}

//----------------------------------------------------------------------

PRUint32
nsSVGGradientFrame::GetStopCount()
{
  return GetStopFrame(-1, nsnull);
}

void
nsSVGGradientFrame::GetStopInformation(PRInt32 aIndex,
                                       float *aOffset,
                                       nscolor *aStopColor,
                                       float *aStopOpacity)
{
  *aOffset = 0.0f;
  *aStopColor = NS_RGBA(0, 0, 0, 0);
  *aStopOpacity = 1.0f;

  nsIFrame *stopFrame = nsnull;
  GetStopFrame(aIndex, &stopFrame);
  nsCOMPtr<nsIDOMSVGStopElement> stopElement =
    do_QueryInterface(stopFrame->GetContent());

  if (stopElement) {
    nsCOMPtr<nsIDOMSVGAnimatedNumber> aNum;
    stopElement->GetOffset(getter_AddRefs(aNum));

    aNum->GetAnimVal(aOffset);
    if (*aOffset < 0.0f)
      *aOffset = 0.0f;
    else if (*aOffset > 1.0f)
      *aOffset = 1.0f;
  }

  *aStopColor   = stopFrame->GetStyleSVGReset()->mStopColor;
  *aStopOpacity = stopFrame->GetStyleSVGReset()->mStopOpacity;
}

PRUint16
nsSVGGradientFrame::GetEnumValue(PRUint32 aIndex, nsIContent *aDefault)
{
  const nsSVGEnum& thisEnum =
    static_cast<nsSVGGradientElement *>(mContent)->mEnumAttributes[aIndex];

  if (thisEnum.IsExplicitlySet())
    return thisEnum.GetAnimValue();

  AutoGradientReferencer gradientRef(this);

  nsSVGGradientFrame *next = GetReferencedGradientIfNotInUse();
  return next ? next->GetEnumValue(aIndex, aDefault) :
    static_cast<nsSVGGradientElement *>(aDefault)->
      mEnumAttributes[aIndex].GetAnimValue();
}

PRUint16
nsSVGGradientFrame::GetGradientUnits()
{
  // This getter is called every time the others are called - maybe cache it?
  return GetEnumValue(nsSVGGradientElement::GRADIENTUNITS);
}

PRUint16
nsSVGGradientFrame::GetSpreadMethod()
{
  return GetEnumValue(nsSVGGradientElement::SPREADMETHOD);
}

const SVGAnimatedTransformList*
nsSVGGradientFrame::GetGradientTransformList(nsIContent* aDefault)
{
  SVGAnimatedTransformList *thisTransformList =
    static_cast<nsSVGGradientElement *>(mContent)->GetAnimatedTransformList();

  if (thisTransformList->IsExplicitlySet())
    return thisTransformList;

  AutoGradientReferencer gradientRef(this);

  nsSVGGradientFrame *next = GetReferencedGradientIfNotInUse();
  return next ? next->GetGradientTransformList(aDefault) :
    static_cast<const nsSVGGradientElement *>(aDefault)
      ->mGradientTransform.get();
}

gfxMatrix
nsSVGGradientFrame::GetGradientTransform(nsIFrame *aSource,
                                         const gfxRect *aOverrideBounds)
{
  gfxMatrix bboxMatrix;

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
    // If this gradient is applied to text, our caller
    // will be the glyph, which is not a container, so we
    // need to get the parent
    if (aSource->GetContent()->IsNodeOfType(nsINode::eTEXT))
      mSource = aSource->GetParent();
    else
      mSource = aSource;
  } else {
    NS_ASSERTION(
      gradientUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX,
      "Unknown gradientUnits type");
    // objectBoundingBox is the default anyway

    gfxRect bbox =
      aOverrideBounds ? *aOverrideBounds : nsSVGUtils::GetBBox(aSource);
    bboxMatrix =
      gfxMatrix(bbox.Width(), 0, 0, bbox.Height(), bbox.X(), bbox.Y());
  }

  const SVGAnimatedTransformList* animTransformList =
    GetGradientTransformList(mContent);
  if (!animTransformList)
    return bboxMatrix;

  gfxMatrix gradientTransform =
    animTransformList->GetAnimValue().GetConsolidationMatrix();
  return bboxMatrix.PreMultiply(gradientTransform);
}

nsSVGLinearGradientElement *
nsSVGGradientFrame::GetLinearGradientWithLength(PRUint32 aIndex,
  nsSVGLinearGradientElement* aDefault)
{
  // If this was a linear gradient with the required length, we would have
  // already found it in nsSVGLinearGradientFrame::GetLinearGradientWithLength.
  // Since we didn't find the length, continue looking down the chain.

  AutoGradientReferencer gradientRef(this);

  nsSVGGradientFrame *next = GetReferencedGradientIfNotInUse();
  return next ? next->GetLinearGradientWithLength(aIndex, aDefault) : aDefault;
}

nsSVGRadialGradientElement *
nsSVGGradientFrame::GetRadialGradientWithLength(PRUint32 aIndex,
  nsSVGRadialGradientElement* aDefault)
{
  // If this was a radial gradient with the required length, we would have
  // already found it in nsSVGRadialGradientFrame::GetRadialGradientWithLength.
  // Since we didn't find the length, continue looking down the chain.

  AutoGradientReferencer gradientRef(this);

  nsSVGGradientFrame *next = GetReferencedGradientIfNotInUse();
  return next ? next->GetRadialGradientWithLength(aIndex, aDefault) : aDefault;
}

//----------------------------------------------------------------------
// nsSVGPaintServerFrame methods:

already_AddRefed<gfxPattern>
nsSVGGradientFrame::GetPaintServerPattern(nsIFrame *aSource,
                                          const gfxMatrix& aContextMatrix,
                                          nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                          float aGraphicOpacity,
                                          const gfxRect *aOverrideBounds)
{
  // Get the transform list (if there is one)
  gfxMatrix patternMatrix = GetGradientTransform(aSource, aOverrideBounds);

  if (patternMatrix.IsSingular())
    return nsnull;

  PRUint32 nStops = GetStopCount();

  // SVG specification says that no stops should be treated like
  // the corresponding fill or stroke had "none" specified.
  if (nStops == 0) {
    nsRefPtr<gfxPattern> pattern = new gfxPattern(gfxRGBA(0, 0, 0, 0));
    return pattern.forget();
  }

  // revert the vector effect transform so that the gradient appears unchanged
  if (aFillOrStroke == &nsStyleSVG::mStroke) {
    patternMatrix.Multiply(nsSVGUtils::GetStrokeTransform(aSource).Invert());
  }

  patternMatrix.Invert();

  nsRefPtr<gfxPattern> gradient = CreateGradient();
  if (!gradient || gradient->CairoStatus())
    return nsnull;

  PRUint16 aSpread = GetSpreadMethod();
  if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD)
    gradient->SetExtend(gfxPattern::EXTEND_PAD);
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT)
    gradient->SetExtend(gfxPattern::EXTEND_REFLECT);
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT)
    gradient->SetExtend(gfxPattern::EXTEND_REPEAT);

  gradient->SetMatrix(patternMatrix);

  // setup stops
  float lastOffset = 0.0f;

  for (PRUint32 i = 0; i < nStops; i++) {
    float offset, stopOpacity;
    nscolor stopColor;

    GetStopInformation(i, &offset, &stopColor, &stopOpacity);

    if (offset < lastOffset)
      offset = lastOffset;
    else
      lastOffset = offset;

    gradient->AddColorStop(offset,
                           gfxRGBA(NS_GET_R(stopColor)/255.0,
                                   NS_GET_G(stopColor)/255.0,
                                   NS_GET_B(stopColor)/255.0,
                                   NS_GET_A(stopColor)/255.0 *
                                     stopOpacity * aGraphicOpacity));
  }

  return gradient.forget();
}

// Private (helper) methods

nsSVGGradientFrame *
nsSVGGradientFrame::GetReferencedGradient()
{
  if (mNoHRefURI)
    return nsnull;

  nsSVGPaintingProperty *property = static_cast<nsSVGPaintingProperty*>
    (Properties().Get(nsSVGEffects::HrefProperty()));

  if (!property) {
    // Fetch our gradient element's xlink:href attribute
    nsSVGGradientElement *grad = static_cast<nsSVGGradientElement *>(mContent);
    nsAutoString href;
    grad->mStringAttributes[nsSVGGradientElement::HREF].GetAnimValue(href, grad);
    if (href.IsEmpty()) {
      mNoHRefURI = true;
      return nsnull; // no URL
    }

    // Convert href to an nsIURI
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetCurrentDoc(), base);

    property =
      nsSVGEffects::GetPaintingProperty(targetURI, this, nsSVGEffects::HrefProperty());
    if (!property)
      return nsnull;
  }

  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nsnull;

  nsIAtom* frameType = result->GetType();
  if (frameType != nsGkAtoms::svgLinearGradientFrame &&
      frameType != nsGkAtoms::svgRadialGradientFrame)
    return nsnull;

  return static_cast<nsSVGGradientFrame*>(result);
}

nsSVGGradientFrame *
nsSVGGradientFrame::GetReferencedGradientIfNotInUse()
{
  nsSVGGradientFrame *referenced = GetReferencedGradient();
  if (!referenced)
    return nsnull;

  if (referenced->mLoopFlag) {
    // XXXjwatt: we should really send an error to the JavaScript Console here:
    NS_WARNING("gradient reference loop detected while inheriting attribute!");
    return nsnull;
  }

  return referenced;
}

PRInt32
nsSVGGradientFrame::GetStopFrame(PRInt32 aIndex, nsIFrame * *aStopFrame)
{
  PRInt32 stopCount = 0;
  nsIFrame *stopFrame = nsnull;
  for (stopFrame = mFrames.FirstChild(); stopFrame;
       stopFrame = stopFrame->GetNextSibling()) {
    if (stopFrame->GetType() == nsGkAtoms::svgStopFrame) {
      // Is this the one we're looking for?
      if (stopCount++ == aIndex)
        break; // Yes, break out of the loop
    }
  }
  if (stopCount > 0) {
    if (aStopFrame)
      *aStopFrame = stopFrame;
    return stopCount;
  }

  // Our gradient element doesn't have stops - try to "inherit" them

  AutoGradientReferencer gradientRef(this);
  nsSVGGradientFrame* next = GetReferencedGradientIfNotInUse();
  if (!next)
    return nsnull;

  return next->GetStopFrame(aIndex, aStopFrame);
}

// -------------------------------------------------------------------------
// Linear Gradients
// -------------------------------------------------------------------------

#ifdef DEBUG
NS_IMETHODIMP
nsSVGLinearGradientFrame::Init(nsIContent* aContent,
                               nsIFrame* aParent,
                               nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "Content is not an SVG linearGradient");

  return nsSVGLinearGradientFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom*
nsSVGLinearGradientFrame::GetType() const
{
  return nsGkAtoms::svgLinearGradientFrame;
}

NS_IMETHODIMP
nsSVGLinearGradientFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                           nsIAtom*        aAttribute,
                                           PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x1 ||
       aAttribute == nsGkAtoms::y1 ||
       aAttribute == nsGkAtoms::x2 ||
       aAttribute == nsGkAtoms::y2)) {
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }

  return nsSVGGradientFrame::AttributeChanged(aNameSpaceID,
                                              aAttribute, aModType);
}

//----------------------------------------------------------------------

float
nsSVGLinearGradientFrame::GetLengthValue(PRUint32 aIndex)
{
  nsSVGLinearGradientElement* lengthElement =
    GetLinearGradientWithLength(aIndex,
      static_cast<nsSVGLinearGradientElement *>(mContent));
  // We passed in mContent as a fallback, so, assuming mContent is non-null, the
  // return value should also be non-null.
  NS_ABORT_IF_FALSE(lengthElement,
    "Got unexpected null element from GetLinearGradientWithLength");
  const nsSVGLength2 &length = lengthElement->mLengthAttributes[aIndex];

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransform, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
    return nsSVGUtils::UserSpace(mSource, &length);
  }

  NS_ASSERTION(
    gradientUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX,
    "Unknown gradientUnits type");

  return length.GetAnimValue(static_cast<nsSVGSVGElement*>(nsnull));
}

nsSVGLinearGradientElement *
nsSVGLinearGradientFrame::GetLinearGradientWithLength(PRUint32 aIndex,
  nsSVGLinearGradientElement* aDefault)
{
  nsSVGLinearGradientElement* thisElement =
    static_cast<nsSVGLinearGradientElement *>(mContent);
  const nsSVGLength2 &length = thisElement->mLengthAttributes[aIndex];

  if (length.IsExplicitlySet()) {
    return thisElement;
  }

  return nsSVGLinearGradientFrameBase::GetLinearGradientWithLength(aIndex,
                                                                   aDefault);
}

already_AddRefed<gfxPattern>
nsSVGLinearGradientFrame::CreateGradient()
{
  float x1, y1, x2, y2;

  x1 = GetLengthValue(nsSVGLinearGradientElement::X1);
  y1 = GetLengthValue(nsSVGLinearGradientElement::Y1);
  x2 = GetLengthValue(nsSVGLinearGradientElement::X2);
  y2 = GetLengthValue(nsSVGLinearGradientElement::Y2);

  gfxPattern *pattern = new gfxPattern(x1, y1, x2, y2);
  NS_IF_ADDREF(pattern);
  return pattern;
}

// -------------------------------------------------------------------------
// Radial Gradients
// -------------------------------------------------------------------------

#ifdef DEBUG
NS_IMETHODIMP
nsSVGRadialGradientFrame::Init(nsIContent* aContent,
                               nsIFrame* aParent,
                               nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "Content is not an SVG radialGradient");

  return nsSVGRadialGradientFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom*
nsSVGRadialGradientFrame::GetType() const
{
  return nsGkAtoms::svgRadialGradientFrame;
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                           nsIAtom*        aAttribute,
                                           PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::r ||
       aAttribute == nsGkAtoms::cx ||
       aAttribute == nsGkAtoms::cy ||
       aAttribute == nsGkAtoms::fx ||
       aAttribute == nsGkAtoms::fy)) {
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }

  return nsSVGGradientFrame::AttributeChanged(aNameSpaceID,
                                              aAttribute, aModType);
}

//----------------------------------------------------------------------

float
nsSVGRadialGradientFrame::GetLengthValue(PRUint32 aIndex)
{
  nsSVGRadialGradientElement* lengthElement =
    GetRadialGradientWithLength(aIndex,
      static_cast<nsSVGRadialGradientElement *>(mContent));
  // We passed in mContent as a fallback, so, assuming mContent is non-null,
  // the return value should also be non-null.
  NS_ABORT_IF_FALSE(lengthElement,
    "Got unexpected null element from GetRadialGradientWithLength");
  return GetLengthValueFromElement(aIndex, *lengthElement);
}

float
nsSVGRadialGradientFrame::GetLengthValue(PRUint32 aIndex, float aDefaultValue)
{
  nsSVGRadialGradientElement* lengthElement =
    GetRadialGradientWithLength(aIndex, nsnull);

  return lengthElement ? GetLengthValueFromElement(aIndex, *lengthElement)
                       : aDefaultValue;
}

float
nsSVGRadialGradientFrame::GetLengthValueFromElement(PRUint32 aIndex,
  nsSVGRadialGradientElement& aElement)
{
  const nsSVGLength2 &length = aElement.mLengthAttributes[aIndex];

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransform, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
    return nsSVGUtils::UserSpace(mSource, &length);
  }

  NS_ASSERTION(
    gradientUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX,
    "Unknown gradientUnits type");

  return length.GetAnimValue(static_cast<nsSVGSVGElement*>(nsnull));
}

nsSVGRadialGradientElement *
nsSVGRadialGradientFrame::GetRadialGradientWithLength(PRUint32 aIndex,
  nsSVGRadialGradientElement* aDefault)
{
  nsSVGRadialGradientElement* thisElement =
    static_cast<nsSVGRadialGradientElement *>(mContent);
  const nsSVGLength2 &length = thisElement->mLengthAttributes[aIndex];

  if (length.IsExplicitlySet()) {
    return thisElement;
  }

  return nsSVGRadialGradientFrameBase::GetRadialGradientWithLength(aIndex,
                                                                   aDefault);
}

already_AddRefed<gfxPattern>
nsSVGRadialGradientFrame::CreateGradient()
{
  float cx, cy, r, fx, fy;

  cx = GetLengthValue(nsSVGRadialGradientElement::CX);
  cy = GetLengthValue(nsSVGRadialGradientElement::CY);
  r  = GetLengthValue(nsSVGRadialGradientElement::R);
  // If fx or fy are not set, use cx/cy instead
  fx = GetLengthValue(nsSVGRadialGradientElement::FX, cx);
  fy = GetLengthValue(nsSVGRadialGradientElement::FY, cy);

  if (fx != cx || fy != cy) {
    // The focal point (fFx and fFy) must be clamped to be *inside* - not on -
    // the circumference of the gradient or we'll get rendering anomalies. We
    // calculate the distance from the focal point to the gradient center and
    // make sure it is *less* than the gradient radius.
    // 1/128 is the limit of the fractional part of cairo's 24.8 fixed point
    // representation divided by 2 to ensure that we get different cairo
    // fractions
    double dMax = NS_MAX(0.0, r - 1.0/128);
    float dx = fx - cx;
    float dy = fy - cy;
    double d = sqrt((dx * dx) + (dy * dy));
    if (d > dMax) {
      double angle = atan2(dy, dx);
      fx = (float)(dMax * cos(angle)) + cx;
      fy = (float)(dMax * sin(angle)) + cy;
    }
  }

  gfxPattern *pattern = new gfxPattern(fx, fy, 0, cx, cy, r);
  NS_IF_ADDREF(pattern);
  return pattern;
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame*
NS_NewSVGLinearGradientFrame(nsIPresShell*   aPresShell,
                             nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGLinearGradientFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGLinearGradientFrame)

nsIFrame*
NS_NewSVGRadialGradientFrame(nsIPresShell*   aPresShell,
                             nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGRadialGradientFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGRadialGradientFrame)
