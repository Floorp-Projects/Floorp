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
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#include"gdk-pixbuf/gdk-pixbuf.h"
#include "nsCOMPtr.h"
#include "nsISVGLibartBitmap.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsIComponentManager.h"
#include "nsDrawingSurfaceGTK.h"
#include "nsRenderingContextGTK.h"

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 *  A libart-bitmap implementation for gtk 2.0.
 */
class nsSVGLibartBitmapGdk : public nsISVGLibartBitmap
{
public:
  nsSVGLibartBitmapGdk();
  ~nsSVGLibartBitmapGdk();
  nsresult Init(nsIRenderingContext *ctx,
                nsPresContext* presContext,
                const nsRect & rect);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGLibartBitmap interface:
  NS_IMETHOD_(PRUint8 *) GetBits();
  NS_IMETHOD_(nsISVGLibartBitmap::PixelFormat) GetPixelFormat();
  NS_IMETHOD_(int) GetLineStride();
  NS_IMETHOD_(int) GetWidth();
  NS_IMETHOD_(int) GetHeight();
  NS_IMETHOD_(void) LockRenderingContext(const nsRect& rect, nsIRenderingContext**ctx);
  NS_IMETHOD_(void) UnlockRenderingContext();
  NS_IMETHOD_(void) Flush();
  
private:
  nsCOMPtr<nsIRenderingContext> mRenderingContext;
  nsRect mBufferRect;
  GdkPixbuf *mBuffer;
  nsRect mLockRect;
  nsIDrawingSurface* mTempSurface;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartBitmapGdk::nsSVGLibartBitmapGdk()
    : mBuffer(nsnull),
      mTempSurface(nsnull)
{
}

nsSVGLibartBitmapGdk::~nsSVGLibartBitmapGdk()
{
  if (mBuffer) {
    gdk_pixbuf_unref(mBuffer);
  }
}

nsresult
nsSVGLibartBitmapGdk::Init(nsIRenderingContext* ctx,
                           nsPresContext* presContext,
                           const nsRect & rect)
{
  mRenderingContext = ctx;
  mBufferRect = rect;
  
  mBuffer = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, rect.width, rect.height);

  return NS_OK;
}

nsresult
NS_NewSVGLibartBitmap(nsISVGLibartBitmap **result,
                      nsIRenderingContext *ctx,
                      nsPresContext* presContext,
                      const nsRect & rect)
{
  nsSVGLibartBitmapGdk* bm = new nsSVGLibartBitmapGdk();
  if (!bm) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(bm);

  nsresult rv = bm->Init(ctx, presContext, rect);

  if (NS_FAILED(rv)) {
    NS_RELEASE(bm);
    return rv;
  }
  
  *result = bm;
  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLibartBitmapGdk)
NS_IMPL_RELEASE(nsSVGLibartBitmapGdk)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartBitmapGdk)
  NS_INTERFACE_MAP_ENTRY(nsISVGLibartBitmap)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// Implementation helpers:

//----------------------------------------------------------------------
// nsISVGLibartBitmap methods:

NS_IMETHODIMP_(PRUint8 *)
nsSVGLibartBitmapGdk::GetBits()
{
  PRUint8* bits=gdk_pixbuf_get_pixels(mBuffer);
  return bits;
}

NS_IMETHODIMP_(nsISVGLibartBitmap::PixelFormat)
nsSVGLibartBitmapGdk::GetPixelFormat()
{
  return PIXEL_FORMAT_24_RGB;
}

NS_IMETHODIMP_(int)
nsSVGLibartBitmapGdk::GetLineStride()
{
  return gdk_pixbuf_get_rowstride(mBuffer);
}

NS_IMETHODIMP_(int)
nsSVGLibartBitmapGdk::GetWidth()
{
  return mBufferRect.width; 
}

NS_IMETHODIMP_(int)
nsSVGLibartBitmapGdk::GetHeight()
{
  return mBufferRect.height;
}

NS_IMETHODIMP_(void)
nsSVGLibartBitmapGdk::LockRenderingContext(const nsRect& rect, nsIRenderingContext**ctx)
{ 
  NS_ASSERTION(mTempSurface==nsnull, "nested locking?");
  mLockRect = rect;
  
  nsDrawingSurfaceGTK *surface=nsnull;
  mRenderingContext->CreateDrawingSurface(rect, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS,
                                          (nsIDrawingSurface*&)surface);
  NS_ASSERTION(surface, "could not create drawing surface");

  // copy from pixbuf to surface
  GdkDrawable *drawable = surface->GetDrawable();
  NS_ASSERTION(drawable, "null gdkdrawable");

  gdk_draw_pixbuf(drawable, 0, mBuffer,
                  mLockRect.x-mBufferRect.x,
                  mLockRect.y-mBufferRect.y,
                  0, 0,
                  mLockRect.width, mLockRect.height,
                  GDK_RGB_DITHER_NONE, 0, 0);

  mRenderingContext->GetDrawingSurface(&mTempSurface);
  mRenderingContext->SelectOffScreenDrawingSurface(surface);
  mRenderingContext->PushState();

  nsTransform2D* transform;
  mRenderingContext->GetCurrentTransform(transform);
  transform->SetTranslation(-mLockRect.x, -mLockRect.y);
  
  *ctx = mRenderingContext;
  NS_ADDREF(*ctx);
}

NS_IMETHODIMP_(void)
nsSVGLibartBitmapGdk::UnlockRenderingContext()
{ 
  NS_ASSERTION(mTempSurface, "no temporary surface. multiple unlock calls?");
  nsDrawingSurfaceGTK *surface=nsnull;
  mRenderingContext->GetDrawingSurface((nsIDrawingSurface**)&surface);
  NS_ASSERTION(surface, "null surface");

  mRenderingContext->PopState();
  mRenderingContext->SelectOffScreenDrawingSurface(mTempSurface);
  mTempSurface = nsnull;
  
  // copy from surface to pixbuf
  GdkPixbuf * pb = gdk_pixbuf_get_from_drawable(mBuffer, surface->GetDrawable(),
                                                0,
                                                0,0,
                                                mLockRect.x-mBufferRect.x,mLockRect.y-mBufferRect.y,
                                                mLockRect.width,mLockRect.height);
  NS_ASSERTION(pb, "gdk_pixbuf_get_from_drawable failed");
  mRenderingContext->DestroyDrawingSurface(surface);
}

NS_IMETHODIMP_(void)
nsSVGLibartBitmapGdk::Flush()
{

  nsDrawingSurfaceGTK *surface=nsnull;
  mRenderingContext->GetDrawingSurface((nsIDrawingSurface**)&surface);
  NS_ASSERTION(surface, "null drawing surface");

  GdkDrawable *drawable = surface->GetDrawable();
  NS_ASSERTION(drawable, "null gdkdrawable");

  nsTransform2D* transform;
  mRenderingContext->GetCurrentTransform(transform);

  gdk_draw_pixbuf(drawable, 0, mBuffer,
                  0, 0,
                  mBufferRect.x + transform->GetXTranslation(),
                  mBufferRect.y + transform->GetYTranslation(),
                  mBufferRect.width, mBufferRect.height,
                  GDK_RGB_DITHER_NONE, 0, 0);
}
