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

// Written by John R. McMullen

#pragma once

#include <LAttachment.h>

class LCommander;
struct SMouseDownEvent;
class LMenu;

//======================================
class CContextMenuAttachment : public LAttachment
//	By adding this to a pane in constructor, and defining a corresponding menu, you get
//	a context menu in that pane.
//
//	Your menu should have all the possible commands.  This attachment class will use the
//	FindCommandStatus call for your pane, and remove any items that are not currently available
//	before calling PopupMenuSelect.
//
//	If necessary, your pane's FindCommandStatus can distinguish context menu calls from other
//	calls by using ::StillDown(), because the normal menu enabling in PowerPlant does NOT
//	happen while the mouse is pressed.
//
//	This listens to mouse clicks, waits for timeout, handles popup menu etc. It will also
// 	handle changing the cursor if called from within AdjustCursorSelf(), etc.
//
//	Must be attached via constructor, just like CPaneEnabler.
//======================================
{
public:
	enum { class_ID = 'CxMn', msg_ContextMenu = 'CxMn', msg_ContextMenuCursor = 'CxMC'};
							CContextMenuAttachment(LStream* inStream);
	virtual					~CContextMenuAttachment();
	
		enum EClickState {
			eMouseHysteresis	=	6
		,	eMouseDragging		=	1
		,	eMouseTimeout
		,	eMouseUpEarly
		,	eHandledByAttachment
		};
	struct SExecuteParams
	{
		const SMouseDownEvent*	inMouseDown;
		EClickState				outResult;
	};

		enum { kDefaultDelay = -1};

	// Public utility method
	static EClickState		WaitMouseAction(
								const Point& 			inInitialPoint,
								Int32					inWhen,
								Int32					inDelay = kDefaultDelay);
protected:

	enum { kContextualCursorID = 6610 };
	
	virtual Boolean			Execute( MessageT inMessage, void *ioParam );

	virtual void			ExecuteSelf(
								MessageT				inMessage,
								void*					ioParam);
	void					DoPopup(
								const SMouseDownEvent&	inMouseDown,
								EClickState&			outResult);
	virtual UInt16			PruneMenu(LMenu* inMenu);
	
	// override to do something other than read in a menu from the resource fork
	virtual LMenu*			BuildMenu( ) ;

	virtual void			ChangeCursor( const EventRecord* inEvent );
	
//----
// Data
//----
private:
	ResIDT			mMenuID;
	ResIDT			mTextTraitsID;
	LView*			mHostView;
	LCommander*		mHostCommander; // same as view, precast for convenience
}; // class CContextMenuAttachment
