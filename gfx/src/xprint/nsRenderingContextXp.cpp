/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#include "nsRenderingContextXp.h"
#include "nsFontMetricsXlib.h"
#include "nsDeviceContextXP.h"
#include "nsCompressedCharMap.h"
#include "xlibrgb.h"
#include "prprf.h"
#include "prmem.h"
#include "prlog.h"
#include "nsGCCache.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#ifdef PR_LOGGING 
static PRLogModuleInfo *RenderingContextXpLM = PR_NewLogModule("RenderingContextXp");
#endif /* PR_LOGGING */ 

nsRenderingContextXp::nsRenderingContextXp()
  : nsRenderingContextXlib(),
  mPrintContext(nsnull)
{
}

nsRenderingContextXp::~nsRenderingContextXp()
{
}

NS_IMETHODIMP
nsRenderingContextXp::Init(nsIDeviceContext* aContext)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::Init(nsIDeviceContext *)\n"));

  mContext = do_QueryInterface(aContext);
  NS_ASSERTION(nsnull != mContext, "Device context is null.");
  if (mContext) {
     nsIDeviceContext *dc = mContext;     
     NS_STATIC_CAST(nsDeviceContextXp *,dc)->GetPrintContext(mPrintContext);
  }
  NS_ASSERTION(nsnull != mPrintContext, "mPrintContext is null.");

  mPrintContext->GetXlibRgbHandle(mXlibRgbHandle);
  mDisplay = xxlib_rgb_get_display(mXlibRgbHandle);
  mScreen  = xxlib_rgb_get_screen(mXlibRgbHandle);
  mVisual  = xxlib_rgb_get_visual(mXlibRgbHandle);
  mDepth   = xxlib_rgb_get_depth(mXlibRgbHandle);

  /* A printer usually does not support things like multiple drawing surfaces
   * (nor "offscreen" drawing surfaces... would be quite difficult to 
   * implement =:-) ...
   * We just feed the nsXPContext object here directly - this is the only
   * "surface" the printer can "draw" on ...  
   */
  Drawable drawable; mPrintContext->GetDrawable(drawable);
  UpdateGC(drawable); // fill mGC
  mPrintContext->SetGC(mGC);
  mRenderingSurface = mPrintContext;

  return CommonInit();
}

NS_IMETHODIMP nsRenderingContextXp::Init(nsIDeviceContext* aContext, nsIWidget *aWidget) 
{ 
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsRenderingContextXp::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRenderingContextXp::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                         PRUint32 aWidth, PRUint32 aHeight,
                                         void **aBits,
                                         PRInt32 *aStride,
                                         PRInt32 *aWidthBytes,
                                         PRUint32 aFlags)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::LockDrawingSurface()\n"));
  PushState();
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::UnlockDrawingSurface(void)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::UnlockDrawingSurface()\n"));
  PRBool clipstate;
  PopState(clipstate);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SelectOffScreenDrawingSurface()\n"));
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetDrawingSurface(nsDrawingSurface *aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetDrawingSurface()\n"));
  *aSurface = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::CreateDrawingSurface()\n"));
  aSurface = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage()\n"));

  nsRect tr;
  tr = aRect;
  mTranMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);
  UpdateGC();
  return mPrintContext->DrawImage(mGC, aImage, tr.x, tr.y, tr.width, tr.height);
}

NS_IMETHODIMP
nsRenderingContextXp::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage()\n"));
  nsRect sr,dr;
  
  sr = aSRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y,
                           &sr.width, &sr.height);
  sr.x = aSRect.x;
  sr.y = aSRect.y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);
  
  dr = aDRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y,
                           &dr.width, &dr.height);
  UpdateGC();
  return mPrintContext->DrawImage(mGC, aImage,
                                  sr.x, sr.y,
                                  sr.width, sr.height,
                                  dr.x, dr.y,
                                  dr.width, dr.height);
}

/* [noscript] void drawImage (in imgIContainer aImage, [const] in nsRect aSrcRect, [const] in nsPoint aDestPoint); */
NS_IMETHODIMP nsRenderingContextXp::DrawImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage()\n"));
  nsPoint pt;
  nsRect  sr;

  pt = *aDestPoint;
  mTranMatrix->TransformCoord(&pt.x, &pt.y);

  sr = *aSrcRect;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) 
    return NS_ERROR_FAILURE;

  UpdateGC();
  // doesn't it seem like we should use more of the params here?
  // img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height,
  //           pt.x + sr.x, pt.y + sr.y, sr.width, sr.height);
  return mPrintContext->DrawImage(mGC, img, pt.x, pt.y, sr.width, sr.height);
}

/* [noscript] void drawScaledImage (in imgIContainer aImage, [const] in nsRect aSrcRect, [const] in nsRect aDestRect); */
NS_IMETHODIMP nsRenderingContextXp::DrawScaledImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawScaledImage()\n"));
  nsRect dr;

  dr = *aDestRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) 
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) 
    return NS_ERROR_FAILURE;

  // doesn't it seem like we should use more of the params here?
  // img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);

  nsRect sr;

  sr = *aSrcRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
  sr.x = aSrcRect->x;
  sr.y = aSrcRect->y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  UpdateGC();
  return mPrintContext->DrawImage(mGC, img,
                                  sr.x, sr.y,
                                  sr.width, sr.height,
                                  dr.x, dr.y,
                                  dr.width, dr.height);
}

NS_IMETHODIMP
nsRenderingContextXp::CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                                        const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::CopyOffScreenBits()\n"));

  NS_NOTREACHED("nsRenderingContextXp::CopyOffScreenBits() not yet implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
