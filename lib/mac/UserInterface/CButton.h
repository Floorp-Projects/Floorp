/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#pragma once

// Includes

#include <LControl.h>

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
