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

// Written by John R. McMullen

#pragma once

#include <LAttachment.h>
#include "Netscape_Constants.h"		// for EClickState

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
//	Must be attached via constructor, just like CPaneEnabler. [This is _so_ not true (pinkerton)]
//======================================
{
public:
	enum { class_ID = 'CxMn', msg_ContextMenu = 'CxMn', msg_ContextMenuCursor = 'CxMC'};
							CContextMenuAttachment(ResIDT inMenuID, ResIDT inTextTraitsID);
							CContextMenuAttachment(LStream* inStream);
	virtual					~CContextMenuAttachment();
	
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
	LPane*			mHostView;
	LCommander*		mHostCommander; // same as view, precast for convenience
}; // class CContextMenuAttachment
