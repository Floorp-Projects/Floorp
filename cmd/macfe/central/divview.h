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

#include <LView.h>
#include <LStream.h>
#include <LBroadcaster.h>
#include <LGAIconSuiteControl.h>

const Int32 kOutsideSlop = 0x80008000;
const MessageT msg_DividerChangedPosition = 'divc';

class LDividedView :
	public LView,
	public LBroadcaster,
	public LListener
{
public:
	enum			{class_ID = 'DIVV'};
	
					LDividedView( LStream* inStream );
	virtual			~LDividedView();
	
	virtual void	FinishCreateSelf();
	
	void			SetSlidesHorizontally( Boolean isHoriz );
	Boolean			GetSlidesHorizontally() const { return mSlidesHorizontally; };
	UInt16			GetDividerSize() const { return mDivSize; }
	void			ToggleFirstPane();
	
	virtual void	ListenToMessage(MessageT inMessage, void* ioParam);
	
	virtual	void	ClickSelf( const SMouseDownEvent& inMouseDown );
	
	virtual	void	AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent );
	virtual void	ChangeDividerPosition( Int16 delta );
	virtual void	ResizeFrameBy(
						Int16		inWidthDelta,
						Int16		inHeightDelta,
						Boolean		inRefresh);

	virtual void	SavePlace( LStream* inStream );
	virtual void	RestorePlace( LStream* inStream );
	
	void			SetCollapseByDragging(Boolean inFirst, Boolean inSecond)
						{ mCollapseFirstByDragging = inFirst; mCollapseSecondByDragging = inSecond; }
	void			GetCollapseByDragging(Boolean& outFirst, Boolean& outSecond)
						{ outFirst = mCollapseFirstByDragging;  outSecond = mCollapseSecondByDragging; }

	Boolean			IsSecondPaneCollapsed() const
						{ return GetDividerPosition() >= (mSlidesHorizontally ? mFrameSize.width : mFrameSize.height) - mDivSize; }
	Boolean			IsFirstPaneCollapsed() const
						{ return GetDividerPosition() <= 0; }
	static void		PlayClosingSound() { PlaySound(128); }
	static void		PlayOpeningSound() { PlaySound(129); }
	static void		PlaySound(ResIDT id);
protected:

	LPane*			GetZapButton();
	void			PositionZapButton();
	Int32			GetDividerPosition() const;
	
					// ¥Êthe area representing the divider rect (only when dragging)
	void			CalcDividerRect( Rect& outRect );
	
	void			SyncFrameBindings();
	
					// ¥ lays out the subpanes (after reanimating, changing horiz./vert.)
	void			PositionViews( Boolean willBeHoriz );

					// ¥Êthe following two routines are called just prior to
					//		dragging
	void			CalcLimitRect( Rect& lRect );
	void			CalcSlopRect( Rect& sRect );

					// ¥Êthis routine is called to invalidate the rectangle between
					//		the two panes (which is larger than divider rect) when
					//		the divider is moved
	void			CalcRectBetweenPanes( Rect& rect );
	
					// ¥ allows the user to drag the closed pane to open it.
	bool			CanExpandByDragging ( ) const { return false; } ;

// Data
protected:

	PaneIDT			mFirstSubview;		// pane ID of the top or left pane
	PaneIDT			mSecondSubview;		// pane ID of the bottom or right pane

	UInt16			mDivSize;			// Width of the divider
	Int16			mMinFirstSize;		// mimimum size of top (or left) pane
	Int16			mMinSecondSize;		// minimum size of bottom (or right) pane
	
	Boolean			mCollapseFirstByDragging;
	Boolean			mCollapseSecondByDragging;
	Int16			mDividerPosBeforeCollapsing;

	Int16			mFirstIndent;
	Int16			mSecondIndent;
	
	Boolean			mSlidesHorizontally;		// top/bottom or left/right configuration?

	LPane*			fFirstView;			// top or left view
	LPane*			fSecondView;		// bottom or right view	
};