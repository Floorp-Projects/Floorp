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
 * The Original Code is the Mozilla SVG Cairo Renderer project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Parts of this file contain code derived from the following files(s)
 * of the Mozilla SVG project (these parts are Copyright (C) by their
 * respective copyright-holders):
 *    layout/svg/renderer/src/gdiplus/nsSVGGDIPlusGlyphGeometry.cpp
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

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsSVGCairoGlyphGeometry.h"
#include "nsISVGRendererGlyphGeometry.h"
#include "nsISVGCairoCanvas.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGCairoRegion.h"
#include "nsISVGCairoRegion.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGGlyphGeometrySource.h"
#include "nsPromiseFlatString.h"
#include "nsSVGCairoGlyphMetrics.h"
#include "nsISVGCairoGlyphMetrics.h"
#include "nsPresContext.h"
#include "nsMemory.h"
#include <cairo.h>

#include "nsSVGCairoGradient.h"
#include "nsISVGCairoSurface.h"
#include "nsSVGCairoPattern.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGTypeCIDs.h"
#include "nsIComponentManager.h"
#include "nsSVGGlyphFrame.h"
#include "nsSVGMatrix.h"

extern cairo_surface_t *gSVGCairoDummySurface;

/**
 * \addtogroup cairo_renderer cairo Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 *  cairo glyph geometry implementation
 */
class nsSVGCairoGlyphGeometry : public nsISVGRendererGlyphGeometry
{
protected:
  friend nsresult NS_NewSVGCairoGlyphGeometry(nsISVGRendererGlyphGeometry **result);

  nsresult GetGlobalTransform(nsSVGGlyphFrame *aSource,
                              cairo_t *ctx, nsISVGCairoCanvas* aCanvas);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphGeometry interface:
  NS_DECL_NSISVGRENDERERGLYPHGEOMETRY
  
private:
  nsCOMPtr<nsISVGRendererRegion> mCoveredRegion;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsresult
NS_NewSVGCairoGlyphGeometry(nsISVGRendererGlyphGeometry **result)
{
  *result = new nsSVGCairoGlyphGeometry;
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGCairoGlyphGeometry)
NS_IMPL_RELEASE(nsSVGCairoGlyphGeometry)

NS_INTERFACE_MAP_BEGIN(nsSVGCairoGlyphGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererGlyphGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsISVGRendererGlyphGeometry methods:

#define LOOP_CHARS(func) \
    if (!cp) { \
      func(ctx, NS_ConvertUTF16toUTF8(text).get()); \
    } else { \
      for (PRUint32 i=0; i<text.Length(); i++) { \
        /* character actually on the path? */  \
        if (cp[i].draw == PR_FALSE) \
          continue; \
        cairo_matrix_t matrix; \
        cairo_get_matrix(ctx, &matrix); \
        cairo_move_to(ctx, cp[i].x, cp[i].y); \
        cairo_rotate(ctx, cp[i].angle); \
        func(ctx, NS_ConvertUTF16toUTF8(Substring(text, i, 1)).get()); \
        cairo_set_matrix(ctx, &matrix); \
      } \
    }

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::Render(nsSVGGlyphFrame *aSource, 
                                nsISVGRendererCanvas *canvas)
{
  nsCOMPtr<nsISVGCairoCanvas> cairoCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(cairoCanvas, "wrong svg render context for geometry!");
  if (!cairoCanvas) return NS_ERROR_FAILURE;

  nsAutoString text;
  aSource->GetCharacterData(text);

  if (!text.Length())
    return NS_OK;

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;
  
  if (NS_FAILED(aSource->GetCharacterPosition(getter_Transfers(cp))))
    return NS_ERROR_FAILURE;

  cairo_t *ctx = cairoCanvas->GetContext();

  /* get the metrics */
  nsCOMPtr<nsISVGCairoGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    aSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics)
      return NS_ERROR_FAILURE;
  }

  PRUint16 renderMode;
  cairo_matrix_t matrix;
  canvas->GetRenderMode(&renderMode);
  if (renderMode == nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL) {
    /* save/pop the state so we don't screw up the xform */
    cairo_save(ctx);
  }
  else {
    cairo_get_matrix(ctx, &matrix);
  }

  if (NS_FAILED(GetGlobalTransform(aSource, ctx, cairoCanvas))) {
    if (renderMode == nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL)
      cairo_restore(ctx);
    return NS_ERROR_FAILURE;
  }

  metrics->SelectFont(ctx);

  float x,y;
  if (!cp) {
    aSource->GetX(&x);
    aSource->GetY(&y);
    cairo_move_to(ctx, x, y);
  }

  if (renderMode != nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL) {
    if (aSource->GetClipRule() == NS_STYLE_FILL_RULE_EVENODD)
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_EVEN_ODD);
    else
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_WINDING);

    LOOP_CHARS(cairo_text_path)

    cairo_set_matrix(ctx, &matrix);

    return NS_OK;
  }

  PRBool hasFill = PR_FALSE;
  PRUint16 filltype = aSource->GetFillPaintType();
  PRUint16 fillServerType = 0;
  if (filltype != eStyleSVGPaintType_None) {
    hasFill = PR_TRUE;
    if (filltype == eStyleSVGPaintType_Server) {
      if (NS_FAILED(aSource->GetFillPaintServerType(&fillServerType)))
        hasFill = PR_FALSE;
    }
  }

  PRBool hasStroke = PR_FALSE;
  PRUint16 stroketype = aSource->GetStrokePaintType();
  PRUint16 strokeServerType = 0;
  if (stroketype != eStyleSVGPaintType_None) {
    hasStroke = PR_TRUE;
    if (stroketype == eStyleSVGPaintType_Server) {
      if (NS_FAILED(aSource->GetStrokePaintServerType(&strokeServerType)))
        hasStroke = PR_FALSE;
    }
  }

  if (!hasFill && !hasStroke) {
    cairo_restore(ctx);
    return NS_OK; // nothing to paint
  }

  if (hasFill) {
      aSource->SetupCairoFill(ctx);
      
      if (filltype == eStyleSVGPaintType_Color) {
        LOOP_CHARS(cairo_show_text)
      } else if (filltype == eStyleSVGPaintType_Server) {
        if (fillServerType == nsSVGGeometryFrame::PAINT_TYPE_GRADIENT) {
          nsSVGGradientFrame *aGrad;
          aSource->GetFillGradient(&aGrad);

          cairo_pattern_t *gradient = CairoGradient(ctx, aGrad, aSource);
          if (gradient) {
            cairo_set_source(ctx, gradient);
            LOOP_CHARS(cairo_show_text)
            cairo_pattern_destroy(gradient);
          }
        } else if (fillServerType == nsSVGGeometryFrame::PAINT_TYPE_PATTERN) {
          nsSVGPatternFrame *aPat;
          aSource->GetFillPattern(&aPat);
          // Paint the pattern -- note that because we will call back into the
          // layout layer to paint, we need to pass the canvas, not just the context
          nsCOMPtr<nsISVGRendererSurface> patSurface;
          cairo_pattern_t *pattern = CairoPattern(canvas, aPat, aSource, getter_AddRefs(patSurface));
          if (pattern) {
            cairo_set_source(ctx, pattern);
            LOOP_CHARS(cairo_show_text)
            cairo_pattern_destroy(pattern);
          } else {
            LOOP_CHARS(cairo_show_text)
          }
        }
    }
  }

  cairo_move_to(ctx, x, y);

  if (hasStroke) {
    aSource->SetupCairoStroke(ctx);

    LOOP_CHARS(cairo_text_path)

    if (stroketype == eStyleSVGPaintType_Color) {
      cairo_stroke(ctx);
    } else if (stroketype == eStyleSVGPaintType_Server) {
      if (strokeServerType == nsSVGGeometryFrame::PAINT_TYPE_GRADIENT) {
        nsSVGGradientFrame *aGrad;
        aSource->GetStrokeGradient(&aGrad);

        cairo_pattern_t *gradient = CairoGradient(ctx, aGrad, aSource);
        if (gradient) {
          cairo_set_source(ctx, gradient);
          cairo_stroke(ctx);
          cairo_pattern_destroy(gradient);
        }
      } else if (strokeServerType == nsSVGGeometryFrame::PAINT_TYPE_PATTERN) {
        nsSVGPatternFrame *aPat;
        aSource->GetStrokePattern(&aPat);
        // Paint the pattern -- note that because we will call back into the
        // layout layer to paint, we need to pass the canvas, not just the context
        nsCOMPtr<nsISVGRendererSurface> patSurface;
        cairo_pattern_t *pattern = CairoPattern(canvas, aPat, aSource, getter_AddRefs(patSurface));
        if (pattern) {
          cairo_set_source(ctx, pattern);
          cairo_stroke(ctx);
          cairo_pattern_destroy(pattern);
        }
      }
    }
  }

  cairo_restore(ctx);
  
  return NS_OK;
}

/** Implements nsISVGRendererRegion update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::Update(nsSVGGlyphFrame *aSource, 
                                PRUint32 updatemask,
                                nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  const unsigned long strokemask =
    nsISVGGlyphMetricsSource::UPDATEMASK_FONT           |
    nsISVGGlyphMetricsSource::UPDATEMASK_CHARACTER_DATA |
    nsISVGGlyphGeometrySource::UPDATEMASK_METRICS       |
    nsISVGGlyphGeometrySource::UPDATEMASK_X             |
    nsISVGGlyphGeometrySource::UPDATEMASK_Y             |
    nsSVGGeometryFrame::UPDATEMASK_STROKE_PAINT_TYPE    |
    nsSVGGeometryFrame::UPDATEMASK_STROKE_WIDTH         |
    nsSVGGeometryFrame::UPDATEMASK_STROKE_LINECAP       |
    nsSVGGeometryFrame::UPDATEMASK_STROKE_LINEJOIN      |
    nsSVGGeometryFrame::UPDATEMASK_STROKE_MITERLIMIT    |
    nsSVGGeometryFrame::UPDATEMASK_STROKE_DASH_ARRAY    |
    nsSVGGeometryFrame::UPDATEMASK_STROKE_DASHOFFSET    |
    nsSVGGeometryFrame::UPDATEMASK_CANVAS_TM;
  
  const unsigned long regionsmask =
    nsISVGGlyphGeometrySource::UPDATEMASK_METRICS |
    nsISVGGlyphGeometrySource::UPDATEMASK_X       |
    nsISVGGlyphGeometrySource::UPDATEMASK_Y       |
    nsSVGGeometryFrame::UPDATEMASK_CANVAS_TM;

  nsCOMPtr<nsISVGRendererRegion> before = mCoveredRegion;

  if ((updatemask & regionsmask) || (updatemask & strokemask)) {
    nsCOMPtr<nsISVGRendererRegion> after;
    GetCoveredRegion(aSource, getter_AddRefs(after));

    if (mCoveredRegion) {
      if (after)
        after->Combine(before, _retval);
    } else {
      *_retval = after;
      NS_IF_ADDREF(*_retval);
    }
    mCoveredRegion = after;
  }

  if (!*_retval) {
    *_retval = before;
    NS_IF_ADDREF(*_retval);
  }

  return NS_OK;
}

/** Implements nsISVGRendererRegion getCoveredRegion(); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::GetCoveredRegion(nsSVGGlyphFrame *aSource,
                                          nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);

  /* get the metrics */
  nsCOMPtr<nsISVGCairoGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    aSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics)
      return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(GetGlobalTransform(aSource, ctx, nsnull))) {
    cairo_destroy(ctx);
    return NS_ERROR_FAILURE;
  }

  metrics->SelectFont(ctx);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  if (NS_FAILED(aSource->GetCharacterPosition(getter_Transfers(cp)))) {
    cairo_destroy(ctx);
    return NS_ERROR_FAILURE;
  }

  float x,y;
  if (!cp) {
    aSource->GetX(&x);
    aSource->GetY(&y);
    cairo_move_to(ctx, x, y);
  } else {
      x = 0.0, y = 0.0;
  }

  PRBool hasCoveredFill = 
    aSource->GetFillPaintType() != eStyleSVGPaintType_None;
  
  bool hasCoveredStroke =
    aSource->GetStrokePaintType() != eStyleSVGPaintType_None;

  if (!hasCoveredFill && !hasCoveredStroke) return NS_OK;

  nsAutoString text;
  aSource->GetCharacterData(text);
  
  if (text.Length() == 0) {
    double xx = x, yy = y;
    cairo_user_to_device(ctx, &xx, &yy);
    cairo_destroy(ctx);
    return NS_NewSVGCairoRectRegion(_retval, xx, yy, 0, 0);
  }

  if (!cp) {
    if (hasCoveredStroke) {
      cairo_text_path(ctx, NS_ConvertUTF16toUTF8(text).get());
    } else {
      cairo_text_extents_t extent;
      cairo_text_extents(ctx,
                         NS_ConvertUTF16toUTF8(text).get(),
                         &extent);
      cairo_rectangle(ctx, x + extent.x_bearing, y + extent.y_bearing,
                      extent.width, extent.height);
    }
  } else {
    cairo_matrix_t matrix;
    for (PRUint32 i=0; i<text.Length(); i++) {
      /* character actually on the path? */
      if (cp[i].draw == PR_FALSE)
        continue;
      cairo_get_matrix(ctx, &matrix);
      cairo_move_to(ctx, cp[i].x, cp[i].y);
      cairo_rotate(ctx, cp[i].angle);
      if (hasCoveredStroke) {
        cairo_text_path(ctx, NS_ConvertUTF16toUTF8(Substring(text, i, 1)).get());
      } else {
        cairo_text_extents_t extent;
        cairo_text_extents(ctx,
                           NS_ConvertUTF16toUTF8(Substring(text, i, 1)).get(),
                           &extent);
        cairo_rel_move_to(ctx, extent.x_bearing, extent.y_bearing);
        cairo_rel_line_to(ctx, extent.width, 0);
        cairo_rel_line_to(ctx, 0, extent.height);
        cairo_rel_line_to(ctx, -extent.width, 0);
        cairo_close_path(ctx);
      }
      cairo_set_matrix(ctx, &matrix);
    }
  }

  double xmin, ymin, xmax, ymax;

  if (hasCoveredStroke) {
    aSource->SetupCairoStrokeGeometry(ctx);
    cairo_stroke_extents(ctx, &xmin, &ymin, &xmax, &ymax);
  } else {
    cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);
  }

  cairo_user_to_device(ctx, &xmin, &ymin);
  cairo_user_to_device(ctx, &xmax, &ymax);

  cairo_destroy(ctx);

  return NS_NewSVGCairoRectRegion(_retval, xmin, ymin, xmax-xmin, ymax-ymin);
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::ContainsPoint(nsSVGGlyphFrame *aSource,
                                       float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;

  /* get the metrics */
  nsCOMPtr<nsISVGCairoGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    aSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics)
      return NS_ERROR_FAILURE;
  }

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);
  if (NS_FAILED(GetGlobalTransform(aSource, ctx, nsnull))) {
    cairo_destroy(ctx);
    return NS_ERROR_FAILURE;
  }

  metrics->SelectFont(ctx);

  nsAutoString text;
  aSource->GetCharacterData(text);

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  if (NS_FAILED(aSource->GetCharacterPosition(getter_Transfers(cp)))) {
    cairo_destroy(ctx);
    return NS_ERROR_FAILURE;
  }

  float xx, yy;
  if (!cp) {
    aSource->GetX(&xx);
    aSource->GetY(&yy);
  }

  cairo_matrix_t matrix;

  for (PRUint32 i=0; i<text.Length(); i++) {
    /* character actually on the path? */
    if (cp && cp[i].draw == PR_FALSE)
      continue;

    cairo_get_matrix(ctx, &matrix);

    if (cp) {
      cairo_move_to(ctx, cp[i].x, cp[i].y);
      cairo_rotate(ctx, cp[i].angle);
    } else {
      cairo_move_to(ctx, xx, yy);
    }

    cairo_text_extents_t extent;
    cairo_text_extents(ctx,
                       NS_ConvertUTF16toUTF8(Substring(text, i, 1)).get(),
                       &extent);
    cairo_rel_move_to(ctx, extent.x_bearing, extent.y_bearing);
    cairo_rel_line_to(ctx, extent.width, 0);
    cairo_rel_line_to(ctx, 0, extent.height);
    cairo_rel_line_to(ctx, -extent.width, 0);
    cairo_close_path(ctx);

    cairo_set_matrix(ctx, &matrix);

    if (!cp) {
      xx += extent.x_advance;
      yy += extent.y_advance;
    }
  }

  cairo_identity_matrix(ctx);
  *_retval = cairo_in_fill(ctx, x, y);
  cairo_destroy(ctx);

  return NS_OK;
}


nsresult
nsSVGCairoGlyphGeometry::GetGlobalTransform(nsSVGGlyphFrame *aSource,
                                            cairo_t *ctx,
                                            nsISVGCairoCanvas* aCanvas)
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm;
  aSource->GetCanvasTM(getter_AddRefs(ctm));
  NS_ASSERTION(ctm, "graphic source didn't specify a ctm");
  
  cairo_matrix_t matrix = NS_ConvertSVGMatrixToCairo(ctm);
  if (aCanvas) {
    aCanvas->AdjustMatrixForInitialTransform(&matrix);
  }

  cairo_matrix_t inverse = matrix;
  if (cairo_matrix_invert(&inverse)) {
    cairo_identity_matrix(ctx);
    return NS_ERROR_FAILURE;
  }

  cairo_set_matrix(ctx, &matrix);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoGlyphGeometry::GetBoundingBox(nsSVGGlyphFrame *aSource,
                                        nsIDOMSVGRect * *aBoundingBox)
{
  *aBoundingBox = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);
  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;

  nsAutoString text;
  aSource->GetCharacterData(text);
  if (!text.Length())
    return NS_OK;

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  if (NS_FAILED(aSource->GetCharacterPosition(getter_Transfers(cp))))
    return NS_ERROR_FAILURE;

  double xmin, ymin, xmax, ymax;

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);

  if (NS_FAILED(GetGlobalTransform(aSource, ctx, nsnull))) {
    cairo_destroy(ctx);
    return NS_ERROR_FAILURE;
  }

  /* get the metrics */
  nsCOMPtr<nsISVGCairoGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    aSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics)
      return NS_ERROR_FAILURE;
  }

  metrics->SelectFont(ctx);

  float x,y;
  if (!cp) {
    aSource->GetX(&x);
    aSource->GetY(&y);
    cairo_move_to(ctx, x, y);
  }

  LOOP_CHARS(cairo_text_path)

  cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);

  cairo_user_to_device(ctx, &xmin, &ymin);
  cairo_user_to_device(ctx, &xmax, &ymax);

  cairo_destroy(ctx);

  rect->SetX(xmin);
  rect->SetY(ymin);
  rect->SetWidth(xmax - xmin);
  rect->SetHeight(ymax - ymin);

  *aBoundingBox = rect;
  NS_ADDREF(*aBoundingBox);
  
  return NS_OK;
}
