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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsUnicodeRenderingToolkit_h__
#define nsUnicodeRenderingToolkit_h__
#include "nsATSUIUtils.h"
#include <UnicodeConverter.h>
class nsUnicodeFallbackCache;
class nsIDeviceContext;
class nsGraphicState;

class nsUnicodeRenderingToolkit 
{
	nsUnicodeRenderingToolkit() {};
	~nsUnicodeRenderingToolkit() {};
public:
  NS_IMETHOD PrepareToDraw(float aP2T, nsIDeviceContext* aContext, nsGraphicState* aGS, GrafPtr aPort);
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
	
	PRBool 				QuestionMarkFallbackGetWidth(const PRUnichar *pChar, short& oWidth);
	PRBool 				QuestionMarkFallbackDrawChar(const PRUnichar *pChar, PRInt32 x, PRInt32 y, short& oWidth);
	
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

};
#endif /* nsUnicodeRenderingToolkit_h__ */
