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
#include "CGWorld.h"

#include "mfinder.h" // needed for workaround function to bug in Appe's
					 // ::TruncText and ::TruncString - andrewb 6/20/97
#include "UPropFontSwitcher.h"
#include "CDefaultFontFontSwitcher.h"
#include "UUTF8TextHandler.h"


#ifndef __PALETTES__
#include <Palettes.h>
#endif

#include <string.h>

RGBColor UGraphicGizmos::sLighter = { 0xFFFF, 0xFFFF, 0xFFFF };
RGBColor UGraphicGizmos::sDarker = { 0x0000, 0x0000, 0x0000 };

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	LoadBevelTraits
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::LoadBevelTraits(
	ResIDT 				inBevelTraitsID,
	SBevelColorDesc& 	outBevelDesc)
{
	Assert_(inBevelTraitsID != resID_Undefined);
	
	StResource theBevelResource('BvTr', inBevelTraitsID);
	::BlockMoveData(*(theBevelResource.mResourceH), &outBevelDesc, sizeof(SBevelColorDesc));
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelRect(
	const Rect&		inRect,
	Int16 			inBevel,
	Int16 			inTopColor,
	Int16			inBottomColor)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect theBevelRect = inRect;
	theBevelRect.bottom--;
	theBevelRect.right--;

	StDeviceLoop theLoop(inRect);
	Int16 theDepth;
	while (theLoop.NextDepth(theDepth))
		{
		if (theDepth < 4)
			{
			// Draw a monochrome bevel
			QDGlobals* theQDPtr = UQDGlobals::GetQDGlobals();
			PatPtr theTopPat, theBottomPat;
	
			if (inBevel < 0)
				{
				inBevel = -inBevel;
				theTopPat = &theQDPtr->black;
				theBottomPat = &theQDPtr->white;
				}
			else
				{
				theTopPat = &theQDPtr->white;
				theBottomPat = &theQDPtr->black;
				}
	
			for (Int16 index = 1; index <= inBevel; index++)
				{
				::PenPat(theTopPat);
				::MoveTo(theBevelRect.right, theBevelRect.top);
				::LineTo(theBevelRect.left, theBevelRect.top);
				// 1997-03-24 pkc
				// Don't draw topleft corner twice
				::Move(0, 1);
				::LineTo(theBevelRect.left, theBevelRect.bottom);
				
				::PenPat(theBottomPat);
				::LineTo(theBevelRect.right, theBevelRect.bottom);
				// 1997-03-24 pkc
				// Don't draw bottomright corner twice
				::Move(0, -1);
				::LineTo(theBevelRect.right, theBevelRect.top);
		
				::InsetRect(&theBevelRect, 1, 1);
				}
			}
		else
			{
			// Draw a color bevel
			Int16 theTopColor, theBottomColor;
	
			if (inBevel < 0)
				{
				inBevel = -inBevel;
				theTopColor = inBottomColor;
				theBottomColor = inTopColor;
				}
			else
				{
				theTopColor = inTopColor;
				theBottomColor = inBottomColor;
				}

			for (Int16 index = 1; index <= inBevel; index++)
				{
				::PmForeColor(theTopColor);
				::MoveTo(theBevelRect.right, theBevelRect.top);
				::LineTo(theBevelRect.left, theBevelRect.top);
				// 1997-03-24 pkc
				// Don't draw topleft corner twice
				::Move(0, 1);
				::LineTo(theBevelRect.left, theBevelRect.bottom);
				
				::PmForeColor(theBottomColor);
				::LineTo(theBevelRect.right, theBevelRect.bottom);
				// 1997-03-24 pkc
				// Don't draw bottomright corner twice
				::Move(0, -1);
				::LineTo(theBevelRect.right, theBevelRect.top);
				
				::InsetRect(&theBevelRect, 1, 1);
				}
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelPartialRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelPartialRect(
	const Rect&			inRect,
	Int16 				inBevel,
	Int16 				inTopColor,
	Int16				inBottomColor,
	const SBooleanRect&	inSides)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect theBevelRect = inRect;
	theBevelRect.bottom--;
	theBevelRect.right--;

	for (Int16 index = 1; index <= inBevel; index++)
		{
		::PmForeColor(inTopColor);
		if (inSides.left)
			{
			// 1997-03-24 pkc
			// Don't draw topleft corner twice
			if (inSides.top)
				::MoveTo(theBevelRect.left, theBevelRect.top + 1);
			else
				::MoveTo(theBevelRect.left, theBevelRect.top);
			::LineTo(theBevelRect.left, theBevelRect.bottom);
			}
			
		if (inSides.top)
			{
			::MoveTo(theBevelRect.right, theBevelRect.top);
			::LineTo(theBevelRect.left, theBevelRect.top);
			}

		::PmForeColor(inBottomColor);
		if (inSides.right)
			{
			::MoveTo(theBevelRect.right, theBevelRect.top);
			// 1997-03-24 pkc
			// Don't draw bottomright corner twice
			if (inSides.bottom)
				::LineTo(theBevelRect.right, theBevelRect.bottom - 1);
			else
				::LineTo(theBevelRect.right, theBevelRect.bottom);
			}
			
		if (inSides.bottom)
			{
			::MoveTo(theBevelRect.left, theBevelRect.bottom);
			::LineTo(theBevelRect.right, theBevelRect.bottom);
			}
		
		::InsetRect(&theBevelRect, 1, 1);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelRect(
	const Rect&		inRect,
	Int16 			inBevel,
	const RGBColor&	inTopColor,
	const RGBColor&	inBottomColor)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect theBevelRect = inRect;
	theBevelRect.bottom--;
	theBevelRect.right--;

	StDeviceLoop theLoop(inRect);
	Int16 theDepth;
	while (theLoop.NextDepth(theDepth))
		{
		if (theDepth < 4)
			{
			// Draw a monochrome bevel
			QDGlobals* theQDPtr = UQDGlobals::GetQDGlobals();
			PatPtr theTopPat, theBottomPat;
	
			if (inBevel < 0)
				{
				inBevel = -inBevel;
				theTopPat = &theQDPtr->black;
				theBottomPat = &theQDPtr->white;
				}
			else
				{
				theTopPat = &theQDPtr->white;
				theBottomPat = &theQDPtr->black;
				}
	
			for (Int16 index = 1; index <= inBevel; index++)
				{
				::PenPat(theTopPat);
				::MoveTo(theBevelRect.right, theBevelRect.top);
				::LineTo(theBevelRect.left, theBevelRect.top);
				// 1997-03-24 pkc
				// Don't draw topleft corner twice
				::Move(0, 1);
				::LineTo(theBevelRect.left, theBevelRect.bottom);
				
				::PenPat(theBottomPat);
				::LineTo(theBevelRect.right, theBevelRect.bottom);
				// 1997-03-24 pkc
				// Don't draw bottomright corner twice
				::Move(0, -1);
				::LineTo(theBevelRect.right, theBevelRect.top);
		
				::InsetRect(&theBevelRect, 1, 1);
				}
			}
		else
			{
			// Draw a color bevel
			RGBColor theTopColor, theBottomColor;
	
			if (inBevel < 0)
				{
				inBevel = -inBevel;
				theTopColor = inBottomColor;
				theBottomColor = inTopColor;
				}
			else
				{
				theTopColor = inTopColor;
				theBottomColor = inBottomColor;
				}

			for (Int16 index = 1; index <= inBevel; index++)
				{
				::RGBForeColor(&theTopColor);
				::MoveTo(theBevelRect.right, theBevelRect.top);
				::LineTo(theBevelRect.left, theBevelRect.top);
				// 1997-03-24 pkc
				// Don't draw topleft corner twice
				::Move(0, 1);
				::LineTo(theBevelRect.left, theBevelRect.bottom);
				
				::RGBForeColor(&theBottomColor);
				::LineTo(theBevelRect.right, theBevelRect.bottom);
				// 1997-03-24 pkc
				// Don't draw bottomright corner twice
				::Move(0, -1);
				::LineTo(theBevelRect.right, theBevelRect.top);
				
				::InsetRect(&theBevelRect, 1, 1);
				}
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelRectAGA
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelRectAGA(
	const Rect&		inRect,
	Int16 			inBevel,
	const RGBColor&	inTopColor,
	const RGBColor&	inBottomColor)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect theBevelRect = inRect;
	theBevelRect.bottom--;
	theBevelRect.right--;

	StDeviceLoop theLoop(inRect);
	Int16 theDepth;
	while (theLoop.NextDepth(theDepth))
		{
		if (theDepth < 4)
			{
			// Draw a monochrome bevel
			QDGlobals* theQDPtr = UQDGlobals::GetQDGlobals();
			PatPtr theTopPat, theBottomPat;
	
			if (inBevel < 0)
				{
				inBevel = -inBevel;
				theTopPat = &theQDPtr->black;
				theBottomPat = &theQDPtr->white;
				}
			else
				{
				theTopPat = &theQDPtr->white;
				theBottomPat = &theQDPtr->black;
				}
	
			for (Int16 index = 1; index <= inBevel; index++)
				{
				::PenPat(theTopPat);
				::MoveTo(theBevelRect.right - 1, theBevelRect.top);
				::LineTo(theBevelRect.left, theBevelRect.top);
				// 1997-03-24 pkc
				// Don't draw topleft corner twice
				::Move(0, 1);
				::LineTo(theBevelRect.left, theBevelRect.bottom - 1);
				
				::PenPat(theBottomPat);
				::MoveTo(theBevelRect.left + 1, theBevelRect.bottom);
				::LineTo(theBevelRect.right, theBevelRect.bottom);
				// 1997-03-24 pkc
				// Don't draw bottomright corner twice
				::Move(0, -1);
				::LineTo(theBevelRect.right, theBevelRect.top + 1);
		
				::InsetRect(&theBevelRect, 1, 1);
				}
			}
		else
			{
			// Draw a color bevel
			RGBColor theTopColor, theBottomColor;
	
			if (inBevel < 0)
				{
				inBevel = -inBevel;
				theTopColor = inBottomColor;
				theBottomColor = inTopColor;
				}
			else
				{
				theTopColor = inTopColor;
				theBottomColor = inBottomColor;
				}

			for (Int16 index = 1; index <= inBevel; index++)
				{
				::RGBForeColor(&theTopColor);
				::MoveTo(theBevelRect.right - 1, theBevelRect.top);
				::LineTo(theBevelRect.left, theBevelRect.top);
				// 1997-03-24 pkc
				// Don't draw topleft corner twice
				::Move(0, 1);
				::LineTo(theBevelRect.left, theBevelRect.bottom - 1);
				
				::RGBForeColor(&theBottomColor);
				::MoveTo(theBevelRect.left + 1, theBevelRect.bottom);
				::LineTo(theBevelRect.right, theBevelRect.bottom);
				// 1997-03-24 pkc
				// Don't draw bottomright corner twice
				::Move(0, -1);
				::LineTo(theBevelRect.right, theBevelRect.top + 1);
				
				::InsetRect(&theBevelRect, 1, 1);
				}
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelPartialRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelPartialRect(
	const Rect&			inRect,
	Int16 				inBevel,
	const RGBColor&		inTopColor,
	const RGBColor&		inBottomColor,
	const SBooleanRect&	inSides)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	Rect theBevelRect = inRect;
	theBevelRect.bottom--;
	theBevelRect.right--;

	for (Int16 index = 1; index <= inBevel; index++)
		{
		::RGBForeColor(&inTopColor);
		if (inSides.left)
			{
			// 1997-03-24 pkc
			// Don't draw topleft corner twice
			if (inSides.top)
				::MoveTo(theBevelRect.left, theBevelRect.top + 1);
			else
				::MoveTo(theBevelRect.left, theBevelRect.top);
			::LineTo(theBevelRect.left, theBevelRect.bottom);
			}
			
		if (inSides.top)
			{
			::MoveTo(theBevelRect.right, theBevelRect.top);
			::LineTo(theBevelRect.left, theBevelRect.top);
			}

		::RGBForeColor(&inBottomColor);
		if (inSides.right)
			{
			::MoveTo(theBevelRect.right, theBevelRect.top);
			// 1997-03-24 pkc
			// Don't draw bottomright corner twice
			if (inSides.bottom)
				::LineTo(theBevelRect.right, theBevelRect.bottom - 1);
			else
				::LineTo(theBevelRect.right, theBevelRect.bottom);
			}
			
		if (inSides.bottom)
			{
			::MoveTo(theBevelRect.left, theBevelRect.bottom);
			::LineTo(theBevelRect.right, theBevelRect.bottom);
			}
		
		::InsetRect(&theBevelRect, 1, 1);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelTintPartialRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelTintPartialRect(
	const Rect &	inRect,
	Int16 			inBevel,
	Uint16			inTopTint,
	Uint16			inBottomTint,
	const SBooleanRect& inSides)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	RGBColor theAddColor = { inTopTint, inTopTint, inTopTint };
	RGBColor theSubColor = { inBottomTint, inBottomTint, inBottomTint };

	Rect theBevelRect = inRect;
	theBevelRect.bottom--;
	theBevelRect.right--;

	if (inBevel >= 0)
		{
		::RGBForeColor(&theAddColor);
		::OpColor(&sLighter);
		::PenMode(addPin);
		}
	else
		{
		::RGBForeColor(&theSubColor);
		::OpColor(&sDarker);
		::PenMode(subPin);
		}

	Int16 theBevel = (inBevel >= 0) ? inBevel : -inBevel;
	for (Int16 index = 1; index <= theBevel; index++)
		{
		if (inSides.top)
			{
			::MoveTo(theBevelRect.right, theBevelRect.top);
			::LineTo(theBevelRect.left, theBevelRect.top);
			}
		if (inSides.left)
			{
			// 1997-03-24 pkc
			// Don't draw topleft corner twice
			if (inSides.top)
				::MoveTo(theBevelRect.left, theBevelRect.top + 1);
			else
				::MoveTo(theBevelRect.left, theBevelRect.top);
			::LineTo(theBevelRect.left, theBevelRect.bottom);
			}
		if (inSides.top)
			theBevelRect.top++;
		if (inSides.left)
			theBevelRect.left++;
		if (inSides.bottom)
			theBevelRect.bottom--;
		if (inSides.right)
			theBevelRect.right--;
		}
		
	theBevelRect = inRect;
	theBevelRect.bottom--;
	theBevelRect.right--;

	if (inBevel >= 0)
		{
		::RGBForeColor(&theSubColor);
		::OpColor(&sDarker);
		::PenMode(subPin);
		}
	else
		{
		::RGBForeColor(&theAddColor);
		::OpColor(&sLighter);
		::PenMode(addPin);
		}

	theBevel = (inBevel >= 0) ? inBevel : -inBevel;
	for (Int16 index = 1; index <= theBevel; index++)
		{
		if (inSides.bottom)
			{
			::MoveTo(theBevelRect.left, theBevelRect.bottom);
			::LineTo(theBevelRect.right, theBevelRect.bottom);
			}
		if (inSides.right)
			{
			// 1997-03-24 pkc
			// Don't draw bottomright corner twice
			if (inSides.bottom)
				::MoveTo(theBevelRect.right, theBevelRect.bottom - 1);
			else
				::MoveTo(theBevelRect.right, theBevelRect.bottom);
			::LineTo(theBevelRect.right, theBevelRect.top);
			}
		if (inSides.top)
			theBevelRect.top++;
		if (inSides.left)
			theBevelRect.left++;
		if (inSides.bottom)
			theBevelRect.bottom--;
		if (inSides.right)
			theBevelRect.right--;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelTintRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelTintRect(
	const Rect &	inRect,
	Int16 			inBevel,
	Uint16			inTopTint,
	Uint16			inBottomTint)
{
	SBooleanRect sides = { true, true, true, true };
	BevelTintPartialRect(inRect, inBevel, inTopTint, inBottomTint, sides);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelTintRoundRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelTintRoundRect(
	const Rect&		inRect,
	Int16			inOvalWidth,
	Int16			inOvalHeight,
	Uint16			inTint,
	Boolean			inLighten)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();
	
	Rect rect = inRect;
	
	RGBColor theTintColor = { inTint, inTint, inTint };

	::RGBForeColor(&theTintColor);

	if (inLighten)
		{
		::OpColor(&sLighter);
		::PenMode(addPin);
		}
	else
		{
		::OpColor(&sDarker);
		::PenMode(subPin);
		}
	
	::FrameRoundRect(&rect, inOvalWidth, inOvalHeight);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelTintLine
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::BevelTintLine(
	Int16			inStartX,
	Int16			inStartY,
	Int16			inEndX,
	Int16			inEndY,
	Uint16			inTint,
	Boolean			inLighten)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	RGBColor theTintColor = { inTint, inTint, inTint };
	
	::RGBForeColor(&theTintColor);

	if (inLighten)
		{
		::OpColor(&sLighter);
		::PenMode(addPin);
		}
	else
		{
		::OpColor(&sDarker);
		::PenMode(subPin);
		}
	
	::MoveTo(inStartX, inStartY);
	::LineTo(inEndX, inEndY);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	BevelTintPixel
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void UGraphicGizmos::BevelTintPixel(
	Int16			inX,
	Int16			inY,
	Uint16			inTint,
	Boolean			inLighten)
{
	BevelTintLine(inX, inY, inX, inY, inTint, inLighten);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	RaiseColorVolume
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::RaiseColorVolume(
	const Rect&		inRect,
	Uint16			inTint)
{
	StColorPenState		theSavedState;
	theSavedState.Normalize();

	RGBColor theAddColor = { inTint, inTint, inTint };
	::RGBForeColor(&theAddColor);
	::OpColor(&sLighter);
	::PenMode(addPin);
	::PaintRect(&inRect);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	LowerColorVolume
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::LowerColorVolume(
	const Rect&		inRect,
	Uint16			inTint)
{
	StColorPenState		theSavedState;
	theSavedState.Normalize();

	RGBColor theSubColor = { inTint, inTint, inTint };
	::RGBForeColor(&theSubColor);
	::OpColor(&sDarker);
	::PenMode(subPin);
	::PaintRect(&inRect);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	LowerRoundRectColorVolume
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::LowerRoundRectColorVolume(
	const Rect&		inRect,
	Int16			inOvalWidth,
	Int16			inOvalHeight,
	Uint16			inTint)
{
	StColorPenState		theSavedState;
	theSavedState.Normalize();

	RGBColor theSubColor = { inTint, inTint, inTint };
	::RGBForeColor(&theSubColor);
	::OpColor(&sDarker);
	::PenMode(subPin);
	::PaintRoundRect(&inRect, inOvalWidth, inOvalHeight);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	PaintDisabledRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::PaintDisabledRect(
	const Rect&		inRect,
	Uint16			inTint,
	Boolean			inMakeLighter)
{
	StColorPenState		theSavedState;
	theSavedState.Normalize();
	
	RGBColor theOpColor;
	theOpColor.red = theOpColor.blue = theOpColor.green = inTint;
	
	RGBColor theNewColor;
	if (inMakeLighter) 
		theNewColor.red = theNewColor.blue = theNewColor.green = 0xFFFF;
	else 
		theNewColor.red = theNewColor.blue = theNewColor.green = 0x0000;
	
	::OpColor(&theOpColor);
	::RGBForeColor(&theNewColor);
	::PenMode(blend);
	::PenPat(&qd.black);
	::PaintRect(&inRect);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CenterRectOnRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::CenterRectOnRect(
	Rect			&inRect,
	const Rect		&inOnRect)
{
	Int16 theRectHeight = RectHeight(inRect);
	Int16 theRectWidth = RectWidth(inRect);
				
	inRect.top = inOnRect.top + ((RectHeight(inOnRect) - theRectHeight) / 2);
	inRect.bottom = inRect.top + theRectHeight;
	inRect.left = inOnRect.left + ((RectWidth(inOnRect) - theRectWidth) / 2);
	inRect.right = inRect.left + theRectWidth;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	AlignRectOnRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::AlignRectOnRect(
	Rect&			inRect,
	const Rect&		inOnRect,
	Uint16			inAlignment)
{
	Int16 theRectHeight = RectHeight(inRect);
	Int16 theRectWidth = RectWidth(inRect);

	switch (inAlignment)
		{
		case kAlignVerticalCenter:
			inRect.top = inOnRect.top + ((RectHeight(inOnRect) - theRectHeight) / 2);
			inRect.bottom = inRect.top + theRectHeight;
			break;
			
		case kAlignTop:
			inRect.top = inOnRect.top;
			inRect.bottom = inRect.top + theRectHeight;
			break;
			
		case kAlignBottom:
			inRect.bottom = inOnRect.bottom;
			inRect.top = inRect.bottom - theRectHeight;
			break;
			
		case kAlignHorizontalCenter:
			inRect.left = inOnRect.left + ((RectWidth(inOnRect) - theRectWidth) / 2);
			inRect.right = inRect.left + theRectWidth;
			break;
			
		case kAlignAbsoluteCenter:
			UGraphicGizmos::CenterRectOnRect(inRect, inOnRect);
			break;
			
		case kAlignCenterTop:
			inRect.left = inOnRect.left + ((RectWidth(inOnRect) - theRectWidth) / 2);
			inRect.right = inRect.left + theRectWidth;
			inRect.top = inOnRect.top;
			inRect.bottom = inRect.top + theRectHeight;
			break;
			
		case kAlignCenterBottom:
			inRect.left = inOnRect.left + ((RectWidth(inOnRect) - theRectWidth) / 2);
			inRect.right = inRect.left + theRectWidth;
			inRect.bottom = inOnRect.bottom;
			inRect.top = inRect.bottom - theRectHeight;
			break;
			
		case kAlignLeft:
			inRect.left = inOnRect.left;
			inRect.right = inRect.left + theRectWidth;
			break;
			
		case kAlignCenterLeft:
			inRect.top = inOnRect.top + ((RectHeight(inOnRect) - theRectHeight) / 2);
			inRect.bottom = inRect.top + theRectHeight;
			inRect.left = inOnRect.left;
			inRect.right = inRect.left + theRectWidth;
			break;
			
		case kAlignTopLeft:
			inRect.top = inOnRect.top;
			inRect.bottom = inRect.top + theRectHeight;
			inRect.left = inOnRect.left;
			inRect.right = inRect.left + theRectWidth;
			break;
			
		case kAlignBottomLeft:
			inRect.bottom = inOnRect.bottom;
			inRect.top = inRect.bottom - theRectHeight;
			inRect.left = inOnRect.left;
			inRect.right = inRect.left + theRectWidth;
			break;
			
		case kAlignRight:
			inRect.right = inOnRect.right;
			inRect.left = inRect.right - theRectWidth;
			break;
			
		case kAlignCenterRight:
			inRect.top = inOnRect.top + ((RectHeight(inOnRect) - theRectHeight) / 2);
			inRect.bottom = inRect.top + theRectHeight;
			inRect.right = inOnRect.right;
			inRect.left = inRect.right - theRectWidth;
			break;
			
		case kAlignTopRight:
			inRect.top = inOnRect.top;
			inRect.bottom = inRect.top + theRectHeight;
			inRect.right = inOnRect.right;
			inRect.left = inRect.right - theRectWidth;
			break;
			
		case kAlignBottomRight:
			inRect.bottom = inOnRect.bottom;
			inRect.top = inRect.bottom - theRectHeight;
			inRect.right = inOnRect.right;
			inRect.left = inRect.right - theRectWidth;
			break;
			
		default:
			Assert_(false);		// invalid selector
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	PadAlignedRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::PadAlignedRect(
	Rect&			ioRect,
	Uint16			inPad,
	Uint16			inAlignment)
{
	Int16 theHOffset = 0, theVOffset = 0;

	switch (inAlignment)
		{
		case kAlignTop:
		case kAlignCenterTop:
			theVOffset = inPad;
			break;
			
		case kAlignBottom:
		case kAlignCenterBottom:
			theVOffset = -inPad;
			break;
						
		case kAlignLeft:
		case kAlignCenterLeft:
			theHOffset = inPad;
			break;
			
		case kAlignTopLeft:
			theVOffset = inPad;
			theHOffset = inPad;
			break;
						
		case kAlignBottomLeft:
			theVOffset = -inPad;
			theHOffset = inPad;
			break;
			
		case kAlignRight:
		case kAlignCenterRight:
			theHOffset = -inPad;
			break;
						
		case kAlignTopRight:
			theVOffset = inPad;
			theHOffset = -inPad;
			break;
			
		case kAlignBottomRight:
			theVOffset = -inPad;
			theHOffset = -inPad;
			break;
			
		
		case kAlignAbsoluteCenter:
			// this is to prevent the Assert_
			break;
		
		default:
			Assert_(false);		// invalid selector
			break;
		}
	
	if (theHOffset != 0)
		{
		Int16 theRectWidth = RectWidth(ioRect);
		ioRect.left += theHOffset;
		ioRect.right = ioRect.left + theRectWidth;
		}
		
	if (theVOffset != 0)
		{
		Int16 theRectHeight = RectHeight(ioRect);
		ioRect.top += theVOffset;
		ioRect.bottom = ioRect.top + theRectHeight;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CenterStringInRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::CenterStringInRect(
	const StringPtr inString,
	const Rect		&inRect)
{
	FontInfo theFontInfo;
	::GetFontInfo(&theFontInfo);
		
	Point thePoint;
	thePoint.h = inRect.left + ((RectWidth(inRect) - StringWidth(inString)) / 2);
		
	Int16 theFontHeight = theFontInfo.ascent + theFontInfo.descent + 1;
	thePoint.v = inRect.top + ((RectHeight(inRect) - theFontHeight) / 2) + theFontInfo.ascent;
	
	MoveTo(thePoint.h, thePoint.v);
	::DrawString(inString);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	PlaceStringInRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::PlaceStringInRect(
	const StringPtr inString,
	const Rect 		&inRect,
	Int16			inHorizJustType,
	Int16			inVertJustType,
	const FontInfo*	inFontInfo)
{
	Point  thePoint;

	thePoint = CalcStringPosition(inRect, ::StringWidth(inString), inHorizJustType, inVertJustType, inFontInfo);
	::MoveTo(thePoint.h, thePoint.v);
	::DrawString(inString);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	PlaceTextInRect
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::PlaceTextInRect(
	const char* 	inText,
	Uint32			inTextLength,
	const Rect 		&inRect,
	Int16			inHorizJustType,
	Int16			inVertJustType,
	const FontInfo*	inFontInfo,
	Boolean			inDoTruncate,
	TruncCode		inTruncWhere)
{
	if (inRect.right <= inRect.left)
		return;
	const char* text = inText;
	short length = inTextLength;
	char buf[256];
	if (inDoTruncate)
	{
		// TSM - Text is not necessarily null terminated!
		::strncpy(buf, inText, sizeof(buf) < inTextLength ? sizeof(buf) : inTextLength);
		
		// BUG IN APPLE's ::TruncText workaround is to call our own version
		// that has the added bonus of not crashing when you try and trunc
		// in the middle. Crash in Apple's code happens when you have a small space
		// and you degenerate into choosing between truncating in the beginning or end.
		
		short available = inRect.right - inRect.left;
		if (inTruncWhere == truncMiddle)
			MiddleTruncationThatWorks(buf, length, available);
		else
			::TruncText(available, buf, &length, inTruncWhere);
			
		text = buf;
	}
	Point thePoint = CalcStringPosition(
		 	inRect,
		 	::TextWidth(text, 0, length),
		 	inHorizJustType,
		 	inVertJustType,
		 	inFontInfo);
	::MoveTo(thePoint.h, thePoint.v);
	::DrawText(text, 0, length);
}	

//----------------------------------------------------------------------------------------
void UGraphicGizmos::DrawUTF8TextString(
	const char*		inText, 
	const FontInfo*	inFontInfo,
	SInt16			inMargin,
	const Rect&		inBounds,
	SInt16			inJustification,
	Boolean			inDoTruncate,
	TruncCode		inTruncWhere)
//----------------------------------------------------------------------------------------
{
	Rect r = inBounds;
	
	r.left  += inMargin;
	r.right -= inMargin;
	
	PlaceUTF8TextInRect(inText, 
									strlen(inText),
									r,
									inJustification,
									teCenter,
									inFontInfo,
									inDoTruncate,
									inTruncWhere );			
}

//----------------------------------------------------------------------------------------
void UGraphicGizmos::PlaceUTF8TextInRect(
	const char* 	inText,
	Uint32			inTextLength,
	const Rect 		&inRect,
	Int16			inHorizJustType,
	Int16			inVertJustType,
	const FontInfo*	/*inFontInfo*/,
	Boolean			inDoTruncate,
	TruncCode		inTruncWhere)
//----------------------------------------------------------------------------------------
{
	FontInfo 			utf8FontInfo;
	CDefaultFontFontSwitcher *fs;
	UMultiFontTextHandler *th;
	th = UUTF8TextHandler::Instance();
	fs = new CDefaultFontFontSwitcher(UPropFontSwitcher::Instance() );
	th->GetFontInfo(fs, &utf8FontInfo);
	
	const char* text = inText;
	char truncatedtext[512];
	char* cur;
	short length = inTextLength;
	if (inDoTruncate)
	{
		short available = inRect.right - inRect.left;
		short totalwidth = th->TextWidth( fs, (char*)inText, inTextLength);
		if( totalwidth > available)
		{
			static char utf8ellips[] = "\xe2\x80\xa6";
			short utf8ellipswidth= th->TextWidth( fs, utf8ellips,3);
			short needWidth = available - utf8ellipswidth;
			 // never truncMiddle if less than 20
			Boolean bMidTruncate = (inTruncWhere == truncMiddle) && (needWidth > 20) ;
			short beginWidth = bMidTruncate ?  needWidth / 2 : needWidth;
			short trunclength = 0;
			short nextChar = 0;
			for(trunclength = 0;trunclength < inTextLength; trunclength = nextChar)
			{
				nextChar = INTL_NextCharIdxInText(CS_UTF8, (unsigned char*)text, trunclength);
				if(th->TextWidth( fs,  (char*)inText,nextChar) > beginWidth)
					break;
			}
			text = cur = truncatedtext;
			memcpy(cur, inText, trunclength);
			cur += trunclength;
			memcpy(cur,  utf8ellips, 3);				
			cur += 3;
			length = trunclength + 3;
			if (bMidTruncate) 
			{
				short endWidth = available - th->TextWidth( fs,  (char*)text, length);
				short endTrunc = trunclength;
				while( endTrunc < inTextLength )
				{
					if(th->TextWidth( fs,  (char*)inText+endTrunc,inTextLength-endTrunc) <= endWidth)
					{
						break;
					}
					endTrunc = INTL_NextCharIdxInText(CS_UTF8, (unsigned char*)inText , endTrunc);
				}
				memcpy(cur,  inText+ endTrunc, (inTextLength-endTrunc));	
				cur += (inTextLength-endTrunc);
				length += (inTextLength-endTrunc);
			}

		}
	}
	Point thePoint = UGraphicGizmos::CalcStringPosition(
		 	inRect,
		 	th->TextWidth(fs, (char*)text, length),
		 	inHorizJustType,
		 	inVertJustType,
		 	&utf8FontInfo);
	::MoveTo(thePoint.h, thePoint.v);
	th->DrawText(fs, (char*)text ,length);
	fs->RestoreDefaultFont();
	delete fs;	
} // UGraphicGizmos::PlaceUTF8TextInRect

//----------------------------------------------------------------------------------------
int UGraphicGizmos::GetUTF8TextWidth(
		const char* 	inText,
		int			inTextLength)	
//----------------------------------------------------------------------------------------
{
	CDefaultFontFontSwitcher *fs;
	fs = new CDefaultFontFontSwitcher( UPropFontSwitcher::Instance() );
	int width= UUTF8TextHandler::Instance()->TextWidth( fs, (char*)inText,inTextLength);
	fs->RestoreDefaultFont();
	delete fs;
	return width;
} // UGraphicGizmos::GetUTF8TextWidth


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcStringPosition
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Point UGraphicGizmos::CalcStringPosition(
	const Rect		&inRect,
	Int16			inStringWidth,
	Int16			inHorizJustType,
	Int16			inVertJustType,
	const FontInfo	*inFontInfo)
{
	Point  thePoint;
	FontInfo 			theFontInfo;
	const FontInfo*	theFontInfoPtr;
	
	theFontInfoPtr = inFontInfo;
	if (inFontInfo == NULL)
		{
		::GetFontInfo(&theFontInfo);
		theFontInfoPtr = &theFontInfo;
		}

	Int16 theFontHeight = theFontInfoPtr->ascent + theFontInfoPtr->descent; // + theFontInfoPtr->leading;
	Int16 theRectHeight = RectHeight(inRect);
	if (inHorizJustType == teFlushDefault)
		{
		inHorizJustType = ::GetSysDirection();
		if (inHorizJustType == teFlushDefault) // that's the way it behaves:
			inHorizJustType = teFlushLeft;
		}
	switch (inHorizJustType)
		{
		case teFlushRight:
			thePoint.h = inRect.right - inStringWidth;
			break;
			
		case teFlushLeft:
			thePoint.h = inRect.left;
			break;
				
		case teCenter:
		default:
			thePoint.h = inRect.left + ((RectWidth(inRect) - inStringWidth) / 2);
			break;
		}
		
	switch (inVertJustType)
		{
		case teFlushTop:
			thePoint.v = inRect.top + theFontInfoPtr->ascent;
			break;
			
		case teFlushBottom:
			thePoint.v = inRect.bottom - theFontInfoPtr->descent;
			break;
			
		case teCenter:
		default:
			if (theFontHeight >= theRectHeight)
			{
				// This is a fudge to fix a special case which is, however, common.  The height
				// we calculated above allows extra space at the top for diacriticals.  So if there's
				// a danger that we'll overlap at the bottom, make it overlap at the top, instead.
				thePoint.v = inRect.bottom - theFontInfoPtr->descent;
			}
			else
				thePoint.v
					= inRect.top + ((theRectHeight - theFontHeight) / 2)
					+ theFontInfoPtr->ascent;
			break;
		}
		
	return thePoint;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcWindowTingeColors
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::CalcWindowTingeColors(WindowPtr inWindowPtr, RGBColor &outLightTinge, RGBColor &outDarkTinge)
{
	AuxWinHandle	theAuxHandle;						// the default AuxWindow data
	Boolean			gotColors;

	::GetAuxWin(inWindowPtr, &theAuxHandle);

	// look for tinge colors in the color table belonging to the given window
	if (!theAuxHandle || !(**theAuxHandle).awCTable ||
		!GetTingeColorsFromColorTable ((**theAuxHandle).awCTable, outLightTinge, outDarkTinge))
		{
		// didn't find them? try the system color table
		// (note: explicit documentation on the wisdom of passing a 0 to GetAuxWin is
		// hard to find; I finally found it in Toolbox Technote 33 (March 1997))
		// (also note: the asserts in here are just what they mean; none of this should
		// ever, ever fail under system 7.)
		::GetAuxWin (0, &theAuxHandle);
		Assert_ (theAuxHandle && (**theAuxHandle).awCTable);
		gotColors = GetTingeColorsFromColorTable ((**theAuxHandle).awCTable, outLightTinge, outDarkTinge);
		Assert_ (gotColors);
		}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	GetTingeColorsFromColorTable
//		returns true if it finds both tinges in the table
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean UGraphicGizmos::GetTingeColorsFromColorTable (
		CTabHandle		inColorTable,
		RGBColor		&outLightTinge,
		RGBColor		&outDarkTinge)
{
	Boolean	foundThem;

	foundThem = false;

	// do we have a system 7 sized window color table?
	if ((**inColorTable).ctSize > wTitleBarColor)
		{
		// yes, now search for the tinge color, start at the end
		// because our tinge colors are usually last in the list.
		Boolean bLightFound = false;
		Boolean	bDarkFound = false;
		for (Int16 theIndex = (**inColorTable).ctSize; theIndex > 0; theIndex--)
			{
			if ((**inColorTable).ctTable[theIndex].value == wTingeLight)
				{
				outLightTinge = (**inColorTable).ctTable[theIndex].rgb;
				bLightFound = true;
				}
			else if ((**inColorTable).ctTable[theIndex].value == wTingeDark)
				{
				outDarkTinge = (**inColorTable).ctTable[theIndex].rgb;
				bDarkFound = true;
				}

			foundThem = bLightFound && bDarkFound;
			if (foundThem)
				break;	
			}
		}

	return foundThem;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	FindInColorTable
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::FindInColorTable(CTabHandle inColorTable, Int16 inColorID, RGBColor &outColor)
{
	RGBColor theFoundColor = { 0, 0, 0 };
	
	for (Int16 theIndex = (**inColorTable).ctSize; theIndex > 0; theIndex--)
	{
		if ((**inColorTable).ctTable[theIndex].value == inColorID)
		{
			theFoundColor = (**inColorTable).ctTable[theIndex].rgb;	
			break;
		}
	}
	
	outColor = theFoundColor;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	MixColor
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::MixColor(
	const RGBColor& inLightColor,
	const RGBColor&	inDarkColor,
	Int16 			inShade,
	RGBColor&		outColor)
{
	RGBColor theMixedColor;
	
	inShade = 0x0F - inShade;
		// This is necessary because we give shades between light and
		// dark (0% is light), but for colors, $0000 is black and $FFFF 
		// is dark.

	theMixedColor.red	= (Int32) (inLightColor.red   - inDarkColor.red)   * inShade / 15 + inDarkColor.red;
	theMixedColor.green = (Int32) (inLightColor.green - inDarkColor.green) * inShade / 15 + inDarkColor.green;
	theMixedColor.blue  = (Int32) (inLightColor.blue  - inDarkColor.blue)  * inShade / 15 + inDarkColor.blue;

	outColor = theMixedColor;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawArithPattern
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UGraphicGizmos::DrawArithPattern(
	const Rect& 		inFrame,
	const Pattern&		inPattern,
	Uint16				inTint,
	Boolean				inLighten)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

	RGBColor thePinColor = { inTint, inTint, inTint };

	::RGBBackColor(&sDarker);
	::RGBForeColor(&thePinColor);

	if (inLighten)
		{
		::OpColor(&sLighter);
		::PenMode(addPin);
		}
	else
		{
		::OpColor(&sDarker);
		::PenMode(subPin);
		}

	::PenPat(&inPattern);
	::PaintRect(&inFrame);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawPopupArrow
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This function is based in part on LGAPopup::DrawPopupArrow().

void
UGraphicGizmos::DrawPopupArrow(
	const Rect&			inLocalFrame,
	Boolean				inIsEnabled,
	Boolean				inIsActive,
	Boolean				inIsHilited)
{
	Rect	theFrame = inLocalFrame;
	Int16	thePopupArrowHeight	= (theFrame.bottom - theFrame.top + 1);
	
	// ¥ Setup a device loop so that we can handle drawing at the correct bit depth
	
	// this is a hack to avoid a clipping problem--LineTo draws down and to the right of the coordinates
	// we'll add a pixel to the right and then decrement it after StDeviceLoop call which clips
	++theFrame.right;
	++theFrame.bottom;
	
	StDeviceLoop	theLoop ( theFrame );

	--theFrame.right;
	--theFrame.bottom;

	Int16			depth;
	while ( theLoop.NextDepth ( depth )) 
	{
		if ( depth < 4 )
		{
			// ¥ If we are hilited and enabled and active we draw the arrow
			//   in white otherwise it is drawn in black
			::RGBForeColor ( &(inIsHilited && inIsEnabled && inIsActive ?
												UGAColorRamp::GetWhiteColor () :
												UGAColorRamp::GetBlackColor()));
			
			// ¥ If we are disabled we draw in gray
			if ( !inIsEnabled)
				::PenPat ( &UQDGlobals::GetQDGlobals()->gray );
		}
		else
		{
			StColorPenState::Normalize ();
			
			// ¥ Handle the actual drawing of the arrow,it is drawn in black in its normal
			// state, when it is hilited it is drawn in white, and it is drawn in gray when
			// dimmed
			if ( inIsHilited && inIsActive )
				::RGBForeColor ( &(inIsEnabled ? UGAColorRamp::GetWhiteColor() :
												UGAColorRamp::GetColor(7)) );
			else if ( !inIsEnabled || !inIsActive)
				::RGBForeColor ( &UGAColorRamp::GetColor(7));
			else
				::RGBForeColor ( &UGAColorRamp::GetBlackColor());
		}

		// ¥ Arrow drawing loop draws thePopupArrowHeight rows to make the arrow
		Int16  counter;
		for ( counter = 0; counter <= thePopupArrowHeight - 1; counter++ )
		{
			::MoveTo ( theFrame.left + counter, theFrame.top + counter );
			::LineTo ( theFrame.right - counter, theFrame.top + counter );
		}
	}
}	




// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Class CSicn
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CSicn::CSicn()
{
	::SetRect(&mSicnRect, 0, 0, 16, 16);
}

CSicn::CSicn(ResIDT inSicnID, Int16 inIndex)
{
	::SetRect(&mSicnRect, 0, 0, 16, 16);
	LoadSicnData(inSicnID, inIndex);
}

void CSicn::LoadSicnData(ResIDT inSicnID, Int16 inIndex)
{
	StResource theSicnList('SICN', inSicnID);
	ThrowIf_((::GetHandleSize(theSicnList.mResourceH) / sizeof(SicnT)) <= inIndex);

	StHandleLocker theLocker(theSicnList);
	::BlockMoveData((Ptr)(*((SicnT *)(*theSicnList.mResourceH) + inIndex)), mSicnData, sizeof(SicnT));
}

void CSicn::Plot(Int16 inMode)
{
	CopyImage(mSicnRect, inMode);
}

void CSicn::Plot(const Rect &inRect, Int16 inMode)
{
	CopyImage(inRect, inMode);
}

void CSicn::CopyImage(const Rect &inRect, Int16 inMode)
{
	// Set up a BitMap for the sicn
	BitMap theSourceBits;
	theSourceBits.baseAddr = (Ptr)(mSicnData);
	theSourceBits.rowBytes = 2;
	::SetRect(&theSourceBits.bounds, 0, 0, 16, 16);

	// draw the small icon in the current grafport
	::CopyBits(&theSourceBits, &(UQDGlobals::GetCurrentPort()->portBits),
				 &theSourceBits.bounds, &inRect, inMode, nil);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Class CIconFamily
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CIconFamily::CIconFamily(ResIDT inFamilyID)
{
	mFamilyID = inFamilyID;
	mIconRect.top = 0;
	mIconRect.left = 0;
	mIconRect.bottom = 0;
	mIconRect.right = 0;
}

CIconFamily::~CIconFamily()
{
}

void CIconFamily::Plot(
	IconAlignmentType inAlign,
	IconTransformType inTransform)
{
	::PlotIconID(&mIconRect, inAlign, inTransform, mFamilyID);
}

void CIconFamily::Plot(
	const Rect& 		inRect,
	IconAlignmentType 	inAlign,
	IconTransformType 	inTransform)
{
	::PlotIconID(&inRect, inAlign, inTransform, mFamilyID);
}

CIconFamily::CIconFamily()
{
	Assert_(false);
}

void CIconFamily::CalcBestSize(const Rect& inContainer)
{
	mIconRect.top = mIconRect.left = 0;

	if ((RectWidth(inContainer) >= 32) && (RectHeight(inContainer) >= 32))
		mIconRect.right = mIconRect.bottom = 32;
	else if ((RectWidth(inContainer) >= 16) && (RectHeight(inContainer) >= 16))
		mIconRect.right = mIconRect.bottom = 16;
	else if ((RectWidth(inContainer) >= 16) && (RectHeight(inContainer) >= 12))
		{
		mIconRect.right = 16;
		mIconRect.bottom = 12;
		}
	else
		Assert_(false);	// you really wont be able to draw in bounds
}
