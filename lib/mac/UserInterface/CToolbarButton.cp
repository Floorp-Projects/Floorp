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

#include "CToolbarButton.h"
#include "CToolbarModeManager.h"
#include "UGraphicGizmos.h"

// "Magic" constants
const Int16 cIconOnlyHeight = 32;
const Int16 cIconOnlyWidth = 36;
const Int16 cTextOnlyHeight = 21;

CToolbarButton::CToolbarButton(LStream* inStream)
	:	CButton(inStream),
		mCurrentMode(CToolbarModeManager::defaultToolbarMode),
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

	if (mCurrentMode != eTOOLBAR_ICONS && mTitle.Length() > 0)
		DrawButtonTitle();

	if (mCurrentMode != eTOOLBAR_TEXT && GetGraphicID() != 0)
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
		case eTOOLBAR_ICONS:
			outDimensionDeltas.width = cIconOnlyWidth - oldDimensions.width;
			outDimensionDeltas.height = cIconOnlyHeight - oldDimensions.height;
			break;
		
		case eTOOLBAR_TEXT:
			outDimensionDeltas.width = mOriginalWidth - oldDimensions.width;
			outDimensionDeltas.height = cTextOnlyHeight - oldDimensions.height;
			break;
		
		case eTOOLBAR_TEXT_AND_ICONS:
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
	if (mCurrentMode == eTOOLBAR_TEXT && (!IsActive() || !IsEnabled()))
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

	if (mCurrentMode != eTOOLBAR_TEXT)
	{
		UGraphicGizmos::AlignRectOnRect(mCachedTitleFrame, mCachedButtonFrame, mTitleAlignment);
		UGraphicGizmos::PadAlignedRect(mCachedTitleFrame, mTitlePadPixels, mTitleAlignment);
	}
	else
	{
		UGraphicGizmos::CenterRectOnRect(mCachedTitleFrame, mCachedButtonFrame);
	}
}
