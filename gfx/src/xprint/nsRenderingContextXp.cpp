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
#include "nsLocalFile.h"
#include <sys/mman.h>
#include <errno.h>

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

  NS_ENSURE_TRUE(nsnull != aContext, NS_ERROR_NULL_POINTER);
  mContext = aContext;
  if (mContext) {
     nsIDeviceContext *dc = mContext;     
     NS_STATIC_CAST(nsDeviceContextXp *,dc)->GetPrintContext(mPrintContext);
  }
  NS_ENSURE_TRUE(nsnull != mPrintContext, NS_ERROR_NULL_POINTER);

  mPrintContext->GetXlibRgbHandle(mXlibRgbHandle);
  mDisplay = xxlib_rgb_get_display(mXlibRgbHandle);

  /* A printer usually does not support things like multiple drawing surfaces
   * (nor "offscreen" drawing surfaces... would be quite difficult to 
   * implement (however - Xprint API supports offscreen surfaces but Mozilla
   * does not make use of them, see bug 124761 ("RFE: Make use of "offpaper"
   * drawing surfaces in some printing APIs")) =:-) ...
   * We just feed the nsXPContext object here directly - this is the only
   * "surface" the Mozilla printer API can "draw" on ...
   * |mOffscreenSurface| is just set for bug 251136 ("nsRenderingContextGTK
   * uses mSurface after it's been freed")
   */
  mOffscreenSurface = mSurface = mPrintContext; 
  UpdateGC(); /* Fill |mGC| */
  mPrintContext->SetGC(mGC);

  return CommonInit();
}

NS_IMETHODIMP nsRenderingContextXp::Init(nsIDeviceContext* aContext, nsIWidget *aWidget) 
{ 
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::Init(nsIDeviceContext* aContext, nsIWidget *aWidget)\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsRenderingContextXp::Init(nsIDeviceContext* aContext, nsIDrawingSurface* aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::Init(nsIDeviceContext* aContext, nsIDrawingSurface* aSurface)\n"));
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
  PopState();
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::SelectOffScreenDrawingSurface(nsIDrawingSurface* aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SelectOffScreenDrawingSurface()\n"));
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetDrawingSurface(nsIDrawingSurface* *aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetDrawingSurface()\n"));
  *aSurface = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::CreateDrawingSurface(const nsRect& aBounds, PRUint32 aSurfFlags, nsIDrawingSurface* &aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::CreateDrawingSurface()\n"));
  aSurface = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextXp::DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage()\n"));

  nsRect dr = aDestRect;
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

  nsRect sr = aSrcRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
  sr.x = aSrcRect.x;
  sr.y = aSrcRect.y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  UpdateGC();
  return mPrintContext->DrawImage(mGC, img,
                                  sr.x, sr.y,
                                  sr.width, sr.height,
                                  dr.x, dr.y,
                                  dr.width, dr.height);
}

NS_IMETHODIMP
nsRenderingContextXp::CopyOffScreenBits(nsIDrawingSurface* aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                                        const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::CopyOffScreenBits()\n"));

  NS_NOTREACHED("nsRenderingContextXp::CopyOffScreenBits() not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRenderingContextXp::RenderEPS(const nsRect& aRect, FILE *aDataFile)
{
  nsresult             rv;
  int                  fd;
  const unsigned char *data;
  size_t               datalen;

  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::RenderPostScriptDataFragment()\n"));

  /* Get file size */
  fseek(aDataFile, 0, SEEK_END);
  datalen = ftell(aDataFile);

  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("file size=%ld\n", (long)datalen));  

  /* Safeguard against bogus values
   * (make sure we clamp the size to a reasonable value (say... 128 MB)) */
  if (datalen <= 0 || datalen > (128 * 1024 * 1024)) {
    PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("error: file size %ld too large\n", (long)datalen));
    return NS_ERROR_FAILURE;
  }
  
  fflush(aDataFile);
  fd = fileno(aDataFile);
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("fileno=%d\n", fd));  
  data = (const unsigned char *)mmap(0, datalen, PROT_READ, MAP_SHARED, fd, 0); 
  if (MAP_FAILED == data) {
    int saved_errno = errno;
    PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("mmap() failure, errno=%s/%d\n",
           strerror(saved_errno), saved_errno));  
    return nsresultForErrno(saved_errno);
  }

  PushState();

  nsRect trect = aRect;
  mTranMatrix->TransformCoord(&trect.x, &trect.y, &trect.width, &trect.height);
  UpdateGC();
  rv = mPrintContext->RenderEPS(trect, data, datalen);

  PopState();
  
  munmap((char *)data, datalen);

  return rv;
}



