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

#include "nsFontMetricsPS.h"
#include "nsDeviceContextPS.h"
#include "nsRenderingContextPS.h"

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
nsFontMetricsPS :: nsFontMetricsPS()
{
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

  if (mDeviceContext) {
    // Notify our device context that owns us so that it can update its font cache
    mDeviceContext->FontMetricsDeleted(this);
    mDeviceContext = nsnull;
  }
}

NS_IMPL_ISUPPORTS1(nsFontMetricsPS, nsIFontMetrics)

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: Init(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIDeviceContext *aContext)
{
  mLangGroup = aLangGroup;

  mFont = new nsFont(aFont);
  //don't addref this to avoid circular refs
  mDeviceContext = (nsDeviceContextPS *)aContext;

  mFontPS = nsFontPS::FindFont(aFont, NS_STATIC_CAST(nsIFontMetrics*, this));
  NS_ENSURE_TRUE(mFontPS, NS_ERROR_FAILURE);

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
  if (mFont && mDeviceContext) {
    float dev2app;
    mDeviceContext->GetDevUnitsToAppUnits(dev2app);
    mFontPS->RealizeFont(this, dev2app);
  }
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
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
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

NS_IMETHODIMP
nsFontMetricsPS :: GetAveCharWidth(nscoord &aAveCharWidth)
{
  aAveCharWidth = mAveCharWidth;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
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

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

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
  NS_ENSURE_TRUE(mFontPS, NS_ERROR_NULL_POINTER);
  aWidth = mFontPS->GetWidth(aString, aLength);
  return NS_OK;

}


/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetStringWidth(const PRUnichar *aString,nscoord& aWidth,nscoord aLength)
{
  NS_ENSURE_TRUE(mFontPS, NS_ERROR_NULL_POINTER);
  aWidth = mFontPS->GetWidth(aString, aLength);
  return NS_OK;
}

// nsFontPS
nsFontPS*
nsFontPS::FindFont(const nsFont& aFont, nsIFontMetrics* aFontMetrics)
{
  /* Currently, we only select font from afm file.
   * In the future, this function is responsible for font selection
   * from different type of fonts.
   */
  nsFontPSAFM* fontAFM = new nsFontPSAFM(aFont, aFontMetrics);
  if (!fontAFM) return nsnull;
  
  if (fontAFM->mFontIndex < 0) {
    delete fontAFM;
    return nsnull;
  }

  return fontAFM;
}

nsFontPS::nsFontPS(const nsFont& aFont, nsIFontMetrics* aFontMetrics) :
mFontIndex(-1)
{
  mFont = new nsFont(aFont);
  if (!mFont) return;
  mFontMetrics = aFontMetrics;
}

nsFontPS::~nsFontPS()
{
  if (mFont) {
    delete mFont;
    mFont = nsnull;
  }

  if (mCCMap) {
    FreeCCMap(mCCMap);
  }

  mFontMetrics = nsnull;
}

// nsFontPSAFM
nsFontPSAFM::nsFontPSAFM(const nsFont& aFont, nsIFontMetrics* aFontMetrics) :
nsFontPS(aFont, aFontMetrics)
{
  if (!mFont) return;

  // get the AFM information
  mAFMInfo = new nsAFMObject();
  if (!mAFMInfo) return;

  mAFMInfo->Init(mFont->size / TWIPS_PER_POINT_FLOAT);

  // first see if the primary font is available
  mFontIndex = mAFMInfo->CheckBasicFonts(aFont, PR_TRUE);
  if (mFontIndex < 0) {
    // look in an AFM file for the primary font
    if (PR_FALSE == mAFMInfo->AFM_ReadFile(aFont)) {
      // look for secondary fonts
      mFontIndex = mAFMInfo->CheckBasicFonts(aFont, PR_FALSE);
      if (mFontIndex < 0) {
        mFontIndex = mAFMInfo->CreateSubstituteFont(aFont);
      }
    }
  }

  mFamilyName.AssignWithConversion((char*)mAFMInfo->mPSFontInfo->mFamilyName);
}

nsFontPSAFM::~nsFontPSAFM()
{
  if (mAFMInfo) {
    delete mAFMInfo;
    mAFMInfo = nsnull;
  }
}

nscoord
nsFontPSAFM::GetWidth(const char* aString, PRUint32 aLength)
{
  nscoord width;
  if (mAFMInfo) {
    mAFMInfo->GetStringWidth(aString, width, aLength);
  }
  return width;
}

nscoord
nsFontPSAFM::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  nscoord width;
  if (mAFMInfo) {
    mAFMInfo->GetStringWidth(aString, width, aLength);
  }
  return width;
}

nscoord
nsFontPSAFM::DrawString(nsRenderingContextPS* aContext,
                        nscoord aX, nscoord aY,
                        const char* aString, PRUint32 aLength)
{
  NS_ENSURE_TRUE(aContext, 0);
  nsPostScriptObj* psObj = aContext->GetPostScriptObj();
  NS_ENSURE_TRUE(psObj, 0);

  psObj->moveto(aX, aY);
  psObj->show(aString, aLength, "");
  return GetWidth(aString, aLength);
}

nscoord
nsFontPSAFM::DrawString(nsRenderingContextPS* aContext,
                        nscoord aX, nscoord aY,
                        const PRUnichar* aString, PRUint32 aLength)
{
  NS_ENSURE_TRUE(aContext, 0);
  nsPostScriptObj* psObj = aContext->GetPostScriptObj();
  NS_ENSURE_TRUE(psObj, 0);

  psObj->moveto(aX, aY);
  psObj->show(aString, aLength, "");
  return GetWidth(aString, aLength);
}

nsresult
nsFontPSAFM::RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app)
{
  NS_ENSURE_ARG_POINTER(aFontMetrics);

  float fontSize;
  float offset;

  nscoord onePixel = NSToCoordRound(1 * dev2app);

  // convert the font size which is in twips to points
  fontSize = mFont->size / TWIPS_PER_POINT_FLOAT;

  offset = NSFloatPointsToTwips(fontSize * mAFMInfo->mPSFontInfo->mXHeight) / 1000.0f;
  nscoord xHeight = NSToCoordRound(offset);
  aFontMetrics->SetXHeight(xHeight);
  aFontMetrics->SetSuperscriptOffset(xHeight);
  aFontMetrics->SetSubscriptOffset(xHeight);
  aFontMetrics->SetStrikeout((nscoord)(xHeight / TWIPS_PER_POINT_FLOAT), onePixel);

  offset = NSFloatPointsToTwips(fontSize * mAFMInfo->mPSFontInfo->mUnderlinePosition) / 1000.0f;
  aFontMetrics->SetUnderline(NSToCoordRound(offset), onePixel);

  nscoord size = NSToCoordRound(fontSize * dev2app);
  aFontMetrics->SetHeight(size);
  aFontMetrics->SetEmHeight(size);
  aFontMetrics->SetMaxAdvance(size);
  aFontMetrics->SetMaxHeight(size);

  offset = NSFloatPointsToTwips(fontSize * mAFMInfo->mPSFontInfo->mAscender) / 1000.0f;
  nscoord ascent = NSToCoordRound(offset);
  aFontMetrics->SetAscent(ascent);
  aFontMetrics->SetEmAscent(ascent);
  aFontMetrics->SetMaxAscent(ascent);

  offset = NSFloatPointsToTwips(fontSize * mAFMInfo->mPSFontInfo->mDescender) / 1000.0f;
  nscoord descent = -(NSToCoordRound(offset));
  aFontMetrics->SetDescent(descent);
  aFontMetrics->SetEmDescent(descent);
  aFontMetrics->SetMaxDescent(descent);

  aFontMetrics->SetLeading(0);

  nscoord spaceWidth = GetWidth(" ", 1);
  aFontMetrics->SetSpaceWidth(spaceWidth);

  nscoord aveCharWidth = GetWidth("x", 1);
  aFontMetrics->SetAveCharWidth(aveCharWidth);

  return NS_OK;
}

#ifdef MOZ_MATHML
nsresult
nsFontPSAFM::GetBoundingMetrics(const char*        aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFontPSAFM::GetBoundingMetrics(const PRUnichar*   aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
