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
//	CProgressCaption.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once


#include "CProgressBar.h"

class CProgressCaption : public CProgressBar
{
	public:
		enum { class_ID = 'PgCp' };
		enum { eBarHidden = -2 };
								CProgressCaption(LStream* inStream);
		virtual					~CProgressCaption();

		virtual	void			SetHidden() {SetValue(eBarHidden);}
		virtual void			SetDescriptor(ConstStringPtr inDescriptor);
		virtual void			SetDescriptor(const char* inCDescriptor);
		virtual StringPtr		GetDescriptor(Str255 outDescriptor) const;

		virtual	void 			Draw(RgnHandle inSuperDrawRgnH);

		virtual	void			SetEraseColor(Int16 inPaletteIndex)
									{	mEraseColor = inPaletteIndex; }
	protected:
		virtual void			GetActiveColors(
									RGBColor& bodyColor,
									RGBColor& barColor,
									RGBColor& FrameColor) const;
	//DATA:
	protected:

		virtual	void			DrawSelf(void);

		TString<Str255>			mText;
		ResIDT					mBoundedTraitsID;		// normal progress bar
		ResIDT					mIndefiniteTraitsID;	// indefinite progress bar
		ResIDT					mHiddenTraitsID;		// hidden progress bar
		Int16					mEraseColor;
};

