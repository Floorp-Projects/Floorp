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

//
// CScrollerWithArrows
// Mike Pinkerton, Netscape Communications
//
// Contains:
//
// ¥ CScrollerWithArrows
//		A class which provides scrolling in one direction, vertical or
//		horizontal, by using directional arrows at either end of the
//		bar. If the height is longer than the width, we assume vertical
//		scrolling (and vice versa).
//
// ¥ CScrollArrowControl
//		The arrow control which sends a message to scroll in a particular 
//		direction. Used by above class.
//

#pragma once


#include <LGAIconButton.h>


class CScrollerWithArrows : public LView,
							public LListener {
public:
	enum { class_ID = 'CSWA' };

	CScrollerWithArrows ( );
	CScrollerWithArrows ( LStream *inStream );
	virtual ~CScrollerWithArrows ( );

	bool IsVertical ( ) const { return mIsVertical; }
	
	virtual void InstallView ( LView *inScrollingView );
	virtual void ExpandSubPane ( LPane *inSub, Boolean inExpandHoriz, Boolean inExpandVert );
	
	virtual void AdjustScrollArrows ( );
	virtual void ResizeFrameBy ( Int16 inWidthDelta, Int16 inHeightDelta, Boolean inRefresh );
	
	virtual void SubImageChanged ( LView *inSubView );
	virtual void ListenToMessage ( MessageT inMessage, void *ioParam );
	
protected:

	enum ScrollDir { kUpLeft, kDownRight } ;

	typedef struct {					// read in from constructor stream
		Int16	leftIndent;
		Int16	rightIndent;
		Int16	topIndent;
		Int16	bottomIndent;
		PaneIDT	scrollingViewID;
	} SScrollerArrowInfo;
	
	LView*			mScrollingView;		// the view that is scrolled
	PaneIDT			mScrollingViewID;	// pane id of view that scrolls
	LControl*		mUpLeftArrow;		// our two scroll arrows
	LControl*		mDownRightArrow;
	bool			mIsVertical;		// true if we scroll up/down, false if left/right

		// Standard PowerPlant overrides for drawing/activation
	virtual void FinishCreateSelf();
	virtual void ActivateSelf();
	virtual void DeactivateSelf();
	
		// Override this to create a different kind of scroll arrow
	virtual LControl* MakeOneScrollArrow ( const SPaneInfo &inPaneInfo,
														ScrollDir inScrollWhichWay ) ;
									
	void MakeScrollArrows ( Int16 inLeftIndent, Int16 inRightIndent, Int16 inTopIndent,
							Int16 inBottomIndent); 
				
}; // class CScrollerWithArrows


class CScrollArrowControl : public LGAIconButton
{
public:
	enum { class_ID = 'CSAC' } ;
	enum { kIconUp = 14504, kIconDown = 14505, kIconLeft = 14504, kIconRight = 14505 } ;
	 						
	CScrollArrowControl ( const SPaneInfo &inPaneInfo, const SControlInfo &inControlInfo,
							ResIDT inIconResID  );
	CScrollArrowControl ( LStream* inStream ) ;
	
protected:

	virtual void DrawSelf ( ) ;
	virtual void HotSpotAction ( Int16 inHotSpot, Boolean inCurrInside, Boolean inPrevInside );

}; // class CScrollArrowControl