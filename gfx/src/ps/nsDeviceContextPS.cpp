/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
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
 *   Ken Herron <kherron@fastmail.us>
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

#include "gfx-config.h"
 
/* PostScript/Xprint print modules do not support more than one object
 * instance because they use global vars which cannot be shared between
 * multiple instances...
 * bug 119491 ("Cleanup global vars in PostScript and Xprint modules) will fix
 * that...
 */
#define WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS 1

#define FORCE_PR_LOG /* Allow logging in the release build */
#define PR_LOGGING 1
#include "prlog.h"

#include "nsDeviceContextPS.h"
#include "nsRenderingContextPS.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsString.h"
#include "nsFontMetricsPS.h"
#include "nsPostScriptObj.h"
#include "nspr.h"
#include "nsILanguageAtomService.h"
#include "nsType8.h"
#include "nsPrintJobPS.h"
#include "nsPrintJobFactoryPS.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *nsDeviceContextPSLM = PR_NewLogModule("nsDeviceContextPS");
#endif /* PR_LOGGING */

nsIAtom* gUsersLocale = nsnull;

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
static int instance_counter = 0;
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

static PRBool
FreePSFontGeneratorList(nsHashKey *aKey, void *aData, void *aClosure)
{
  nsPSFontGenerator *psFG = (nsPSFontGenerator*)aData;
  if (psFG) {
    delete psFG;
    psFG = nsnull;
  }
  return PR_TRUE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS :: nsDeviceContextPS()
  : DeviceContextImpl(),
  mSpec(nsnull),
  mParentDeviceContext(nsnull),
  mPrintJob(nsnull),
  mPSObj(nsnull),
  mPSFontGeneratorList(nsnull)
{ 
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::nsDeviceContextPS()\n"));

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  instance_counter++;
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS::~nsDeviceContextPS()
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::~nsDeviceContextPS()\n"));

  delete mPSObj;
  delete mPrintJob;
  mParentDeviceContext = nsnull;

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  instance_counter--;
  NS_ASSERTION(instance_counter >= 0, "We cannot have less than zero instances.");
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

  if (mPSFontGeneratorList) {
    mPSFontGeneratorList->Reset(FreePSFontGeneratorList, nsnull);
    delete mPSFontGeneratorList;
    mPSFontGeneratorList = nsnull;
  }
  NS_IF_RELEASE(gUsersLocale);
}

NS_IMETHODIMP
nsDeviceContextPS::SetSpec(nsIDeviceContextSpec* aSpec)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::SetSpec()\n"));
  NS_PRECONDITION(!mPSObj, "Already have a postscript object");
  NS_PRECONDITION(!mPrintJob, "Already have a printjob object");

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
  if (instance_counter > 1) {
    return NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW;
  }
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

  mSpec = aSpec;

  mPSObj = new nsPostScriptObj();
  if (!mPSObj)
    return  NS_ERROR_OUT_OF_MEMORY; 

  nsresult rv;
  nsCOMPtr<nsIDeviceContextSpecPS> psSpec = do_QueryInterface(mSpec, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = mPSObj->Init(psSpec);
    if (NS_SUCCEEDED(rv))
      rv = nsPrintJobFactoryPS::CreatePrintJob(psSpec, mPrintJob);
  }
  if (NS_FAILED(rv) && mPSObj) {
    delete mPSObj;
    mPSObj = nsnull;
  }
  return rv;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDeviceContextPS,
                             DeviceContextImpl,
                             nsIDeviceContextPS)

/** ---------------------------------------------------
 *  See documentation in nsDeviceContextPS.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsDeviceContextPS::InitDeviceContextPS(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aParentContext)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::InitDeviceContextPS()\n"));

  float origscale, newscale;
  float t2d, a2d;

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
  if (instance_counter > 1) {
    return NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW;
  }
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

  NS_ENSURE_ARG_POINTER(aParentContext);

  mDepth = 24; /* Our PostScript module code expects images and other stuff in 24bit RGB-format (8bits per gun)*/

  mTwipsToPixels = (float)72.0/(float)NSIntPointsToTwips(72);
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  newscale = TwipsToDevUnits();
  origscale = aParentContext->TwipsToDevUnits();
  mCPixelScale = newscale / origscale;

  t2d = aParentContext->TwipsToDevUnits();
  a2d = aParentContext->AppUnitsToDevUnits();

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  mParentDeviceContext = aParentContext;

  mPSFontGeneratorList = new nsHashtable();
  NS_ENSURE_TRUE(mPSFontGeneratorList, NS_ERROR_OUT_OF_MEMORY);
 
  nsresult rv;
  nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = pref->GetBoolPref("font.FreeType2.enable", &mFTPEnable);
    if (NS_FAILED(rv))
      mFTPEnable = PR_FALSE;
    if (mFTPEnable) {
      rv = pref->GetBoolPref("font.FreeType2.printing", &mFTPEnable);
      if (NS_FAILED(rv))
        mFTPEnable = PR_FALSE;
    }
  }
  
  // the user's locale
  nsCOMPtr<nsILanguageAtomService> langService;
  langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
  if (langService) {
    langService->GetLocaleLanguageGroup(&gUsersLocale);
  }
  if (!gUsersLocale) {
    gUsersLocale = NS_NewAtom("x-western");
  }

  return NS_OK;
}

/** ---------------------------------------------------
 *  Create a Postscript RenderingContext.  This will create a RenderingContext
 *  from the deligate RenderingContext and intall that into our Postscript RenderingContext.
 *	@update 12/21/98 dwc
 *  @param aContext -- our newly created Postscript RenderingContextPS
 *  @return -- NS_OK if everything succeeded.
 */
NS_IMETHODIMP nsDeviceContextPS::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::CreateRenderingContext()\n"));

  nsresult rv;
   
  aContext = nsnull;

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsRenderingContextPS> renderingContext = new nsRenderingContextPS();
  if (!renderingContext)
    return NS_ERROR_OUT_OF_MEMORY;
     
  rv = renderingContext->Init(this);

  if (NS_SUCCEEDED(rv)) {
    aContext = renderingContext;
    NS_ADDREF(aContext);
  }

  return rv;
}

NS_IMETHODIMP nsDeviceContextPS::CreateRenderingContextInstance(nsIRenderingContext *&aContext)
{
  nsCOMPtr<nsIRenderingContext> renderingContext = new nsRenderingContextPS();
  if (!renderingContext)
    return NS_ERROR_OUT_OF_MEMORY;
         
  aContext = renderingContext;
  NS_ADDREF(aContext);
  
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::SupportsNativeWidgets()\n"));

  aSupportsWidgets = PR_FALSE;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetScrollBarDimensions()\n"));

  //XXX: Hardcoded values for Postscript
  float scale;
  GetCanonicalPixelScale(scale);
  aWidth  = 20.f * scale;
  aHeight = 20.f * scale;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDepth(PRUint32& aDepth)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetDepth(mDepth=%d)\n", mDepth));

  return mDepth;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::CheckFontExistence(const nsString& aFontName)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::CheckFontExistence()\n"));

  // XXX this needs to find out if this font is supported for postscript
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPS::GetSystemFont(nsSystemFontID aID, nsFont *aFont) const
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetSystemFont()\n"));

  if (mParentDeviceContext != nsnull) {
    return mParentDeviceContext->GetSystemFont(aID, aFont);
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetDeviceSurfaceDimensions()\n"));

  NS_ENSURE_TRUE(mPSObj && mPSObj->mPrintSetup, NS_ERROR_NULL_POINTER);

  // Height and width are already in twips
  aWidth  = mPSObj->mPrintSetup->width;
  aHeight = mPSObj->mPrintSetup->height;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextPS::GetRect(nsRect &aRect)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetRect()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

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
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetClientRect()\n"));

  return GetRect(aRect);
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceContextFor(nsIDeviceContextSpec *aDevice, nsIDeviceContext *&aContext)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetDeviceContextFor()\n"));
  aContext = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::BeginDocument(PRUnichar * aTitle, PRUnichar* aPrintToFileName, PRInt32 aStartPage, PRInt32 aEndPage)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::BeginDocument()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

  mPSObj->settitle(aTitle); 
  return NS_OK;
}

static PRBool PR_CALLBACK
GeneratePSFontCallback(nsHashKey *aKey, void *aData, void* aClosure)
{
  nsPSFontGenerator* psFontGenerator = (nsPSFontGenerator*)aData;
  NS_ENSURE_TRUE(psFontGenerator && aClosure, PR_FALSE);

  if (aClosure)
    psFontGenerator->GeneratePSFont((FILE *)aClosure);
  return PR_TRUE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *       @update 12/12/02 louie
 */
NS_IMETHODIMP nsDeviceContextPS::EndDocument(void)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::EndDocument()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

  // Finish the document and print it...
  nsresult rv = mPSObj->end_document();
  if (NS_SUCCEEDED(rv)) {
    FILE *submitFP;
    rv = mPrintJob->StartSubmission(&submitFP);
    if (NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED == rv) {
      // This was probably a print-preview operation
      rv = NS_OK;
    }
    else if (NS_SUCCEEDED(rv)) {
      NS_ASSERTION(submitFP, "No print job submission handle");

      // Start writing the print job to the job handler
      mPSObj->write_prolog(submitFP);

#ifdef MOZ_ENABLE_FREETYPE2
      // Before output Type8 font, check whether printer support CID font
      if (mFTPEnable && mPSFontGeneratorList)
        if (mPSFontGeneratorList->Count() > 0)
          AddCIDCheckCode(submitFP);
#endif
 
      /* Core of TrueType printing:
       *   enumerate items("nsPSFontGenerator") in hashtable
       *   to generate Type8 font and output to Postscript file
       */
      if (mPSFontGeneratorList)
        mPSFontGeneratorList->Enumerate(GeneratePSFontCallback,
            (void *) submitFP);

      rv = mPSObj->write_script(submitFP);
      if (NS_SUCCEEDED(rv))
        rv = mPrintJob->FinishSubmission();
    }
  }

  delete mPrintJob;
  mPrintJob = nsnull;

  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG,
      ("nsDeviceContextPS::EndDocument() return value %d\n", rv));

  return rv;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextPS::AbortDocument(void)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::AbortDocument()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);
  
  delete mPrintJob;
  mPrintJob = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::BeginPage(void)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::BeginPage()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

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
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::EndPage()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

  // end the page
  mPSObj->end_page();
  return NS_OK;
}

class nsFontCachePS : public nsFontCache
{
public:
  /* override DeviceContextImpl::CreateFontCache() */
  virtual nsresult CreateFontMetricsInstance(nsIFontMetrics** aResult);
};


nsresult nsFontCachePS::CreateFontMetricsInstance(nsIFontMetrics** aResult)
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
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::CreateFontCache()\n"));

  mFontCache = new nsFontCachePS();
  if (!mFontCache) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return mFontCache->Init(this);
}


