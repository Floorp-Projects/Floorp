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
//	CSpinningN.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include <UResourceMgr.h>
#include "CSpinningN.h"
#include "UGraphicGizmos.h"
#include "UGAAppearance.h"
#include "CSharedPatternWorld.h"
#include "macutil.h"
#include "CNSContext.h"	// for the broadcast messages
#include "uapp.h"
#include "resgui.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Internal Constants
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

const Int32 	kSpinInterval 			= 	10;
const ResIDT	ResID_LargeSpinIconBase =	700;
const ResIDT	ResID_SmallSpinIconBase =	800;
const ResIDT	ResID_LargeCicnBase 	=	700;
const ResIDT	ResID_SmallCicnBase 	=	128;
short CSpinningN::mAnimResFile			=	-1;		// refNum_Undefined

const Int16		cLargeIconHeight		=	30;
const Int16		cLargeIconWidth			=	30;
const Int16		cSmallIconHeight		=	16;
const Int16		cSmallIconWidth			=	16;
const Int16		cButtonBevelBorderSize	=	8;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CSpinningN::CSpinningN(LStream *inStream)
	:	mAdornment(nil),
		mFill(nil),
		
		CPatternButton(inStream)
{
	mSpinCounter = 0;		// 0 indicates the icon is not spinning
	mIsCoBranded = false;
	mSmallSeqCount = 0;
	mLargeSeqCount = 0;
	mRefCount = 0;

	mHasLargeCoBrand = true;
	mHasSmallCoBrand = true;

	mFinishedCreatingSelf = false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::FinishCreateSelf(void)
{
	CPatternButton::FinishCreateSelf();
	
	CalcAnimationMode();
	RepositionSelf();
	mFinishedCreatingSelf = true;
	
	LView* theSuperView = GetSuperView();
	if (theSuperView)
	{
		mAdornment	= theSuperView->FindPaneByID(kAdornmentPaneID);
		mFill		= theSuperView->FindPaneByID(kFillPaneID);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
	
void CSpinningN::ListenToMessage(
	MessageT			inMessage,
	void*				/* ioParam */)
{
	switch (inMessage)
		{
		case msg_NSCStartLoadURL:
			StartRepeating();
			break;
			
		case msg_NSCAllConnectionsComplete:
			StopRepeating();
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::AdaptToSuperFrameSize(
	Int32				inSurrWidthDelta,
	Int32				inSurrHeightDelta,
	Boolean 			inRefresh)
{
	LPane::AdaptToSuperFrameSize(inSurrWidthDelta, inSurrHeightDelta, inRefresh);

//	1997-03-22 pkc
//	Put in hack to work around problem below. We can't comment out call
//	to RepositionSelf because then the spinning 'n' won't resize itself when
//	the toolbar containing it changes its size.
	if (mFinishedCreatingSelf)
		RepositionSelf();
//		Don't do that here: AdaptToSuperFrameSize() can be called before
//		FinishCreateSelf() and for some reason, the resource file may
//		not be positionned correctly at that time.
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::StartRepeating(void)
{
	if (mRefCount == 0)
	{
		mSpinCounter = 1;		// > 0 means that we're spinning
		mLastSpinTicks = ::TickCount();
	
		LPeriodical::StartRepeating();
	}
	++mRefCount;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::StopRepeating()
{
	if (mRefCount <= 1)
	{
		LPeriodical::StopRepeating();
		
		mSpinCounter = 0;		// 0 indicates the icon is not spinning
		mLastSpinTicks = ::TickCount();
		
		if (IsVisible() && FocusDraw())
			Draw(NULL);
	}

	--mRefCount;
	if (mRefCount < 0)
		mRefCount = 0;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// BEWARE: this method bypasses the mRefCount counter in order to force
// the icon to stop spinning. It's useful in several places where we do
// know for sure that all background activity has stopped.
void CSpinningN::StopSpinningNow()
{
	mRefCount = 1;
	StopRepeating();
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::SpendTime(const EventRecord& /* inMacEvent */)
{
	if ((::TickCount() - mLastSpinTicks) < kSpinInterval) 
		return;
	
	mLastSpinTicks = ::TickCount();

	mSpinCounter = (mSpinCounter + 1) % mIconCount;
	if (mSpinCounter == 0) // skip zero
		mSpinCounter++;
		
	if (IsVisible() && FocusDraw())
		Draw(NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::DrawSelf(void)
{
	PrepareDrawButton();

	DrawButtonContent();
	
	DrawButtonGraphic();			
			
	FinalizeDrawButton();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawButtonContent
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
	
void CSpinningN::DrawButtonContent(void)
{
	CGrafPtr thePort;
	::GetPort(&(GrafPtr)thePort);
	
	Rect theFrame = mCachedButtonFrame;
	Point theAlignment;
	
	CalcOrientationPoint(theAlignment);
	mPatternWorld->Fill(thePort, theFrame, theAlignment);

	::InsetRect(&theFrame, 2, 2);

	if (IsActive() && IsEnabled())
		{
		if (IsTrackInside() || (!IsBehaviourButton() && (mValue == Button_On)))
			{
			DrawButtonHilited();
			}
		else
			{
			DrawButtonNormal();
			}
		}
	else
		{
		DrawSelfDisabled();
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawSelfDisabled
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
	
void CSpinningN::DrawSelfDisabled(void)
{
	UGAAppearance::DrawGAButtonDimmedBevelTint(mCachedButtonFrame);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawButtonGraphic
//
//	the first cicn's or icl8's in each of the large and small sequences should
//	be preloaded and locked.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::DrawButtonGraphic(void)
{
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	
	UGraphicGizmos::CenterRectOnRect(mCachedIconFrame, theFrame);

	if (IsTrackInside() || (GetValue() == Button_On))
		::OffsetRect(&mCachedIconFrame, 1, 1);

	if (mIsCoBranded)
		{
		StUseResFile rf(mAnimResFile);
		CIconHandle theHandle = ::GetCIcon(mGraphicID + mSpinCounter);
		if (theHandle == NULL)
			theHandle = ::GetCIcon(mGraphicID);
	
		if (theHandle != NULL)	
			{
			::PlotCIcon(&mCachedIconFrame, theHandle);
			::DisposeCIcon(theHandle);
			}
		}
	else
		{
//		UGraphicGizmos::CenterRectOnRect(mCachedIconFrame, theFrame);
		OSErr theErr = ::PlotIconID(&mCachedIconFrame, atNone, ttNone, mGraphicID + mSpinCounter);
		if (theErr != noErr)
			::PlotIconID(&mCachedIconFrame, atNone, ttNone, mGraphicID);
		}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
//  For E-Kit compatability continue to support cicn resources as well
//	as icl icons. E-Kit must also allow for a variable number of frames
//	and non-default icon dimensions.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::CalcAnimationMode(void)
{
	StResLoad theResLoad(false);
	StUseResFile rf(mAnimResFile);
	
	Handle theSequence = ::Get1Resource('cicn', ResID_LargeCicnBase + mLargeSeqCount);
	while (theSequence != NULL)
		{
		mLargeSeqCount++;
		theSequence = ::Get1Resource('cicn', ResID_LargeCicnBase + mLargeSeqCount);
		}
		
	theSequence = ::Get1Resource('cicn', ResID_SmallCicnBase + mSmallSeqCount);
	while (theSequence != NULL)
		{
		mSmallSeqCount++;
		theSequence = ::Get1Resource('cicn', ResID_SmallCicnBase + mSmallSeqCount);
		}

	mHasSmallCoBrand = mSmallSeqCount > 0;
	mHasLargeCoBrand = mLargeSeqCount > 0;

	if (!mHasSmallCoBrand) {
		mSmallSeqCount = kMaxSpinStages;
	}

	if (!mHasLargeCoBrand) {
		mLargeSeqCount = kMaxSpinStages;
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ChangeMode
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CSpinningN::ChangeMode(Int8 inNewMode, SDimension16& outDimensionDeltas)
{
	SDimension16 oldDimensions, frameSize;
	GetFrameSize(oldDimensions);

	outDimensionDeltas.width = 0;
	outDimensionDeltas.height = 0;

	mCurrentMode = inNewMode;

	switch (inNewMode)
	{
		case eMode_IconsOnly:
		case eMode_TextOnly:
			GetSmallIconFrameSize(frameSize);
			break;
		
		case eMode_IconsAndText:
			GetLargeIconFrameSize(frameSize);
			break;
	}

	outDimensionDeltas.width = frameSize.width - oldDimensions.width;
	outDimensionDeltas.height = frameSize.height - oldDimensions.height;
	ResizeFrameBy(outDimensionDeltas.width, outDimensionDeltas.height, true);
	return true;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CSpinningN::RepositionSelf(SDimension16* inNewToolbarSize)
{
	SDimension16	theSuperFrameSize;
	
	Assert_(GetSuperView() != NULL);
	if (!GetSuperView())
		return;
	
	if (!inNewToolbarSize)
		GetSuperView()->GetFrameSize(theSuperFrameSize);
	else
	{
		theSuperFrameSize.height = inNewToolbarSize->height;
		theSuperFrameSize.width = inNewToolbarSize->width;
	}
	
	Int16 theHeight, theWidth;
	SDimension16 frameSize;
	// Initalize frameSize to default value just in case GetLargeIconFrameSize fails
	frameSize.height = frameSize.width = cLargeIconHeight + cButtonBevelBorderSize;
	GetLargeIconFrameSize(frameSize);
	
	if (theSuperFrameSize.height >= frameSize.height)
		{
		mIconCount = mLargeSeqCount;
		mIsCoBranded = mHasLargeCoBrand;
		
		if (mIsCoBranded)
			{
			mGraphicID = ResID_LargeCicnBase;
			}
		else
			{
			mGraphicID = ResID_LargeSpinIconBase;
			theHeight = theWidth = cLargeIconHeight;
			::SetRect(&mCachedIconFrame, 0, 0, 32, 32);
			}
		}
	else
		{
		mIconCount = mSmallSeqCount;
		mIsCoBranded = mHasSmallCoBrand;
		
		if (mIsCoBranded)
			{
			mGraphicID = ResID_SmallCicnBase;
			}
		else
			{
			mGraphicID = ResID_SmallSpinIconBase;
			theHeight = theWidth = cSmallIconHeight;
			::SetRect(&mCachedIconFrame, 0, 0, 32, 32);
			}
		}

	if (mIsCoBranded)
		{
		StUseResFile rf(mAnimResFile);
		CIconHandle theIcon = ::GetCIcon(mGraphicID);
		if (theIcon != NULL)
			{
			theHeight = RectHeight((*theIcon)->iconPMap.bounds);
			theWidth = RectWidth((*theIcon)->iconPMap.bounds);
			::SetRect(&mCachedIconFrame, 0, 0, theWidth, theHeight);
			::DisposeCIcon(theIcon);
			}
		}

	if (mSpinCounter >= mIconCount)
		mSpinCounter = 1;


	// Adjust the position and size of this pane (center vertically)
	
	SInt32	h;
	SInt32	v;
	
//	SDimension16 frameSize;
	GetFrameSize(frameSize);
	
//	v = (theSuperFrameSize.height - theHeight) / 2;
//	h = theSuperFrameSize.width - theWidth - v - 1;
//	PlaceInSuperFrameAt(h, v, false);
//	ResizeFrameTo(theWidth, theHeight, true);

	// Adjust theWidth and theHeight to take into account
	// button bevel
	
	theWidth += 8;
	theHeight += 8;

	v = (theSuperFrameSize.height - theHeight) / 2;
	h = theSuperFrameSize.width - theWidth - v - 1;
	PlaceInSuperFrameAt(h, v, false);
	ResizeFrameTo(theWidth, theHeight, true);

/*
	// Adjust the adornment pane
	
	if (theSuperFrameSize.height <= theHeight || theHeight < 40)
	{
		if (mAdornment)
			mAdornment->Hide();
	}
	else
	{
		if (mAdornment)
		{
			mAdornment->PlaceInSuperFrameAt(h - 1, v - 1, false);
			mAdornment->ResizeFrameTo(theWidth + 2, theHeight + 2, true);
			mAdornment->Show();
		}
	}
	
	// Adjust the position and size of the fill pane
	
	if (mFill)
	{
		h = theSuperFrameSize.width - theWidth - v - 1;
		mFill->PlaceInSuperFrameAt(h, 1, false);
		mFill->ResizeFrameTo(theSuperFrameSize.width - h, theSuperFrameSize.height - 2, true);
	}
*/
}

void CSpinningN::GetLargeIconFrameSize(SDimension16& outFrameSize)
{
	if (mHasLargeCoBrand)
	{
		StUseResFile rf(mAnimResFile);
		CIconHandle theIcon = ::GetCIcon(ResID_LargeCicnBase);
		if (theIcon != NULL)
			{
			outFrameSize.height = RectHeight((*theIcon)->iconPMap.bounds) + cButtonBevelBorderSize;
			outFrameSize.width = RectWidth((*theIcon)->iconPMap.bounds) + cButtonBevelBorderSize;
			::DisposeCIcon(theIcon);
			}
	}
	else
	{
		outFrameSize.height = cLargeIconHeight + cButtonBevelBorderSize;
		outFrameSize.width = cLargeIconWidth + cButtonBevelBorderSize;
	}
}

void CSpinningN::GetSmallIconFrameSize(SDimension16& outFrameSize)
{
	if (mHasSmallCoBrand)
	{
		StUseResFile rf(mAnimResFile);
		CIconHandle theIcon = ::GetCIcon(ResID_SmallCicnBase);
		if (theIcon != NULL)
			{
			outFrameSize.height = RectHeight((*theIcon)->iconPMap.bounds) + cButtonBevelBorderSize;
			outFrameSize.width = RectWidth((*theIcon)->iconPMap.bounds) + cButtonBevelBorderSize;
			::DisposeCIcon(theIcon);
			}
	}
	else
	{
		outFrameSize.height = cSmallIconHeight + cButtonBevelBorderSize;
		outFrameSize.width = cSmallIconWidth + cButtonBevelBorderSize;
	}
}


