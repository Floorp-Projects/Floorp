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

#pragma once

#include "CTooltipAttachment.h"


class CInternetKeywordTooltipPane : public CToolTipPane
{
public:
	enum { class_ID = 'IKTT' } ;
	
	static CInternetKeywordTooltipPane*
		 CreateInternetKeywordTooltipPaneStream ( LStream* inStream ) ;

	CInternetKeywordTooltipPane ( LStream* inStream ) : CToolTipPane(inStream) { };
	virtual ~CInternetKeywordTooltipPane ( ) { } ;

	void CalcFrameWithRespectTo ( LWindow* inOwningWindow, LPane* inOwningPane,
									const EventRecord& inMacEvent, Rect& outPortFrame ) ;
	virtual	void CalcTipText( LWindow* inOwningWindow, LPane* inOwningPane,
								const EventRecord& inMacEvent, StringPtr outTipText) ;

}; // CDynamicTooltipPane
