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
 *    layout/svg/renderer/src/libart/nsSVGLibartCanvas.cpp
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
#include "nsSVGCairoCanvas.h"
#include "nsISVGCairoCanvas.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsRenderingContextGTK.h"
#include <gdk/gdkx.h>
#include <cairo.h>

/**
 * \addtogroup cairo_renderer Cairo Rendering Engine
 * @{
 */
//////////////////////////////////////////////////////////////////////
/**
 * Cairo canvas implementation
 */
class nsSVGCairoCanvas : public nsISVGCairoCanvas
{
public:
  nsSVGCairoCanvas();
  ~nsSVGCairoCanvas();
  nsresult Init(nsIRenderingContext* ctx, nsPresContext* presContext,
                const nsRect & dirtyRect);
    
  // nsISupports interface:
  NS_DECL_ISUPPORTS
    
  // nsISVGRendererCanvas interface:
  NS_DECL_NSISVGRENDERERCANVAS

  // nsISVGCairoCanvas interface:
  NS_IMETHOD_(cairo_t*) GetContext() { return mCR; }
    
private:
  nsCOMPtr<nsIRenderingContext> mMozContext;
  nsCOMPtr<nsPresContext> mPresContext;
  cairo_t *mCR;
  PRUint32 mWidth, mHeight;
};


/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGCairoCanvas::nsSVGCairoCanvas()
{
}

nsSVGCairoCanvas::~nsSVGCairoCanvas()
{
  mMozContext = nsnull;
  mPresContext = nsnull;

  cairo_destroy(mCR);
}

nsresult
nsSVGCairoCanvas::Init(nsIRenderingContext *ctx,
                       nsPresContext *presContext,
                       const nsRect &dirtyRect)
{
  mPresContext = presContext;
  mMozContext = ctx;
  NS_ASSERTION(mMozContext, "empty rendering context");

  nsDrawingSurfaceGTK *surface;
  ctx->GetDrawingSurface((nsIDrawingSurface**)&surface);
  surface->GetSize(&mWidth, &mHeight);
  GdkDrawable *drawable = surface->GetDrawable();

  mCR = cairo_create();
  cairo_set_target_drawable(mCR,
                            GDK_WINDOW_XDISPLAY(drawable),
                            GDK_WINDOW_XWINDOW(drawable));

  // get the translation set on the rendering context. It will be in
  // displayunits (i.e. pixels*scale), *not* pixels:
  nsTransform2D* xform;
  mMozContext->GetCurrentTransform(xform);
  float dx, dy;
  xform->GetTranslation(&dx, &dy);
  cairo_translate(mCR, dx, dy);

  return NS_OK;
}

nsresult
NS_NewSVGCairoCanvas(nsISVGRendererCanvas **result,
                     nsIRenderingContext *ctx,
                     nsPresContext *presContext,
                     const nsRect & dirtyRect)
{
  nsSVGCairoCanvas* pg = new nsSVGCairoCanvas();
  if (!pg) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(pg);

  nsresult rv = pg->Init(ctx, presContext, dirtyRect);

  if (NS_FAILED(rv)) {
    NS_RELEASE(pg);
    return rv;
  }
  
  *result = pg;
  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGCairoCanvas)
NS_IMPL_RELEASE(nsSVGCairoCanvas)

NS_INTERFACE_MAP_BEGIN(nsSVGCairoCanvas)
NS_INTERFACE_MAP_ENTRY(nsISVGRendererCanvas)
NS_INTERFACE_MAP_ENTRY(nsISVGCairoCanvas)
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererCanvas methods:

/** Implements [noscript] nsIRenderingContext lockRenderingContext(const in nsRectRef rect); */
NS_IMETHODIMP
nsSVGCairoCanvas::LockRenderingContext(const nsRect & rect,
                                       nsIRenderingContext **_retval)
{
  // XXX do we need to flush?
  Flush();
  
  *_retval = mMozContext;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/** Implements void unlockRenderingContext(); */
NS_IMETHODIMP 
nsSVGCairoCanvas::UnlockRenderingContext()
{
  return NS_OK;
}

/** Implements nsPresContext getPresContext(); */
NS_IMETHODIMP
nsSVGCairoCanvas::GetPresContext(nsPresContext **_retval)
{
  *_retval = mPresContext;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/** Implements void clear(in nscolor color); */
NS_IMETHODIMP
nsSVGCairoCanvas::Clear(nscolor color)
{
  cairo_set_rgb_color(mCR,
                      NS_GET_R(color)/255.0,
                      NS_GET_G(color)/255.0,
                      NS_GET_B(color)/255.0);
  cairo_rectangle(mCR, 0, 0, mWidth, mHeight);
  cairo_fill(mCR);

  return NS_OK;
}

/** Implements void flush(); */
NS_IMETHODIMP
nsSVGCairoCanvas::Flush()
{
  return NS_OK;
}

/** Implements pushClip(); */
NS_IMETHODIMP
nsSVGCairoCanvas::PushClip()
{
  cairo_save(mCR);
  return NS_OK;
}

/** Implements popClip(); */
NS_IMETHODIMP
nsSVGCairoCanvas::PopClip()
{
  cairo_restore(mCR);
  return NS_OK;
}

/** Implements setClipRect(in nsIDOMSVGMatrix canvasTM, in float x, in float y,
    in float width, in float height); */
NS_IMETHODIMP
nsSVGCairoCanvas::SetClipRect(nsIDOMSVGMatrix *aCTM, float aX, float aY,
                              float aWidth, float aHeight)
{
  if (!aCTM)
    return NS_ERROR_FAILURE;

  cairo_save(mCR);

  float m[6];
  float val;
  aCTM->GetA(&val);
  m[0] = val;
    
  aCTM->GetB(&val);
  m[1] = val;
    
  aCTM->GetC(&val);  
  m[2] = val;  
    
  aCTM->GetD(&val);  
  m[3] = val;  
  
  aCTM->GetE(&val);
  m[4] = val;
  
  aCTM->GetF(&val);
  m[5] = val;

  cairo_matrix_t *matrix = cairo_matrix_create();
  if (!matrix) {
    cairo_restore(mCR);
    return NS_ERROR_FAILURE;
  }

  cairo_matrix_set_affine(matrix, m[0], m[1], m[2], m[3], m[4], m[5]);
  cairo_concat_matrix(mCR, matrix);
  cairo_matrix_destroy(matrix);

  double x[4], y[4];
  x[0] = aX;
  y[0] = aY;
  x[1] = aX + aWidth;
  y[1] = aY;
  x[2] = aX + aWidth;
  y[2] = aY + aHeight;
  x[3] = aX;
  y[3] = aY + aHeight;

  cairo_transform_point(mCR, &x[0], &y[0]);
  cairo_transform_point(mCR, &x[1], &y[1]);
  cairo_transform_point(mCR, &x[2], &y[2]);
  cairo_transform_point(mCR, &x[3], &y[3]);

  cairo_restore(mCR);

  cairo_new_path(mCR);
  cairo_move_to(mCR, x[0], y[0]);
  cairo_line_to(mCR, x[1], y[1]);
  cairo_line_to(mCR, x[2], y[2]);
  cairo_line_to(mCR, x[3], y[3]);
  cairo_close_path(mCR);
  cairo_clip(mCR);
  cairo_new_path(mCR);

  return NS_OK;
}
