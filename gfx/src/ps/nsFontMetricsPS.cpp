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
 */

#include "nsFontMetricsPS.h"
#include "nsDeviceContextPS.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
nsFontMetricsPS :: nsFontMetricsPS()
{
  NS_INIT_REFCNT();
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
nsFontMetricsPS :: ~nsFontMetricsPS()
{
  if (nsnull != mFont){
    delete mFont;
    mFont = nsnull;
  }

  if(nsnull != mAFMInfo){
    delete mAFMInfo;
    mAFMInfo = nsnull;
  }


  mDeviceContext = nsnull;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
#ifdef LEAK_DEBUG
nsrefcnt
nsFontMetricsPS :: AddRef()
{
  NS_PRECONDITION(mRefCnt != 0, "resurrecting a dead object");
  return ++mRefCnt;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
nsrefcnt
nsFontMetricsPS :: Release()
{
  NS_PRECONDITION(mRefCnt != 0, "too many release's");
  if (--mRefCnt == 0) {
    delete this;
  }
  return mRefCnt;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
nsresult
nsFontMetricsPS :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kIFontMetricsIID);
  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}
#else
NS_IMPL_ISUPPORTS(nsFontMetricsPS, kIFontMetricsIID)
#endif

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: Init(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIDeviceContext *aContext)
{

  mFont = new nsFont(aFont);
  //don't addref this to avoid circular refs
  mDeviceContext = (nsDeviceContextPS *)aContext;

  // get the AFM information
  mAFMInfo = new nsAFMObject();
  mAFMInfo->Init(mFont->size/20);

  // first see if the primary font is available
  mFontIndex = mAFMInfo->CheckBasicFonts(aFont,PR_TRUE);
  if( mFontIndex < 0){
    // look in an AFM file for the primary font
    if (PR_FALSE == mAFMInfo->AFM_ReadFile(aFont) ) {
      // look for secondary fonts
      mFontIndex = mAFMInfo->CheckBasicFonts(aFont,PR_FALSE);
      if( mFontIndex < 0){
        mFontIndex = mAFMInfo->CreateSubstituteFont(aFont);
      }
    }
  }

  RealizeFont();
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: Destroy()
{
  mDeviceContext = nsnull;
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
void
nsFontMetricsPS::RealizeFont()
{  
float fontsize;
float dev2app;
float offset;

  mDeviceContext->GetDevUnitsToAppUnits(dev2app);
  nscoord onePixel = NSToCoordRound(1 * dev2app);

  // convert the font size which is in twips to points
  fontsize = mFont->size/20.0f;

  offset=NSFloatPointsToTwips(fontsize*mAFMInfo->mPSFontInfo->mXHeight)/1000.0f;
  mXHeight = NSToCoordRound(offset);
  //mXHeight = NSToCoordRound((float)((fontsize*mAFMInfo->mPSFontInfo->mXHeight)/1000.0)*dev2app);

  mSuperscriptOffset = mXHeight;
  mSubscriptOffset = mXHeight;

  mStrikeoutSize = onePixel;
  mStrikeoutOffset = (nscoord)(mXHeight / 2.0f);
  mUnderlineSize = onePixel;


  offset=NSFloatPointsToTwips(fontsize*mAFMInfo->mPSFontInfo->mUnderlinePosition)/1000.0f;
  mUnderlineOffset = NSToCoordRound(offset);

  mHeight = NSToCoordRound(fontsize * dev2app);


  offset=NSFloatPointsToTwips(fontsize*mAFMInfo->mPSFontInfo->mAscender)/1000.0f;
  mAscent = NSToCoordRound(offset);

  offset=NSFloatPointsToTwips(fontsize*mAFMInfo->mPSFontInfo->mDescender)/1000.0f;
  mDescent = -(NSToCoordRound(offset));

  mLeading = 0;
  mEmHeight = mHeight;
  mEmAscent = mAscent;
  mEmDescent = mDescent;
  mMaxHeight = mHeight;
  mMaxAscent = mAscent;
  mMaxDescent = mDescent;
  mMaxAdvance = mHeight;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetFont(const nsFont *&aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  // XXX need to return real lang group
  *aLangGroup = nsnull;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS::GetFontHandle(nsFontHandle &aHandle)
{

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetStringWidth(const char *aString,nscoord& aWidth,nscoord aLength)
{

  if(mAFMInfo){
      mAFMInfo->GetStringWidth(aString,aWidth,aLength);
    }

  return NS_OK;

}


/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetStringWidth(const PRUnichar *aString,nscoord& aWidth,nscoord aLength)
{

  if(mAFMInfo){
      mAFMInfo->GetStringWidth(aString,aWidth,aLength);
    }

  return NS_OK;
}
