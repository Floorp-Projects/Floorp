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

#include <Fonts.h>		// for FetchFontInfo

#include "nsFontMetricsMac.h"
#include "nsDeviceContextMac.h"
#include "nsUnicodeFontMappingMac.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

#define BAD_FONT_NUM	-1


nsFontMetricsMac :: nsFontMetricsMac()
{
  NS_INIT_REFCNT();
  mFont = nsnull;
  mFontNum = BAD_FONT_NUM;
  mFontMapping = nsnull;
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

NS_IMPL_ISUPPORTS(nsFontMetricsMac, kIFontMetricsIID);

NS_IMETHODIMP nsFontMetricsMac :: Init(const nsFont& aFont, nsIDeviceContext* aCX)
{
  NS_ASSERTION(!(nsnull == aCX), "attempt to init fontmetrics with null device context");

  mFont = new nsFont(aFont);
  mContext = aCX;
  RealizeFont();
	
	TextStyle		theStyle;
	nsFontMetricsMac::GetNativeTextStyle(*this, *mContext, theStyle);
	
  FontInfo fInfo;
  // FetchFontInfo gets the font info without having to touch a grafport. It's 8.5 only
  #if !TARGET_CARBON
  OSErr	err = ::FetchFontInfo(mFontNum, theStyle.tsSize, theStyle.tsFace, &fInfo);
  NS_ASSERTION(err == noErr, "Error in FetchFontInfo");
  #else
  // pinkerton - hack because this routine isn't yet in carbon.
  fInfo.ascent = theStyle.tsSize;
  fInfo.descent = 3;
  fInfo.widMax = 12;
  fInfo.leading = 3;
  #endif
  
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);

  mAscent = NSToCoordRound(float(fInfo.ascent) * dev2app);
  mDescent = NSToCoordRound(float(fInfo.descent) * dev2app);
  mLeading = NSToCoordRound(float(fInfo.leading) * dev2app);
  mHeight = mAscent + mDescent + mLeading;
  mMaxAscent = mAscent;
  mMaxDescent = mDescent;

	GrafPtr thePort;
	::GetPort(&thePort);
  NS_ASSERTION(thePort != nil, "GrafPort is nil in nsFontMetricsMac::Init");
	if (thePort == nil)
	{
		mMaxAdvance = 0;
		mSpaceWidth = 0;
		return NS_ERROR_FAILURE;
	}

#if TARGET_CARBON
	short saveFont = GetPortTextFont(thePort);
 	short saveFace = GetPortTextFace(thePort);
 	short saveSize = GetPortTextSize(thePort);		
#else
	short saveFont = thePort->txFont;
	short saveSize = thePort->txSize;
	short saveFace = thePort->txFace;
#endif
	::TextFont(theStyle.tsFont);
	::TextSize(theStyle.tsSize);
	::TextFace(theStyle.tsFace);

  mMaxAdvance = NSToCoordRound(float(::CharWidth('M')) * dev2app);	// don't use fInfo.widMax here
  mSpaceWidth = NSToCoordRound(float(::CharWidth(' ')) * dev2app);

	::TextFont(saveFont);
	::TextSize(saveSize);
	::TextFace(saveFace);

  return NS_OK;
}

nsUnicodeFontMappingMac* nsFontMetricsMac :: GetUnicodeFontMapping()
{
	// we should pass the documentCharset from the nsIDocument level and
	// the lang attribute from the tag level to here.
	// XXX hard code to some value till peterl pass them down.
	nsAutoString lang("");
	nsAutoString documentCharset("ISO-8859-1");
	if(! mFontMapping)
		mFontMapping = nsUnicodeFontMappingMac::GetCachedInstance(mFont, mContext,documentCharset, lang);
	return mFontMapping;
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
  else if (aGenericFamily.EqualsIgnoreCase("-moz-fixed"))
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
    data->mFaceName = realFace;
    return PR_FALSE;  // stop
  }
  else
  {
    nsAutoString realFace;
    PRBool  aliased;
    data->mContext->GetLocalFontName(aFamily, realFace, aliased);
    if (aliased || (NS_OK == data->mContext->CheckFontExistence(realFace)))
    {
    	data->mFaceName = realFace;
      return PR_FALSE;  // stop
    }
  }
  return PR_TRUE;
}

void nsFontMetricsMac::RealizeFont()
{
	nsAutoString	fontName;
  FontEnumData  fontData(mContext, fontName);
  mFont->EnumerateFamilies(FontEnumCallback, &fontData);
  
  nsDeviceContextMac::GetMacFontNumber(fontName, mFontNum);
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
  aResult = NSToCoordRound(float(mMaxAscent / 2) - dev2app);
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
  aOffset = -NSToCoordRound( dev2app );
  aSize   = NSToCoordRound( dev2app );
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

nsresult nsFontMetricsMac :: GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
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
	// NOTE: the name in the mFont may be a comma-separated list of
	// font names, like "Verdana, Arial, sans-serif"
	// If you want to do the conversion again to a Mac font, you'll
	// have to EnumerateFamilies() to resolve it to an installed
	// font again.
	NS_PRECONDITION(mFontNum != BAD_FONT_NUM, "Font metrics have not been initialized");
	
	// We have no 'font handles' on Mac like they have on Windows
	// so let's use it for the fontNum.
	aHandle = (nsFontHandle)mFontNum;
	return NS_OK;
}

// A utility routine to the the text style in a convenient manner.
// This is static, which is unfortunate, because it introduces link
// dependencies between libraries that should not exist.
NS_EXPORT void nsFontMetricsMac::GetNativeTextStyle(nsIFontMetrics& inMetrics,
		const nsIDeviceContext& inDevContext, TextStyle &outStyle)
{
	
	nsFont	*aFont;
	inMetrics.GetFont(aFont);
	
	nsFontHandle	fontNum;
	inMetrics.GetFontHandle(fontNum);
	
	float  dev2app;
	inDevContext.GetDevUnitsToAppUnits(dev2app);
	short		textSize = float(aFont->size) / dev2app;

	if (textSize < 9 && !nsDeviceContextMac::DisplayVerySmallFonts())
		textSize = 9;
	
	Style textFace = normal;
	switch (aFont->style)
	{
		case NS_FONT_STYLE_NORMAL: 								break;
		case NS_FONT_STYLE_ITALIC: 		textFace |= italic;		break;
		case NS_FONT_STYLE_OBLIQUE: 	textFace |= italic;		break;	//XXX
	}
#if 0
	switch (aFont->variant)
	{
		case NS_FONT_VARIANT_NORMAL: 							break;
		case NS_FONT_VARIANT_SMALL_CAPS: 						break;
	}
#endif
	if (aFont->weight > NS_FONT_WEIGHT_NORMAL)	// don't test NS_FONT_WEIGHT_BOLD
		textFace |= bold;

	if ( aFont->decorations & NS_FONT_DECORATION_UNDERLINE )
		textFace |= underline;
	if ( aFont->decorations & NS_FONT_DECORATION_OVERLINE )
		textFace |= underline;  // THIS IS WRONG, BUT HERE FOR COMPLETENESS
	if ( aFont->decorations & NS_FONT_DECORATION_LINE_THROUGH )
		textFace |= underline;  // THIS IS WRONG, BUT HERE FOR COMPLETENESS

	RGBColor	black = {0};
	
	outStyle.tsFont = (short)fontNum;
	outStyle.tsFace = textFace;
	outStyle.tsSize = textSize;
	outStyle.tsColor = black;
}
	
	
