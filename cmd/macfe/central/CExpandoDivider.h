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

#pragma once

#include "divview.h"

//-----------------------------------
class CExpansionData
// persistent data about both states of a CExpandable.
//-----------------------------------
{
public:
	virtual void	ReadStatus(LStream* inStream) = 0;
	virtual void	WriteStatus(LStream* inStream) = 0;
}; //  class CExpansionData

enum { closed_state = false, open_state = true };
typedef Boolean ExpandStateT;

//======================================
class CExpandable
//======================================
{
public:
	virtual void	ReadStatus(LStream* inStream);
	virtual void	WriteStatus(LStream* inStream);
	ExpandStateT	GetExpandState() const { return mExpandState; }
	void			NoteExpandState(ExpandStateT inExpanded) { mExpandState = inExpanded; }
protected:
	virtual void	SetExpandState(ExpandStateT inExpanded) = 0;
private:
	virtual void	StoreDimensions(CExpansionData& outState) = 0;
	virtual void	RecallDimensions(const CExpansionData& inState) = 0;	
protected:
					CExpandable(CExpansionData* closedState, CExpansionData* openState);
						// clients that mix this class in should have two members that are
						// CExpansionData, and pass the references in here.
protected:
	void			StoreCurrentDimensions();
	void			RecallCurrentDimensions();
	void			RecallOtherDimensions();
protected:
	ExpandStateT	mExpandState;
	CExpansionData*	mStates[2];
}; // class CExpandable

//======================================
class CExpandoListener : public LListener, public CExpandable
//======================================
{
public:
	enum			{ msg_TwistieMessage = 'Twst' }; // Broadcast by twistie control
					CExpandoListener(
						CExpansionData* closedState, CExpansionData* openState)
						:	CExpandable(closedState, openState) {}
	virtual void	ListenToMessage(MessageT inMessage, void *ioParam);
		// Listen to the twistie
}; // class CExpandoListener

//======================================
class CDividerData : public CExpansionData
//======================================
{
public:
	CDividerData();
		// default is set for the open state, because the closed state is in the PPOb.
// Overrides
	virtual void	ReadStatus(LStream* inStream);
	virtual void	WriteStatus(LStream* inStream);
// Data
	SInt32			mDividerPosition;
}; // class CDividerData

//======================================
class CExpandoDivider : public LDividedView, public CExpandable
// This class acts like a divider between two panes, one above the other.
// In addition to the LDividedView behavior, it also has an "expando" twistie, that
// hides/shows the bottom pane.
//======================================
{
private:
	typedef LDividedView Inherited;
public:
	enum			{ class_ID = 'Expo' };
					CExpandoDivider(LStream* inStream);
	virtual			~CExpandoDivider();

// PowerPlant overrides
protected:	
	virtual void	FinishCreateSelf();
	virtual	void	ClickSelf(const SMouseDownEvent& inMouseDown);
	virtual	void	AdjustCursorSelf(Point inPortPt, const EventRecord& inMacEvent);
public:
	virtual void	ResizeFrameBy(
						Int16		inWidthDelta,
						Int16		inHeightDelta,
						Boolean		inRefresh);
// CExpandable overrides
public:
	virtual void	SetExpandState(ExpandStateT inExpanded);
	virtual void	StoreDimensions(CExpansionData& outState);
	virtual void	RecallDimensions(const CExpansionData& inState);
// Special interfaces
public:
	Int16			GetCorrectDistanceFromBottom() const { return mDistanceFromWindowBottom; }
	Int16			GetCorrectDividerDistanceFromBottom() const
						{ return mDividerDistanceFromWindowBottom; }
// Down to business:
protected:
	virtual void	ChangeDividerPosition(Int16 delta); // also changes the twistie+caption
	virtual void	ChangeTwistiePosition(Int16 delta); // only changes the twistie+caption
	void			SetStickToBottom(LPane* inPane, Boolean inStick);
	void			SetStickToBottom(Boolean inStick);
// Data:
protected:
	Int16			mDistanceFromWindowBottom;
	Int16			mDividerDistanceFromWindowBottom;
	LPane			*mTwistie, *mCaption;
	CDividerData	mClosedData, mOpenData;
}; // class CExpandoDivider
