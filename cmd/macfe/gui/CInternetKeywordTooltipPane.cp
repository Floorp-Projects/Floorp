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

//
// CInternetKeywordTooltipPane.h
// Mike Pinkerton
// Netscape Communications
//
// A new kind of tooltip pane which gets its text from the browser window. It
// displays the "internet keyword" of the current url, specified in the html.
//

#include "CInternetKeywordTooltipPane.h"

#include "CBrowserWindow.h"


CInternetKeywordTooltipPane*
CInternetKeywordTooltipPane :: CreateInternetKeywordTooltipPaneStream ( LStream* inStream )
{
	return new CInternetKeywordTooltipPane ( inStream );

}


//
// CalcTipText
//
// The current internet keyword has nicely been stashed in the browser window for us. Go look
// it up and make it the text of the tooltip.
//
void 
CInternetKeywordTooltipPane :: CalcTipText( LWindow* inOwningWindow, LPane* /*inOwningPane*/,
											const EventRecord& /*inMacEvent*/, StringPtr outTipText)
{
	CBrowserWindow* browserWindow = dynamic_cast<CBrowserWindow*>(inOwningWindow);
	const LStr255 & keyword = browserWindow->GetInternetKeyword();
	LStr255::CopyPStr ( keyword, outTipText );
		
} // CInternetKeywordTooltipPane :: CalcTipText


//
// CalcFrameWithRespectTo
//
// Use the inherited version to size the tooltip, etc, but after all is said and done,
// move it just below the bottom left corner of the parent.
//
void 
CInternetKeywordTooltipPane :: CalcFrameWithRespectTo ( LWindow* inOwningWindow,
														LPane* inOwningPane,
														const EventRecord& inMacEvent,
														Rect& outPortFrame )
{
	// calc pane size with it centered under the location bar
	CToolTipPane::CalcFrameWithRespectTo ( inOwningWindow, inOwningPane, inMacEvent, outPortFrame );
	
	Rect theOwningPortFrame;
	inOwningPane->CalcPortFrameRect(theOwningPortFrame);
	
	// move it to just below the bottom left corner
	const short width = outPortFrame.right - outPortFrame.left;
	outPortFrame.left = theOwningPortFrame.left;
	outPortFrame.right = theOwningPortFrame.left + width;
	
} // CalcFrameWithRespectTo
