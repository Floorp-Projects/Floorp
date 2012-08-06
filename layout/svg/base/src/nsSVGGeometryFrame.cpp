/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGGeometryFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "nsPresContext.h"
#include "nsSVGEffects.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGPathElement.h"
#include "nsSVGUtils.h"

NS_IMPL_FRAMEARENA_HELPERS(nsSVGGeometryFrame)

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGGeometryFrame::Init(nsIContent* aContent,
                         nsIFrame* aParent,
                         nsIFrame* aPrevInFlow)
{
  AddStateBits(aParent->GetStateBits() &
               (NS_STATE_SVG_NONDISPLAY_CHILD | NS_STATE_SVG_CLIPPATH_CHILD));
  nsresult rv = nsSVGGeometryFrameBase::Init(aContent, aParent, aPrevInFlow);
  return rv;
}

//----------------------------------------------------------------------

float
nsSVGGeometryFrame::GetStrokeWidth()
{
  nsSVGElement *ctx = static_cast<nsSVGElement*>
                                 (mContent->IsNodeOfType(nsINode::eTEXT) ?
                                     mContent->GetParent() : mContent);

  return
    nsSVGUtils::CoordToFloat(PresContext(),
                             ctx,
                             GetStyleSVG()->mStrokeWidth);
}

bool
nsSVGGeometryFrame::GetStrokeDashData(FallibleTArray<gfxFloat>& dashes,
                                      gfxFloat *dashOffset)
{
  PRUint32 count = GetStyleSVG()->mStrokeDasharrayLength;
  if (!count || !dashes.SetLength(count)) {
    return false;
  }

  gfxFloat pathScale = 1.0;

  if (mContent->Tag() == nsGkAtoms::path) {
    pathScale = static_cast<nsSVGPathElement*>(mContent)->
                  GetPathLengthScale(nsSVGPathElement::eForStroking);
    if (pathScale <= 0) {
      return false;
    }
  }
  
  nsSVGElement *ctx = static_cast<nsSVGElement*>
                                 (mContent->IsNodeOfType(nsINode::eTEXT) ?
                                     mContent->GetParent() : mContent);

  const nsStyleCoord *dasharray = GetStyleSVG()->mStrokeDasharray;
  nsPresContext *presContext = PresContext();
  gfxFloat totalLength = 0.0;

  for (PRUint32 i = 0; i < count; i++) {
    dashes[i] =
      nsSVGUtils::CoordToFloat(presContext,
                               ctx,
                               dasharray[i]) * pathScale;
    if (dashes[i] < 0.0) {
      return false;
    }
    totalLength += dashes[i];
  }

  *dashOffset = nsSVGUtils::CoordToFloat(presContext,
                                         ctx,
                                         GetStyleSVG()->mStrokeDashoffset);

  return (totalLength > 0.0);
}

PRUint16
nsSVGGeometryFrame::GetClipRule()
{
  return GetStyleSVG()->mClipRule;
}

float
nsSVGGeometryFrame::MaybeOptimizeOpacity(float aFillOrStrokeOpacity)
{
  float opacity = GetStyleDisplay()->mOpacity;
  if (opacity < 1 && nsSVGUtils::CanOptimizeOpacity(this)) {
    return aFillOrStrokeOpacity * opacity;
  }
  return aFillOrStrokeOpacity;
}

bool
nsSVGGeometryFrame::HasStroke()
{
  const nsStyleSVG *style = GetStyleSVG();
  return style->mStroke.mType != eStyleSVGPaintType_None &&
         style->mStrokeOpacity > 0 &&
         GetStrokeWidth() > 0;
}

void
nsSVGGeometryFrame::SetupCairoStrokeGeometry(gfxContext *aContext)
{
  float width = GetStrokeWidth();
  if (width <= 0)
    return;
  aContext->SetLineWidth(width);

  // Apply any stroke-specific transform
  aContext->Multiply(nsSVGUtils::GetStrokeTransform(this));

  const nsStyleSVG* style = GetStyleSVG();
  
  switch (style->mStrokeLinecap) {
  case NS_STYLE_STROKE_LINECAP_BUTT:
    aContext->SetLineCap(gfxContext::LINE_CAP_BUTT);
    break;
  case NS_STYLE_STROKE_LINECAP_ROUND:
    aContext->SetLineCap(gfxContext::LINE_CAP_ROUND);
    break;
  case NS_STYLE_STROKE_LINECAP_SQUARE:
    aContext->SetLineCap(gfxContext::LINE_CAP_SQUARE);
    break;
  }

  aContext->SetMiterLimit(style->mStrokeMiterlimit);

  switch (style->mStrokeLinejoin) {
  case NS_STYLE_STROKE_LINEJOIN_MITER:
    aContext->SetLineJoin(gfxContext::LINE_JOIN_MITER);
    break;
  case NS_STYLE_STROKE_LINEJOIN_ROUND:
    aContext->SetLineJoin(gfxContext::LINE_JOIN_ROUND);
    break;
  case NS_STYLE_STROKE_LINEJOIN_BEVEL:
    aContext->SetLineJoin(gfxContext::LINE_JOIN_BEVEL);
    break;
  }
}

void
nsSVGGeometryFrame::SetupCairoStrokeHitGeometry(gfxContext *aContext)
{
  SetupCairoStrokeGeometry(aContext);

  AutoFallibleTArray<gfxFloat, 10> dashes;
  gfxFloat dashOffset;
  if (GetStrokeDashData(dashes, &dashOffset)) {
    aContext->SetDash(dashes.Elements(), dashes.Length(), dashOffset);
  }
}

bool
nsSVGGeometryFrame::SetupCairoFill(gfxContext *aContext)
{
  return nsSVGUtils::SetupCairoFill(aContext, this);
}

bool
nsSVGGeometryFrame::SetupCairoStroke(gfxContext *aContext)
{
  if (!HasStroke()) {
    return false;
  }
  SetupCairoStrokeHitGeometry(aContext);

  return nsSVGUtils::SetupCairoStroke(aContext, this);
}

PRUint16
nsSVGGeometryFrame::GetHitTestFlags()
{
  PRUint16 flags = 0;

  switch(GetStyleVisibility()->mPointerEvents) {
  case NS_STYLE_POINTER_EVENTS_NONE:
    break;
  case NS_STYLE_POINTER_EVENTS_AUTO:
  case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
    if (GetStyleVisibility()->IsVisible()) {
      if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None)
        flags |= SVG_HIT_TEST_FILL;
      if (GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
        flags |= SVG_HIT_TEST_STROKE;
      if (GetStyleSVG()->mStrokeOpacity > 0)
        flags |= SVG_HIT_TEST_CHECK_MRECT;
    }
    break;
  case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
    if (GetStyleVisibility()->IsVisible()) {
      flags |= SVG_HIT_TEST_FILL;
    }
    break;
  case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
    if (GetStyleVisibility()->IsVisible()) {
      flags |= SVG_HIT_TEST_STROKE;
    }
    break;
  case NS_STYLE_POINTER_EVENTS_VISIBLE:
    if (GetStyleVisibility()->IsVisible()) {
      flags |= SVG_HIT_TEST_FILL | SVG_HIT_TEST_STROKE;
    }
    break;
  case NS_STYLE_POINTER_EVENTS_PAINTED:
    if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None)
      flags |= SVG_HIT_TEST_FILL;
    if (GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
      flags |= SVG_HIT_TEST_STROKE;
    if (GetStyleSVG()->mStrokeOpacity)
      flags |= SVG_HIT_TEST_CHECK_MRECT;
    break;
  case NS_STYLE_POINTER_EVENTS_FILL:
    flags |= SVG_HIT_TEST_FILL;
    break;
  case NS_STYLE_POINTER_EVENTS_STROKE:
    flags |= SVG_HIT_TEST_STROKE;
    break;
  case NS_STYLE_POINTER_EVENTS_ALL:
    flags |= SVG_HIT_TEST_FILL | SVG_HIT_TEST_STROKE;
    break;
  default:
    NS_ERROR("not reached");
    break;
  }

  return flags;
}
