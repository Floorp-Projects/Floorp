/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
	dev2app = inDevContext.DevUnitsToAppUnits();
	short		textSize = float(aFont->size) / dev2app;
	textSize = PR_MAX(1, textSize);

	if (textSize < 9 && !nsFontUtils::DisplayVerySmallFonts())
		textSize = 9;
	
	Style textFace = normal;
	switch (aFont->style)
	{
		case NS_FONT_STYLE_NORMAL: 								break;
		case NS_FONT_STYLE_ITALIC: 		textFace |= italic;		break;
		case NS_FONT_STYLE_OBLIQUE: 	textFace |= italic;		break;	//XXX
	}

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

	RGBColor	black = {0};

	outStyle.tsFont = (short)fontNum;
	outStyle.tsFace = textFace;
	outStyle.tsSize = textSize;
	outStyle.tsColor = black;
}
	
