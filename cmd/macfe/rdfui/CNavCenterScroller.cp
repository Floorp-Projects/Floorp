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
// Mike Pinkerton, Netscape Communications
//
// Subclass of LActiveScroller to handle resizing the vertical scrollbar
// when the column headers appear or disappear.
//

#include "CNavCenterScroller.h"
#include <LScrollBar.h>


CNavCenterScroller :: CNavCenterScroller ( LStream* inStream )
	: LScrollerView(inStream)
{
}


//
// ColumnHeadersChangedVisibility
//
// Adjust the vertical scrollbar when the column headers appear or disappear.
// If they are being hidden (probably the most common case since they appear by default
// in the PPob), move the top of the scrollbar up and stretch it by the size of the
// column headers. The only tricky part is that we move and resize by opposingly signed 
// amounts.
//
void
CNavCenterScroller :: ColumnHeadersChangedVisibility ( bool inNowVisible, Uint16 inHeaderHeight )
{
	Int16 delta = inNowVisible ? inHeaderHeight : -inHeaderHeight;
	
	mVerticalBar->MoveBy ( 0, delta, false );
	mVerticalBar->ResizeFrameBy ( 0, -delta, true );
		// why do we need to refresh the scrollbar? if we mode-switch on the fly while the
		// window is open (which we probably won't ever do...BUT), the scrollbar is the only
		// part that doesn't get redrawn by the coordinator. As a result, we need to make sure
		// it is done here. If the window is not visible, PowerPlant is smart enough not to
		// redraw it, so we don't get any overhead in the normal case. Wow, 11 lines of comments
		// for 3 lines of obivous code. You gotta love that! 

} // ColumnHeadesrChangedVisibility