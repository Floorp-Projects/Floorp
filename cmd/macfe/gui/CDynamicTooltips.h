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
// Interface for a pair of classes that provided a tooltip that takes its text dynamically
// based on the content of a pane and the mouse location. The CDynamicTooltipPane asks its 
// parent view what string it should display  depending on the current mouse location.
// The CDynamicTootipMixin class is mixed into the parent view to give the the correct 
// interface.
//

#pragma once

#include "CTooltipAttachment.h"

//
// class CDynamicTooltipPane
//
// Handles querying the parent view to get the appropriate text based on the mouse location.
// Works with the CDynamicTooltipMixin class, which the parent view MUST mixin for this
// to work.
//
// To use this class instead of the normal tooltip pane, include the PPob id of an
// instantiation of this class (located in Tooltips.cnst) in the appropriate 
// CTooltipAttachment's properties where it asks for the mTipPaneResID.
//
// NOTE: This will not _crash_ when the parent is not a CDynamicTooltipMixin, but will
// assert so you know you're using it incorrectly.
//

class CDynamicTooltipPane : public CToolTipPane
{
public:
	enum { class_ID = 'DyTT' } ;
	
	CDynamicTooltipPane ( LStream* inStream ) : CToolTipPane(inStream) { };
	virtual ~CDynamicTooltipPane ( ) { } ;

	virtual void		CalcFrameWithRespectTo(
							LWindow*				inOwningWindow,
							LPane*					inOwningPane,
							const EventRecord&		inMacEvent,
							Rect& 					outTipFrame);

	virtual	void 		CalcTipText(
							LWindow*				inOwningWindow,
							LPane*					inOwningPane,
							const EventRecord&		inMacEvent,
							StringPtr				outTipText);

}; // CDynamicTooltipPane


//
// class CDynamicTooltipMixin
//
// Mix into your view/pane and override the appropriate methods to return the correct 
// information to the query for dynamic tooltip text from the CDynamicTooltipPane.
// All methods are pure virtual so you must override them if you intend to use this class.
//
// There is no data in this class, as it only presents an interface.
//
// As a side note, in addition to implementing the FindTooltipforMouseLocation()
// method, you will probably want to override MouseWithin() and track when the
// mouse enters/leaves a "hot" area that gets a tooltip. This is quite important
// if you want to hide the tooltip when the mouse moves away from that spot. The
// code to do this in MouseWithin() would look something like:
//
//		if ( newArea != oldArea ) {
//			... update your own roll-over feedback as necessary ...
//			ExecuteAttachments ( msg_HideTooltip, nil );
//		}
//
// The CTooltipAttachment responds to this message by hiding the tooltip, thereby
// resetting it for the next "hot spot" the user may move the cursor to within
// the pane.
//

class CDynamicTooltipMixin
{

public:
	virtual void FindTooltipForMouseLocation ( const EventRecord& inMacEvent,
												StringPtr outTip ) = 0;

private:

	// make these private so you can't instantiate one of these standalone
	CDynamicTooltipMixin ( ) { } ;
	~CDynamicTooltipMixin ( ) { } ;
	
}; // CDynamicTooltipMixin
