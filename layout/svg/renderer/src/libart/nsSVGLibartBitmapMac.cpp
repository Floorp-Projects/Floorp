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
#include "nsISVGLibartBitmap.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsIImage.h"
#include "nsIComponentManager.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * A libart bitmap implementation based on gfxIImageFrame that should
 * work for Mac but doesn't support obtaining RenderingContexts with
 * Lock/UnlockRenderingContext
 */
class nsSVGLibartBitmapMac : public nsISVGLibartBitmap
{
public:
  nsSVGLibartBitmapMac();
  ~nsSVGLibartBitmapMac();
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
  void LockBuffer();
  void UnlockBuffer();

  PRBool mLocked;
  nsCOMPtr<nsIRenderingContext> mRenderingContext;
  nsCOMPtr<imgIContainer> mContainer;
  nsCOMPtr<gfxIImageFrame> mBuffer;
  nsRect mRectTwips;
  nsRect mRect;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartBitmapMac::nsSVGLibartBitmapMac()
    : mLocked(PR_FALSE)
{
}

nsSVGLibartBitmapMac::~nsSVGLibartBitmapMac()
{

}

nsresult
nsSVGLibartBitmapMac::Init(nsIRenderingContext* ctx,
                               nsPresContext* presContext,
                               const nsRect & rect)
{
  mRenderingContext = ctx;

  float twipsPerPx;
  twipsPerPx = presContext->PixelsToTwips();
  mRectTwips.x = (nscoord)(rect.x*twipsPerPx);
  mRectTwips.y = (nscoord)(rect.y*twipsPerPx);
  mRectTwips.width = (nscoord)(rect.width*twipsPerPx);
  mRectTwips.height = (nscoord)(rect.height*twipsPerPx);
  mRect = rect;
  
  mContainer = do_CreateInstance("@mozilla.org/image/container;1");
  mContainer->Init(rect.width, rect.height, nsnull);
    
  mBuffer = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
  // even though we request a 24bpp buffer, on the mac we get a 32bpp one...
  mBuffer->Init(0, 0, rect.width, rect.height, gfxIFormats::RGB, 24);
  mContainer->AppendFrame(mBuffer);
  
  return NS_OK;
}

nsresult
NS_NewSVGLibartBitmap(nsISVGLibartBitmap **result,
                      nsIRenderingContext *ctx,
                      nsPresContext* presContext,
                      const nsRect & rect)
{
  nsSVGLibartBitmapMac* bm = new nsSVGLibartBitmapMac();
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

NS_IMPL_ADDREF(nsSVGLibartBitmapMac)
NS_IMPL_RELEASE(nsSVGLibartBitmapMac)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartBitmapMac)
  NS_INTERFACE_MAP_ENTRY(nsISVGLibartBitmap)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// Implementation helpers:
void
nsSVGLibartBitmapMac::LockBuffer()
{
  if (mLocked) return;

  mBuffer->LockImageData();    
  mLocked = PR_TRUE;
}

void
nsSVGLibartBitmapMac::UnlockBuffer()
{
  if (!mLocked) return;

  mBuffer->UnlockImageData();
  mLocked = PR_FALSE;
}


//----------------------------------------------------------------------
// nsISVGLibartBitmap methods:

NS_IMETHODIMP_(PRUint8 *)
nsSVGLibartBitmapMac::GetBits()
{
  LockBuffer();
  PRUint8* bits=nsnull;
  PRUint32 length;
  mBuffer->GetImageData(&bits, &length);
  return bits;
}

NS_IMETHODIMP_(nsISVGLibartBitmap::PixelFormat)
nsSVGLibartBitmapMac::GetPixelFormat()
{
  return PIXEL_FORMAT_32_ABGR;
}

NS_IMETHODIMP_(int)
nsSVGLibartBitmapMac::GetLineStride()
{
  PRUint32 bytesPerRow=0;
  mBuffer->GetImageBytesPerRow(&bytesPerRow);
  return (int) bytesPerRow;
}

NS_IMETHODIMP_(int)
nsSVGLibartBitmapMac::GetWidth()
{
  return mRect.width; 
}

NS_IMETHODIMP_(int)
nsSVGLibartBitmapMac::GetHeight()
{
  return mRect.height;
}

NS_IMETHODIMP_(void)
nsSVGLibartBitmapMac::LockRenderingContext(const nsRect& rect, nsIRenderingContext** ctx)
{
  // doesn't work on default bitmap!
  *ctx = nsnull;
}

NS_IMETHODIMP_(void)
nsSVGLibartBitmapMac::UnlockRenderingContext()
{
  // doesn't work on default bitmap!
}

NS_IMETHODIMP_(void)
nsSVGLibartBitmapMac::Flush()
{
  UnlockBuffer();

  nsCOMPtr<nsIDeviceContext> ctx;
  mRenderingContext->GetDeviceContext(*getter_AddRefs(ctx));

  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(mBuffer));
  if (ireq) {
    nsCOMPtr<nsIImage> img(do_GetInterface(ireq));

    if (!img->GetIsRowOrderTopToBottom()) {
      // XXX we need to flip the image. This is silly. Blt should take
      // care of it
      int stride = img->GetLineStride();
      int height = GetHeight();
      PRUint8* bits = img->GetBits();
      PRUint8* rowbuf = new PRUint8[stride];
      for (int row=0; row<height/2; ++row) {
        memcpy(rowbuf, bits+row*stride, stride);
        memcpy(bits+row*stride, bits+(height-1-row)*stride, stride);
        memcpy(bits+(height-1-row)*stride, rowbuf, stride);
      }
      delete[] rowbuf;
    }
    
    nsRect r(0, 0, GetWidth(), GetHeight());
    img->ImageUpdated(ctx, nsImageUpdateFlags_kBitsChanged, &r);
  }
  
  mContainer->DecodingComplete();
  mRenderingContext->DrawTile(mContainer, mRectTwips.x, mRectTwips.y, &mRectTwips);
}
