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
//	CSwatchBrokerView.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CSwatchBrokerView.h"
#include "CBevelView.h"


CBrokeredView::CBrokeredView(LStream* inStream)
	:	LView(inStream)
{
}

void CBrokeredView::GetPendingFrameSize(SDimension16& outSize) const
{
	outSize = mPendingSize;
}

void CBrokeredView::GetPendingFrameLocation(SPoint32& outLocation) const
{
	outLocation = mPendingLocation;
}

void CBrokeredView::ResizeFrameBy(
	Int16 				inWidthDelta,
	Int16 				inHeightDelta,
	Boolean 			inRefresh)
{
	if (inRefresh)
		{
		GetFrameSize(mPendingSize);
		mPendingSize.width += inWidthDelta;
		mPendingSize.height += inHeightDelta;
		BroadcastMessage(msg_BrokeredViewAboutToChangeSize, this);
		}
		
	LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	
	if (!inRefresh)
		BroadcastMessage(msg_BrokeredViewChangeSize, this);
}

void CBrokeredView::MoveBy(
	Int32				inHorizDelta,
	Int32 				inVertDelta,
	Boolean 			inRefresh)
{
	if (inRefresh)
		{
		GetFrameLocation(mPendingLocation);
		mPendingLocation.h += inHorizDelta;
		mPendingLocation.v += inVertDelta;
		BroadcastMessage(msg_BrokeredViewAboutToMove, this);
		}
		
	LView::MoveBy(inHorizDelta, inVertDelta, inRefresh);
	
	if (!inRefresh)
		BroadcastMessage(msg_BrokeredViewMoved, this);
}

CSwatchBrokerView::CSwatchBrokerView(LStream* inStream)
	:	LView(inStream)
{
	*inStream >> mDynamicPaneID;
	*inStream >> mBalancePaneID;
	SetRefreshAllWhenResized(false);
}


void CSwatchBrokerView::FinishCreateSelf(void)
{
	LView::FinishCreateSelf();

	mBalancePane = FindPaneByID(mBalancePaneID);
	Assert_(mBalancePane != NULL);
	
	mDynamicPane = dynamic_cast<CBrokeredView*>(FindPaneByID(mDynamicPaneID));
	if (mDynamicPane)
	{
		mDynamicPane->AddListener(this);

		// We need to "prime to pump".  The dynamic view may have resized itself
		// in it's FinishCreateSelf(), at which point we were not listening.
		SDimension16 theSize;
		mDynamicPane->GetFrameSize(theSize);
		AdaptToBrokeredViewResize(mDynamicPane, theSize, true);
	}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// on adapt to super frame size we ignore broadcasts from the subs
// so that we dont recalculate too often.  we really want to know when
// a sub changes height so we can reposition and resize the others.

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSwatchBrokerView::AdaptToSuperFrameSize(
	Int32				inSurrWidthDelta,
	Int32				inSurrHeightDelta,
	Boolean				inRefresh)
{
	StopListening();
	LView::AdaptToSuperFrameSize(inSurrWidthDelta, inSurrHeightDelta, inRefresh);
	StartListening();
}


void CSwatchBrokerView::ListenToMessage(
	MessageT			inMessage,
	void*				ioParam)
{
	CBrokeredView* theView = (CBrokeredView*)ioParam;

	switch (inMessage)
		{
		case msg_BrokeredViewAboutToChangeSize:
			{
			SDimension16 thePendingSize;
			theView->GetPendingFrameSize(thePendingSize);
			AdaptToBrokeredViewResize(theView, thePendingSize, true);
			}
			break;
			
		case msg_BrokeredViewChangeSize:
			{
			SDimension16 theNewSize;
			theView->GetFrameSize(theNewSize);
			AdaptToBrokeredViewResize(theView, theNewSize, false);
			}
			break;
			
		case msg_BrokeredViewAboutToMove:
			{
			SPoint32 thePendingLocation;
			theView->GetPendingFrameLocation(thePendingLocation);
			AdaptToBrokeredViewMove(theView, thePendingLocation, true);
			}
			break;
		
		case msg_BrokeredViewMoved:
			{
			SPoint32 theNewLocation;
			theView->GetFrameLocation(theNewLocation);
			AdaptToBrokeredViewMove(theView, theNewLocation, false);
			}
			break;
		}

}

// this whole thing needs to be way more robust.  multiple brokered views,
// top to bottom ordering, etc.

void CSwatchBrokerView::AdaptToBrokeredViewResize(
	CBrokeredView*		inView,
	const SDimension16	inNewSize,
	Boolean				inRefresh)
{
	SDimension16 theBrokerSize;
	GetFrameSize(theBrokerSize);

	SDimension16 theBalanceSize;
	mBalancePane->GetFrameSize(theBalanceSize);
	
	SPoint32 theBalanceLocation;
	mBalancePane->GetFrameLocation(theBalanceLocation);
	
	SPoint32 theNewLocation;
	inView->GetFrameLocation(theNewLocation);
	
	Int32 theBalanceHeight= (theBrokerSize.height - inNewSize.height) - theBalanceSize.height;
	
	mBalancePane->ResizeFrameBy(0, theBalanceHeight, false);
	
	// Need to determine if the balance pane is above or below the dynamic pane
	SPoint32 dynamicPaneLocation, balancePaneLocation;
	mDynamicPane->GetFrameLocation( dynamicPaneLocation );
	mBalancePane->GetFrameLocation( balancePaneLocation );
	if( balancePaneLocation.v >= dynamicPaneLocation.v)
		mBalancePane->MoveBy(0, -theBalanceHeight, false);
	
	// if we're in an enclosing CBevelView, we need to notify it that
	// it's subpanes have changed so that it can recalculate the
	// bevel region.
	
	CBevelView::SubPanesChanged(this, inRefresh);
	
	if (inRefresh)
		Refresh();
}

void CSwatchBrokerView::AdaptToBrokeredViewMove(
	CBrokeredView*		inView,
	const SPoint32&		inNewLocation,
	Boolean				inRefresh)
{

}
	
