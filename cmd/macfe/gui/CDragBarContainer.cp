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
//	CDragBarContainer.cp
//
//
//	NOTE: It is assumed that the first level sub views of this container are
//	either CDragBar's or the CDragBarDockControl.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CDragBar.h"
#include "CDragBarContainer.h"
#include "CDragBarDockControl.h"
#include "CDragBarDragTask.h"
#include "UGraphicGizmos.h"

#include <UMemoryMgr.h>
#include <LArray.h>
#include <LArrayIterator.h>
#include <Drag.h>

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDragBarContainer::CDragBarContainer(LStream* inStream)
	:	CBrokeredView(inStream),
		LDropArea(GetMacPort()),
		mBars(sizeof(CDragBar*))
{
	*inStream >> mBarListResID;
	mDock = NULL;
	
	SetRefreshAllWhenResized(false);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDragBarContainer::~CDragBarContainer()
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::FinishCreateSelf(void)
{
	CBrokeredView::FinishCreateSelf();

	// We need to get the dock first so that when we initialize the initial 
	// state of the bars we have somewhere to put them.
	mDock = dynamic_cast<CDragBarDockControl*>(FindPaneByID(CDragBarDockControl::class_ID));
	Assert_(mDock != NULL);
	mDock->AddListener(this);

	StResource	theIDList('RidL', mBarListResID);
	{
		StHandleLocker theLock(theIDList);
		LDataStream theBarStream(*theIDList.mResourceH, ::GetHandleSize(theIDList));
		
		Int16 theItemCount;
		theBarStream >> theItemCount;	
		for (Int16 i = 0; i < theItemCount; i++)
			{
			PaneIDT thePaneID;
			theBarStream >> thePaneID;
			
			CDragBar* theBar = dynamic_cast<CDragBar*>(FindPaneByID(thePaneID));
			Assert_(theBar != NULL);
			
			if (theBar->IsDocked())
				mDock->AddBarToDock(theBar);

			mBars.InsertItemsAt(1, 	LArray::index_Last, &theBar);
			theBar->AddListener(this);
			}
	}
	
	AdjustContainer();
	AdjustDock();
	
	// Because new toolbar drag bars could adjust their frames
	// at FinishCreateSelf time, call RepositionBars
	RepositionBars();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::ListenToMessage(
	MessageT			inMessage,
	void*				ioParam)
{
	if (inMessage == msg_DragBarCollapse)
		{
		CDragBar* theBar = (CDragBar*)ioParam;		
		NoteCollapseBar(theBar);
		}
	else if (inMessage == msg_DragBarExpand)
		{
		CDragBar* theBar = (CDragBar*)ioParam;		
		NoteExpandBar(theBar);
		}
	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::SavePlace(LStream *outPlace)
{
	*outPlace << mBars.GetCount();

	CDragBar* theBar;
	LArrayIterator theIter(mBars, LArrayIterator::from_Start);
	while (theIter.Next(&theBar))
		{
		*outPlace << theBar->GetPaneID();
		*outPlace << theBar->IsDocked();
		*outPlace << theBar->IsAvailable();
		}
		
	mDock->SavePlace(outPlace);

}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::RestorePlace(LStream *inPlace)
{
	Int32 theBarCount;
	*inPlace >> theBarCount;
	for (Int32 theIndex = LArray::index_First; theIndex <= theBarCount; theIndex++)
	{
		PaneIDT thePaneID;
		*inPlace >> thePaneID;
		
		Boolean bIsDocked;
		*inPlace >> bIsDocked;
		
		Boolean bIsAvailable;
		*inPlace >> bIsAvailable;
		
		CDragBar* theBar = dynamic_cast<CDragBar*>(FindPaneByID(thePaneID));
		Assert_(theBar != NULL);
	
		// First we want to place the bar in the same position in the
		// bar array as it was before.  We can tell that by its position
		// from when it was saved out.	
		ArrayIndexT theBarIndex = mBars.FetchIndexOf(&theBar);
		Assert_(theBarIndex != LArray::index_Bad);
		mBars.MoveItem(theBarIndex, theIndex);
		
		// Now we need to sync the state of the bar properly.
		if (bIsDocked && !theBar->IsDocked())
			mDock->AddBarToDock(theBar);
		if (bIsAvailable)
			theBar->Show();
		else
			theBar->Hide();
		theBar->SetAvailable(bIsAvailable);
	}
	

	mDock->RestorePlace(inPlace);

	RepositionBars();
	AdjustContainer();
	AdjustDock();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Hides the drag bar from the container AND dock without changing the docking
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CDragBarContainer::HideBar(CDragBar* inBar, Boolean inRefresh)
{ // hide drag bar
	if (inBar->IsAvailable())
	{
		if (!inBar->IsDocked()) inBar->Hide(); // hide if undocked
		mDock->HideBar(inBar);
		inBar->SetAvailable(false);
		
		RepositionBars();
		AdjustContainer();
		AdjustDock();
		if (inRefresh) Refresh();
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Show the drag bar without changing the docking
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CDragBarContainer::ShowBar(CDragBar* inBar, Boolean inRefresh)
{
	if (!inBar->IsAvailable())
	{
		if (!inBar->IsDocked()) inBar->Show(); // show if undocked
		mDock->ShowBar(inBar);
		inBar->SetAvailable(true);

		RepositionBars();
		AdjustContainer();
		AdjustDock();
		if (inRefresh) Refresh();
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::NoteCollapseBar(CDragBar* inBar)
{
	FocusDraw();

	// 3/6/97 pkc
	// When inBar is NULL, we just want to adjust things, but we
	// don't need to add a bar to the dock
	if (inBar)
		mDock->AddBarToDock(inBar);

	RepositionBars();
	AdjustContainer();
	AdjustDock();
	Refresh();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::NoteExpandBar(CDragBar* inBar)
{
	FocusDraw();

	mDock->RemoveBarFromDock(inBar);
	AdjustDock(); // show or hide as appropriate
	
	RepositionBars();
	AdjustContainer();
	Refresh();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::RepositionBars(void)
{
	Rect theContainerFrame;
	CalcLocalFrameRect(theContainerFrame);

	Int32 theHeight = theContainerFrame.top;
	CDragBar* theBar;
	LArrayIterator theIter(mBars, LArrayIterator::from_Start);
	while (theIter.Next(&theBar))
		{
		if (theBar->IsDocked())		// Docked bars are ignored
			continue;
		if (!theBar->IsAvailable())    // Unavailable (hidden from container and dock)
			continue;

		SDimension16 theBarSize;
		theBar->GetFrameSize(theBarSize);
		theBar->PlaceInSuperFrameAt(theContainerFrame.left, theHeight, false);

		theHeight += theBarSize.height;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::SwapBars(
	CDragBar* 			inSourceBar, 
	CDragBar*			inDestBar,
	Boolean				inRefresh)
{
	if (inSourceBar == inDestBar)
		return;
	
	// Get the indeces and swap the bars in the array	
	ArrayIndexT theSourceIndex = mBars.FetchIndexOf(&inSourceBar);
	Assert_(theSourceIndex != LArray::index_Bad);
	
	ArrayIndexT theDestIndex = mBars.FetchIndexOf(&inDestBar);
	Assert_(theDestIndex != LArray::index_Bad);

	mBars.SwapItems(theSourceIndex, theDestIndex);

	// align the indeces so we can loop up from one to another.
	if (theSourceIndex > theDestIndex)
		{
		ArrayIndexT theTemp = theSourceIndex;
		theSourceIndex = theDestIndex;
		theDestIndex = theTemp;
		}

	// Now we need to invalidate all of the bars between, and including,
	// the source and destination.
	
	FocusDraw();
	while (theSourceIndex <= theDestIndex)
		{
		CDragBar* theBar = NULL;
		mBars.FetchItemAt(theSourceIndex, &theBar);
		Assert_(theBar != NULL);
		
		Rect thePortFrame;
		theBar->CalcPortFrameRect(thePortFrame);
		theBar->InvalPortRect(&thePortFrame);

		theSourceIndex++;
		}
		
	if (inRefresh)
		{
		RepositionBars();
		UpdatePort();		// expensive call!!! use sparingly
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::AdjustContainer(void)
{
	// First get our frame size, we need the width.
	SDimension16 theNewSize;
	GetFrameSize(theNewSize);

	// The height is the sum of the non-docked bars
	// If all bars are docked, the size is the height of the dock
	theNewSize.height = 0;
		
	CDragBar* theBar;
	LArrayIterator theIter(mBars, LArrayIterator::from_Start);
	while (theIter.Next(&theBar))
		{
		if (theBar->IsDocked())	// Docked bars are ignored.
			continue;
		if (!theBar->IsAvailable())    // Unavailable (hidden from container and dock)
			continue;
	
		SDimension16 theBarSize;
		theBar->GetFrameSize(theBarSize);
		theNewSize.height += theBarSize.height;
		}

	SDimension16 theDockSize;
	mDock->GetFrameSize(theDockSize);

	if (mDock->HasDockedBars())
		theNewSize.height += theDockSize.height;
		
	ResizeFrameTo(theNewSize.width, theNewSize.height, true);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDragBarContainer::AdjustDock(void)
{
	if (mDock->HasDockedBars())
		mDock->Show();
	else
		mDock->Hide();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- DROP AREA BEHAVIOUR ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	PointInDropArea
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Return whether a Point, in Global coords, is inside a DropArea

Boolean CDragBarContainer::PointInDropArea(
	Point	inPoint)
{
	GlobalToPortPoint(inPoint);
	return IsHitBy(inPoint.h, inPoint.v);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CDragBarContainer::ItemIsAcceptable(
	DragReference		inDragRef,
	ItemReference		inItemRef)
{
	Boolean bIsAcceptable = false;
	
	FlavorFlags	theFlags;
	OSErr theErr = ::GetFlavorFlags(inDragRef, inItemRef, Flavor_DragBar, &theFlags);
	if (theErr == noErr)
		{
		DragAttributes theAttributes;
		theErr = ::GetDragAttributes(inDragRef, &theAttributes);
		if (theErr == noErr)
			bIsAcceptable = (theAttributes & kDragInsideSenderWindow) != 0;
		}
	
	return bIsAcceptable;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	EnterDropArea
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	A Drag is entering a DropArea. This call will be followed by a
//	corresponding LeaveDropArea call (when the Drag moves out of the
//	DropArea or after the Drag is received by this DropArea).
//
//	If the DropArea can accept the Drag and the Drag is coming from outside
//	the DropArea, hilite the DropArea

void CDragBarContainer::EnterDropArea(
	DragReference	/* inDragRef */,
	Boolean			/* inDragHasLeftSender */)
{
	// NULL IMPLEMENTATION
	// No hiliting necessary.
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	LeaveDropArea
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	A Drag is leaving a DropArea. This call will have been preceded by
//	a corresponding EnterDropArea call.
//
//	Remove hiliting of the DropArea if necessary

void CDragBarContainer::LeaveDropArea(
	DragReference	/* inDragRef */)
{
	// No hiliting necessary.
	mCanAcceptCurrentDrag = false;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	InsideDropArea
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Track a Drag while it is inside a DropArea. This function is called
//	repeatedly while an acceptable Drag is inside a DropArea.
//
// 	This more difficult than it needs to be, because  PowerPlant does not pass the refCon
//	through to this routine.  If it did, I could easily get at the tracking bar.
//	Instead I need to go through the drag item reference number to get the
//	task, and then get the tracking bar.

void CDragBarContainer::InsideDropArea(DragReference inDragRef)
{
	FocusDraw();
	
	Point theMouse, thePinnedMouse;
	OSErr theErr = ::GetDragMouse(inDragRef, &theMouse, &thePinnedMouse);
	if (theErr != noErr)
	{
		return;
	}

	GlobalToPortPoint(theMouse);
	LPane* thePane = FindShallowSubPaneContaining(theMouse.h, theMouse.v);
	
	// We have the special knowledge that the top level sub panes
	// can only be other drag bars, or the dock.  

	CDragBar* theBar = dynamic_cast<CDragBar*>(thePane);
	if (theBar != NULL)
		{
		CDragBarDragTask* theTask = NULL;
		theErr = ::GetDragItemReferenceNumber(inDragRef, 1, (ItemReference*)(&theTask));
		if (theErr != noErr)
		{
			return;
		}


		// See if we're over a bar, and it's not the one we're dragging
		CDragBar* theTrackingBar = theTask->GetTrackingBar();
		if (theBar != theTrackingBar)
			{
			ArrayIndexT theBarPosition;
			theBarPosition = mBars.FetchIndexOf(&theBar);
			
			ArrayIndexT theTrackPosition;
			theTrackPosition = mBars.FetchIndexOf(&theTrackingBar);
			
			// OK, we have the indeces which correspond to the bars' top to
			// bottom ordering on the screen.  To prevent violent swapping of the bars,
			// we only want to swap if the point has passed through the mid horizontal
			// point of the bar in the direction leading away from the current tracking
			// bar.
			
			Rect theBarPortFrame;
			theBar->CalcPortFrameRect(theBarPortFrame);

			Int16 theMidLine = theBarPortFrame.top + (RectHeight(theBarPortFrame) / 2);

			Boolean bShouldSwap;
			if ((theBarPosition < theTrackPosition) && (theMouse.v <= theMidLine))
				bShouldSwap = true;
			else if ((theBarPosition > theTrackPosition) && (theMouse.v >= theMidLine))
				bShouldSwap = true;
			else
				bShouldSwap = false;

			if (bShouldSwap)			
				SwapBars(theTrackingBar, theBar, true);
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ReceiveDragItem
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Process an Item which has been dragged into a DropArea
//
//	This function gets called once for each Item contained in a completed
//	Drag. The Item will have returned true from ItemIsAcceptable().
//
//	The DropArea is focused upon entry and inItemBounds is specified
//	in the local coordinates of the DropArea.

void CDragBarContainer::ReceiveDragItem(
	DragReference	/* inDragRef */,
	DragAttributes	/* inDragAttrs */,
	ItemReference	/* inItemRef */,
	Rect&			/* inItemBounds */)	// In Local coordinates
{
}






