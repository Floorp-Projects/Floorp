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

// Includes

#include <LControl.h>

#if defined(QAP_BUILD)
#include <QAP_Assist.h>
#endif

#include "MPaneEnablerPolicy.h"

// abstract base class for button controls

class CButton : public LControl, public MPaneEnablerPolicy
{
	public:		
		enum	{ class_ID = 'CBut' };	

							CButton(LStream* inStream);
		virtual				~CButton();
		
		virtual StringPtr	GetDescriptor(Str255 outDescriptor) const;
		virtual void		SetDescriptor(ConstStr255Param inDescriptor);
		
		void				SetTrackInside(Boolean inInside);
		Boolean				IsTrackInside(void) const;

		Int8				GetBehaviour(void) const;
		Boolean				IsBehaviourButton(void) const;
		Boolean				IsBehaviourRadio(void) const;
		Boolean				IsBehaviourToggle(void) const;
		
		virtual void		SetGraphicID(ResIDT inResID);
		virtual	ResIDT		GetGraphicID(void) const;
		
//#if defined(QAP_BUILD)
//		virtual void		QapGetContents (PWCINFO pwc, short *pCount, short max);
//		virtual void		QapGetCDescriptorMax (char * cp_buf, short s_max);
//#endif
		
		virtual	void 		Draw(RgnHandle inSuperDrawRgnH);

	protected:

		enum {
			eBehaviour_Button = 0,
			eBehaviour_Radio,
			eBehaviour_Toggle
		};

		virtual	void		FinishCreateSelf(void);

		virtual void		ActivateSelf(void);
		virtual void		DeactivateSelf(void);
		
		virtual void		EnableSelf(void);
		virtual void		DisableSelf(void);
		
		virtual void		PrepareDrawButton(void);
		virtual	void		CalcTitleFrame(void);
		virtual	void		CalcGraphicFrame(void);
		
		virtual void		FinalizeDrawButton(void);
		virtual	void		DrawSelf(void);

		virtual	void		DrawSelfDisabled(void);
		virtual void		DrawButtonContent(void);
		virtual	void		DrawButtonTitle(void);
		virtual	void		DrawButtonGraphic(void);
				
		
		virtual void		HotSpotAction(
									Int16			inHotSpot,
									Boolean 		inCurrInside,
									Boolean			inPrevInside);
									
		virtual void		HotSpotResult(Int16 inHotSpot);
		virtual	void		SetValue(Int32 inValue);


		Boolean				mIsTrackInside;
		Int8				mTrackBehaviour;				
		Rect 				mCachedButtonFrame;
		Rect				mCachedTitleFrame;
		Rect				mCachedGraphicFrame;
		
		RgnHandle			mButtonMask;
		
		Int16				mOvalWidth;
		Int16				mOvalHeight;

		TString<Str255>		mTitle;
		ResIDT				mTitleTraitsID;	
		Int16				mTitleAlignment;
		Int16				mTitlePadPixels;
		
		ResType				mGraphicType;
		ResIDT				mGraphicID;
		Int16				mGraphicAlignment;
		Int16				mGraphicPadPixels;
		
		CIconHandle			mGraphicHandle;
		IconTransformType	mIconTransform;
		
private:
		virtual void		HandleEnablingPolicy();		
};

inline void CButton::SetTrackInside(Boolean inInside)
	{	mIsTrackInside = inInside;						}
inline Boolean CButton::IsTrackInside(void) const
	{	return mIsTrackInside;							}

inline Int8	CButton::GetBehaviour(void) const
	{	return mTrackBehaviour;							}

inline Boolean CButton::IsBehaviourButton(void) const
	{	return mTrackBehaviour == eBehaviour_Button;	}
inline Boolean CButton::IsBehaviourRadio(void) const
	{	return mTrackBehaviour == eBehaviour_Radio;		}
inline Boolean CButton::IsBehaviourToggle(void) const
	{	return mTrackBehaviour == eBehaviour_Toggle;	}
