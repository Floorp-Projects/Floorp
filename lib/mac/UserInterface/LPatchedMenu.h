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

#pragma once
class LPatchedMenu {
public:
	LPatchedMenu() {} ;
	virtual void Draw	(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item)
		{	CallMenuDefProc (*root, mDrawMsg, 	menu, rect, hitPt, item);	};
	virtual void Choose	(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item)
		{	CallMenuDefProc (*root, mChooseMsg, menu, rect, hitPt, item);	};
	virtual void Size	(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item)
		{	CallMenuDefProc (*root, mSizeMsg, 	menu, rect, hitPt, item);	};
	virtual void PopUp	(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item)
		{	CallMenuDefProc (*root, mPopUpMsg, 	menu, rect, hitPt, item);	};
};
