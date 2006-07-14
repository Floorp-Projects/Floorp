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
#include "nsPromiseFlatString.h"
#include "nsPresContext.h"
#include "nsMemory.h"
#include <cairo.h>

#include "nsIDOMSVGRect.h"
#include "nsSVGRect.h"
#include "nsSVGGlyphFrame.h"
#include "nsSVGMatrix.h"
#include "nsSVGUtils.h"

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
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsresult
NS_NewSVGCairoGlyphGeometry(nsISVGRendererGlyphGeometry **result)
{
  *result = new nsSVGCairoGlyphGeometry;
  if (!*result) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

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

static void
LoopCharacters(cairo_t *aCtx, nsAString &aText, nsSVGCharacterPosition *aCP,
               void (*aFunc)(cairo_t *cr, const char *utf8))
{
  if (!aCP) {
    aFunc(aCtx, NS_ConvertUTF16toUTF8(aText).get());
  } else {
    for (PRUint32 i = 0; i < aText.Length(); i++) {
      /* character actually on the path? */
      if (aCP[i].draw == PR_FALSE)
        continue;
      cairo_matrix_t matrix;
      cairo_get_matrix(aCtx, &matrix);
      cairo_move_to(aCtx, aCP[i].x, aCP[i].y);
      cairo_rotate(aCtx, aCP[i].angle);
      aFunc(aCtx, NS_ConvertUTF16toUTF8(Substring(aText, i, 1)).get());
      cairo_set_matrix(aCtx, &matrix);
    }
  }
}

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::Render(nsSVGGlyphFrame *aSource, 
                                nsISVGRendererCanvas *canvas)
{
  nsCOMPtr<nsISVGCairoCanvas> cairoCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(cairoCanvas, "wrong svg render context for geometry!");
  if (!cairoCanvas) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString text;
  aSource->GetCharacterData(text);

  if (text.IsEmpty()) {
    return NS_OK;
  }

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  cairo_t *ctx = cairoCanvas->GetContext();

  aSource->SelectFont(ctx);

  nsresult rv = aSource->GetCharacterPosition(ctx, getter_Transfers(cp));

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

  rv = GetGlobalTransform(aSource, ctx, cairoCanvas);
  if (NS_FAILED(rv)) {
    if (renderMode == nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL)
      cairo_restore(ctx);
    return rv;
  }

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

    if (renderMode == nsISVGRendererCanvas::SVG_RENDER_MODE_CLIP_MASK) {
      cairo_set_antialias(ctx, CAIRO_ANTIALIAS_NONE);
      cairo_set_source_rgba(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
      LoopCharacters(ctx, text, cp, cairo_show_text);
    } else {
      LoopCharacters(ctx, text, cp, cairo_text_path);
    }

    cairo_set_matrix(ctx, &matrix);

    return NS_OK;
  }

  void *closure;
  if (aSource->HasFill() &&
      NS_SUCCEEDED(aSource->SetupCairoFill(canvas, ctx, &closure))) {
    LoopCharacters(ctx, text, cp, cairo_show_text);
    aSource->CleanupCairoFill(ctx, closure);
  }
  
  if (aSource->HasStroke() &&
      NS_SUCCEEDED(aSource->SetupCairoStroke(canvas, ctx, &closure))) {
    cairo_new_path(ctx);
    if (!cp)
      cairo_move_to(ctx, x, y);
    LoopCharacters(ctx, text, cp, cairo_text_path);
    cairo_stroke(ctx);
    aSource->CleanupCairoStroke(ctx, closure);
    cairo_new_path(ctx);
  }

  cairo_restore(ctx);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoGlyphGeometry::GetCoveredRegion(nsSVGGlyphFrame *aSource,
                                          nsRect *aRect)
{
  aRect->Empty();

  PRBool hasFill = aSource->HasFill();
  PRBool hasStroke = aSource->HasStroke();

  if (!hasFill && !hasStroke) {
    return NS_OK;
  }

  nsAutoString text;
  aSource->GetCharacterData(text);
  
  if (text.IsEmpty()) {
    return NS_OK;
  }

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(aSource);

  nsresult rv = aSource->GetCharacterPosition(ctx, getter_Transfers(cp));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetGlobalTransform(aSource, ctx, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  float x,y;
  if (!cp) {
    aSource->GetX(&x);
    aSource->GetY(&y);
    cairo_move_to(ctx, x, y);
  } else {
    x = 0.0, y = 0.0;
  }

  if (!cp) {
    if (hasStroke) {
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
      if (hasStroke) {
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

  if (hasStroke) {
    aSource->SetupCairoStrokeGeometry(ctx);
    cairo_stroke_extents(ctx, &xmin, &ymin, &xmax, &ymax);
  } else {
    cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);
  }

  cairo_user_to_device(ctx, &xmin, &ymin);
  cairo_user_to_device(ctx, &xmax, &ymax);

  *aRect = nsSVGUtils::ToBoundingPixelRect(xmin, ymin, xmax, ymax);

  return NS_OK;
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGCairoGlyphGeometry::ContainsPoint(nsSVGGlyphFrame *aSource,
                                       float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;

  nsAutoString text;
  aSource->GetCharacterData(text);

  if (text.IsEmpty()) {
    return NS_OK;
  }

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(aSource);

  nsresult rv = aSource->GetCharacterPosition(ctx, getter_Transfers(cp));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetGlobalTransform(aSource, ctx, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

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

  nsAutoString text;
  aSource->GetCharacterData(text);

  if (text.IsEmpty()) {
    return NS_OK;
  }

  nsAutoArrayPtr<nsSVGCharacterPosition> cp;

  nsSVGAutoGlyphHelperContext ctx(aSource);

  nsresult rv = aSource->GetCharacterPosition(ctx, getter_Transfers(cp));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetGlobalTransform(aSource, ctx, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  float x,y;
  if (!cp) {
    aSource->GetX(&x);
    aSource->GetY(&y);
    cairo_move_to(ctx, x, y);
  }

  LoopCharacters(ctx, text, cp, cairo_text_path);

  double xmin, ymin, xmax, ymax;

  cairo_fill_extents(ctx, &xmin, &ymin, &xmax, &ymax);

  cairo_user_to_device(ctx, &xmin, &ymin);
  cairo_user_to_device(ctx, &xmax, &ymax);

  return NS_NewSVGRect(aBoundingBox, xmin, ymin, xmax - xmin, ymax - ymin);
}
