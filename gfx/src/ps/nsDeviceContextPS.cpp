/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDeviceContextPS.h"
#include "nsRenderingContextPS.h"
#include "nsString.h"
#include "nsFontMetricsPS.h"
#include "il_util.h"
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
  
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS :: ~nsDeviceContextPS()
{
PRInt32 i, n;

  // get rid of the fonts in our mFontMetrics cache
  n= mFontMetrics.Count();
  for (i = 0; i < n; i++){
    nsIFontMetrics* fm = (nsIFontMetrics*) mFontMetrics.ElementAt(i);
    fm->Destroy();
    NS_RELEASE(fm);
  }
  mFontMetrics.Clear();
  NS_IF_RELEASE(mSpec);

}

void nsDeviceContextPS :: SetSpec(nsIDeviceContextSpec* aSpec)
{
  mSpec = aSpec;
  NS_ADDREF(aSpec);
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextPS, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextPS)
NS_IMPL_RELEASE(nsDeviceContextPS)

/** ---------------------------------------------------
 *  See documentation in nsDeviceContextPS.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: Init(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aPrinterContext)
{
float origscale, newscale;
float t2d, a2d;

  mDepth = 1;     // just for arguments sake

  mTwipsToPixels = (float)72.0/(float)NSIntPointsToTwips(72);
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  GetTwipsToDevUnits(newscale);
  aPrinterContext->GetTwipsToDevUnits(origscale);
  mPixelScale = newscale / origscale;

  aPrinterContext->GetTwipsToDevUnits(t2d);
  aPrinterContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

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

  if (NS_OK != rv){
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
NS_IMETHODIMP nsDeviceContextPS::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
#ifdef NOTNOW
  if (nsnull == mColorSpace) {
    mColorSpace = IL_CreateGreyScaleColorSpace(1, 1);

    if (nsnull == mColorSpace) {
      aColorSpace = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Return the color space
  aColorSpace = mColorSpace;
  IL_AddRefToColorSpace(aColorSpace);
#endif

  if(nsnull==mColorSpace) {
      IL_RGBBits colorRGBBits;
    
      // Create a 24-bit color space
      colorRGBBits.red_shift = 16;  
      colorRGBBits.red_bits = 8;
      colorRGBBits.green_shift = 8;
      colorRGBBits.green_bits = 8; 
      colorRGBBits.blue_shift = 0; 
      colorRGBBits.blue_bits = 8;  
    
      mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 24);

    if (nsnull == mColorSpace) {
      aColorSpace = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }


  // Return the color space
  aColorSpace = mColorSpace;
  IL_AddRefToColorSpace(aColorSpace);

  return NS_OK;
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

  switch (anID) {
    case 0:
      break;
    default:
      break;
  }

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{

  aWidth = NSToIntRound((72.0f*8.0f) * mDevUnitsToAppUnits);
  aHeight = NSToIntRound((72.0f*10.0f) * mDevUnitsToAppUnits);

  return NS_OK;
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
NS_IMETHODIMP nsDeviceContextPS::BeginDocument(void)
{  
  nsIDeviceContextSpecPS *psSpec;
  nsresult res;

  if ( nsnull != mSpec ) {
    mPSObj = new nsPostScriptObj();  
    res = mSpec->QueryInterface(kIDeviceContextSpecPSIID, (void **) &psSpec);
    if ( res == NS_OK ) {
      mPSObj->Init(psSpec);
    }
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::EndDocument(void)
{

  delete mPSObj;
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

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetMetricsFor(const nsFont& aFont, nsIFontMetrics  *&aMetrics)
{
PRInt32         n,cnt;
nsresult        rv;

  // First check our cache
  n = mFontMetrics.Count();

  for (cnt = 0; cnt < n; cnt++){
    aMetrics = (nsIFontMetrics*) mFontMetrics.ElementAt(cnt);

    const nsFont  *font;
    aMetrics->GetFont(font);
    if (aFont.Equals(*font)){
      NS_ADDREF(aMetrics);
      return NS_OK;
    }
  }

  // It's not in the cache. Get font metrics and then cache them.
  nsIFontMetrics* fm = new nsFontMetricsPS();
  if (nsnull == fm) {
    aMetrics = nsnull;
    return NS_ERROR_FAILURE;
  }

  rv = fm->Init(aFont, this);

  if (NS_OK != rv) {
    aMetrics = nsnull;
    return rv;
  }

  mFontMetrics.AppendElement(fm);
  NS_ADDREF(fm);     // this is for the cache


  for (cnt = 0; cnt < n; cnt++){
    aMetrics = (nsIFontMetrics*) mFontMetrics.ElementAt(cnt);
    const nsFont  *font;
    aMetrics->GetFont(font);
  }

  NS_ADDREF(fm);      // this is for the routine that needs this font
  aMetrics = fm;
  return NS_OK;
}
