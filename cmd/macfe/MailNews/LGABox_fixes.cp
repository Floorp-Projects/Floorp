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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


// LGABox_fixes.cp

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "LGABox_fixes.h"


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/


/*======================================================================================
	Turds be gone! Assumes entire frame is visible through superview.
======================================================================================*/

void LGABox_fixes::ResizeFrameBy(Int16 inWidthDelta, Int16 inHeightDelta,
								 Boolean inRefresh) {

	
	if ( mHasBorder && (mBorderStyle != borderStyleGA_NoBorder) && !mRefreshAllWhenResized ) {
		Rect borderRect;
		CalcBorderRect(borderRect);
		Point beforeBotRight = botRight(borderRect);
		Rect invalRect;
		CalcPortFrameRect(invalRect);
		Point portBeforeBotRight = botRight(invalRect);

		inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);

		CalcPortFrameRect(invalRect);
		CalcBorderRect(borderRect);
		Point portAfterBotRight = botRight(invalRect);

		if ( inWidthDelta != 0 ) {
			if ( beforeBotRight.h < borderRect.right ) {
				::SetRect(&invalRect, beforeBotRight.h - mContentOffset.right, 
						  borderRect.top, portBeforeBotRight.h, portBeforeBotRight.v);
			} else {
				::SetRect(&invalRect, borderRect.right - mContentOffset.right, 
						  borderRect.top, portAfterBotRight.h, portAfterBotRight.v);
			}
			LocalToPortPoint(topLeft(invalRect));
			LocalToPortPoint(botRight(invalRect));
			InvalPortRect(&invalRect);
		}
		if ( inHeightDelta != 0 ) {
			if ( beforeBotRight.v < borderRect.bottom ) {
				::SetRect(&invalRect, borderRect.left, beforeBotRight.v - mContentOffset.bottom,
						  portBeforeBotRight.h, portBeforeBotRight.v);
			} else {
				::SetRect(&invalRect, borderRect.left, borderRect.bottom - mContentOffset.bottom,
						  portAfterBotRight.h, portAfterBotRight.v);
			}
			LocalToPortPoint(topLeft(invalRect));
			LocalToPortPoint(botRight(invalRect));
			InvalPortRect(&invalRect);
		}
	} else {
		inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	}
}


/*======================================================================================
	Fix bug where everything was not refreshed correctly.
======================================================================================*/

void LGABox_fixes::RefreshBoxBorder(void) {

	Rect theFrame;
	if ( IsVisible() && CalcLocalFrameRect(theFrame) && (mSuperView) ) {
		Rect revealed;
		mSuperView->GetRevealedRect(revealed);
		Point delta = topLeft(revealed);
		PortToLocalPoint(topLeft(revealed));
		PortToLocalPoint(botRight(revealed));
		delta.h -= revealed.left; delta.v -= revealed.top;
		if ( ::SectRect(&theFrame, &revealed, &revealed) ) {
			RgnHandle borderRgn = GetBoxBorderRegion(revealed);
			::OffsetRgn(borderRgn, delta.h, delta.v);
			InvalPortRgn(borderRgn);
			::DisposeRgn(borderRgn);
		}
	}
}


/*======================================================================================
	Fix bug where everything was not refreshed correctly.
======================================================================================*/

void LGABox_fixes::RefreshBoxTitle(void) {

	Rect theFrame;
	if ( IsVisible() && CalcPortFrameRect(theFrame) && (mSuperView) ) {
		Rect revealed;
		mSuperView->GetRevealedRect(revealed);
		if ( ::SectRect(&theFrame, &revealed, &theFrame) ) {
			Rect titleRect;
			CalcTitleRect(titleRect);
			LocalToPortPoint(topLeft(titleRect));
			LocalToPortPoint(botRight(titleRect));
			if ( ::SectRect(&titleRect, &revealed, &titleRect) ) {
				InvalPortRect(&titleRect);
			}
		}
	}
}


/*======================================================================================
	Fix bug where everything was not refreshed correctly.
======================================================================================*/

void LGABox_fixes::Disable(void) {

	LView::Disable();
	
	RefreshBoxBorder();
	RefreshBoxTitle();
}


/*======================================================================================
	Get rid of the flicker.
======================================================================================*/

void LGABox_fixes::Enable(void) {

	LView::Enable();
	
	RefreshBoxBorder();
	RefreshBoxTitle();
}


