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

#ifndef __H_CGAStatusBar
#define __H_CGAStatusBar
#pragma once

/*======================================================================================
	AUTHOR:			Ted Morris - 01 NOV 96

	DESCRIPTION:	Implements a class for drawing a status bar and associated status
					text.
					
					If the mUserCon is negative, specifies the width of the status bar
					in pixels. If positive, specifies a percentage (0..100) of the width
					of the pane for the width of the status bar.
					
	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include <LGABox.h>


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark -

class CGAStatusBar : protected LGABox {

#if !defined(__MWERKS__) || (__MWERKS__ >= 0x2000)
	typedef LGABox inherited;
#endif
				  
public:

						enum { class_ID = 'Gbar' };

						CGAStatusBar(LStream *inStream);
	virtual				~CGAStatusBar(void);

	virtual	StringPtr	GetDescriptor(Str255 outDescriptor) const;
	virtual	void		SetDescriptor(ConstStr255Param inDescriptor);

	virtual	Int32		GetValue(void) const;
	
						enum { eIndefiniteValue = -1, eHiddenValue = -2, 
							   eMaxValue = 100, eMinValue = 0,
							   eMaxIndefiniteIndex = 15 };
	virtual	void		SetValue(Int32 inValue);

protected:

	virtual	void		DrawBoxTitle(void);
	virtual	void		DrawColorBorder(const Rect &inBorderRect,
										EGABorderStyle inBorderStyle);
	virtual	void		DrawBlackAndWhiteBorder(const Rect &inBorderRect,
												EGABorderStyle inBorderStyle);
	void				DrawProgressBar(const Rect &inBorderRect, Boolean inUseColor);


	virtual	void		CalcBorderRect(Rect &outRect);
	virtual	void		CalcContentRect(Rect &outRect);
	virtual	void		CalcTitleRect(Rect &outRect);
	
	// Instance variables
	
	Int16				mValue;
	Int16				mCurIndefiniteIndex;
	PixPatHandle		mIndefiniteProgessPatH;
	UInt32				mNextIndefiniteDraw;
};


#endif // __H_CGAStatusBar
