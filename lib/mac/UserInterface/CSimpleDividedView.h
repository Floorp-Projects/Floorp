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


enum
{
	kHorizontalResizeCursor	=	1120,
	kVerticalResizeCursor	=	1136
};


/*****************************************************************************
	class CSimpleDividedView
	Simplest divided view.
 
 ### Standard usage:
  In Constructor, create the CSimpleDividedView from a template,
  set the IDs and min size of subviews
  
  It should be in the same level of hierarchy as the views it manages.
  It does not do any drawing
  
  It broadcasts msg_SubviewChangedSize after a successful drag. Hook it up
  to your window manually (outside the RidL resource), because it is not a
  LControl subclass. Do this only if you need to do something in response to
  resize. For example, recalc the GrayBevelView.
  
  timm has a more sophisticated class, divview.cp, which takes car of resizing
 *****************************************************************************/

#include <LPane.h>
#include <LBroadcaster.h>

const MessageT msg_SubviewChangedSize = 32400;	// ioParam is CSimpleDividedView
 
class CSimpleDividedView : public LPane,
							public LBroadcaster
{
	public:
	
	// constructors

		enum { class_ID = 'SDIV' };
		
							CSimpleDividedView(LStream *inStream);

		virtual				~CSimpleDividedView();
	
		virtual void		FinishCreateSelf();
		
	// access
		
				void		SetMinSize(Int16 topLeft, Int16 botRight)
				{
					fTopLeftMinSize = topLeft;
					fBottomRightMinSize = botRight;
					RepositionView();
				}

	// event handling

		virtual void		AdaptToSuperFrameSize(Int32 inSurrWidthDelta,
										Int32 inSurrHeightDelta,
										Boolean inRefresh);
		virtual void		ClickSelf(const SMouseDownEvent	&inMouseDown);

		virtual void		AdjustCursorSelf(Point inPortPt, const EventRecord&	inMacEvent );


#ifdef DEBUG
		virtual void		DrawSelf();
#endif
	private:
	
		void				ReadjustConstraints();
		void				RepositionView();
		Boolean				GetViewRects(Rect& r1, Rect& r2);

		PaneIDT				fTopLeftViewID;
		PaneIDT				fBottomRightViewID;
		Int16				fTopLeftMinSize;
		Int16				fBottomRightMinSize;
		Int16				fIsVertical;	// Are views stacked vertically?. -1 means uninitialized, true and false
};
