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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <Appearance.h>

#include "CAMSavvyBevelView.h"

//
// Constructor and Destructor
//
// Since there is no stream data, or any local data at all for that matter, the
// constructor/destructor don't need to do anything
//

CAMSavvyBevelView::CAMSavvyBevelView(LStream *inStream)
	:	CBevelView(inStream)
{
	// nothing	
}


CAMSavvyBevelView::~CAMSavvyBevelView()
{
	// nothing
}


//
// DrawBevelFill
//
// Use Appearance Manager to draw the entire area, both bevel and interior. We use
// the Window List View Header rather than the Window Header because the list view
// header doesn't draw the dark bevel at the bottom and it looks nicer when you put
// multiple bevel rects next to each other.
//
void 
CAMSavvyBevelView :: DrawBeveledFill ( )
{
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	
	StClipRgnState theClipSaver(mBevelRegion);
	--theFrame.top;			// get rid of thick top/left bevels
	--theFrame.left;
	::DrawThemeWindowListViewHeader ( &theFrame, kThemeStateActive );

} // DrawBeveledFill



//
// DrawBeveledFrame
//
// Since the Appearance Manager draws both the bevel and the interior, we
// don't need to do anything separately like we would have if we were drawing
// by hand (the old way).
//
void 
CAMSavvyBevelView :: DrawBeveledFrame ( )
{
	// do nothing
}


//
// DrawBeveledSub
//
// Draw sub-bevels using appearance manager.
//
void 
CAMSavvyBevelView :: DrawBeveledSub( const SSubBevel& inDesc )
{
	Rect subFrame = inDesc.cachedLocalFrame;
	Int16 theInsetLevel = inDesc.bevelLevel;

	if (theInsetLevel == 0) {
		ApplyForeAndBackColors();
		::EraseRect(&subFrame);
	}
	else {
		if (theInsetLevel < 0)
			theInsetLevel = -theInsetLevel;
					
		::InsetRect(&subFrame, -(theInsetLevel), -(theInsetLevel));
		--subFrame.top;
		::DrawThemeWindowListViewHeader ( &subFrame, kThemeStateActive );
	}
}

