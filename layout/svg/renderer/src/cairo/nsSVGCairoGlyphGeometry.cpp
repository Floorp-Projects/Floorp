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

  void GetGlobalTransform(cairo_t *ctx);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphGeometry interface:
  NS_DECL_NSISVGRENDERERGLYPHGEOMETRY
  
protected:
  nsCOMPtr<nsISVGGlyphGeometrySource> mSource;
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

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::Render(nsISVGRendererCanvas *canvas)
{
  nsCOMPtr<nsISVGCairoCanvas> cairoCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(cairoCanvas, "wrong svg render context for geometry!");
  if (!cairoCanvas) return NS_ERROR_FAILURE;

  cairo_t *ctx = cairoCanvas->GetContext();

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

  cairo_font_t *font = metrics->GetFont();

  /* save/pop the state so we don't screw up the xform */
  cairo_save(ctx);

  cairo_set_font(ctx, font);

  GetGlobalTransform(ctx);

  float x,y;
  mSource->GetX(&x);
  mSource->GetY(&y);
  cairo_move_to(ctx, x, y);

  PRBool hasFill = PR_FALSE;
  PRUint16 filltype;
  mSource->GetFillPaintType(&filltype);
  if (filltype != nsISVGGeometrySource::PAINT_TYPE_NONE)
    hasFill = PR_TRUE;

  PRBool hasStroke = PR_FALSE;
  PRUint16 stroketype;
  mSource->GetStrokePaintType(&stroketype);
  if (stroketype != nsISVGGeometrySource::PAINT_TYPE_NONE)
    hasStroke = PR_TRUE;

  if (!hasFill && !hasStroke) return NS_OK; // nothing to paint

  
  
  if (hasFill) {
      nscolor rgb;
      mSource->GetFillPaint(&rgb);
      float opacity;
      mSource->GetFillOpacity(&opacity);
      
      cairo_set_rgb_color(ctx,
			  NS_GET_R(rgb)/255.0,
			  NS_GET_G(rgb)/255.0,
			  NS_GET_B(rgb)/255.0);
      cairo_set_alpha(ctx, double(opacity));
      
      nsAutoString text;
      mSource->GetCharacterData(text);

      if (filltype == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR) {
        cairo_show_text(ctx, (unsigned char*)NS_ConvertUCS2toUTF8(text).get());
      } else {
        nsCOMPtr<nsISVGGradient> aGrad;
        mSource->GetFillGradient(getter_AddRefs(aGrad));

        cairo_text_extents_t extents;
        cairo_text_extents(ctx,
                           (unsigned char*)NS_ConvertUCS2toUTF8(text).get(),
                           &extents);
        
        cairo_pattern_t *gradient = CairoGradient(ctx, aGrad, &extents);
        cairo_set_pattern(ctx, gradient);
        cairo_show_text(ctx, (unsigned char*)NS_ConvertUCS2toUTF8(text).get());
        cairo_pattern_destroy(gradient);
      }
  }

  if (hasStroke) {
    nscolor rgb;
    mSource->GetStrokePaint(&rgb);
    float opacity;
    mSource->GetStrokeOpacity(&opacity);
    cairo_set_rgb_color(ctx,
                        NS_GET_R(rgb)/255.0,
                        NS_GET_G(rgb)/255.0,
                        NS_GET_B(rgb)/255.0);
    cairo_set_alpha(ctx, double(opacity));

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

    nsAutoString text;
    mSource->GetCharacterData(text);
    cairo_text_path(ctx, (unsigned char*)NS_ConvertUCS2toUTF8(text).get());

    if (stroketype == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR) {
      cairo_stroke(ctx);
    } else {
      nsCOMPtr<nsISVGGradient> aGrad;
      mSource->GetStrokeGradient(getter_AddRefs(aGrad));
      
      cairo_pattern_t *gradient = CairoGradient(ctx, aGrad);
      cairo_set_pattern(ctx, gradient);
      cairo_stroke(ctx);
      cairo_pattern_destroy(gradient);
    }
  }

  cairo_restore(ctx);
  
  return NS_OK;
}

/** Implements nsISVGRendererRegion update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::Update(PRUint32 updatemask, nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  return NS_OK;
}

/** Implements nsISVGRendererRegion getCoveredRegion(); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::GetCoveredRegion(nsISVGRendererRegion **_retval)
{
  return NS_NewSVGCairoRectRegion(_retval, -10000, -10000, 20000, 20000);
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

  cairo_font_t *font = metrics->GetFont();

  cairo_t *ctx = cairo_create();
  cairo_set_font(ctx, font);

  GetGlobalTransform(ctx);
  nsAutoString text;
  mSource->GetCharacterData(text);
  cairo_text_path(ctx, (unsigned char*)NS_ConvertUCS2toUTF8(text).get());
  cairo_default_matrix(ctx);
  *_retval = cairo_in_fill(ctx, x, y);
  cairo_destroy(ctx);

  return NS_OK;
}


void
nsSVGCairoGlyphGeometry::GetGlobalTransform(cairo_t *ctx)
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

  cairo_matrix_t *matrix = cairo_matrix_create();
  cairo_matrix_set_affine(matrix, m[0], m[1], m[2], m[3], m[4], m[5]);
  cairo_concat_matrix(ctx, matrix);
  cairo_matrix_destroy(matrix);
}
