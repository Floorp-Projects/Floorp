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

#include <UMemoryMgr.h>
#include <UDrawingState.h>
#include <UDrawingUtils.h>
#include <UGAColorRamp.h>

#include "UGraphicGizmos.h"
#include "UGAAppearance.h"

// GA Bevel tints.

Uint16	UGAAppearance::sGAOneGrayLevel 		= 0x1111;
Uint16	UGAAppearance::sGATwoGrayLevels 	= 0x2222;
Uint16	UGAAppearance::sGAThreeGrayLevels 	= 0x3333;
Uint16	UGAAppearance::sGAFourGrayLevels 	= 0x4444;
Uint16	UGAAppearance::sGAFiveGrayLevels 	= 0x5555;
Uint16	UGAAppearance::sGASixGrayLevels 	= 0x6666;
Uint16	UGAAppearance::sGASevenGrayLevels 	= 0x7777;

Uint16	UGAAppearance::sGAHiliteContentTint	= 0x0500;

const Int16 gsPopup_ArrowButtonWidth 	= 	22;	//	Width used in drawing the arrow only


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawGAButtonBevelTint
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This function is based in part on LGAPushButton::DrawButtonNormalColor().
//	Given a rect and assuming the content of the rect is filled with 87% gray
//	(0xDDDD, 0xDDDD, 0xDDDD), this function will apply tints such that it will
//	look like a GA button

void UGAAppearance::DrawGAButtonBevelTint(
	const Rect &inRect)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect frame = inRect;

	//::RGBForeColor(&UGAColorRamp::GetBlackColor());
	::RGBForeColor(&UGAColorRamp::GetColor(colorRamp_Gray8));
	::FrameRoundRect(&frame, 8, 8);

	// bevel light side
	::InsetRect(&frame, 2, 2);
	frame.bottom -= 1;
	frame.right -= 1;
	SBooleanRect sides = { true, true, false, false };
	UGraphicGizmos::BevelTintPartialRect(frame, 1, sGATwoGrayLevels, sGAThreeGrayLevels, sides);

	// corner hilites			
	UGraphicGizmos::BevelTintPixel(inRect.left + 3, inRect.top + 3, sGATwoGrayLevels, true);
	UGraphicGizmos::BevelTintPixel(inRect.left + 1, inRect.top + 2, sGAThreeGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(inRect.left + 2, inRect.top + 1, sGAThreeGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(inRect.left + 1, inRect.bottom - 3, sGAThreeGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(inRect.left + 2, inRect.bottom - 2, sGAThreeGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(inRect.right - 3, inRect.top + 1, sGAThreeGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(inRect.right - 2, inRect.top + 2, sGAThreeGrayLevels, false);

	// bevel dark side
	::OffsetRect(&frame, 1, 1);
	sides.left = sides.top = false;
	sides.right = sides.bottom = true;
	UGraphicGizmos::BevelTintPartialRect(frame, 1, sGATwoGrayLevels, sGAThreeGrayLevels, sides);

	UGraphicGizmos::BevelTintLine(frame.left, frame.bottom,
								  frame.right, frame.bottom,
								  sGASixGrayLevels, false);

	UGraphicGizmos::BevelTintLine(frame.right, frame.bottom - 1,
								  frame.right, frame.top,
								  sGASixGrayLevels, false);

	// lower right corner pixel dark "hilites"
	UGraphicGizmos::BevelTintPixel(inRect.right - 3, inRect.bottom - 3, sGAThreeGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(inRect.right - 4, inRect.bottom - 4, sGAThreeGrayLevels, false);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawGAButtonPressedBevelTint
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This function is based in part on LGAPushButton::DrawButtonHilitedColor().
//	Given a rect and assuming the content of the rect is filled with 40% gray
//	(0x6666, 0x6666, 0x6666), this function will apply tints such that it will
//	look like a GA button that's pressed down.

void UGAAppearance::DrawGAButtonPressedBevelTint(
	const Rect &inRect)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect frame = inRect;

	// Draw frame
	//::RGBForeColor(&UGAColorRamp::GetBlackColor());
	::RGBForeColor(&UGAColorRamp::GetColor(colorRamp_Gray8));
	::FrameRoundRect(&frame, 8, 8);

	// ¥ LIGHT EDGES
	// ¥ Start by rendering the bevelled edges of the sides facing the light
	// source
	// Draw first dark bevel
	// Left
	UGraphicGizmos::BevelTintLine(frame.left + 1, frame.top + 2,
								  frame.left + 1, frame.bottom - 2,
								  sGATwoGrayLevels, false);
	// Top
	UGraphicGizmos::BevelTintLine(frame.left + 2, frame.top + 1,
								  frame.right - 2, frame.top + 1,
								  sGATwoGrayLevels, false);

	// second topleft dark bevel 
	::InsetRect(&frame, 2, 2);
	frame.bottom -= 1;
	frame.right -= 1;
	SBooleanRect sides = { true, true, false, false };
	UGraphicGizmos::BevelTintPartialRect(frame, -1, sGAOneGrayLevel, sGAOneGrayLevel, sides);

	// corner "hilites"
	UGraphicGizmos::BevelTintPixel(inRect.left + 3, inRect.top + 3, sGAOneGrayLevel, false);
	UGraphicGizmos::BevelTintPixel(inRect.left + 2, inRect.top + 2, sGAOneGrayLevel, false);
	UGraphicGizmos::BevelTintPixel(inRect.left + 2, inRect.bottom - 2, sGAOneGrayLevel, true);
	UGraphicGizmos::BevelTintPixel(inRect.right - 2, inRect.top + 2, sGAOneGrayLevel, true);

	// ¥ SHADOW EDGES
	::OffsetRect(&frame, 1, 1);
	sides.left = sides.top = false;
	sides.right = sides.bottom = true;
	UGraphicGizmos::BevelTintPartialRect(frame, -1, sGAOneGrayLevel, sGAOneGrayLevel, sides);

	UGraphicGizmos::BevelTintLine(frame.left, frame.bottom,
								  frame.right, frame.bottom,
								  sGATwoGrayLevels, true);

	UGraphicGizmos::BevelTintLine(frame.right, frame.bottom - 1,
								  frame.right, frame.top,
								  sGATwoGrayLevels, true);

	// lower right corner pixel dark "hilites"
	UGraphicGizmos::BevelTintPixel(inRect.right - 3, inRect.bottom - 3, sGAOneGrayLevel, true);
	UGraphicGizmos::BevelTintPixel(inRect.right - 4, inRect.bottom - 4, sGAOneGrayLevel, true);	
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawGAButtonDimmedBevelTint
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This function is based in part on LGAPushButton::DrawButtonDimmedColor().
//	Given a rect and assuming the content of the rect is filled with 87% gray
//	(0xDDDD, 0xDDDD, 0xDDDD), this function will apply tints such that it will
//	look like a dimmed GA button

void UGAAppearance::DrawGAButtonDimmedBevelTint(
	const Rect &inRect)
{
	Rect frame = inRect;

	// ¥ Draw the frame for the control outside of everything else
//	::RGBForeColor ( &UGAColorRamp::GetColor(colorRamp_Gray7) );
//	::FrameRoundRect ( &frame, 8, 8 );
	UGraphicGizmos::BevelTintRoundRect(frame, 8, 8, sGAFiveGrayLevels, false);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawGAPopupBevelTint
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This function is based in part on LGAPopup::DrawPopupNormalColor().
//	Given a rect and assuming the content of the rect is filled with 87% gray
//	(0xDDDD, 0xDDDD, 0xDDDD), this function will apply tints such that it will
//	look like a GA popup menu

void UGAAppearance::DrawGAPopupBevelTint(
	const Rect &inRect)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect frame = inRect;

	// Draw frame
	::RGBForeColor(&UGAColorRamp::GetBlackColor());
	::FrameRoundRect(&frame, 8, 8);

	// ¥ LIGHT EDGES
	// ¥ Start by rendering the bevelled edges of the sides facing the light
	// source
	// Draw first dark bevel
	// Left
	UGraphicGizmos::BevelTintLine(frame.left + 1, frame.top + 2,
								  frame.left + 1, frame.bottom - 2,
								  sGATwoGrayLevels, true);
	// Top
	UGraphicGizmos::BevelTintLine(frame.left + 2, frame.top + 1,
								  frame.right - gsPopup_ArrowButtonWidth - 1, frame.top + 1,
								  sGATwoGrayLevels, true);

	// ¥ SHADOW EDGES
	// ¥ Render the shadow bevels
	// Bottom
	UGraphicGizmos::BevelTintLine(frame.left + 2, frame.bottom - 2,
								  frame.right - gsPopup_ArrowButtonWidth, frame.bottom - 2,
								  sGAThreeGrayLevels, false);
	// Right
	UGraphicGizmos::BevelTintLine(frame.right - gsPopup_ArrowButtonWidth, frame.top + 2,
								  frame.right - gsPopup_ArrowButtonWidth, frame.bottom - 3,
								  sGAThreeGrayLevels, false);
	// corner hilite
	UGraphicGizmos::BevelTintPixel(frame.right - gsPopup_ArrowButtonWidth, frame.top + 1,
								   sGAOneGrayLevel, true);
	// ¥¥ EDGES ON ARROW BUTTON
	// ¥ LIGHT EDGES
	// ¥ Start by rendering the bevelled edges of the sides facing the light
	// source
	// Left
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 2), frame.top + 2,
								  frame.right - (gsPopup_ArrowButtonWidth - 2), frame.bottom - 4,
								  sGATwoGrayLevels, true);
	// Top
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 3), frame.top + 2,
								  frame.right - 4, frame.top + 2,
								  sGATwoGrayLevels, true);
	// ¥ SHADOW EDGES
	// ¥ Render the shadow bevels
	// Light shadow bevel
	// Bottom
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 3), frame.bottom - 3,
								  frame.right - 3, frame.bottom - 3,
								  sGAThreeGrayLevels, false);
	// Right
	UGraphicGizmos::BevelTintLine(frame.right - 3, frame.top + 3,
								  frame.right - 3, frame.bottom - 4,
								  sGAThreeGrayLevels, false);
	// Dark shadow bevel
	// Bottom
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 2), frame.bottom - 2,
								  frame.right - 2, frame.bottom - 2,
								  sGASixGrayLevels, false);
	// Right
	UGraphicGizmos::BevelTintLine(frame.right - 2, frame.top + 3,
								  frame.right - 2, frame.bottom - 2,
								  sGASixGrayLevels, false);
	// corner hilites
	UGraphicGizmos::BevelTintPixel(frame.right - 3, frame.top + 1,
								   sGATwoGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(frame.right - (gsPopup_ArrowButtonWidth - 1), frame.bottom - 2,
								   sGATwoGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(frame.right - 2, frame.top + 2,
								   sGAThreeGrayLevels, false);
	UGraphicGizmos::BevelTintPixel(frame.right - 3, frame.bottom - 3,
								   sGAThreeGrayLevels, false);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawGAPopupPressedBevelTint
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This function is based in part on LGAPopup::DrawPopupHilitedColor().
//	Given a rect and assuming the content of the rect is filled with 40% gray
//	(0x6666, 0x6666, 0x6666), this function will apply tints such that it will
//	look like a GA popup menu that's pressed down.

void UGAAppearance::DrawGAPopupPressedBevelTint(
	const Rect	&inRect)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect frame = inRect;

	::RGBForeColor(&UGAColorRamp::GetBlackColor());
	::FrameRoundRect(&frame, 8, 8);
	
	// ¥ LIGHT EDGES
	// ¥ Start by rendering the bevelled edges of the sides facing the light
	// source
	// Left
	UGraphicGizmos::BevelTintLine(frame.left + 1, frame.top + 2,
								  frame.left + 1, frame.bottom - 2,
								  sGAOneGrayLevel, false);
	// Top
	UGraphicGizmos::BevelTintLine(frame.left + 2, frame.top + 1,
								  frame.right - gsPopup_ArrowButtonWidth, frame.top + 1,
								  sGAOneGrayLevel, false);

	// ¥ SHADOW EDGES
	// ¥ Render the shadow bevels
	// Bottom
	UGraphicGizmos::BevelTintLine(frame.left + 2, frame.bottom - 2,
								  frame.right - gsPopup_ArrowButtonWidth, frame.bottom - 2,
								  sGAOneGrayLevel, true);
	// Right
	UGraphicGizmos::BevelTintLine(frame.right - gsPopup_ArrowButtonWidth, frame.top + 2,
								  frame.right - gsPopup_ArrowButtonWidth, frame.bottom - 3,
								  sGAOneGrayLevel, true);

	// ¥¥ EDGES ON ARROW BUTTON
	// ¥ LIGHT EDGES
	// ¥ Start by rendering the bevelled edges of the sides facing the light
	// source
	// Draw first dark bevel
	// Left
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 1), frame.top + 1,
								  frame.right - (gsPopup_ArrowButtonWidth - 1), frame.bottom - 2,
								  sGATwoGrayLevels, false);
	// Top
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 2), frame.top + 1,
								  frame.right - 3, frame.top + 1,
								  sGATwoGrayLevels, false);
	// Draw second dark bevel
	// Left
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 2), frame.top + 2,
								  frame.right - (gsPopup_ArrowButtonWidth - 2), frame.bottom - 4,
								  sGAOneGrayLevel, false);
	// Top
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 3), frame.top + 2,
								  frame.right - 4, frame.top + 2,
								  sGAOneGrayLevel, false);

	// ¥ SHADOW EDGES
	// ¥ Render the shadow bevels
	// First light bevel
	// Right
	UGraphicGizmos::BevelTintLine(frame.right - 3, frame.top + 3,
								  frame.right - 3, frame.bottom - 4,
								  sGAOneGrayLevel, true);
	// Bottom
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 3), frame.bottom - 3,
								  frame.right - 4, frame.bottom - 3,
								  sGAOneGrayLevel, true);
	// Second light bevel
	// Right
	UGraphicGizmos::BevelTintLine(frame.right - 2, frame.top + 3,
								  frame.right - 2, frame.bottom - 3,
								  sGATwoGrayLevels, true);
	// Bottom
	UGraphicGizmos::BevelTintLine(frame.right - (gsPopup_ArrowButtonWidth - 2), frame.bottom - 2,
								  frame.right - 3, frame.bottom - 2,
								  sGATwoGrayLevels, true);
	// corner hilite
	UGraphicGizmos::BevelTintPixel(frame.right - 3, frame.bottom - 3,
								   sGATwoGrayLevels, true);

}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawGAPopupDimmedBevelTint
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This function is based in part on LGAPopup::DrawPopupDimmedColor().
//	Given a rect and assuming the content of the rect is filled with 87% gray
//	(0xDDDD, 0xDDDD, 0xDDDD), this function will apply tints such that it will
//	look like a dimmed GA popup menu

void UGAAppearance::DrawGAPopupDimmedBevelTint(
	const Rect	&inRect)
{
	Rect frame = inRect;

	// draw round rect frame
	UGraphicGizmos::BevelTintRoundRect(frame, 8, 8, sGAFiveGrayLevels, false);
	// draw seperator between text and arrow
	UGraphicGizmos::BevelTintLine(frame.right - gsPopup_ArrowButtonWidth,
								  frame.top + 1,
								  frame.right - gsPopup_ArrowButtonWidth,
								  frame.bottom - 2,
								  sGAFiveGrayLevels,
								  false);
}

