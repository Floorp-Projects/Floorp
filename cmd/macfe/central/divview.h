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


const Int32 kOutsideSlop = 0x80008000;
const MessageT msg_DividerChangedPosition = 'divc';

class LDividedView :
	public LView,
	public LBroadcaster,	// broadcasts msg_DividerChangedPosition
	public LListener		// Listens to Zap button (if any)
{
public:
	enum			{class_ID = 'DIVV'};
	enum			FeatureFlags
					{
						eNoFeatures					= 0
					,	eCollapseFirstByDragging	= (1<<0)
					,	eCollapseSecondByDragging	= (1<<1)
					,	eZapClosesFirst				= (1<<2)
					,	eZapClosesSecond			= (1<<3)
					,	eBindZapButtonBottomOrRight	= (1<<4) // if neither of these....
					,	eBindZapButtonTopOrLeft		= (1<<5) // ... then we center it.
					,	eNoOpenByDragging			= (1<<6)
					// Handy combinations
					,	eCollapseBothWaysByDragging	= 
							(eCollapseFirstByDragging|eCollapseSecondByDragging)
					,	eCollapseBothZapFirst		=
							(eCollapseBothWaysByDragging|eZapClosesFirst)
					,	eCollapseBothZapSecond		= 
							(eCollapseBothWaysByDragging|eZapClosesSecond)
					,	eOnlyFirstIsCollapsible		= 
							(eZapClosesFirst|eCollapseFirstByDragging)
					,	eOnlySecondIsCollapsible	= 
							(eZapClosesSecond|eCollapseSecondByDragging)
					// The bindings are set using the PP binding
					// fields in the PP constructor resource:
					,	eBindingMask				=
							(eBindZapButtonBottomOrRight|eBindZapButtonTopOrLeft)
					// Bits in this mask are not changeable through the public
					// SetFeatureFlags calls:
					,	ePrivateMask				=
							(eBindingMask)
					};
					LDividedView( LStream* inStream );
	virtual			~LDividedView();
	
	virtual void	FinishCreateSelf();
	
	void			SetSlidesHorizontally( Boolean isHoriz );
	Boolean			GetSlidesHorizontally() const { return mSlidesHorizontally; };
	UInt16			GetDividerSize() const { return mDivSize; }
	void			DoZapAction();
	
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
	
	// These reposition things as necessary.  But the bindings are private.
	void			OnlySetFeatureFlags(FeatureFlags inFlags); // adjusts positions etc.
	void			SetFeatureFlags(FeatureFlags inFlags) // adjusts positions etc.
						{ OnlySetFeatureFlags((FeatureFlags)(mFeatureFlags | inFlags)); }
	void			UnsetFeatureFlags(FeatureFlags inFlags)
						{ OnlySetFeatureFlags((FeatureFlags)(mFeatureFlags & ~inFlags)); }
	
	Boolean			IsSecondPaneCollapsed() const
						{ return GetDividerPosition() >= (mSlidesHorizontally ?
							mFrameSize.width : mFrameSize.height) - mDivSize; }
	Boolean			IsFirstPaneCollapsed() const
						{ return GetDividerPosition() <= 0; }
	void			SetSubPanes(
						LPane* inFirstPane,
						LPane* inSecondPane,
						Int32 inDividerPos,
						Boolean inRefresh);
	void			GetSubPanes(
						LPane*& outFirstPane,
						LPane*& outSecondPane) const
						{ outFirstPane = mFirstPane; outSecondPane = mSecondPane; }
	Int32			GetDividerPosition() const;
						// WARNING: this does not access a stored value, it is
						// calculated by looking at the first subpane's current size.
						// It doesn't work too well if the first subpane is nil, or
						// if the first subpane has not yet been installed.


	static Boolean	PreferSounds(); 
	static void		PlayClosingSound() { PlaySound(128); }
	static void		PlayOpeningSound() { PlaySound(129); }
	static void		PlaySound(ResIDT id);

protected:

	LPane*			GetZapButton();
	void			PositionZapButton();
	void			OrientZapButtonTriangles();
	void			SetZapFrame(
						SInt16 inZapWidth,
						SInt16 inDividerPosition,			// rel. to div view's frame
						SInt16 inDividerLength,
						SInt16& ioZapLength,
						SInt32& ioZapOffsetLateral, 		// rel. to div view's frame
						SInt32& ioZapOffsetLongitudinal);	// rel. to div view's frame
	
					// These two have no side-effects, and work on all bits.
	void			SetFeatureFlagBits(FeatureFlags inFlags) // no other effects
						{ mFeatureFlags = (FeatureFlags)(mFeatureFlags | inFlags); }
	void			UnsetFeatureFlagBits(FeatureFlags inFlags) // no other effects
						{ mFeatureFlags = (FeatureFlags)(mFeatureFlags & (~inFlags)); }
	
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

					// ¥ check if we're allowed to pull out a collapsed pane by dragging
	virtual	bool	CanExpandByDragging ( ) const { return !(mFeatureFlags & eNoOpenByDragging); };
	
// Data
protected:

	PaneIDT			mFirstSubviewID;		// pane ID of the top or left pane
	PaneIDT			mSecondSubviewID;		// pane ID of the bottom or right pane

	UInt16			mDivSize;			// Width of the divider
	Int16			mMinFirstSize;		// mimimum size of top (or left) pane
	Int16			mMinSecondSize;		// minimum size of bottom (or right) pane
	
	FeatureFlags	mFeatureFlags;
	Int16			mDividerPosBeforeCollapsing;

	Int16			mFirstIndent;
	Int16			mSecondIndent;
	
	Boolean			mSlidesHorizontally;// top/bottom or left/right configuration?

	LPane*			mFirstPane;			// top or left view
	LPane*			mSecondPane;		// bottom or right view
	
	static Boolean	sSettingUp;
};