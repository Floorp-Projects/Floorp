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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#ifndef nsUnicodeRenderingToolkit_h__
#define nsUnicodeRenderingToolkit_h__
#include "nsATSUIUtils.h"
#include <UnicodeConverter.h>
#include "nsCOMPtr.h"
#include "nsISaveAsCharset.h"
class nsUnicodeFallbackCache;
class nsIDeviceContext;
class nsGraphicState;

class nsUnicodeRenderingToolkit 
{
public:
	nsUnicodeRenderingToolkit() {};
	virtual ~nsUnicodeRenderingToolkit() {};

#ifdef IBMBIDI
  NS_IMETHOD PrepareToDraw(float aP2T, nsIDeviceContext* aContext, nsGraphicState* aGS, GrafPtr aPort, PRBool aRightToLeftText);
#else
  NS_IMETHOD PrepareToDraw(float aP2T, nsIDeviceContext* aContext, nsGraphicState* aGS, GrafPtr aPort);
#endif
  NS_IMETHOD GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth,
                      PRInt32 *aFontID);
  NS_IMETHOD DrawString(const PRUnichar *aString, PRUint32 aLength, nscoord aX, nscoord aY,
                        PRInt32 aFontID,
                        const nscoord* aSpacing);
private:	
	// Unicode text measure/drawing functions
	UnicodeToTextInfo 	GetConverterByScript(ScriptCode sc);
	PRBool 				TECFallbackGetWidth(const PRUnichar *pChar, short& oWidth,
									short fontNum, const short* scriptFallbackFonts);
	PRBool 				TECFallbackDrawChar(const PRUnichar *pChar, PRInt32 x, PRInt32 y, short& oWidth,
									short fontNum, const short* scriptFallbackFonts);
									
	PRBool 				ATSUIFallbackGetWidth(const PRUnichar *pChar, short& oWidth, short fontNum,	short aSize, PRBool aBold, PRBool aItalic, nscolor aColor);
	PRBool 				ATSUIFallbackDrawChar(const PRUnichar *pChar, PRInt32 x, PRInt32 y, short& oWidth, short fontNum, short aSize, PRBool aBold, PRBool aItalic, nscolor aColor);
	
	PRBool 				UPlusFallbackGetWidth(const PRUnichar *pChar, short& oWidth);
	PRBool 				UPlusFallbackDrawChar(const PRUnichar *pChar, PRInt32 x, PRInt32 y, short& oWidth);

	PRBool 				QuestionMarkFallbackGetWidth(const PRUnichar *pChar, short& oWidth);
	PRBool 				QuestionMarkFallbackDrawChar(const PRUnichar *pChar, PRInt32 x, PRInt32 y, short& oWidth);

	PRBool 				LatinFallbackGetWidth(const PRUnichar *pChar, short& oWidth);
	PRBool 				LatinFallbackDrawChar(const PRUnichar *pChar, PRInt32 x, PRInt32 y, short& oWidth);
	PRBool 				PrecomposeHangulFallbackGetWidth(const PRUnichar *pChar, short& oWidth,short koreanFont, short origFont);
	PRBool 				PrecomposeHangulFallbackDrawChar(const PRUnichar *pChar, PRInt32 x, PRInt32 y, short& oWidth,short koreanFont, short origFont);
	PRBool 				TransliterateFallbackGetWidth(const PRUnichar *pChar, short& oWidth);
	PRBool 				TransliterateFallbackDrawChar(const PRUnichar *pChar, PRInt32 x, PRInt32 y, short& oWidth);
	PRBool 				LoadTransliterator();
	
	void 				GetScriptTextWidth(const char* aText, ByteCount aLen, short& aWidth);
	void 				DrawScriptText(const char* aText, ByteCount aLen, PRInt32 x, PRInt32 y, short& aWidth);
	
	nsresult 			GetTextSegmentWidth( const PRUnichar *aString, PRUint32 aLength, short fontNum, const short *scriptFallbackFonts,
												PRUint32& oWidth);
	nsresult 			DrawTextSegment( const PRUnichar *aString, PRUint32 aLength, short fontNum, const short *scriptFallbackFonts,
												PRInt32 x, PRInt32 y, PRUint32& oWidth);
												
	nsUnicodeFallbackCache* GetTECFallbackCache();		
private:
	float             		mP2T; 				// Pixel to Twip conversion factor
	nsIDeviceContext *		mContext;
	nsGraphicState *		mGS;				// current graphic state - shortcut for mCurrentSurface->GetGS()

	GrafPtr					mPort;			// current grafPort - shortcut for mCurrentSurface->GetPort()
	nsATSUIToolkit			mATSUIToolkit;
	nsCOMPtr<nsISaveAsCharset> mTrans;
#ifdef IBMBIDI
	PRBool mRightToLeftText;
#endif

};
#endif /* nsUnicodeRenderingToolkit_h__ */
