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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//	This is a replacement class for PowerPlant's LGWorld.  It allows for the
//	resizing and reorienting of the offscreen world, which LGWorld won't let
//	you do.  Because the CGWorld can be updated, the pixmap can be made
//	purgeable by passing in the pixPurge flag in the constructor.  LGWorld
//	would let you do this, however, if the pixels ever got purged you'd be
//	completely hosed.
//
//	This class has ben submitted to metrowerks for a potential replacement
//	of LGWorld.  In the event that metrowerks adds the extended functionality
//	that CGWorld offers, we can make this module obsolete.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CGWorld.h"
#include "UGraphicGizmos.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CGWorld
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CGWorld::CGWorld(
	const Rect&		inBounds,
	Int16 			inPixelDepth,
	GWorldFlags 	inFlags,
	CTabHandle 		inCTableH,
	GDHandle 		inGDeviceH)
{
	::GetGWorld(&mSavePort, &mSaveDevice);

	mMacGWorld = nil;
	mBounds = inBounds;
	mDepth = inPixelDepth;

		// NewGWorld interprets the bounds in global coordinates
		// when specifying a zero pixel depth. It uses the maximum
		// depth of all screen devices intersected by the bounds.
	
	Rect	gwRect = inBounds;
	if (inPixelDepth == 0)
		{
		LocalToGlobal(&topLeft(gwRect));
		LocalToGlobal(&botRight(gwRect));
		}

	mGWorldStatus = ::NewGWorld(&mMacGWorld, inPixelDepth, &gwRect,
								inCTableH, inGDeviceH, inFlags);
						
	ThrowIfOSErr_(mGWorldStatus);
	ThrowIfNil_(mMacGWorld);
	
		// Set up coordinates and erase pixels in GWorld
	
	::SetGWorld(mMacGWorld, nil);
	::SetOrigin(mBounds.left, mBounds.top);
	::LockPixels(::GetGWorldPixMap(mMacGWorld));
	::EraseRect(&mBounds);
	::UnlockPixels(::GetGWorldPixMap(mMacGWorld));
	::SetGWorld(mSavePort, mSaveDevice);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CGWorld
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// This does not initialize the GWorld.  Must be overriden.
CGWorld::CGWorld()
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	~CGWorld
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CGWorld::~CGWorld()
{
	if (mMacGWorld != nil)
		::DisposeGWorld(mMacGWorld);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BeginDrawing
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CGWorld::BeginDrawing(void)
{
	Boolean bReadyToDraw = false;

		// The mac GWolrd will be inconsistent if a previous call to UpdateGWorld()
		// has failed.  We get one more shot a fixing that here before we draw.

	if (!IsConsistent())
		AdjustGWorld(mBounds);
	
	if (IsConsistent())
		{
		if (!::LockPixels(::GetGWorldPixMap(mMacGWorld)))
			{
			// This means we're consistentlty sized and oriented, but our pixMap
			// data has been purged.
			AdjustGWorld(mBounds);
			
			if (IsConsistent())
				bReadyToDraw = ::LockPixels(::GetGWorldPixMap(mMacGWorld));
			}
		else
			bReadyToDraw = true;
		}

	if (bReadyToDraw)
		{
		::GetGWorld(&mSavePort, &mSaveDevice);
		::SetGWorld(mMacGWorld, nil);
		}
		
	return bReadyToDraw;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	EndDrawing
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CGWorld::EndDrawing(void)
{
	Assert_(IsConsistent());			// You should have noticed that
										// BeginDrawing() returned false
										
	::UnlockPixels(::GetGWorldPixMap(mMacGWorld));
	::SetGWorld(mSavePort, mSaveDevice);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CopyImage
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CGWorld::CopyImage(
	GrafPtr 	inDestPort,
	const Rect&	inDestRect,
	Int16 		inXferMode,
	RgnHandle 	inMaskRegion)
{
	::CopyBits(&((GrafPtr)mMacGWorld)->portBits,
				&inDestPort->portBits,
				&mBounds, &inDestRect, inXferMode, inMaskRegion);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	SetBounds
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CGWorld::SetBounds(const Rect &inBounds)
{
	if (::EqualRect(&inBounds, &mBounds))
		return;
	
	UpdateBounds(inBounds);
		
		// Note that even if we're not consistent, we still remember the bounds so that
		// the next time through BeginDrawing() we can attempt to realloc for the
		// size it's supposed to be.
		
	mBounds = inBounds;		
		
		// We've reallocted successfully.  Now lets get the origin set properly.
		
	if (IsConsistent())
		UpdateOrigin(topLeft(mBounds));
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	AdjustGWorld
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CGWorld::AdjustGWorld(const Rect& inBounds)
{
	UpdateBounds(inBounds);
	if (IsConsistent())
		UpdateOrigin(topLeft(inBounds));
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	UpdateBounds
//
//	Internal method which is called when the offscreen world needs to change
//	it's dimensions.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CGWorld::UpdateBounds(const Rect& inBounds)
{
	Rect theNewBounds;

	if (mDepth == 0)
		{
		theNewBounds = inBounds;
		::LocalToGlobal(&topLeft(theNewBounds));
		::LocalToGlobal(&botRight(theNewBounds));
		}
	else
		theNewBounds = inBounds;
	
	GWorldFlags theFlags = ::UpdateGWorld(&mMacGWorld, mDepth, &theNewBounds, NULL, NULL, 0);
													
	if (theFlags & gwFlagErr)
		mGWorldStatus = ::QDError();
	else
		mGWorldStatus = noErr;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	UpdateOrigin
//
//	Internal method in which the origin on the offscreen world needs to be
//	offset to match the on-screen representation.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CGWorld::UpdateOrigin(const Point& inOrigin)
{
	::GetGWorld(&mSavePort, &mSaveDevice);
	::SetGWorld(mMacGWorld, nil);
	::SetOrigin(inOrigin.h, inOrigin.v);
	::SetGWorld(mSavePort, mSaveDevice);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CSharedWorld::CSharedWorld(
	const Rect& 	inFrame,
	Int16 			inDepth,
	GWorldFlags 	inFlags)
	:	CGWorld(inFrame, inDepth, inFlags)
{
}

