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
// Implementation for a pair of classes that provided a tooltip that takes its text dynamically
// based on the content of a pane and the mouse location. The CDynamicTooltipPane asks its 
// parent view what string it should display  depending on the current mouse location.
// The CDynamicTootipMixin class is mixed into the parent view to give the the correct 
// interface.
//

#include "CDynamicTooltips.h"


//
// CalcFrameWithRespectTo
//
// Figures out how big the tip needs to be and where it should be located in the port for
// the tip text. We are guaranteed that |mTip| (the string holding the text) has already
// been correctly set.
//
void
CDynamicTooltipPane :: CalcFrameWithRespectTo( LWindow* inOwningWindow, LPane*	inOwningPane,
												const EventRecord& inMacEvent, 
												Rect& outTipFrame)
{
	const short kXPadding = 5;
	const short kYPadding = 3;
	
	StTextState theTextSaver;
	UTextTraits::SetPortTextTraits(mTipTraitsID);
	
	FontInfo theFontInfo;
	::GetFontInfo(&theFontInfo);
	Int16 theTextHeight	= theFontInfo.ascent + theFontInfo.descent + theFontInfo.leading + (2 * 2);
	Int16 theTextWidth = ::StringWidth(mTip) + (2 * ::CharWidth(char_Space));

	Point theLocalPoint = inMacEvent.where;
	GlobalToPortPoint(theLocalPoint);

	Rect theWindowFrame;
	inOwningPane->CalcPortFrameRect(theWindowFrame);

	outTipFrame.left = theLocalPoint.h + kXPadding;
	outTipFrame.top = theLocalPoint.v + kYPadding;
	outTipFrame.right = outTipFrame.left + theTextWidth + kXPadding; 
	outTipFrame.bottom = outTipFrame.top + theTextHeight + kYPadding;

	ForceInPortFrame(inOwningWindow, outTipFrame);

} // CDynamicTooltipPane :: CalcFrameWithRespectTo


//
// CalcTipText
//
// Asks the parent pane (which has the interface provided by the CDynamicTooltipMixin class)
// for the tooltip at the current mouse location
//
void 
CDynamicTooltipPane :: CalcTipText( LWindow* /* inOwningWindow */, LPane* inOwningPane,
									const EventRecord& inMacEvent, StringPtr outTipText)
{
	CDynamicTooltipMixin* parent = dynamic_cast<CDynamicTooltipMixin*>(inOwningPane);
	Assert_(parent != NULL);
	parent->FindTooltipForMouseLocation ( inMacEvent, outTipText );

} // CDynamicTooltipPane :: CalcTipText
