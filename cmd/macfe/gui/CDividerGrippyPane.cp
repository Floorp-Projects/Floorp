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

// Implementation for class that adapts the CPatternedGrippyPane for use in the chrome of
// the window as visual feedback/interaction point for sliding a drawer in and out. 
//
// NOTE: Makes an "implicit" assumption that the superView is an LDividedView. By this
// I mean that it assumes that the super view will handle certain behaviors and will 
// understand certain messages broadcast to it. There are no dependancies on data or
// on the LDividedView class itself.
//

#include "CDividerGrippyPane.h"
#include "macutil.h"


//
// Constructor
//
CDividerGrippyPane :: CDividerGrippyPane ( LStream* inStream )
	: CPatternedGrippyPane(inStream)
{

}


//
// ClickSelf
//
// Overloaded to handle clicking and dragging. If the user drags the grippy pane,
// step aside and let the parent's ClickSelf() routine handle it (which is most likely
// an LDividedView). If the user just clicks on the pane, send a message saying that
// there was a click (the LDividedView is listening).
//
void CDividerGrippyPane::ClickSelf(const SMouseDownEvent& inMouseDown)
{
	// Drags are passed to the superView.  Handle only single clicks.
	Point where;
	::GetMouse(&where);
	if (::WaitForMouseAction(
		where,
		inMouseDown.macEvent.when,
		::LMGetDoubleTime()) == MOUSE_DRAGGING)
	{
		LView* superView = GetSuperView();
		if (superView)
		{
			this->LocalToPortPoint(((SMouseDownEvent&)inMouseDown).whereLocal);
			superView->PortToLocalPoint(((SMouseDownEvent&)inMouseDown).whereLocal);
			superView->ClickSelf(inMouseDown);
		}
		return;
	} // if drag
	else
		BroadcastMessage ( 'Zap!', this );
	
} // ClickSelf


//
// AdjustCursorSelf
//
// Change the cursor to a hand while mouse is over this pane
//
void CDividerGrippyPane::AdjustCursorSelf(Point /* inPortPt */, const EventRecord & /*inMacEvent*/)
{
	const ResIDT kPointingFingerID = 128;
	::SetCursor(*::GetCursor(kPointingFingerID));
	
} // AdjustCursorSelf
