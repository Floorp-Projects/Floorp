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
//	CSpinningN.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LPane.h>
#include <LListener.h>
#include <LPeriodical.h>

#include "CPatternButton.h"

const Int16 kMaxSpinStages 		=	40;

class CSpinningN :
		public CPatternButton,
		public LListener,
		protected LPeriodical
{
	public:
		enum {
			class_ID			= 'SpnN',
			kAdornmentPaneID	= 'Adrn',
			kFillPaneID			= 'Fill'
		};
		
							CSpinningN(LStream* inStream);

		virtual	void		ListenToMessage(
								MessageT			inMessage,
								void*				ioParam);

		virtual void		AdaptToSuperFrameSize(
								Int32				inSurrWidthDelta,
								Int32				inSurrHeightDelta,
								Boolean 			inRefresh);
		static short&		AnimResFile() { return mAnimResFile; }
		
		virtual	void		StopSpinningNow(void);

		virtual Boolean		ChangeMode(Int8 newMode, SDimension16& outDimensionDeltas);
		
	protected:

		virtual	void		SpendTime(
								const EventRecord&	inMacEvent);

		virtual	void		StartRepeating(void);
		virtual	void		StopRepeating(void);

	
		virtual void		FinishCreateSelf(void);
		virtual void		DrawSelf(void);

		virtual void		DrawButtonGraphic(void);
		virtual void		DrawButtonContent(void);
		virtual void		DrawSelfDisabled(void);
		
			//	We redraw on Activate/Deactivate/Enable/Disable in order to play well
			//	with the darn CBevelView stuff.
		virtual void		ActivateSelf()		{ Draw(nil); }
		virtual void		DeactivateSelf()	{ Draw(nil); }
		virtual void		EnableSelf()		{ Draw(nil); }
		virtual void		DisableSelf()		{ Draw(nil); }
		
		virtual void		CalcAnimationMode(void);
		virtual	void		RepositionSelf(SDimension16* inNewToolbarSize = NULL);

		void				GetLargeIconFrameSize(SDimension16& outFrameSize);
		void				GetSmallIconFrameSize(SDimension16& outFrameSize);

		Int32				mIconCount;
		ResIDT				mGraphicID;
		Rect 				mCachedIconFrame;
		
		Int32 				mLastSpinTicks;		// Last time icon was spun.
		Int16				mSpinCounter;		// 0 			== not spinning
												// 1..num icons == spin stage
		
		Boolean				mIsCoBranded;

		Boolean				mHasLargeCoBrand;
		Int32				mLargeSeqCount;

		Boolean				mHasSmallCoBrand;
		Int32				mSmallSeqCount;
		static short		mAnimResFile;
		
		Int32				mRefCount;
		
		Boolean				mFinishedCreatingSelf;
		
		LPane*				mAdornment;
		LPane*				mFill;
};
