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

// ===========================================================================
//	CColorPopup.cp
// ===========================================================================

#include "CColorPopup.h"
#include "UGraphicGizmos.h"
#include "UGraphicsUtilities.h"
#include "CTargetedUpdateMenuRegistry.h"
#include "CEditView.h"			// constant for menu string resource
#include "edt.h"				// EDT calls
#include "prefapi.h"
#include "StSetBroadcasting.h"
#include "CBrowserContext.h"	// MWContext


// prototypes
pascal void
colorPopupMDEFProc( short message, MenuHandle theMenu, 
					Rect *menuRect, Point hitPt, short *whichItem );

// functionality

static
void
colorPopupGetBox( Rect *menuRect, Rect *itemBox, short numItems, short theItem )
{
	Rect	box;

	SetRect( itemBox, 0, 0, 0, 0 );
	if ( theItem <= numItems )
	{
		if ( theItem == CColorPopup::CURRENT_COLOR_ITEM )
		{
			box.bottom = menuRect->bottom - CColorPopup::COLOR_FRAME_BORDER;
			box.top = box.bottom - CColorPopup::COLOR_BOX_HEIGHT;
			box.left = menuRect->left + CColorPopup::COLOR_FRAME_BORDER;
			box.right = box.left + CColorPopup::COLOR_BOX_HEIGHT;
		}
		else if ( theItem == CColorPopup::DEFAULT_COLOR_ITEM )
		{
			box.bottom = menuRect->bottom - CColorPopup::COLOR_FRAME_BORDER 
										- CColorPopup::COLOR_BOX_HEIGHT;
			box.top = box.bottom - CColorPopup::COLOR_BOX_HEIGHT;
			box.left = menuRect->left + CColorPopup::COLOR_FRAME_BORDER;
			box.right = box.left + CColorPopup::COLOR_BOX_HEIGHT;
		}
		else if ( theItem == CColorPopup::LAST_COLOR_PICKED_ITEM )
		{
			box.top = menuRect->top + CColorPopup::COLOR_FRAME_BORDER;
			box.bottom = box.top + CColorPopup::COLOR_BOX_HEIGHT;
			box.left = menuRect->left + CColorPopup::COLOR_FRAME_BORDER;
			box.right = box.left + CColorPopup::COLOR_BOX_HEIGHT;
		}
		else
		{
			box.top = menuRect->top /* + CColorPopup::COLOR_FRAME_BORDER */ + CColorPopup::COLOR_HEX_DISPLAY_SIZE;
			box.left = menuRect->left + CColorPopup::COLOR_FRAME_BORDER;
			box.bottom = box.top + CColorPopup::COLOR_BOX_HEIGHT;
			box.right = box.left + CColorPopup::COLOR_BOX_WIDTH;

			while ( --theItem > 1 )	// 1 is # menuItems preceding color chip matrix
			{
				OffsetRect( &box, CColorPopup::COLOR_BOX_WIDTH, 0 );
				if ( box.left >= menuRect->right - CColorPopup::COLOR_FRAME_BORDER )
				{
					box.left = menuRect->left + CColorPopup::COLOR_FRAME_BORDER;
					box.right = box.left + CColorPopup::COLOR_BOX_WIDTH;
					OffsetRect( &box, 0, CColorPopup::COLOR_BOX_HEIGHT );
				}
			}
		}
		*itemBox = box;
	}
}


static
Boolean
colorPopupParseColor( Str255 menuStr, RGBColor *theColor )
{
	Boolean			foundColorFlag = false;
	unsigned short	part;
	short			loop;

	theColor->red = theColor->green = theColor->blue = 0;
	for ( loop = 1; loop <= (unsigned)menuStr[ 0 ] - 6; ++loop )
	{
		if ( menuStr[0] > 1 
		&& ( menuStr[ loop ] == CColorPopup::COLOR_CHIP_CHAR 
		|| menuStr[ loop ] == CColorPopup::LAST_COLOR_PICKED_CHAR 
		|| menuStr[ loop ] == CColorPopup::DEFAULT_COLOR_CHAR
		|| menuStr[ loop ] == CColorPopup::CURRENT_COLOR_CHAR ) )
		{
		// Converting from ASCII to Hex
		// This is BAD code...

			part = menuStr[ loop + 1 ] - '0';
			if ( part > 9 )
				part = menuStr[ loop + 1 ] - 'A' + 10;
			theColor->red = part << 4;
			part = menuStr[ loop + 2 ] - '0';
			if ( part > 9 )
				part = menuStr[ loop + 2 ] - 'A' + 10;
			theColor->red |= part;
			theColor->red = theColor->red << 8;

			part = menuStr[ loop + 3 ] - '0';
			if ( part > 9 )
				part = menuStr[ loop + 3 ] - 'A' + 10;
			theColor->green = part << 4;
			part = menuStr[ loop + 4 ] - '0';
			if ( part > 9 )
				part = menuStr[ loop + 4 ] - 'A' + 10;
			theColor->green |= part;
			theColor->green = theColor->green << 8;

			part = menuStr[ loop + 5 ] - '0';
			if ( part > 9 )
				part = menuStr[ loop + 5 ] - 'A' + 10;
			theColor->blue = part << 4;
			part = menuStr[ loop + 6 ] - '0';
			if ( part > 9 )
				part = menuStr[ loop + 6 ] - 'A' + 10;
			theColor->blue |= part;
			theColor->blue = theColor->blue << 8;

			foundColorFlag = true;
			break;
		}
	}
	
	return foundColorFlag;
}


static
void
colorPopupDrawBox( MenuHandle theMenu, Rect *menuRect, short numItems )
{
	Boolean			wildcardFlag;
	CIconHandle		cicnHandle;
	RGBColor		theColor;
	const RGBColor	black = {0,0,0}, white = {0xFFFF,0xFFFF,0xFFFF};
	Rect			box;
	Str255			menuStr;
	short			theItem;

	for ( theItem = 1; theItem <= numItems; ++theItem )
	{
		menuStr[ 0 ] = 0;
		GetMenuItemText( theMenu, theItem, menuStr );
		wildcardFlag = ((menuStr[0] == 0x01 && menuStr[1] == CColorPopup::COLOR_PICKER_CHAR)) ? true : false;
		
		colorPopupGetBox( menuRect, &box, numItems, theItem );

		// text (if any)
		int stringNum;
		switch ( menuStr[1] )
		{
			default:									stringNum = 0;	break;
			case CColorPopup::LAST_COLOR_PICKED_CHAR:	stringNum = 20;	break;
			case CColorPopup::DEFAULT_COLOR_CHAR:		stringNum = 21;	break;
			case CColorPopup::CURRENT_COLOR_CHAR:		stringNum = 22;	break;
		}

		if ( stringNum )
		{
			Point baseLinePoint;
			baseLinePoint.v = box.top + CColorPopup::COLOR_BOX_HEIGHT - 4;
			baseLinePoint.h = box.right + CColorPopup::COLOR_FRAME_BORDER;

			Str255 menuItemStr;
			menuItemStr[ 0 ] = 0;
			::GetIndString( menuItemStr, CEditView::STRPOUND_EDITOR_MENUS, stringNum );
			::RGBBackColor( &white );
			::RGBForeColor( &black );
			::MoveTo( baseLinePoint.h, baseLinePoint.v );
			::DrawString( menuItemStr );
		}

		// draw color chip
		if ( (true == wildcardFlag ) || (colorPopupParseColor( menuStr, &theColor ) == true))
		{
			::InsetRect( &box, 1, 1 );
			::RGBForeColor( &black );
			::MoveTo( box.left, box.bottom - 1 );
			::LineTo( box.left, box.top );
			::LineTo( box.right - 1, box.top );
#ifdef THREE_D_EFFECT_ON_COLOR_POPUP
			::RGBForeColor( &white );
#endif
			::LineTo( box.right - 1, box.bottom - 1 );
			::LineTo( box.left, box.bottom - 1 );

			if ( true == wildcardFlag )
			{
				RGBForeColor( &black );
				if ( (cicnHandle = GetCIcon(11685)) != NULL )
				{
					// don't scale, plot into same size rect
					Rect	r = (**cicnHandle).iconPMap.bounds;
					box.bottom = box.top + r.bottom - r.top;
					box.right = box.left + r.right - r.left;

					::PlotCIcon( &box, cicnHandle );
					::DisposeCIcon( cicnHandle );
				}
			}
			else
			{
				InsetRect( &box, 1, 1 );
				RGBForeColor( &theColor );
				PaintRect( &box );

				::RGBForeColor( &black );
				box.left += ( box.right-box.left ) / 2;
				box.right = box.left;
				box.top += ( box.bottom - box.top ) / 2;
				box.bottom = box.top;
				InsetRect( &box, -4, -4 );
				}
			}
		}
}


static
void
colorPopupChooseColor( Rect *menuRect, Point hitPt,
						short *whichItem, short numItems, RGBColor *backColor )
{
	RGBColor	aColor;
	Rect		box, oldBox;
	short		newItem = 0, loop;

	colorPopupGetBox( menuRect, &oldBox, numItems, *whichItem );

	if ( PtInRect( hitPt, menuRect ) )
	{
		for ( loop = 1; loop <= numItems; loop++ )
		{
			colorPopupGetBox( menuRect, &box, numItems, loop );
			if ( PtInRect( hitPt, &box ) )
			{
				newItem = loop;
				break;
			}
		}
	
		if ( *whichItem != newItem )
		{
			// deselect old menu item, select new menu item
			
			if ( *whichItem > 0 )
			{
				RGBForeColor( backColor );
				FrameRect( &oldBox );
			}
			if ( newItem > 0 )
			{
				aColor.red = aColor.blue = aColor.green = 0;
				RGBForeColor( &aColor );
				FrameRect( &box );
			}

#if COLOR_DISPLAY_TEXT
			box = *menuRect;
			box.top = box.bottom - COLOR_HEX_DISPLAY_SIZE;
			EraseRect( &box );
			if ( newItem > 0 )
			{
				menuStr[ 0 ] = 0;
				GetMenuItemText( theMenu, newItem, menuStr );
				if ( colorPopupParseColor( menuStr, &aColor ) == true )
				{
					if ( menuStr[ 0 ] )
					{
						// XXX should save/restore current font info, and set a particular (system?) font
					
						MoveTo( box.left + ((box.right-box.left) / 2) - ( ::StringWidth(menuStr) / 2), 
								box.bottom - 4);
						DrawString( menuStr );
					}
				}
			}
#endif
			*whichItem = newItem;
		}
	}
	else if ( *whichItem > 0 )
	{
		// deselect old menu item

		RGBForeColor( backColor );
		FrameRect( &oldBox );
#if COLOR_DISPLAY_TEXT
		box = *menuRect;
		box.top = box.bottom - COLOR_HEX_DISPLAY_SIZE;
		EraseRect( &box );
#endif
		*whichItem = 0;	// clear old item
	}
}


pascal void
colorPopupMDEFProc(short message, MenuHandle theMenu, 
		Rect *menuRect, Point hitPt, short *whichItem)
{
	if ( theMenu == NULL )
		return;
	
	PenState	pnState;
	RGBColor	foreColor, backColor;
#if 0
	Boolean		growRowsFlag = false;
#endif
	short		numItems, numCols, numRows;

	numItems = ::CountMItems( theMenu );
	::GetPenState( &pnState );
	::GetForeColor( &foreColor );
	::PenNormal();
	
	switch( message )
	{
		case mDrawMsg:
			colorPopupDrawBox( theMenu, menuRect, numItems );
			break;

		case mChooseMsg:
			::GetBackColor( &backColor );
			colorPopupChooseColor( menuRect, hitPt, whichItem, numItems, &backColor );
			::RGBBackColor( &backColor );
			break;

		case mSizeMsg:

		// determine # of rows/cols needed

#if 0
			if ( numItems > 0 )
			{
				numRows = numCols = 1;
				while( numItems > (numRows * numCols) )
				{
					if ( growRowsFlag )
						++numRows;
					else
						++numCols;
					
					growRowsFlag = (!growRowsFlag);
				}
			}
#endif			
#if 1
			// hard code these for now
			numRows = 10;
			numCols = 10;
#endif
			// why are the width and height backwards?  If they are switched, the menu is wrong shape
			(**theMenu).menuHeight = (numRows * CColorPopup::COLOR_BOX_HEIGHT) + (2 * CColorPopup::COLOR_FRAME_BORDER);
			(**theMenu).menuWidth = (numCols * CColorPopup::COLOR_BOX_WIDTH) + CColorPopup::COLOR_FRAME_BORDER;
	#if COLOR_DISPLAY_TEXT
			(**theMenu).menuWidth += CColorPopup::COLOR_HEX_DISPLAY_SIZE;
	#endif
			break;

		case mPopUpMsg:
			::SetRect( menuRect, hitPt.v, hitPt.h, 
						hitPt.v + (**theMenu).menuWidth, hitPt.h + (**theMenu).menuHeight );
			break;
/*
		case	mDrawItemMsg:
		break;

		case	mCalcItemMsg:
		break;
*/
		}

	::RGBForeColor( &foreColor );
	SetPenState( &pnState );
}


// This class overrides CPatternButtonPopup to provide a popup menu which
// changes the descriptor based on the menu selection
// assumes left-justified text in DrawButtonTitle()


// ---------------------------------------------------------------------------
//		¥ CColorPopup
// ---------------------------------------------------------------------------
// Stream-based ctor

CColorPopup::CColorPopup(LStream* inStream)
							:	CPatternButtonPopupText(inStream)
{
}


void CColorPopup::FinishCreateSelf()
{
	LView *superview = NULL, *view = GetSuperView();
	while ( view )
	{
		view = view->GetSuperView();
		if ( view )
			superview = view;
	}

	mEditView = (CEditView *)superview->FindPaneByID( CEditView::pane_ID );
	mDoSetLastPickedPreference = false;

	// need to Finish FinishCreateSelf to get menus set for retrieval
	super::FinishCreateSelf();
	
	// set the control by adjusting the menu and then getting the current color
	// unfortunately, when this is called, the editView doesn't have an mwcontext
	// so we can't query to find out what the current color is to set it properly
}


void
CColorPopup::InitializeCurrentColor()
{
	MenuHandle menuh;
	menuh = GetMenu()->GetMacMenuH();
	
	MWContext *mwcontext;
	if ( mEditView )
		mwcontext = mEditView->GetNSContext() 
						? mEditView->GetNSContext()->operator MWContext*()
						: NULL;
	else
		mwcontext = NULL;
	
	char colorstr[9];
	LO_Color color;
	Boolean isColorFound = false;
	
	// first try to get the color out of the character data
	EDT_CharacterData* chardata;
	chardata = EDT_GetCharacterData( mwcontext );
	if ( chardata && chardata->pColor )
	{
		isColorFound = true;
		color = *chardata->pColor;
		XP_SPRINTF( &colorstr[2], "%02X%02X%02X", color.red, color.green, color.blue);
	}
	if ( chardata )
		EDT_FreeCharacterData( chardata );
	
	// if we still haven't found it, let's try the page data
	if ( !isColorFound )
	{
		EDT_PageData *pagedata = EDT_GetPageData( mwcontext );
		if ( pagedata && pagedata->pColorText )
		{
			isColorFound = true;
			color = *pagedata->pColorText;
			XP_SPRINTF( &colorstr[2], "%02X%02X%02X", color.red, color.green, color.blue);
		}
		if ( pagedata )
			EDT_FreePageData( pagedata );
	}
	
	// if we still haven't found the color, get the browser preference
	if ( !isColorFound )
	{
		// editor.text_color
		int iSize = 9;
		int result = PREF_GetCharPref( "browser.foreground_color", &colorstr[1], &iSize );
		if ( result != PREF_NOERROR )
			colorstr[1] = 0;	// zero string if error is encountered
	}
	
	colorstr[1] = CURRENT_COLOR_CHAR;	// put in leading character
	colorstr[0] = strlen( &colorstr[1] );
	::SetMenuItemText( menuh, CURRENT_COLOR_ITEM, (unsigned char *)&colorstr );
	
	// set descriptor of control
	if ( GetValue() == CURRENT_COLOR_ITEM )
	{
		SetDescriptor( (const unsigned char *)colorstr );
		Refresh();	// inval the control's visible pane area
	}
}


// ---------------------------------------------------------------------------
//		¥ AdjustMenuContents
// ---------------------------------------------------------------------------
//	Set last color picked (first item).

void
CColorPopup::AdjustMenuContents()
{
	MenuHandle menuh;
	menuh = GetMenu()->GetMacMenuH();
	
	// initialize last color picked
	char colorstr[9];
	int iSize = 9;
	int result;
	
	// note hack to avoid converting c-string to p-string
	result = PREF_GetCharPref( "editor.last_color_used", &colorstr[1], &iSize );
	if ( result == PREF_NOERROR )
	{
		colorstr[1] = LAST_COLOR_PICKED_CHAR;	// replace '#' with '<'
		colorstr[0] = strlen( &colorstr[1] );
		::SetMenuItemText( menuh, LAST_COLOR_PICKED_ITEM, (unsigned char *)&colorstr );
	}
	
	// initialize the default color
	result = PREF_GetCharPref( "browser.foreground_color", &colorstr[1], &iSize );
	if ( result == PREF_NOERROR )
	{
		colorstr[1] = DEFAULT_COLOR_CHAR;	// replace '#' with '<'
		colorstr[0] = strlen( &colorstr[1] );
		::SetMenuItemText( menuh, DEFAULT_COLOR_ITEM, (unsigned char *)&colorstr );
	}
	
	// initialize the current color
	InitializeCurrentColor();
}


void CColorPopup::HandlePopupMenuSelect( Point inPopupLoc, Int16 inCurrentItem, 
										Int16& outMenuID, Int16& outMenuItem )
{
	super::HandlePopupMenuSelect( inPopupLoc, inCurrentItem, outMenuID, outMenuItem );
	
	// check if we need to set the preference here...
	mDoSetLastPickedPreference = ( outMenuID && outMenuItem != DEFAULT_COLOR_ITEM );
}


void CColorPopup::SetValue(Int32 inValue)
{
	if ( inValue == 0 )
		inValue = CURRENT_COLOR_ITEM;
	
	Boolean shouldBroadcast;
	// Handle color picker item special
	if ( inValue == COLOR_PICKER_ITEM )
	{
		mValue = inValue;			//  Store new value to broadcast it
		BroadcastValueMessage();	//  Inform Listeners of value change

		inValue = CURRENT_COLOR_ITEM;	//	Reset value to current color
		shouldBroadcast = false;	//	Already broadcast above; don't do again
	}
	else
		shouldBroadcast = IsBroadcasting();
	
	// broadcast only if it's not a color picker item (handled above)
	StSetBroadcasting setBroadcasting( this, shouldBroadcast );
	super::SetValue( inValue );
}


// ---------------------------------------------------------------------------
//		¥ HandleNewValue
// ---------------------------------------------------------------------------
//	Hook for handling value changes. Called by SetValue.
//	Note that the setting of the new value is done by CPatternButtonPopup::SetValue.
//  Therefore, GetValue() will still return the old value here, so the old value is
//	still available in this method.

Boolean
CColorPopup::HandleNewValue(Int32 inNewValue)
{
	Str255	str;
	MenuHandle menuh;
	menuh = GetMenu()->GetMacMenuH();
	::GetMenuItemText ( menuh, inNewValue, str );

	if ( str[1] != COLOR_PICKER_CHAR )
		SetDescriptor( str );

	if ( mDoSetLastPickedPreference 
	&& str[1] != LAST_COLOR_PICKED_CHAR && str[1] != COLOR_PICKER_CHAR )
	{
		mDoSetLastPickedPreference = false;

		// skip over any text which might preceed color
		int index;
		for ( index = 1; index <= str[0]
			&& str[index] != LAST_COLOR_PICKED_CHAR
			&& str[index] != COLOR_CHIP_CHAR
			&& str[index] != COLOR_PICKER_CHAR
			&& str[index] != DEFAULT_COLOR_CHAR
			&& str[index] != CURRENT_COLOR_CHAR; ++index )
			;
		
		if ( index + 7 < str[0] )
			str[index + 7] = 0;	// null terminate after symbol + 6chars of color
		
		str[ index ] = COLOR_CHIP_CHAR;	// prefs assume #-format color
		p2cstr( str );
		int result;
		result = PREF_SetCharPref( "editor.last_color_used", (char *)&str[index-1] );
														// skip past initial symbol; index-1 since now c-string
	}
	
	return false;
}


const Int16 cPopupArrowHeight	= 	5;		// height of the arrow
const Int16 cPopupArrowWidth	= 	9;		// widest width of the arrow


void
CColorPopup::DrawButtonContent(void)
{
	CPatternButtonPopupText::DrawButtonContent();
	DrawPopupArrow();
}


void
CColorPopup::DrawButtonTitle(void)
{
}


void
CColorPopup::DrawButtonGraphic(void)
{
	ResIDT		theSaveID = GetGraphicID();
	ResIDT		theNewID = theSaveID;
	RGBColor	foreColor, theColor;
	const RGBColor	black={0,0,0};

	Int32 theValue = GetValue();
	if (theValue > 0)
	{
		if ( theValue == COLOR_PICKER_ITEM )
			theValue = CURRENT_COLOR_ITEM;
		
		if (GetMenuItemRGBColor((short)theValue, &theColor) == true)
		{
			CButton::CalcGraphicFrame();
			InsetRect(&mCachedGraphicFrame, (mCachedGraphicFrame.right-mCachedGraphicFrame.left-12)/2,
				(mCachedGraphicFrame.bottom-mCachedGraphicFrame.top-12)/2);
			
			::GetForeColor(&foreColor);

			::RGBForeColor(&black);
			::FrameRect(&mCachedGraphicFrame);
			::RGBForeColor(&foreColor);

			::InsetRect(&mCachedGraphicFrame,1,1);
			::RGBForeColor(&theColor);
			::PaintRect(&mCachedGraphicFrame);
		}
		else
			theValue = CURRENT_COLOR_ITEM;	// not sure this is the right thing; 
											// it'd be nice if GetMenuItemRGBColor always returned true
	}
}


void
CColorPopup::DrawPopupArrow(void)
{
	Rect theFrame;
	
	CalcLocalFrameRect(theFrame);
	
	Int16 width = theFrame.right - theFrame.left;
	Int16 height = theFrame.bottom - theFrame.top;
	theFrame.top += ((height - cPopupArrowHeight) / 2);
	theFrame.left = theFrame.right - cPopupArrowWidth - 7;
	theFrame.right = theFrame.left + cPopupArrowWidth - 1;
	theFrame.bottom = theFrame.top + cPopupArrowHeight - 1;

	// check if we have moved past the right edge of the button
	// if so, adjust it back to the right edge of the button
	if ( theFrame.right > mCachedButtonFrame.right - 4 )
	{
		theFrame.right = mCachedButtonFrame.right - 4;
		theFrame.left = theFrame.right - cPopupArrowWidth - 1;
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
CColorPopup::HandleEnablingPolicy()
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
	CPatternButtonPopup::Enable();
}



Boolean
CColorPopup::GetMenuItemRGBColor(short menuItem, RGBColor *theColor)
{
	Str255		str;
	MenuHandle	menuh;
	Boolean		colorFoundFlag;

	menuh = GetMenu()->GetMacMenuH();
	::GetMenuItemText( menuh, menuItem, str );
	colorFoundFlag = colorPopupParseColor( str, theColor );
	return colorFoundFlag;
}



short
CColorPopup::GetMenuItemFromRGBColor(RGBColor *theColor)
{
	MenuHandle	menuh;
	RGBColor	tempColor;
	Str255		str;
	short		loop,numItems,retVal=CURRENT_COLOR_ITEM; // return current color if not found

	if ((menuh = GetMenu()->GetMacMenuH()) != NULL)
	{
		numItems = ::CountMItems(menuh);
		for (loop=1; loop<= numItems; loop++)
		{
			::GetMenuItemText( menuh, loop, str);
			if (colorPopupParseColor(str, &tempColor) == true)
			{
				if ((tempColor.red == theColor->red) && (tempColor.blue == theColor->blue) && (tempColor.green == theColor->green))
				{
					retVal = loop;
					break;
				}
			}
		}
	}
	return retVal;
}
