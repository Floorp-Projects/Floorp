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
 *   Bradley Baetz <bbaetz@cs.mcgill.ca>
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1 /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"
 
#include "nsDeviceContextXP.h"
#include "nsRenderingContextXp.h"
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
  : nsDeviceContextX(),
    mPrintContext(nsnull),
    mSpec(nsnull),
    mParentDeviceContext(nsnull),
    mFontMetricsContext(nsnull),
    mRCContext(nsnull)
{
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
nsDeviceContextXp::SetSpec(nsIDeviceContextSpec* aSpec)
{
  nsresult  rv = NS_ERROR_FAILURE;
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::SetSpec()\n"));

  nsCOMPtr<nsIDeviceContextSpecXp> xpSpec;

  mSpec = aSpec;

  if (mPrintContext)
    DestroyXPContext(); // we cannot reuse that...
    
  mPrintContext = new nsXPrintContext();
  if (!mPrintContext)
    return  NS_ERROR_OUT_OF_MEMORY;
    
  xpSpec = do_QueryInterface(mSpec, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = mPrintContext->Init(this, xpSpec);

    if (NS_FAILED(rv)) {
      DestroyXPContext();
    }
  }
 
  return rv;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDeviceContextXp,
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
  nsresult rv;

  // Initialization moved to SetSpec to be done after creating the Print Context
  float origscale, newscale;
  float t2d, a2d;
  int   print_x_resolution,
        print_y_resolution;

  mPrintContext->GetPrintResolution(print_x_resolution, print_y_resolution);
  
  if (print_x_resolution != print_y_resolution) {
    PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("print_x_resolution != print_y_resolution not yet supported by Mozilla's layout engine\n"));
    return NS_ERROR_NOT_IMPLEMENTED; /* this error code should be more detailed */
  }

  mPixelsToTwips = (float)NSIntPointsToTwips(72) / (float)print_x_resolution;
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  newscale = TwipsToDevUnits();
  origscale = aParentContext->TwipsToDevUnits();

  mCPixelScale = newscale / origscale;

  t2d = aParentContext->TwipsToDevUnits();
  a2d = aParentContext->AppUnitsToDevUnits();

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  NS_ASSERTION(aParentContext, "aCreatingDeviceContext cannot be NULL!!!");
  mParentDeviceContext = aParentContext;

  /* be sure we've cleaned-up old rubbish - new values will re-populate nsFontMetricsXlib soon... */
  DeleteRenderingContextXlibContext(mRCContext);
  DeleteFontMetricsXlibContext(mFontMetricsContext);
  mRCContext          = nsnull;
  mFontMetricsContext = nsnull;
 
  rv = CreateFontMetricsXlibContext(this, PR_TRUE, &mFontMetricsContext);
  if (NS_FAILED(rv))
    return rv;

  rv = CreateRenderingContextXlibContext(this, &mRCContext);
  if (NS_FAILED(rv))
    return rv;
   
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsDeviceContextXp :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
  nsresult rv;
   
  aContext = nsnull;

  NS_ENSURE_TRUE(mPrintContext != nsnull, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsRenderingContextXp> renderingContext = new nsRenderingContextXp();
  if (!renderingContext)
    return NS_ERROR_OUT_OF_MEMORY;
     
  rv = renderingContext->Init(this);

  if (NS_SUCCEEDED(rv)) {
    aContext = renderingContext;
    NS_ADDREF(aContext);
  }

  return rv;
}

NS_IMETHODIMP nsDeviceContextXp::CreateRenderingContextInstance(nsIRenderingContext *&aContext)
{
  nsCOMPtr<nsIRenderingContext> renderingContext = new nsRenderingContextXp();
  if (!renderingContext)
    return NS_ERROR_OUT_OF_MEMORY;
         
  aContext = renderingContext;
  NS_ADDREF(aContext);
  
  return NS_OK;
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
  float scale;
  GetCanonicalPixelScale(scale);
  aWidth  = 15.f * mPixelsToTwips * scale;
  aHeight = 15.f * mPixelsToTwips * scale;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXp :: CheckFontExistence(const nsString& aFontName)
{
  return nsFontMetricsXlib::FamilyExists(mFontMetricsContext, aFontName);
}

NS_IMETHODIMP nsDeviceContextXp :: GetSystemFont(nsSystemFontID aID, 
                                                 nsFont *aFont) const
{
  if (mParentDeviceContext != nsnull) {
    return mParentDeviceContext->GetSystemFont(aID, aFont);
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::GetDeviceSurfaceDimensions(PRInt32 &aWidth, 
                                                        PRInt32 &aHeight)
{
  NS_ENSURE_TRUE(mPrintContext != nsnull, NS_ERROR_NULL_POINTER);

  float width, height;
  width  = (float) mPrintContext->GetWidth();
  height = (float) mPrintContext->GetHeight();

  aWidth  = NSToIntRound(width  * mDevUnitsToAppUnits);
  aHeight = NSToIntRound(height * mDevUnitsToAppUnits);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXp::GetRect(nsRect &aRect)
{
  NS_ENSURE_TRUE(mPrintContext != nsnull, NS_ERROR_NULL_POINTER);

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
  aContext = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::BeginDocument(PRUnichar * aTitle, PRUnichar* aPrintToFileName, PRInt32 aStartPage, PRInt32 aEndPage)
{  
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::BeginDocument()\n"));

  NS_ENSURE_TRUE(mPrintContext != nsnull, NS_ERROR_NULL_POINTER);

  return mPrintContext->BeginDocument(aTitle, aPrintToFileName, aStartPage, aEndPage);
}


void nsDeviceContextXp::DestroyXPContext()
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::DestroyXPContext()\n"));

  if (!mPrintContext)
    return;

  /* gisburn: mPrintContext cannot be reused between to print
   * tasks as the destination print server may be a different one
   * or the printer used on the same print server has other
   * properties (build-in fonts for example ) than the printer
   * previously used. */
  FlushFontCache();
  DeleteRenderingContextXlibContext(mRCContext);
  DeleteFontMetricsXlibContext(mFontMetricsContext);
  mRCContext          = nsnull;
  mFontMetricsContext = nsnull;

  mPrintContext = nsnull; // nsCOMPtr will call |delete mPrintContext;|
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::EndDocument(void)
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::EndDocument()\n"));

  NS_ENSURE_TRUE(mPrintContext != nsnull, NS_ERROR_NULL_POINTER);

  nsresult rv = mPrintContext->EndDocument();
  DestroyXPContext();
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::AbortDocument(void)
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::AbortDocument()\n"));

  NS_ENSURE_TRUE(mPrintContext != nsnull, NS_ERROR_NULL_POINTER);

  nsresult rv = mPrintContext->AbortDocument();
  DestroyXPContext();
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::BeginPage(void)
{
  NS_ENSURE_TRUE(mPrintContext != nsnull, NS_ERROR_NULL_POINTER);

  return mPrintContext->BeginPage();
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXp::EndPage(void)
{
  NS_ENSURE_TRUE(mPrintContext != nsnull, NS_ERROR_NULL_POINTER);

  return mPrintContext->EndPage();
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
  virtual nsresult CreateFontMetricsInstance(nsIFontMetrics** aResult);
};


nsresult nsFontCacheXp::CreateFontMetricsInstance(nsIFontMetrics** aResult)
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
  return mFontCache->Init(this);
}


