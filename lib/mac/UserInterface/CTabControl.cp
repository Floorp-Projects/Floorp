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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <LArray.h>
#include <LArrayIterator.h>
#include <LDataStream.h>
#include <PP_Messages.h>
#include <UDrawingState.h>
#include <URegions.h>
#include <UResourceMgr.h>
#include <UGWorld.h>
#include <UTextTraits.h>

#include "CTabControl.h"
#include "UStdBevels.h"
#include "UGraphicGizmos.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CTabControl::CTabControl(LStream* inStream)
	:	LControl(inStream),
		mTabs(sizeof(CTabInstance*))
{
	*inStream >> mOrientation;
	*inStream >> mCornerPixels;
	*inStream >> mBevelDepth;
	*inStream >> mSpacing;
	*inStream >> mLeadPixels;
	*inStream >> mRisePixels;
	
	ResIDT theBevelTraitsID;
	*inStream >> theBevelTraitsID;
	UGraphicGizmos::LoadBevelTraits(theBevelTraitsID, mActiveColors);

	*inStream >> theBevelTraitsID;
	UGraphicGizmos::LoadBevelTraits(theBevelTraitsID, mOtherColors);

	*inStream >> mTabDescID;
	*inStream >> mTitleTraitsID;
	mControlMask = ::NewRgn();
	ThrowIfNULL_(mControlMask);

	mCurrentTab = NULL;
	mTrackInside = false;
	mBevelSides.left = true;
	mBevelSides.top = true;
	mBevelSides.right = true;
	mBevelSides.bottom = true;
	mMinimumSize.h = mMinimumSize.v = -1;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CTabControl::~CTabControl()
{
	if (mControlMask != NULL)
		::DisposeRgn(mControlMask);
		
	CTabInstance* theTab;
	LArrayIterator theIter(mTabs, LArrayIterator::from_Start);
	while (theIter.Next(&theTab))
		delete theTab;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::FinishCreateSelf(void)
{
	LControl::FinishCreateSelf();
	
	if (mTabDescID != 0)
		DoLoadTabs(mTabDescID);	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::DoLoadTabs(ResIDT inTabDescResID)
{
	Assert_(inTabDescResID != 0);
	
	CTabInstance* theTab;
	LArrayIterator theIter(mTabs, LArrayIterator::from_Start);
	while (theIter.Next(&theTab))
		delete theTab;

	mTabDescID = inTabDescResID;
	StResource theTabRes(ResType_TabDescList, mTabDescID);
	{
		StHandleLocker theLock(theTabRes);
		LDataStream theTabStream(*theTabRes.mResourceH, ::GetHandleSize(theTabRes));
		Int16 bevelSubCount = theTabStream.GetLength() / sizeof(STabDescriptor);
		for (Int32 index = 0; index < bevelSubCount; index++)
			{
			STabDescriptor theDesc;
			theTabStream.ReadData(&theDesc, sizeof(STabDescriptor));
			CTabInstance* theTab;
			if (theDesc.width == -2) 
				theTab = new CIconTabInstance(theDesc);
			else
				theTab = new CTextTabInstance(theDesc);
			mTabs.InsertItemsAt(1, LArray::index_Last, &theTab);
			}
	}

	Recalc();

// don't call SetMinValue and SetMaxValue as they may do range-checking which may
// not work if the resources haven't set good initial values
// this is ok since we are setting all 3 values at the same time
	mMinValue = 1;
	mMaxValue = mTabs.GetCount();
#if 0
	SetMaxValue(mTabs.GetCount());
	SetMinValue(1);
#endif
	SetValue(mValue);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::SetTabEnable(ResIDT inPageResID, Boolean inEnable)
{
	CTabInstance* theTab;
	LArrayIterator theIter(mTabs, LArrayIterator::from_Start);
	while (theIter.Next(&theTab))
	{
		if (theTab->mMessage == inPageResID)
		{
			if (theTab->mEnabled != inEnable)
			{
				theTab->mEnabled = inEnable;
				FocusDraw();
				DrawSelf();		// because it's not sufficient to DrawOneTab(theTab)
				break;
			}
		}
	}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::Draw(RgnHandle inSuperDrawRgnH)
{
	Rect theFrame;
	if ((mVisible == triState_On) && CalcPortFrameRect(theFrame) && ((inSuperDrawRgnH == nil) ||
			RectInRgn(&theFrame, inSuperDrawRgnH)) && FocusDraw())
		{
		PortToLocalPoint(topLeft(theFrame));	// Get Frame in Local coords
		PortToLocalPoint(botRight(theFrame));
	
		if (ExecuteAttachments(msg_DrawOrPrint, &theFrame))
			{
			Boolean bDidDraw = false;

			StColorPenState thePenSaver;
			StColorPenState::Normalize();
			
			// Fail safe offscreen drawing
			StValueChanger<EDebugAction> okayToFail(UDebugging::gDebugThrow, debugAction_Nothing);
			try
				{			
				LGWorld theOffWorld(theFrame, 0, useTempMem);

				if (!theOffWorld.BeginDrawing())
					throw memFullErr;
					
				DrawSelf();
					
				theOffWorld.EndDrawing();
				theOffWorld.CopyImage(GetMacPort(), theFrame, srcCopy, mControlMask);
				bDidDraw = true;
				}
			catch (...)
				{
				Assert_(false);
				// 	& draw onscreen
				}
				
			if (!bDidDraw)
				DrawSelf();
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::DrawSelf(void)
{
	CTabInstance* theTab;
	LArrayIterator theIter(mTabs, LArrayIterator::from_Start);
	while (theIter.Next(&theTab))
		{
		if (theTab != mCurrentTab)
			// FIX ME!!! we could check to see if the tab's mask intersects
			// the local update region
			DrawOneTab(theTab);
		}

	Rect theControlFrame;
	CalcLocalFrameRect(theControlFrame);
	
	// Need to draw the current tab last otherwise the
	// single pixel black border lines won't get drawn right
	DrawOneTab(mCurrentTab);
	
	SetClipForDrawingSides();
	DrawSides();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::DrawOneTabBackground(
	RgnHandle inRegion,
	Boolean inCurrentTab)
{
	SBevelColorDesc& theTabColors = inCurrentTab ? mActiveColors : mOtherColors;
	Int16 thePaletteIndex = theTabColors.fillColor;
	if (mTrackInside)
		thePaletteIndex--;
	::PmForeColor(thePaletteIndex);
	::PaintRgn(inRegion);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::DrawOneTabFrame(RgnHandle inRegion, Boolean inCurrentTab)
{
	// Draw the tab frame	
	SBevelColorDesc& theTabColors = inCurrentTab ? mActiveColors : mOtherColors;
	Int16 thePaletteIndex = theTabColors.frameColor;
	::PmForeColor(thePaletteIndex);
	::FrameRgn(inRegion);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::DrawCurrentTabSideClip(RgnHandle inRegion)
{
	// Always only for the current tab
	::SetClip(inRegion);
	::PmForeColor(mActiveColors.fillColor);
	::PaintRect(&mSideClipFrame);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


void CTabControl::DrawOneTab(CTabInstance* inTab)
{
	StClipRgnState theClipSaver;

	Rect theControlFrame;
	CalcLocalFrameRect(theControlFrame);

	Boolean isCurrentTab = (inTab == mCurrentTab);
	if (isCurrentTab)
		{
		StRegion theTempMask(inTab->mMask);
		StRegion theSideMask(mSideClipFrame);
		::DiffRgn(theTempMask, theSideMask, theTempMask);
		::SetClip(theTempMask);
		}
	else
		{
		StRegion theTempMask(inTab->mMask);
		::DiffRgn(theTempMask, mCurrentTab->mMask, theTempMask);
		::SetClip(theTempMask);
		}
	
	// This draws the fill pattern in the tab	
	DrawOneTabBackground(inTab->mMask, isCurrentTab);
	// This just calls FrameRgn on inTab->mMask
	DrawOneTabFrame(inTab->mMask, isCurrentTab);

	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	if (isCurrentTab)
		DrawCurrentTabSideClip(inTab->mMask);

	// Draw the title before the bevel because the bevel needs to futz
	// with the clip
	inTab->DrawTitle(mCurrentTab, mTitleTraitsID);

	if (mBevelDepth > 0)
	{

		::PenSize(mBevelDepth, mBevelDepth);

		if (isCurrentTab) {
			// Draw the top bevel		
			RGBColor	theAddColor = {0x4000, 0x4000, 0x4000};
			::RGBForeColor(&theAddColor);
			::OpColor(&UGraphicGizmos::sLighter);
			::PenMode(subPin);
			
			// This runs the pen around the top and left sides
			DrawTopBevel(inTab);
		}

		// Draw the bevel shadow
		StRegion theShadeRgn(inTab->mShadeFrame);
		
		if (inTab != mCurrentTab)
			::DiffRgn(theShadeRgn, mCurrentTab->mMask, theShadeRgn); 

		::SetClip(theShadeRgn);
		// Set up the colors for the bevel shadow
		RGBColor	theSubColor = {0x4000, 0x4000, 0x4000};
		::RGBForeColor(&theSubColor);
		::OpColor(&UGraphicGizmos::sDarker);
		::PenMode(subPin);
		// This runs the pen around the bottom and right sides
		DrawBottomBevel(inTab, isCurrentTab);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::Recalc(void)
{
	FocusDraw();
	::SetEmptyRgn(mControlMask);

	Rect theControlFrame;
	CalcLocalFrameRect(theControlFrame);
	mSideClipFrame = theControlFrame;
	
	Int16 theExtendFactor = mBevelDepth + mRisePixels;
	Point theBasePoint;
	switch (mOrientation)
		{
		case eNorthTab:
			theBasePoint.h = theControlFrame.left + mLeadPixels;
			theBasePoint.v = theControlFrame.bottom;
			mBevelSides.bottom = false;
			mSideClipFrame.top = mSideClipFrame.bottom - theExtendFactor;
			mSideClipFrame.bottom += mBevelDepth;
			::InsetRect(&mSideClipFrame, 1, 0);
			break;
			
		case eEastTab:
			theBasePoint.h = theControlFrame.left;
			theBasePoint.v = theControlFrame.top + mLeadPixels;
			mBevelSides.left = false;
			mSideClipFrame.right = mSideClipFrame.left + theExtendFactor;
			mSideClipFrame.left -= mBevelDepth;
			::InsetRect(&mSideClipFrame, 0, 1);
			break;
						
		case eSouthTab:
			theBasePoint.h = theControlFrame.left + mLeadPixels;
			theBasePoint.v = theControlFrame.top;
			mBevelSides.top = false;
			mSideClipFrame.bottom = mSideClipFrame.top + theExtendFactor;
			mSideClipFrame.top -= mBevelDepth;
			::InsetRect(&mSideClipFrame, 1, 0);
			break;

		case eWestTab:
			theBasePoint.h = theControlFrame.right;
			theBasePoint.v = theControlFrame.top + mLeadPixels;
			mBevelSides.right = false;
			mSideClipFrame.left = mSideClipFrame.right - theExtendFactor;
			mSideClipFrame.right += mBevelDepth;
			::InsetRect(&mSideClipFrame, 0, 1);
			break;
		}

	// Set the Title traits becuase we may need to calculate width based on
	// title dimensions
	UTextTraits::SetPortTextTraits(mTitleTraitsID);

	CTabInstance* theTab;
	LArrayIterator theIter(mTabs, LArrayIterator::from_Start);
	while (theIter.Next(&theTab))
		{
		theTab->mFrame = theControlFrame;

		if (theTab->mMask == NULL)
			theTab->mMask = ::NewRgn();
		ThrowIfNULL_(theTab->mMask);

		Int16 theWidth;
		// Size to title if we are in a norh or south orientation, and the width is flagged appropriately
		if (((mOrientation == eNorthTab) || (mOrientation == eSouthTab)) && (theTab->mWidth == -1))
			{
			theWidth = ::StringWidth(theTab->mTitle) + (4 * ::CharWidth('W'));
			}
		else
			theWidth = theTab->mWidth;
		
		switch (mOrientation)
			{
			case eNorthTab:
				theTab->mFrame.left = theBasePoint.h;
				theTab->mFrame.right = theTab->mFrame.left + theWidth;
				theTab->mFrame.bottom -= mBevelDepth + mRisePixels;
				theBasePoint.h = theTab->mFrame.right + mSpacing;
				theTab->mShadeFrame = theTab->mFrame;
				theTab->mShadeFrame.left = theTab->mShadeFrame.right - (mCornerPixels + mBevelDepth);
				break;
				
			case eEastTab:
				theTab->mFrame.top = theBasePoint.v;
				theTab->mFrame.bottom = theTab->mFrame.top + theWidth; // or in this case height
				theTab->mFrame.left += mBevelDepth + mRisePixels;
				theBasePoint.v = theTab->mFrame.bottom + mSpacing;
				theTab->mShadeFrame = theTab->mFrame;
				theTab->mShadeFrame.top += mCornerPixels + (mBevelDepth - 1);
				break;
							
			case eSouthTab:
				theTab->mFrame.left = theBasePoint.h;
				theTab->mFrame.top += mBevelDepth + mRisePixels;
				theTab->mFrame.right = theTab->mFrame.left + theWidth;
				theBasePoint.h = theTab->mFrame.right + mSpacing;
				theTab->mShadeFrame = theTab->mFrame;
				theTab->mShadeFrame.left += mCornerPixels + (mBevelDepth - 1);
				break;

			case eWestTab:
				theTab->mFrame.top = theBasePoint.v;
				theTab->mFrame.bottom = theTab->mFrame.top + theWidth; // or in this case height
				theTab->mFrame.right -= mBevelDepth + mRisePixels;
				theBasePoint.v = theTab->mFrame.bottom + mSpacing;
				theTab->mShadeFrame = theTab->mFrame;
				theTab->mShadeFrame.top = theTab->mShadeFrame.bottom - (mCornerPixels + mBevelDepth);
				break;
			}

		CalcTabMask(theControlFrame, theTab->mFrame, theTab->mMask);
		::UnionRgn(mControlMask, theTab->mMask, mControlMask);

		// Here we conpensate for the 1 pixel frame.  Since drawing is done below and to
		// the left of a pixel, we need to adjust the right and bottom of the frame so
		// the frame relative drawing will be inside the tab mask region.
		theTab->mFrame.right--;
		theTab->mFrame.bottom--; 
		}
	if ( theTab )
		switch ( mOrientation )	{
			case eNorthTab:
			case eSouthTab:
				mMinimumSize.h = theTab->mFrame.right + 8;
				mMinimumSize.v = theTab->mFrame.bottom - theTab->mFrame.top;
				break;
			case eEastTab:
			case eWestTab:
				mMinimumSize.h = theTab->mFrame.right - theTab->mFrame.left + 8;
				mMinimumSize.v = theTab->mFrame.bottom;
				break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::CalcTabMask(
	const Rect& 	inControlFrame,
	const Rect&		inTabFrame,
	RgnHandle		ioTabMask)
{
	::RectRgn(ioTabMask, &inControlFrame);

	StRegion theTempRgn1, theTempRgn2;
	
	// when we subtract 1 from the tab frame, it's to accomodate for the frame
	// which draws to the left and below the actual pixel
	
	switch (mOrientation)
		{
		case eNorthTab:
			::SetRectRgn(theTempRgn1, inControlFrame.left, inControlFrame.top, inTabFrame.left, inTabFrame.bottom - 1);
			::SetRectRgn(theTempRgn2, inTabFrame.right, inControlFrame.top, inControlFrame.right, inTabFrame.bottom - 1);
			break;
			
		case eEastTab:
			::SetRectRgn(theTempRgn1, inTabFrame.left + 1, inControlFrame.top, inControlFrame.right, inTabFrame.top);
			::SetRectRgn(theTempRgn2, inTabFrame.left + 1, inTabFrame.bottom, inControlFrame.right, inControlFrame.bottom);
			break;
						
		case eSouthTab:
			::SetRectRgn(theTempRgn1, inControlFrame.left, inTabFrame.top + 1, inTabFrame.left, inControlFrame.bottom);
			::SetRectRgn(theTempRgn2, inTabFrame.right, inTabFrame.top + 1, inControlFrame.right, inControlFrame.bottom);
			break;

		case eWestTab:
			::SetRectRgn(theTempRgn1, inControlFrame.left, inControlFrame.top, inTabFrame.right - 1, inTabFrame.top);
			::SetRectRgn(theTempRgn2, inControlFrame.left, inTabFrame.bottom, inTabFrame.right - 1, inControlFrame.bottom);
			break;
		}

	::DiffRgn(ioTabMask, theTempRgn1, ioTabMask);
	::DiffRgn(ioTabMask, theTempRgn2, ioTabMask);

	// Knock off the corners of the tab
	::OpenRgn();
	
	if ((mOrientation == eWestTab) || (mOrientation == eNorthTab))
		{
		::MoveTo(inTabFrame.left, inTabFrame.top);
		::LineTo(inTabFrame.left + mCornerPixels, inTabFrame.top);
		::LineTo(inTabFrame.left, inTabFrame.top + mCornerPixels);
		::LineTo(inTabFrame.left, inTabFrame.top);
		}
		
	if ((mOrientation == eNorthTab) || (mOrientation == eEastTab))
		{
		::MoveTo(inTabFrame.right, inTabFrame.top);
		::LineTo(inTabFrame.right, inTabFrame.top + mCornerPixels);
		::LineTo(inTabFrame.right - (mCornerPixels + 1), inTabFrame.top);
		::LineTo(inTabFrame.right, inTabFrame.top);
		}

	if ((mOrientation == eEastTab) || (mOrientation == eSouthTab))
		{
		::MoveTo(inTabFrame.right, inTabFrame.bottom);
		::LineTo(inTabFrame.right - (mCornerPixels + 1), inTabFrame.bottom);
		::LineTo(inTabFrame.right, inTabFrame.bottom - (mCornerPixels + 1));
		::LineTo(inTabFrame.right, inTabFrame.bottom);
		}

	if ((mOrientation == eSouthTab) || (mOrientation == eWestTab))
		{
		::MoveTo(inTabFrame.left, inTabFrame.bottom);
		::LineTo(inTabFrame.left, inTabFrame.bottom - mCornerPixels);
		::LineTo(inTabFrame.left + mCornerPixels, inTabFrame.bottom);
		::LineTo(inTabFrame.left, inTabFrame.bottom);
		}
		
	::CloseRgn(theTempRgn1);
	::DiffRgn(ioTabMask, theTempRgn1, ioTabMask);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::DrawTopBevel(CTabInstance* inTab)
{
	const Rect& theTabFrame = inTab->mFrame;
	switch (mOrientation)
		{
		case eNorthTab:
			{
			::MoveTo(theTabFrame.left + 1, theTabFrame.bottom + 1);
			::LineTo(theTabFrame.left + 1, theTabFrame.top + mCornerPixels);
			::LineTo(theTabFrame.left + mCornerPixels, theTabFrame.top + 1);
			::LineTo(theTabFrame.right - (mCornerPixels + mBevelDepth) + 1, theTabFrame.top + 1);
			}
			break;

		case eEastTab:
			{
			// I'm not sure whether this is quite right...
			::MoveTo(theTabFrame.left - 1, theTabFrame.top + 1);
			::LineTo(theTabFrame.right - (mCornerPixels + mBevelDepth) + 1, theTabFrame.top + 1);
			::LineTo(theTabFrame.right - mBevelDepth, theTabFrame.top + mCornerPixels);
			}
			break;
			
		case eSouthTab:
			{
			// I'm not sure whether this is quite right...
			::MoveTo(theTabFrame.left + 1, theTabFrame.top - 1);
			::LineTo(theTabFrame.left + 1, theTabFrame.bottom - (mCornerPixels + mBevelDepth) + 1);
			::LineTo(theTabFrame.left + mCornerPixels, theTabFrame.bottom - mBevelDepth);
			}
			break;
		
		case eWestTab:
			{
			::MoveTo(theTabFrame.right + 1, theTabFrame.top + 1);
			::LineTo(theTabFrame.left + mCornerPixels, theTabFrame.top + 1);
			::LineTo(theTabFrame.left + 1, theTabFrame.top + mCornerPixels);
			::LineTo(theTabFrame.left + 1, theTabFrame.bottom - (mCornerPixels + mBevelDepth) + 1);
			}
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawBottomBevel
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::DrawBottomBevel(CTabInstance* inTab, Boolean	/* inCurrentTab */)
{
	const Rect& theTabFrame = inTab->mFrame;
	switch (mOrientation)
		{
		case eNorthTab:
			{
			::MoveTo(theTabFrame.right - mBevelDepth, theTabFrame.top + mCornerPixels);
			::LineTo(theTabFrame.right - mBevelDepth, theTabFrame.bottom + 1);
			}
			break;

		case eEastTab:
			{
			// I'm not sure whether this is quite right...
			::MoveTo(theTabFrame.right - mBevelDepth, theTabFrame.top + mCornerPixels);			
			::LineTo(theTabFrame.right - mBevelDepth, theTabFrame.bottom - (mCornerPixels + mBevelDepth) + 1);
			::LineTo(theTabFrame.right - (mCornerPixels + mBevelDepth) + 1, theTabFrame.bottom - mBevelDepth);
			::LineTo(theTabFrame.left - 1, theTabFrame.bottom - mBevelDepth);
			}
			break;
			
		case eSouthTab:
			{
			// I'm not sure whether this is quite right...
			::MoveTo(theTabFrame.left + mCornerPixels, theTabFrame.bottom - mBevelDepth);
			::LineTo(theTabFrame.right - (mCornerPixels + mBevelDepth) + 1, theTabFrame.bottom - mBevelDepth);
			::LineTo(theTabFrame.right - mBevelDepth, theTabFrame.bottom - (mCornerPixels + mBevelDepth) + 1);
			::LineTo(theTabFrame.right - mBevelDepth, theTabFrame.top - 1);
			}
			break;
		
		case eWestTab:
			{
			::MoveTo(theTabFrame.left + mCornerPixels, theTabFrame.bottom - mBevelDepth);
			::LineTo(theTabFrame.right + 1, theTabFrame.bottom - mBevelDepth);
			}
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CTabControl::SetClipForDrawingSides(void)
{
	Assert_(mCurrentTab != NULL);

	Rect theTempTabFrame = mCurrentTab->mFrame;
	Int16 theExtendFactor = mBevelDepth + mRisePixels;
	
	switch (mOrientation)
		{
		case eNorthTab:
			theTempTabFrame.bottom += theExtendFactor;
			theTempTabFrame.right -= mBevelDepth;
			theTempTabFrame.left += mBevelDepth + 1;
			break;

		case eEastTab:
			theTempTabFrame.left -= theExtendFactor;
			theTempTabFrame.top += mBevelDepth + 1;
			theTempTabFrame.bottom -= mBevelDepth;
			break;
			
		case eSouthTab:
			theTempTabFrame.top -= theExtendFactor;
			theTempTabFrame.right -= mBevelDepth;
			theTempTabFrame.left += mBevelDepth + 1;
			break;
		
		case eWestTab:
			theTempTabFrame.right += theExtendFactor;
			theTempTabFrame.top += mBevelDepth + 1;
			theTempTabFrame.bottom -= mBevelDepth;
			break;
		
		}

	StRegion theTabFrameMask(theTempTabFrame);
	StRegion theSideClipMask(mSideClipFrame);
	::DiffRgn(theSideClipMask, theTabFrameMask, theSideClipMask);
	::SectRgn(theSideClipMask, mCurrentTab->mMask, theSideClipMask);
	::SetClip(theSideClipMask);	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CTabControl::DrawSides(void)
{
	UGraphicGizmos::BevelTintRect(mSideClipFrame, mBevelDepth, 0x4000, 0x4000);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 CTabControl::FindHotSpot(Point inPoint) const
{
	Assert_(mCurrentTab != NULL);
	
	// Since the regions overlap, check the currently selected tab's region
	// before checking the others.
	Int16 theHotSpot = 0;
	
	if (::PtInRgn(inPoint, mCurrentTab->mMask))
		{
		theHotSpot = mValue;
		}
	else
		{
		CTabInstance* theTab;
		LArrayIterator theIter(mTabs, LArrayIterator::from_Start);
		while (theIter.Next(&theTab))
			{
			if (theTab == mCurrentTab)
				continue;
				
			if (! theTab->mEnabled)
				{
				::SysBeep(1);
				break;
				}
				
			if (::PtInRgn(inPoint, theTab->mMask))
				{
				theHotSpot = mTabs.FetchIndexOf(&theTab);
				break;
				}
			}
		}
		
	return theHotSpot;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CTabControl::PointInHotSpot(Point inPoint, Int16 inHotSpot) const
{
	Assert_(mCurrentTab != NULL);
	Boolean bInHotSpot = false;
	// If the hot spot that we're checking is not the current tab
	// then the tab is behind the current and is only partially
	// exposed.  We need to mask out the hidden area.
	if ((GetValue() != inHotSpot))
		{
		CTabInstance* theTab;
		mTabs.FetchItemAt(inHotSpot, &theTab);
		
		StRegion theTempMask(theTab->mMask);
		::DiffRgn(theTempMask, mCurrentTab->mMask, theTempMask);
		bInHotSpot = ::PtInRgn(inPoint, theTempMask);
		}
	else
		bInHotSpot = ::PtInRgn(inPoint, mCurrentTab->mMask);

	return bInHotSpot;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::HotSpotAction(
	Int16			inHotSpot,
	Boolean 		inCurrInside,
	Boolean			inPrevInside)
{
	Assert_(mCurrentTab != NULL);

	// We only want to draw track feedback if the tab is not currently selected
	if ((GetValue() != inHotSpot) && (inCurrInside != inPrevInside))
	{
		mTrackInside = inCurrInside;
		
		CTabInstance* theTab;
		mTabs.FetchItemAt(inHotSpot, &theTab);
		// This is not the proper way to draw feed back
		// 1) it flickers badly. If you hold down the mouse button on a control you can see
		//		 the lines that make up the fram disappear;
		// 2) If you click on a tab and then move the mouse out of the tab it doesn't
		// 		unhighlight the tab.
	 	// DrawOneTab( theTab );
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
	
void CTabControl::HotSpotResult(Int16 inHotSpot)
{
	// We got here because we successfully tracked,
	// therefore changing value will undo tab hilighting
	mTrackInside = false;
	SetValue(inHotSpot);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::SetValue(Int32 inValue)
{
	if ((inValue != mValue) ||  (mCurrentTab == NULL))
		{
		if (inValue < mMinValue) {		// Enforce min/max range
			inValue = mMinValue;
		} else if (inValue > mMaxValue) {
			inValue = mMaxValue;
		}
		mTabs.FetchItemAt(inValue, &mCurrentTab);
		SetValueMessage(mCurrentTab->mMessage);

		// setting the value broadcasts msg_TabSwitched with the tab's
		// message in the ioParam
		LControl::SetValue(inValue);  

		Draw(NULL);		// Draws offscreen if possible
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

MessageT CTabControl::GetMessageForValue(Int32 inValue)
{
	MessageT theMessage = msg_Nothing;
	if ((inValue >= mMinValue) && (inValue <= mMaxValue))
		{
		CTabInstance* theTab;
		mTabs.FetchItemAt(inValue, &theTab);
		theMessage = theTab->mMessage;
		}
		
	return theMessage;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::BroadcastValueMessage()
{
	if (mValueMessage != cmd_Nothing)
		{
		CTabInstance* theTab;		
		mTabs.FetchItemAt(mValue, &theTab);
		BroadcastMessage(msg_TabSwitched, &mValueMessage);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::ResizeFrameBy(
	Int16 			inWidthDelta,
	Int16			inHeightDelta,
	Boolean			inRefresh)
{
	LControl::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	Recalc();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
		
void CTabControl::ActivateSelf(void)
{
	FocusDraw();
	Draw(NULL);
	
	::ValidRgn(mControlMask);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabControl::DeactivateSelf(void)
{
	FocusDraw();
	Draw(NULL);
	
	::ValidRgn(mControlMask);
}
		
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CTabInstance::CTabInstance()
{
	mMask = NULL;
	mEnabled = true;
}

CTextTabInstance::CTextTabInstance(const STabDescriptor& inDesc)
{
	mMessage = inDesc.valueMessage;
	mWidth = inDesc.width;
	mTitle = inDesc.title;

	mMask = NULL;
}

CTabInstance::~CTabInstance()
{
	if (mMask != NULL)
		::DisposeRgn(mMask);
}

void CTextTabInstance::DrawTitle(CTabInstance* inCurrentTab, ResIDT inTitleTraitsID)
{
	if (mTitle.Length() >0)
	{
		StColorPenState::Normalize();

		TextTraitsH	theTraitsHandle = UTextTraits::LoadTextTraits(inTitleTraitsID);
		Rect theFrame = mFrame;

		if (this == inCurrentTab)
		{
			(**theTraitsHandle).style |= bold;
			::OffsetRect(&theFrame, 0, 1);
		}
		
		if (! mEnabled)
			(**theTraitsHandle).mode |= grayishTextOr;

		{
			StHandleLocker theLocker((Handle)theTraitsHandle);
			UTextTraits::SetPortTextTraits(*theTraitsHandle);
			UGraphicGizmos::CenterStringInRect(mTitle, theFrame);
		}
		
		::ReleaseResource((Handle)theTraitsHandle);
	}
}


CIconTabInstance::CIconTabInstance(const STabDescriptor& inDesc)
{
	mMessage = inDesc.valueMessage;
	mWidth = 24; // Really should calc the size of the icon
	::StringToNum(inDesc.title, &mIconID);

	mMask = NULL;
}

void CIconTabInstance::DrawTitle(CTabInstance* inCurrentTab, ResIDT /*inTitleTraitsID*/)
{
	StColorPenState::Normalize();

	Rect theFrame = mFrame;
	theFrame.left += 4;//	And you thought the Instance was a hack
	theFrame.right = theFrame.left + 16;
	theFrame.top += 4;
	theFrame.bottom = theFrame.top + 16;
	IconTransformType transform = kTransformNone;
	if (this == inCurrentTab)
	{
		::OffsetRect(&theFrame, 0, 1);
	}	
	if (! mEnabled)
		transform |= kTransformDisabled;

	::PlotIconID(&theFrame, kAlignAbsoluteCenter, transform, mIconID);
}


