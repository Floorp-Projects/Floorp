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

#include "nsFontMetricsMac.h"
#include "nsDeviceContextMac.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

#define BAD_FONT_NUM	-1


nsFontMetricsMac :: nsFontMetricsMac()
{
  NS_INIT_REFCNT();
  mFont = nsnull;
  mFontNum = BAD_FONT_NUM;
}
  
nsFontMetricsMac :: ~nsFontMetricsMac()
{
  if (nsnull != mFont)
  {
    delete mFont;
    mFont = nsnull;
  }
}

//------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsFontMetricsMac, kIFontMetricsIID)

NS_IMETHODIMP nsFontMetricsMac :: Init(const nsFont& aFont, nsIDeviceContext* aCX)
{
  NS_ASSERTION(!(nsnull == aCX), "attempt to init fontmetrics with null device context");

  mFont = new nsFont(aFont);
  mContext = aCX;
  RealizeFont();
  if (mFont != nsnull)
    nsFontMetricsMac::SetFont(*mFont, mContext);

  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);

  FontInfo fInfo;
  ::GetFontInfo(&fInfo);
 
  mAscent = NSToCoordRound(float(fInfo.ascent) * dev2app);
  mDescent = NSToCoordRound(float(fInfo.descent) * dev2app);
  mLeading = NSToCoordRound(float(fInfo.leading) * dev2app);
  mHeight = mAscent + mDescent + mLeading;
  mMaxAscent = mAscent;
  mMaxDescent = mDescent;
  mMaxAdvance = NSToCoordRound(float(::CharWidth(' ')) * dev2app);	// don't use fInfo.widMax here

  return NS_OK;
}


static void MapGenericFamilyToFont(const nsString& aGenericFamily, nsString& aFontFace)
{
  // the CSS generic names (conversions from the old Mac Mozilla code for now)

  if (aGenericFamily.EqualsIgnoreCase("serif"))
  {
    aFontFace = "Times";
  }
  else if (aGenericFamily.EqualsIgnoreCase("sans-serif"))
  {
    aFontFace="Helvetica";
  }
  else if (aGenericFamily.EqualsIgnoreCase("cursive"))
  {
     aFontFace="Zapf Chancery";
  }
  else if (aGenericFamily.EqualsIgnoreCase("fantasy"))
  {
    aFontFace ="New Century Schlbk";
  }
  else if (aGenericFamily.EqualsIgnoreCase("monospace"))
  {
    aFontFace = "Courier";
  }
}

struct FontEnumData {
  FontEnumData(nsIDeviceContext* aDC, nsString& aFaceName)
    : mContext(aDC), mFaceName(aFaceName)
  {}
  nsIDeviceContext* mContext;
  nsString&         mFaceName;
};

static PRBool FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  FontEnumData* data = (FontEnumData*)aData;
  if (aGeneric)
  {
    nsAutoString realFace;
    MapGenericFamilyToFont(aFamily, realFace);
    data->mFaceName=realFace;
    return PR_FALSE;  // stop
  }
  else
  {
    nsAutoString realFace;
    PRBool  aliased;
    data->mContext->GetLocalFontName(aFamily, realFace, aliased);
    if (aliased || (NS_OK == data->mContext->CheckFontExistence(realFace)))
    {
    	data->mFaceName=realFace;
      	return PR_FALSE;  // stop
    }
  }
  return PR_TRUE;
}

void nsFontMetricsMac::RealizeFont()
{
  nsString faceName;
  FontEnumData  data(mContext,  mFont->name);
  mFont->EnumerateFamilies(FontEnumCallback, &data);
}


NS_IMETHODIMP
nsFontMetricsMac :: Destroy()
{
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP
nsFontMetricsMac :: GetXHeight(nscoord& aResult)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aResult = NSToCoordRound(float(mMaxAscent / 2) - dev2app);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetSuperscriptOffset(nscoord& aResult)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aResult = NSToCoordRound(float(mMaxAscent / 2) - dev2app);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetSubscriptOffset(nscoord& aResult)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aResult = NSToCoordRound(- float(mMaxDescent / 2) + dev2app);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aOffset = NSToCoordRound(float(mMaxAscent / 2) - dev2app);
  aSize   = dev2app;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aOffset = NSToCoordRound(- float(mMaxDescent / 2) + dev2app);
  aSize   = dev2app;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetFont(const nsFont *&aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetWidths(const nscoord *&aWidths)
{
  return NS_ERROR_NOT_IMPLEMENTED;	//XXX
}

NS_IMETHODIMP nsFontMetricsMac :: GetFontHandle(nsFontHandle &aHandle)
{
	// We have no 'font handles' on Mac like they have on Windows
	// so let's use it for the fontNum.
	if (mFontNum == BAD_FONT_NUM)
		nsDeviceContextMac::GetMacFontNumber(mFont->name, mFontNum);

	aHandle = (nsFontHandle)mFontNum;
	return NS_OK;
}

//------------------------------------------------------------------------

NS_GFX void nsFontMetricsMac :: SetFont(const nsFont& aFont, nsIDeviceContext* aContext)
{
	short fontNum;
			//¥TODO?: This is not very efficient. Look in nsDeviceContextMac::GetMacFontNumber()
	nsDeviceContextMac::GetMacFontNumber(aFont.name, fontNum);
	::TextFont(fontNum);

	float  dev2app;
	aContext->GetDevUnitsToAppUnits(dev2app);
	::TextSize(short(float(aFont.size) / dev2app));

	Style textFace = normal;
	switch (aFont.style)
	{
		case NS_FONT_STYLE_NORMAL: 								break;
		case NS_FONT_STYLE_ITALIC: 		textFace |= italic;		break;
		case NS_FONT_STYLE_OBLIQUE: 	textFace |= italic;		break;	//XXX
	}
	switch (aFont.variant)
	{
		case NS_FONT_VARIANT_NORMAL: 							break;
		case NS_FONT_VARIANT_SMALL_CAPS: textFace |= condense;	break;	//XXX
	}
	if (aFont.weight > NS_FONT_WEIGHT_NORMAL)	// don't test NS_FONT_WEIGHT_BOLD
		textFace |= bold;

	::TextFace(textFace);
}
