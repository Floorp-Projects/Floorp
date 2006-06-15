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
#include "nsISVGPathGeometrySource.h"
#include "nsMemory.h"
#include <float.h>
#include <cairo.h>
#include "nsIDOMSVGRect.h"
#include "nsSVGRect.h"
#include "nsSVGPathGeometryFrame.h"
#include "nsSVGMatrix.h"
#include "nsSVGUtils.h"
#ifdef DEBUG
#include <stdio.h>
#endif

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
  friend nsresult NS_NewSVGCairoPathGeometry(nsISVGRendererPathGeometry **result);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS
  
  // nsISVGRendererPathGeometry interface:
  NS_DECL_NSISVGRENDERERPATHGEOMETRY
  
private:
  void GeneratePath(nsSVGPathGeometryFrame *aSource,
                    cairo_t *ctx, nsISVGCairoCanvas* aCanvas);
};

/** @} */


//----------------------------------------------------------------------
// implementation:

nsresult
NS_NewSVGCairoPathGeometry(nsISVGRendererPathGeometry **result)
{
  *result = new nsSVGCairoPathGeometry;
  if (!*result) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*result);
  return NS_OK;
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
nsSVGCairoPathGeometry::GeneratePath(nsSVGPathGeometryFrame *aSource,
                                     cairo_t *ctx, nsISVGCairoCanvas* aCanvas)
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
    cairo_new_path(ctx);
    return;
  }
  cairo_set_matrix(ctx, &matrix);

  cairo_new_path(ctx);
  aSource->ConstructPath(ctx);
}


//----------------------------------------------------------------------
// nsISVGRendererPathGeometry methods:

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGCairoPathGeometry::Render(nsSVGPathGeometryFrame *aSource, 
                               nsISVGRendererCanvas *canvas)
{
  nsCOMPtr<nsISVGCairoCanvas> cairoCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(cairoCanvas, "wrong svg render context for geometry!");
  if (!cairoCanvas) {
    return NS_ERROR_FAILURE;
  }

  cairo_t *ctx = cairoCanvas->GetContext();

  PRUint16 renderMode;
  canvas->GetRenderMode(&renderMode);

  /* save/pop the state so we don't screw up the xform */
  cairo_save(ctx);

  GeneratePath(aSource, ctx, cairoCanvas);

  if (renderMode != nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL) {
    cairo_restore(ctx);

    if (aSource->GetClipRule() == NS_STYLE_FILL_RULE_EVENODD)
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_EVEN_ODD);
    else
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_WINDING);

    if (renderMode == nsISVGRendererCanvas::SVG_RENDER_MODE_CLIP_MASK) {
      cairo_set_antialias(ctx, CAIRO_ANTIALIAS_NONE);
      cairo_set_source_rgba(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
      cairo_fill(ctx);
    }

    return NS_OK;
  }

  PRUint16 shapeMode;
  aSource->GetShapeRendering(&shapeMode);
  switch (shapeMode) {
  case nsISVGPathGeometrySource::SHAPE_RENDERING_OPTIMIZESPEED:
  case nsISVGPathGeometrySource::SHAPE_RENDERING_CRISPEDGES:
    cairo_set_antialias(ctx, CAIRO_ANTIALIAS_NONE);
    break;
  default:
    cairo_set_antialias(ctx, CAIRO_ANTIALIAS_DEFAULT);
    break;
  }

  void *closure;
  if (aSource->HasFill() &&
      NS_SUCCEEDED(aSource->SetupCairoFill(canvas, ctx, &closure))) {
    cairo_fill_preserve(ctx);
    aSource->CleanupCairoFill(ctx, closure);
  }

  if (aSource->HasStroke() &&
      NS_SUCCEEDED(aSource->SetupCairoStroke(canvas, ctx, &closure))) {
    cairo_stroke(ctx);
    aSource->CleanupCairoStroke(ctx, closure);
  }

  cairo_new_path(ctx);

  cairo_restore(ctx);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoPathGeometry::GetCoveredRegion(nsSVGPathGeometryFrame *aSource,
                                         nsRect *aRect)
{
  aRect->Empty();

  PRBool hasFill = aSource->HasFill();
  PRBool hasStroke = aSource->HasStroke();

  if (!hasFill && !hasStroke) {
    return NS_OK;
  }

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);

  GeneratePath(aSource, ctx, nsnull);

  double xmin, ymin, xmax, ymax;

  if (hasStroke) {
    aSource->SetupCairoStrokeGeometry(ctx);
    cairo_stroke_extents(ctx, &xmin, &ymin, &xmax, &ymax);
  } else
    cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);

  cairo_user_to_device(ctx, &xmin, &ymin);
  cairo_user_to_device(ctx, &xmax, &ymax);

  cairo_destroy(ctx);

  *aRect = nsSVGUtils::ToBoundingPixelRect(xmin, ymin, xmax, ymax);

  return NS_OK;
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGCairoPathGeometry::ContainsPoint(nsSVGPathGeometryFrame *aSource,
                                      float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);
  cairo_set_tolerance(ctx, 1.0);

  GeneratePath(aSource, ctx, nsnull);
  double xx = x, yy = y;
  cairo_device_to_user(ctx, &xx, &yy);

  if (aSource->IsClipChild()) {
    if (aSource->GetClipRule() == NS_STYLE_FILL_RULE_EVENODD)
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_EVEN_ODD);
    else
      cairo_set_fill_rule(ctx, CAIRO_FILL_RULE_WINDING);
  }

  PRUint16 mask = 0;
  aSource->GetHittestMask(&mask);
  if (mask & nsISVGPathGeometrySource::HITTEST_MASK_FILL)
    *_retval = cairo_in_fill(ctx, xx, yy);
  if (!*_retval && (mask & nsISVGPathGeometrySource::HITTEST_MASK_STROKE))
    *_retval = cairo_in_stroke(ctx, xx, yy);

  cairo_destroy(ctx);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoPathGeometry::GetBoundingBox(nsSVGPathGeometryFrame *aSource,
                                       nsIDOMSVGRect * *aBoundingBox)
{
  *aBoundingBox = nsnull;

  double xmin, ymin, xmax, ymax;

  cairo_t *ctx = cairo_create(gSVGCairoDummySurface);
  GeneratePath(aSource, ctx, nsnull);

  cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);
#ifdef DEBUG_scooter
  printf("CairoPathGeometry::GetBoundingBox returns (%f,%f,%f,%f)\n",xmin,ymin,xmax,ymax);
#endif

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

  return NS_NewSVGRect(aBoundingBox, xmin, ymin, xmax - xmin, ymax - ymin);
}
