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

#include "nsCarbonHelpers.h"

#include "nsFontMetricsMac.h"
#include "nsDeviceContextMac.h"
#include "nsUnicodeFontMappingMac.h"
#include "nsUnicodeMappingUtil.h"
#include "nsGfxUtils.h"

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

NS_IMPL_ISUPPORTS1(nsFontMetricsMac, nsIFontMetrics)


NS_IMETHODIMP nsFontMetricsMac::Init(const nsFont& aFont, nsIAtom* aLangGroup, nsIDeviceContext* aCX)
{
  NS_ASSERTION(!(nsnull == aCX), "attempt to init fontmetrics with null device context");

  mFont = new nsFont(aFont);
  mLangGroup = aLangGroup;
  mContext = aCX;
  RealizeFont();
	
	TextStyle		theStyle;
	nsFontMetricsMac::GetNativeTextStyle(*this, *mContext, theStyle);
	
  StTextStyleSetter styleSetter(theStyle);
  
  FontInfo fInfo;
  GetFontInfo(&fInfo);
  
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);

  mLeading    = NSToCoordRound(float(fInfo.leading) * dev2app);
  mEmAscent   = NSToCoordRound(float(fInfo.ascent) * dev2app);
  mEmDescent  = NSToCoordRound(float(fInfo.descent) * dev2app);
  mEmHeight   = mEmAscent + mEmDescent;

	mMaxHeight  = mEmHeight + mLeading;
  mMaxAscent  = mEmAscent;
  mMaxDescent = mEmDescent;

  mMaxAdvance = NSToCoordRound(float(::CharWidth('M')) * dev2app);	// don't use fInfo.widMax here
  mSpaceWidth = NSToCoordRound(float(::CharWidth(' ')) * dev2app);

  return NS_OK;
}

nsUnicodeFontMappingMac* nsFontMetricsMac::GetUnicodeFontMapping()
{
  if (!mFontMapping)
  {
  	// we should pass the documentCharset from the nsIDocument level and
  	// the lang attribute from the tag level to here.
  	// XXX hard code to some value till peterl pass them down.
  	nsAutoString langGroup;
  	if (mLangGroup)
  		mLangGroup->ToString(langGroup);
    else
      langGroup.AssignWithConversion("ja");
      
  	nsString lang;
  	mFontMapping = nsUnicodeFontMappingMac::GetCachedInstance(mFont, mContext, langGroup, lang);
  }
  
	return mFontMapping;
}


static void MapGenericFamilyToFont(const nsString& aGenericFamily, nsString& aFontFace, ScriptCode aScriptCode)
{
  // the CSS generic names (conversions from the old Mac Mozilla code for now)
  nsUnicodeMappingUtil* unicodeMappingUtil = nsUnicodeMappingUtil::GetSingleton();
  if (unicodeMappingUtil)
  {
    nsString*   foundFont = unicodeMappingUtil->GenericFontNameForScript(
          aScriptCode,
          unicodeMappingUtil->MapGenericFontNameType(aGenericFamily));
    if (foundFont)
    {
      aFontFace = *foundFont;
      return;
    }
  }
  
  NS_ASSERTION(0, "Failed to find a font");
  aFontFace.AssignWithConversion("Times");
	
  /*
  // fall back onto hard-coded font names
  if (aGenericFamily.EqualsIgnoreCase("serif"))
  {
    aFontFace.AssignWithConversion("Times");
  }
  else if (aGenericFamily.EqualsIgnoreCase("sans-serif"))
  {
    aFontFace.AssignWithConversion("Helvetica");
  }
  else if (aGenericFamily.EqualsIgnoreCase("cursive"))
  {
     aFontFace.AssignWithConversion("Apple Chancery");
  }
  else if (aGenericFamily.EqualsIgnoreCase("fantasy"))
  {
    aFontFace.AssignWithConversion("Gadget");
  }
  else if (aGenericFamily.EqualsIgnoreCase("monospace"))
  {
    aFontFace.AssignWithConversion("Courier");
  }
  else if (aGenericFamily.EqualsIgnoreCase("-moz-fixed"))
  {
    aFontFace.AssignWithConversion("Courier");
  }
  */
}

struct FontEnumData {
  FontEnumData(nsIDeviceContext* aDC, nsString& aFaceName, ScriptCode aScriptCode)
    : mContext(aDC), mFaceName(aFaceName), mScriptCode(aScriptCode)
  {}
  nsIDeviceContext* mContext;
  nsString&         mFaceName;
  ScriptCode		mScriptCode;
};

static PRBool FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  FontEnumData* data = (FontEnumData*)aData;
  if (aGeneric)
  {
    nsAutoString realFace;
    MapGenericFamilyToFont(aFamily, realFace, data->mScriptCode);
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
	nsUnicodeMappingUtil	*unicodeMappingUtil;
	ScriptCode				theScriptCode;

	unicodeMappingUtil = nsUnicodeMappingUtil::GetSingleton ();
	if (unicodeMappingUtil)
	{
		const char		*theCString;
		nsAutoString	theLangGroupString;

		if (mLangGroup)
			mLangGroup->ToString(theLangGroupString);
		else
			theLangGroupString.AssignWithConversion("ja");
		theCString = theLangGroupString.GetBuffer ();
		if (theCString)
			theScriptCode = unicodeMappingUtil->MapLangGroupToScriptCode (theCString);
		else
		{
			NS_ConvertUCS2toUTF8 theUnicodeString(theLangGroupString);

			theScriptCode = unicodeMappingUtil->MapLangGroupToScriptCode(theUnicodeString.get());
		}

	}
	else
		theScriptCode = GetScriptManagerVariable (smSysScript);

	FontEnumData  fontData(mContext, fontName, theScriptCode);
	mFont->EnumerateFamilies(FontEnumCallback, &fontData);
  
	nsDeviceContextMac::GetMacFontNumber(fontName, mFontNum);
}


NS_IMETHODIMP
nsFontMetricsMac::Destroy()
{
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP
nsFontMetricsMac :: GetXHeight(nscoord& aResult)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aResult = NSToCoordRound(float(mMaxAscent * 0.71f));		// 0.71 = 5 / 7
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
  aSize = NSToCoordRound(dev2app);
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
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight; // on Windows, it's mEmHeight + mLeading (= mMaxHeight on the Mac)
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
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
NS_IMETHODIMP nsFontMetricsMac::GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

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
	
	const nsFont *aFont;
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
	PRInt32 offset = aFont->weight % 100;
	PRInt32 baseWeight = aFont->weight / 100;
	NS_ASSERTION((offset < 10) || (offset > 90), "Invalid bolder or lighter value");
	if (offset == 0) {
		if (aFont->weight >= NS_FONT_WEIGHT_BOLD)
			textFace |= bold;
	} else {
		if (offset < 10)
			textFace |= bold;
	}

	if ( aFont->decorations & NS_FONT_DECORATION_UNDERLINE )
		textFace |= underline;
	if ( aFont->decorations & NS_FONT_DECORATION_OVERLINE )
		textFace |= underline;  // THIS IS WRONG, BUT HERE FOR COMPLETENESS
	if ( aFont->decorations & NS_FONT_DECORATION_LINE_THROUGH )
		textFace |= underline;  // THIS IS WRONG, BUT HERE FOR COMPLETENESS

	RGBColor	black = {0};
	
  float textZoom = 1.0;
  inDevContext.GetTextZoom(textZoom);
  textSize *= textZoom;

	outStyle.tsFont = (short)fontNum;
	outStyle.tsFace = textFace;
	outStyle.tsSize = textSize;
	outStyle.tsColor = black;
}
	
	
