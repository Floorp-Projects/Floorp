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
 *    layout/svg/renderer/src/libart/nsSVGLibartPathGeometry.cpp
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com>
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
#include "nsSVGCairoPathGeometry.h"
#include "nsISVGRendererPathGeometry.h"
#include "nsISVGCairoCanvas.h"
#include "nsIDOMSVGMatrix.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGPathGeometrySource.h"
#include "nsISVGRendererPathBuilder.h"
#include "nsSVGCairoPathBuilder.h"
#include "nsMemory.h"
#include <float.h>
#include <cairo.h>
#include "nsSVGCairoRegion.h"

/**
 * \addtogroup cairo_renderer Cairo Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Cairo path geometry implementation
 */
class nsSVGCairoPathGeometry : public nsISVGRendererPathGeometry
{
protected:
  friend nsresult NS_NewSVGCairoPathGeometry(nsISVGRendererPathGeometry **result,
                                             nsISVGPathGeometrySource *src);

  nsSVGCairoPathGeometry();
  ~nsSVGCairoPathGeometry();
  nsresult Init(nsISVGPathGeometrySource* src);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS
  
  // nsISVGRendererPathGeometry interface:
  NS_DECL_NSISVGRENDERERPATHGEOMETRY
  
private:
  nsCOMPtr<nsISVGPathGeometrySource> mSource;
  nsCOMPtr<nsISVGRendererRegion> mCoveredRegion;

  void GeneratePath(cairo_t *ctx);
};

/** @} */


//----------------------------------------------------------------------
// implementation:

nsSVGCairoPathGeometry::nsSVGCairoPathGeometry()
{
}

nsSVGCairoPathGeometry::~nsSVGCairoPathGeometry()
{
}

nsresult nsSVGCairoPathGeometry::Init(nsISVGPathGeometrySource* src)
{
  mSource = src;
  return NS_OK;
}


nsresult
NS_NewSVGCairoPathGeometry(nsISVGRendererPathGeometry **result,
                           nsISVGPathGeometrySource *src)
{
  nsSVGCairoPathGeometry* pg = new nsSVGCairoPathGeometry();
  if (!pg) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(pg);

  nsresult rv = pg->Init(src);

  if (NS_FAILED(rv)) {
    NS_RELEASE(pg);
    return rv;
  }
  
  *result = pg;
  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGCairoPathGeometry)
NS_IMPL_RELEASE(nsSVGCairoPathGeometry)

NS_INTERFACE_MAP_BEGIN(nsSVGCairoPathGeometry)
NS_INTERFACE_MAP_ENTRY(nsISVGRendererPathGeometry)
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------

void
nsSVGCairoPathGeometry::GeneratePath(cairo_t *ctx)
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

  nsCOMPtr<nsISVGRendererPathBuilder> builder;
  NS_NewSVGCairoPathBuilder(getter_AddRefs(builder), ctx);
  mSource->ConstructPath(builder);
  builder->EndPath();

  PRUint16 type;

  mSource->GetStrokePaintType(&type);
  if (type != nsISVGGeometrySource::PAINT_TYPE_NONE) {
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
  }

  mSource->GetFillPaintType(&type);
  if (type != nsISVGGeometrySource::PAINT_TYPE_NONE) {
    PRUint16 rule;
    mSource->GetFillRule(&rule);
    if (rule == nsISVGGeometrySource::FILL_RULE_EVENODD)
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_EVEN_ODD);
    else
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_WINDING);
  }
}


//----------------------------------------------------------------------
// nsISVGRendererPathGeometry methods:

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGCairoPathGeometry::Render(nsISVGRendererCanvas *canvas)
{
  nsCOMPtr<nsISVGCairoCanvas> cairoCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(cairoCanvas, "wrong svg render context for geometry!");
  if (!cairoCanvas) return NS_ERROR_FAILURE;

  cairo_t *ctx = cairoCanvas->GetContext();

  /* save/pop the state so we don't screw up the xform */
  cairo_save(ctx);

  GeneratePath(ctx);

  PRUint16 type;

  PRBool bStroking = PR_FALSE;
  mSource->GetStrokePaintType(&type);
  if (type == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR)
    bStroking = PR_TRUE;

  mSource->GetFillPaintType(&type);
  if (type == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR) {
    if (bStroking)
      cairo_save(ctx);

    nscolor rgb;
    mSource->GetFillPaint(&rgb);
    float opacity;
    mSource->GetFillOpacity(&opacity);

    cairo_set_rgb_color(ctx,
                        NS_GET_R(rgb)/255.0,
                        NS_GET_G(rgb)/255.0,
                        NS_GET_B(rgb)/255.0);
    cairo_set_alpha(ctx, double(opacity));

    cairo_fill(ctx);

    if (bStroking)
      cairo_restore(ctx);
  }

  if (bStroking) {
    nscolor rgb;
    mSource->GetStrokePaint(&rgb);
    float opacity;
    mSource->GetStrokeOpacity(&opacity);
    cairo_set_rgb_color(ctx,
                        NS_GET_R(rgb)/255.0,
                        NS_GET_G(rgb)/255.0,
                        NS_GET_B(rgb)/255.0);
    cairo_set_alpha(ctx, double(opacity));

    cairo_stroke(ctx);
  }

  cairo_restore(ctx);

  return NS_OK;
}

/** Implements nsISVGRendererRegion update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGCairoPathGeometry::Update(PRUint32 updatemask, nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  const unsigned long pathmask =
    nsISVGPathGeometrySource::UPDATEMASK_PATH |
    nsISVGGeometrySource::UPDATEMASK_CANVAS_TM;

  const unsigned long fillmask = 
    pathmask |
    nsISVGGeometrySource::UPDATEMASK_FILL_RULE;

  const unsigned long strokemask =
    pathmask |
    nsISVGGeometrySource::UPDATEMASK_STROKE_WIDTH       |
    nsISVGGeometrySource::UPDATEMASK_STROKE_LINECAP     |
    nsISVGGeometrySource::UPDATEMASK_STROKE_LINEJOIN    |
    nsISVGGeometrySource::UPDATEMASK_STROKE_MITERLIMIT  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_DASH_ARRAY  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_DASHOFFSET;

  const unsigned long coveredregionmask =
    fillmask                                            |
    strokemask                                          |
    nsISVGGeometrySource::UPDATEMASK_FILL_PAINT_TYPE    |
    nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT_TYPE;

  if (updatemask & coveredregionmask) {
    nsCOMPtr<nsISVGRendererRegion> after;
    GetCoveredRegion(getter_AddRefs(after));

    if (mCoveredRegion) {
      if (after)
        after->Combine(mCoveredRegion, _retval);
    } else {
      *_retval = after;
      NS_IF_ADDREF(*_retval);
    }
    mCoveredRegion = after;
  }
  else if (updatemask != nsISVGGeometrySource::UPDATEMASK_NOTHING) {
    *_retval = mCoveredRegion;
    NS_IF_ADDREF(*_retval);
  }

  return NS_OK;
}

/** Implements nsISVGRendererRegion getCoveredRegion(); */
NS_IMETHODIMP
nsSVGCairoPathGeometry::GetCoveredRegion(nsISVGRendererRegion **_retval)
{
  cairo_t *ctx = cairo_create();

  GeneratePath(ctx);
  cairo_default_matrix(ctx);

  PRUint16 type;  
  mSource->GetFillPaintType(&type);
  PRBool hasCoveredFill = type != nsISVGGeometrySource::PAINT_TYPE_NONE;
  
  mSource->GetStrokePaintType(&type);
  bool hasCoveredStroke = type != nsISVGGeometrySource::PAINT_TYPE_NONE;

  if (!hasCoveredFill && !hasCoveredStroke) return NS_OK;

  double x[4], y[4];

  if (hasCoveredStroke)
    cairo_stroke_extents(ctx, &x[0], &y[0], &x[1], &y[1]);
  else
    cairo_fill_extents(ctx, &x[0], &y[0], &x[1], &y[1]);

  x[2] = x[0];  y[2] = y[1];
  x[3] = x[1];  y[3] = y[0];

  double xmin = DBL_MAX, ymin = DBL_MAX;
  double xmax = DBL_MIN, ymax = DBL_MIN;

  for (int i=0; i<4; i++) {
    cairo_transform_point(ctx, &x[i], &y[i]);
    if (x[i] < xmin) xmin = x[i];
    if (y[i] < ymin) ymin = y[i];
    if (x[i] > xmax) xmax = x[i];
    if (y[i] > ymax) ymax = y[i];
  }

  cairo_destroy(ctx);

  return NS_NewSVGCairoRectRegion(_retval, xmin, ymin, xmax-xmin, ymax-ymin);
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGCairoPathGeometry::ContainsPoint(float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;

  cairo_t *ctx = cairo_create();

  GeneratePath(ctx);
  cairo_default_matrix(ctx);

  PRUint16 mask = 0;
  mSource->GetHittestMask(&mask);
  if (mask & nsISVGPathGeometrySource::HITTEST_MASK_FILL)
    *_retval = cairo_in_fill(ctx, x, y);
  if (!*_retval & mask & nsISVGPathGeometrySource::HITTEST_MASK_STROKE)
    *_retval = cairo_in_stroke(ctx, x, y);

  cairo_destroy(ctx);

  return NS_OK;
}
