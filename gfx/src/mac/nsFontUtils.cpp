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


#include "nsIPref.h"
#include "nsIServiceManager.h"

#include "nsFont.h"
#include "nsDeviceContextMac.h"

#include "nsFontUtils.h"

PRBool nsFontUtils::sDisplayVerySmallFonts = true;


PRBool
nsFontUtils::DisplayVerySmallFonts()
{
	static PRBool sInitialized = PR_FALSE;
	if (sInitialized)
		return sDisplayVerySmallFonts;

	sInitialized = PR_TRUE;

  nsresult rv;
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefs) {
    PRBool boolVal;
    if (NS_SUCCEEDED(prefs->GetBoolPref("browser.display_very_small_fonts", &boolVal))) {
      sDisplayVerySmallFonts = boolVal;
    }
  }

	return sDisplayVerySmallFonts;
}


// A utility routine to the the text style in a convenient manner.
// This is static, which is unfortunate, because it introduces link
// dependencies between libraries that should not exist.
void
nsFontUtils::GetNativeTextStyle(nsIFontMetrics& inMetrics,
		const nsIDeviceContext& inDevContext, TextStyle &outStyle)
{
	
	const nsFont *aFont;
	inMetrics.GetFont(aFont);
	
	nsFontHandle	fontNum;
	inMetrics.GetFontHandle(fontNum);
	
	float  dev2app;
	inDevContext.GetDevUnitsToAppUnits(dev2app);
	short		textSize = float(aFont->size) / dev2app;

	if (textSize < 9 && !nsFontUtils::DisplayVerySmallFonts())
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
	
	outStyle.tsFont = (short)fontNum;
	outStyle.tsFace = textFace;
	outStyle.tsSize = textSize;
	outStyle.tsColor = black;
}
	
