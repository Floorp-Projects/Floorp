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

