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
// a public service announcement from pinkerton:
// 
// Right now this class is in a state of flux because it is drawing partially with
// appearance, but can still draw the old way when appearance is not present. I want
// to rip out all the non-appearance stuff at some point, but we need a way to go back
// to the old way if appearance doesn't work out for us.
//

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <Appearance.h>

#include "CAMSavvyBevelView.h"
#include "UGraphicGizmos.h"
#include "CSharedPatternWorld.h"



// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
CAMSavvyBevelView::CAMSavvyBevelView(LStream *inStream)
	:	CBevelView(inStream)
{
	ResIDT theBevelTraitsID;
	*inStream >> theBevelTraitsID;
	
	ResIDT thePatternResID;
	*inStream >> thePatternResID;
	
	*inStream >> mPatternOrientation;
	
	if ( ! UEnvironment::HasFeature(env_HasAppearance) ) {
		UGraphicGizmos::LoadBevelTraits(theBevelTraitsID, mArithBevelColors);

		mPatternWorld = CSharedPatternWorld::CreateSharedPatternWorld(thePatternResID);
		ThrowIfNULL_(mPatternWorld);
		mPatternWorld->AddUser(this);
	}
	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CAMSavvyBevelView::~CAMSavvyBevelView()
{
	if ( !UEnvironment::HasFeature(env_HasAppearance) )
		mPatternWorld->RemoveUser(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CAMSavvyBevelView::DrawBeveledFill(void)
{
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	
	StClipRgnState theClipSaver(mBevelRegion);
	if ( UEnvironment::HasFeature(env_HasAppearance) ) {
		--theFrame.top;
		--theFrame.left;
		::DrawThemeWindowListViewHeader ( &theFrame, kThemeStateActive );
	}
	else {
		Point theAlignment;
		CSharedPatternWorld::CalcRelativePoint(this, CSharedPatternWorld::eOrientation_Port, theAlignment);
		
		CGrafPtr thePort = (CGrafPtr)GetMacPort();
		mPatternWorld->Fill(thePort, theFrame, theAlignment);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CAMSavvyBevelView::DrawBeveledFrame(void)
{
	if ( ! UEnvironment::HasFeature(env_HasAppearance) ) {
		Rect theFrame;
		CalcLocalFrameRect(theFrame);
		UGraphicGizmos::BevelTintRect(theFrame, mMainBevel, 0x4000, 0x4000);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CAMSavvyBevelView::DrawBeveledSub(const SSubBevel&	inDesc)
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
		if ( UEnvironment::HasFeature(env_HasAppearance) ) {
			--subFrame.top;
			::DrawThemeWindowListViewHeader ( &subFrame, kThemeStateActive );
		}
		else
			UGraphicGizmos::BevelTintRect(subFrame, inDesc.bevelLevel, 0x4000, 0x4000);
	}
}

