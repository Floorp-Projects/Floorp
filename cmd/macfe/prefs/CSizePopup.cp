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

#include "CSizePopup.h"

#include "PascalString.h"
#include "uerrmgr.h"
#include "resgui.h"
#include "macutil.h"
#include "UModalDialogs.h"
#include "UGraphicGizmos.h"

#include <UNewTextDrawing.h>
#include <UGAColorRamp.h>
#include <UGraphicsUtilities.h>

static const Char16	gsPopup_SmallMark			=	'¥';	// Mark used for small font popups
static const Int16 gsPopup_ArrowButtonWidth 	= 	22;	//	Width used in drawing the arrow only
static const Int16 gsPopup_ArrowButtonHeight	= 	16;	//	Height used for drawing arrow only
static const Int16 gsPopup_ArrowHeight			= 	5;		//	Actual height of the arrow
static const Int16 gsPopup_ArrowWidth			= 	9;		//	Actual width of the arrow at widest

//-----------------------------------
CSizePopup::CSizePopup(LStream* inStream)
//-----------------------------------
:	mFontNumber(0)
,	LGAPopup(inStream)
{
}

//-----------------------------------
Int32 CSizePopup::GetMenuSize() const
//-----------------------------------
{
	Int32		size = 0;
	MenuHandle	menuH = const_cast<CSizePopup*>(this)->GetMacMenuH();
	
	if (menuH)
	{
		size = ::CountMItems(menuH);
	}
	
	return size;
}

//-----------------------------------
void CSizePopup::SetUpCurrentMenuItem(MenuHandle	inMenuH, Int16 inCurrentItem )
//-----------------------------------
{
	
	// ¥ If the current item has changed then make it so, this
	// also involves removing the mark from any old item
	if (inMenuH)
	{
		CStr255		sizeString;
		CStr255		itemString;
		Handle		ellipsisHandle = nil;
		char		ellipsisChar;	
		short		menuSize = ::CountMItems(inMenuH);
		
		ThrowIfNil_(ellipsisHandle = ::GetResource('elps', 1000));

		ellipsisChar = **ellipsisHandle;
		::ReleaseResource(ellipsisHandle);

		if (inCurrentItem == ::CountMItems(inMenuH))
		{
			itemString = ::GetCString(OTHER_FONT_SIZE);
			::StringParamText(itemString, (SInt32) GetFontSize(), 0, 0, 0);
		}
		else
		{
			itemString = ::GetCString(OTHER_RESID);
			itemString += ellipsisChar;
		}
		
		::SetMenuItemText(inMenuH, menuSize, itemString);

		// ¥ Get the current value
		Int16 oldItem = GetValue();
		
		// ¥ Remove the current mark
		::SetItemMark(inMenuH, oldItem, 0);
		
		// ¥ Always make sure item is marked
		Char16	mark = GetMenuFontSize() < 12 ? gsPopup_SmallMark : checkMark;
		::SetItemMark(inMenuH, inCurrentItem,  mark);
	}
	
}

//-----------------------------------
Int32 CSizePopup::GetFontSizeFromMenuItem(Int32 inMenuItem) const
//-----------------------------------
{
	Str255		sizeString;
	Int32		fontSize = 0;

	::GetMenuItemText(const_cast<CSizePopup*>(this)->GetMacMenuH(), inMenuItem, sizeString);
	
	myStringToNum(sizeString, &fontSize);
	return fontSize;
}

//-----------------------------------
void CSizePopup::SetFontSize(Int32 inFontSize)
//-----------------------------------
{
	mFontSize = inFontSize;
	if (inFontSize)
	{
		MenuHandle sizeMenu = GetMacMenuH();
		short menuSize = ::CountMItems(sizeMenu);
		Boolean isOther = true;
		for (int i = 1; i <= menuSize; ++i)
		{
			CStr255 sizeString;
			::GetMenuItemText(sizeMenu, i, sizeString);
			Int32 fontSize = 0;
			myStringToNum(sizeString, &fontSize);
			if (fontSize == inFontSize)
			{
				SetValue(i);
				if (i != menuSize)
					isOther = false;
				break;
			}
		}		
		if (isOther)
			SetValue(menuSize);
	}
}

//-----------------------------------
void CSizePopup::SetValue(Int32 inValue)
//-----------------------------------
{
	// ¥ We intentionally do not guard against setting the value to the 
	//		same value the popup current has. This is so that the other
	//		size stuff works correctly.

	// ¥ Get the current item setup in the menu
	MenuHandle menuH = GetMacMenuH();
	if ( menuH )
	{
		SetUpCurrentMenuItem( menuH, inValue );
	}

	if (inValue < mMinValue) {		// Enforce min/max range
		inValue = mMinValue;
	} else if (inValue > mMaxValue) {
		inValue = mMaxValue;
	}

	mValue = inValue;			//   Store new value
	BroadcastValueMessage();	//   Inform Listeners of value change

	// ¥ Now we need to get the popup redrawn so that the change
	// will be seen
	Draw( nil );
	
}	//	LGAPopup::SetValue

//-----------------------------------
Boolean CSizePopup::TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers)
//-----------------------------------
{

	// ¥ We only want the popup menu to appear if the mouse went down
	// in the our hot spot which is the popup portion of the control
	// not the label area
	if ( PointInHotSpot( inPoint, inHotSpot ))
	{
		// ¥ Get things started off on the right foot
		Boolean		currInside = true;
		Boolean		prevInside = false;
		HotSpotAction( inHotSpot, currInside, prevInside );

		// ¥ We skip the normal tracking that is done in the control as
		// the call to PopupMenuSelect will take control of the tracking
		// once the menu is up
		// ¥ Now we need to handle the display of the actual popup menu
		// we start by setting up some values that we will need
		Int16	menuID = 0;
		Int16 menuItem = GetValue();
		Int16	currItem = IsPulldownMenu() ? 1 : GetValue();
		Point popLoc;
		GetPopupMenuPosition( popLoc );
		
		// ¥ Call our utility function which handles the display of the menu
		// menu is disposed of inside this function
		HandlePopupMenuSelect( popLoc, currItem, menuID, menuItem );
		
		if ( menuItem > 0)
		{
			if ( menuItem == ::CountMItems( GetMacMenuH() ))
			{
				StDialogHandler handler(Wind_OtherSizeDialog, nil);
				LWindow* dialog = handler.GetDialog();
				LEditField	*sizeField = (LEditField *)dialog->FindPaneByID(1504);
				Assert_(sizeField);
				sizeField->SetValue(GetFontSize());
				sizeField->SelectAll();

				// Run the dialog
				MessageT message = msg_Nothing;
				do
				{
					message = handler.DoDialog();
				} while (message == msg_Nothing);

				if (msg_ChangeFontSize == message)
				{
					SetFontSize(sizeField->GetValue());
				}
			}
			else
			{
				SetFontSize(GetFontSizeFromMenuItem(menuItem));
			}
		}
			
		// ¥ Make sure that we get the HotSpotAction called one last time
		HotSpotAction( inHotSpot, false, true );
		
		return menuItem > 0;
	}
	else
		return false;
		
}

//-----------------------------------
void CSizePopup::MarkRealFontSizes(LGAPopup *fontPopup)
//-----------------------------------
{
	CStr255	fontName;
	::GetMenuItemText(	fontPopup->GetMacMenuH(),
						fontPopup->GetValue(),
						fontName);
	GetFNum(fontName, &mFontNumber);
	MarkRealFontSizes(mFontNumber);
}

//-----------------------------------
void CSizePopup::MarkRealFontSizes(short fontNum)
//-----------------------------------
{
	Str255			itemString;
	MenuHandle		sizeMenu;
	short			menuSize;
	
	sizeMenu = GetMacMenuH();
	menuSize = CountMItems(sizeMenu);
	
	for (short menuItem = 1; menuItem <= menuSize; ++menuItem)
	{
		Int32	fontSize;
		::GetMenuItemText(sizeMenu, menuItem, itemString);
		fontSize = 0;
		myStringToNum(itemString, &fontSize);

		Style	theSyle;

		if (fontSize && RealFont(fontNum, fontSize))
		{
			theSyle = outline;
		}
		else
		{
			theSyle = normal;
		}
		::SetItemStyle(	sizeMenu,
						menuItem,
						theSyle);
	}
}

//-----------------------------------
void CSizePopup::DrawPopupTitle()
// DrawPopupTitle is overridden to draw in the outline style
// as needed
//-----------------------------------
{
	StColorPenState	theColorPenState;
	StTextState 		theTextState;
	
	// ¥ Get some loal variables setup including the rect for the title
	ResIDT	textTID = GetTextTraitsID();
	Rect	titleRect;
	Str255 title;
	GetCurrentItemTitle( title );
	
	// ¥ Figure out what the justification is from the text trait and 
	// get the port setup with the text traits
	UTextTraits::SetPortTextTraits( textTID );
		
	// ¥ Set outline style if it's an outline size

	if (GetMacMenuH() && GetValue() != ::CountMItems(GetMacMenuH()))
	{
		Int32	fontSize;
		CStr255 itemString;

		::GetMenuItemText(GetMacMenuH(), GetValue(), itemString);
		fontSize = 0;
		myStringToNum(itemString, &fontSize);

		Style	theSyle;

		if (fontSize && ::RealFont(mFontNumber, fontSize))
		{
			theSyle = outline;
		}
		else
		{
			theSyle = normal;
		}
		
		::TextFace(theSyle);
	}
	
	// ¥ Set up the title justification which is always left justified
	Int16	titleJust = teFlushLeft;
	
	// ¥ Get the current item's title
	Str255 currentItemTitle;
	GetCurrentItemTitle( currentItemTitle );
	
	// ¥ Calculate the title rect
	CalcTitleRect( titleRect );
	
	// ¥ Kludge for drawing (correctly) in outline style left-justified
	Rect actualRect;
	UNewTextDrawing::MeasureWithJustification(
									(char*) &currentItemTitle[1],
									currentItemTitle[0],
									titleRect,
									titleJust,
									actualRect);
	actualRect.right += 2;	
	titleRect = actualRect;
	titleJust = teJustRight;								
	
	// ¥ Set up the text color which by default is black
	RGBColor	textColor;
	::GetForeColor( &textColor );

	// ¥ Loop over any devices we might be spanning and handle the drawing
	// appropriately for each devices screen depth
	StDeviceLoop	theLoop( titleRect );
	Int16				depth;
	while ( theLoop.NextDepth( depth )) 
	{
		if ( depth < 4 )		//	¥ BLACK & WHITE
		{
			// ¥ If the control is dimmed then we use the grayishTextOr 
			// transfer mode to draw the text
			if ( !IsEnabled())
			{
				::RGBForeColor( &UGAColorRamp::GetBlackColor() );
				::TextMode( grayishTextOr );
			}
			else if ( IsEnabled() && IsHilited() )
			{
				// ¥ When we are hilited we simply draw the title in white
				::RGBForeColor( &UGAColorRamp::GetWhiteColor() );
			}
				
			// ¥ Now get the actual title drawn with all the appropriate settings
			UTextDrawing::DrawWithJustification(
												(char*) &currentItemTitle[1],
												currentItemTitle[0],
												titleRect,
												titleJust);
		}
		else	// ¥ COLOR
		{
			// ¥ If control is selected we always draw the text in the title
			// hilite color, if requested
			if ( IsHilited())
				::RGBForeColor( &UGAColorRamp::GetWhiteColor() );
		
			// ¥ If the box is dimmed then we have to do our own version of the
			// grayishTextOr as it does not appear to work correctly across
			// multiple devices
			if ( !IsEnabled() || !IsActive())
			{
				textColor = UGraphicsUtilities::Lighten( &textColor );
				::TextMode( srcOr );
				::RGBForeColor( &textColor );
			}
				
			// ¥ Now get the actual title drawn with all the appropriate settings
			UTextDrawing::DrawWithJustification(
												(char*) &currentItemTitle[1],
												currentItemTitle[0],
												titleRect,
												titleJust);
		}	
	}
} // CSizePopup::DrawPopupTitle

//-----------------------------------
void CSizePopup::DrawPopupArrow()
//-----------------------------------
{
	StColorPenState	theColorPenState;
	
	// ¥ Get the local popup frame rect
	Rect	popupFrame;
	CalcLocalPopupFrameRect( popupFrame );
	
	// ¥ Set up some variables used in the drawing loop
	Int16		start = (( UGraphicsUtilities::RectHeight( popupFrame ) - gsPopup_ArrowHeight) / 2) + 1;

	// ¥ Figure out the left and right edges based on whether we are drawing
	// only the arrow portion or the entire popup
	Int16		leftEdge = gsPopup_ArrowButtonWidth - 6; 
	Int16		rightEdge = leftEdge - (gsPopup_ArrowWidth - 1);

	popupFrame.top		= popupFrame.top	+ start; 
	popupFrame.bottom	= popupFrame.top	+ gsPopup_ArrowHeight - 1; 
	popupFrame.left		= popupFrame.right	- leftEdge; 
	popupFrame.right	= popupFrame.right	- rightEdge; 
	
	UGraphicGizmos::DrawPopupArrow(
									popupFrame,
									IsEnabled(),
									IsActive(),
									IsHilited());
} // CSizePopup::DrawPopupArrow

