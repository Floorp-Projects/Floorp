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
//	CDragBarDockControl.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LControl.h>
#include "CToolTipAttachment.h"
#include "CDynamicTooltips.h"

class CDragBar;
class CSharedPatternWorld;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CDragBarDockControl : public LControl, public CDynamicTooltipMixin
{
	public:
		enum { class_ID = 'DbDc' };
		
								CDragBarDockControl(LStream* inStream);
		virtual					~CDragBarDockControl();
		
		virtual	void			AddBarToDock(CDragBar* inBar);
		virtual	void			RemoveBarFromDock(CDragBar* inBar);
		virtual	Boolean			HasDockedBars(void) const;

		virtual void			SavePlace(LStream *outPlace);
		virtual void			RestorePlace(LStream *inPlace);

			// ¥ Drawing

		virtual void			ResizeFrameBy(
									Int16 				inWidthDelta,
									Int16				inHeightDelta,
									Boolean				inRefresh);

		virtual	void			SetNeedsRecalc(Boolean inRecalc);
		virtual	Boolean			IsRecalcRequired(void) const;
		
		virtual void			HideBar(CDragBar* inBar); // for javascript - mjc		
		virtual void			ShowBar(CDragBar* inBar); // for javascript - mjc	

		const CDragBar*			FindBarSpot(
									Point 				inLocalPoint);

		// for hilighting on mouse entry/exit
		void MouseLeave ( ) ;
		void MouseWithin ( Point inPoint, const EventRecord & inEvent ) ;

		virtual void FindTooltipForMouseLocation ( const EventRecord& inMacEvent,
													StringPtr outTip ) ;

	protected:


			// ¥ Drawing	
		virtual void			DrawSelf(void);
		virtual	void			DrawOneDockedBar(
									CDragBar* 			inBar);

		virtual void			RecalcDock(void);
		virtual	void			CalcOneDockedBar(
									CDragBar*			inBar,
									const Rect&			inBounds);

			// CONTROL BEHAVIOUR
			
		virtual Int16			FindHotSpot(
									Point 				inPoint);
									
		virtual Boolean			PointInHotSpot(
									Point 				inPoint,
									Int16 				inHotSpot);

		virtual void			HotSpotAction(
									Int16				inHotSpot,
									Boolean 			inCurrInside,
									Boolean				inPrevInside);
									
		virtual void			HotSpotResult(Int16 inHotSpot);	

		CSharedPatternWorld* 	mBackPattern;
		CSharedPatternWorld*	mBackPatternHilite;
		CSharedPatternWorld*	mGrippy;
		CSharedPatternWorld*	mGrippyHilite;
		ResIDT					mBarTraitsID;
		LArray					mBars;
		Boolean					mNeedsRecalc;
		CIconHandle				mTriangle;
		const CDragBar*			mMouseInside;		// which bar is mouse hovering over?
};
