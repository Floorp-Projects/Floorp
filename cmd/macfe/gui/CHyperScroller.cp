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

#include "CHyperScroller.h"
#include "UGraphicGizmos.h"
#include "macgui.h"

const ResIDT cSizeBoxIconID = 29200;

//------------------------------------------------------------------------------
// CHyperScroller
//------------------------------------------------------------------------------
#define FOCUSBOXWIDTH 3


#include "CHTMLView.h"

CHyperScroller::CHyperScroller(LStream *inStream)
	:	CConfigActiveScroller(inStream)
{
	fVerticalBar = NULL;
	fHorizontalBar = NULL;
	mHandleGrowIconManually = false;
	
	ResetExpansion();
}


void CHyperScroller::ResetExpansion(void)
{
	mExpanded.left = false;
	mExpanded.top = false;
	mExpanded.right = false;
	mExpanded.bottom = false;
}


void CHyperScroller::ExpandLeft(void)
{
	mExpanded.left = true;
}

void CHyperScroller::ExpandTop(void)
{
	mExpanded.top = true;
}

void CHyperScroller::ExpandRight(void)
{
	mExpanded.right = true;
}

void CHyperScroller::ExpandBottom(void)
{
	mExpanded.bottom = true;
}


void CHyperScroller::AdjustHyperViewBounds(void)
{
	Assert_(mScrollingView);
	
	if (!mScrollingView)
	{
		return;
	}
	
	CHTMLView* theHyperView = dynamic_cast<CHTMLView*>(mScrollingView);
	
	SDimension16 theFrameSize;
	GetFrameSize(theFrameSize);

	SDimension16 theHyperSize = theFrameSize;

	if (!mExpanded.top)
		theHyperSize.height--;
		
	if (!mExpanded.left)
		theHyperSize.width--;
		
	if (!mExpanded.bottom)
		theHyperSize.height--;
		
	if (!mExpanded.right)
		theHyperSize.width--;
		
	if (mVerticalBar != NULL)
	{
		theHyperSize.width -= (ScrollBar_Size - 1);
		if (mExpanded.right)
			theHyperSize.width--;
	}

	if (mHorizontalBar != NULL)
	{
		theHyperSize.height -= (ScrollBar_Size - 1);
		if (mExpanded.bottom)
			theHyperSize.height--;
	}

	Int32 theHorizOffset = 1;
	Int32 theVertOffset = 1;
	
	if (mExpanded.top)
		theVertOffset--;
		
	if (mExpanded.left)
		theHorizOffset--;
		
	mScrollingView->PlaceInSuperFrameAt(theHorizOffset, theVertOffset, false);
	mScrollingView->ResizeFrameTo(theHyperSize.width, theHyperSize.height, false);
}

void CHyperScroller::AdjustScrollBars()
	{
		Boolean need_inherited_AdjustScrollbars = true;

			/*
				If there is no scrolling view, then we don't adjust anything (even though
				you might think we should) because there probably is about to be a scrolling
				view, and adjusting now will mess things up...
			*/

		if ( mScrollingView )
			{
				Boolean hasHorizontal	= mHorizontalBar != NULL;
				Boolean hasVertical		= mVerticalBar != NULL;

					// Unless told otherwise, assume we've got what we need.
				Boolean needHorizontal	= hasHorizontal;
				Boolean needVertical		= hasVertical;

				CHTMLView* htmlView = dynamic_cast<CHTMLView*>(mScrollingView);
				if ( !htmlView || (htmlView->GetScrollMode()==LO_SCROLL_AUTO) )
					{
						switch ( htmlView->GetScrollMode() )
							{
								case LO_SCROLL_NO:
									needHorizontal = needVertical = false;
									break;

								case LO_SCROLL_YES:
									needHorizontal = needVertical = true;
									break;

								case LO_SCROLL_AUTO:
									{
											/*
												...if there is a scrolling view, and either it's not a |CHTMLView| or
												if it is a |CHTMLView|, it allows scrollbars... then figure out if we
												need scrollbars based on the size of the frame and image
											*/

										SDimension16 frameSize;
										mScrollingView->GetFrameSize(frameSize);

										SDimension32 imageSize;
										mScrollingView->GetImageSize(imageSize);

										needHorizontal	= imageSize.width > frameSize.width;
										needVertical		= imageSize.height > frameSize.height;
									}
									break;
							}
					}

				if ( (hasHorizontal != needHorizontal) || (hasVertical != needVertical) )
					{
						ShowScrollbars(needHorizontal, needVertical);
						need_inherited_AdjustScrollbars = false; // ...since |ShowScrollbars| calls it.
					}
			}

		if ( need_inherited_AdjustScrollbars )
			/*inherited*/	CConfigActiveScroller::AdjustScrollBars();
	}

void CHyperScroller::RefreshSelf()
		/*
			...refresh just the area occupied by me and my scrollers,
			and not by my mScrollingView
		*/
	{
		if ( mHorizontalBar )
			mHorizontalBar->Refresh();

		if ( mVerticalBar )
			mVerticalBar->Refresh();

//		if ( mHorizontalBar && mVerticalBar )
		if ((mHandleGrowIconManually && (mVerticalBar || mHorizontalBar)) || (mVerticalBar && mHorizontalBar))
			{
				Rect R = CalcDummySizeBox();
				LocalToPortPoint(topLeft(R));
				LocalToPortPoint(botRight(R));
				InvalPortRect(&R);
			}
	}

// ¥¥ display control
// Show/hide the controls
// Adjust the hyperview size
void CHyperScroller::ShowScrollbars( Boolean horiOn, Boolean vertOn )
{
	Boolean		hasHorizontal = mHorizontalBar != NULL;
	Boolean		hasVertical = mVerticalBar != NULL;

	// ¥ quick check
	if ((hasHorizontal == horiOn) && (hasVertical == vertOn))
		return;

	SDimension16 theFrameSize;
	GetFrameSize(theFrameSize);
	
	// ¥ vertical scrollbar
	{
		// ¥ adjustment for visiblity of horizontal scroller
		// ¥ show/hide
		if (mVerticalBar && !vertOn)
			{
			mVerticalBar->Hide();
			fVerticalBar = mVerticalBar;
			mVerticalBar = NULL;
			}
		else if (!mVerticalBar && vertOn)
			{
			mVerticalBar = fVerticalBar;
			fVerticalBar = NULL;
			if (mVerticalBar)
				mVerticalBar->Show();
			}

		Int32 theVerticalSize = theFrameSize.height;
//		if (hasHorizontal || horiOn)
		if (mHandleGrowIconManually || hasHorizontal || horiOn)
			theVerticalSize -= (ScrollBar_Size - 1);

		LStdControl* theBar = (mVerticalBar != NULL) ? mVerticalBar : fVerticalBar;
		StFocusAndClipIfHidden theControlClip(theBar);
		theBar->PlaceInSuperFrameAt((theFrameSize.width - ScrollBar_Size), 0, false);
		theBar->ResizeFrameTo(ScrollBar_Size, theVerticalSize, false);

		if (mVerticalBar != NULL)
			mVerticalBar->Refresh();
	}

	// ¥Êhorizontal scrollbar
	{

		if (mHorizontalBar && !horiOn)
			{
			mHorizontalBar->Hide();
			fHorizontalBar = mHorizontalBar;
			mHorizontalBar = NULL;
			}
		else if (!mHorizontalBar && horiOn)
			{
			mHorizontalBar = fHorizontalBar;
			fHorizontalBar = NULL;
			if (mHorizontalBar)
				mHorizontalBar->Show();
			}

		Int32 theHorizontalSize = theFrameSize.width;
//		if (hasVertical || vertOn)
		if (mHandleGrowIconManually || hasVertical || vertOn)
			theHorizontalSize -= (ScrollBar_Size - 1);

		LPane* theBar = (mHorizontalBar != NULL) ? mHorizontalBar : fHorizontalBar;
		if ( theBar )
		{
			StFocusAndClipIfHidden theControlClip(theBar);
			theBar->PlaceInSuperFrameAt(0, (theFrameSize.height - ScrollBar_Size), false);
			theBar->ResizeFrameTo(theHorizontalSize, ScrollBar_Size, false);
		}

		if (mHorizontalBar != NULL)
			mHorizontalBar->Refresh();
	}

	if ( mScrollingView )
		AdjustHyperViewBounds();
	CConfigActiveScroller::AdjustScrollBars(); // call inherited AdjustScrollBars(), as mine could call _this_ routine back

	// ¥ only refresh the little box. Refreshing the whole view causes flashes
//	if (HasScrollbars())	
	if ((mHandleGrowIconManually && (mVerticalBar || mHorizontalBar)) || (mVerticalBar && mHorizontalBar))	
		{
		Rect	sizeBox  = CalcDummySizeBox();
		sizeBox.top += mFrameLocation.v;
		sizeBox.bottom += mFrameLocation.v;
		sizeBox.left += mFrameLocation.h;
		sizeBox.right += mFrameLocation.h;
		InvalPortRect( &sizeBox );
		}
}

// Calculates the position of the dummy size box
Rect CHyperScroller::CalcDummySizeBox()
{
	Rect		r;	

	r.bottom = mFrameSize.height;
	r.right = mFrameSize.width;
	r.top = r.bottom - 16;
	r.left = r.right - 16;

	return r;
}

// This is the little box between the scrollers.
// HyperView and windows draw it
void CHyperScroller::DrawDummySizeBox()
{
	// We need to erase that little rect above the grow box
//	if ( mHorizontalBar && mVerticalBar )
	if ((mHandleGrowIconManually && (mVerticalBar || mHorizontalBar)) || (mVerticalBar && mHorizontalBar))
	{
		if ( !FocusDraw() )
			return;
		Rect theDummyFrame = CalcDummySizeBox();

		if ( IsActive() )
		{
			if (!mHandleGrowIconManually)
			{
				SBevelColorDesc theTraits;
				UGraphicGizmos::LoadBevelTraits(1000, theTraits);

				::PmForeColor(theTraits.fillColor);
				::PaintRect(&theDummyFrame);	

				::PmForeColor(eStdGrayBlack);
				::FrameRect(&theDummyFrame);
				::InsetRect(&theDummyFrame, 1, 1);
				UGraphicGizmos::BevelRect(theDummyFrame, 1, theTraits.topBevelColor, theTraits.bottomBevelColor);
			}
			else
			{
				::PlotIconID(&theDummyFrame, atAbsoluteCenter, ttNone, cSizeBoxIconID);
			}
		}
		else
		{
			::PmForeColor(eStdGrayWhite);
			theDummyFrame.top += 1;
			theDummyFrame.left += 1;
			::EraseRect(&theDummyFrame);
		}
	}
}

void CHyperScroller::DrawEmptyScroller( LStdControl* scrollBar )
{
	Rect		frame;

	scrollBar->CalcPortFrameRect( frame );
	PortToLocalPoint( topLeft( frame ) );
	PortToLocalPoint( botRight( frame ) );
	::FrameRect( &frame );
	::InsetRect( &frame, 1, 1 );
	::EraseRect( &frame );
}

// Empty scrollbar drawing copied from LScroller
// Also draws the frame if needed

void CHyperScroller::DrawSelf(void)
{
	StColorPenState::Normalize();

	Rect theFrame;
	CalcLocalFrameRect(theFrame);

	CHTMLView* theHyperView = dynamic_cast<CHTMLView*>(mScrollingView);

	Boolean bIsBorderless = theHyperView->IsBorderless();
	if ((!mExpanded.top) && (!bIsBorderless))
		{
		::MoveTo(theFrame.left, theFrame.top);
		::LineTo(theFrame.right, theFrame.top);
		}
		
	if ((!mExpanded.left) && (!bIsBorderless))
		{
		::MoveTo(theFrame.left, theFrame.top);
		::LineTo(theFrame.left, theFrame.bottom);
		}

	if ((!mExpanded.bottom) && (!bIsBorderless))
		{
		::MoveTo(theFrame.left, theFrame.bottom - 1);
		::LineTo(theFrame.right, theFrame.bottom - 1);
		}	

	if ((!mExpanded.right) && (!bIsBorderless))
		{
		::MoveTo(theFrame.right - 1, theFrame.top);
		::LineTo(theFrame.right - 1, theFrame.bottom - 1);
		}	

	if (!IsActive())
		{
		UGraphics::SetFore(CPrefs::Black);
		UGraphics::SetBack(CPrefs::WindowBkgnd);
	
		if (mVerticalBar)
			DrawEmptyScroller(mVerticalBar);
		if (mHorizontalBar)
			DrawEmptyScroller(mHorizontalBar);
		}

//	if (mVerticalBar && mHorizontalBar)
	if ((mHandleGrowIconManually && (mVerticalBar || mHorizontalBar)) || (mVerticalBar && mHorizontalBar))
		DrawDummySizeBox();
}

// Adjust the position of the scrollbars by 1 pixel, so that they fix well.
// This cannot be done inside the constructor, because -1 offset means that there are no scrollers
void CHyperScroller::FinishCreateSelf()
{
	LScroller::FinishCreateSelf();
	
// FIX ME!!! put back in
//	if (((CHTMLView*)mScrollingView)->GetScrollable() != LO_SCROLL_YES)
//		ShowScrollbars( FALSE, FALSE );

	// Initially there is no content, so we don't need scroll bars
	
	ShowScrollbars(false, false);
}

// 97-05-06 pkc -- override ActivateSelf so that we call draw dummy size box in active state.
void CHyperScroller::ActivateSelf()
{
	inherited::ActivateSelf();

	// 97-06-06 pkc -- also check to see if we're visible
//	if ((mVisible == triState_On) && mVerticalBar && mHorizontalBar)
	if ((mVisible == triState_On) && ((mHandleGrowIconManually && (mVerticalBar || mHorizontalBar)) || (mVerticalBar && mHorizontalBar)))
		DrawDummySizeBox();
}

// 97-05-12 pkc -- override ClickSelf so that we can handle grow icon manually
void CHyperScroller::ClickSelf(const SMouseDownEvent &inMouseDown)
{
	if(mHandleGrowIconManually)
	{
		Rect theDummyFrame = CalcDummySizeBox();
		if (::PtInRect(inMouseDown.whereLocal, &theDummyFrame))
		{
			// Find our LWindow superview and call ClickInGrow
			LView* superView = this;
			LWindow* window;
			do {
				superView = superView->GetSuperView();
				window = dynamic_cast<LWindow*>(superView);
			} while (!window && superView);
			if (window)
				window->ClickInGrow(inMouseDown.macEvent);
		}
	}
	else
		LView::ClickSelf(inMouseDown);
}
