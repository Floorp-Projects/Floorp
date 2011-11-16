/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPresContext.h"
#include "nsSVGPathElement.h"
#include "nsSVGUtils.h"
#include "nsSVGGeometryFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "nsContentUtils.h"
#include "gfxContext.h"
#include "nsSVGEffects.h"

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

nsSVGPaintServerFrame *
nsSVGGeometryFrame::GetPaintServer(const nsStyleSVGPaint *aPaint,
                                   const FramePropertyDescriptor *aType)
{
  if (aPaint->mType != eStyleSVGPaintType_Server)
    return nsnull;

  nsIFrame *frame = mContent->IsNodeOfType(nsINode::eTEXT) ?
                      GetParent() : this;
  nsSVGPaintingProperty *property =
    nsSVGEffects::GetPaintingProperty(aPaint->mPaint.mPaintServer, frame, aType);
  if (!property)
    return nsnull;
  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nsnull;

  nsIAtom *type = result->GetType();
  if (type != nsGkAtoms::svgLinearGradientFrame &&
      type != nsGkAtoms::svgRadialGradientFrame &&
      type != nsGkAtoms::svgPatternFrame)
    return nsnull;

  return static_cast<nsSVGPaintServerFrame*>(result);
}

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

nsresult
nsSVGGeometryFrame::GetStrokeDashArray(gfxFloat **aDashes, PRUint32 *aCount)
{
  nsSVGElement *ctx = static_cast<nsSVGElement*>
                                 (mContent->IsNodeOfType(nsINode::eTEXT) ?
                                     mContent->GetParent() : mContent);
  *aDashes = nsnull;
  *aCount = 0;

  PRUint32 count = GetStyleSVG()->mStrokeDasharrayLength;
  gfxFloat *dashes = nsnull;

  if (count) {
    const nsStyleCoord *dasharray = GetStyleSVG()->mStrokeDasharray;
    nsPresContext *presContext = PresContext();
    gfxFloat totalLength = 0.0f;

    gfxFloat pathScale = 1.0;

    if (mContent->Tag() == nsGkAtoms::path) {
      pathScale =
        static_cast<nsSVGPathElement*>(mContent)->GetPathLengthScale();
      if (pathScale <= 0) {
        return NS_OK;
      }
    }

    dashes = new gfxFloat[count];
    if (dashes) {
      for (PRUint32 i = 0; i < count; i++) {
        dashes[i] =
          nsSVGUtils::CoordToFloat(presContext,
                                   ctx,
                                   dasharray[i]) * pathScale;
        if (dashes[i] < 0.0f) {
          delete [] dashes;
          return NS_OK;
        }
        totalLength += dashes[i];
      }
    } else {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (totalLength == 0.0f) {
      delete [] dashes;
      return NS_OK;
    }

    *aDashes = dashes;
    *aCount = count;
  }

  return NS_OK;
}

float
nsSVGGeometryFrame::GetStrokeDashoffset()
{
  nsSVGElement *ctx = static_cast<nsSVGElement*>
                                 (mContent->IsNodeOfType(nsINode::eTEXT) ?
                                     mContent->GetParent() : mContent);

  return
    nsSVGUtils::CoordToFloat(PresContext(),
                             ctx,
                             GetStyleSVG()->mStrokeDashoffset);
}

PRUint16
nsSVGGeometryFrame::GetClipRule()
{
  return GetStyleSVG()->mClipRule;
}

static void
SetupCairoColor(gfxContext *aContext, nscolor aRGB, float aOpacity)
{
  aContext->SetColor(gfxRGBA(NS_GET_R(aRGB)/255.0,
                             NS_GET_G(aRGB)/255.0,
                             NS_GET_B(aRGB)/255.0,
                             NS_GET_A(aRGB)/255.0 * aOpacity));
}

static void
SetupFallbackOrPaintColor(gfxContext *aContext, nsStyleContext *aStyleContext,
                          nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                          float aOpacity)
{
  const nsStyleSVGPaint &paint = aStyleContext->GetStyleSVG()->*aFillOrStroke;
  nsStyleContext *styleIfVisited = aStyleContext->GetStyleIfVisited();
  bool isServer = paint.mType == eStyleSVGPaintType_Server;
  nscolor color = isServer ? paint.mFallbackColor : paint.mPaint.mColor;
  if (styleIfVisited) {
    const nsStyleSVGPaint &paintIfVisited =
      styleIfVisited->GetStyleSVG()->*aFillOrStroke;
    // To prevent Web content from detecting if a user has visited a URL
    // (via URL loading triggered by paint servers or performance
    // differences between paint servers or between a paint server and a
    // color), we do not allow whether links are visited to change which
    // paint server is used or switch between paint servers and simple
    // colors.  A :visited style may only override a simple color with
    // another simple color.
    if (paintIfVisited.mType == eStyleSVGPaintType_Color &&
        paint.mType == eStyleSVGPaintType_Color) {
      nscolor colorIfVisited = paintIfVisited.mPaint.mColor;
      nscolor colors[2] = { color, colorIfVisited };
      color = nsStyleContext::CombineVisitedColors(colors,
                                         aStyleContext->RelevantLinkVisited());
    }
  }

  SetupCairoColor(aContext, color, aOpacity);
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
nsSVGGeometryFrame::SetupCairoFill(gfxContext *aContext)
{
  const nsStyleSVG* style = GetStyleSVG();
  if (style->mFill.mType == eStyleSVGPaintType_None)
    return false;

  if (style->mFillRule == NS_STYLE_FILL_RULE_EVENODD)
    aContext->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
  else
    aContext->SetFillRule(gfxContext::FILL_RULE_WINDING);

  float opacity = MaybeOptimizeOpacity(style->mFillOpacity);

  nsSVGPaintServerFrame *ps =
    GetPaintServer(&style->mFill, nsSVGEffects::FillProperty());
  if (ps && ps->SetupPaintServer(aContext, this, opacity))
    return true;

  // On failure, use the fallback colour in case we have an
  // objectBoundingBox where the width or height of the object is zero.
  // See http://www.w3.org/TR/SVG11/coords.html#ObjectBoundingBox
  SetupFallbackOrPaintColor(aContext, GetStyleContext(),
                            &nsStyleSVG::mFill, opacity);

  return true;
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

  gfxFloat *dashArray;
  PRUint32 count;
  GetStrokeDashArray(&dashArray, &count);
  if (count > 0) {
    aContext->SetDash(dashArray, count, GetStrokeDashoffset());
    delete [] dashArray;
  }
}

bool
nsSVGGeometryFrame::SetupCairoStroke(gfxContext *aContext)
{
  if (!HasStroke()) {
    return false;
  }
  SetupCairoStrokeHitGeometry(aContext);

  const nsStyleSVG* style = GetStyleSVG();
  float opacity = MaybeOptimizeOpacity(style->mStrokeOpacity);

  nsSVGPaintServerFrame *ps =
    GetPaintServer(&style->mStroke, nsSVGEffects::StrokeProperty());
  if (ps && ps->SetupPaintServer(aContext, this, opacity))
    return true;

  // On failure, use the fallback colour in case we have an
  // objectBoundingBox where the width or height of the object is zero.
  // See http://www.w3.org/TR/SVG11/coords.html#ObjectBoundingBox
  SetupFallbackOrPaintColor(aContext, GetStyleContext(),
                            &nsStyleSVG::mStroke, opacity);

  return true;
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
