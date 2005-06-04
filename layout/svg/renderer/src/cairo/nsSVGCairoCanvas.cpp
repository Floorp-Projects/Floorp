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
#include "nsISVGCairoSurface.h"
#include <cairo.h>

#ifdef XP_MACOSX
#include "nsIDrawingSurfaceMac.h"
extern "C" {
#include <cairo-quartz.h>
}
#elif defined(XP_WIN)
#include "nsDrawingSurfaceWin.h"
extern "C" {
#include <cairo-win32.h>
}
#else
#include "nsRenderingContextGTK.h"
#include <gdk/gdkx.h>
extern "C" {
#include <cairo-xlib.h>
}
#endif

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
  NS_IMETHOD AdjustMatrixForInitialTransform(cairo_matrix_t* aMatrix);
    
private:
  nsCOMPtr<nsIRenderingContext> mMozContext;
  nsCOMPtr<nsPresContext> mPresContext;
  cairo_t *mCR;
  PRUint32 mWidth, mHeight;
  cairo_matrix_t mInitialTransform;
  nsVoidArray mContextStack;

#ifdef XP_MACOSX
  nsCOMPtr<nsIDrawingSurfaceMac> mSurface;
  CGContextRef mQuartzRef;
#endif

  PRUint16 mRenderMode;
  PRPackedBool mOwnsCR;
};


/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGCairoCanvas::nsSVGCairoCanvas()
{
  mOwnsCR = PR_FALSE;
}

nsSVGCairoCanvas::~nsSVGCairoCanvas()
{
  mMozContext = nsnull;
  mPresContext = nsnull;

  if (mOwnsCR) {
    cairo_destroy(mCR);
  }
}

nsresult
nsSVGCairoCanvas::AdjustMatrixForInitialTransform(cairo_matrix_t* aMatrix)
{
  if (mContextStack.Count() == 0)
    cairo_matrix_multiply(aMatrix, aMatrix, &mInitialTransform);
  return NS_OK;
}

nsresult
nsSVGCairoCanvas::Init(nsIRenderingContext *ctx,
                       nsPresContext *presContext,
                       const nsRect &dirtyRect)
{
  mPresContext = presContext;
  mMozContext = ctx;
  NS_ASSERTION(mMozContext, "empty rendering context");

  mRenderMode = SVG_RENDER_MODE_NORMAL;

#if defined(MOZ_ENABLE_CAIRO_GFX)
  mOwnsCR = PR_FALSE;
  void* data;
  ctx->RetrieveCurrentNativeGraphicData(&data);
  mCR = (cairo_t*)data;
  cairo_get_matrix(mCR, &mInitialTranslation);
  return NS_OK;

#else // !MOZ_ENABLE_CAIRO_GFX
  cairo_surface_t* cairoSurf = nsnull;
#if defined(XP_MACOSX)
  nsIDrawingSurface *surface;
  ctx->GetDrawingSurface(&surface);
  mSurface = do_QueryInterface(surface);
  surface->GetDimensions(&mWidth, &mHeight);
  mQuartzRef = mSurface->StartQuartzDrawing();

  CGrafPtr port;
  mSurface->GetGrafPtr(&port);
  Rect portRect;
  ::GetPortBounds(port, &portRect);

#ifdef DEBUG_tor
  fprintf(stderr, "CAIRO: DS=0x%08x port x=%d y=%d w=%d h=%d\n",
          mSurface.get(), portRect.right, portRect.top,
          portRect.right - portRect.left, portRect.bottom - portRect.top);
#endif

  ::SyncCGContextOriginWithPort(mQuartzRef, port);
  cairoSurf = cairo_quartz_surface_create(mQuartzRef,
                                          portRect.right - portRect.left,
                                          portRect.bottom - portRect.top);
#elif defined(XP_WIN)
  nsDrawingSurfaceWin *surface;
  HDC hdc;
  ctx->GetDrawingSurface((nsIDrawingSurface**)&surface);
  surface->GetDimensions(&mWidth, &mHeight);
  surface->GetDC(&hdc);
  cairoSurf = cairo_win32_surface_create(hdc);
#elif defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_GTK)
  nsDrawingSurfaceGTK *surface;
  ctx->GetDrawingSurface((nsIDrawingSurface**)&surface);
  surface->GetSize(&mWidth, &mHeight);
  GdkDrawable *drawable = surface->GetDrawable();
  GdkVisual *visual = gdk_window_get_visual(drawable);
  cairoSurf = cairo_xlib_surface_create(GDK_WINDOW_XDISPLAY(drawable),
                                        GDK_WINDOW_XWINDOW(drawable),
                                        GDK_VISUAL_XVISUAL(visual),
                                        mWidth, mHeight);
#endif

  NS_ASSERTION(cairoSurf, "No surface");
  mOwnsCR = PR_TRUE;
  mCR = cairo_create(cairoSurf);

  // get the translation set on the rendering context. It will be in
  // displayunits (i.e. pixels*scale), *not* pixels:
  nsTransform2D* xform;
  mMozContext->GetCurrentTransform(xform);
  float dx, dy;
  xform->GetTranslation(&dx, &dy);
  cairo_translate(mCR, dx, dy);
  cairo_get_matrix(mCR, &mInitialTransform);

#if defined(DEBUG_tor) || defined(DEBUG_roc)
  fprintf(stderr, "cairo translate: %f %f\n", dx, dy);

  fprintf(stderr, "cairo dirty: %d %d %d %d\n",
                  dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
#endif

  // clip to dirtyRect
  cairo_new_path(mCR);
  cairo_rectangle(mCR,
                  dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
  cairo_clip(mCR);
  cairo_new_path(mCR);

  return NS_OK;
#endif // !MOZ_ENABLE_CAIRO_GFX
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
  cairo_set_source_rgba(mCR,
                        NS_GET_R(color)/255.0,
                        NS_GET_G(color)/255.0,
                        NS_GET_B(color)/255.0,
                        NS_GET_A(color)/255.0);
  cairo_rectangle(mCR, 0, 0, mWidth, mHeight);
  cairo_fill(mCR);

  return NS_OK;
}

/** Implements void flush(); */
NS_IMETHODIMP
nsSVGCairoCanvas::Flush()
{
#ifdef XP_MACOSX
  if (mSurface)
    mSurface->EndQuartzDrawing(mQuartzRef);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoCanvas::GetRenderMode(PRUint16 *aMode)
{
  *aMode = mRenderMode;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGCairoCanvas::SetRenderMode(PRUint16 mode)
{
  if (mRenderMode == SVG_RENDER_MODE_CLIP && mode == SVG_RENDER_MODE_NORMAL)
    cairo_clip(mCR);
  mRenderMode = mode;
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

  cairo_matrix_t oldMatrix;
  cairo_get_matrix(mCR, &oldMatrix);
  cairo_matrix_t matrix = {m[0], m[1], m[2], m[3], m[4], m[5]};
  cairo_transform(mCR, &matrix);

  cairo_new_path(mCR);
  cairo_rectangle(mCR, aX, aY, aWidth, aHeight);
  cairo_clip(mCR);
  cairo_new_path(mCR);

  cairo_set_matrix(mCR, &oldMatrix);

  return NS_OK;
}

/** Implements pushSurface(in nsISVGRendererSurface surface); */
NS_IMETHODIMP
nsSVGCairoCanvas::PushSurface(nsISVGRendererSurface *aSurface)
{
  nsCOMPtr<nsISVGCairoSurface> cairoSurface = do_QueryInterface(aSurface);
  if (!cairoSurface)
    return NS_ERROR_FAILURE;

  cairo_t* oldCR = mCR;
  mContextStack.AppendElement(NS_STATIC_CAST(void*, oldCR));

  mCR = cairo_create(cairoSurface->GetSurface());
  // XXX Copy state over from oldCR?

  return NS_OK;
}

/** Implements popSurface(); */
NS_IMETHODIMP
nsSVGCairoCanvas::PopSurface()
{
  PRUint32 count = mContextStack.Count();
  if (count != 0) {
    cairo_destroy(mCR);
    mCR = NS_STATIC_CAST(cairo_t*, mContextStack[count - 1]);
    mContextStack.RemoveElementAt(count - 1);
  }

  return NS_OK;
}

/** Implements  void compositeSurface(in nsISVGRendererSurface surface,
                                      in unsigned long x, in unsigned long y,
                                      in float opacity); */
NS_IMETHODIMP
nsSVGCairoCanvas::CompositeSurface(nsISVGRendererSurface *aSurface,
                                   PRUint32 aX, PRUint32 aY, float aOpacity)
{
  nsCOMPtr<nsISVGCairoSurface> cairoSurface = do_QueryInterface(aSurface);
  if (!cairoSurface)
    return NS_ERROR_FAILURE;

  cairo_save(mCR);
  cairo_translate(mCR, aX, aY);

  PRUint32 width, height;
  aSurface->GetWidth(&width);
  aSurface->GetHeight(&height);

  cairo_set_source_surface(mCR, cairoSurface->GetSurface(), 0.0, 0.0);
  cairo_paint_with_alpha(mCR, aOpacity);
  cairo_restore(mCR);

  return NS_OK;
}

/** Implements  void compositeSurface(in nsISVGRendererSurface surface,
                                      in nsIDOMSVGMatrix canvasTM,
                                      in float opacity); */
NS_IMETHODIMP
nsSVGCairoCanvas::CompositeSurfaceMatrix(nsISVGRendererSurface *aSurface,
                                         nsIDOMSVGMatrix *aCTM, float aOpacity)
{
  nsCOMPtr<nsISVGCairoSurface> cairoSurface = do_QueryInterface(aSurface);
  if (!cairoSurface)
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

  cairo_matrix_t matrix = {m[0], m[1], m[2], m[3], m[4], m[5]};
  cairo_transform(mCR, &matrix);

  PRUint32 width, height;
  aSurface->GetWidth(&width);
  aSurface->GetHeight(&height);

  cairo_set_source_surface(mCR, cairoSurface->GetSurface(), 0.0, 0.0);
  cairo_paint_with_alpha(mCR, aOpacity);
  cairo_restore(mCR);

  return NS_OK;
}

