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

#include "CGAIconPopup.h"

#include <UGAColorRamp.h>
#include <UGraphicsUtilities.h>


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

static const Int16 gsPopup_RightInset 			= 	24;		//	Used to position the title rect
static const Int16 gsPopup_TitleInset			= 	8;		// 	Apple specification
static const Int16 gsPopup_LabelOffset 			= 	2;		//	Offset of label from popup


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
	Return the icon resource ID for the current title, 0 if none. Returns the actual
	resource ID that can be used to access the icon using ::GetCIcon().
======================================================================================*/

Int16 CGAIconPopup::GetTitleIconID(void) {
	
	Int16 iconID;
	::GetItemIcon(GetMacMenuH(), mValue, &iconID);
	
	return (iconID ? iconID + 256 : 0);
}


/*======================================================================================
	Refresh just the menu portion, not the title
======================================================================================*/

void CGAIconPopup::RefreshMenu(void) {

	if ( !IsVisible() || (GetSuperView() == nil) ) return;

	Rect refreshRect, superRevealed;
	GetSuperView()->GetRevealedRect(superRevealed);
	
	CalcLocalPopupFrameRect(refreshRect);
	::InsetRect(&refreshRect, 0, 2);	// Dont refresh shadows
	refreshRect.right -= gsPopup_RightInset;
	refreshRect.left += gsPopup_TitleInset;
	
	LocalToPortPoint(topLeft(refreshRect));
	LocalToPortPoint(botRight(refreshRect));
	
	if ( ::SectRect(&refreshRect, &superRevealed, &refreshRect) ) {
		InvalPortRect(&refreshRect);
	}
}



/*======================================================================================
	Get the title rect. Fix a bug in class that doesn't center text vertically.
======================================================================================*/

void CGAIconPopup::CalcTitleRect(Rect &outRect) {
	
	StTextState theTextState;
	const Int16 bevelWidth = 2;
	
	UTextTraits::SetPortTextTraits(GetTextTraitsID());
	
	FontInfo fInfo;
	GetFontInfo(&fInfo);
	Int16 textHeight = fInfo.ascent + fInfo.descent;
	
	CalcLocalPopupFrameRect(outRect);
	::InsetRect(&outRect, 0, bevelWidth);
	outRect.right -= gsPopup_RightInset;
	outRect.left += gsPopup_TitleInset;
	
	outRect.top += ((UGraphicsUtilities::RectHeight(outRect) - textHeight) / 2) - 1;
	outRect.bottom = outRect.top + textHeight;

	if ( GetTitleIconID() ) {
		outRect.left += cMenuIconWidth + cMenuIconTitleMargin;
	}
}



/*======================================================================================
	Get the label rect. Fix a bug in class that doesn't center text vertically.
======================================================================================*/

void CGAIconPopup::CalcLabelRect(Rect &outRect) {
	
	if ( HasLabel() ) {
		StTextState theTextState;
		const Int16 bevelWidth = 2;
		
		UTextTraits::SetPortTextTraits(GetTextTraitsID());
		
		FontInfo fInfo;
		GetFontInfo(&fInfo);
		Int16 textHeight = fInfo.ascent + fInfo.descent;
			
		CalcLocalFrameRect(outRect);
		outRect.right = outRect.left + (mLabelWidth - gsPopup_LabelOffset);
		::InsetRect(&outRect, 0, bevelWidth);
		
		outRect.top += ((UGraphicsUtilities::RectHeight(outRect) - textHeight) / 2) - 1;
		outRect.bottom = outRect.top + textHeight;
	} else {
		outRect = gEmptyRect;
	}
}



/*======================================================================================
	Draw the popup title
======================================================================================*/

void CGAIconPopup::DrawPopupTitle(void) {
	
	LGAPopup::DrawPopupTitle();
	
	Int16 iconID = GetTitleIconID();
	
	if ( iconID != 0 ) {
	
		CIconHandle theIconH = ::GetCIcon(iconID);
		
		if ( theIconH != nil ) {
			Rect iconRect;
			LGAPopup::CalcTitleRect(iconRect);
			
			iconRect.right = iconRect.left + cMenuIconWidth;
			iconRect.top = iconRect.top + ((iconRect.bottom - iconRect.top - cMenuIconHeight) / 2);
			iconRect.bottom = iconRect.top + cMenuIconHeight;
			
			Int16 transform = ttNone;
			
			if ( IsEnabled() ) {
				if ( IsHilited() ) {
					transform = ttSelected;
				}
			} else {
				transform = ttDisabled;
			}
		
			::PlotCIconHandle(&iconRect, ttNone, transform, theIconH);
			
			::DisposeCIcon(theIconH);
		}
	}
}


