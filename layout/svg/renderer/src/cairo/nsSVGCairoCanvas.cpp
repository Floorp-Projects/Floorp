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

#ifdef MOZ_X11
extern "C" {
#include <cairo-xlib.h>
}
#endif

#include "nsIImage.h"
#include "nsIComponentManager.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#ifdef XP_MACOSX
#include "nsIDrawingSurfaceMac.h"
extern "C" {
#include <cairo-quartz.h>
}
#elif defined(MOZ_WIDGET_XLIB)
#include "nsDrawingSurfaceXlib.h"
#elif defined(MOZ_WIDGET_QT)
#undef CursorShape
#include "nsDrawingSurfaceQt.h"
#elif defined(XP_WIN)
#include "nsDrawingSurfaceWin.h"
extern "C" {
#include <cairo-win32.h>
}
#else
#include "nsRenderingContextGTK.h"
#include <gdk/gdkx.h>
#endif

/*
 * The quartz and win32 backends in cairo currently (1.0.x) don't
 * implement acceleration to be useful, and in fact slow down
 * rendering due to the repeated locking/unlocking of the target
 * surface.  Change these to defines if you want to test the native
 * surfaces.
 */
#undef MOZ_ENABLE_QUARTZ_SURFACE
#undef MOZ_ENABLE_WIN32_SURFACE

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

  // image fallback data
  nsSize mSrcSizeTwips;
  nsRect mDestRectScaledTwips;
  nsCOMPtr<imgIContainer> mContainer;
  nsCOMPtr<gfxIImageFrame> mBuffer;
  PRUint8 *mData;

  PRUint16 mRenderMode;
  PRPackedBool mOwnsCR;
  PRPackedBool mPreBlendImage;
};


/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGCairoCanvas::nsSVGCairoCanvas() : mData(nsnull)
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

  if (mData) {
    free(mData);
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
  mPreBlendImage = PR_FALSE;

#if defined(MOZ_ENABLE_CAIRO_GFX)
  mOwnsCR = PR_FALSE;
  void* data = ctx->GetNativeGraphicData(nsIRenderingContext::NATIVE_CAIRO_CONTEXT);
  mCR = (cairo_t*)data;
  cairo_get_matrix(mCR, &mInitialTransform);
  return NS_OK;

#else // !MOZ_ENABLE_CAIRO_GFX
  cairo_surface_t* cairoSurf = nsnull;
#if defined(XP_MACOSX) && defined(MOZ_ENABLE_QUARTZ_SURFACE)
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

#elif defined(MOZ_WIDGET_XLIB)
  nsIDrawingSurfaceXlib* surface;
  ctx->GetDrawingSurface((nsIDrawingSurface**)&surface);

  surface->GetDimensions(&mWidth, &mHeight);

  XlibRgbHandle* handle;
  surface->GetXlibRgbHandle(handle);
  Drawable drawable = surface->GetDrawable();
  cairoSurf = cairo_xlib_surface_create(xxlib_rgb_get_display(handle),
                                        drawable,
                                        xxlib_rgb_get_visual(handle),
                                        mWidth, mHeight);
#elif defined(MOZ_WIDGET_QT)
  nsDrawingSurfaceQt* surface;
  ctx->GetDrawingSurface((nsIDrawingSurface**)&surface);
  surface->GetDimensions(&mWidth, &mHeight);

  QPaintDevice* dev = surface->GetPaintDevice();

  cairoSurf = cairo_xlib_surface_create(dev->x11Display(),
                                        dev->handle(),
                                        (Visual*)dev->x11Visual(),
                                        mWidth, mHeight);
#elif defined(XP_WIN)
  nsDrawingSurfaceWin *surface;
  ctx->GetDrawingSurface((nsIDrawingSurface**)&surface);

  PRInt32 canRaster;
  surface->GetTECHNOLOGY(&canRaster);
  if (canRaster == DT_RASPRINTER)
    mPreBlendImage = PR_TRUE;
#if defined(MOZ_ENABLE_WIN32_SURFACE)
  if (canRaster != DT_RASPRINTER) {
    HDC hdc;
    surface->GetDimensions(&mWidth, &mHeight);
    surface->GetDC(&hdc);
    cairoSurf = cairo_win32_surface_create(hdc);
  }
#endif
#elif defined(MOZ_ENABLE_GTK2) || defined(MOZ_ENABLE_GTK)
  nsDrawingSurfaceGTK *surface;
  ctx->GetDrawingSurface((nsIDrawingSurface**)&surface);
  if (surface) {
    surface->GetSize(&mWidth, &mHeight);
    GdkDrawable *drawable = surface->GetDrawable();
    GdkVisual *visual = gdk_window_get_visual(drawable);
    cairoSurf = cairo_xlib_surface_create(GDK_WINDOW_XDISPLAY(drawable),
                                          GDK_WINDOW_XWINDOW(drawable),
                                          GDK_VISUAL_XVISUAL(visual),
                                          mWidth, mHeight);
  }
#endif

  // Account for the transform in the rendering context. We set dx,dy
  // to the translation required in the cairo context that will
  // ensure the cairo context's origin coincides with the top-left
  // of the area we need to draw.
  nsTransform2D* xform;
  mMozContext->GetCurrentTransform(xform);
  float dx, dy;
  xform->GetTranslation(&dx, &dy);
  
  if (!cairoSurf) {
    /* printing or some platform we don't know about yet - use an image */
    float scaledTwipsPerPx = presContext->ScaledPixelsToTwips();
    mDestRectScaledTwips = dirtyRect;
    mDestRectScaledTwips.ScaleRoundOut(scaledTwipsPerPx);
    
    float twipsPerPx = presContext->PixelsToTwips();
    nsRect r = dirtyRect;
    r.ScaleRoundOut(twipsPerPx);
    mSrcSizeTwips = r.Size();
  
    mWidth = dirtyRect.width;
    mHeight = dirtyRect.height;

    mContainer = do_CreateInstance("@mozilla.org/image/container;1");
    mContainer->Init(mWidth, mHeight, nsnull);
    
    mBuffer = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
    if (mPreBlendImage) {
#ifdef XP_WIN
      mBuffer->Init(0, 0, mWidth, mHeight, gfxIFormats::BGR, 24);
#else
      mBuffer->Init(0, 0, mWidth, mHeight, gfxIFormats::RGB, 24);
#endif
    } else {
#ifdef XP_WIN
      mBuffer->Init(0, 0, mWidth, mHeight, gfxIFormats::BGR_A8, 24);
#else
      mBuffer->Init(0, 0, mWidth, mHeight, gfxIFormats::RGB_A8, 24);
#endif
    }
    mContainer->AppendFrame(mBuffer);

    mData = (PRUint8 *)calloc(4 * mWidth * mHeight, 1);
    if (!mData)
      return NS_ERROR_FAILURE;

    cairoSurf = cairo_image_surface_create_for_data(mData, CAIRO_FORMAT_ARGB32,
                                                    mWidth, mHeight, 4 * mWidth);
    
    // Ensure that dirtyRect.TopLeft() is translated to (0,0) in the surface                            
    dx = -dirtyRect.x;
    dy = -dirtyRect.y;
  }

  mOwnsCR = PR_TRUE;
  mCR = cairo_create(cairoSurf);
  // Destroy our reference to the surface; the cairo_t will continue to hold a reference to it
  cairo_surface_destroy(cairoSurf);

  cairo_translate(mCR, dx, dy);
  cairo_get_matrix(mCR, &mInitialTransform);

#if defined(DEBUG_tor) || defined(DEBUG_roc) || defined(DEBUG_scooter)
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

// XXX this is copied from nsCanvasRenderingContext2D, which sucks,
// but all this is going to go away in 1.9 so making it shareable is
// a bit of unnecessary pain
static nsresult CopyCairoImageToIImage(PRUint8* aData, PRInt32 aWidth, PRInt32 aHeight,
                                       gfxIImageFrame* aImage, PRBool aPreBlend)
{
  PRUint8 *alphaBits = nsnull, *rgbBits;
  PRUint32 alphaLen = 0, rgbLen;
  PRUint32 alphaStride = 0, rgbStride;

  nsresult rv = aImage->LockImageData();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!aPreBlend) {
    rv = aImage->LockAlphaData();
    if (NS_FAILED(rv)) {
      aImage->UnlockImageData();
      return rv;
    }
  }

  rv = aImage->GetImageBytesPerRow(&rgbStride);
  rv |= aImage->GetImageData(&rgbBits, &rgbLen);

  if (!aPreBlend) {
    rv |= aImage->GetAlphaBytesPerRow(&alphaStride);
    rv |= aImage->GetAlphaData(&alphaBits, &alphaLen);
  }

  if (NS_FAILED(rv)) {
    aImage->UnlockImageData();
    if (!aPreBlend)
      aImage->UnlockAlphaData();
    return rv;
  }

  nsCOMPtr<nsIImage> img(do_GetInterface(aImage));
  PRBool topToBottom = img->GetIsRowOrderTopToBottom();

  for (PRUint32 j = 0; j < (PRUint32) aHeight; j++) {
    PRUint8 *inrow = (PRUint8*)(aData + (aWidth * 4 * j));

    PRUint32 rowIndex;
    if (topToBottom)
      rowIndex = j;
    else
      rowIndex = aHeight - j - 1;

    PRUint8 *outrowrgb = rgbBits + (rgbStride * rowIndex);
    PRUint8 *outrowalpha = alphaBits + (alphaStride * rowIndex);

    for (PRUint32 i = 0; i < (PRUint32) aWidth; i++) {
#ifdef IS_LITTLE_ENDIAN
      PRUint8 b = *inrow++;
      PRUint8 g = *inrow++;
      PRUint8 r = *inrow++;
      PRUint8 a = *inrow++;
#else
      PRUint8 a = *inrow++;
      PRUint8 r = *inrow++;
      PRUint8 g = *inrow++;
      PRUint8 b = *inrow++;
#endif
      // now recover the real bgra from the cairo
      // premultiplied values
      if (a == 0) {
        // can't do much for us if we're at 0
        b = g = r = 0;
      } else {
        // the (a/2) factor is a bias similar to one cairo applies
        // when premultiplying
        b = (b * 255 + a / 2) / a;
        g = (g * 255 + a / 2) / a;
        r = (r * 255 + a / 2) / a;
      }

      if (!aPreBlend)
        *outrowalpha++ = a;

#ifdef XP_MACOSX
      // On the mac, RGB_A8 is really RGBX_A8
      *outrowrgb++ = 0;
#endif

#ifdef XP_WIN
      // On windows, RGB_A8 is really BGR_A8.
      // in fact, BGR_A8 is also BGR_A8.
      if (!aPreBlend) {
        *outrowrgb++ = b;
        *outrowrgb++ = g;
        *outrowrgb++ = r;
      } else {
        MOZ_BLEND(*outrowrgb++, 255, b, (unsigned)a);
        MOZ_BLEND(*outrowrgb++, 255, g, (unsigned)a);
        MOZ_BLEND(*outrowrgb++, 255, r, (unsigned)a);
      }
#else
      if (!aPreBlend) {
        *outrowrgb++ = r;
        *outrowrgb++ = g;
        *outrowrgb++ = b;
      } else {
        MOZ_BLEND(*outrowrgb++, 255, r, (unsigned)a);
        MOZ_BLEND(*outrowrgb++, 255, g, (unsigned)a);
        MOZ_BLEND(*outrowrgb++, 255, b, (unsigned)a);
      }
#endif
    }
  }

  if (!aPreBlend)
    rv = aImage->UnlockAlphaData();
  else
    rv = NS_OK;
  rv |= aImage->UnlockImageData();
  if (NS_FAILED(rv))
    return rv;

  nsRect r(0, 0, aWidth, aHeight);
  img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);
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

  if (mData) {
    // ensure that everything's flushed
    cairo_destroy(mCR);
    mCR = nsnull;
    mOwnsCR = PR_FALSE;
    
    nsCOMPtr<nsIDeviceContext> ctx;
    mMozContext->GetDeviceContext(*getter_AddRefs(ctx));

    nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(mBuffer));
    if (ireq) {
      nsCOMPtr<gfxIImageFrame> img(do_GetInterface(ireq));
      CopyCairoImageToIImage(mData, mWidth, mHeight, img, mPreBlendImage);
    }
    mContainer->DecodingComplete();

    nsRect src(0, 0, mSrcSizeTwips.width, mSrcSizeTwips.height);
    mMozContext->DrawImage(mContainer, src, mDestRectScaledTwips);
  }

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
  cairo_matrix_t inverse = matrix;
  if (cairo_matrix_invert(&inverse))
    return NS_ERROR_FAILURE;
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

