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

#include "CSharedPatternWorld.h"
#include "UGraphicGizmos.h"

#include <LArray.h>
#include <LArrayIterator.h>

LArray* CSharedPatternWorld::sPatterns = NULL;
Boolean CSharedPatternWorld::sUseUtilityPattern = false;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CreateSharedPatternWorld							[ static ]
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CSharedPatternWorld* CSharedPatternWorld::CreateSharedPatternWorld(ResIDT inPatternID)
{
	CSharedPatternWorld* thePatternWorld = NULL;
	
	if (sUseUtilityPattern)
		inPatternID = cUtilityPatternResID;
	
	try
		{
		if (sPatterns == NULL)
			sPatterns = new LArray(sizeof(CSharedPatternWorld*));
		
		CSharedPatternWorld* theIndexWorld = NULL;	
		LArrayIterator theIter(*sPatterns, LArrayIterator::from_Start);
		while (theIter.Next(&theIndexWorld))
			{
			if (theIndexWorld->mPatternID == inPatternID)
				{
				thePatternWorld = theIndexWorld;
				break;
				}
			}
		
		if (thePatternWorld == NULL)
			thePatternWorld = new CSharedPatternWorld(inPatternID);
		}
	catch (...)
		{
		delete thePatternWorld;
		thePatternWorld = NULL;		
		}

	return thePatternWorld;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcRelativePoint								[ static ]
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSharedPatternWorld::CalcRelativePoint(
	LPane*					inForPane,
	EPatternOrientation		inOrientation,
	Point& 					outPoint)
{
	// the downward scale from 32 to 16 bit coordinate spaces
	// in converting from SPoint32 to plain old Point
	// is ok because we are guaranteed to be in bounds since we
	// wont be drawing if we cant FocusExposed().

	switch (inOrientation)
		{
		case CSharedPatternWorld::eOrientation_Self:
			{
			SPoint32 theFrameLocation;
			inForPane->GetFrameLocation(theFrameLocation);
			outPoint.h = theFrameLocation.h;
			outPoint.v = theFrameLocation.v;
			inForPane->PortToLocalPoint(outPoint);
			}
			break;
			
		case CSharedPatternWorld::eOrientation_Superview:
			{
			SPoint32 theFrameLocation;
			inForPane->GetSuperView()->GetFrameLocation(theFrameLocation);
			outPoint.h = -theFrameLocation.h;
			outPoint.v = -theFrameLocation.v;
			inForPane->PortToLocalPoint(outPoint);
			}
			break;
			
		case CSharedPatternWorld::eOrientation_Port:
			{
			SPoint32 theFrameLocation;
			inForPane->GetFrameLocation(theFrameLocation);
			outPoint.h = -theFrameLocation.h;
			outPoint.v = -theFrameLocation.v;
			}
			break;
		}
}



// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CSharedPatternWorld
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CSharedPatternWorld::CSharedPatternWorld(ResIDT inPatternID)
{
	PixPatHandle thePattern = NULL;
	
	mPatternWorld = NULL;
	mPatternHandle = NULL;

	try
		{
		mPatternID = inPatternID;
		thePattern  = ::GetPixPat(inPatternID);
		ThrowIfResFail_(thePattern);
		
		// If our pix map is 8 bits or less deep we can avoid making an
		// offscreen world to store the pattern.
		PixMapHandle thePatPixMap = (*thePattern)->patMap;	
		mIsShallow = ((*thePatPixMap)->pixelSize <= 8);

		if (!IsShallow())
			{
			// We have a pattern that is deeper than 8 bits.  As documented
			// in the header file, QuickDraw will draw the pattern differently
			// in this case.  We need to create an offscreen copy of the pattern
			// so that we can draw it ourselves.
			
			// Get the pattern's rect from its pix map and normalize
			Rect thePatternFrame = (*thePatPixMap)->bounds;
			::OffsetRect(&thePatternFrame, -thePatternFrame.left, -thePatternFrame.top);
	
			// make an offscreen world the same size as the pattern
			mPatternWorld = new CGWorld(thePatternFrame, (*thePatPixMap)->pixelSize, 0, (*thePatPixMap)->pmTable);
	
			// and draw into it
			ThrowIfNot_(mPatternWorld->BeginDrawing());
			::FillCRect(&thePatternFrame, thePattern);
			mPatternWorld->EndDrawing();		

			// Ok, we've successfully copied the pattern so we can get
			// rid of the pattern handle.
			::DisposePixPat(thePattern);
			}
		else
			{
			// the pattern is 8 bits or less.  We'll just keep the pattern
			// handle around for drawing.
			mPatternHandle = thePattern;			
			}
		// add this to static shared pattern list
		Assert_(sPatterns);
		sPatterns->InsertItemsAt(1, LArray::index_Last, &this);
		}
	catch (...)
		{
		delete mPatternWorld;
		mPatternWorld = NULL;
		
		if (thePattern != NULL)
			::DisposePixPat(thePattern);
		
		throw;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	~CSharedPatternWorld
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CSharedPatternWorld::~CSharedPatternWorld()
{
	// remove this from static shared pattern list
	Assert_(sPatterns);
	sPatterns->Remove(&this);
	
	if (mPatternHandle != NULL)
		::DisposePixPat(mPatternHandle);

	delete mPatternWorld;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Fill 			(Rect)
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSharedPatternWorld::Fill(
	CGrafPtr				inPort,
	const Rect& 			inBounds,
	Point					inAlignTo)
{
	if (IsShallow())
		FillShallow(inPort, inBounds, inAlignTo);
	else
		FillDeep(inPort, inBounds, inAlignTo);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Fill			(Region)
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSharedPatternWorld::Fill(
	CGrafPtr				inPort,
	RgnHandle 				inRegion,
	Point					inAlignTo)
{
	StClipRgnState theClipSaver;
	::SetClip(inRegion);
	Rect theBounds = (*inRegion)->rgnBBox;
	if (IsShallow())
		FillShallow(inPort, theBounds, inAlignTo);
	else
		FillDeep(inPort, theBounds, inAlignTo);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	FillShallow
//
//	Here we fill a rect with a pattern that is 8 bits or less in depth.  See
//	the header file for why this is necessary.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSharedPatternWorld::FillShallow(
	CGrafPtr				inPort,
	const Rect& 			inBounds,
	Point					inAlignTo)
{
	StClipRgnState theClipSaver;
	theClipSaver.ClipToIntersection(inBounds);
	
	StPortOriginState theOriginSaver((GrafPtr)inPort);
	Point theOldOrigin = topLeft(inPort->portRect);
	
	Point theNewOrigin;
	theNewOrigin.h = theOldOrigin.h - inAlignTo.h;
	theNewOrigin.v = theOldOrigin.v - inAlignTo.v;

	Point theDelta;
	theDelta.h = theNewOrigin.h - theOldOrigin.h;
	theDelta.v = theNewOrigin.v - theOldOrigin.v; 
	
	StRegion theClip;
	::GetClip(theClip);
	::OffsetRgn(theClip, theDelta.h, theDelta.v);
	::SetClip(theClip);
	
	Rect theOffsetBounds = inBounds;
	::OffsetRect(&theOffsetBounds, theDelta.h, theDelta.v);
	
	::SetOrigin(theNewOrigin.h, theNewOrigin.v);
	::FillCRect(&theOffsetBounds, mPatternHandle);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	FillDeep
//
//	Here we fill a rect with a pattern that is deeper than 8 bits.  See
//	the header file for why this is necessary.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSharedPatternWorld::FillDeep(
	CGrafPtr				inPort,
	const Rect& 			inBounds,
	Point					inAlignTo)
{
	PixMapHandle thePatternMap = ::GetGWorldPixMap(mPatternWorld->GetMacGWorld());
	PixMapHandle theDestMap = inPort->portPixMap;

	StClipRgnState theClipSaver;
	theClipSaver.ClipToIntersection(inBounds);
	
	StRegion theClip;
	::GetClip(theClip);
	
	Rect thePatternFrame = (*thePatternMap)->bounds;
	Int16 thePatternWidth = RectWidth(thePatternFrame);
	Int16 thePatternHeight = RectHeight(thePatternFrame);
	
	Int32 theStampLeft = inAlignTo.h + thePatternWidth * ((inBounds.left - inAlignTo.h) / thePatternWidth);
	Int32 theStampTop = inAlignTo.v + thePatternHeight * ((inBounds.top - inAlignTo.v) / thePatternHeight);

	Rect theStamp;
	theStamp.top = theStampTop;
	theStamp.left = theStampLeft;
	theStamp.bottom = theStamp.top + thePatternHeight;
	theStamp.right = theStamp.left + thePatternWidth;
	
	while (theStamp.top <= inBounds.bottom)
		{
		while (theStamp.left <= inBounds.right)
			{
			if (::RectInRgn(&theStamp, theClip))
				::CopyBits((BitMap*)(*thePatternMap), (BitMap*)(*theDestMap), &thePatternFrame, &theStamp, srcCopy, NULL);
				
			theStamp.left += thePatternWidth;
			theStamp.right += thePatternWidth;
			}
	
		theStamp.left = theStampLeft;
		theStamp.top += thePatternHeight;
		theStamp.bottom = theStamp.top + thePatternHeight;
		theStamp.right = theStamp.left + thePatternWidth;
		}
}

