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

#include "divview.h"

#include "resgui.h"
#include "macutil.h"
#include "macgui.h"
#include "CBevelView.h"

#include <Sound.h>


//-----------------------------------
LDividedView::LDividedView( LStream* inStream )
//-----------------------------------
:	LView( inStream )
,	fFirstView(nil)
,	fSecondView(nil)
,	mCollapseFirstByDragging(false)
,	mCollapseSecondByDragging(false)
,	mDividerPosBeforeCollapsing(100)
{
	*inStream >> mFirstSubview >> mSecondSubview;
		
	*inStream >> mDivSize;
	*inStream >> mFirstIndent;
	if ( mFirstIndent > mDivSize )
		mFirstIndent = mDivSize;
	*inStream >> mSecondIndent;
	if ( mSecondIndent > mDivSize )
		mSecondIndent = mDivSize;
	*inStream >> mMinFirstSize;
	*inStream >> mMinSecondSize;
	*inStream >> mSlidesHorizontally;
}

//-----------------------------------
LDividedView::~LDividedView()
//-----------------------------------
{
}

//-----------------------------------
void LDividedView::FinishCreateSelf()
//-----------------------------------
{
	LView::FinishCreateSelf();
			
	fFirstView = (LView*)this->FindPaneByID( mFirstSubview );
	fSecondView = (LView*)this->FindPaneByID( mSecondSubview );
	LBroadcaster* zapButton = dynamic_cast<LBroadcaster*>(GetZapButton());
	if (zapButton)
	{
		zapButton->AddListener(this);
		// Make sure the first pane can be collapsed
		Boolean collapseFirst, collapseSecond;
		GetCollapseByDragging(collapseFirst, collapseSecond);
		SetCollapseByDragging(true, collapseSecond);
	}
	this->SyncFrameBindings();
	mDividerPosBeforeCollapsing = GetDividerPosition();
}

//-----------------------------------
LPane* LDividedView::GetZapButton()
//-----------------------------------
{
	return FindPaneByID('Zap!');
}

//-----------------------------------
void LDividedView::ToggleFirstPane()
//-----------------------------------
{
	Int16 delta;
	if (IsFirstPaneCollapsed())
		delta = 1; // at left, restore to center.
	else if (IsSecondPaneCollapsed())
		delta = -1; // at right, restore to center
	else
		delta =  - GetDividerPosition(); // collapse left
	StValueChanger<Boolean> save(mCollapseFirstByDragging, true);
	ChangeDividerPosition(delta);
}
	
//-----------------------------------
void LDividedView::ListenToMessage(MessageT inMessage, void* /*ioParam*/)
//-----------------------------------
{
	if (inMessage == 'Zap!')
		ToggleFirstPane();
}
	
//-----------------------------------
void LDividedView::SyncFrameBindings()
//-----------------------------------
{
	SBooleanRect	firstBindings;
	SBooleanRect	secondBindings;

	firstBindings.top = firstBindings.bottom = firstBindings.left = firstBindings.right = TRUE;
	secondBindings.top = secondBindings.bottom = secondBindings.left = secondBindings.right = TRUE;
	
	if ( mSlidesHorizontally )
		firstBindings.right = FALSE;
	else
		firstBindings.bottom = FALSE;	
	
	fFirstView->SetFrameBinding( firstBindings );
	fSecondView->SetFrameBinding( secondBindings );
}

void LDividedView::PositionZapButton()
{
	SDimension16 zapFrameSize;
	LPane* zapButton = GetZapButton();
	if (!zapButton)
		return;
	zapButton->GetFrameSize(zapFrameSize);
	SDimension16 firstFrameSize;
	fFirstView->GetFrameSize( firstFrameSize );
	if (mSlidesHorizontally)
		zapButton->PlaceInSuperFrameAt(
			firstFrameSize.width + mFirstIndent,
			(mFrameSize.height >> 1) - (zapFrameSize.height >> 1), 
			FALSE );			
	else
		zapButton->PlaceInSuperFrameAt(
			(mFrameSize.width >> 1) - (zapFrameSize.width >> 1), 
			firstFrameSize.height + mFirstIndent,
			FALSE );			
		
}

void LDividedView::PositionViews( Boolean willBeHoriz )
{
	if ( mSlidesHorizontally == willBeHoriz )			
		return;

	this->FocusDraw();

	SDimension16 firstFrameSize;
	fFirstView->GetFrameSize( firstFrameSize );
	if ( mSlidesHorizontally )
	{
		Int16 subpaneSize = ( mFrameSize.height - mDivSize ) / 2;
		
		fFirstView->ResizeFrameTo(	mFrameSize.width - ( mFirstIndent * 2 ),
									subpaneSize - mFirstIndent,
									FALSE );
		fSecondView->ResizeFrameTo( mFrameSize.width - ( mSecondIndent * 2 ),
									subpaneSize - mSecondIndent,
									FALSE );
		
		fFirstView->PlaceInSuperFrameAt( mFirstIndent, mFirstIndent, FALSE );							
		fSecondView->PlaceInSuperFrameAt(	mSecondIndent,
											firstFrameSize.height + mFirstIndent + mDivSize,
											FALSE );
	}
	else
	{
		Int16 subpaneSize = ( mFrameSize.width - mDivSize ) / 2;

		fFirstView->ResizeFrameTo(	subpaneSize - mFirstIndent,
									mFrameSize.height - ( mFirstIndent * 2 ),
									FALSE );
		fSecondView->ResizeFrameTo(	subpaneSize - mSecondIndent,
									mFrameSize.height - ( mSecondIndent * 2),
									FALSE );

		fFirstView->PlaceInSuperFrameAt( mFirstIndent, mFirstIndent, FALSE );	
		fSecondView->PlaceInSuperFrameAt(	firstFrameSize.width + mFirstIndent + mDivSize,
											mSecondIndent,
											FALSE );
	}
	PositionZapButton();
	mSlidesHorizontally = willBeHoriz;
	this->SyncFrameBindings();
	BroadcastMessage( msg_DividerChangedPosition, this );
	CBevelView::SubPanesChanged(this, true);	
	this->Refresh();
} // LDividedView::PositionViews

// ¥Êset whether we are vertical or horizontal
void LDividedView::SetSlidesHorizontally( Boolean isHoriz )
{
	if ( mSlidesHorizontally != isHoriz )
		this->PositionViews( isHoriz );
}

Int32 LDividedView::GetDividerPosition() const
{
	Rect	firstFrame;
	
	GetSubpaneRect( (LView*)this, fFirstView, firstFrame );
	if ( this->GetSlidesHorizontally() )
		return firstFrame.right;
	else
		return firstFrame.bottom;
}

void LDividedView::CalcRectBetweenPanes( Rect& invRect )
{
	Rect		firstFrame;
	Rect		secondFrame;
	
	GetSubpaneRect( this, fFirstView, firstFrame );	
	GetSubpaneRect( this, fSecondView, secondFrame );

	this->CalcLocalFrameRect( invRect );

	if ( mSlidesHorizontally )
	{	
		invRect.left = firstFrame.right;
		invRect.right = secondFrame.left;
	}
	else
	{
		invRect.top = firstFrame.bottom;
		invRect.bottom = secondFrame.top;
	}
}

void LDividedView::CalcDividerRect( Rect& outRect )
{
	Rect		firstFrame;
	Rect		secondFrame;
	
	GetSubpaneRect( this, fFirstView, firstFrame );	
	GetSubpaneRect( this, fSecondView, secondFrame );

	this->CalcLocalFrameRect( outRect );
	
	if ( mSlidesHorizontally )
	{	
		Int16 mid = ( firstFrame.right + secondFrame.left ) / 2;
		outRect.left = mid - 1;
		outRect.right = mid + 1;
		outRect.top = ( firstFrame.top < secondFrame.top ) ? firstFrame.top : secondFrame.top;
		outRect.bottom = ( firstFrame.bottom > secondFrame.bottom ) ? firstFrame.bottom : secondFrame.bottom;
	}
	else
	{
		Int16 mid = ( firstFrame.bottom + secondFrame.top ) / 2;
		outRect.top = mid - 1;
		outRect.bottom = mid + 1;
		outRect.left = ( firstFrame.left < secondFrame.left ) ? firstFrame.left : secondFrame.left;
		outRect.right = ( firstFrame.right > secondFrame.right ) ? firstFrame.right : secondFrame.right;
	}
}

// ¥ this is somewhat hackish, but this routine calculates the limit rect
//		for dragging -- the hackish part is that it looks at its subviews
//		pane IDs to see if they begin with a 'D' and if so, assumes they
//		are also LDividedViews, and then does limiting based on that
//	Well, I fixed this by using dynamic_cast - jrm 97/10/17
void LDividedView::CalcLimitRect( Rect& lRect )
{
	this->CalcLocalFrameRect( lRect );

	LDividedView* other = dynamic_cast<LDividedView*>(fFirstView);
	if (other)
	{
		if ( other->GetSlidesHorizontally() == mSlidesHorizontally )
		{
			Point		local;
			local.h = local.v = 0;
			if ( other->GetSlidesHorizontally() )
				local.h = other->GetDividerPosition();
			else
				local.v = other->GetDividerPosition();
		
			other->LocalToPortPoint( local );
			this->PortToLocalPoint( local );
			
			if ( mSlidesHorizontally )
				lRect.left = local.h;
			else
				lRect.top = local.v;
		}					
	}

	other = dynamic_cast<LDividedView*>(fSecondView);
	if (other)
	{
		if ( other->GetSlidesHorizontally() == mSlidesHorizontally )
		{
			Point		local;
			local.h = local.v = 0;
			if ( other->GetSlidesHorizontally() )
				local.h = other->GetDividerPosition();
			else
				local.v = other->GetDividerPosition();
		
			other->LocalToPortPoint( local );
			this->PortToLocalPoint( local );
			
			if ( mSlidesHorizontally )
				lRect.right = local.h;
			else
				lRect.bottom = local.v;
		}					
	}

	if ( mSlidesHorizontally )
	{
		if (!mCollapseFirstByDragging)
			lRect.left += mMinFirstSize;
		if (!mCollapseSecondByDragging)
			lRect.right -= mMinSecondSize;
	}
	else
	{
		if (!mCollapseFirstByDragging)
			lRect.top += mMinFirstSize;
		if (!mCollapseSecondByDragging)
			lRect.bottom -= mMinSecondSize;
	}
}

void LDividedView::CalcSlopRect( Rect& sRect )
{
	LWindow*		myWin;
	
	myWin = LWindow::FetchWindowObject( this->GetMacPort() );
	if ( myWin )
	{
		myWin->CalcPortFrameRect( sRect );
		sRect = PortToLocalRect( this, sRect );
	}
	else
		this->CalcLocalFrameRect( sRect );
	// If we're allowed to drag/collapse, allow a drag beyond the frame
	if (mCollapseFirstByDragging)
		if (mSlidesHorizontally)
			sRect.left -= 100;
		else
			sRect.top -= 100;
	else if (mCollapseSecondByDragging)
		if (mSlidesHorizontally)
			sRect.right += 100;
		else
			sRect.bottom += 100;
}

void LDividedView::ClickSelf( const SMouseDownEvent& inMouseDown )
{
	Rect	frame;

	this->CalcRectBetweenPanes( frame );
	if ( PtInRect( inMouseDown.whereLocal, &frame ) )
	{
		this->FocusDraw();
		if (GetClickCount() > 1)
		{	
			// we don't necessarily want users double-clicking to close the pane.
			// If it makes sense, we can turn this back on.
//			ListenToMessage('Zap!', this);
			return;
		}
		
		// if the first pane is closed and dragging to open is turned off,
		// bail before we get to the dragging code.
		if ( !GetDividerPosition() && !CanExpandByDragging() )
			return;
			
		StRegion	myRgn;
		this->CalcDividerRect( frame );
		::RectRgn( myRgn, &frame );
		
		Rect lRect, sRect;
		this->CalcLimitRect( lRect );
		this->CalcSlopRect( sRect );				

		long result = ::DragGrayRgn(	myRgn, inMouseDown.whereLocal, 
								&lRect, &sRect, 
								mSlidesHorizontally ? hAxisOnly : vAxisOnly, nil );
		
		Int16 delta = mSlidesHorizontally ? LoWord( result ) : HiWord( result );
		
		if ( ( result == kOutsideSlop ) || ( delta == 0 ) ) 
			return;
		
		this->ChangeDividerPosition( delta );
	}
}

#define SAVE_VERSION	28

void LDividedView::SavePlace( LStream* inStream )
{
	WriteVersionTag( inStream, SAVE_VERSION );

	*inStream << mSlidesHorizontally;
	Int32 divPos = this->GetDividerPosition();
	*inStream << divPos;
	*inStream << mDividerPosBeforeCollapsing;
}

void LDividedView::RestorePlace( LStream* inStream )
{
	if ( !ReadVersionTag( inStream, SAVE_VERSION ) )
		throw (OSErr)rcDBWrongVersion;
		
	Int32		shouldBe;
	Boolean		isHoriz;
	
	*inStream >> isHoriz;
	this->PositionViews( isHoriz );
	Int32 divPos = this->GetDividerPosition();
	*inStream >> shouldBe;
	this->ChangeDividerPosition( shouldBe - divPos );
	*inStream >> mDividerPosBeforeCollapsing;
}

//-----------------------------------
void LDividedView::PlaySound(ResIDT id)
//-----------------------------------
{
	// Play the swishing sound, if available
	SndListHandle soundH =
		(SndListHandle)::GetResource('snd ', id);
	if (soundH)
	{
		::DetachResource((Handle)soundH);
		::SndPlay(nil, soundH, false);
		::DisposeHandle((Handle)soundH);
	}
}

//-----------------------------------
void LDividedView::ChangeDividerPosition( Int16 delta )
// ¥ move the divider, resize the top/left pane, and
//		move the bottom/right one
//-----------------------------------
{
	Int32 dividerPos = this->GetDividerPosition();
	Int16 newPos = dividerPos + delta;
	Int16 frameBound // drag beyond which causes slam
		=	mSlidesHorizontally ? mFrameSize.width : mFrameSize.height;	
	
	Boolean showFirst = false;
	Boolean showSecond = false;
	if (newPos > frameBound - mMinSecondSize)
	{
		if (mCollapseSecondByDragging)
		{
			if (IsSecondPaneCollapsed() && delta <= 0)
			{
				// Uncollapse from right
				delta = mDividerPosBeforeCollapsing - dividerPos;
				showSecond = true;
			}
			else
			{
				// Collapse to right
				delta = frameBound - mDivSize - dividerPos;
				fSecondView->Hide();
				if (dividerPos == 0) // user dragged from one extreme to the other
					showFirst = true;
				else
					mDividerPosBeforeCollapsing = dividerPos;
				if (IsVisible())
					PlayClosingSound();
			}
		}
		else
			return;
	}
	else if (newPos < mMinFirstSize)
	{
		if (mCollapseFirstByDragging)
		{
			if (IsFirstPaneCollapsed() && delta >= 0)
			{
				// Uncollapse from left
				delta = mDividerPosBeforeCollapsing - dividerPos;
				showFirst = true;
			}
			else
			{
				// Collapse to left
				delta = - dividerPos;
				fFirstView->Hide();
				if (dividerPos == frameBound - mDivSize)
					showSecond = true; // user dragged from one extreme to the other
				else
					mDividerPosBeforeCollapsing = dividerPos;
				if (IsVisible())
					PlayClosingSound();
			}
		}
		else
			return;
	}
	else if (IsSecondPaneCollapsed() && delta <= 0)
		showSecond = true;
	else if (IsFirstPaneCollapsed() && delta >= 0)
		showFirst = true;

	LPane* zapButton = GetZapButton();
	if ( mSlidesHorizontally )
	{
		fFirstView->ResizeFrameBy( delta, 0, FALSE );
		fSecondView->MoveBy( delta, 0, FALSE );
		fSecondView->ResizeFrameBy( -delta, 0, FALSE );
		if (zapButton)
			zapButton->MoveBy(delta, 0, FALSE);
	}
	else
	{
		fFirstView->ResizeFrameBy( 0, delta, FALSE );
		fSecondView->MoveBy( 0, delta, FALSE );
		fSecondView->ResizeFrameBy( 0, -delta, FALSE );
		if (zapButton)
			zapButton->MoveBy(0, delta, FALSE);
	}
	
	BroadcastMessage( msg_DividerChangedPosition, this );
	
	if (showFirst)
		fFirstView->Show();
	if (showSecond)
		fSecondView->Show();
	CBevelView::SubPanesChanged(this, false);	
	if ((showFirst || showSecond) && IsVisible())
		PlayOpeningSound();
	this->Refresh();
} // LDividedView::ChangeDividerPosition

void
LDividedView::ResizeFrameBy(
	Int16		inWidthDelta,
	Int16		inHeightDelta,
	Boolean		inRefresh)
{
	// The big flaw in this class is that the situations when the two subpanes change
	// have to be handled case by case.  This is but another.
	LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	PositionZapButton();
	CBevelView::SubPanesChanged(this, inRefresh);
}

void LDividedView::AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent )
{
	Rect		frame;
	
	// if the pane is closed and drag-to-expand is turned off, bail.
	if ( !GetDividerPosition() && !CanExpandByDragging() )
		return;
	
	this->CalcRectBetweenPanes( frame );
	this->PortToLocalPoint( inPortPt );
	if ( PtInRect( inPortPt, &frame ) )
	{
		if ( mSlidesHorizontally )
			::SetCursor( *(::GetCursor( curs_HoriDrag ) ) );
		else
			::SetCursor( *(::GetCursor( curs_VertDrag ) ) );
	}
	else
		LView::AdjustCursorSelf(inPortPt, inMacEvent);
}
