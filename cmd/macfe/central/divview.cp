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
#include "CDividerGrippyPane.h"

#include "resgui.h"
#include "macutil.h"
#include "macgui.h"
#include "CBevelView.h"

#include <Sound.h>


//-----------------------------------
LDividedView::LDividedView( LStream* inStream )
//-----------------------------------
:	LView( inStream )
,	mFirstPane(nil)
,	mSecondPane(nil)
,	mFeatureFlags(eNoFeatures)
,	mDividerPosBeforeCollapsing(100)
{
	sSettingUp = true;

	*inStream >> mFirstSubviewID >> mSecondSubviewID;
		
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

Boolean LDividedView::sSettingUp;

//----------------------------------------------------------------------------------------
LDividedView::~LDividedView()
//-----------------------------------
{
}

//-----------------------------------
void LDividedView::FinishCreateSelf()
//-----------------------------------
{
	LView::FinishCreateSelf();
			
	mFirstPane = FindPaneByID( mFirstSubviewID );
	mSecondPane = FindPaneByID( mSecondSubviewID );
	LPane* zapPane = GetZapButton();
	LBroadcaster* zapButton = dynamic_cast<LBroadcaster*>(zapPane);
	if (zapButton)
	{
		zapButton->AddListener(this);
		// Turn off any bindings.  We do this ourselves
		SBooleanRect zapFrameBinding;
		zapPane->GetFrameBinding(zapFrameBinding);
		// NOTE THE FOLLOWING CONVENTION: THE TOP AND BOTTOM BINDINGS ARE USED
		// TO SET THE FEATURE FLAGS.  THEN WE TURN OFF THE POWERPLANT BINDINGS.
		// This is because the entire button has to be moved, not just the
		// bottom or top edge of it.  So we take over this different type of
		// binding ourselves.
		// By the way, we cannot call SetFeatureFlags here, because we don't
		// want the side-effect of positioning the zap button - we haven't
		// calculated the offset yet!
		if (zapFrameBinding.top)
			SetFeatureFlagBits(eBindZapButtonTopOrLeft);
		else if (zapFrameBinding.bottom)
			SetFeatureFlagBits(eBindZapButtonBottomOrRight);
		zapFrameBinding.left
			= zapFrameBinding.top
			= zapFrameBinding.left
			= zapFrameBinding.bottom
			= zapFrameBinding.right
			= false;
		zapPane->SetFrameBinding(zapFrameBinding);
		OrientZapButtonTriangles();
	}
	SyncFrameBindings();
	if (!IsFirstPaneCollapsed() && !IsSecondPaneCollapsed())
		mDividerPosBeforeCollapsing = GetDividerPosition();
	sSettingUp = false;
} // LDividedView::FinishCreateSelf

//----------------------------------------------------------------------------------------
LPane* LDividedView::GetZapButton()
//-----------------------------------
{
	return FindPaneByID('Zap!');
}

//----------------------------------------------------------------------------------------
void LDividedView::DoZapAction()
// If one pane is collapsed, restore it to the saved position.
// Otherwise, zap it as specified by the feature flags.
//----------------------------------------------------------------------------------------
{
	if ((mFeatureFlags & eZapClosesFirst) && (mFeatureFlags & eZapClosesSecond))
	{
		Assert_(false); // you shouldn't have set both!
		UnsetFeatureFlags(eZapClosesFirst);
	}
	Int16 delta;
	if (IsFirstPaneCollapsed())
		delta = 1; // at left, restore to center.
	else if (IsSecondPaneCollapsed())
		delta = -1; // at right, restore to center
	else if (mFeatureFlags & eZapClosesFirst)
		delta =  - 16000; // collapse left
	else if (mFeatureFlags & eZapClosesSecond)
	{
		delta =  16000; // collapse second
	}
	FeatureFlags savedFlags = mFeatureFlags;
	try
	{
		SetFeatureFlagBits(eCollapseBothWaysByDragging);
		ChangeDividerPosition(delta);
	}
	catch(...)
	{
	}
	mFeatureFlags = savedFlags;
} // LDividedView::DoZapAction
	
//----------------------------------------------------------------------------------------
void LDividedView::ListenToMessage(MessageT inMessage, void* /*ioParam*/)
//-----------------------------------
{
	if (inMessage == 'Zap!')
		DoZapAction();
} // LDividedView::ListenToMessage
	
//----------------------------------------------------------------------------------------
void LDividedView::OnlySetFeatureFlags(FeatureFlags inFlags)
//----------------------------------------------------------------------------------------
{
	// Since this is a public function, we don't allow it to change private bits (eg,
	// the binding bits).
	Assert_(inFlags == (inFlags & ~ePrivateMask));
	mFeatureFlags = (FeatureFlags)(mFeatureFlags | (inFlags & ~ePrivateMask));
	OrientZapButtonTriangles();
} // LDividedView::OnlySetFeatureFlags

//----------------------------------------------------------------------------------------
void LDividedView::SyncFrameBindings()
//----------------------------------------------------------------------------------------
{
	SBooleanRect	firstBindings;
	SBooleanRect	secondBindings;

	firstBindings.top = firstBindings.bottom =
		firstBindings.left = firstBindings.right = true;
	secondBindings.top = secondBindings.bottom =
		secondBindings.left = secondBindings.right = true;
	
	if ( mSlidesHorizontally )
	{
		if (IsSecondPaneCollapsed())
			secondBindings.left = false;
		else
			firstBindings.right = false;
	}
	else
	{
		if (IsSecondPaneCollapsed())
			secondBindings.top = false;
		else
			firstBindings.bottom = false;
	}	
	
	mFirstPane->SetFrameBinding( firstBindings );
	mSecondPane->SetFrameBinding( secondBindings );
} // LDividedView::SyncFrameBindings

//----------------------------------------------------------------------------------------
void LDividedView::SetZapFrame(
	SInt16 inZapWidth,
	SInt16 inDividerPosition,			// relative to divided view's frame
	SInt16 inDividerLength,
	SInt16& ioZapLength,
	SInt32& ioZapOffsetLateral, 		// relative to divided view's frame
	SInt32& ioZapOffsetLongitudinal)	// relative to divided view's frame

// This routine is here to combine two cases : horizontal and vertical sliding.
// The cases are identical except for having to interchange "h" and "v", "width" and
// "height".  The solution is to provide this routine in "latitude"/"longitude"
// coordinates.
//
// The Zap button is long and thin.  "Lateral" means orthogonal to its long dimension,
// and "longitudinal" means parallel to its long dimension.
//
// Other blocks of code  in this class could share code in a similar way.
//----------------------------------------------------------------------------------------
{ 
	const SInt16 kPreferredZapSize = 150;
	const UInt16 zapIndent = inZapWidth;
	const UInt16 zapMaxSize = inDividerLength * 2 / 3;
	ioZapLength = kPreferredZapSize;
	if (ioZapLength > zapMaxSize)
		ioZapLength = zapMaxSize;
	ioZapOffsetLateral = inDividerPosition;
	if (mFeatureFlags & eBindZapButtonTopOrLeft)
		ioZapOffsetLongitudinal = zapIndent;
	else if (mFeatureFlags & eBindZapButtonBottomOrRight)
		ioZapOffsetLongitudinal = inDividerLength - ioZapLength - zapIndent;
	else
		ioZapOffsetLongitudinal = (inDividerLength >> 1) - (ioZapLength >> 1);
} // LDividedView::SetZapFrame

//----------------------------------------------------------------------------------------
void LDividedView::PositionZapButton()
//----------------------------------------------------------------------------------------
{
	LPane* zapButton = GetZapButton();
	if (!zapButton || !mFirstPane || !FocusDraw())
		return;
	
	SDimension16 zapFrameSize;
	zapButton->GetFrameSize(zapFrameSize);
		
	SDimension16 firstFrameSize;
	mFirstPane->GetFrameSize(firstFrameSize);

	SPoint32 zapFrameLocation;
	
	if (mSlidesHorizontally)
	{
		SetZapFrame(
			zapFrameSize.width,
			firstFrameSize.width + mFirstIndent,
			mFrameSize.height,
			zapFrameSize.height,
			zapFrameLocation.h,
			zapFrameLocation.v);
	}
	else
	{
		SetZapFrame(
			zapFrameSize.height,
			firstFrameSize.height + mFirstIndent, 
			mFrameSize.width,
			zapFrameSize.width,
			zapFrameLocation.v,
			zapFrameLocation.h);
	}
	zapButton->ResizeFrameTo(zapFrameSize.width, zapFrameSize.height, false);
	zapButton->PlaceInSuperFrameAt(zapFrameLocation.h, zapFrameLocation.v, false);
} // LDividedView::PositionZapButton

//----------------------------------------------------------------------------------------
void LDividedView::SetSubPanes(
	LPane* inFirstPane,
	LPane* inSecondPane,
	Int32 inDividerPos,
	Boolean inRefresh)
// After a rearrangement of the hierarchy, change the two subviews and reposition them.
//----------------------------------------------------------------------------------------
{
	mFirstPane = inFirstPane;
	mSecondPane = inSecondPane;

	mFirstPane->PutInside(this);
	mSecondPane->PutInside(this);

	Int32 secondFrameOffset = inDividerPos + mDivSize;
	Int32 firstFrameSize = inDividerPos - mFirstIndent;
	
	StValueChanger<Boolean> setterUp(sSettingUp, true);
	if ( mSlidesHorizontally )
	{
		mFirstPane->ResizeFrameTo(	firstFrameSize,
									mFrameSize.height - (mFirstIndent * 2 ),
									false );
		mSecondPane->ResizeFrameTo(	mFrameSize.width - (secondFrameOffset + mSecondIndent),
									mFrameSize.height - (mSecondIndent * 2),
									false );

		mFirstPane->PlaceInSuperFrameAt( mFirstIndent, mFirstIndent, false );	
		mSecondPane->PlaceInSuperFrameAt(secondFrameOffset,
										mSecondIndent,
										false );
	}
	else
	{
		mFirstPane->ResizeFrameTo(	mFrameSize.width - ( mFirstIndent * 2 ),
									firstFrameSize,
									false );
		mSecondPane->ResizeFrameTo( mFrameSize.width - ( mSecondIndent * 2 ),
									mFrameSize.height - (secondFrameOffset + mSecondIndent),
									false );
		
		mFirstPane->PlaceInSuperFrameAt( mFirstIndent, mFirstIndent, false );							
		mSecondPane->PlaceInSuperFrameAt(mSecondIndent,
										secondFrameOffset,
										false );
	}
	PositionZapButton();
	SyncFrameBindings();
	BroadcastMessage( msg_DividerChangedPosition, this );
	CBevelView::SubPanesChanged(this, inRefresh);	
} // LDividedView::SetSubPanes

//----------------------------------------------------------------------------------------
void LDividedView::PositionViews( Boolean willBeHoriz )
{
	if ( mSlidesHorizontally == willBeHoriz )			
		return;

	FocusDraw();

	mSlidesHorizontally = willBeHoriz;
	SDimension16 firstFrameSize;
	mFirstPane->GetFrameSize( firstFrameSize );
	if ( !mSlidesHorizontally ) // ie, it did BEFORE
	{
		Int16 subpaneSize = ( mFrameSize.height - mDivSize ) / 2;
		
		mFirstPane->ResizeFrameTo(	mFrameSize.width - ( mFirstIndent * 2 ),
									subpaneSize - mFirstIndent,
									FALSE );
		mSecondPane->ResizeFrameTo( mFrameSize.width - ( mSecondIndent * 2 ),
									subpaneSize - mSecondIndent,
									FALSE );
		
		mFirstPane->PlaceInSuperFrameAt(
									mFirstIndent, mFirstIndent, false );							
		mSecondPane->PlaceInSuperFrameAt(mSecondIndent,
									firstFrameSize.height + mFirstIndent + mDivSize,
									false );
	}
	else
	{
		Int16 subpaneSize = ( mFrameSize.width - mDivSize ) / 2;

		mFirstPane->ResizeFrameTo(	subpaneSize - mFirstIndent,
									mFrameSize.height - ( mFirstIndent * 2 ),
									FALSE );
		mSecondPane->ResizeFrameTo(	subpaneSize - mSecondIndent,
									mFrameSize.height - ( mSecondIndent * 2),
									FALSE );

		mFirstPane->PlaceInSuperFrameAt( mFirstIndent, mFirstIndent, false );	
		mSecondPane->PlaceInSuperFrameAt(
									firstFrameSize.width + mFirstIndent + mDivSize,
									mSecondIndent,
									false );
	}
	PositionZapButton();
	SyncFrameBindings();
	BroadcastMessage( msg_DividerChangedPosition, this );
	CBevelView::SubPanesChanged(this, true);	
	Refresh();
} // LDividedView::PositionViews

//----------------------------------------------------------------------------------------
void LDividedView::SetSlidesHorizontally( Boolean isHoriz )
// ¥Êset whether we are vertical or horizontal
//----------------------------------------------------------------------------------------
{
	if ( mSlidesHorizontally != isHoriz )
		PositionViews( isHoriz );
} // LDividedView::SetSlidesHorizontally

//----------------------------------------------------------------------------------------
Int32 LDividedView::GetDividerPosition() const
//----------------------------------------------------------------------------------------
{
	Rect	firstFrame;
	
	GetSubpaneRect( (LView*)this, mFirstPane, firstFrame );
	if ( GetSlidesHorizontally() )
		return firstFrame.right;
	else
		return firstFrame.bottom;
} // LDividedView::GetDividerPosition

//----------------------------------------------------------------------------------------
void LDividedView::CalcRectBetweenPanes( Rect& invRect )
//----------------------------------------------------------------------------------------
{
	Rect		firstFrame;
	Rect		secondFrame;
	
	GetSubpaneRect( this, mFirstPane, firstFrame );	
	GetSubpaneRect( this, mSecondPane, secondFrame );

	CalcLocalFrameRect( invRect );

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

//----------------------------------------------------------------------------------------
void LDividedView::CalcDividerRect( Rect& outRect )
//----------------------------------------------------------------------------------------
{
	Rect		firstFrame;
	Rect		secondFrame;
	
	GetSubpaneRect( this, mFirstPane, firstFrame );	
	GetSubpaneRect( this, mSecondPane, secondFrame );

	CalcLocalFrameRect( outRect );
	
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

//----------------------------------------------------------------------------------------
void LDividedView::CalcLimitRect( Rect& lRect )
// ¥ this is somewhat hackish, but this routine calculates the limit rect
//		for dragging -- the hackish part is that it looks at its subviews
//		pane IDs to see if they begin with a 'D' and if so, assumes they
//		are also LDividedViews, and then does limiting based on that
//	Well, I fixed this by using dynamic_cast - jrm 97/10/17
//----------------------------------------------------------------------------------------
{
	CalcLocalFrameRect( lRect );

	LDividedView* other = dynamic_cast<LDividedView*>(mFirstPane);
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
			PortToLocalPoint( local );
			
			if ( mSlidesHorizontally )
				lRect.left = local.h;
			else
				lRect.top = local.v;
		}					
	}

	other = dynamic_cast<LDividedView*>(mSecondPane);
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
			PortToLocalPoint( local );
			
			if ( mSlidesHorizontally )
				lRect.right = local.h;
			else
				lRect.bottom = local.v;
		}					
	}

	if ( mSlidesHorizontally )
	{
		if (!(mFeatureFlags & eCollapseFirstByDragging))
			lRect.left += mMinFirstSize;
		if (!(mFeatureFlags & eCollapseSecondByDragging))
			lRect.right -= mMinSecondSize;
	}
	else
	{
		if (!(mFeatureFlags & eCollapseFirstByDragging))
			lRect.top += mMinFirstSize;
		if (!(mFeatureFlags & eCollapseSecondByDragging))
			lRect.bottom -= mMinSecondSize;
	}
}

//----------------------------------------------------------------------------------------
void LDividedView::CalcSlopRect( Rect& sRect )
//----------------------------------------------------------------------------------------
{
	LWindow*		myWin;
	
	myWin = LWindow::FetchWindowObject( GetMacPort() );
	if ( myWin )
	{
		myWin->CalcPortFrameRect( sRect );
		sRect = PortToLocalRect( this, sRect );
	}
	else
		CalcLocalFrameRect( sRect );
	// If we're allowed to drag/collapse, allow a drag beyond the frame
	if ((mFeatureFlags & eCollapseFirstByDragging))
		if (mSlidesHorizontally)
			sRect.left -= 100;
		else
			sRect.top -= 100;
	else if ((mFeatureFlags & eCollapseSecondByDragging))
		if (mSlidesHorizontally)
			sRect.right += 100;
		else
			sRect.bottom += 100;
}

//----------------------------------------------------------------------------------------
void LDividedView::ClickSelf( const SMouseDownEvent& inMouseDown )
//----------------------------------------------------------------------------------------
{
	Rect	frame;

	CalcRectBetweenPanes( frame );
	if ( PtInRect( inMouseDown.whereLocal, &frame ) )
	{
		FocusDraw();
		if (GetClickCount() > 1)
		{	
			// we don't necessarily want users double-clicking to close the pane.
			// If it makes sense, we can turn this back on.
//			ListenToMessage('Zap!', this);
			return;
		}

		// if either pane is closed and dragging to open is turned off,
		// bail before we get to the dragging code.
		if ( (IsFirstPaneCollapsed() || IsSecondPaneCollapsed()) && !CanExpandByDragging() )
			return;
			
		StRegion	myRgn;
		CalcDividerRect( frame );
		::RectRgn( myRgn, &frame );
		
		Rect lRect, sRect;
		CalcLimitRect( lRect );
		CalcSlopRect( sRect );				

		long result = ::DragGrayRgn( myRgn, inMouseDown.whereLocal, &lRect, &sRect, 
										mSlidesHorizontally ? hAxisOnly : vAxisOnly, nil );
		
		Int16 delta = mSlidesHorizontally ? LoWord( result ) : HiWord( result );
		
		if (result == kOutsideSlop || delta == 0) 
			return;
		
		ChangeDividerPosition( delta );
	}
}

#define SAVE_VERSION	28

//----------------------------------------------------------------------------------------
void LDividedView::SavePlace( LStream* inStream )
//----------------------------------------------------------------------------------------
{
	WriteVersionTag( inStream, SAVE_VERSION );

	*inStream << mSlidesHorizontally;
	Int32 divPos = GetDividerPosition();
	*inStream << divPos;
	*inStream << mDividerPosBeforeCollapsing;
}

//----------------------------------------------------------------------------------------
void LDividedView::RestorePlace( LStream* inStream )
//----------------------------------------------------------------------------------------
{
	if ( !ReadVersionTag( inStream, SAVE_VERSION ) )
		throw (OSErr)rcDBWrongVersion;
	
	Boolean isSlidingHoriz;
	*inStream >> isSlidingHoriz;

	Int32 shouldBeHere;
	*inStream >> shouldBeHere;

	*inStream >> mDividerPosBeforeCollapsing;

	PositionViews( isSlidingHoriz );

	Int32 divPos = GetDividerPosition();
	if (shouldBeHere != divPos)
		ChangeDividerPosition( shouldBeHere - divPos );
} // LDividedView::RestorePlace

//----------------------------------------------------------------------------------------
void LDividedView::PlaySound(ResIDT id)
//----------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------
Boolean LDividedView::PreferSounds()
//----------------------------------------------------------------------------------------
{
	return false;		// no sounds! no sounds!
}

//----------------------------------------------------------------------------------------
void LDividedView::ChangeDividerPosition( Int16 delta )
// ¥ move the divider, resize the top/left pane, and
//		move the bottom/right one
//----------------------------------------------------------------------------------------
{
	Int32 dividerPos = GetDividerPosition();
	Int16 newPos = dividerPos + delta;
	Int16 frameBound // drag beyond which causes slam
		=	mSlidesHorizontally ? mFrameSize.width : mFrameSize.height;	
	
	Boolean showFirst = false;
	Boolean showSecond = false;
	if (newPos > frameBound - mMinSecondSize)
	{
		if ((mFeatureFlags & eCollapseSecondByDragging))
		{
			if (IsSecondPaneCollapsed() && delta <= 0)
			{
				// Uncollapse from right
				if (mDividerPosBeforeCollapsing >= frameBound)
					mDividerPosBeforeCollapsing = frameBound / 2;
				delta = mDividerPosBeforeCollapsing - dividerPos;
				showSecond = true;
			}
			else
			{
				// Collapse to right
				delta = frameBound - mDivSize - dividerPos;
				mSecondPane->Hide();
				if (dividerPos == 0) // user dragged from one extreme to the other
					showFirst = true;
				else
					mDividerPosBeforeCollapsing = dividerPos;
				if (IsVisible() && PreferSounds())
					PlayClosingSound();
			}
		}
		else
			return;
	}
	else if (newPos < mMinFirstSize)
	{
		if ((mFeatureFlags & eCollapseFirstByDragging))
		{
			if (IsFirstPaneCollapsed() && delta >= 0)
			{
				// Uncollapse from left
				if (mDividerPosBeforeCollapsing >= frameBound)
					mDividerPosBeforeCollapsing = frameBound / 2;
				delta = mDividerPosBeforeCollapsing - dividerPos;
				showFirst = true;
			}
			else
			{
				// Collapse to left
				delta = - dividerPos;
				mFirstPane->Hide();
				if (dividerPos == frameBound - mDivSize)
					showSecond = true; // user dragged from one extreme to the other
				else
					mDividerPosBeforeCollapsing = dividerPos;
				if (IsVisible() && PreferSounds())
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

	if (mSlidesHorizontally)
	{
		mFirstPane->ResizeFrameBy( delta, 0, false );
		mSecondPane->MoveBy( delta, 0, false );
		mSecondPane->ResizeFrameBy( -delta, 0, false );
	}
	else
	{
		mFirstPane->ResizeFrameBy( 0, delta, false );
		mSecondPane->MoveBy( 0, delta, false );
		mSecondPane->ResizeFrameBy( 0, -delta, false );
	}
	SyncFrameBindings();
	
	BroadcastMessage( msg_DividerChangedPosition, this );
	
	if (showFirst)
		mFirstPane->Show();
	if (showSecond)
		mSecondPane->Show();
	CBevelView::SubPanesChanged(this, false);	
	if ((showFirst || showSecond) && IsVisible() && PreferSounds())
		PlayOpeningSound();
	
	PositionZapButton();
	OrientZapButtonTriangles();
	Refresh();
} // LDividedView::ChangeDividerPosition

enum
{
	kTriangleRight = 10000
,	kTriangleDown
,	kTriangleLeft
,	kTriangleUp
};

//----------------------------------------------------------------------------------------
void LDividedView::OrientZapButtonTriangles()
//----------------------------------------------------------------------------------------
{
	// no grippies for mozilla??? not yet at least, maybe for HTML area.
#if 0
	CDividerGrippyPane* zapper = dynamic_cast<CDividerGrippyPane*>(GetZapButton());
	if (!zapper)
		return;
	if (IsFirstPaneCollapsed()
	|| ((mFeatureFlags & eZapClosesSecond) && !IsSecondPaneCollapsed()))
		zapper->SetTriangleIconID(mSlidesHorizontally ? kTriangleRight : kTriangleDown);
	else if (IsSecondPaneCollapsed()
	|| ((mFeatureFlags & eZapClosesFirst) && !IsFirstPaneCollapsed()))
		zapper->SetTriangleIconID(mSlidesHorizontally ? kTriangleLeft : kTriangleUp);
#endif
} // LDividedView::OrientZapButtonTriangles

//----------------------------------------------------------------------------------------
void LDividedView::ResizeFrameBy(
	Int16		inWidthDelta,
	Int16		inHeightDelta,
	Boolean		inRefresh)
//----------------------------------------------------------------------------------------
{
	LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	if (sSettingUp)
		return;

	if (mFirstPane
		&& GetDividerPosition() > (mSlidesHorizontally ?
				mFrameSize.width : mFrameSize.height) - mDivSize)
	{
		// Bug #309526.  The divider is bound to the left/top.  If the second pane is
		// hidden, then it is bound to the right/bottom also.  But if the second pane
		// is showing, and the window is sized too small by dragging the grow box,
		// then the divider can be out of view (not quite correct)!  The real problem
		// with this is that the scrollbar can be clipped.
		// So if after the resize this situation obtains, make the following call.
		// ChangeDividerPosition will notice that the divider is offscreen,
		// and it will zap the second frame closed, so now the divider will stick to
		// the edge.
		ChangeDividerPosition(1);
	}
	else
	{
		PositionZapButton();
		CBevelView::SubPanesChanged(this, inRefresh);
	}
}

//----------------------------------------------------------------------------------------
void LDividedView::AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent )
//----------------------------------------------------------------------------------------
{
	Rect		frame;
	
	CalcRectBetweenPanes( frame );
	PortToLocalPoint( inPortPt );
	if ( PtInRect( inPortPt, &frame ) )
	{
		// if either pane is closed and dragging to open is turned off,
		// bail before we get to cursor switching code
		if ( (IsFirstPaneCollapsed() || IsSecondPaneCollapsed()) && !CanExpandByDragging() )
			return;			

		if ( mSlidesHorizontally )
			::SetCursor( *(::GetCursor( curs_HoriDrag ) ) );
		else
			::SetCursor( *(::GetCursor( curs_VertDrag ) ) );
	}
	else
		LView::AdjustCursorSelf(inPortPt, inMacEvent);
}
