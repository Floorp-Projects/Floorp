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

class CProgressBar : public LPane
{
	public:
		enum { class_ID = 'PgBr' };
		enum { eIndefinite = -1 };
								CProgressBar(LStream* inStream);
		virtual					~CProgressBar();

		virtual	void			SetValue(Int32 inValue);
		virtual	void			SetToIndefinite() {SetValue(eIndefinite);}
		virtual	Int32			GetValue(void) const;
//		virtual	void			SetValueRange(const Range32T& inRange);
		
	protected:
		virtual void			GetActiveColors(
									RGBColor& bodyColor,
									RGBColor& barColor,
									RGBColor& FrameColor) const;
		virtual void			ActivateSelf(void);
		virtual void			DeactivateSelf(void);
	
		virtual	void			FinishCreateSelf(void);
		virtual	void			DrawSelf(void);

		virtual	void			DrawIndefiniteBar(Rect& inBounds);

		virtual	void			DrawPercentageBar(
									Rect& 				inBounds,
									const RGBColor& 	inBarColor,
									const RGBColor& 	inBodyColor);

		Int32 					mValue;
		Uint32					mValueRange;
		PixPatHandle			mPolePattern;
		Int32					mPatOffset;
		Int32					mPatWidth;
		ResIDT					mPatternID;
};
