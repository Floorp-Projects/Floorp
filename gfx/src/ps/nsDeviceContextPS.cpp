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
 */

#include "nsDeviceContextPS.h"
#include "nsRenderingContextPS.h"
#include "nsString.h"
#include "nsFontMetricsPS.h"
#include "nsPostScriptObj.h"

static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);
static NS_DEFINE_IID(kIDeviceContextSpecPSIID, NS_IDEVICE_CONTEXT_SPEC_PS_IID);

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS :: nsDeviceContextPS()
{
  NS_INIT_REFCNT();
  mSpec = nsnull; 
  mParentDeviceContext = nsnull;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS :: ~nsDeviceContextPS()
{
  NS_IF_RELEASE(mSpec);
  NS_IF_RELEASE(mParentDeviceContext);
}

NS_IMETHODIMP
nsDeviceContextPS :: SetSpec(nsIDeviceContextSpec* aSpec)
{
  mSpec = aSpec;
  NS_ADDREF(aSpec);
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsDeviceContextPS,
                            DeviceContextImpl,
                            nsIDeviceContextPS)

/** ---------------------------------------------------
 *  See documentation in nsDeviceContextPS.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsDeviceContextPS::InitDeviceContextPS(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aParentContext)
{
float origscale, newscale;
float t2d, a2d;

  mDepth = 1;     // just for arguments sake

  mTwipsToPixels = (float)72.0/(float)NSIntPointsToTwips(72);
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  GetTwipsToDevUnits(newscale);
  aParentContext->GetTwipsToDevUnits(origscale);
  mCPixelScale = newscale / origscale;

  aParentContext->GetTwipsToDevUnits(t2d);
  aParentContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  mParentDeviceContext = aParentContext;
  NS_ASSERTION(mParentDeviceContext, "aCreatingDeviceContext cannot be NULL!!!");
  NS_ADDREF(mParentDeviceContext);
  return  NS_OK;
}

/** ---------------------------------------------------
 *  Create a Postscript RenderingContext.  This will create a RenderingContext
 *  from the deligate RenderingContext and intall that into our Postscript RenderingContext.
 *	@update 12/21/98 dwc
 *  @param aContext -- our newly created Postscript RenderingContextPS
 *  @return -- NS_OK if everything succeeded.
 */
NS_IMETHODIMP nsDeviceContextPS :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
nsresult  rv = NS_ERROR_OUT_OF_MEMORY;

	aContext = new nsRenderingContextPS();
  if (nsnull != aContext){
    NS_ADDREF(aContext);
    rv = ((nsRenderingContextPS*) aContext)->Init(this);
  }else{
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_FAILED(rv)) {
    NS_IF_RELEASE(aContext);
  }

  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  aSupportsWidgets = PR_FALSE;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
   //XXX: Hardcoded values for Postscript
  aWidth = 20;
  aHeight = 20;
  return NS_OK;

}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDepth(PRUint32& aDepth)
{
  return(1);    // postscript is 1 bit
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: CheckFontExistence(const nsString& aFontName)
{

  // XXX this needs to find out if this font is supported for postscript
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPS :: GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  if (mParentDeviceContext != nsnull) {
    return mParentDeviceContext->GetSystemAttribute(anID, aInfo);
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  nsIDeviceContextSpecPS *psSpec;
  nsresult rv = NS_ERROR_FAILURE;
  float width, height;
  float top,left,right,bottom;

  if ( nsnull != mSpec ) {
    rv = mSpec->QueryInterface(kIDeviceContextSpecPSIID, (void **) &psSpec);
    if (NS_SUCCEEDED(rv)) {
      psSpec->GetPageDimensions( width, height );
      aWidth = NSToIntRound((72.0f*width) * mDevUnitsToAppUnits); 
      aHeight = NSToIntRound((72.0f*height) * mDevUnitsToAppUnits); 
    }
  }
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextPS::GetRect(nsRect &aRect)
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
NS_IMETHODIMP nsDeviceContextPS::GetClientRect(nsRect &aRect)
{
  return GetRect(aRect);
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,nsIDeviceContext *&aContext)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::BeginDocument(PRUnichar * aTitle)
{  
  nsIDeviceContextSpecPS *psSpec;
  nsresult res = NS_OK;

  if ( nsnull != mSpec ) {
    mPSObj = new nsPostScriptObj();  
    res = mSpec->QueryInterface(kIDeviceContextSpecPSIID, (void **) &psSpec);
    if (NS_SUCCEEDED(res)) {
      res = mPSObj->Init(psSpec,aTitle);
    }
  }
  return res;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::EndDocument(void)
{
  delete mPSObj;
  mPSObj = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::BeginPage(void)
{
  // begin the page
  mPSObj->begin_page(); 
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::EndPage(void)
{
  // end the page
  mPSObj->end_page();
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  aPixel = aColor;
  return NS_OK;
}

class nsFontCachePS : public nsFontCache
{
public:
  /* override DeviceContextImpl::CreateFontCache() */
  NS_IMETHODIMP CreateFontMetricsInstance(nsIFontMetrics** aResult);
};


NS_IMETHODIMP nsFontCachePS::CreateFontMetricsInstance(nsIFontMetrics** aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  nsIFontMetrics *fm = new nsFontMetricsPS();
  if (!fm)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(fm);
  *aResult = fm;
  return NS_OK;
}

/* override DeviceContextImpl::CreateFontCache() */
NS_IMETHODIMP nsDeviceContextPS::CreateFontCache()
{
  mFontCache = new nsFontCachePS();
  if (nsnull == mFontCache) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mFontCache->Init(this);
  return NS_OK;
}


