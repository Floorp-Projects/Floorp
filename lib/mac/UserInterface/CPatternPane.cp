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