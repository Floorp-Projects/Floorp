/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Bradley Baetz <bbaetz@cs.mcgill.ca>
 */
 
#include "nsDeviceContextXP.h"
#include "nsRenderingContextXlib.h"
#include "nsFontMetricsXlib.h"
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpecXPrint.h"
#include "nspr.h"
#include "nsXPrintContext.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *nsDeviceContextXpLM = PR_NewLogModule("nsDeviceContextXp");
#endif /* PR_LOGGING */

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
nsDeviceContextXp :: nsDeviceContextXp()
 : DeviceContextImpl()
{ 
  mPrintContext        = nsnull;
  mSpec                = nsnull; 
  mParentDeviceContext = nsnull;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
nsDeviceContextXp :: ~nsDeviceContextXp() 
{ 
  DestroyXPContext();
}


NS_IMETHODIMP
nsDeviceContextXp :: SetSpec(nsIDeviceContextSpec* aSpec)
{
  nsresult  rv = NS_ERROR_FAILURE;

  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::SetSpec()\n"));

  nsCOMPtr<nsIDeviceContextSpecXp> xpSpec;

  mSpec = aSpec;

  if(mPrintContext) DestroyXPContext(); // we cannot reuse that...
    
  mPrintContext = new nsXPrintContext();
  xpSpec = do_QueryInterface(mSpec);
  if(xpSpec) {
    rv = mPrintContext->Init(this, xpSpec);
  }
 
  return rv;
}

NS_IMPL_ISUPPORTS_INHERITED(nsDeviceContextXp,
                            DeviceContextImpl,
                            nsIDeviceContextXp)

/** ---------------------------------------------------
 *  See documentation in nsDeviceContextXp.h
 * 
 * This method is called after SetSpec
 */
NS_IMETHODIMP
nsDeviceContextXp::InitDeviceContextXP(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aParentContext)
{
  // Initialization moved to SetSpec to be done after creating the Print Context
  float origscale, newscale;
  float t2d, a2d;
  int   print_resolution;

  mPrintContext->GetPrintResolution(print_resolution);

  mPixelsToTwips = (float)NSIntPointsToTwips(72) / (float)print_resolution;
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  GetTwipsToDevUnits(newscale);
  aParentContext->GetTwipsToDevUnits(origscale);

  mCPixelScale = newscale / origscale;

  aParentContext->GetTwipsToDevUnits(t2d);
  aParentContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  NS_ASSERTION(aParentContext, "aCreatingDeviceContext cannot be NULL!!!");
  mParentDeviceContext = aParentContext;

  /* be sure we've cleaned-up old rubbish - new values will re-populate nsFontMetricsXlib soon... */
  nsFontMetricsXlib::FreeGlobals();
  nsFontMetricsXlib::EnablePrinterMode(PR_TRUE);
    
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsDeviceContextXp :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
   nsresult  rv = NS_ERROR_OUT_OF_MEMORY;

   nsCOMPtr<nsRenderingContextXp> xpContext;

   xpContext = new nsRenderingContextXp();
   if (xpContext) {
     rv = xpContext->Init(this);
   }

   if (NS_SUCCEEDED(rv)) {
     aContext = xpContext;
     NS_ADDREF(aContext);
   }
   return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  aSupportsWidgets = PR_FALSE;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp :: GetScrollBarDimensions(float &aWidth, 
                                        float &aHeight) const
{
  // XXX Oh, yeah.  These are hard coded.
  aWidth  = 15.f * mPixelsToTwips;
  aHeight = 15.f * mPixelsToTwips;

  return NS_OK;
}

void  nsDeviceContextXp :: SetDrawingSurface(nsDrawingSurface  aSurface) 
{ 
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  aSurface = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXp :: CheckFontExistence(const nsString& aFontName)
{
  return nsFontMetricsXlib::FamilyExists(this, aFontName);
}

NS_IMETHODIMP nsDeviceContextXp :: GetSystemAttribute(nsSystemAttrID anID, 
                                                      SystemAttrStruct * aInfo) const
{
  if (mParentDeviceContext != nsnull) {
    return mParentDeviceContext->GetSystemAttribute(anID, aInfo);
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::GetDeviceSurfaceDimensions(PRInt32 &aWidth, 
                                                        PRInt32 &aHeight)
{
  float width, height;
  width  = (float) mPrintContext->GetWidth();
  height = (float) mPrintContext->GetHeight();

  aWidth  = NSToIntRound(width  * mDevUnitsToAppUnits);
  aHeight = NSToIntRound(height * mDevUnitsToAppUnits);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXp::GetRect(nsRect &aRect)
{
  PRInt32 width, height;
  nsresult rv;
  rv = GetDeviceSurfaceDimensions(width, height);
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = width;
  aRect.height = height;
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::GetClientRect(nsRect &aRect)
{
  return GetRect(aRect);
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::GetDeviceContextFor(nsIDeviceContextSpec *aDevice, nsIDeviceContext *&aContext)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::BeginDocument(PRUnichar * aTitle)
{  
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::BeginDocument()\n"));
  nsresult  rv = NS_OK;
  if (mPrintContext != nsnull) {
    rv = mPrintContext->BeginDocument(aTitle);
  } 
  return rv;
}


void nsDeviceContextXp::DestroyXPContext()
{
  if (mPrintContext != nsnull) {   
    /* gisburn: mPrintContext cannot be reused between to print 
     * tasks as the destination print server may be a different one 
     * or the printer used on the same print server has other 
     * properties (build-in fonts for example ) than the printer 
     * previously used. */
    FlushFontCache();           
    nsRenderingContextXlib::Shutdown();
    nsFontMetricsXlib::FreeGlobals();
    
    mPrintContext = nsnull; // nsCOMPtr will call |delete mPrintContext;|
  } 
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::EndDocument(void)
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::EndDocument()\n"));
  nsresult rv = NS_OK;

  if (mPrintContext != nsnull) {
    rv = mPrintContext->EndDocument();
    DestroyXPContext();
  }
  
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::BeginPage(void)
{
  nsresult  rv = NS_OK;
  if (mPrintContext != nsnull) {
    rv = mPrintContext->BeginPage();
  }
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXp::EndPage(void)
{
  nsresult  rv = NS_OK;
  if (mPrintContext != nsnull) {
    rv = mPrintContext->EndPage();
  }
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXp :: ConvertPixel(nscolor aColor, 
                                                        PRUint32 & aPixel)
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::ConvertPixel()\n"));
  aPixel = xxlib_rgb_xpixel_from_rgb(GetXlibRgbHandle(),
                                     NS_RGB(NS_GET_B(aColor),
                                            NS_GET_G(aColor),
                                            NS_GET_R(aColor)));
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextXp::GetPrintContext(nsXPrintContext*& aContext)
{
  aContext = mPrintContext;
  return NS_OK;
}

class nsFontCacheXp : public nsFontCache
{
public:
  /* override DeviceContextImpl::CreateFontCache() */
  NS_IMETHODIMP CreateFontMetricsInstance(nsIFontMetrics** aResult);
};


NS_IMETHODIMP nsFontCacheXp::CreateFontMetricsInstance(nsIFontMetrics** aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  nsIFontMetrics *fm = new nsFontMetricsXlib();
  if (!fm)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(fm);
  *aResult = fm;
  return NS_OK;
}

/* override DeviceContextImpl::CreateFontCache() */
NS_IMETHODIMP nsDeviceContextXp::CreateFontCache()
{
  mFontCache = new nsFontCacheXp();
  if (nsnull == mFontCache) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mFontCache->Init(this);
  return NS_OK;
}


