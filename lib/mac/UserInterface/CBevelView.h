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
#include <LArray.h>
#include <URegions.h>

const ResType ResType_BevelDescList = 'BevL';

typedef struct SSubBevel {
	PaneIDT		paneID;
	Int16		bevelLevel;
	Rect		cachedLocalFrame;
} SSubBevel;

class CBevelView : public LView
{
	public:
		enum { class_ID = 'BvVw' };
	
							CBevelView(LStream *inStream);
		virtual				~CBevelView();
	
		virtual void		ResizeFrameBy(
									Int16 				inWidthDelta,
									Int16				inHeightDelta,
									Boolean 			inRefresh);

		virtual	void		CalcBevelRegion(void);
		virtual	void		CalcBevelMask(void);
		
		virtual	void		SubPanesChanged(Boolean inRefresh = true);

		static void			SubPanesChanged(LPane* subPane, Boolean inRefresh);
			// This is a safe, but ugly way of telling any enclosing GrayBevelView
			// that the panes have moved.  The subPane is the starting point for
			// an upwards search
		
		// 97-06-05 pkc -- methods to control bevelling of grow icon area
		void				BevelGrowIcon();
		void				DontBevelGrowIcon();
		
	protected:

		virtual void		FinishCreateSelf(void);

		virtual	void		DrawSelf(void);
		virtual	void		DrawBeveledFill(void) = 0;
		virtual	void		DrawBeveledFrame(void) = 0;
		virtual	void		DrawBeveledSub(const SSubBevel&	inDesc) = 0;
									
		virtual	void		InvalMainFrame(void);
		virtual void		InvalSubFrames(void);

		virtual	Boolean		CalcSubpaneLocalRect(PaneIDT inPaneID, Rect& subFrame);

		Int16				mMainBevel;
		LArray				mBevelList;
		StRegion			mBevelRegion;		// Local
		StRegion			mBevelMaskRegion;	// Port Relative
		Boolean				mNeedsRecalc;
		Boolean				mBevelGrowIcon;
};

