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

#include <LPopupButton.h>

//======================================
class CSizePopup: public LPopupButton
//======================================
{
public:
		enum
		{
			class_ID = 'Szpp'
		};
						CSizePopup(LStream* inStream);
	Int32				GetMenuSize() const;

	virtual void		SetFontSize(Int32 inFontSize);	// the value is font size in points
	Int32				GetFontSize() const	// the value is font size in points
						{
							return mFontSize;
						}

	virtual void		SetValue(Int32 inValue);
						
	void				MarkRealFontSizes(short fontNum);
	void				MarkRealFontSizes(LPopupButton *fontPopup);

	virtual	void		SetUpCurrentMenuItem(MenuHandle	inMenuH, Int16 inCurrentItem);
	
protected:

	virtual Int32		GetFontSizeFromMenuItem(Int32 inMenuItem) const;

	Int32				mFontSize;	
	short				mFontNumber;
	
}; // class CSizePopup
