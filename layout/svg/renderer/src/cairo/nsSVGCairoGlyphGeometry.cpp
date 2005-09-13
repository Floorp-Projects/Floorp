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

#include "nsISVGGradient.h"
#include "nsSVGCairoGradient.h"
#include "nsISVGPattern.h"
#include "nsSVGCairoPattern.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGTypeCIDs.h"
#include "nsIComponentManager.h"

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
  friend nsresult NS_NewSVGCairoGlyphGeometry(nsISVGRendererGlyphGeometry **result,
                                                nsISVGGlyphGeometrySource *src);

  nsSVGCairoGlyphGeometry();
  ~nsSVGCairoGlyphGeometry();
  nsresult Init(nsISVGGlyphGeometrySource* src);

  void GetGlobalTransform(cairo_t *ctx, nsISVGCairoCanvas* aCanvas);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphGeometry interface:
  NS_DECL_NSISVGRENDERERGLYPHGEOMETRY
  
protected:
  nsCOMPtr<nsISVGGlyphGeometrySource> mSource;

private:
  nsCOMPtr<nsISVGRendererRegion> mCoveredRegion;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGCairoGlyphGeometry::nsSVGCairoGlyphGeometry()
{
}

nsSVGCairoGlyphGeometry::~nsSVGCairoGlyphGeometry()
{
}

nsresult
nsSVGCairoGlyphGeometry::Init(nsISVGGlyphGeometrySource* src)
{
  mSource = src;
  return NS_OK;
}


nsresult
NS_NewSVGCairoGlyphGeometry(nsISVGRendererGlyphGeometry **result,
                              nsISVGGlyphGeometrySource *src)
{
  *result = nsnull;
  
  nsSVGCairoGlyphGeometry* gg = new nsSVGCairoGlyphGeometry();
  if (!gg) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(gg);

  nsresult rv = gg->Init(src);

  if (NS_FAILED(rv)) {
    NS_RELEASE(gg);
    return rv;
  }
  
  *result = gg;
  return rv;
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
      func(ctx, NS_ConvertUCS2toUTF8(text).get()); \
    } else { \
      for (PRUint32 i=0; i<text.Length(); i++) { \
        if (cp[i].draw == PR_FALSE) \
          continue; \
        cairo_matrix_t matrix; \
        cairo_get_matrix(ctx, &matrix); \
        cairo_move_to(ctx, cp[i].x, cp[i].y); \
        cairo_rotate(ctx, cp[i].angle); \
        func(ctx, NS_ConvertUCS2toUTF8(Substring(text, i, 1)).get()); \
        cairo_set_matrix(ctx, &matrix); \
      } \
    }

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::Render(nsISVGRendererCanvas *canvas)
{
  nsCOMPtr<nsISVGCairoCanvas> cairoCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(cairoCanvas, "wrong svg render context for geometry!");
  if (!cairoCanvas) return NS_ERROR_FAILURE;

  nsAutoString text;
  mSource->GetCharacterData(text);

  if (!text.Length())
    return NS_OK;

  nsSVGCharacterPosition *cp;
  mSource->GetCharacterPosition(&cp);
  
  if (NS_FAILED(mSource->GetCharacterPosition(&cp)))
    return NS_ERROR_FAILURE;

  cairo_t *ctx = cairoCanvas->GetContext();

  /* get the metrics */
  nsCOMPtr<nsISVGCairoGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    mSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics) {
      delete [] cp;
      return NS_ERROR_FAILURE;
    }
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

  GetGlobalTransform(ctx, cairoCanvas);

  metrics->SelectFont(ctx);

  float x,y;
  mSource->GetX(&x);
  mSource->GetY(&y);
  cairo_move_to(ctx, x, y);

  if (renderMode != nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL) {
    PRUint16 rule;
    mSource->GetClipRule(&rule);
    if (rule == nsISVGGeometrySource::FILL_RULE_EVENODD)
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_EVEN_ODD);
    else
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_WINDING);

    LOOP_CHARS(cairo_text_path)

    cairo_set_matrix(ctx, &matrix);

    delete [] cp;

    return NS_OK;
  }

  PRBool hasFill = PR_FALSE;
  PRUint16 filltype;
  mSource->GetFillPaintType(&filltype);
  PRUint16 fillServerType = 0;
  if (filltype != nsISVGGeometrySource::PAINT_TYPE_NONE) {
    hasFill = PR_TRUE;
    if (filltype == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
      if (NS_FAILED(mSource->GetFillPaintServerType(&fillServerType)))
        hasFill = PR_FALSE;
    }
  }

  PRBool hasStroke = PR_FALSE;
  PRUint16 stroketype;
  mSource->GetStrokePaintType(&stroketype);
  PRUint16 strokeServerType = 0;
  if (stroketype != nsISVGGeometrySource::PAINT_TYPE_NONE) {
    hasStroke = PR_TRUE;
    if (stroketype == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
      if (NS_FAILED(mSource->GetStrokePaintServerType(&strokeServerType)))
        hasStroke = PR_FALSE;
    }
  }

  if (!hasFill && !hasStroke) {
    delete [] cp;
    return NS_OK; // nothing to paint
  }

  if (hasFill) {
      nscolor rgb;
      mSource->GetFillPaint(&rgb);
      float opacity;
      mSource->GetFillOpacity(&opacity);
      
      cairo_set_source_rgba(ctx,
                            NS_GET_R(rgb)/255.0,
                            NS_GET_G(rgb)/255.0,
                            NS_GET_B(rgb)/255.0,
                            opacity);
      
      nsAutoString text;
      mSource->GetCharacterData(text);

      if (filltype == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR) {
        LOOP_CHARS(cairo_show_text)
      } else if (filltype == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
        if (fillServerType == nsISVGGeometrySource::PAINT_TYPE_GRADIENT) {
          nsCOMPtr<nsISVGGradient> aGrad;
          mSource->GetFillGradient(getter_AddRefs(aGrad));

          cairo_pattern_t *gradient = CairoGradient(ctx, aGrad, mSource);
          cairo_set_source(ctx, gradient);
          LOOP_CHARS(cairo_show_text)
          cairo_pattern_destroy(gradient);
        } else if (fillServerType == nsISVGGeometrySource::PAINT_TYPE_PATTERN) {
          nsCOMPtr<nsISVGPattern> aPat;
          mSource->GetFillPattern(getter_AddRefs(aPat));
          // Paint the pattern -- note that because we will call back into the
          // layout layer to paint, we need to pass the canvas, not just the context
          nsCOMPtr<nsISVGGeometrySource> aGsource = do_QueryInterface(mSource);
          cairo_pattern_t *pattern = CairoPattern(canvas, aPat, aGsource);
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
    nscolor rgb;
    mSource->GetStrokePaint(&rgb);
    float opacity;
    mSource->GetStrokeOpacity(&opacity);
    cairo_set_source_rgba(ctx,
                          NS_GET_R(rgb)/255.0,
                          NS_GET_G(rgb)/255.0,
                          NS_GET_B(rgb)/255.0,
                          opacity);

    float width;
    mSource->GetStrokeWidth(&width);
    cairo_set_line_width(ctx, double(width));

    PRUint16 capStyle;
    mSource->GetStrokeLinecap(&capStyle);
    switch (capStyle) {
    case nsISVGGeometrySource::STROKE_LINECAP_BUTT:
      cairo_set_line_cap(ctx, CAIRO_LINE_CAP_BUTT);
      break;
    case nsISVGGeometrySource::STROKE_LINECAP_ROUND:
      cairo_set_line_cap(ctx, CAIRO_LINE_CAP_ROUND);
      break;
    case nsISVGGeometrySource::STROKE_LINECAP_SQUARE:
      cairo_set_line_cap(ctx, CAIRO_LINE_CAP_SQUARE);
      break;
    }

    float miterlimit;
    mSource->GetStrokeMiterlimit(&miterlimit);
    cairo_set_miter_limit(ctx, double(miterlimit));

    PRUint16 joinStyle;
    mSource->GetStrokeLinejoin(&joinStyle);
    switch(joinStyle) {
    case nsISVGGeometrySource::STROKE_LINEJOIN_MITER:
      cairo_set_line_join(ctx, CAIRO_LINE_JOIN_MITER);
      break;
    case nsISVGGeometrySource::STROKE_LINEJOIN_ROUND:
      cairo_set_line_join(ctx, CAIRO_LINE_JOIN_ROUND);
      break;
    case nsISVGGeometrySource::STROKE_LINEJOIN_BEVEL:
      cairo_set_line_join(ctx, CAIRO_LINE_JOIN_BEVEL);
      break;
    }

    float *dashArray, offset;
    PRUint32 count;
    mSource->GetStrokeDashArray(&dashArray, &count);
    if (count > 0) {
      double *dashes = new double[count];
      for (unsigned i=0; i<count; i++)
        dashes[i] = dashArray[i];
      mSource->GetStrokeDashoffset(&offset);
      cairo_set_dash(ctx, dashes, count, double(offset));
      nsMemory::Free(dashArray);
      delete [] dashes;
    }

    LOOP_CHARS(cairo_text_path)

    if (stroketype == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR) {
      cairo_stroke(ctx);
    } else if (stroketype == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
      if (strokeServerType == nsISVGGeometrySource::PAINT_TYPE_GRADIENT) {
        nsCOMPtr<nsISVGGradient> aGrad;
        mSource->GetStrokeGradient(getter_AddRefs(aGrad));

        cairo_pattern_t *gradient = CairoGradient(ctx, aGrad, mSource);
        cairo_set_source(ctx, gradient);
        cairo_stroke(ctx);
        cairo_pattern_destroy(gradient);
      } else if (strokeServerType == nsISVGGeometrySource::PAINT_TYPE_PATTERN) {
        nsCOMPtr<nsISVGPattern> aPat;
        mSource->GetStrokePattern(getter_AddRefs(aPat));
        // Paint the pattern -- note that because we will call back into the
        // layout layer to paint, we need to pass the canvas, not just the context
        nsCOMPtr<nsISVGGeometrySource> aGsource = do_QueryInterface(mSource);
        cairo_pattern_t *pattern = CairoPattern(canvas, aPat, aGsource);
        if (pattern) {
          cairo_set_source(ctx, pattern);
          cairo_stroke(ctx);
          cairo_pattern_destroy(pattern);
        }
      }
    }
  }

  delete [] cp;

  cairo_restore(ctx);
  
  return NS_OK;
}

/** Implements nsISVGRendererRegion update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::Update(PRUint32 updatemask, nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  const unsigned long strokemask =
    nsISVGGlyphMetricsSource::UPDATEMASK_FONT           |
    nsISVGGlyphMetricsSource::UPDATEMASK_CHARACTER_DATA |
    nsISVGGlyphGeometrySource::UPDATEMASK_METRICS       |
    nsISVGGlyphGeometrySource::UPDATEMASK_X             |
    nsISVGGlyphGeometrySource::UPDATEMASK_Y             |
    nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT_TYPE  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_WIDTH       |
    nsISVGGeometrySource::UPDATEMASK_STROKE_LINECAP     |
    nsISVGGeometrySource::UPDATEMASK_STROKE_LINEJOIN    |
    nsISVGGeometrySource::UPDATEMASK_STROKE_MITERLIMIT  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_DASH_ARRAY  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_DASHOFFSET  |
    nsISVGGeometrySource::UPDATEMASK_CANVAS_TM;
  
  const unsigned long regionsmask =
    nsISVGGlyphGeometrySource::UPDATEMASK_METRICS |
    nsISVGGlyphGeometrySource::UPDATEMASK_X       |
    nsISVGGlyphGeometrySource::UPDATEMASK_Y       |
    nsISVGGeometrySource::UPDATEMASK_CANVAS_TM;

  nsCOMPtr<nsISVGRendererRegion> before = mCoveredRegion;

  if ((updatemask & regionsmask) || (updatemask & strokemask)) {
    nsCOMPtr<nsISVGRendererRegion> after;
    GetCoveredRegion(getter_AddRefs(after));

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
nsSVGCairoGlyphGeometry::GetCoveredRegion(nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);

  /* get the metrics */
  nsCOMPtr<nsISVGCairoGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    mSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics)
      return NS_ERROR_FAILURE;
  }

  GetGlobalTransform(ctx, nsnull);

  metrics->SelectFont(ctx);

  float x,y;
  mSource->GetX(&x);
  mSource->GetY(&y);
  cairo_move_to(ctx, x, y);

  PRUint16 type;  
  mSource->GetFillPaintType(&type);
  PRBool hasCoveredFill = type != nsISVGGeometrySource::PAINT_TYPE_NONE;
  mSource->GetStrokePaintType(&type);
  bool hasCoveredStroke = type != nsISVGGeometrySource::PAINT_TYPE_NONE;

  if (!hasCoveredFill && !hasCoveredStroke) return NS_OK;

  nsAutoString text;
  mSource->GetCharacterData(text);
  
  if (text.Length() == 0) {
    double xx = x, yy = y;
    cairo_user_to_device(ctx, &xx, &yy);
    cairo_destroy(ctx);
    return NS_NewSVGCairoRectRegion(_retval, xx, yy, 0, 0);
  }

  nsSVGCharacterPosition *cp;

  if (NS_FAILED(mSource->GetCharacterPosition(&cp))) {
    cairo_destroy(ctx);
    return NS_ERROR_FAILURE;
  }

  if (!cp) {
    if (hasCoveredStroke) {
      cairo_text_path(ctx, NS_ConvertUCS2toUTF8(text).get());
    } else {
      cairo_text_extents_t extent;
      cairo_text_extents(ctx,
                         NS_ConvertUCS2toUTF8(text).get(),
                         &extent);
      cairo_rectangle(ctx, x + extent.x_bearing, y + extent.y_bearing,
                      extent.width, extent.height);
    }
  } else {
    cairo_matrix_t matrix;
    for (PRUint32 i=0; i<text.Length(); i++) {
      cairo_get_matrix(ctx, &matrix);
      cairo_move_to(ctx, cp[i].x, cp[i].y);
      cairo_rotate(ctx, cp[i].angle);
      if (hasCoveredStroke) {
        cairo_text_path(ctx, NS_ConvertUCS2toUTF8(Substring(text, i, 1)).get());
      } else {
        cairo_text_extents_t extent;
        cairo_text_extents(ctx,
                           NS_ConvertUCS2toUTF8(Substring(text, i, 1)).get(),
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

  delete [] cp;

  double xmin, ymin, xmax, ymax;

  if (hasCoveredStroke) {
    float width;
    mSource->GetStrokeWidth(&width);
    cairo_set_line_width(ctx, double(width));
    
    PRUint16 capStyle;
    mSource->GetStrokeLinecap(&capStyle);
    switch (capStyle) {
    case nsISVGGeometrySource::STROKE_LINECAP_BUTT:
      cairo_set_line_cap(ctx, CAIRO_LINE_CAP_BUTT);
      break;
    case nsISVGGeometrySource::STROKE_LINECAP_ROUND:
      cairo_set_line_cap(ctx, CAIRO_LINE_CAP_ROUND);
      break;
    case nsISVGGeometrySource::STROKE_LINECAP_SQUARE:
      cairo_set_line_cap(ctx, CAIRO_LINE_CAP_SQUARE);
      break;
    }
    
    float miterlimit;
    mSource->GetStrokeMiterlimit(&miterlimit);
    cairo_set_miter_limit(ctx, double(miterlimit));
    
    PRUint16 joinStyle;
    mSource->GetStrokeLinejoin(&joinStyle);
    switch(joinStyle) {
    case nsISVGGeometrySource::STROKE_LINEJOIN_MITER:
      cairo_set_line_join(ctx, CAIRO_LINE_JOIN_MITER);
      break;
    case nsISVGGeometrySource::STROKE_LINEJOIN_ROUND:
      cairo_set_line_join(ctx, CAIRO_LINE_JOIN_ROUND);
      break;
    case nsISVGGeometrySource::STROKE_LINEJOIN_BEVEL:
      cairo_set_line_join(ctx, CAIRO_LINE_JOIN_BEVEL);
      break;
    }
    
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
nsSVGCairoGlyphGeometry::ContainsPoint(float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;

  /* get the metrics */
  nsCOMPtr<nsISVGCairoGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    mSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics)
      return NS_ERROR_FAILURE;
  }

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);
  GetGlobalTransform(ctx, nsnull);

  metrics->SelectFont(ctx);

  nsAutoString text;
  mSource->GetCharacterData(text);

  nsSVGCharacterPosition *cp;

  if (NS_FAILED(mSource->GetCharacterPosition(&cp))) {
    cairo_destroy(ctx);
    return NS_ERROR_FAILURE;
  }

  float xx, yy;
  mSource->GetX(&xx);
  mSource->GetY(&yy);

  cairo_matrix_t matrix;

  for (PRUint32 i=0; i<text.Length(); i++) {
    cairo_get_matrix(ctx, &matrix);

    if (cp) {
      cairo_move_to(ctx, cp[i].x, cp[i].y);
      cairo_rotate(ctx, cp[i].angle);
    } else {
      cairo_move_to(ctx, xx, yy);
    }

    cairo_text_extents_t extent;
    cairo_text_extents(ctx,
                       NS_ConvertUCS2toUTF8(Substring(text, i, 1)).get(),
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

  delete [] cp;

  cairo_identity_matrix(ctx);
  *_retval = cairo_in_fill(ctx, x, y);
  cairo_destroy(ctx);

  return NS_OK;
}


void
nsSVGCairoGlyphGeometry::GetGlobalTransform(cairo_t *ctx, nsISVGCairoCanvas* aCanvas)
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm;
  mSource->GetCanvasTM(getter_AddRefs(ctm));
  NS_ASSERTION(ctm, "graphic source didn't specify a ctm");
  
  float m[6];
  float val;
  ctm->GetA(&val);
  m[0] = val;
  
  ctm->GetB(&val);
  m[1] = val;
  
  ctm->GetC(&val);  
  m[2] = val;  
  
  ctm->GetD(&val);  
  m[3] = val;  
  
  ctm->GetE(&val);
  m[4] = val;
  
  ctm->GetF(&val);
  m[5] = val;

  cairo_matrix_t matrix = {m[0], m[1], m[2], m[3], m[4], m[5]};
  if (aCanvas) {
    aCanvas->AdjustMatrixForInitialTransform(&matrix);
  }
  cairo_set_matrix(ctx, &matrix);
}

NS_IMETHODIMP
nsSVGCairoGlyphGeometry::GetBoundingBox(nsIDOMSVGRect * *aBoundingBox)
{
  *aBoundingBox = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);
  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;

  nsAutoString text;
  mSource->GetCharacterData(text);
  if (!text.Length())
    return NS_OK;

  nsSVGCharacterPosition *cp;
  mSource->GetCharacterPosition(&cp);
  if (NS_FAILED(mSource->GetCharacterPosition(&cp)))
    return NS_ERROR_FAILURE;

  double xmin, ymin, xmax, ymax;

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);

  GetGlobalTransform(ctx, nsnull);

  /* get the metrics */
  nsCOMPtr<nsISVGCairoGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    mSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics)
      return NS_ERROR_FAILURE;
  }

  metrics->SelectFont(ctx);

  float x,y;
  mSource->GetX(&x);
  mSource->GetY(&y);
  cairo_move_to(ctx, x, y);

  LOOP_CHARS(cairo_text_path)

  delete [] cp;

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
