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
#include "nsSVGUtils.h"
#include "nsSVGGeometryFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "nsContentUtils.h"
#include "gfxContext.h"

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGeometryFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGeometryFrameBase)

//----------------------------------------------------------------------

nsSVGGeometryFrame::nsSVGGeometryFrame(nsStyleContext* aContext)
  : nsSVGGeometryFrameBase(aContext)
{
}

void
nsSVGGeometryFrame::Destroy()
{
  // Remove the properties before the frame goes away, since we need it for QI
  RemovePaintServerProperties();
  nsSVGGeometryFrameBase::Destroy();
}

void
nsSVGGeometryFrame::RemovePaintServerProperties()
{
  DeleteProperty(nsGkAtoms::fill);
  DeleteProperty(nsGkAtoms::stroke);
  RemoveStateBits(NS_STATE_SVG_PSERVER_MASK);
}

nsSVGPaintServerFrame *
nsSVGGeometryFrame::GetPaintServer(const nsStyleSVGPaint *aPaint)
{
  if (aPaint->mType != eStyleSVGPaintType_Server)
    return nsnull;

  nsIURI *uri;
  uri = aPaint->mPaint.mPaintServer;
  if (!uri)
    return nsnull;

  nsIFrame *result;
  if (NS_FAILED(nsSVGUtils::GetReferencedFrame(&result, uri, mContent,
                                               PresContext()->PresShell())))
    return nsnull;

  nsIAtom *type = result->GetType();
  if (type != nsGkAtoms::svgLinearGradientFrame &&
      type != nsGkAtoms::svgRadialGradientFrame &&
      type != nsGkAtoms::svgPatternFrame)
    return nsnull;

  // Loop check for pattern
  if (type == nsGkAtoms::svgPatternFrame &&
      nsContentUtils::ContentIsDescendantOf(mContent, result->GetContent()))
    return nsnull;

  nsSVGPaintServerFrame *server =
    NS_STATIC_CAST(nsSVGPaintServerFrame*, result);

  server->AddObserver(this);
  return server;
}

NS_IMETHODIMP
nsSVGGeometryFrame::InitSVG()
{
  AddStateBits(mParent->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGeometryFrame::Init(nsIContent* aContent,
                         nsIFrame* aParent,
                         nsIFrame* aPrevInFlow)
{
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  if (mContent) {
    mContent->SetMayHaveFrame(PR_TRUE);
  }

  InitSVG();
  DidSetStyleContext();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGeometryFrame::DidSetStyleContext()
{
  // One of the styles that might have been changed are the urls that
  // point to gradients, etc.  Drop our cached values to those
  RemovePaintServerProperties();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGeometryFrame::WillModifySVGObservable(nsISVGValue* observable,
					   nsISVGValue::modificationType aModType)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGeometryFrame::DidModifySVGObservable(nsISVGValue* observable,
					   nsISVGValue::modificationType aModType)
{
  if (!(GetStateBits() & NS_STATE_SVG_PSERVER_MASK))
    return NS_OK;

  nsIFrame *frame;
  CallQueryInterface(observable, &frame);

  if (!frame)
    return NS_OK;

  if (GetStateBits() & NS_STATE_SVG_FILL_PSERVER) {
    nsIFrame *ps = NS_STATIC_CAST(nsIFrame*, GetProperty(nsGkAtoms::fill));
    if (frame == ps) {
      if (aModType == nsISVGValue::mod_die) {
        DeleteProperty(nsGkAtoms::fill);
        RemoveStateBits(NS_STATE_SVG_FILL_PSERVER);
      }
      UpdateGraphic();
    }
  }

  if (GetStateBits() & NS_STATE_SVG_STROKE_PSERVER) {
    nsIFrame *ps = NS_STATIC_CAST(nsIFrame*, GetProperty(nsGkAtoms::stroke));
    if (frame == ps) {
      if (aModType == nsISVGValue::mod_die) {
        DeleteProperty(nsGkAtoms::stroke);
        RemoveStateBits(NS_STATE_SVG_STROKE_PSERVER);
      }
      UpdateGraphic();
    }
  }

  return NS_OK;
}


//----------------------------------------------------------------------

float
nsSVGGeometryFrame::GetStrokeWidth()
{
  nsSVGElement *ctx = NS_STATIC_CAST(nsSVGElement*,
                                     GetType() == nsGkAtoms::svgGlyphFrame ?
                                     mContent->GetParent() : mContent);

  return
    nsSVGUtils::CoordToFloat(PresContext(),
                             ctx,
                             GetStyleSVG()->mStrokeWidth);
}

nsresult
nsSVGGeometryFrame::GetStrokeDashArray(gfxFloat **aDashes, PRUint32 *aCount)
{
  *aDashes = nsnull;
  *aCount = 0;

  PRUint32 count = GetStyleSVG()->mStrokeDasharrayLength;
  gfxFloat *dashes = nsnull;

  if (count) {
    const nsStyleCoord *dasharray = GetStyleSVG()->mStrokeDasharray;
    nsPresContext *presContext = PresContext();
    gfxFloat totalLength = 0.0f;

    dashes = new gfxFloat[count];
    if (dashes) {
      for (PRUint32 i = 0; i < count; i++) {
        dashes[i] =
          nsSVGUtils::CoordToFloat(presContext,
                                   NS_STATIC_CAST(nsSVGElement*, mContent),
                                   dasharray[i]);
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
  return
    nsSVGUtils::CoordToFloat(PresContext(),
                             NS_STATIC_CAST(nsSVGElement*, mContent),
                             GetStyleSVG()->mStrokeDashoffset);
}

PRUint16
nsSVGGeometryFrame::GetClipRule()
{
  return GetStyleSVG()->mClipRule;
}

static void
PServerPropertyDtor(void *aObject, nsIAtom *aPropertyName,
                    void *aPropertyValue, void *aData)
{
  nsIFrame *ps = NS_STATIC_CAST(nsIFrame*, aPropertyValue);
  nsSVGUtils::RemoveObserver(NS_STATIC_CAST(nsIFrame*, aObject), ps);
}

PRBool
nsSVGGeometryFrame::HasStroke()
{
  if (!(GetStateBits() & NS_STATE_SVG_STROKE_PSERVER)) {
    nsIFrame *ps = GetPaintServer(&GetStyleSVG()->mStroke);
    if (ps) {
      SetProperty(nsGkAtoms::stroke, ps, PServerPropertyDtor);
      AddStateBits(NS_STATE_SVG_STROKE_PSERVER);
    }
  }

  // cairo will stop rendering if stroke-width is less than or equal to zero
  if (GetStrokeWidth() <= 0)
    return PR_FALSE;

  if (GetStyleSVG()->mStroke.mType == eStyleSVGPaintType_Color ||
      (GetStyleSVG()->mStroke.mType == eStyleSVGPaintType_Server &&
       (GetStateBits() & NS_STATE_SVG_STROKE_PSERVER)))
    return PR_TRUE;

  return PR_FALSE;
}

PRBool
nsSVGGeometryFrame::HasFill()
{
  if (!(GetStateBits() & NS_STATE_SVG_FILL_PSERVER)) {
    nsIFrame *ps = GetPaintServer(&GetStyleSVG()->mFill);
    if (ps) {
      SetProperty(nsGkAtoms::fill, ps, PServerPropertyDtor);
      AddStateBits(NS_STATE_SVG_FILL_PSERVER);
    }
  }

  if (GetStyleSVG()->mFill.mType == eStyleSVGPaintType_Color ||
      GetStyleSVG()->mFill.mType == eStyleSVGPaintType_Server)
    return PR_TRUE;

  return PR_FALSE;
}

PRBool
nsSVGGeometryFrame::IsClipChild()
{
  nsIContent *node = mContent;

  do {
    // Return false if we find a non-svg ancestor. Non-SVG elements are not
    // allowed inside an SVG clipPath element.
    if (node->GetNameSpaceID() != kNameSpaceID_SVG) {
      break;
    }
    if (node->NodeInfo()->Equals(nsGkAtoms::clipPath, kNameSpaceID_SVG)) {
      return PR_TRUE;
    }
    node = node->GetParent();
  } while (node);
    
  return PR_FALSE;
}

static void
SetupCairoColor(gfxContext *aContext, nscolor aRGB, float aOpacity)
{
  aContext->SetColor(gfxRGBA(NS_GET_R(aRGB)/255.0,
                             NS_GET_G(aRGB)/255.0,
                             NS_GET_B(aRGB)/255.0,
                             NS_GET_A(aRGB)/255.0 * aOpacity));
}

float
nsSVGGeometryFrame::MaybeOptimizeOpacity(float aOpacity)
{
  if (nsSVGUtils::CanOptimizeOpacity(this)) {
    aOpacity *= GetStyleDisplay()->mOpacity;
  }
  return aOpacity;
}

nsresult
nsSVGGeometryFrame::SetupCairoFill(gfxContext *aContext,
                                   void **aClosure)
{
  if (GetStyleSVG()->mFillRule == NS_STYLE_FILL_RULE_EVENODD)
    aContext->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
  else
    aContext->SetFillRule(gfxContext::FILL_RULE_WINDING);

  float opacity = MaybeOptimizeOpacity(GetStyleSVG()->mFillOpacity);

  if (GetStateBits() & NS_STATE_SVG_FILL_PSERVER) {
    nsSVGPaintServerFrame *ps = NS_STATIC_CAST(nsSVGPaintServerFrame*,
                                               GetProperty(nsGkAtoms::fill));
    return ps->SetupPaintServer(aContext, this, opacity, aClosure);
  } else if (GetStyleSVG()->mFill.mType == eStyleSVGPaintType_Server) {
    SetupCairoColor(aContext,
                    GetStyleSVG()->mFill.mFallbackColor,
                    opacity);
  } else
    SetupCairoColor(aContext,
                    GetStyleSVG()->mFill.mPaint.mColor,
                    opacity);

  return NS_OK;
}

void
nsSVGGeometryFrame::CleanupCairoFill(gfxContext *aContext, void *aClosure)
{
  if (GetStateBits() & NS_STATE_SVG_FILL_PSERVER) {
    nsSVGPaintServerFrame *ps = NS_STATIC_CAST(nsSVGPaintServerFrame*,
                                               GetProperty(nsGkAtoms::fill));
    ps->CleanupPaintServer(aContext, aClosure);
  }
}

void
nsSVGGeometryFrame::SetupCairoStrokeGeometry(gfxContext *aContext)
{
  aContext->SetLineWidth(GetStrokeWidth());

  switch (GetStyleSVG()->mStrokeLinecap) {
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

  aContext->SetMiterLimit(GetStyleSVG()->mStrokeMiterlimit);

  switch (GetStyleSVG()->mStrokeLinejoin) {
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

nsresult
nsSVGGeometryFrame::SetupCairoStroke(gfxContext *aContext,
                                     void **aClosure)
{
  SetupCairoStrokeHitGeometry(aContext);

  float opacity = MaybeOptimizeOpacity(GetStyleSVG()->mStrokeOpacity);

  if (GetStateBits() & NS_STATE_SVG_STROKE_PSERVER) {
    nsSVGPaintServerFrame *ps = NS_STATIC_CAST(nsSVGPaintServerFrame*,
                                               GetProperty(nsGkAtoms::stroke));
    return ps->SetupPaintServer(aContext, this, opacity, aClosure);
  } else if (GetStyleSVG()->mStroke.mType == eStyleSVGPaintType_Server) {
    SetupCairoColor(aContext,
                    GetStyleSVG()->mStroke.mFallbackColor,
                    opacity);
  } else
    SetupCairoColor(aContext,
                    GetStyleSVG()->mStroke.mPaint.mColor,
                    opacity);

  return NS_OK;
}

void
nsSVGGeometryFrame::CleanupCairoStroke(gfxContext *aContext, void *aClosure)
{
  if (GetStateBits() & NS_STATE_SVG_STROKE_PSERVER) {
    nsSVGPaintServerFrame *ps = NS_STATIC_CAST(nsSVGPaintServerFrame*,
                                               GetProperty(nsGkAtoms::stroke));
    ps->CleanupPaintServer(aContext, aClosure);
  }
}
