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

#ifndef nsATSUIUtils_h___
#define nsATSUIUtils_h___


#include "nsCRT.h"
#include "nsError.h"
#include "nsCoord.h"
#include "nsColor.h"
#include <ATSUnicode.h>

class ATSUILayoutCache;
class nsDrawingSurfaceMac;
class nsIDeviceContext;

class nsATSUIUtils
{
public:
	static void		Initialize();
	static PRBool	IsAvailable();

	static ATSUILayoutCache*	gTxLayoutCache;

private:
	static PRBool				gIsAvailable;
	static PRBool				gInitialized;
};



class nsATSUIToolkit
{
public:
	nsATSUIToolkit();
	virtual ~nsATSUIToolkit() {};

	void        		PrepareToDraw(GrafPtr aPort, nsIDeviceContext* aContext);
 
    NS_IMETHOD			GetWidth(const PRUnichar *aCharPt, short &oWidth, 
    						short aSize, short fontNum, PRBool aBold, PRBool aItalic, nscolor aColor);
    NS_IMETHOD			DrawString(const PRUnichar *aCharPt, PRInt32 x, PRInt32 y, short &oWidth, 
    						short aSize, short fontNum, PRBool aBold, PRBool aItalic, nscolor aColor);

private:
	void        		StartDraw(const PRUnichar *aCharPt, short aSize, short fontNum, PRBool aBold, PRBool aItalic, nscolor aColor, 
									GrafPtr& oSavePort, ATSUTextLayout& oLayout);
	void        		EndDraw(GrafPtr aSavePort);
	ATSUTextLayout		GetTextLayout(short aFontNum, short aSize, PRBool aBold, PRBool aItalic, nscolor aColor);
	
private:
	GrafPtr				mPort;

	nsIDeviceContext*	mContext;
};

#endif //nsATSUIUtils_h___
