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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include <Gdiplus.h>
using namespace Gdiplus;

#include "nsCOMPtr.h"
#include "nsSVGGDIPlusCanvas.h"
#include "nsISVGGDIPlusCanvas.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsIRenderingContextWin.h"
#include "nsIDOMSVGMatrix.h"

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */
//////////////////////////////////////////////////////////////////////
/**
 * GDI+ canvas implementation
 */
class nsSVGGDIPlusCanvas : public nsISVGGDIPlusCanvas
{
public:
  nsSVGGDIPlusCanvas();
  ~nsSVGGDIPlusCanvas();
  nsresult Init(nsIRenderingContext* ctx, nsPresContext* presContext,
                const nsRect & dirtyRect);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererCanvas interface:
  NS_DECL_NSISVGRENDERERCANVAS

  // nsISVGGDIPlusCanvas interface:
  NS_IMETHOD_(Graphics*) GetGraphics();
  

private:
  nsCOMPtr<nsIRenderingContext> mMozContext;
  nsCOMPtr<nsPresContext> mPresContext;
  Graphics *mGraphics;
  nsVoidArray mClipStack;

#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
  Bitmap *mOffscreenBitmap;
  Graphics *mOffscreenGraphics;
  HDC mOffscreenHDC;
  nsIDrawingSurface* mTempBuffer; // temp storage for during DC locking
#endif
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGGDIPlusCanvas::nsSVGGDIPlusCanvas()
    : mGraphics(nsnull)
#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
      , mOffscreenBitmap(nsnull), mOffscreenGraphics(nsnull), mOffscreenHDC(nsnull)
#endif
{
}

nsSVGGDIPlusCanvas::~nsSVGGDIPlusCanvas()
{
#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
  if (mOffscreenGraphics)
    delete mOffscreenGraphics;
  if (mOffscreenBitmap)
    delete mOffscreenBitmap;
#endif
  if (mGraphics)
    delete mGraphics;
  mMozContext = nsnull;
}

nsresult
nsSVGGDIPlusCanvas::Init(nsIRenderingContext* ctx,
                         nsPresContext* presContext,
                         const nsRect & dirtyRect)
{
  mPresContext = presContext;
  mMozContext = ctx;
  NS_ASSERTION(mMozContext, "empty rendering context");

  HDC hdc;
  // this ctx better be what we think it is...
  mMozContext->RetrieveCurrentNativeGraphicData((PRUint32 *)(&hdc));
    
  mGraphics = new Graphics(hdc);
  if (!mGraphics) return NS_ERROR_FAILURE;

  // Work in pixel units instead of the default DisplayUnits.
  mGraphics->SetPageUnit(UnitPixel);

  // We'll scale our logical 'pixles' to device units according to the
  // resolution of the device. For display devices, the canonical
  // pixel scale will be 1, for printers it will be some other value:
  nsCOMPtr<nsIDeviceContext>  devcontext;
  mMozContext->GetDeviceContext(*getter_AddRefs(devcontext));
  float scale;
  devcontext->GetCanonicalPixelScale(scale);

  mGraphics->SetPageScale(scale);

  
  // get the translation set on the rendering context. It will be in
  // displayunits (i.e. pixels*scale), *not* pixels:
  nsTransform2D* xform;
  mMozContext->GetCurrentTransform(xform);
  float dx, dy;
  xform->GetTranslation(&dx, &dy);

#if defined(DEBUG) && defined(SVG_DEBUG_PRINTING)
  printf("nsSVGGDIPlusCanvas(%p)::Init()[\n", this);
  printf("pagescale=%f\n", scale);
  printf("page unit=%d\n", mGraphics->GetPageUnit());
  printf("]\n");
#endif
  
#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
  mGraphics->TranslateTransform(dx+dirtyRect.x, dy+dirtyRect.y);

  // GDI+ internally works on 32bpp surfaces. Writing directly to
  // Mozilla's backbuffer can be very slow if it hasn't got the right
  // format (!=32bpp).  For complex SVG docs it is advantageous to
  // render to a 32bppPARGB bitmap first:
  mOffscreenBitmap = new Bitmap(dirtyRect.width, dirtyRect.height, PixelFormat32bppPARGB);
  if (!mOffscreenBitmap) return NS_ERROR_FAILURE;
  mOffscreenGraphics = new Graphics(mOffscreenBitmap);
  if (!mOffscreenGraphics) return NS_ERROR_FAILURE;
  mOffscreenGraphics->TranslateTransform((float)-dirtyRect.x, (float)-dirtyRect.y);
#else
  mGraphics->TranslateTransform(dx/scale, dy/scale, MatrixOrderPrepend);
#endif
  
  return NS_OK;
}

nsresult
NS_NewSVGGDIPlusCanvas(nsISVGRendererCanvas **result,
                       nsIRenderingContext *ctx,
                       nsPresContext *presContext,
                       const nsRect & dirtyRect)
{
  nsSVGGDIPlusCanvas* pg = new nsSVGGDIPlusCanvas();
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

NS_IMPL_ADDREF(nsSVGGDIPlusCanvas)
NS_IMPL_RELEASE(nsSVGGDIPlusCanvas)

NS_INTERFACE_MAP_BEGIN(nsSVGGDIPlusCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISVGGDIPlusCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
//  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGRendererCanvas)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererCanvas methods:

/** Implements [noscript] nsIRenderingContext lockRenderingContext(const in nsRectRef rect); */
NS_IMETHODIMP
nsSVGGDIPlusCanvas::LockRenderingContext(const nsRect & rect,
                                         nsIRenderingContext **_retval)
{
#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
  NS_ASSERTION(!mOffscreenHDC, "offscreen hdc already created! Nested rendering context locking?");
  mOffscreenHDC =  mOffscreenGraphics->GetHDC();
  
  nsCOMPtr<nsIRenderingContextWin> wincontext = do_QueryInterface(mMozContext);
  NS_ASSERTION(wincontext, "no windows rendering context");
  
  nsCOMPtr<nsIDrawingSurface> mOffscreenSurface;
  wincontext->CreateDrawingSurface(mOffscreenHDC, (void*&)*getter_AddRefs(mOffscreenSurface));
  mMozContext->GetDrawingSurface(&mTempBuffer);
  mMozContext->SelectOffScreenDrawingSurface(mOffscreenSurface);

#else
  // XXX do we need to flush?
  Flush();
#endif
  
  *_retval = mMozContext;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/** Implements void unlockRenderingContext(); */
NS_IMETHODIMP 
nsSVGGDIPlusCanvas::UnlockRenderingContext()
{
#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
  NS_ASSERTION(mOffscreenHDC, "offscreen hdc already freed! Nested rendering context locking?");

  // restore original surface
  mMozContext->SelectOffScreenDrawingSurface(mTempBuffer);
  mTempBuffer = nsnull;

  mOffscreenGraphics->ReleaseHDC(mOffscreenHDC);
  mOffscreenHDC = nsnull;
#else
  // nothing to do
#endif
  return NS_OK;
}

/** Implements nsPresContext getPresContext(); */
NS_IMETHODIMP
nsSVGGDIPlusCanvas::GetPresContext(nsPresContext **_retval)
{
  *_retval = mPresContext;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/** Implements void clear(in nscolor color); */
NS_IMETHODIMP
nsSVGGDIPlusCanvas::Clear(nscolor color)
{
#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
  mOffscreenGraphics->Clear(Color(NS_GET_R(color),
                                  NS_GET_G(color),
                                  NS_GET_B(color)));
#else
  mGraphics->Clear(Color(NS_GET_R(color),
                         NS_GET_G(color),
                         NS_GET_B(color)));
#endif
  
  return NS_OK;
}

/** Implements void flush(); */
NS_IMETHODIMP
nsSVGGDIPlusCanvas::Flush()
{
#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
  mGraphics->SetCompositingMode(CompositingModeSourceCopy);
  mGraphics->DrawImage(mOffscreenBitmap, 0, 0,
                       mOffscreenBitmap->GetWidth(),
                       mOffscreenBitmap->GetHeight());
#endif

  mGraphics->Flush(FlushIntentionSync);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGGDIPlusCanvas methods:

NS_IMETHODIMP_(Graphics*)
nsSVGGDIPlusCanvas::GetGraphics()
{
#ifdef SVG_GDIPLUS_ENABLE_OFFSCREEN_BUFFER
  return mOffscreenGraphics;
#else
  return mGraphics;
#endif
}

/** Implements pushClip(); */
NS_IMETHODIMP
nsSVGGDIPlusCanvas::PushClip()
{
  Region *region = new Region;
  if (region)
    mGraphics->GetClip(region);
  // append even if we failed to allocate the region so push/pop match
  mClipStack.AppendElement((void *)region);

  return NS_OK;
}

/** Implements popClip(); */
NS_IMETHODIMP
nsSVGGDIPlusCanvas::PopClip()
{
  PRUint32 count = mClipStack.Count();
  if (count == 0)
    return NS_OK;

  Region *region = (Region *)mClipStack[count-1];
  if (region) {
    mGraphics->SetClip(region);
    delete region;
  }
  mClipStack.RemoveElementAt(count-1);

  return NS_OK;
}

/** Implements setClipRect(in nsIDOMSVGMatrix canvasTM, in float x, in float y,
    in float width, in float height); */
NS_IMETHODIMP
nsSVGGDIPlusCanvas::SetClipRect(nsIDOMSVGMatrix *aCTM, float aX, float aY,
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

  Matrix matrix(m[0], m[1], m[2], m[3], m[4], m[5]);
  RectF rect(aX, aY, aWidth, aHeight);
  Region clip(rect);
  clip.Transform(&matrix);
  mGraphics->SetClip(&clip, CombineModeIntersect);

  return NS_OK;
}
