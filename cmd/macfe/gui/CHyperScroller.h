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

#include "CConfigActiveScroller.h"

/******************************************************************************
 * CHyperScroller 
 * Each hyperview needs the ability to show/hide a 
 * - focus/dragging border
 * - scrollers
 * This is implemented as an addition to the LScroller class, because LScroller
 * already manages the scrollbars
 ******************************************************************************/

class CHyperScroller: public CConfigActiveScroller
{
	typedef CConfigActiveScroller Inherited;


public:
	//friend class CHyperView;		// this class no longer exists
	
					enum { class_ID = 'HSCR' };
					
				 	CHyperScroller( LStream* inStream );	

	void			ResetExpansion(void);

	void			ExpandLeft(void);
	void			ExpandTop(void);
	void			ExpandRight(void);
	void			ExpandBottom(void);
	
	// ¥ display control
	void			ShowScrollbars( Boolean hori, Boolean vert );
	Boolean			HasScrollbars()	{ return ( !fVerticalBar  && !fHorizontalBar ); }
	Boolean			HasVerticalScrollbar() { return !fVerticalBar;}
	Boolean			HasHorizontalScrollbar() {return !fHorizontalBar; }

	Boolean			ScrolledToMinVerticalExtent();
	Boolean			ScrolledToMaxVerticalExtent();
	
	// Draw  a little gray rectangle next to scrollers if needed
	void			DrawDummySizeBox();
	
	void			AdjustHyperViewBounds(void);
	virtual void 	AdjustScrollBars();
	
	virtual void 	RefreshSelf();

	// Access to handle grow icon state
	Boolean			GetHandleGrowIconManually() { return mHandleGrowIconManually; }
	void			SetHandleGrowIconManually(Boolean inState)
					{ mHandleGrowIconManually = inState; }

	// Override ClickSelf so that we can handle grow icon clicking
	virtual void	ClickSelf(const SMouseDownEvent &inMouseDown);

protected:
	// ¥ Standard overrides
	virtual void		FinishCreateSelf(void);
	virtual void		DrawSelf(void);
	virtual void		ActivateSelf();

	SBooleanRect		mExpanded;

	Rect			CalcDummySizeBox();	// Rectangle enclosing the dummy box
	void			DrawEmptyScroller( LStdControl* scrollBar );
	LStdControl*	fVerticalBar;	// When it is hidden, we need to save this value
									// because LScroller assumes that we want scrollbars
									// to be visible. The only way to turn this off 
									// is to set scrollbars to NIL.
	LStdControl*	fHorizontalBar;
	
	Boolean			mHandleGrowIconManually;
};
