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
 * Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 * Bradley Baetz <bbaetz@cs.mcgill.ca>
 *
 */

#include <unistd.h>
#include "nsDeviceContextXP.h"
#include "nsRenderingContextXP.h"
#include "nsString.h"
#include "nsFontMetricsXP.h"
#include "nsIDeviceContextSpecXPrint.h"
#include "il_util.h"
#include "nspr.h"
#include "nsXPrintContext.h"

static PRLogModuleInfo *nsDeviceContextXPLM = PR_NewLogModule("nsDeviceContextXP");

static NS_DEFINE_IID(kIDeviceContextSpecXPIID, NS_IDEVICE_CONTEXT_SPEC_XP_IID);

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
nsDeviceContextXP :: nsDeviceContextXP()
{
  NS_INIT_REFCNT();
  /* Inherited from xlib device context code */
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
  mNumCells = 0;
  mPrintContext = nsnull;

  mWidthFloat  = 0.0f;
  mHeightFloat = 0.0f;
  mWidth  = -1;
  mHeight = -1;

  NS_NewISupportsArray(getter_AddRefs(mFontMetrics));
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
nsDeviceContextXP :: ~nsDeviceContextXP() { }


NS_IMETHODIMP
nsDeviceContextXP :: SetSpec(nsIDeviceContextSpec* aSpec)
{
  PR_LOG(nsDeviceContextXPLM, PR_LOG_DEBUG, ("nsDeviceContextXP::SetSpec()\n"));

  nsCOMPtr<nsIDeviceContextSpecXP> xpSpec;

  mSpec = aSpec;

  if (!mPrintContext) {
     mPrintContext = new nsXPrintContext();
     xpSpec = do_QueryInterface(mSpec);
     if (xpSpec) {
        mPrintContext->Init(xpSpec);
     }
  }
 
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsDeviceContextXP,
                            DeviceContextImpl,
                            nsIDeviceContextXP)

/** ---------------------------------------------------
 *  See documentation in nsDeviceContextXP.h
 * 
 * This method is called after SetSpec
 */
NS_IMETHODIMP
nsDeviceContextXP::InitDeviceContextXP(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aPrinterContext)
{
  float t2d, a2d;
  // Initialization moved to SetSpec to be done after creating the Print Context
  int print_resolution;
  mPrintContext->GetPrintResolution(print_resolution);

  mTwipsToPixels = (float)print_resolution/(float)NSIntPointsToTwips(72);
  mTwipsToPixels = mTwipsToPixels * 2.0;
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  aPrinterContext->GetTwipsToDevUnits(t2d);
  aPrinterContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  mScreen  = mPrintContext->GetScreen();
  mDisplay = mPrintContext->GetDisplay();

  // mPrintContext->GetAppUnitsToDevUnits(mAppUnitsToDevUnits);
  // mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  mPrintContext->GetTextZoom(mTextZoom);
  return  NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsDeviceContextXP :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
   nsresult  rv = NS_ERROR_OUT_OF_MEMORY;

   nsCOMPtr<nsRenderingContextXP> xpContext;

   xpContext = new nsRenderingContextXP();
   if (xpContext){
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
NS_IMETHODIMP nsDeviceContextXP :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  aSupportsWidgets = PR_FALSE;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXP :: GetScrollBarDimensions(float &aWidth, 
                                        float &aHeight) const
{
  // XXX Oh, yeah.  These are hard coded.
  aWidth  = 15 * mPixelsToTwips;
  aHeight = 15 * mPixelsToTwips;

  return NS_OK;

}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXP :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXP::GetILColorSpace(IL_ColorSpace*& aColorSpace)
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

  if(!mColorSpace) {
      IL_RGBBits colorRGBBits;
    
      // Create a 24-bit color space
      colorRGBBits.red_shift   = 16;  
      colorRGBBits.red_bits    =  8;
      colorRGBBits.green_shift =  8;
      colorRGBBits.green_bits  =  8; 
      colorRGBBits.blue_shift  =  0; 
      colorRGBBits.blue_bits   =  8;  
    
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
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXP :: CheckFontExistence(const nsString& aFontName)
{
  PR_LOG(nsDeviceContextXPLM, PR_LOG_DEBUG, ("nsDeviceContextXP::CheckFontExistence()\n"));
  /* BUG: this does - unfortunately - not work due missing DISPLAY ptr
   * Additionally this is the _wrong_ time to make any assumtions 
   * about fonts - this info+the correct information is _only_ 
   * available _after_ XpSetContext() - therefore any assumtions 
   * made here are _wrong_ until we have a vaoid XPContext 
   * (X11 print context)
   */
#if 0
  return nsFontMetricsXP::FamilyExists(aFontName);
#else  
  char        **fnames = nsnull;
  PRInt32     namelen = aFontName.Length() + 1;
  char        *wildstring = (char *)PR_Malloc(namelen + 200);
  float       t2d;
  GetTwipsToDevUnits(t2d);
  PRInt32     dpi = NSToIntRound(t2d * 1440);
  int         numnames = 0;
  XFontStruct *fonts;
  nsresult    rv = NS_ERROR_FAILURE;

  if (!wildstring)
    return NS_ERROR_UNEXPECTED;

#if 0 /* gisburn: this cannot work - Xprint usually operates at >= 300dpi */
  if (abs(dpi - 75) < abs(dpi - 100))
    dpi = 75;
  else
    dpi = 100;
#endif

  PR_snprintf(wildstring, namelen + 200,
              " -*-%s-*-*-normal-*-*-*-%d-%d-*-*-*-*",
              aFontName.get(), dpi, dpi);

  /* applications must not make any assumptions about fonts _before_ XpSetContext() !!! */
  NS_ASSERTION((XpGetContext(mDisplay) != None), "Obtaining font information (XListFontsWithInfo()) _before_ XpSetContext()");
  fnames = ::XListFontsWithInfo(mDisplay, wildstring, 1, &numnames, &fonts);

  PR_LOG(nsDeviceContextXPLM, PR_LOG_DEBUG, 
         ("nsDeviceContextXP::CheckFontExistence: XListFontsWithInfo '%s'=%d\n", wildstring, numnames));

  if (numnames > 0)
  {
    ::XFreeFontInfo(fnames, fonts, numnames);
    rv = NS_OK;
  }

  PR_Free(wildstring);
  return NS_OK;
#endif  
}

NS_IMETHODIMP nsDeviceContextXP :: GetSystemAttribute(nsSystemAttrID anID, 
                                                SystemAttrStruct * aInfo) const
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
 */
NS_IMETHODIMP nsDeviceContextXP::GetDeviceSurfaceDimensions(PRInt32 &aWidth, 
                                                        PRInt32 &aHeight)
{
  float width, height;
  width = (float) mPrintContext->GetWidth();
  height = (float) mPrintContext->GetHeight();
  // width = width - 200;
  // height = height - 200;
  aWidth = NSToIntRound(width * mDevUnitsToAppUnits);
  aHeight = NSToIntRound(height * mDevUnitsToAppUnits);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXP::GetRect(nsRect &aRect)
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
NS_IMETHODIMP nsDeviceContextXP::GetClientRect(nsRect &aRect)
{
  return GetRect(aRect);
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXP::GetDeviceContextFor(nsIDeviceContextSpec *aDevice, nsIDeviceContext *&aContext)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXP::BeginDocument(PRUnichar * aTitle)
{  
  nsresult  rv = NS_OK;
  if (mPrintContext != nsnull) {
      rv = mPrintContext->BeginDocument(aTitle);
  } 
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXP::EndDocument(void)
{
  nsresult  rv = NS_OK;
  if (mPrintContext != nsnull) {
      rv = mPrintContext->EndDocument();
      
      // gisburn: mPrintContext cannot be reused between to print 
      // tasks as the destination print server may be a different one 
      // or the printer used on the same print server has other 
      // properties (build-in fonts for example ) than the printer 
      // previously used
      delete mPrintContext;
      mPrintContext = nsnull;
  } 
  
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXP::BeginPage(void)
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
NS_IMETHODIMP nsDeviceContextXP::EndPage(void)
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
NS_IMETHODIMP nsDeviceContextXP :: ConvertPixel(nscolor aColor, 
                                                        PRUint32 & aPixel)
{
  aPixel = aColor;
  return NS_OK;
}

Display *
nsDeviceContextXP::GetDisplay() 
{
   if (mPrintContext != nsnull) {
      return mDisplay; 
   } else {
      return (Display *)0;
   }
}

NS_IMETHODIMP nsDeviceContextXP::GetDepth(PRUint32& aDepth)
{
   if (mPrintContext != nsnull) {
      aDepth = mPrintContext->GetDepth(); 
   } else {
      aDepth = 0; 
   }
   return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXP::GetMetricsFor(const nsFont& aFont, 
                        nsIAtom* aLangGroup, nsIFontMetrics  *&aMetrics)
{
    return GetMetricsFor(aFont, aMetrics);
}

NS_IMETHODIMP nsDeviceContextXP::GetMetricsFor(const nsFont& aFont, 
                                        nsIFontMetrics  *&aMetrics)
{
  PRUint32         n,cnt;
  nsresult        rv;

  // First check our cache
  rv = mFontMetrics->Count(&n);
  if (NS_FAILED(rv))
    return rv;
  
  nsCOMPtr<nsIFontMetrics> m;

  for (cnt = 0; cnt < n; cnt++) {
    if (NS_SUCCEEDED(mFontMetrics->QueryElementAt(cnt,
                                                  NS_GET_IID(nsIFontMetrics),
                                                  getter_AddRefs(m)))) {
      const nsFont* font;
      m->GetFont(font);
      if (aFont.Equals(*font)) {
        aMetrics = m;
        NS_ADDREF(aMetrics);
        return NS_OK;
      }
    }
  }

  // It's not in the cache. Get font metrics and then cache them.
  nsCOMPtr<nsIFontMetrics> fm = new nsFontMetricsXP();
  if (!fm) {
    aMetrics = nsnull;
    return NS_ERROR_FAILURE;
  }

  // XXX need to pass real lang group
  rv = fm->Init(aFont, nsnull, this);

  if (NS_FAILED(rv)) {
    aMetrics = nsnull;
    return rv;
  }

  mFontMetrics->AppendElement(fm);

  aMetrics = fm;
  NS_ADDREF(aMetrics);
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextXP::GetPrintContext(nsXPrintContext*& aContext) {
  aContext = mPrintContext;
  //NS_ADDREF(aContext);
  return NS_OK;
}
