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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDragBarDockControl.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
#include "CDragBarDockControl.h"


#include "CDragBar.h"
#include "CSharedPatternWorld.h"
#include "UGraphicGizmos.h"

#include <LArray.h>
#include <LArrayIterator.h>
#include <UTextTraits.h>
#include <LString.h>

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDragBarDockControl::CDragBarDockControl(LStream* inStream)
	:	LControl(inStream),
		mBars(sizeof(CDragBar*))
{
	mNeedsRecalc = true;
	mMouseInside = nil;
	
	// ¥ÊThese constants should be in the PPob....
	
	ResIDT thePatternID;
	*inStream >> thePatternID;
	mBackPattern = CSharedPatternWorld::CreateSharedPatternWorld(thePatternID);
	ThrowIfNULL_(mBackPattern);
	mBackPattern->AddUser(this);
	mBackPatternHilite = CSharedPatternWorld::CreateSharedPatternWorld(10001);
	ThrowIfNULL_(mBackPatternHilite);
	mBackPatternHilite->AddUser(this);

	mGrippy = CSharedPatternWorld::CreateSharedPatternWorld(10004);
	ThrowIfNULL_(mGrippy);
	mGrippy->AddUser(this);
	mGrippyHilite = CSharedPatternWorld::CreateSharedPatternWorld(10005);
	ThrowIfNULL_(mGrippyHilite);
	mGrippyHilite->AddUser(this);

	*inStream >> mBarTraitsID;

	mTriangle = ::GetCIcon(10000);
	ThrowIfNULL_(mTriangle);

	SetRefreshAllWhenResized(false);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDragBarDockControl::~CDragBarDockControl()
{
	mBackPattern->RemoveUser(this);
	mBackPatternHilite->RemoveUser(this);
	mGrippy->RemoveUser(this);
	mGrippyHilite->RemoveUser(this);
	
	::DisposeCIcon(mTriangle);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::SetNeedsRecalc(Boolean inRecalc)
{
	mNeedsRecalc = inRecalc;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CDragBarDockControl::IsRecalcRequired(void) const
{
	return mNeedsRecalc;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::AddBarToDock(CDragBar* inBar)
{
	inBar->Dock();
	mBars.InsertItemsAt(1, LArray::index_First, &inBar);
	SetNeedsRecalc(true);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::RemoveBarFromDock(CDragBar* inBar)
{
	inBar->Undock();
	mBars.Remove(&inBar);
	SetNeedsRecalc(true);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CDragBarDockControl::HasDockedBars(void) const
{
	return (mBars.GetCount() > 0);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::RecalcDock(void)
{
	FocusDraw();
	UTextTraits::SetPortTextTraits(mBarTraitsID);

	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	Int16 theLeftIndex = theFrame.left;
	
	CDragBar* theBar;
	LArrayIterator theIter(mBars, LArrayIterator::from_Start);
	while (theIter.Next(&theBar))
		{
		SDimension16 theBarSize;
		theBar->GetFrameSize(theBarSize);
		
		Rect theBarFrame;
		theBarFrame.left = theLeftIndex;
		theBarFrame.top = theFrame.top;
		theBarFrame.right = theBarFrame.left + theBarSize.height + 12; 
		theBarFrame.bottom = theFrame.bottom;
		CalcOneDockedBar(theBar, theBarFrame);

		theLeftIndex += RectWidth(theBarFrame);
		}	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::CalcOneDockedBar(
	CDragBar*		inBar,
	const Rect&		inBounds)
{
	Int16 theHeight = RectHeight(inBounds);
	
	::OpenRgn();
	::MoveTo(inBounds.left, inBounds.top);
	::LineTo(inBounds.right, inBounds.top);
	::LineTo(inBounds.right - theHeight, inBounds.bottom);
	
	if (mBars.FetchIndexOf(&inBar) == LArray::index_First)
		::LineTo(inBounds.left, inBounds.bottom);
	else
		::LineTo(inBounds.left - theHeight, inBounds.bottom);
		
	::LineTo(inBounds.left, inBounds.top);
	::CloseRgn(inBar->mDockedMask);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::DrawSelf(void)
{
	if (mBars.GetCount() == 0)
		{
		Assert_(false);		// we should have been hidden and not drawn
		return;
		}
		
	if (IsRecalcRequired())
		RecalcDock();

	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	StClipRgnState theClipSaver(theFrame);

	// FIXME the port relative calculation works when you're a view and not a pane
	Point theAlignment;
	//	CSharedPatternWorld::CalcRelativePoint(this, CSharedPatternWorld::eOrientation_Port, theAlignment);
	mSuperView->GetPortOrigin(theAlignment);

	// We do this instead of LPane::GetMacPort() because we may be being
	// drawn offscreen.
	CGrafPtr thePort;
	::GetPort(&(GrafPtr)thePort);

	mBackPattern->Fill(thePort, theFrame, theAlignment);
	UGraphicGizmos::LowerColorVolume(theFrame, 0x2000);
	
	CDragBar* theBar;
	LArrayIterator theIter(mBars, LArrayIterator::from_Start);
	while (theIter.Next(&theBar))
		DrawOneDockedBar(theBar);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


void CDragBarDockControl::DrawOneDockedBar(
	CDragBar* 			inBar)
{
	StClipRgnState theClipSaver(inBar->mDockedMask);
	Rect theTabFrame = (**static_cast<RgnHandle>(inBar->mDockedMask)).rgnBBox;
	Int16 theHeight = RectHeight(theTabFrame);
	Int16 theHalfHeight = theHeight / 2;

	// FIXME the port relative calculation works when you're a view and not a pane
	Point theAlignment;
	//	CSharedPatternWorld::CalcRelativePoint(this, CSharedPatternWorld::eOrientation_Port, theAlignment);
	mSuperView->GetPortOrigin(theAlignment);

	CGrafPtr thePort;
	::GetPort(&(GrafPtr)thePort);
	if ( mMouseInside == inBar )
		mBackPatternHilite->Fill(thePort, theTabFrame, theAlignment);
	else
		mBackPattern->Fill(thePort, theTabFrame, theAlignment);

		// Draw the beveled outline hilighting
	RGBColor theTintColor = { 0x5000, 0x5000, 0x5000 };
	::RGBForeColor(&theTintColor);
	::OpColor(&UGraphicGizmos::sLighter);
	::PenMode(addPin);

	ArrayIndexT theIndex = mBars.FetchIndexOf(&inBar);
	::MoveTo(theTabFrame.left, theTabFrame.bottom - 1);
	if (theIndex == LArray::index_First)
		::LineTo(theTabFrame.left, theTabFrame.top);
	else
		::LineTo(theTabFrame.left + theHeight, theTabFrame.top);

	::LineTo(theTabFrame.right - 1, theTabFrame.top);
	
	::OpColor(&UGraphicGizmos::sDarker);
	::PenMode(subPin);

	::LineTo(theTabFrame.right - theHeight - 1, theTabFrame.bottom - 1);
	::LineTo(theTabFrame.left, theTabFrame.bottom - 1);

		// Draw the triangle
	Rect theTriangleDest = theTabFrame;
	if (theIndex == LArray::index_First)
		theTriangleDest.left += theHalfHeight;
	else
		theTriangleDest.left += theHeight;
	theTriangleDest.right = theTriangleDest.left + (theHeight / 2);

	Rect theTriangleFrame = (**mTriangle).iconPMap.bounds;
	UGraphicGizmos::CenterRectOnRect(theTriangleFrame, theTriangleDest);
	::PlotCIcon(&theTriangleFrame, mTriangle);

		// Inset the tab shape then knock out the area surrounding the triangle.
	StRegion theInsetCopy(inBar->mDockedMask);
	::InsetRgn(theInsetCopy, 2, 2);

	theTriangleDest.left = theTabFrame.left;
	theTriangleDest.right += theHalfHeight;
	StRegion theTriangleMask(theTriangleDest);
	::DiffRgn(theInsetCopy, theTriangleMask, theInsetCopy);
	::SetClip(theInsetCopy);
	
		// draw the grippy pattern
	Rect thePatternFrame = (**static_cast<RgnHandle>(theInsetCopy)).rgnBBox;
	if ( mMouseInside == inBar )
		mGrippyHilite->Fill(thePort, thePatternFrame, theAlignment);
	else
		mGrippy->Fill(thePort, thePatternFrame, theAlignment);

}



// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::ResizeFrameBy(
	Int16 			inWidthDelta,
	Int16			inHeightDelta,
	Boolean			inRefresh)
{
	LControl::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	SetNeedsRecalc(true);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Hides the bar from the dock if docked - mjc
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CDragBarDockControl::HideBar(CDragBar* inBar)
{
	if (inBar->IsDocked() && inBar->IsAvailable())
	{
		mBars.Remove(&inBar);
		SetNeedsRecalc(true);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Shows the bar in the dock if it was docked previously (use with HideBar) - mjc
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CDragBarDockControl::ShowBar(CDragBar* inBar)
{
	if (inBar->IsDocked()  && !inBar->IsAvailable())
	{
		mBars.InsertItemsAt(1, LArray::index_First, &inBar);
		SetNeedsRecalc(true);
	}
}


//
// MouseWithin
//
// Called by powerPlant when the mouse is inside the control. Find which collapsed bar the mouse
// is hovering over and if it is different from the one that is currently hilighted, 
// redraw.
//
void 
CDragBarDockControl :: MouseWithin ( Point inPoint, const EventRecord & /* inEvent */) 
{
	PortToLocalPoint(inPoint);
	const CDragBar* curr = FindBarSpot ( inPoint );
	if ( mMouseInside != curr ) {
		mMouseInside = curr;
		Refresh();
		ExecuteAttachments(msg_HideTooltip, nil);
	}
}


//
// MouseLeave
//
// Called by powerplant when the mouse leaves the control. No more hilighting, so forget
// that any bar is selected and redraw back to the normal state
//
void 
CDragBarDockControl :: MouseLeave ( )
{
	if ( mMouseInside ) {
		mMouseInside = nil;
		Refresh();
	}
}


//
// 
void
CDragBarDockControl :: FindTooltipForMouseLocation ( const EventRecord& inMacEvent, StringPtr outTip )
{	
	Point where = inMacEvent.where;
	GlobalToPortPoint ( where );
	PortToLocalPoint ( where );
	const CDragBar* curr = FindBarSpot ( where );
	if ( curr )
		curr->GetDescriptor(outTip);
	else
		::GetIndString ( outTip, 10506, 13 );		// supply a helpful message...

} // FindTooltipForMouseLocation


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CONTROL BEHAVIOUR ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 CDragBarDockControl::FindHotSpot(Point inPoint)
{
	Int16 theHotSpot = 0;
	CDragBar* theBar;
	LArrayIterator theIter(mBars, LArrayIterator::from_Start);
	while (theIter.Next(&theBar))
		{			
		if (::PtInRgn(inPoint, theBar->mDockedMask))
			{
			theHotSpot = theIter.GetCurrentIndex();
			break;
			}
		}
	
	return theHotSpot;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CDragBarDockControl::PointInHotSpot(Point inPoint, Int16 inHotSpot)
{
	CDragBar* theBar = NULL;
	mBars.FetchItemAt(inHotSpot, &theBar);
	Assert_(theBar != NULL);	

	Boolean bInHotSpot = ::PtInRgn(inPoint, theBar->mDockedMask);
	return bInHotSpot;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::HotSpotAction(
	Int16				/* inHotSpot */,
	Boolean 			/* inCurrInside */,
	Boolean				/* inPrevInside */)
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
									
void CDragBarDockControl::HotSpotResult(Int16 inHotSpot)
{
	CDragBar* theBar = NULL;
	mBars.FetchItemAt(inHotSpot, &theBar);
	Assert_(theBar != NULL);
	
	BroadcastMessage(msg_DragBarExpand, theBar);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::SavePlace(LStream *outPlace)
{
	*outPlace << mBars.GetCount();

	CDragBar* theBar;
	LArrayIterator theIter(mBars, LArrayIterator::from_Start);
	while (theIter.Next(&theBar))
		*outPlace << theBar->GetPaneID();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarDockControl::RestorePlace(LStream *inPlace)
{
	Int32 theBarCount;
	*inPlace >> theBarCount;
	
	for (Int32 theIndex = LArray::index_First; theIndex <= theBarCount; theIndex++)
		{
		PaneIDT theBarID;
		*inPlace >> theBarID;
		
		CDragBar* theBar;
		LArrayIterator theIter(mBars, LArrayIterator::from_Start);
		while (theIter.Next(&theBar))
			{
			if (theBar->GetPaneID() == theBarID)
				mBars.MoveItem(theIter.GetCurrentIndex(), theIndex);				
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

const CDragBar* CDragBarDockControl::FindBarSpot(
	Point 				inLocalPoint)
{
	const CDragBar* theBar = NULL;
	Int16 theHotSpot = FindHotSpot(inLocalPoint);
	if (theHotSpot != 0)
		mBars.FetchItemAt(theHotSpot, &theBar);

	return theBar;
}
