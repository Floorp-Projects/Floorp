/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


