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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CBevelView.h"

#include <LStream.h>
#include <UDrawingState.h>
#include <UDrawingUtils.h>
#include <UResourceMgr.h>
#include <LArrayIterator.h>
#include <URegions.h>

	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CBevelView::CBevelView(LStream *inStream)
	:	LView(inStream),
		mBevelList(sizeof(SSubBevel)),
		mBevelGrowIcon(true)
{
	*inStream >> mMainBevel;
	
	ResIDT theBevelResID;
	*inStream >> theBevelResID;
	
	if (theBevelResID != 0)
		{
		StResource theBevelRes(ResType_BevelDescList, theBevelResID);

			{
				StHandleLocker theLock(theBevelRes);
				LDataStream theBevelStream(*theBevelRes.mResourceH, ::GetHandleSize(theBevelRes));
					
				SSubBevel theDesc;
				::SetRect(&theDesc.cachedLocalFrame, 0, 0, 0, 0);			
				while (!theBevelStream.AtEnd())
					{
					theBevelStream >> theDesc.paneID;
					theBevelStream >> theDesc.bevelLevel;
					mBevelList.InsertItemsAt(1, LArray::index_Last, &theDesc);
					}
			}
		}
							
	mNeedsRecalc = true;
	SetRefreshAllWhenResized(false);
}

CBevelView::~CBevelView()
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBevelView::FinishCreateSelf(void)
{
	CalcBevelRegion();
	CalcBevelMask();
}

void CBevelView::ResizeFrameBy(
	Int16			inWidthDelta,
	Int16			inHeightDelta,
	Boolean			inRefresh)
{

	if ((inWidthDelta == 0) && (inHeightDelta == 0)) return;	// Don't need any changes

	if (inRefresh)
		{
		// Force an update on the region that represents the main bevel area and
		// subframes
		InvalMainFrame();
		InvalSubFrames();

		// We need to flag the resize because we know we're going to redraw.  If we
		// don't, the gray area will get drawing in it's pre-resize shape.
		if (inRefresh)
			mNeedsRecalc = true;

		LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
		CalcBevelRegion();
		CalcBevelMask();


		// Force an update on the region that represents the main bevel area and
		// subframes
		InvalMainFrame();
		InvalSubFrames();
		}
	else
		{
		LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
		CalcBevelRegion();
		CalcBevelMask();
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBevelView::DrawSelf(void)
{
	if (mNeedsRecalc)
		CalcBevelRegion();
		
	StColorPenState theState;
	theState.Normalize();
		
	StClipRgnState theClipSaver(mBevelRegion);
	
	DrawBeveledFill();
	DrawBeveledFrame();

	LArrayIterator iterator(mBevelList, LArrayIterator::from_Start);
	SSubBevel theBevelDesc;
	while (iterator.Next(&theBevelDesc))
		{
		Boolean visible = CalcSubpaneLocalRect(theBevelDesc.paneID, theBevelDesc.cachedLocalFrame);
		if (visible)
			DrawBeveledSub(theBevelDesc);
		}		
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBevelView::CalcSubpaneLocalRect(PaneIDT inPaneID, Rect& subFrame)
{
	// Handle the growbox case first, as it's not really a pane at all.
	if (inPaneID == '$Grw')
	{
		subFrame = (*static_cast<RgnHandle>(mBevelRegion))->rgnBBox;
		subFrame.top = subFrame.bottom - 15;
		subFrame.left = subFrame.right - 15;
		// return true (means visible) if mBevelGrowIcon is true
		return mBevelGrowIcon;
	}
	LPane *theSub = FindPaneByID(inPaneID);
	if (theSub && theSub->IsVisible())
	{
		theSub->CalcPortFrameRect(subFrame);
		PortToLocalPoint(topLeft(subFrame));
		PortToLocalPoint(botRight(subFrame));
		return true;
	}
	return false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CBevelView::CalcBevelRegion(void)
{
	// If we can't focus, we cant establish a coordinate scheme and make
	// subpane position calculations.  So leave the recalc flag set.
	if (!FocusExposed(false))
		{
		mNeedsRecalc = true;
		return;
		}

	StRegion theSavedGrayRgn(mBevelRegion);
		
	// Get the current frame of the gray bevel view.
	// and convert the rectangle to a region
	Rect grayFrame;
	CalcLocalFrameRect(grayFrame);
	::RectRgn(mBevelRegion, &grayFrame);

	// Rip through the sub-panes to define the region that will
	// be gray when the gray bevel view is drawn.
	mBevelList.Lock();
	StRegion theUtilRgn;
	for (ArrayIndexT theIndex = LArray::index_First; theIndex <= mBevelList.GetCount(); theIndex++)
	{
		SSubBevel* theBevelDesc = (SSubBevel*)mBevelList.GetItemPtr(theIndex);
		Boolean visible = CalcSubpaneLocalRect(theBevelDesc->paneID, theBevelDesc->cachedLocalFrame);
		if (visible)
		{
			::RectRgn(theUtilRgn, &theBevelDesc->cachedLocalFrame);
			::DiffRgn(mBevelRegion, theUtilRgn, mBevelRegion);		
		}
			
		// if the pane was not found we are assuming that we don't want to
		// clear the gray area underneath that pane.
	}
		
	mBevelList.Unlock();
	mNeedsRecalc = false;

	// Calculate the difference between the old an new gray
	// areas and invalidate the newly exposed stuff.	
//	::DiffRgn(theSavedGrayRgn, mBevelRegion, theSavedGrayRgn);
//	InvalPortRgn(theSavedGrayRgn);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBevelView::InvalMainFrame(void)
{
	Rect theFrame;
	CalcPortFrameRect(theFrame);

	StRegion theInvalidRgn(theFrame);
	::InsetRect(&theFrame, mMainBevel, mMainBevel);
	
	StRegion theInsetRgn(theFrame);
	::DiffRgn(theInvalidRgn, theInsetRgn, theInvalidRgn);

	InvalPortRgn(theInvalidRgn);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBevelView::InvalSubFrames(void)
{
	StRegion theInvalidRgn, theFrameRgn, theInsetRgn;
	::SetEmptyRgn(theInvalidRgn);
	
	LArrayIterator iterator(mBevelList, LArrayIterator::from_Start);
	SSubBevel theBevelDesc;
	while (iterator.Next(&theBevelDesc))
	{
		Rect theSubFrame = theBevelDesc.cachedLocalFrame;
		::RectRgn(theFrameRgn, &theSubFrame);

		Int16 theInsetLevel = theBevelDesc.bevelLevel;

		if (theInsetLevel < 0)
			theInsetLevel = -theInsetLevel;
			
		::InsetRect(&theSubFrame, -(theInsetLevel), -(theInsetLevel));
		::RectRgn(theInsetRgn, &theSubFrame);
		
		::DiffRgn(theInsetRgn, theFrameRgn, theFrameRgn);


		::UnionRgn(theInvalidRgn, theFrameRgn, theInvalidRgn);
	}

	// Convert region to port coordinates
	
	Point offset = { 0, 0 };
	LocalToPortPoint(offset);
	::OffsetRgn(theInvalidRgn, offset.h, offset.v);

	InvalPortRgn(theInvalidRgn);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBevelView::CalcBevelMask(void)
{
	::SetEmptyRgn(mBevelMaskRegion);
	StRegion theFrameRgn, theInsetRgn;
	
	LArrayIterator iterator(mBevelList, LArrayIterator::from_Start);
	SSubBevel theBevelDesc;
	while (iterator.Next(&theBevelDesc))
	{
		Rect theSubFrame;
		if ( CalcSubpaneLocalRect(theBevelDesc.paneID, theSubFrame) ) {
			::RectRgn(theFrameRgn, &theSubFrame);

			Int16 theInsetLevel = theBevelDesc.bevelLevel;

			if (theInsetLevel < 0)
				theInsetLevel = -theInsetLevel;
			
			::InsetRect(&theSubFrame, -(theInsetLevel), -(theInsetLevel));
			::RectRgn(theInsetRgn, &theSubFrame);
		
			::DiffRgn(theInsetRgn, theFrameRgn, theFrameRgn);


			::UnionRgn(mBevelMaskRegion, theFrameRgn, mBevelMaskRegion);
		}
	}
	
	// Convert region to port coordinates
	Point offset = { 0, 0 };
	LocalToPortPoint(offset);
	::OffsetRgn(mBevelMaskRegion, offset.h, offset.v);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
//
//	This method should be called anytime beveled subpanes are changed directly (i.e.
// 	calling the pane's method directly, such as ResizeFrameBy(), MoveBy(), etc.)
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBevelView::SubPanesChanged(Boolean inRefresh)
{
	// Invalidate old bevel mask
	
	if (inRefresh)
		InvalPortRgn(mBevelMaskRegion);
	
	// Calculate new masks

	CalcBevelRegion();
	CalcBevelMask();
	
	// Invalidate new bevel mask

	if (inRefresh)
		InvalPortRgn(mBevelMaskRegion);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥													[ static ]
//
//	This is a safe, but ugly way of telling any enclosing GrayBevelView
//	that the panes have moved.  The subPane is the starting point for an
//	upwards search.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBevelView::SubPanesChanged(LPane* inSubPane, Boolean inRefresh)
{
	LPane* super = inSubPane;
	do {
		super = super->GetSuperView();
		if (!super) break;
		CBevelView* graySuper = dynamic_cast<CBevelView*>(super);
		if (graySuper)
		{
			graySuper->SubPanesChanged(inRefresh);
			break;
		}
	} while (true);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
//
//	This method should be called to turn on bevelling of the grow icon area
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CBevelView::BevelGrowIcon()
{
	mBevelGrowIcon = true;
	SubPanesChanged();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
//
//	This method should be called to stop us from bevelling the grow icon area
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CBevelView::DontBevelGrowIcon()
{
	mBevelGrowIcon = false;
	SubPanesChanged();
}