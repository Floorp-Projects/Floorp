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


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CGAStatusBar.h"

#include <UGAColorRamp.h>
#include <UGraphicsUtilities.h>


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

#pragma mark -

/*======================================================================================
	Constructor.
======================================================================================*/

CGAStatusBar::CGAStatusBar(LStream *inStream) :
						   LGABox(inStream),
						   mIndefiniteProgessPatH(nil) {
	
	mValue = eHiddenValue;
	mHasBorder = true;
	mBorderStyle = borderStyleGA_RecessedOneBorder;
	mTitlePosition = titlePositionGA_None;
	mCurIndefiniteIndex = 0;
	mNextIndefiniteDraw = 0;
	Assert_((mUserCon < 0) || ((mUserCon <= 100) && (mUserCon >= 0)));
}


/*======================================================================================
	Destructor.
======================================================================================*/

CGAStatusBar::~CGAStatusBar(void) {
	
	if ( mIndefiniteProgessPatH != nil ) {
		::DisposePixPat(mIndefiniteProgessPatH);
		mIndefiniteProgessPatH = nil;
	}
}


/*======================================================================================
	Get the status title.
======================================================================================*/

StringPtr CGAStatusBar::GetDescriptor(Str255 outDescriptor) const {

	return Inherited::GetDescriptor(outDescriptor);
}


/*======================================================================================
	Set the status title.
======================================================================================*/

void CGAStatusBar::SetDescriptor(ConstStr255Param inDescriptor) {

	if ( mTitle != inDescriptor ) {
		if ( FocusExposed() ) {
			Rect titleRect1, titleRect2;
			CalcTitleRect(titleRect1);
			mTitle = inDescriptor;
			CalcTitleRect(titleRect2);
			::PenNormal();
			ApplyForeAndBackColors();
			if ( titleRect1.right > (titleRect2.right - 1) ) {
				titleRect1.left = titleRect2.right - 1;
				::EraseRect(&titleRect1);
			}
			DrawBoxTitle();
		} else {
			mTitle = inDescriptor;
		}
	}
}


/*======================================================================================
	Get the current progress value (0..100) or eIndefiniteValue.
======================================================================================*/

Int32 CGAStatusBar::GetValue(void) const {

	return mValue;
}


/*======================================================================================
	Set the current progress value (0..100) or eIndefiniteValue.
======================================================================================*/

void CGAStatusBar::SetValue(Int32 inValue) {

	if ( inValue == eIndefiniteValue ) {
		if ( mNextIndefiniteDraw < LMGetTicks() ) {
			if ( ++mCurIndefiniteIndex > eMaxIndefiniteIndex ) {
				mCurIndefiniteIndex = 0;
			}
		} else {
			mValue = inValue;
			return;
		}
	} else if ( inValue != eHiddenValue ) {
		if ( inValue < eMinValue ) {
			inValue = eMinValue;
		} else if ( inValue > eMaxValue ) {
			inValue = eMaxValue;
		}
		if ( mValue == inValue ) return;
	} else {
		if ( mValue != inValue ) {
			mValue = inValue;
			Refresh();
		}
		return;
	}
	
	mValue = inValue;
	if ( FocusExposed() ) DrawBoxBorder();
	if ( mValue == eIndefiniteValue ) mNextIndefiniteDraw = LMGetTicks() + 2;
}


/*======================================================================================
	Draw the b&w progress bar.
======================================================================================*/

void CGAStatusBar::DrawBoxTitle(void) {

	StTextState theTextState;
	StColorPenState theColorPenState;
	Rect titleRect;
	CalcTitleRect(titleRect);
	
	UTextTraits::SetPortTextTraits(GetTextTraitsID());
	
	Str255 boxTitle;
	GetDescriptor(boxTitle);
	
	{
		RGBColor textColor;
		::GetForeColor(&textColor);
		ApplyForeAndBackColors();
		::RGBForeColor(&textColor);
	}
	
	StClipRgnState clipState(titleRect);
	
	UStringDrawing::DrawJustifiedString(boxTitle, titleRect, teJustCenter );
}


/*======================================================================================
	Draw the b&w progress bar.
======================================================================================*/

void CGAStatusBar::DrawBlackAndWhiteBorder(const Rect &inBorderRect,
								     	   EGABorderStyle inBorderStyle) {

	#pragma unused(inBorderStyle)
	
	if ( mValue == eHiddenValue ) return;
	
	DrawProgressBar(inBorderRect, false);
}


/*======================================================================================
	Draw the color progress bar.
======================================================================================*/

void CGAStatusBar::DrawColorBorder(const Rect &inBorderRect,
								   EGABorderStyle inBorderStyle) {

	#pragma unused(inBorderStyle)
	
	if ( mValue == eHiddenValue ) return;
	
	Inherited::DrawColorBorder(inBorderRect, inBorderStyle);
	
	DrawProgressBar(inBorderRect, true);
}


/*======================================================================================
	Draw the progress bar.
======================================================================================*/

void CGAStatusBar::DrawProgressBar(const Rect &inBorderRect, Boolean inUseColor) {

	StColorPenState::Normalize();
	Rect drawRect = inBorderRect;

	::InsetRect(&drawRect, 1, 1);
	::FrameRect(&drawRect);
	::InsetRect((Rect *) &drawRect, 1, 1);

	if ( mValue != eIndefiniteValue ) {
		Int16 mid = drawRect.left + (((drawRect.right - drawRect.left) * mValue) / eMaxValue);
		Int16 right = drawRect.right;

		drawRect.right = mid;
		if ( inUseColor ) {
			::RGBForeColor(&UGAColorRamp::GetColor(11));
		}
		::PaintRect(&drawRect);
		drawRect.right = right;
		drawRect.left = mid;
		if ( inUseColor ) {
			::RGBForeColor(&UGAColorRamp::GetColor(colorRamp_Purple1));
			::PaintRect(&drawRect);
			::ForeColor(blackColor);
		} else {
			::EraseRect(&drawRect);
		}
	} else {
		if ( mIndefiniteProgessPatH == nil ) {
			mIndefiniteProgessPatH = ::GetPixPat(mTextTraitsID);
		}
		if ( mIndefiniteProgessPatH != nil ) {
			Point saveOrigin = topLeft(GetMacPort()->portRect);
			::SetOrigin(saveOrigin.h - mCurIndefiniteIndex, saveOrigin.v);
			::OffsetRgn(GetMacPort()->clipRgn, -mCurIndefiniteIndex, 0);
			::OffsetRect(&drawRect, -mCurIndefiniteIndex, 0);
			::PenPixPat(mIndefiniteProgessPatH);
			::PaintRect(&drawRect);
			StColorPenState::Normalize();
			::OffsetRgn(GetMacPort()->clipRgn, mCurIndefiniteIndex, 0);
			::SetOrigin(saveOrigin.h, saveOrigin.v);
		}
	}
}


/*======================================================================================
	Calculate the rectagle used for drawing the status bar.
======================================================================================*/

static const Int16 cStatusBarHeight = 12;
static const Int16 cStatusBarTitleMargin = 5;

void CGAStatusBar::CalcBorderRect(Rect &outRect) {

	CalcLocalFrameRect(outRect);
	
	if ( mValue == eHiddenValue ) {
		outRect.right = outRect.left;
		outRect.bottom = outRect.top;
		return;
	} else if ( mUserCon < 0 ) {
		outRect.left = outRect.right + mUserCon;
	} else {
		const Int32 width = outRect.right - outRect.left;
		outRect.left = outRect.right - ((width * mUserCon) / 100L);
	}
	
	const Int32 height = outRect.bottom - outRect.top;
	outRect.top += (height - cStatusBarHeight) / 2;
	outRect.bottom = outRect.top + cStatusBarHeight;
}


/*======================================================================================
	Nothing.
======================================================================================*/

void CGAStatusBar::CalcContentRect(Rect &outRect) {

	CalcLocalFrameRect(outRect);
	::SetRect(&outRect, outRect.left, outRect.top, outRect.left, outRect.top);
}


/*======================================================================================
	Calculate the rectangle for drawing the status text.
======================================================================================*/

void CGAStatusBar::CalcTitleRect(Rect &outRect) {

	CalcLocalFrameRect(outRect);
	
	Int16 txHeight, txWidth;
	CalculateTitleHeightAndWidth(&txHeight, &txWidth);
	
	if ( mUserCon < 0 ) {
		outRect.right += mUserCon;
	} else {
		const Int32 width = outRect.right - outRect.left;
		outRect.right -= ((width * mUserCon) / 100L);
	}
	outRect.right -= cStatusBarTitleMargin;
	
	if ( outRect.right < outRect.left ) outRect.right = outRect.left;
	if ( (outRect.right - outRect.left) > txWidth ) {
		outRect.right = outRect.left + txWidth;
	}
}


