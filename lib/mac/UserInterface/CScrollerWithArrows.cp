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

//
// CScrollerWithArrows
// Mike Pinkerton, Netscape Communications
//
// See header file for class descriptions.
//
// NOTES:
// - does not yet autoscroll when doing drag and drop
// - icons for horizontal orientation are incorrect, but we don't have any better ones
//		in the app so I just put the vertical ones in as place holders.
// - has only been tested in vertical orientation
// - has only been tested with all indents set to 0
//

#include "CScrollerWithArrows.h"

#include <LStdControl.h>
#include <PP_Types.h>
#include <UGAColorRamp.h>

#pragma mark -- class CScrollArrowControl

//
// CScrollerWithArrows
//
// Default contsructor
//
CScrollerWithArrows::CScrollerWithArrows()
{
	mScrollingView = nil;
	mScrollingViewID = -1;
	mUpLeftArrow = nil;
	mDownRightArrow = nil;
	mIsVertical = false;
}


//
// CScrollerWithArrows								
//
// Stream Constructor
//
CScrollerWithArrows::CScrollerWithArrows(
	LStream	*inStream)
		: LView(inStream)
{
	SScrollerArrowInfo scrollerInfo;
	inStream->ReadData(&scrollerInfo, sizeof(SScrollerArrowInfo));
	
	// ScrollingView has not yet been created, since SuperViews are
	// created before their subviews when building Panes from a Stream.
	// Therefore, we store the ID of the ScrollingView so that the
	// FinishCreateSelf function can set up the proper connections.		
	mScrollingViewID = scrollerInfo.scrollingViewID;
	mScrollingView = nil;
	
	mUpLeftArrow = nil;
	mDownRightArrow = nil;

	// Determine orientation of scrolling. I guess I could just make this 
	// a flag in constructor....
	if ( mFrameSize.height > mFrameSize.width )
		mIsVertical = true;
		
	try {
		MakeScrollArrows(scrollerInfo.leftIndent, scrollerInfo.rightIndent,
							scrollerInfo.topIndent, scrollerInfo.bottomIndent);
	}	
	catch (...) {					// Failed to fully build scroll arrows
		delete mUpLeftArrow;
		delete mDownRightArrow;
		throw;
	}
	
} // CScrollerWithArrows stream constructor


//
// ~CScrollerWithArrows
//
CScrollerWithArrows::~CScrollerWithArrows()
{
	// view subsystem will dispose of scroll arrows because they are children
}


//
// MakeScrollArrows
//
// Create the arrow control objects in the view, indented accordingly to fit the
// needs of the subview. In contrast to how LScroller works, the indents apply to
// both scrollers.
//
void
CScrollerWithArrows::MakeScrollArrows(
	Int16			inLeftIndent,
	Int16			inRightIndent,
	Int16			inTopIndent,
	Int16			inBottomIndent)
{
	SPaneInfo arrowInfo;				// Common information for scroll arrows
	arrowInfo.visible = false;			// arrows aren't visible until
	arrowInfo.enabled = true;			//    scroller is activated
	arrowInfo.userCon = 0;
	arrowInfo.superView = this;	
	arrowInfo.paneID = 0;	
	
	if ( IsVertical() ) {
	
		arrowInfo.bindings.left = true;
		arrowInfo.bindings.right = true;
		
		arrowInfo.width = mFrameSize.width - inLeftIndent - inRightIndent;
		arrowInfo.height = 16;
		arrowInfo.left = inLeftIndent;
		
		// create the top vertical arrow
		arrowInfo.top = inTopIndent;
		arrowInfo.bindings.bottom = false;
		mUpLeftArrow = MakeOneScrollArrow ( arrowInfo, kUpLeft );								
		mUpLeftArrow->AddListener(this);
		
		// create the bottom vertical arrow
		arrowInfo.top = mFrameSize.height - inBottomIndent - arrowInfo.height;
		arrowInfo.bindings.top = false;
		arrowInfo.bindings.bottom = true;
		mDownRightArrow = MakeOneScrollArrow ( arrowInfo, kDownRight );								
		mDownRightArrow->AddListener(this);
			
	} // if view orientation is vertical
	else {
	
		arrowInfo.bindings.top = true;
		arrowInfo.bindings.bottom = true;
		
		arrowInfo.width = 16;
		arrowInfo.height = mFrameSize.height - inTopIndent - inBottomIndent;
		arrowInfo.top = inTopIndent;
		
		// create the left horizontal arrow
		arrowInfo.left = inLeftIndent;
		arrowInfo.bindings.right = false;
		mUpLeftArrow = MakeOneScrollArrow ( arrowInfo, kUpLeft );								
		mUpLeftArrow->AddListener(this);
		
		// create the right horizontal arrow
		arrowInfo.left = mFrameSize.width - inRightIndent - arrowInfo.width;
		arrowInfo.bindings.left = false;
		arrowInfo.bindings.right = true;
		mDownRightArrow = MakeOneScrollArrow ( arrowInfo, kDownRight );								
		mDownRightArrow->AddListener(this);
		
	} // else view orientation is horizontal
	
	SetDefaultAttachable(this);		// Reset so Attachments don't get
									//   attached to the ScrollBars
									
} // MakeScrollArrows


//
// MakeOneScrollArrow
//
// Creates a scroll arrow in the given direction.
//
LControl*
CScrollerWithArrows :: MakeOneScrollArrow ( const SPaneInfo &inPaneInfo,
												ScrollDir inScrollWhichWay )
{
	ResIDT iconID = 0;
	SControlInfo controlInfo;
	controlInfo.value = 0;
	controlInfo.minValue = 0;
	controlInfo.maxValue = 0;
	
	if ( inScrollWhichWay == kUpLeft ) {
		controlInfo.valueMessage = kControlUpButtonPart;
		iconID = IsVertical() ? CScrollArrowControl::kIconUp : CScrollArrowControl::kIconLeft;
		
	}
	else {
		controlInfo.valueMessage = kControlDownButtonPart;
		iconID = IsVertical() ? CScrollArrowControl::kIconDown : CScrollArrowControl::kIconRight;
	}
	
	return new CScrollArrowControl ( inPaneInfo, controlInfo, iconID );
	
} // MakeOneScrollArrow


//
// FinishCreateSelf
//
// Finish creation of the scroller by installing its scrolling subview.
//
void
CScrollerWithArrows::FinishCreateSelf()
{
	LView* scrollingView = dynamic_cast<LView*>(FindPaneByID(mScrollingViewID));
	if (scrollingView != nil) {
		GrafPtr	myPort = GetMacPort();
		if (myPort == nil)
			myPort = UQDGlobals::GetCurrentPort();
		StVisRgn	suppressDrawing(myPort);
		InstallView(scrollingView);
	}
	
} // FinishCreateSelf


//
// ActivateSelf
//
// Show arrows that were hidden when app was deactivated
//
void
CScrollerWithArrows::ActivateSelf()
{
	if ( mUpLeftArrow )
		mUpLeftArrow->Show();
	
	if ( mDownRightArrow )
		mDownRightArrow->Show();

} // ActivateSelf


//
// DeactivateSelf
//
// According to Mac Human Interface Guidelines, ScrollBars in inactive
// windows are hidden. We follow this by hiding the scroll arrows.
//
void
CScrollerWithArrows::DeactivateSelf()
{
	if ( mUpLeftArrow && mUpLeftArrow->IsVisible() ) {
		mUpLeftArrow->Hide();
		mUpLeftArrow->DontRefresh(true);
	}
	
	if ( mDownRightArrow && mDownRightArrow->IsVisible() ) {
		mDownRightArrow->Hide();
		mDownRightArrow->DontRefresh(true);
	}
	
	if ( FocusExposed() ) {				// Redraw now!
		Rect frame;
		CalcLocalFrameRect(frame);
		if (ExecuteAttachments(msg_DrawOrPrint, &frame)) {
			DrawSelf();
		}
	}

} // DeactivateSelf


//
// InstallView
//
// Install a Scrolling View within this Scroller
//
void
CScrollerWithArrows::InstallView ( LView *inScrollingView )
{
	mScrollingView = inScrollingView;
	AdjustScrollArrows();
	
} // InstallView


//
// ExpandSubPane
//
// Expand a SubPane, which should be the Scroller's ScrollingView, to
// fill the interior of a Scroller
//
void
CScrollerWithArrows::ExpandSubPane ( LPane *inSub, Boolean inExpandHoriz,
										Boolean	inExpandVert )
{
	SDimension16 subSize;
	inSub->GetFrameSize(subSize);
	SPoint32 subLocation;
	inSub->GetFrameLocation(subLocation);

	if (inExpandHoriz) {
		subSize.width = mFrameSize.width - 2;
		if ( !IsVertical() )
			subSize.width -= 2 * 15;		// account for 2 scroll arrows
		subLocation.h = 1;
	} else
		subLocation.h -= mFrameLocation.h;
	
	if (inExpandVert) {
		subSize.height = mFrameSize.height - 2;
		if ( IsVertical() )
			subSize.height -= 2 * 15;		// account for 2 scroll arrows
		subLocation.v = 1;
	} else
		subLocation.v -= mFrameLocation.v;
	
	inSub->PlaceInSuperFrameAt(subLocation.h, subLocation.v, false);
	inSub->ResizeFrameTo(subSize.width, subSize.height, false);

} // ExpandSubPane


//
// AdjustScrollArrows
//
// Adjust the arrows (value, min, and max) according to the current
// state of the Scroller and ScrollingView
//
void
CScrollerWithArrows::AdjustScrollArrows()
{
	// nothing to scroll, bail out after disabling scrollarrows.
	if ( !mScrollingView ) {
		if ( mUpLeftArrow )
			mUpLeftArrow->SetValue(0);
	
		if ( mDownRightArrow )
			mDownRightArrow->SetMinValue(0);
		
		return;
	}

	SPoint32		scrollUnit;
	SDimension16	scrollFrameSize;
	SDimension32	scrollImageSize;
	SPoint32		scrollPosition;
	mScrollingView->GetScrollUnit(scrollUnit);
	mScrollingView->GetFrameSize(scrollFrameSize);
	mScrollingView->GetImageSize(scrollImageSize);
	mScrollingView->GetScrollPosition(scrollPosition);
	
	bool enable = false;
	if ( IsVertical() ) {
		Int32 vertDiff = scrollImageSize.height - scrollFrameSize.height;
		if (scrollPosition.v > vertDiff)
			vertDiff = scrollPosition.v;

		// if there is a reason to scroll, then enable the scroll arrows
		if (vertDiff > 0)
			enable = true;
	}
	else {
		Int32	horizDiff = scrollImageSize.width - scrollFrameSize.width;
		if (scrollPosition.h > horizDiff)
			horizDiff = scrollPosition.h;

		// if there is a reason to scroll, then enable the scroll arrows
		if (horizDiff > 0)
			enable = true;
	}
	if ( enable ) {
		mUpLeftArrow->Enable();
		mDownRightArrow->Enable();
	}
	else {
		mUpLeftArrow->Disable();
		mDownRightArrow->Disable();
	}			
	
} // AdjustScrollArrows


//
// ResizeFrameBy
//
// Adjust the framesize by the given ammounts and adjust the scroll arrows to reflect
// the new sizes.
//
void
CScrollerWithArrows::ResizeFrameBy( Int16 inWidthDelta, Int16 inHeightDelta, Boolean inRefresh )
{
	// Let LView do all the work. All Scroller has to do is
	// adjust the ScrollBars to account for the new size
	// of the Scroller and resize its Image so it matches
	// its Frame size.		
	LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	
	{		
		// Prevent scroll bars from drawing while adjusting them
		StVisRgn	suppressDrawing(GetMacPort());
		AdjustScrollArrows();
	}
	
	ResizeImageBy(inWidthDelta, inHeightDelta, false);

} // ResizeFrameBy


//
// SubImageChanged
//
// arrow settings depend on the ScrollingView Image, so adjust them
// to match the current state.
//
void
CScrollerWithArrows::SubImageChanged ( LView *inSubView )
{
	if (inSubView == mScrollingView)
		AdjustScrollArrows();

} // SubImageChanged


//
// ListenToMessage
//
// Unlike LScroller which just listens for the thumb changing message, this scroller
// uses messages for all aspects of scrolling (of course, there is no such thing as
// a thumb in this case).
//
void
CScrollerWithArrows::ListenToMessage( MessageT inMessage, void *ioParam)
{
	Int32 scrollUnits = 0;
	
	switch ( inMessage ) {

		case kControlUpButtonPart:		// Scroll up/left one unit
			scrollUnits = -1;
			break;
			
		case kControlDownButtonPart:	// Scroll down/right one unit
			scrollUnits = 1;
			break;
		
	} // case of which button clicked
		
	if ( scrollUnits ) {
		SPoint32 scrollUnit;
		Int32 scrollBy;
		
		mScrollingView->GetScrollUnit(scrollUnit);
		scrollBy = scrollUnits * (IsVertical() ? scrollUnit.v : scrollUnit.h);
		mScrollingView->ScrollPinnedImageBy(0, scrollBy, true);
	}
	
} // ListenToMessage


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------

#pragma mark -- class CScrollArrowControl

CScrollArrowControl :: CScrollArrowControl (  const SPaneInfo &inPaneInfo,
												const SControlInfo	&inControlInfo,
												ResIDT inIconResID )
	: LGAIconButton ( inPaneInfo, inControlInfo, controlMode_Button, inIconResID,
						 sizeSelector_SmallIconSize, iconPosition_Center )
{
	// nothing else needed
}


CScrollArrowControl :: CScrollArrowControl ( LStream* inStream ) 
	:LGAIconButton(inStream)
{
	// nothing else needed	
}


//
// HotSpotAction
//
// Do the normal hot-spot stuff, but also send the message that the click is still going on,
// which effectively does auto-scroll. It is a bit too fast for my taste, but it works.
//
void
CScrollArrowControl :: HotSpotAction ( Int16 inHotSpot, Boolean inCurrInside, Boolean inPrevInside )
{
	LGAIconButton::HotSpotAction ( inHotSpot, inCurrInside, inPrevInside );
	if ( inCurrInside )
		BroadcastValueMessage();

} // HotSpotAction


//
// DrawSelf
//
// While the cursor tracking, etc of LGAIconButton is great, we don't want a border around
// the scroll triangle (it just looks bad). Only paint the background and draw the icon
//
void
CScrollArrowControl :: DrawSelf ( )
{
	StColorPenState::Normalize ();

	// Get the frame for the control and paint it
	Rect localFrame;
	CalcLocalFrameRect ( localFrame );
	localFrame.right--;
	::RGBForeColor ( &UGAColorRamp::GetColor(2));
	::PaintRect ( &localFrame );

	DrawIcon();

} // DrawSelf