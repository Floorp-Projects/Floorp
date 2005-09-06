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
#include "nsISVGCairoRegion.h"
#include "nsIDOMSVGMatrix.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGPathGeometrySource.h"
#include "nsISVGRendererPathBuilder.h"
#include "nsSVGCairoPathBuilder.h"
#include "nsMemory.h"
#include <float.h>
#include <cairo.h>
#include "nsSVGCairoRegion.h"
#include "nsISVGGradient.h"
#include "nsSVGCairoGradient.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGTypeCIDs.h"
#include "nsIComponentManager.h"
#include "nsISVGPathFlatten.h"

extern cairo_surface_t *gSVGCairoDummySurface;

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

  void GeneratePath(cairo_t *ctx, nsISVGCairoCanvas* aCanvas);
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
nsSVGCairoPathGeometry::GeneratePath(cairo_t *ctx, nsISVGCairoCanvas* aCanvas)
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

  cairo_matrix_t matrix = { m[0], m[1], m[2], m[3], m[4], m[5] };
  if (aCanvas) {
    aCanvas->AdjustMatrixForInitialTransform(&matrix);
  }
  cairo_set_matrix(ctx, &matrix);

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

  PRUint16 renderMode;
  canvas->GetRenderMode(&renderMode);
  cairo_matrix_t matrix;

  if (renderMode == nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL) {
    cairo_new_path(ctx);

    /* save/pop the state so we don't screw up the xform */
    cairo_save(ctx);
  } else {
    cairo_get_matrix(ctx, &matrix);
  }

  GeneratePath(ctx, cairoCanvas);

  if (renderMode != nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL) {
    PRUint16 rule;
    mSource->GetClipRule(&rule);
    if (rule == nsISVGGeometrySource::FILL_RULE_EVENODD)
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_EVEN_ODD);
    else
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_WINDING);

    cairo_set_matrix(ctx, &matrix);

    return NS_OK;
  }

  PRUint16 strokeType, fillType;
  PRUint16 strokeServerType = 0;

  PRBool bStroking = PR_FALSE;
  mSource->GetStrokePaintType(&strokeType);
  if (strokeType != nsISVGGeometrySource::PAINT_TYPE_NONE) {
    bStroking = PR_TRUE;
    if (strokeType == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
      if (NS_FAILED(mSource->GetStrokePaintServerType(&strokeServerType)))
        // unknown type or missing frame
        bStroking = PR_FALSE;
    }
  }

  mSource->GetFillPaintType(&fillType);
  PRUint16 fillServerType = 0;
  if (fillType == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
    if (NS_FAILED(mSource->GetFillPaintServerType(&fillServerType)))
      // unknown type or missing frame
      fillType = nsISVGGeometrySource::PAINT_TYPE_NONE;
  }

  if (fillType != nsISVGGeometrySource::PAINT_TYPE_NONE) {
    nscolor rgb;
    mSource->GetFillPaint(&rgb);
    float opacity;
    mSource->GetFillOpacity(&opacity);

    cairo_set_source_rgba(ctx,
                          NS_GET_R(rgb)/255.0,
                          NS_GET_G(rgb)/255.0,
                          NS_GET_B(rgb)/255.0,
                          opacity);

    if (fillType == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR) {
      cairo_fill_preserve(ctx);
    } else if (fillType == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
      if (fillServerType == nsISVGGeometrySource::PAINT_TYPE_GRADIENT) {
        nsCOMPtr<nsISVGGradient> aGrad;
        mSource->GetFillGradient(getter_AddRefs(aGrad));

        cairo_pattern_t *gradient = CairoGradient(ctx, aGrad, mSource);
        cairo_set_source(ctx, gradient);
        cairo_fill_preserve(ctx);
        cairo_pattern_destroy(gradient);
      } else {
        cairo_fill_preserve(ctx);
      }
    }

    if (!bStroking)
      cairo_new_path(ctx);
  }

  if (bStroking) {
    nscolor rgb;
    mSource->GetStrokePaint(&rgb);
    float opacity;
    mSource->GetStrokeOpacity(&opacity);
    cairo_set_source_rgba(ctx,
                          NS_GET_R(rgb)/255.0,
                          NS_GET_G(rgb)/255.0,
                          NS_GET_B(rgb)/255.0,
                          opacity);

    if (strokeType == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR) {
      cairo_stroke(ctx);
    } else if (strokeType == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
      PRUint16 serverType;
      mSource->GetStrokePaintServerType(&serverType);
      if (serverType == nsISVGGeometrySource::PAINT_TYPE_GRADIENT) {
        nsCOMPtr<nsISVGGradient> aGrad;
        mSource->GetStrokeGradient(getter_AddRefs(aGrad));

        cairo_pattern_t *gradient = CairoGradient(ctx, aGrad, mSource);
        cairo_set_source(ctx, gradient);
        cairo_stroke(ctx);
        cairo_pattern_destroy(gradient);
      } else {
        cairo_stroke(ctx);
      }
    }
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

  nsCOMPtr<nsISVGRendererRegion> before = mCoveredRegion;

  if (updatemask & coveredregionmask) {
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
nsSVGCairoPathGeometry::GetCoveredRegion(nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);

  GeneratePath(ctx, nsnull);

  PRUint16 type;  
  mSource->GetFillPaintType(&type);
  PRBool hasCoveredFill = type != nsISVGGeometrySource::PAINT_TYPE_NONE;
  
  mSource->GetStrokePaintType(&type);
  bool hasCoveredStroke = type != nsISVGGeometrySource::PAINT_TYPE_NONE;

  if (!hasCoveredFill && !hasCoveredStroke) return NS_OK;

  double xmin, ymin, xmax, ymax;

  if (hasCoveredStroke)
    cairo_stroke_extents(ctx, &xmin, &ymin, &xmax, &ymax);
  else
    cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);

  cairo_user_to_device(ctx, &xmin, &ymin);
  cairo_user_to_device(ctx, &xmax, &ymax);

  cairo_destroy(ctx);

  return NS_NewSVGCairoRectRegion(_retval, xmin, ymin, xmax-xmin, ymax-ymin);
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGCairoPathGeometry::ContainsPoint(float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;

  // early reject test
  if (mCoveredRegion) {
    nsCOMPtr<nsISVGCairoRegion> region = do_QueryInterface(mCoveredRegion);
    if (!region->Contains(x,y))
      return NS_OK;
  }

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);
  cairo_set_tolerance(ctx, 1.0);

  GeneratePath(ctx, nsnull);
  double xx = x, yy = y;
  cairo_device_to_user(ctx, &xx, &yy);

  PRBool isClip;
  mSource->IsClipChild(&isClip);
  if (isClip) {
    PRUint16 rule;
    mSource->GetClipRule(&rule);
    if (rule == nsISVGGeometrySource::FILL_RULE_EVENODD)
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_EVEN_ODD);
    else
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_WINDING);
  }

  PRUint16 mask = 0;
  mSource->GetHittestMask(&mask);
  if (mask & nsISVGPathGeometrySource::HITTEST_MASK_FILL)
    *_retval = cairo_in_fill(ctx, xx, yy);
  if (!*_retval && (mask & nsISVGPathGeometrySource::HITTEST_MASK_STROKE))
    *_retval = cairo_in_stroke(ctx, xx, yy);

  cairo_destroy(ctx);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoPathGeometry::GetBoundingBox(nsIDOMSVGRect * *aBoundingBox)
{
  *aBoundingBox = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);

  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;

  double xmin, ymin, xmax, ymax;

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);
  GeneratePath(ctx, nsnull);

  cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);

  /* cairo_fill_extents doesn't work on degenerate paths */
  if (xmin ==  32767 &&
      ymin ==  32767 &&
      xmax == -32768 &&
      ymax == -32768) {
    /* cairo_stroke_extents doesn't work with stroke width zero, fudge */
    cairo_set_line_width(ctx, 0.0001);
    cairo_stroke_extents(ctx, &xmin, &ymin, &xmax, &ymax);
  }

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

NS_IMETHODIMP
nsSVGCairoPathGeometry::Flatten(nsSVGPathData **aData)
{
  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);
  GeneratePath(ctx, nsnull);

  *aData = new nsSVGPathData;

  cairo_path_t *path;
  cairo_path_data_t *data;

  path = cairo_copy_path_flat(ctx);

  for (PRInt32 i = 0; i < path->num_data; i += path->data[i].header.length) {
    data = &path->data[i];
    switch (data->header.type) {
    case CAIRO_PATH_MOVE_TO:
      (*aData)->AddPoint(data[1].point.x,
                         data[1].point.y,
                         NS_SVGPATHFLATTEN_MOVE);
      break;
    case CAIRO_PATH_LINE_TO:
      (*aData)->AddPoint(data[1].point.x,
                         data[1].point.y,
                         NS_SVGPATHFLATTEN_LINE);
      break;
    case CAIRO_PATH_CURVE_TO:
      /* should never happen with a flattened path */
      break;
    case CAIRO_PATH_CLOSE_PATH:
    {
      if (!(*aData)->count)
        break;

      /* find beginning of current subpath */
      for (PRUint32 k = (*aData)->count - 1; k >= 0; k--)
        if ((*aData)->type[k] == NS_SVGPATHFLATTEN_MOVE) {
          (*aData)->AddPoint((*aData)->x[k],
                             (*aData)->y[k],
                             NS_SVGPATHFLATTEN_LINE);
          break;
        }
    }
    }
  }

  cairo_path_destroy(path);
  cairo_destroy(ctx);

  return NS_OK;
}
