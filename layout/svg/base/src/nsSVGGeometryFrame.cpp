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

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGeometryFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGeometryFrameBase)

//----------------------------------------------------------------------

nsSVGGeometryFrame::nsSVGGeometryFrame(nsStyleContext* aContext)
  : nsSVGGeometryFrameBase(aContext),
    mFillServer(nsnull), mStrokeServer(nsnull)
{
}

void
nsSVGGeometryFrame::Destroy()
{
  // Do this here instead of in the destructor, so virtual calls still work
  if (mFillServer)
    mFillServer->RemoveObserver(this);
  if (mStrokeServer)
    mStrokeServer->RemoveObserver(this);

  nsSVGGeometryFrameBase::Destroy();
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
                                               GetPresContext()->PresShell())))
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
nsSVGGeometryFrame::DidSetStyleContext()
{
  // One of the styles that might have been changed are the urls that
  // point to gradients, etc.  Drop our cached values to those
  if (mFillServer) {
    mFillServer->RemoveObserver(this);
    mFillServer = nsnull;
  }
  if (mStrokeServer) {
    mStrokeServer->RemoveObserver(this);
    mStrokeServer = nsnull;
  }

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
  nsIFrame *frame;
  CallQueryInterface(observable, &frame);

  if (!frame)
    return NS_OK;

  if (frame == mFillServer) {
    if (aModType == nsISVGValue::mod_die)
      mFillServer = nsnull;
    UpdateGraphic();
  }

  if (frame == mStrokeServer) {
    if (aModType == nsISVGValue::mod_die)
      mStrokeServer = nsnull;
    UpdateGraphic();
  }

  return NS_OK;
}


//----------------------------------------------------------------------

float
nsSVGGeometryFrame::GetStrokeWidth()
{
  return nsSVGUtils::CoordToFloat(GetPresContext(),
                                  mContent, GetStyleSVG()->mStrokeWidth);
}

nsresult
nsSVGGeometryFrame::GetStrokeDashArray(double **aDashes, PRUint32 *aCount)
{
  *aDashes = nsnull;
  *aCount = 0;

  PRUint32 count = GetStyleSVG()->mStrokeDasharrayLength;
  double *dashes = nsnull;

  if (count) {
    const nsStyleCoord *dasharray = GetStyleSVG()->mStrokeDasharray;
    nsPresContext *presContext = GetPresContext();
    float totalLength = 0.0f;

    dashes = new double[count];
    if (dashes) {
      for (PRUint32 i = 0; i < count; i++) {
        dashes[i] =
          nsSVGUtils::CoordToFloat(presContext, mContent, dasharray[i]);
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
    nsSVGUtils::CoordToFloat(GetPresContext(), mContent,
                             GetStyleSVG()->mStrokeDashoffset);
}

PRUint16
nsSVGGeometryFrame::GetClipRule()
{
  return GetStyleSVG()->mClipRule;
}

PRBool
nsSVGGeometryFrame::HasStroke()
{
  if (!mStrokeServer)
    mStrokeServer = GetPaintServer(&GetStyleSVG()->mStroke);

  // cairo will stop rendering if stroke-width is less than or equal to zero
  if (GetStrokeWidth() <= 0)
    return PR_FALSE;

  if (GetStyleSVG()->mStroke.mType == eStyleSVGPaintType_Color ||
      (GetStyleSVG()->mStroke.mType == eStyleSVGPaintType_Server &&
       mStrokeServer))
    return PR_TRUE;

  return PR_FALSE;
}

PRBool
nsSVGGeometryFrame::HasFill()
{
  if (!mFillServer)
    mFillServer = GetPaintServer(&GetStyleSVG()->mFill);

  if (GetStyleSVG()->mFill.mType == eStyleSVGPaintType_Color ||
      (GetStyleSVG()->mFill.mType == eStyleSVGPaintType_Server && mFillServer))
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
SetupCairoColor(cairo_t *aCtx, nscolor aRGB, float aOpacity)
{
  cairo_set_source_rgba(aCtx,
                        NS_GET_R(aRGB)/255.0,
                        NS_GET_G(aRGB)/255.0,
                        NS_GET_B(aRGB)/255.0,
                        aOpacity);
}

nsresult
nsSVGGeometryFrame::SetupCairoFill(nsISVGRendererCanvas *aCanvas,
                                   cairo_t *aCtx,
                                   void **aClosure)
{
  if (GetStyleSVG()->mFillRule == NS_STYLE_FILL_RULE_EVENODD)
    cairo_set_fill_rule(aCtx, CAIRO_FILL_RULE_EVEN_ODD);
  else
    cairo_set_fill_rule(aCtx, CAIRO_FILL_RULE_WINDING);

  if (mFillServer)
    return mFillServer->SetupPaintServer(aCanvas, aCtx, this,
                                         GetStyleSVG()->mFillOpacity,
                                         aClosure);
  else
    SetupCairoColor(aCtx,
                    GetStyleSVG()->mFill.mPaint.mColor,
                    GetStyleSVG()->mFillOpacity);

  return NS_OK;
}

void
nsSVGGeometryFrame::CleanupCairoFill(cairo_t *aCtx, void *aClosure)
{
  if (mFillServer)
    mFillServer->CleanupPaintServer(aCtx, aClosure);
}

void
nsSVGGeometryFrame::SetupCairoStrokeGeometry(cairo_t *aCtx)
{
  cairo_set_line_width(aCtx, GetStrokeWidth());
  
  switch (GetStyleSVG()->mStrokeLinecap) {
  case NS_STYLE_STROKE_LINECAP_BUTT:
    cairo_set_line_cap(aCtx, CAIRO_LINE_CAP_BUTT);
    break;
  case NS_STYLE_STROKE_LINECAP_ROUND:
    cairo_set_line_cap(aCtx, CAIRO_LINE_CAP_ROUND);
    break;
  case NS_STYLE_STROKE_LINECAP_SQUARE:
    cairo_set_line_cap(aCtx, CAIRO_LINE_CAP_SQUARE);
    break;
  }
  
  cairo_set_miter_limit(aCtx, GetStyleSVG()->mStrokeMiterlimit);
  
  switch (GetStyleSVG()->mStrokeLinejoin) {
  case NS_STYLE_STROKE_LINEJOIN_MITER:
    cairo_set_line_join(aCtx, CAIRO_LINE_JOIN_MITER);
    break;
  case NS_STYLE_STROKE_LINEJOIN_ROUND:
    cairo_set_line_join(aCtx, CAIRO_LINE_JOIN_ROUND);
    break;
  case NS_STYLE_STROKE_LINEJOIN_BEVEL:
    cairo_set_line_join(aCtx, CAIRO_LINE_JOIN_BEVEL);
    break;
  }
}

nsresult
nsSVGGeometryFrame::SetupCairoStroke(nsISVGRendererCanvas *aCanvas,
                                     cairo_t *aCtx,
                                     void **aClosure)
{
  SetupCairoStrokeGeometry(aCtx);

  double *dashArray;
  PRUint32 count;
  GetStrokeDashArray(&dashArray, &count);
  if (count > 0) {
    cairo_set_dash(aCtx, dashArray, count, GetStrokeDashoffset());
    delete [] dashArray;
  }

  if (mStrokeServer)
    return mStrokeServer->SetupPaintServer(aCanvas, aCtx, this,
                                           GetStyleSVG()->mStrokeOpacity,
                                           aClosure);
  else
    SetupCairoColor(aCtx,
                    GetStyleSVG()->mStroke.mPaint.mColor,
                    GetStyleSVG()->mStrokeOpacity);

  return NS_OK;
}

void
nsSVGGeometryFrame::CleanupCairoStroke(cairo_t *aCtx, void *aClosure)
{
  if (mStrokeServer)
    mStrokeServer->CleanupPaintServer(aCtx, aClosure);
}
