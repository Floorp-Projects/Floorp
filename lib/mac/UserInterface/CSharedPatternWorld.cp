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

