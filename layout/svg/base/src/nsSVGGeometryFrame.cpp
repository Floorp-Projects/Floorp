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
#include "nsSVGGradient.h"
#include "nsSVGPattern.h"
#include "nsISVGValueUtils.h"
#include "nsSVGUtils.h"
#include "nsSVGGeometryFrame.h"
#include "nsISVGGradient.h"
#include "nsISVGPattern.h"

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGeometryFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGeometryFrameBase)

//----------------------------------------------------------------------

nsSVGGeometryFrame::nsSVGGeometryFrame(nsStyleContext* aContext)
  : nsSVGGeometryFrameBase(aContext),
    mFillGradient(nsnull), mStrokeGradient(nsnull),
    mFillPattern(nsnull), mStrokePattern(nsnull)
{
}

nsSVGGeometryFrame::~nsSVGGeometryFrame()
{
  if (mFillGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillGradient);
  }
  if (mStrokeGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokeGradient);
  }
  if (mFillPattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillPattern);
  }
  if (mStrokePattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokePattern);
  }
}

NS_IMETHODIMP
nsSVGGeometryFrame::DidSetStyleContext()
{
  // One of the styles that might have been changed are the urls that
  // point to gradients, etc.  Drop our cached values to those
  if (mFillGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillGradient);
    mFillGradient = nsnull;
  }
  if (mStrokeGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokeGradient);
    mStrokeGradient = nsnull;
  }
  if (mFillPattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillPattern);
    mFillPattern = nsnull;
  }
  if (mStrokePattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokePattern);
    mStrokePattern = nsnull;
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
  nsISVGGradient *gradient;
  CallQueryInterface(observable, &gradient);

  if (gradient) {
    // Yes, we need to handle this differently
    if (mFillGradient == gradient) {
      if (aModType == nsISVGValue::mod_die) {
        mFillGradient = nsnull;
      }
      UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_FILL_PAINT);
    } else {
      // No real harm in assuming a stroke gradient at this point
      if (aModType == nsISVGValue::mod_die) {
        mStrokeGradient = nsnull;
      }
      UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_STROKE_PAINT);
    }

    return NS_OK;
  }

  nsISVGPattern *pval;
  CallQueryInterface(observable, &pval);

  if (pval) {
    // Handle Patterns
    if (mFillPattern == pval) {
      if (aModType == nsISVGValue::mod_die) {
        mFillPattern = nsnull;
      }
      UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_FILL_PAINT);
    } else {
      // Assume stroke pattern
      if (aModType == nsISVGValue::mod_die) {
        mStrokePattern = nsnull;
      }
      UpdateGraphic(nsSVGGeometryFrame::UPDATEMASK_STROKE_PAINT);
    }
    return NS_OK;
  }

  return NS_OK;
}


//----------------------------------------------------------------------

float
nsSVGGeometryFrame::GetStrokeOpacity()
{
  return GetStyleSVG()->mStrokeOpacity;
}

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

float
nsSVGGeometryFrame::GetFillOpacity()
{
  return GetStyleSVG()->mFillOpacity;
}

PRUint16
nsSVGGeometryFrame::GetClipRule()
{
  return GetStyleSVG()->mClipRule;
}

PRUint16
nsSVGGeometryFrame::GetStrokePaintType()
{
  float strokeWidth = GetStrokeWidth();

  // cairo will stop rendering if stroke-width is less than or equal to zero
  return strokeWidth <= 0 ? eStyleSVGPaintType_None
                          : GetStyleSVG()->mStroke.mType;
}

nsresult
nsSVGGeometryFrame::GetStrokePaintServerType(PRUint16 *aStrokePaintServerType) {
  return nsSVGUtils::GetPaintType(aStrokePaintServerType,
                                  GetStyleSVG()->mStroke, mContent,
                                  GetPresContext()->PresShell());
}

nsresult
nsSVGGeometryFrame::GetStrokeGradient(nsISVGGradient **aGrad)
{
  nsresult rv = NS_OK;
  *aGrad = nsnull;
  if (!mStrokeGradient) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mStroke.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGGradient(&mStrokeGradient, aServer, mContent, 
                           GetPresContext()->PresShell());
    NS_ADD_SVGVALUE_OBSERVER(mStrokeGradient);
  }
  *aGrad = mStrokeGradient;
  return rv;
}

nsresult
nsSVGGeometryFrame::GetStrokePattern(nsISVGPattern **aPat)
{
  nsresult rv = NS_OK;
  *aPat = nsnull;
  if (!mStrokePattern) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mStroke.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGPattern(&mStrokePattern, aServer, mContent, 
                          GetPresContext()->PresShell());
    if (mStrokePattern)
      NS_ADD_SVGVALUE_OBSERVER(mStrokePattern);
  }
  *aPat = mStrokePattern;
  return rv;
}

PRUint16
nsSVGGeometryFrame::GetFillPaintType()
{
  return GetStyleSVG()->mFill.mType;
}

nsresult
nsSVGGeometryFrame::GetFillPaintServerType(PRUint16 *aFillPaintServerType)
{
  return nsSVGUtils::GetPaintType(aFillPaintServerType,
                                  GetStyleSVG()->mFill, mContent,
                                  GetPresContext()->PresShell());
}

nsresult
nsSVGGeometryFrame::GetFillGradient(nsISVGGradient **aGrad)
{
  nsresult rv = NS_OK;
  *aGrad = nsnull;
  if (!mFillGradient) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mFill.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGGradient(&mFillGradient, aServer, mContent, 
                           GetPresContext()->PresShell());
    NS_ADD_SVGVALUE_OBSERVER(mFillGradient);
  }
  *aGrad = mFillGradient;
  return rv;
}

nsresult
nsSVGGeometryFrame::GetFillPattern(nsISVGPattern **aPat)
{
  nsresult rv = NS_OK;
  *aPat = nsnull;
  if (!mFillPattern) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mFill.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the pattern 
    rv = NS_GetSVGPattern(&mFillPattern, aServer, mContent, 
                          GetPresContext()->PresShell());
    if (mFillPattern)
      NS_ADD_SVGVALUE_OBSERVER(mFillPattern);
  }
  *aPat = mFillPattern;
  return rv;
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

void
nsSVGGeometryFrame::SetupCairoFill(cairo_t *aCtx)
{
  if (GetStyleSVG()->mFillRule == NS_STYLE_FILL_RULE_EVENODD)
    cairo_set_fill_rule(aCtx, CAIRO_FILL_RULE_EVEN_ODD);
  else
    cairo_set_fill_rule(aCtx, CAIRO_FILL_RULE_WINDING);

  SetupCairoColor(aCtx, GetStyleSVG()->mFill.mPaint.mColor, GetFillOpacity());
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

void
nsSVGGeometryFrame::SetupCairoStroke(cairo_t *aCtx)
{
  SetupCairoStrokeGeometry(aCtx);

  double *dashArray;
  PRUint32 count;
  GetStrokeDashArray(&dashArray, &count);
  if (count > 0) {
    cairo_set_dash(aCtx, dashArray, count, GetStrokeDashoffset());
    delete [] dashArray;
  }

  SetupCairoColor(aCtx,
                  GetStyleSVG()->mStroke.mPaint.mColor, GetStrokeOpacity());
}
