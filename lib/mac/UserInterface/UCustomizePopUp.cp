/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
			// Assert_(FALSE);
			// somehow we got message equal to 6 when we doing debug. So far I cannot find out any documentation
			// describe what is message 6 mean. We just let the default routine handle it for now.
			// We probably need to change it later- after we know what 6 mean...
			CallMenuDefProc(*UCustomizePopUp::fOrigMenuDefUPP, message, menu, rect, hitPt, item);
			break;
	}
  ::SetA5 (savedA5) ;
}


