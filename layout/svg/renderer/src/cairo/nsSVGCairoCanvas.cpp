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
