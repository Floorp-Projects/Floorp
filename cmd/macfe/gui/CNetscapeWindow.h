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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CNetscapeWindow.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include "CWindowMediator.h"

typedef struct _History_entry History_entry;

#define	kMaxToolbars (4)

#define	BROWSER_MENU_TOGGLE_STRINGS_ID	(1037)
#define	HIDE_NAVIGATION_TOOLBAR_STRING	(5)
#define	SHOW_NAVIGATION_TOOLBAR_STRING	(6)
#define	HIDE_LOCATION_TOOLBAR_STRING	(7)
#define	SHOW_LOCATION_TOOLBAR_STRING	(8)
#define	HIDE_MESSAGE_TOOLBAR_STRING		(9)
#define	SHOW_MESSAGE_TOOLBAR_STRING		(10)
#define	HIDE_FOLDERPANE_STRING			(11)
#define	SHOW_FOLDERPANE_STRING			(12)
#define HIDE_NAVCENTER_STRING			(13)
#define SHOW_NAVCENTER_STRING			(14)
#define HIDE_PERSONAL_TOOLBAR_STRING	(15)
#define SHOW_PERSONAL_TOOLBAR_STRING	(16)


class CNSContext;
class CExtraFlavorAdder;
typedef struct URL_Struct_ URL_Struct;

class CNetscapeWindow : public CMediatedWindow
{
	private:
		typedef CMediatedWindow Inherited;
	public:
							CNetscapeWindow(
									LStream* 	inStream,
									DataIDT		inWindowType);
									
		virtual				~CNetscapeWindow();
	
		virtual Boolean		ObeyCommand(CommandT inCommand, void *ioParam = nil);
		void				ShowOneDragBar(PaneIDT dragBarID, Boolean isShown);
		void				ToggleDragBar(PaneIDT dragBarID, int whichBar,
											const char* prefName = nil);
		virtual	CNSContext*	GetWindowContext() const = 0;
		virtual CExtraFlavorAdder* CreateExtraFlavorAdder() const { return nil; }
		virtual URL_Struct*	CreateURLForProxyDrag(char* outTitle = nil);

		static void			DisplayStatusMessageInAllWindows(ConstStringPtr inMessage);

	protected:
		// Visibility of toolbars and status bars
		Boolean				mToolbarShown[kMaxToolbars];
		virtual	void		DoDefaultPrefs() = 0;
};


