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

#define kMaxToolbars (4)

enum
{
	BROWSER_MENU_TOGGLE_STRINGS_ID	= 1037
,	HIDE_NAVIGATION_TOOLBAR_STRING	= 5
,	SHOW_NAVIGATION_TOOLBAR_STRING	= 6
,	HIDE_LOCATION_TOOLBAR_STRING	= 7
,	SHOW_LOCATION_TOOLBAR_STRING	= 8
,	HIDE_MESSAGE_TOOLBAR_STRING		= 9
,	SHOW_MESSAGE_TOOLBAR_STRING		= 10
,	HIDE_FOLDERPANE_STRING			= 11
,	SHOW_FOLDERPANE_STRING			= 12
,	HIDE_NAVCENTER_STRING			= 13
,	SHOW_NAVCENTER_STRING			= 14
,	HIDE_PERSONAL_TOOLBAR_STRING	= 15
,	SHOW_PERSONAL_TOOLBAR_STRING	= 16
#ifdef MOZ_MAIL_NEWS
,	SAVE_DRAFT_STRING				= 17
,	SAVE_TEMPLATE_STRING			= 18
#endif //MOZ_MAIL_NEWS
};


class CNSContext;
class CExtraFlavorAdder;
typedef struct URL_Struct_ URL_Struct;


//========================================================================================
class CNetscapeWindow
//========================================================================================
:	public CMediatedWindow
{
	private:
		typedef CMediatedWindow Inherited;
	public:
							CNetscapeWindow(
									LStream* 	inStream,
									DataIDT		inWindowType);
									
		virtual				~CNetscapeWindow();
	
		virtual void		FindCommandStatus(
									CommandT			inCommand,
									Boolean				&outEnabled,
									Boolean				&outUsesMark,
									Char16				&outMark,
									Str255				outName);
		virtual Boolean		ObeyCommand(
									CommandT			inCommand,
									void				*ioParam = nil);
		void				ShowOneDragBar(PaneIDT dragBarID, Boolean isShown);
		void				ToggleDragBar(PaneIDT dragBarID, int whichBar,
											const char* prefName = nil);

		void				AdjustStagger(NetscapeWindowT inType);
		virtual	CNSContext*	GetWindowContext() const = 0;
		virtual void		StopAllContexts();
		virtual Boolean		IsAnyContextBusy();
		virtual CExtraFlavorAdder* CreateExtraFlavorAdder() const { return nil; }
		virtual URL_Struct*	CreateURLForProxyDrag(char* outTitle = nil);

		static void			DisplayStatusMessageInAllWindows(ConstStringPtr inMessage);

	protected:
		// Visibility of toolbars and status bars
		Boolean				mToolbarShown[kMaxToolbars];
		virtual	void		DoDefaultPrefs() = 0;
}; // class CNetscapeWindowclass CNetscapeWindow
