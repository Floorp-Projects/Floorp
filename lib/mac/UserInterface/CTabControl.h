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

#include <LArray.h>
#include <LControl.h>
#include <LString.h>

#include "UStdBevels.h"


#ifdef powerc
#pragma options align=mac68k
#endif

typedef struct STabDescriptor {
	MessageT		valueMessage;
	Int16			width;			// -1 means auto adjust, -2 =icon tab YUCK
	Str63			title;
} STabDescriptor;

#ifdef powerc
#pragma options align=reset
#endif

const ResType_TabDescList 		=	'TabL';
const MessageT msg_TabSwitched 	=	'TbSw';

class CTabInstance;

class CTabControl : public LControl
{
	public:
		enum { class_ID = 'TabC' };
		enum { eNorthTab = 0, eEastTab, eSouthTab, eWestTab };

								CTabControl(LStream* inStream);
		virtual					~CTabControl();

			// ¥ Access

		virtual void			SetValue(Int32 inValue);
		virtual MessageT		GetMessageForValue(Int32 inValue);
		virtual Point			GetMinumumSize() { return mMinimumSize; }	// -1 = uninitialized

			// ¥ Drawing
		virtual void			Draw(RgnHandle inSuperDrawRgnH);
		virtual void			ResizeFrameBy(
										Int16 			inWidthDelta,
										Int16			inHeightDelta,
										Boolean			inRefresh);
										
		virtual void			DoLoadTabs(ResIDT inTabDescResID);
		virtual void			SetTabEnable(ResIDT inPageResID, Boolean inEnable);
		
	protected:

			// ¥ Initialization & Configuration
		virtual void			FinishCreateSelf(void);
		virtual	void			Recalc(void);
		virtual	void			CalcTabMask(
										const Rect& 	inControlFrame,
										const Rect&		inTabFrame,
										RgnHandle		ioTabMask);
			// ¥ Drawing
		virtual void			DrawSelf(void);
		virtual	void			DrawOneTab(CTabInstance* inTab);
		virtual	void			DrawTopBevel(CTabInstance* inTab);
		virtual	void			DrawBottomBevel(CTabInstance *inTab, Boolean inCurrentTab);
		virtual	void			SetClipForDrawingSides(void);
		virtual	void			DrawSides(void);
		
		virtual void			DrawOneTabBackground(
									RgnHandle inRegion,
									Boolean inCurrentTab);
		virtual void			DrawOneTabFrame(RgnHandle inRegion, Boolean inCurrentTab);
		virtual void			DrawCurrentTabSideClip(RgnHandle inRegion);

		virtual	void			ActivateSelf(void);
		virtual	void			DeactivateSelf(void);

			// ¥ Control Behaviour
		virtual Int16			FindHotSpot(Point inPoint) const;
		virtual Boolean			PointInHotSpot(Point inPoint, Int16 inHotSpot) const;

		virtual void			HotSpotAction(
										Int16			inHotSpot,
										Boolean 		inCurrInside,
										Boolean			inPrevInside);
									
		virtual void			HotSpotResult(Int16 inHotSpot);	
		virtual void			BroadcastValueMessage();

		Uint8					mOrientation;
		Int16					mCornerPixels;
		Int16					mBevelDepth;
		Int16					mSpacing;
		Int16					mLeadPixels;
		Int16					mRisePixels;
		ResIDT					mTabDescID;
		ResIDT					mTitleTraitsID;
		Point					mMinimumSize;	// The minimum size at which all tabs will be displayed

		SBevelColorDesc			mActiveColors;
		SBevelColorDesc			mOtherColors;
		SBooleanRect			mBevelSides;
		Rect					mSideClipFrame;
		
		LArray					mTabs;
		CTabInstance*			mCurrentTab;
		RgnHandle				mControlMask;	
		Boolean					mTrackInside;
		
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CTabInstance
{
	friend class CTabControl;
	
	protected:
								CTabInstance();
								CTabInstance(const STabDescriptor&inDesc );
		virtual					~CTabInstance();
		virtual void			DrawTitle(CTabInstance* inCurrentTab, ResIDT mTitleTraitsID) = 0;

	protected:

		MessageT				mMessage;
		RgnHandle 				mMask;
		Rect					mFrame;
		Rect					mShadeFrame;
		Int16					mWidth;
		TString<Str63> 			mTitle;	
		Boolean					mEnabled;
};

class CTextTabInstance: public CTabInstance
{
	friend class CTabControl;
	protected:
			 					CTextTabInstance(const STabDescriptor&inDesc );
	 virtual void				DrawTitle(CTabInstance* inTab, ResIDT mTitleTraitsID);	
};

class CIconTabInstance: public CTabInstance
{
	friend class CTabControl;
	protected:
				 				CIconTabInstance(const STabDescriptor&inDesc );
		virtual void			DrawTitle(CTabInstance* inCurrentTab, ResIDT inTitleTraitsID);
	protected:
		SInt32					mIconID;
};




