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
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
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
#include "nsSVGLibartCanvas.h"
#include "nsISVGLibartCanvas.h"
#include "nsISVGLibartBitmap.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "libart-incs.h"

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Libart canvas implementation
 */
class nsSVGLibartCanvas : public nsISVGLibartCanvas
{
public:
  nsSVGLibartCanvas();
  ~nsSVGLibartCanvas();
  nsresult Init(nsIRenderingContext* ctx, nsPresContext* presContext,
                const nsRect & dirtyRect);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererCanvas interface:
  NS_DECL_NSISVGRENDERERCANVAS

  // nsISVGLibartCanvas interface:
  NS_IMETHOD_(ArtRender*) NewRender();
  NS_IMETHOD_(ArtRender*) NewRender(int x0, int y0, int x1, int y1);
  NS_IMETHOD_(void) InvokeRender(ArtRender* render);  
  NS_IMETHOD_(void) GetArtColor(nscolor rgb, ArtColor& artColor);
  
private:
  nsCOMPtr<nsIRenderingContext> mRenderingContext;
  nsCOMPtr<nsPresContext> mPresContext;
  nsCOMPtr<nsISVGLibartBitmap> mBitmap;
  nsRect mDirtyRect;

};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartCanvas::nsSVGLibartCanvas()
{
}

nsSVGLibartCanvas::~nsSVGLibartCanvas()
{
}

nsresult
nsSVGLibartCanvas::Init(nsIRenderingContext* ctx,
                        nsPresContext* presContext,
                        const nsRect & dirtyRect)
{
  mPresContext = presContext;
  NS_ASSERTION(mPresContext, "empty prescontext");
  mRenderingContext = ctx;
  NS_ASSERTION(mRenderingContext, "empty rendering context");

  mDirtyRect = dirtyRect;
  NS_NewSVGLibartBitmap(getter_AddRefs(mBitmap), ctx, presContext,
                        dirtyRect);
  if (!mBitmap) {
    NS_ERROR("could not construct bitmap");
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

nsresult
NS_NewSVGLibartCanvas(nsISVGRendererCanvas **result,
                      nsIRenderingContext *ctx,
                      nsPresContext *presContext,
                      const nsRect & dirtyRect)
{
  nsSVGLibartCanvas* pg = new nsSVGLibartCanvas();
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

NS_IMPL_ADDREF(nsSVGLibartCanvas)
NS_IMPL_RELEASE(nsSVGLibartCanvas)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISVGLibartCanvas)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
//  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGRendererCanvas)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererCanvas methods:

/** Implements [noscript] nsIRenderingContext lockRenderingContext(const in nsRectRef rect); */
NS_IMETHODIMP
nsSVGLibartCanvas::LockRenderingContext(const nsRect & rect,
                                        nsIRenderingContext **_retval)
{
  *_retval = nsnull;
  mBitmap->LockRenderingContext(rect, _retval);
  if (!*_retval) return NS_ERROR_FAILURE;
  return NS_OK;
}

/** Implements void unlockRenderingContext(); */
NS_IMETHODIMP 
nsSVGLibartCanvas::UnlockRenderingContext()
{
  mBitmap->UnlockRenderingContext();
  return NS_OK;
}

/** Implements nsPresContext getPresContext(); */
NS_IMETHODIMP
nsSVGLibartCanvas::GetPresContext(nsPresContext **_retval)
{
  *_retval = mPresContext;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/** void clear(in nscolor color); */
NS_IMETHODIMP
nsSVGLibartCanvas::Clear(nscolor color)
{
  PRUint8 red   = NS_GET_R(color);
  PRUint8 green = NS_GET_G(color);
  PRUint8 blue  = NS_GET_B(color);

  switch (mBitmap->GetPixelFormat()) {
    case nsISVGLibartBitmap::PIXEL_FORMAT_24_RGB:
    {
      PRInt32 stride = mBitmap->GetLineStride();
      PRInt32 width  = mBitmap->GetWidth();
      PRUint8* buf = mBitmap->GetBits();
      PRUint8* end = buf + stride*mBitmap->GetHeight();
      for ( ; buf<end; buf += stride) {
        art_rgb_fill_run(buf, red, green, blue, width);
      }
      break;
    }
    case nsISVGLibartBitmap::PIXEL_FORMAT_24_BGR:
    {
      PRInt32 stride = mBitmap->GetLineStride();
      PRInt32 width  = mBitmap->GetWidth();
      PRUint8* buf = mBitmap->GetBits();
      PRUint8* end = buf + stride*mBitmap->GetHeight();
      for ( ; buf<end; buf += stride) {
        art_rgb_fill_run(buf, blue, green, red, width);
      }
      break;
    }
    case nsISVGLibartBitmap::PIXEL_FORMAT_32_ABGR:
    {
      NS_ASSERTION(mBitmap->GetLineStride() == 4*mBitmap->GetWidth(), "strange pixel format");
      PRUint32 pixel = (blue<<24)+(green<<16)+(red<<8)+0xff;
      PRUint32 *dest = (PRUint32*)mBitmap->GetBits();
      PRUint32 *end  = dest+mBitmap->GetWidth()*mBitmap->GetHeight();
      while (dest!=end)
        *dest++ = pixel;
      break;
    }
    default:
      NS_ERROR("Unknown pixel format");
      break;
  }
  
  return NS_OK;
}

/** void flush(); */
NS_IMETHODIMP
nsSVGLibartCanvas::Flush()
{
  mBitmap->Flush();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGLibartCanvas methods:
NS_IMETHODIMP_(ArtRender*)
nsSVGLibartCanvas::NewRender()
{
  return art_render_new(mDirtyRect.x, mDirtyRect.y, // x0,y0
                        mDirtyRect.x+mDirtyRect.width, // x1
                        mDirtyRect.y+mDirtyRect.height, // y1
                        mBitmap->GetBits(), // pixels
                        mBitmap->GetLineStride(), // rowstride
                        3, // n_chan
                        8, // depth
                        mBitmap->GetPixelFormat()==nsISVGLibartBitmap::PIXEL_FORMAT_32_ABGR ? ART_ALPHA_SEPARATE : ART_ALPHA_NONE, // alpha_type
                        NULL //alphagamma
                        );
}

NS_IMETHODIMP_(ArtRender*)
nsSVGLibartCanvas::NewRender(int x0, int y0, int x1, int y1)
{
  NS_ASSERTION(x0<x1 && y0<y1, "empty rect passed to NewRender()");
  if (x0>=x1 || y0>=y1) return nsnull;

  // only construct a render object if there is overlap with the dirty rect:
  if (x1<mDirtyRect.x || x0>mDirtyRect.x+mDirtyRect.width ||
      y1<mDirtyRect.y || y0>mDirtyRect.y+mDirtyRect.height)
    return nsnull;

  int rx0 = (x0>mDirtyRect.x ? x0 : mDirtyRect.x);
  int rx1 = (x1<mDirtyRect.x+mDirtyRect.width ? x1 : mDirtyRect.x+mDirtyRect.width);
  int ry0 = (y0>mDirtyRect.y ? y0 : mDirtyRect.y);
  int ry1 = (y1<mDirtyRect.y+mDirtyRect.height ? y1 : mDirtyRect.y+mDirtyRect.height);

  int offset = 3*(rx0-mDirtyRect.x) + mBitmap->GetLineStride()*(ry0-mDirtyRect.y);
  
  return art_render_new(rx0, ry0, rx1, ry1,
                        mBitmap->GetBits()+offset, // pixels
                        mBitmap->GetLineStride(),  // rowstride
                        3, // n_chan
                        8, // depth
                        mBitmap->GetPixelFormat()==nsISVGLibartBitmap::PIXEL_FORMAT_32_ABGR ? ART_ALPHA_SEPARATE : ART_ALPHA_NONE, // alpha_type
                        NULL //alphagamma
                        );
}

NS_IMETHODIMP_(void)
nsSVGLibartCanvas::InvokeRender(ArtRender* render)
{
  art_render_invoke(render);
}

NS_IMETHODIMP_(void)
nsSVGLibartCanvas::GetArtColor(nscolor rgb, ArtColor& artColor)
{
  switch (mBitmap->GetPixelFormat()) {
    case nsISVGLibartBitmap::PIXEL_FORMAT_24_RGB:
      artColor[0] = ART_PIX_MAX_FROM_8(NS_GET_R(rgb));
      artColor[1] = ART_PIX_MAX_FROM_8(NS_GET_G(rgb));
      artColor[2] = ART_PIX_MAX_FROM_8(NS_GET_B(rgb));
      break;
    case nsISVGLibartBitmap::PIXEL_FORMAT_24_BGR:
    case nsISVGLibartBitmap::PIXEL_FORMAT_32_ABGR:
      artColor[0] = ART_PIX_MAX_FROM_8(NS_GET_B(rgb));
      artColor[1] = ART_PIX_MAX_FROM_8(NS_GET_G(rgb));
      artColor[2] = ART_PIX_MAX_FROM_8(NS_GET_R(rgb));
      break;
    default:
      NS_ERROR("unknown pixel format");
      break;
  }
}

