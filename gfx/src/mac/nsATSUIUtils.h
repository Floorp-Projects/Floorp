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

#ifndef nsATSUIUtils_h___
#define nsATSUIUtils_h___


#include "nsCRT.h"
#include "nsError.h"
#include "nsCoord.h"
#include <ATSUnicode.h>


class ATSUILayoutCache;
class nsDrawingSurfaceMac;
class nsIDeviceContext;
class GraphicState;


class nsATSUIUtils
{
public:
	static void		Initialize();
	static PRBool	IsAvailable();
	static PRBool	ConvertToMacRoman(const PRUnichar *aString, PRUint32 aLength, char* macroman, PRBool onlyAllowASCII);

	static ATSUILayoutCache*	gTxLayoutCache;

private:
	static PRBool				gIsAvailable;
	static PRBool				gInitialized;
};



class nsATSUIToolkit
{
public:
	nsATSUIToolkit();
	~nsATSUIToolkit() {};

	void				PrepareToDraw(PRBool aFontOrColorChanged, GraphicState* aGS, GrafPtr aPort, nsIDeviceContext* aContext);

	NS_IMETHOD			GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth, PRInt32 *aFontID);
	NS_IMETHOD			DrawString(const PRUnichar *aString, PRUint32 aLength, nscoord aX, nscoord aY, PRInt32 aFontID, const nscoord* aSpacing);

private:
	ATSUTextLayout		GetTextLayout();
	
private:
	ATSUTextLayout  	mLastTextLayout;
	PRBool				mFontOrColorChanged;
	GraphicState*		mGS;
	GrafPtr				mPort;
	nsIDeviceContext*	mContext;
};

#endif //nsATSUIUtils_h___
