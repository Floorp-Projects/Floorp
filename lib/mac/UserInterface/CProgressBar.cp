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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <LView.h>
#include <UDrawingState.h>

#include "CProgressBar.h"
#include "UGraphicGizmos.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CProgressBar::CProgressBar(LStream* inStream)
	:	LPane(inStream)
{
	*inStream >> mPatternID;
	
	mPatOffset = 0;
	mValue = 0;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CProgressBar::~CProgressBar()
{
	if (mPolePattern != NULL)
	::DisposePixPat(mPolePattern);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressBar::FinishCreateSelf(void)
{
	mPolePattern = ::GetPixPat(mPatternID);
	ThrowIfNULL_(mPolePattern);
	
	PixMapHandle theMap = (**mPolePattern).patMap;
	mPatWidth = RectWidth((*theMap)->bounds);
	
	mValueRange = 100;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressBar::ActivateSelf(void)
{
	if (FocusExposed())
		Refresh(); //Draw(NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressBar::DeactivateSelf(void)
{
	if (FocusExposed())
		Refresh(); //Draw(NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressBar::GetActiveColors(
	RGBColor& bodyColor,
	RGBColor& barColor,
	RGBColor& frameColor) const
{
	bodyColor.red = 0xCCCC;
	bodyColor.green = 0xCCCC;
	bodyColor.blue = 0xFFFF;
	frameColor.red = 0;
	frameColor.green = 0;
	frameColor.blue = 0;
	barColor.red = 0x4444;
	barColor.green = 0x4444;
	barColor.blue = 0x4444;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressBar::DrawSelf(void)
{
	StColorPenState theState;
	theState.Normalize();	

	// Do not assume that we are drawing in a window. It may be
	// an offscreen port.
	GrafPtr	thePort;
	::GetPort(&thePort);

	AuxWinHandle theAuxHandle;
	::GetAuxWin(thePort, &theAuxHandle);
	CTabHandle theColorTable = (**theAuxHandle).awCTable;

	RGBColor theWindowBack;

	UGraphicGizmos::FindInColorTable(theColorTable, wContentColor, theWindowBack);

	RGBColor theBodyColor, theFrameColor, theBarColor;
#if 0
	if (IsActive())
#endif
		GetActiveColors(theBodyColor, theBarColor, theFrameColor);
#if 0
	else
		{
		// This looks ugly. It used to look "OK" in previous versions.
		// However, drawing it with the active colors looks better.
		GDHandle theGD = ::GetGDevice();
		::GetGray(theGD, &theWindowBack, &theFrameColor);
		::GetGray(theGD, &theWindowBack, &theBarColor);
		::GetGray(theGD, &theWindowBack, &theBodyColor);
		}
#endif
		
#if 0
	RGBColor theBodyColor = { 0xCCCC, 0xCCCC, 0xFFFF };
	RGBColor theFrameColor = { 0, 0, 0 };
	RGBColor theBarColor = { 0x4444, 0x4444, 0x4444 };

	
	if (!IsActive())
		{
		GDHandle theGD = ::GetGDevice();
		::GetGray(theGD, &theWindowBack, &theFrameColor);
		::GetGray(theGD, &theWindowBack, &theBarColor);
		::GetGray(theGD, &theWindowBack, &theBodyColor);
		}
#endif	
	::RGBForeColor(&theFrameColor);
	::RGBBackColor(&theWindowBack);

	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	::FrameRect(&theFrame);
	::InsetRect(&theFrame, 1, 1);

	if (GetValue() == eIndefinite)
		DrawIndefiniteBar(theFrame);
	else
		DrawPercentageBar(theFrame, theBarColor, theBodyColor);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressBar::DrawIndefiniteBar(Rect& inBounds)
{
	Rect theBoundsCopy = inBounds;

	// Get our drawing port, and save the origin state.
	// Don't assume we are onscreen (GetMacPort() would do that).
	GrafPtr	thePort;
	GetPort(&thePort);	
	StPortOriginState theOriginSaver(thePort);

	// fake the pattern into thinking it's drawn somewhere else	
	Point theOrigin;
	theOrigin.h = thePort->portRect.left;
	theOrigin.v = thePort->portRect.top;
	::SetOrigin(theOrigin.h - mPatOffset, theOrigin.v);

	// Adjust the drawing frame for the shift in port origin
	::OffsetRect(&theBoundsCopy, -mPatOffset, 0);

	// changing the origion means that we must offset the clip region
	StClipRgnState theClipSaver;
	::ClipRect(&theBoundsCopy);
//	StRegion		tempClipRgn;	// This doesn't work, but I don't know why.
//	::GetClip(tempClipRgn);
//	::OffsetRgn(tempClipRgn, - mPatOffset, 0);
//	::SetClip(tempClipRgn);

	::FillCRect(&theBoundsCopy, mPolePattern);
	mPatOffset = (mPatOffset + 1) % mPatWidth;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressBar::DrawPercentageBar(
	Rect& 				inBounds,
	const RGBColor& 	inBarColor,
	const RGBColor& 	inBodyColor)
{
	Int32 theStart = 0;
	Int32 theEnd = mValueRange;

	Int32 theValue = GetValue();
	if (theValue < theStart)
		theValue = theStart;
	else if (theValue > theEnd)
		theValue = theEnd;
		
	Int32 thePercentage = ((theValue * 100) / theEnd);
	
	Rect theBox = inBounds;
	theBox.right = theBox.left + (thePercentage * RectWidth(inBounds)) / 100;
	::RGBForeColor(&inBarColor);
	::RGBBackColor(&inBodyColor);
	::PaintRect(&theBox);

	theBox.left = theBox.right;
	theBox.right = inBounds.right;
	::RGBForeColor(&inBodyColor);
	::PmBackColor(eStdGrayBlack);

	::PaintRect(&theBox);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int32 CProgressBar::GetValue() const
{
	return mValue;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressBar::SetValue(Int32 inValue)
{
	if ((inValue != mValue) || (inValue == eIndefinite))
		{
		mValue = inValue;
		if (FocusExposed())
			Draw(NULL);
		}
}
