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

#include "CPatternPane.h"
#include "UGraphicGizmos.h"
#include "CSharedPatternWorld.h"

#include "StRegionHandle.h"

// ---------------------------------------------------------------------------
//		¥ CPatternPane
// ---------------------------------------------------------------------------

CPatternPane::CPatternPane(LStream* inStream)
	:	mOrientation(CSharedPatternWorld::eOrientation_Port),
		mAdornWithBevel(false),
		mDontFill(false),
		mPatternWorld(nil),
	
		super(inStream)
{
	ThrowIfNil_(inStream);
	
	ResIDT	thePatternID;
	Int16	theOrientation;
	
	*inStream >> thePatternID;
	*inStream >> theOrientation;
	*inStream >> mAdornWithBevel;
	*inStream >> mDontFill;
	
	mOrientation = static_cast<CSharedPatternWorld::EPatternOrientation>(theOrientation);
	
	mPatternWorld = CSharedPatternWorld::CreateSharedPatternWorld(thePatternID);
	ThrowIfNil_(mPatternWorld);
	mPatternWorld->AddUser(this);
}

// ---------------------------------------------------------------------------
//		¥ ~CPatternPane
// ---------------------------------------------------------------------------

CPatternPane::~CPatternPane()
{
	mPatternWorld->RemoveUser(this);
}

// ---------------------------------------------------------------------------
//		¥ AdornWithBevel
// ---------------------------------------------------------------------------
	
void
CPatternPane::AdornWithBevel(
	const Rect& inFrameToBevel)
{
	UGraphicGizmos::BevelTintRect(inFrameToBevel, -1, 0x4000, 0x4000);
}

// ---------------------------------------------------------------------------
//		¥ DrawSelf
// ---------------------------------------------------------------------------
	
void
CPatternPane::DrawSelf()
{
	Rect theLocalFrame;
	
	if (!CalcLocalFrameRect(theLocalFrame))
		return;
	
	CGrafPtr	thePort;
	Point		theAlignment;
	
	::GetPort(&(GrafPtr)thePort);
	
	CSharedPatternWorld::CalcRelativePoint(this, mOrientation, theAlignment);

	// Fill the pane with the specified pattern
	
	if (mDontFill)
	{
		if (mAdornWithBevel)
		{	
			// If no filling is specified, but adorning is specified,
			// then we must adjust clip region appropriately and then
			// fill in order to ensure a properly patterned bevel.
			
			StClipRgnState	theClipRegionState;
			StRegionHandle	theClipRegion;
			Rect			theRect = theLocalFrame;
			
			theClipRegion	= theRect;
			::InsetRect(&theRect, 1, 1);
			theClipRegion  -= theRect;
			
			::SetClip(theClipRegion);

			mPatternWorld->Fill(thePort, theLocalFrame, theAlignment);
		}
	}
	else
	{
		mPatternWorld->Fill(thePort, theLocalFrame, theAlignment);
	}	

	// Now bevel the pane
	
	if (mAdornWithBevel)
		AdornWithBevel(theLocalFrame);
}