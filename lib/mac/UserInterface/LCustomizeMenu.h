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
