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

#include "CToolbarButton.h"
#include "UGraphicGizmos.h"

// "Magic" constants
const Int16 cIconOnlyHeight = 32;
const Int16 cIconOnlyWidth = 36;
const Int16 cTextOnlyHeight = 21;

CToolbarButton::CToolbarButton(LStream* inStream)
	:	CButton(inStream),
		mCurrentMode(defaultMode),
		mOriginalWidth(0),
		mOriginalHeight(0)
{
}

CToolbarButton::~CToolbarButton()
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	FinishCreateSelf
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CToolbarButton::FinishCreateSelf()
{
	CButton::FinishCreateSelf();

	Rect theButtonRect;
	CalcLocalFrameRect(theButtonRect);

	mOriginalWidth = theButtonRect.right - theButtonRect.left;
	mOriginalHeight = theButtonRect.bottom - theButtonRect.top;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawSelf
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolbarButton::DrawSelf()
{
	PrepareDrawButton();

	DrawButtonContent();

	if (mCurrentMode != eMode_IconsOnly && mTitle.Length() > 0)
		DrawButtonTitle();

	if (mCurrentMode != eMode_TextOnly && GetGraphicID() != 0)
		DrawButtonGraphic();
			
	if (!IsEnabled() || !IsActive())
		DrawSelfDisabled();
			
	FinalizeDrawButton();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ChangeMode
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CToolbarButton::ChangeMode(Int8 inNewMode, SDimension16& outDimensionDeltas)
{
	SDimension16 oldDimensions;
	GetFrameSize(oldDimensions);

	outDimensionDeltas.width = 0;
	outDimensionDeltas.height = 0;

	mCurrentMode = inNewMode;

	switch (inNewMode)
	{
		case eMode_IconsOnly:
			outDimensionDeltas.width = cIconOnlyWidth - oldDimensions.width;
			outDimensionDeltas.height = cIconOnlyHeight - oldDimensions.height;
			break;
		
		case eMode_TextOnly:
			outDimensionDeltas.width = mOriginalWidth - oldDimensions.width;
			outDimensionDeltas.height = cTextOnlyHeight - oldDimensions.height;
			break;
		
		case eMode_IconsAndText:
			outDimensionDeltas.width = mOriginalWidth - oldDimensions.width;
			outDimensionDeltas.height = mOriginalHeight - oldDimensions.height;
			break;
	}

	ResizeFrameBy(outDimensionDeltas.width, outDimensionDeltas.height, true);
	return true;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawButtonTitle
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolbarButton::DrawButtonTitle(void)
{
	if (mCurrentMode == eMode_TextOnly && (!IsActive() || !IsEnabled()))
		::TextMode(grayishTextOr);  // this is so light you cant see it.

	CButton::DrawButtonTitle();
}

// Since we have constant heights for eMode_TextOnly and eMode_IconOnly,
// we need to ignore mTitlePadPixels and mGraphicPadPixels

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcTitleFrame
//
//	This calculates the bounding box of the title (if any).  This is useful
//	for both the string placement, as well as position the button graphic
//	(again, if any).
//
//	Note that this routine sets the text traits for the ensuing draw.  If
//	you override this method, make sure that you're doing the same.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CToolbarButton::CalcTitleFrame(void)
{
	if (mTitle.Length() == 0)
		return;

	UTextTraits::SetPortTextTraits(mTitleTraitsID);

	FontInfo theInfo;
	::GetFontInfo(&theInfo);
	mCachedTitleFrame.top = mCachedButtonFrame.top;
	mCachedTitleFrame.left = mCachedButtonFrame.left;		
	mCachedTitleFrame.right = mCachedTitleFrame.left + ::StringWidth(mTitle);;
	mCachedTitleFrame.bottom = mCachedTitleFrame.top + theInfo.ascent + theInfo.descent + theInfo.leading;;

	if (mCurrentMode != eMode_TextOnly)
	{
		UGraphicGizmos::AlignRectOnRect(mCachedTitleFrame, mCachedButtonFrame, mTitleAlignment);
		UGraphicGizmos::PadAlignedRect(mCachedTitleFrame, mTitlePadPixels, mTitleAlignment);
	}
	else
	{
		UGraphicGizmos::CenterRectOnRect(mCachedTitleFrame, mCachedButtonFrame);
	}
}
