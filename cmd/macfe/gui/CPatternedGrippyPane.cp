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
// CPatternedGrippyPane.cp
//
// Implementation for class that draws a "grippy" pattern in the pane rectangle so users know
// they can drag/click in this area. Also hilights when mouse enters (roll-over feedback).
//
// I didn't actually write this class, just moved it out of DragBar.h.
// 


#include "CPatternedGrippyPane.h"
#include "CSharedPatternWorld.h"
#include "UGraphicGizmos.h"


CPatternedGrippyPane::CPatternedGrippyPane(LStream* inStream)
	:	LPane(inStream)
{
	ResIDT theBackgroundID;	
	*inStream >> theBackgroundID;
	ResIDT theBackgroundHiliteID;	
	*inStream >> theBackgroundHiliteID;
	
	ResIDT theTopPatternID;					// read in but ignore...unused...
	*inStream >> theTopPatternID;
	
	Int16 theTopPatternIndex;				// read in but ignore...unused...
	*inStream >> theTopPatternIndex;
	
	ResIDT theBottomPatternID;				// read in but ignore...unused...
	*inStream >> theBottomPatternID;
	
	Int16 theBottomPatternIndex;			// read in but ignore...unused...
	*inStream >> theBottomPatternIndex;

	mBackPattern = CSharedPatternWorld::CreateSharedPatternWorld(theBackgroundID);
	ThrowIfNULL_(mBackPattern);
	mBackPattern->AddUser(this);
	mBackPatternHilite = CSharedPatternWorld::CreateSharedPatternWorld(theBackgroundHiliteID);
	ThrowIfNULL_(mBackPatternHilite);
	mBackPatternHilite->AddUser(this);

	mGrippy = CSharedPatternWorld::CreateSharedPatternWorld(10002);
	ThrowIfNULL_(mGrippy);
	mGrippy->AddUser(this);
	mGrippyHilite = CSharedPatternWorld::CreateSharedPatternWorld(10003);
	ThrowIfNULL_(mGrippyHilite);
	mGrippyHilite->AddUser(this);

	mTriangle = ::GetCIcon(10001);
	ThrowIfNULL_(mTriangle);

	mMouseInside = false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CPatternedGrippyPane::~CPatternedGrippyPane()
{
	mGrippy->RemoveUser(this);
	mGrippyHilite->RemoveUser(this);
	mBackPattern->RemoveUser(this);
	mBackPatternHilite->RemoveUser(this);
	::DisposeCIcon(mTriangle);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CPatternedGrippyPane::DrawSelf(void)
{
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	StClipRgnState theClipSaver(theFrame);

	Point theAlignment;
	CSharedPatternWorld::CalcRelativePoint(this, CSharedPatternWorld::eOrientation_Self, theAlignment);

		// We do this instead of LPane::GetMacPort()
		// because we may be being drawn offscreen.
	CGrafPtr thePort;
	::GetPort(&(GrafPtr)thePort);
	if ( mMouseInside )
		mBackPatternHilite->Fill(thePort, theFrame, theAlignment);
	else
		mBackPattern->Fill(thePort, theFrame, theAlignment);
	
	UGraphicGizmos::BevelTintRect(theFrame, 1, 0x4000, 0x4000);

	Rect theTriangleFrame = (**mTriangle).iconPMap.bounds;
	Rect theTriangleDest = theFrame;
	theTriangleDest.bottom = theTriangleDest.top + RectWidth(theFrame);
	
	UGraphicGizmos::CenterRectOnRect(theTriangleFrame, theTriangleDest);
	::PlotCIcon(&theTriangleFrame, mTriangle);

	Rect thePatternDest = theFrame;
	::InsetRect(&thePatternDest, 2, 2);
	thePatternDest.top = theTriangleDest.bottom - 1;	
	::ClipRect(&thePatternDest);
 	
 	// hack to make it look centered for both drag bars and expand/collapse widget
 	if ( thePatternDest.right - thePatternDest.left > 5 )
 		theAlignment.h++;
 	
	if ( mMouseInside )
		mGrippyHilite->Fill(thePort, thePatternDest, theAlignment);
	else
		mGrippy->Fill(thePort, thePatternDest, theAlignment);
		
//	UGraphicGizmos::DrawArithPattern(theFrame, mBottomShadePat, 0x6000, false);
}


void 
CPatternedGrippyPane :: MouseEnter ( Point /* inPoint */, const EventRecord & /* inEvent */ ) 
{
	mMouseInside = true;
	Refresh();
}

void 
CPatternedGrippyPane :: MouseLeave ( )
{
	mMouseInside = false;
	Refresh();
}


