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

#pragma once
#include "LCustomizeMenu.h"
class UCustomizePopUp	{
public:
	static long PopUpMenuSelect(MenuHandle menu, LCustomizeMenu* subclass, short top, short left, short popUpItem);
protected:
	static pascal void MDEF(short message, MenuHandle menu, Rect *rect, Point hitPt, short *item);
	static void Draw	(MenuHandle menu, MenuDefUPP*, Rect *rect, Point hitPt, short *item);	
	static void Choose	(MenuHandle menu, MenuDefUPP*, Rect *rect, Point hitPt, short *item);
	static void Size	(MenuHandle menu, MenuDefUPP*, Rect *rect, Point hitPt, short *item);
	static void PopUp	(MenuHandle menu, MenuDefUPP*, Rect *rect, Point hitPt, short *item);
private:
	static MenuDefUPP fMyMenuDefUPP;
	static MenuDefUPP* fOrigMenuDefUPP;
	static LCustomizeMenu*	fCustomize;
};
