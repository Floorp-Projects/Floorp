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
#include "LPatchedMenu.h"

class LCustomizeMenu : public LPatchedMenu {
public: 
	LCustomizeMenu();
	virtual void Draw	(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item);
	virtual void Size	(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item);
	virtual void Choose(MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item);
	virtual void PopUp  (MenuHandle menu, MenuDefUPP* root, Rect *rect, Point hitPt, short *item);
protected:
	virtual void DrawItem			(MenuHandle menu, int item, Rect& itemrect);
	virtual short MeasureItem		(MenuHandle menu, int item);
	
	// Function called by DrawItem() 
	virtual void DrawItemSeperator	(Rect& itemrect);
	virtual void DrawItemText		(Rect& itemrect, Str255 itemtext );
	virtual void DrawItemIcon		(Rect& itemrect, short iconindex);
	virtual void DrawItemCommand	(Rect& itemrect, short cmd);
	virtual void DrawItemMark		(Rect& itemrect, short mark);
	virtual void DrawItemSubmenuIndicator(Rect& itemrect);
	virtual void DrawItemDisable	(Rect& itemrect);

	virtual void MoveToItemTextPosition(Rect& itemrect);
	virtual void MoveToItemCommandPosition(Rect& itemrect);
	virtual void MoveToItemMarkPosition(Rect& itemrect);

	// Function called by MeasureItem() 
	virtual short MeasureItemText	(Str255 text);
	virtual short MeasureItemCommand(short command);
	virtual short MeasureItemSubmenuIndicator	()	{ return 16; };
	virtual short MeasureItemSlop	()	{ return 18; };
	
	
	// Shared Function
	virtual void InvertItem			(MenuHandle menu, int item, Rect* menurect, Boolean leaveblack);
	virtual void GetItemRect		(int item, Rect* menurect, Rect& itemrect);

	// Basic Testing function
	virtual Boolean ItemEnable		(MenuHandle menu, int item) 
			{ return  (item < 32 ) ?  (((1L << (item )) & (**menu).enableFlags) != 0) : true; };
	virtual	Boolean	HaveMark		(short cmd, short mark)
			{ return ((cmd != 0x1b) && (mark != 0)); };
	virtual	Boolean	HaveIcon		(short cmd, short iconindex)
			{ return ((cmd != 0x1b) && (cmd != 0x1c) && (iconindex != 0)); };
	virtual	Boolean	HaveCommand		(short cmd)
			{ return ( cmd > 0x1f); };
	virtual	Boolean	HaveSubmenu		(short cmd)
			{ return (cmd == 0x1b); };
private:
	short fItemHeight;
	short fItemCount;
	short fTextMargin;
	short fMarkMargin;
};
