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

#include "CExpandoDivider.h"

#include "prtypes.h"
#include "macutil.h"
#include "CDragBarDockControl.h"

#define kTwistieID 'Twst'
#define kCaptionID 'TwCp'

const Int16 kTwistiePixelDifference = 3; // difference in height (collapsed minus expanded).

//======================================
// CExpandoListener
//======================================

//-----------------------------------
void CExpandoListener::ListenToMessage(MessageT inMessage, void *ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
	case msg_TwistieMessage:
		{
		Int32 value = *(Int32*)ioParam;
		SetExpandState((ExpandStateT)value);
		break;
		}
	}
} // CExpandoListener::ListenToMessage

//======================================
// CExpandable
//======================================

//-----------------------------------
CExpandable::CExpandable(CExpansionData* closedState, CExpansionData* openState)
//-----------------------------------
:	mExpandState(closed_state)
{
	mStates[0] = closedState;
	mStates[1] = openState;
}

//-----------------------------------
void CExpandable::StoreCurrentDimensions()
//-----------------------------------
{
	StoreDimensions(*(mStates[GetExpandState()]));
}

//-----------------------------------
void CExpandable::RecallCurrentDimensions()
//-----------------------------------
{
	RecallDimensions(*(mStates[GetExpandState()]));
}

//-----------------------------------
void CExpandable::RecallOtherDimensions()
//-----------------------------------
{
	RecallDimensions(*mStates[1 - GetExpandState()]);
}

//-----------------------------------
void CExpandable::ReadStatus(LStream* inStream)
//-----------------------------------
{
	if (!inStream) return;
	*inStream >> mExpandState;
	mStates[0]->ReadStatus(inStream);
	mStates[1]->ReadStatus(inStream);
	// Don't do anything with them here.
} // CExpandable::ReadStatus

//-----------------------------------
void CExpandable::WriteStatus(LStream* inStream)
//-----------------------------------
{
	StoreCurrentDimensions();
	if (!inStream) return;
	*inStream << mExpandState;
	mStates[0]->WriteStatus(inStream);
	mStates[1]->WriteStatus(inStream);
} // CExpandable::ReadStatus

const Int16 kDefaultTopFrameHeight = 110;
	// FIXME.  A preference?  This value shows 5 full message lines in geneva 9.

//-----------------------------------
inline CDividerData::CDividerData()
//-----------------------------------
:	mDividerPosition(kDefaultTopFrameHeight)
{
} // CDividerData::CDividerData

//-----------------------------------
void CDividerData::ReadStatus(LStream* inStream)
//-----------------------------------
{
	if (!inStream) return;
	*inStream >> mDividerPosition;
} // CDividerData::ReadStatus

//-----------------------------------
void CDividerData::WriteStatus(LStream* inStream)
//-----------------------------------
{
	if (!inStream) return;
	*inStream << mDividerPosition;
} // CDividerData::WriteStatus

//======================================
// CExpandoDivider
//======================================

//-----------------------------------
CExpandoDivider::CExpandoDivider(LStream* inStream)
//-----------------------------------
:	Inherited( inStream )
,	CExpandable(&mClosedData, &mOpenData)
{
} // CExpandoDivider::CExpandoDivider

//-----------------------------------
CExpandoDivider::~CExpandoDivider()
//-----------------------------------
{
}

//-----------------------------------
void CExpandoDivider::FinishCreateSelf()
//-----------------------------------
{
	Inherited::FinishCreateSelf();
	mTwistie = FindPaneByID(kTwistieID);
	mCaption = FindPaneByID(kCaptionID);
//	CExpandable::InitializeStates();
	StoreCurrentDimensions(); // get the closed state from PPOb
	// Base class calls SyncFrameBinding which sets the "open" behavior. Undo this, then.
	SetStickToBottom(true);

	// Record the height of the status bar, so that we can preserve it on expansion.
	LWindow* window = LWindow::FetchWindowObject(GetMacPort());
	Rect windowRect;
	window->CalcPortFrameRect(windowRect); // relative is fine
	Rect expandoRect;
	this->CalcPortFrameRect(expandoRect);
	mDistanceFromWindowBottom = windowRect.bottom - expandoRect.bottom;
	Assert_(mDistanceFromWindowBottom >= 0);
	mDividerDistanceFromWindowBottom
		= windowRect.bottom - (expandoRect.top + GetDividerPosition());
} // CExpandoDivider::FinishCreateSelf

//-----------------------------------
void CExpandoDivider::StoreDimensions(CExpansionData& outState)
//-----------------------------------
{
	((CDividerData&)outState).mDividerPosition = GetDividerPosition();
} // CExpandoDivider::StoreDimensions

//-----------------------------------
void CExpandoDivider::RecallDimensions(const CExpansionData& inState)
//-----------------------------------
{
	SInt32 dividerPosition = GetDividerPosition();
	Int16 dividerDelta = ((CDividerData&)inState).mDividerPosition - dividerPosition;
	this->ChangeDividerPosition(dividerDelta);
} // CExpandoDivider::RecallDimensions

//-----------------------------------
void CExpandoDivider::SetStickToBottom(LPane* inPane, Boolean inStick)
//-----------------------------------
{
	SBooleanRect bindings;
	inPane->GetFrameBinding(bindings);
	bindings.bottom = inStick;
	inPane->SetFrameBinding(bindings);
} // CExpandoDivider::SetStickToBottom

//-----------------------------------
void CExpandoDivider::SetStickToBottom(Boolean inStick)
//-----------------------------------
{
	SetStickToBottom(mTwistie, inStick);
	SetStickToBottom(mCaption, inStick);
	SetStickToBottom(fFirstView, inStick);
	SetStickToBottom(fSecondView, true);
} // CExpandoDivider::SetStickToBottom

//-----------------------------------
void CExpandoDivider::ClickSelf(const SMouseDownEvent& inMouseDown)
//-----------------------------------
{
	if (GetExpandState() == open_state) Inherited::ClickSelf(inMouseDown);
}

//-----------------------------------
void CExpandoDivider::AdjustCursorSelf(Point inPortPt, const EventRecord& inMacEvent)
//-----------------------------------
{
	if (GetExpandState() == open_state) Inherited::AdjustCursorSelf(inPortPt, inMacEvent);
}

//-----------------------------------
void CExpandoDivider::ChangeTwistiePosition(Int16 delta)
// Move the twistie and caption
//-----------------------------------
{
	mTwistie->MoveBy(0, delta, FALSE);
	mCaption->MoveBy(0, delta, FALSE);
} // CExpandoDivider::ChangeTwistiePosition

//-----------------------------------
void CExpandoDivider::ChangeDividerPosition(Int16 delta)
//-----------------------------------
{
	if (mExpandState == open_state && delta > 0)
	{
		// If the user drags the divider to the bottom, it should close the twistie.
		Int32 dividerPos = this->GetDividerPosition();
		Int32 newPos = dividerPos + delta;
		Rect secondFrame;
		GetSubpaneRect(this, fSecondView, secondFrame);
		if (newPos > secondFrame.bottom - 50)
		{
			mTwistie->SetValue(closed_state);
			return;
		}
	}
	Inherited::ChangeDividerPosition(delta);
	ChangeTwistiePosition(delta);
} // CExpandoDivider::ChangeDividerPosition

//-----------------------------------
void CExpandoDivider::ResizeFrameBy(
	Int16		inWidthDelta,
	Int16		inHeightDelta,
	Boolean		inRefresh)
//-----------------------------------
{
	Inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	// Unless we do this, there's no way to enforce the rule that the second pane's TOP
	// sticks to the bottom when in the collapsed state.
	if (mExpandState == closed_state && fFirstView && fSecondView)
	{
		SPoint32 loc1, loc2;
		SDimension16 siz1, siz2;
		fFirstView->GetFrameLocation(loc1);
		fFirstView->GetFrameSize(siz1);
		fSecondView->GetFrameLocation(loc2);
		fSecondView->GetFrameSize(siz2);
		Int32 secondViewOffset = siz1.height + loc1.v + mDivSize - loc2.v;
		if (secondViewOffset)
		{
			fSecondView->MoveBy(0, secondViewOffset, false);
			fSecondView->ResizeFrameBy(0, -secondViewOffset, false);
		}
	}
} // CExpandoDivider::ResizeFrameBy

//-----------------------------------
void CExpandoDivider::SetExpandState(ExpandStateT inExpanded)
//-----------------------------------
{
#if 0
	// We now assume that the my view's bottom is flush with the bottom of the
	// second subview.
	SPoint32 locMe, loc2;
	SDimension16 sizMe, siz2;
	GetFrameSize(sizMe);
	GetFrameLocation(locMe);
	fSecondView->GetFrameLocation(loc2);
	fSecondView->GetFrameSize(siz2);
	Assert_(loc2.v + siz2.height == locMe.v + sizMe.height);
#endif // DEBUG

	LWindow* win = LWindow::FetchWindowObject(GetMacPort());
	Rect winRect;
	win->CalcPortFrameRect(winRect); // relative is fine
	const Int16 statusBarHeight = this->GetCorrectDistanceFromBottom();
	const Int16 dividerDistanceFromBottom
		= this->GetCorrectDividerDistanceFromBottom();
	Rect expandoRect;
	this->CalcPortFrameRect(expandoRect);

	if (mExpandState != inExpanded)
		StoreCurrentDimensions();
	mExpandState = inExpanded;
	if (inExpanded)
	{
		// When expanded, topview, twistie and caption do not stick to the bottom.
		mCaption->Hide();
		SetStickToBottom(false);
		SyncFrameBindings();
		
		
		// The expanded twistie is not as high as the collapsed one, and the following
		// adjustment allows us to have a narrower divider bar.
		ChangeTwistiePosition(- kTwistiePixelDifference);
		
		// Now expand.  The divider will pull the frame up.
		RecallCurrentDimensions();

		fSecondView->Show();
	}
	else
	{
		ChangeTwistiePosition(+ kTwistiePixelDifference);
		fSecondView->Hide();
		mCaption->Show();
		RecallCurrentDimensions();

	}
	// The following is a kludge to fix cases where the bottom of Message view
	// can disappear under the bottom of the window, or where the divider containing
	// the twistie icon can be a one-inch thick grey area just over the bottom of the window.
	short vertError = (winRect.bottom - statusBarHeight) - expandoRect.bottom;
	if (vertError != 0)
	{
		this->ResizeFrameBy(0, vertError, false);
	}
	if (!inExpanded)
	{
		vertError = (winRect.bottom - (expandoRect.top + dividerDistanceFromBottom))
									- this->GetDividerPosition();
		if (vertError != 0)
			this->ChangeDividerPosition(vertError);
		// When collapsed, topview, twistie and caption stick to the bottom.
		SetStickToBottom(true); //еее this line is not part of the kludge
	}
} // CExpandoDivider::SetExpandedState

