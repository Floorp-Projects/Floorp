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

#include "CPatternButtonPopupText.h"
#include "UGraphicGizmos.h"
#include "UGraphicsUtilities.h"
#include "UGAAppearance.h"
#include "UGAColorRamp.h"
#include "CSharedPatternWorld.h"
#include "CTargetedUpdateMenuRegistry.h"

#include "PascalString.h"

// This class overrides CPatternButtonPopup to provide a popup menu which
// changes the descriptor based on the menu selection
// assumes left-justified text in DrawButtonTitle()

// ---------------------------------------------------------------------------
//		¥ CPatternButtonPopupText
// ---------------------------------------------------------------------------
// Stream-based ctor

CPatternButtonPopupText::CPatternButtonPopupText(LStream* inStream)
							:	CPatternButtonPopup(inStream)
{
}


// ---------------------------------------------------------------------------
//		¥ HandleNewValue
// ---------------------------------------------------------------------------
//	Hook for handling value changes. Called by SetValue.
//	Note that the setting of the new value is done by CPatternButtonPopup::SetValue.
//  Therefore, GetValue() will still return the old value here, so the old value is
//	still available in this method.

Boolean
CPatternButtonPopupText::HandleNewValue(Int32 inNewValue)
{
	Str255	str;
	MenuHandle menuh;
	menuh = GetMenu()->GetMacMenuH();
	::GetMenuItemText ( menuh, inNewValue, str );
	
	SetDescriptor( str );
	return false;
}


// taken from LGAPopup.cp and then modified
const Int16 gsPopup_ArrowHeight			= 	5;		//	Actual height of the arrow
const Int16 gsPopup_ArrowWidth			= 	9;		//	Actual width of the arrow at widest
const Int16 gsPopup_ArrowButtonWidth 	= 	22;		//	Width used in drawing the arrow only
const Int16 gsPopup_ArrowButtonHeight	= 	16;		//	Height used for drawing arrow only

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawButtonContent
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// copied from CPatternButton::DrawButtonContent()
// comment out check for "IsMouseInFrame()" and always draw the bevel rect
	
void
CPatternButtonPopupText::DrawButtonContent(void)
{
	CGrafPtr thePort;
	::GetPort(&(GrafPtr)thePort);
	
	Rect theFrame = mCachedButtonFrame;
	Point theAlignment;
	
	CalcOrientationPoint(theAlignment);
	mPatternWorld->Fill(thePort, theFrame, theAlignment);

	::InsetRect(&theFrame, 2, 2);

	if (IsActive() && IsEnabled())
		{
		// ¥ Setup a device loop so that we can handle drawing at the correct bit depth
		StDeviceLoop	theLoop ( mCachedButtonFrame );
		Int16			depth;
			
		if (IsTrackInside() || (!IsBehaviourButton() && (mValue == Button_On)))
			{
			while ( theLoop.NextDepth ( depth )) 
				if ( depth >= 4 )		// don't do anything for black and white
					{
					Rect frame = mCachedButtonFrame;
					::InsetRect(&frame, 1, 1);
					UGraphicGizmos::LowerRoundRectColorVolume(frame, 4, 4, UGAAppearance::sGASevenGrayLevels);
					}

			UGAAppearance::DrawGAPopupPressedBevelTint(mCachedButtonFrame);
			}
		else if (IsMouseInFrame())
			{
			UGAAppearance::DrawGAPopupBevelTint(mCachedButtonFrame);
			
			while ( theLoop.NextDepth ( depth )) 
				if ( depth >= 4 )		// don't do anything for black and white
					UGraphicGizmos::LowerColorVolume(theFrame, UGAAppearance::sGAHiliteContentTint);
			}
		else // ! (IsMouseInFrame())
			{
			// draw round rect frame
			UGraphicGizmos::BevelTintRoundRect(mCachedButtonFrame, 8, 8, UGAAppearance::sGASevenGrayLevels, false);
			// draw seperator between text and arrow
			UGraphicGizmos::BevelTintLine(mCachedButtonFrame.right - gsPopup_ArrowButtonWidth,
										  mCachedButtonFrame.top + 1,
										  mCachedButtonFrame.right - gsPopup_ArrowButtonWidth,
										  mCachedButtonFrame.bottom - 2,
										  UGAAppearance::sGAFiveGrayLevels,
										  false);
						
			while ( theLoop.NextDepth ( depth )) 
				if ( depth >= 4 )		// don't do anything for black and white
					UGraphicGizmos::LowerColorVolume(theFrame, UGAAppearance::sGAHiliteContentTint);
			}
		}
}

void
CPatternButtonPopupText::DrawButtonTitle(void)
{
	StColorPenState thePenSaver;
	thePenSaver.Normalize();

// only if we're tracking; checking for "Button_On" is bad as mValue is current menuSelection
	if (IsTrackInside())
		::RGBForeColor(&UGAColorRamp::GetWhiteColor());

	CStr255			truncTitle(mTitle);
	
	::TruncString( mFrameSize.width - (gsPopup_ArrowButtonWidth + 5), truncTitle, smTruncEnd);
	
	UGraphicGizmos::PlaceStringInRect(truncTitle, mCachedTitleFrame, teFlushLeft, teCenter);
	
	DrawPopupArrow();
}

void
CPatternButtonPopupText::DrawPopupArrow()
{
	Rect theFrame;
	
	CalcLocalFrameRect(theFrame);
	
	Int16 width = theFrame.right - theFrame.left;
	Int16 height = theFrame.bottom - theFrame.top;
	theFrame.top += ((height - gsPopup_ArrowHeight) / 2);
	theFrame.left = theFrame.right - gsPopup_ArrowWidth - 7;
	theFrame.right = theFrame.left + gsPopup_ArrowWidth - 1;
	theFrame.bottom = theFrame.top + gsPopup_ArrowHeight - 1;

	// check if we have moved past the right edge of the button
	// if so, adjust it back to the right edge of the button
	if ( theFrame.right > mCachedButtonFrame.right - 4 )
	{
		theFrame.right = mCachedButtonFrame.right - 4;
		theFrame.left = theFrame.right - gsPopup_ArrowWidth - 1;
	}

	UGraphicGizmos::DrawPopupArrow(
									theFrame,
									IsEnabled(),
									IsActive(),
									IsTrackInside());
}


#pragma mark -

// always call ProcessCommandStatus for popup menus which can change values
void
CPatternButtonPopupText::HandleEnablingPolicy()
{
	LCommander* theTarget		= LCommander::GetTarget();
	MessageT	buttonCommand	= GetValueMessage();
	Boolean 	enabled			= false;
	Boolean		usesMark		= false;
	Str255		outName;
	Char16		outMark;
	
	if (!CTargetedUpdateMenuRegistry::UseRegistryToUpdateMenus() ||
			CTargetedUpdateMenuRegistry::CommandInRegistry(buttonCommand))
	{
		if (!IsActive() || !IsVisible())
			return;
			
		if (!theTarget)
			return;
		
		CPatternButtonPopup::HandleEnablingPolicy();
		
		if (buttonCommand)
			theTarget->ProcessCommandStatus(buttonCommand, enabled, usesMark, outMark, outName);
	}
}