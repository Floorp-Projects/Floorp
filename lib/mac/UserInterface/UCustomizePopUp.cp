/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "prtypes.h"
#include "UCustomizePopUp.h"

MenuDefUPP* UCustomizePopUp::fOrigMenuDefUPP = NULL;
MenuDefUPP UCustomizePopUp::fMyMenuDefUPP = NULL;
LCustomizeMenu* UCustomizePopUp::fCustomize = NULL;

long UCustomizePopUp::PopUpMenuSelect(MenuHandle menu, LCustomizeMenu* subclass, short top, short left, short popUpItem)
{
	long result;
	fOrigMenuDefUPP = (MenuDefUPP*)(*menu)->menuProc;
	fCustomize = subclass;
	if(UCustomizePopUp::fMyMenuDefUPP == NULL)
	{
		UCustomizePopUp::fMyMenuDefUPP = NewMenuDefProc(UCustomizePopUp::MDEF);
	}
	(*menu)->menuProc = (char**) &UCustomizePopUp::fMyMenuDefUPP;
	
	result = ::PopUpMenuSelect(menu, top, left, popUpItem);
	
	(*menu)->menuProc = (char**)fOrigMenuDefUPP;
	fCustomize = NULL;
	return result;
}

pascal void UCustomizePopUp::MDEF(short message, MenuHandle menu, Rect *rect, Point hitPt, short *item)
{
  // restore the application's A5, so that we can access global
  // variables.
  	long savedA5 = ::SetCurrentA5() ;	
	switch(message)
	{
		case mDrawMsg:
			UCustomizePopUp::fCustomize->Draw	(menu, UCustomizePopUp::fOrigMenuDefUPP, rect, hitPt, item);
			break;
		case mChooseMsg:
			UCustomizePopUp::fCustomize->Choose	(menu, UCustomizePopUp::fOrigMenuDefUPP, rect, hitPt, item);
			break;
		case mSizeMsg:
			UCustomizePopUp::fCustomize->Size	(menu, UCustomizePopUp::fOrigMenuDefUPP, rect, hitPt, item);
			break;
		case mPopUpMsg:
			UCustomizePopUp::fCustomize->PopUp	(menu, UCustomizePopUp::fOrigMenuDefUPP, rect, hitPt, item);
			break;
		default:
			Assert_(FALSE);
			break;
	}
  ::SetA5 (savedA5) ;
}


