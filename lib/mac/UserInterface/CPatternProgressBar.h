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

#include <LPane.h>
#include <LString.h>
#include <URegions.h>

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CSharedPatternWorld;

class CPatternProgressBar : public LPane
{
	public:

		enum {
			eIndefinite = -1,
			eSeamless = -2
		};

		enum { class_ID = 'PtPb' };
								CPatternProgressBar(LStream* inStream);
		virtual					~CPatternProgressBar();

		virtual	void			SetValue(Int32 inValue);
		virtual	Int32			GetValue(void) const;
		virtual	void			SetValueRange(UInt32 inValueRange);
		virtual	void			SetToIndefinite(void);
		virtual	void			SetSeamless(void);

		virtual	void 			Draw(RgnHandle inSuperDrawRgnH);
		virtual void			ResizeFrameBy(
										Int16 			inWidthDelta,
										Int16			inHeightDelta,
										Boolean			inRefresh);
	protected:

		virtual void			ActivateSelf(void);
		virtual void			DeactivateSelf(void);
	
		virtual	void			FinishCreateSelf(void);
		virtual	void			DrawSelf(void);

		virtual	void			DrawIndefiniteBar(const Rect& inBounds);
		virtual	void			DrawSeamlessBar(const Rect& inBounds);
		virtual	void			DrawPercentageBar(const Rect& inBounds);
		
		virtual	void			RecalcPoleMasks(void);

		Int32 					mValue;
		Uint32					mValueRange;

		Int32					mPatOffset;
		Int32					mPatWidth;

		CSharedPatternWorld*	mPatternWorld;
		Int16					mOrientation;
		StRegion				mFillMask;
		StRegion				mTopBevelMask;
		StRegion				mBottomBevelMask;
};


inline void CPatternProgressBar::SetToIndefinite(void)
	{	SetValue(eIndefinite);			}
inline void CPatternProgressBar::SetSeamless(void)
	{	SetValue(eSeamless);			}	
	
	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CPatternProgressCaption : public CPatternProgressBar
{
	public:

		enum { class_ID = 'PtPc' };
								CPatternProgressCaption(LStream* inStream);
	
		virtual void			SetDescriptor(ConstStringPtr inDescriptor);
		virtual void			SetDescriptor(const char* inCDescriptor);
		virtual StringPtr		GetDescriptor(Str255 outDescriptor) const;

	protected:	

		virtual	void			DrawSelf(void);

		TString<Str255>			mText;
		ResIDT					mTraitsID;
};