/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
