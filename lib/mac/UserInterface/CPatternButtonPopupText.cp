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