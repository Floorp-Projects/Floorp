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

// StPrepareForDialog

	// macfe
#include "macgui.h"
#include "uprefd.h"
#include "resgui.h"
#include "mimages.h"
#include "uerrmgr.h"
#include "macutil.h"
	// Netscape
#include "client.h"
#include "ntypes.h"
#include "shist.h"
#include "glhist.h"
#include "lo_ele.h"
#include "mkutils.h"
#include "prefapi.h"

	// PowerPlant
#include <UEnvironment.h>
	// system

#include <LowMem.h>

#ifdef PROFILE
#pragma profile on
#endif

void DrawArc( const RGBColor& initColor, const Rect& box, Boolean inset );

// ¥¥ StPrepareForDialog

StPrepareForDialog::StPrepareForDialog()
{
	ErrorManager::PrepareToInteract();
	UDesktop::Deactivate();
}

StPrepareForDialog::~StPrepareForDialog()
{
	UDesktop::Activate();
}


// class StTempClipRgn
RgnHandle StTempClipRgn::sOldClip = NULL;	
RgnHandle StTempClipRgn::sMergedNewClip = NULL;

StTempClipRgn::StTempClipRgn (RgnHandle newClip)
{
	if (sOldClip == NULL) sOldClip = ::NewRgn();
	if (sMergedNewClip == NULL) sMergedNewClip = ::NewRgn();
	
	::GetClip(sOldClip);
	::SectRgn(sOldClip, newClip, sMergedNewClip);
	::SetClip(sMergedNewClip);
}

StTempClipRgn::~StTempClipRgn()
{
	::SetClip(sOldClip);
}


// class HyperStyle
extern Boolean gIsPrinting;

/*
HyperStyle::HyperStyle( HyperStyle* style )
{
	memcpy( this, style, sizeof( HyperStyle ) );	// Just copy the fields
}
*/

HyperStyle::HyperStyle( MWContext *context, const CCharSet* charSet, LO_TextAttr* attr, Boolean onlyMeasuring,
	LO_TextStruct* element )
{
	fElement = element;
	
	fFontReference = CFontReference::GetFontReference(charSet, attr, context, sUnderlineLinks);
	
	strike = ( attr->attrmask & LO_ATTR_STRIKEOUT );
	fInlineInput = ( attr->attrmask & ( LO_ATTR_INLINEINPUT | LO_ATTR_INLINEINPUTTHICK | LO_ATTR_INLINEINPUTDOTTED ) );
	fOnlyMeasuring = onlyMeasuring;

	if ( !fOnlyMeasuring )
	{
		rgbFgColor = UGraphics::MakeRGBColor( attr->fg.red, attr->fg.green, attr->fg.blue );
		rgbBkColor = UGraphics::MakeRGBColor( attr->bg.red, attr->bg.green, attr->bg.blue );
	}
	
	memcpy( &fAttr, attr, sizeof( LO_TextAttr ) );	// Just copy the fields

}

// ¥ calculate a rectangular area of the page based on "where" and the
//	length of "text", using this style's font information; note that if
//	we have a reference element, we use the line height given by layout,
//	otherwise we figure out a line height using the text metrics
Rect HyperStyle::InvalBackground( Point where, char* text, int start, int end, Boolean blinks )
{
	int					usedText;
	int					unusedText;
	Rect				hiliteArea;

	int					length;
	
	Apply();
	GetFontInfo();
	
	length = end - start + 1;
	
	usedText = fFontReference->TextWidth(text, start, length);
	unusedText = start ? fFontReference->TextWidth(text, 0, start) : 0;
	if ( !fElement || !fElement->line_height )
	{
		hiliteArea.top = where.v;
		hiliteArea.left = where.h + unusedText;
		hiliteArea.bottom = hiliteArea.top + fFontInfo.ascent + fFontInfo.descent;
		hiliteArea.right = hiliteArea.left + usedText;
	}
	else
	{
		hiliteArea.top = where.v - fElement->y_offset;
		hiliteArea.left = where.h + unusedText;
		hiliteArea.bottom = hiliteArea.top + fElement->line_height;
		hiliteArea.right = hiliteArea.left + usedText;

		if ( blinks ||
			( fAttr.fontmask & LO_FONT_ITALIC ) )
		{
			hiliteArea.right += ( fFontInfo.widMax / 2 );
			hiliteArea.left -= MIN( 2, ( fFontInfo.widMax / 8 ) );
		}
	}
	
	return hiliteArea;
}


void HyperStyle::DrawText(	Point where,
							char* text,
							int start,
							int end )
{
	Boolean		selected;
	Boolean		swapped = FALSE;
	int			length = end - start + 1;
	
	Assert_( fElement );
	
	selected = ( ( fElement->ele_attrmask & LO_ELE_SELECTED ) != 0 &&
					fElement->sel_start <= start &&
					end <= fElement->sel_end );

	Boolean		paint_background = selected || !fAttr.no_background;

	fFontReference->SetMode( paint_background ? srcCopy : srcOr );
	
	// ¥ stupid layout reverses bk / fg colors when it tries to draw selected
	// 		put the actual fg color back in fg, and put the user's hilite color
	//		into bk
	// ¥ selection takes precedence over blink
	if ( selected )
	{
		// ¥ put the colors back and check to make sure that highlighting
		//		will actually show up
		rgbFgColor = rgbBkColor;
		LMGetHiliteRGB( &rgbBkColor );
		if ( UGraphics::EqualColor( rgbFgColor, rgbBkColor ) )
		{
			swapped = TRUE;
			rgbFgColor.red = ~rgbBkColor.red;
			rgbFgColor.green = ~rgbBkColor.green;
			rgbFgColor.blue = ~rgbBkColor.blue;
		}
	}

	Apply();
	GetFontInfo();

	long unusedText = start ? fFontReference->TextWidth(text, 0, start) : 0;
	int x = where.h + unusedText;
	int y = where.v + fFontInfo.ascent;

	fFontReference->DrawText(x, y, text, start, end );
	
	if ( paint_background )
	{			
		short	fontHeight = fFontInfo.ascent + fFontInfo.descent + fFontInfo.leading;
		
		if ( !fElement )
			return;
		
		// ¥Êif the font height is less than the element's line height,
		//		there will be rectangular areas above and below the text
		//		that are not drawn with the selection highlighting
		//		we draw them here
		if ( fontHeight < fElement->line_height )
		{
			long	usedText = fFontReference->TextWidth(text, start, length);
			Rect	hiliteArea;
		
			hiliteArea.top = where.v - fElement->y_offset;
			hiliteArea.left = where.h + unusedText;
			hiliteArea.right = hiliteArea.left + usedText;
			hiliteArea.bottom = where.v;
			
			::EraseRect( &hiliteArea );

			hiliteArea.bottom = hiliteArea.top + fElement->line_height;
			hiliteArea.top += ( fFontInfo.ascent + fFontInfo.descent + fElement->y_offset );
			::EraseRect( &hiliteArea );
		}
	}		

/*
	// ¥Êspecial case for selected italics
// FIX ME! Don't forget about this when we redo printing
//	if ( !gIsPrinting && ( (fStyle & italic) != 0 ) )
	if ( (fAttr.fontmask & LO_FONT_ITALIC) != 0 )
	{
		if ( swapped )
			::RGBForeColor( &rgbBkColor );
		
		// **** should this use fFontReference->SetMode?	
		::TextMode( srcOr );
		fFontReference->DrawText(where.h, where.v + fFontInfo.ascent, text, 0, fElement->text_len );
	}
*/

	Boolean isMisspelled = (fAttr.attrmask & LO_ATTR_SPELL) != 0;
	if ( strike || isMisspelled )
	{
		// I have consciously decided that if a word is misspelled and
		// striked that it is preferable to not strike and just mark
		// for misspelling; this may not be the correct decision. [brade]
		long	usedText = fFontReference->TextWidth(text, start, length);
		short	vstrike;
		
		if ( isMisspelled )
		{
			// for misspelled words we want a dashed underline
			PenPat( &qd.gray );
			vstrike = -( fFontInfo.descent - 1 );
		}
		else
			vstrike = ( fFontInfo.ascent / 2 ) - 1;

		::Move( -usedText, -vstrike );
		::Line( usedText, 0 );

		// restore things...
		::Move( 0, vstrike );
		
		if ( isMisspelled )
			PenPat( &qd.black );
	}
	
	if ( fInlineInput )
	{
		long	usedText = fFontReference->TextWidth( text, start, length );
		short	vInline = fFontInfo.descent;

		if ( fInlineInput & LO_ATTR_INLINEINPUTTHICK ) {
		
			PenSize( 1, 2);
			vInline -= 2;
			
		} else {
		
			PenSize( 1, 1);
			vInline -= 1;
			
		}
		
		if ( fInlineInput & LO_ATTR_INLINEINPUTDOTTED )
			PenPat( &UQDGlobals::GetQDGlobals()->gray);
		else
			PenPat( &UQDGlobals::GetQDGlobals()->black);

		
		::Move( -usedText, vInline );
		::Line( usedText, 0 );
		
		// restore things...
		::Move( 0, -vInline );
		PenSize( 1, 1);
		PenPat( &UQDGlobals::GetQDGlobals()->black);
		
	}
}

const char* sUnderlineLinksPref = "browser.underline_anchors";
Boolean HyperStyle::sUnderlineLinks = true;
XP_HashTable HyperStyle::sFontHash = NULL;

void HyperStyle::InitHyperStyle()
{
#if 0
	if ( !sFontHash )
	{	
		// the table size be: 2 fonts * 7 styles (bold, italic underline combos) * 
		// 10 for safety (multiple languages)
		sFontHash = XP_HashTableNew( 140, StyleHash, StyleCompFunction );
	}
#endif
	
	PREF_RegisterCallback(sUnderlineLinksPref, SetUnderlineLinks, nil);
	SetUnderlineLinks(sUnderlineLinksPref, nil);
	
	CWebFontReference::Init();
}

void HyperStyle::FinishHyperStyle()
{
	CWebFontReference::Finish();
}

int HyperStyle::SetUnderlineLinks(const char * /*newpref*/, void * /*stuff*/)
{
	XP_Bool value;
	PREF_GetBoolPref(sUnderlineLinksPref, &value);
	sUnderlineLinks = value;
	return 0;
}

/*
// Hash it.
// Time critical
uint32 HyperStyle::StyleHash( const void* style )
{
	return ((	((HyperStyle*)style)->fFont << 1 ) + 
				((HyperStyle*)style)->fSize );
}

// Compare two hyperstyles
// time critical routine
// should function like strcmp
int HyperStyle::StyleCompFunction( const void* style1, const void* style2 )
{
	Boolean equal = ( 	style1 &&
						style2 &&
					(((HyperStyle*)style1)->fFont == ((HyperStyle*)style2)->fFont) && 
					(((HyperStyle*)style1)->fSize == ((HyperStyle*)style2)->fSize) );

	if ( equal )
		return 0;
	else if ( !style1 )
		return -1;
	else if ( !style2 )
		return 1;
	else if (
		(((HyperStyle*)style1)->fFont + ((HyperStyle*)style1)->fSize) > 
		(((HyperStyle*)style2)->fFont + ((HyperStyle*)style2)->fSize) )
		return -1;
	else
		return 1;
}
*/

void HyperStyle::GetFontInfo()
{
/*
	HyperStyle* hashStyle = (HyperStyle*)XP_Gethash( sFontHash, this, NULL );
	if ( !hashStyle )
	{		
		fFontReference->GetFontInfo(&fFontInfo);

		hashStyle = new HyperStyle(this);
		XP_Puthash(sFontHash, hashStyle, hashStyle);
	}
	else
		fFontInfo = hashStyle->fFontInfo;
*/
	fFontReference->GetFontInfo(&fFontInfo);
}

//	TextWidth Dispatch routine
short  HyperStyle::TextWidth(char* text, int firstByte, int byteCount )
{	
	return fFontReference->TextWidth(text, firstByte, byteCount);
}

/*-----------------------------------------------------------------------------
UGraphics is full of little graphics utilities
Original version by atotic
mark messed it up
-----------------------------------------------------------------------------*/

void UGraphics::Initialize()
{
}

void UGraphics::SetFore( CPrefs::PrefEnum r )
{
	UGraphics::SetIfColor( CPrefs::GetColor( r ) );
}

void UGraphics::SetBack( CPrefs::PrefEnum r )
{
	UGraphics::SetIfBkColor( CPrefs::GetColor( r ) );
}

void UGraphics::VertLine( int x, int top, int height, CPrefs::PrefEnum r )
{
	UGraphics::SetFore( r );
	MoveTo( x, top );
	LineTo( x, top + height - 1 );
}

void UGraphics::HorizLineAtWidth (int vertical, int left, int width, CPrefs::PrefEnum r)
{
	UGraphics::SetFore (r);
	MoveTo (left, vertical);
	LineTo (left + width - 1, vertical);
}


// Draws a diagonal to the rectangle
void UGraphics::DrawLine( int16 top, int16 left, int16 bottom, int16 right, CPrefs::PrefEnum color )
{
	UGraphics::SetFore( color );
	MoveTo( left, top );
	LineTo( right, bottom );
}

CGrafPtr UGraphics::IsColorPort( GrafPtr port )
{
	Assert_( port );
	return ((((CGrafPtr)port)->portVersion) & 0xC000) == 0xC000? (CGrafPtr) port: nil;
}

//
//	Important Note!
//	If you are going to muck with the colors of a window, be sure to give it
//	a custom wctb resource! Otherwise the window will be created with a pointer
//	to the SYSTEMWIDE global AuxRec and you'll change EVERYTHING.
//

void 
UGraphics::SetWindowColor( GrafPort* window, short field, const RGBColor& color )
{
	if ( !UEnvironment::HasFeature( env_SupportsColor ) )
		return;
	
	// Get window's auxilary record. Skip on failure
	AuxWinHandle		aux = NULL;
	AuxWinHandle		def = NULL;
	
	CTabHandle			awCTable = NULL;
	Boolean				foundFieldInTable = FALSE;
	Boolean				hasAuxWinRec = FALSE;
	Boolean				isDefault = FALSE;
	
	Assert_( window );
	if ( !window )
		return;
		
	hasAuxWinRec = ::GetAuxWin( window, &aux );
	::GetAuxWin( NULL, &def );
	isDefault = (*aux)->awCTable == (*def)->awCTable;
	
	if ( isDefault || !hasAuxWinRec || !aux )
	{
		XP_TRACE(("UGraphics::SetWindowColor (%p) -> nil", window));
		return;
	}

	// Get the color table
	// Find the fore/background colors, and set them to the unified scheme
	awCTable = (*aux)->awCTable;
	
	for ( unsigned long i = 0; i <= (*awCTable)->ctSize; i++ )
	{
		if ( (*awCTable)->ctTable[ i ].value == field )
		{
			foundFieldInTable = true;
			if ( UGraphics::EqualColor( (*awCTable)->ctTable[ i ].rgb, color ) )
				return;
			
			(*awCTable)->ctTable[ i ].rgb = color;
			CTabChanged( awCTable );
		}	
	}
	Assert_( foundFieldInTable );
}

void UGraphics::SetWindowColors (LWindow *window)
{
	SetWindowColor (window->GetMacPort(), wContentColor, CPrefs::GetColor(CPrefs::WindowBkgnd));
}

void UGraphics::FrameRectMotif (const Rect& box, Boolean inset)
{
	RGBColor lighter = {65535,65535,65535}, darker = {0,0,0};
	FrameRectBevel(box, inset, lighter, darker);
}


void UGraphics::FrameRectSubtle (const Rect& box, Boolean inset)
{
	RGBColor lighter = {60000,60000,60000}, darker = {20000,20000,20000};
	FrameRectBevel(box, inset, lighter, darker);
}

void UGraphics::FrameRectBevel (const Rect& box, Boolean inset, const RGBColor &lighter,
									const RGBColor &darker )
{		
	::PenSize(1,1);						
	if (inset)	UGraphics::SetIfColor (darker);
	else		UGraphics::SetIfColor (lighter);
	MoveTo (box.left, box.top);
	LineTo (box.right-1, box.top);
	MoveTo (box.left, box.top);
	LineTo (box.left, box.bottom-1);
	
	if (inset)	UGraphics::SetIfColor (lighter);
	else		UGraphics::SetIfColor (darker);
	MoveTo (box.right-1, box.bottom-1);
	LineTo (box.right-1, box.top+1);
	MoveTo (box.right-1, box.bottom-1);
	LineTo (box.left+1, box.bottom-1);
}

void UGraphics::FrameRectShaded (const Rect& box, Boolean inset)
{
	RGBColor addOn = {20000, 20000, 20000};
	RGBColor lighter = {60000,60000,60000}, darker = {0,0,0};
	// subPin substracts value, makes color darker
	// addPin adds value, makes color lighter
	SetIfColor(addOn);
	if (inset)
	{
		OpColor(&darker);
		PenMode(subPin);
	}
	else
	{	
		OpColor(&lighter);
		PenMode(addPin);
	}
	MoveTo (box.left, box.top);
	LineTo (box.right-1, box.top);
	MoveTo (box.left, box.top);
	LineTo (box.left, box.bottom-1);
	
	if (inset)
	{	
		OpColor(&lighter);
		PenMode(addPin);
	}
	else
	{
		OpColor(&darker);
		PenMode(subPin);
	}
	MoveTo (box.right-1, box.bottom-1);
	LineTo (box.right-1, box.top+1);
	MoveTo (box.right-2, box.bottom-1);
	LineTo (box.left+1, box.bottom-1);
		PenMode(srcCopy);
}

void DrawArc( const RGBColor& initColor, const Rect& box, Boolean inset )
{
	RGBColor lighter = {60000,60000,60000}, darker = {0,0,0};

	UGraphics::SetIfColor( initColor );
	if ( inset )
	{
		OpColor( &darker );
		PenMode( subPin );
	}
	else
	{	
		OpColor( &lighter );
		PenMode( addPin );
	}
	FrameArc( &box, 225, 180 );
	if ( inset )
	{	
		OpColor( &lighter );
		PenMode( addPin );
	}
	else
	{
		OpColor( &darker );
		PenMode( subPin );
	}
	FrameArc( &box, 45, 180 );
}

void UGraphics::FrameCircleShaded( const Rect& box, Boolean inset )
{
	RGBColor first = { 20000, 20000, 20000 };	
	RGBColor second = { 30000, 30000, 30000 };
	// subPin substracts value, makes color darker
	// addPin adds value, makes color lighter
	DrawArc( first, box, inset );
	Rect tempRect = box; // box is CONST!
	InsetRect( &tempRect, -1, -1 );
	DrawArc( second, tempRect, inset );	
	PenMode( srcCopy );
}

void ArithHighlight( const Rect& box, const RGBColor& curr );
void ArithHighlight( const Rect& box, const RGBColor& curr )
{
	RGBColor	tmp = curr;

	tmp.red = ~tmp.red;
	tmp.green = ~tmp.green;
	tmp.blue = ~tmp.blue;

	RGBColor darker = { 0, 0, 0};
	RGBColor addOn = { 30000, 30000, 30000 };

	UGraphics::SetIfColor( addOn );
	OpColor( &tmp );
	
	PenMode( addPin );
	PenPat( &UQDGlobals::GetQDGlobals()->black );
	::PaintRect( &box );
	PenMode( patCopy );
}

void UGraphics::FrameTopLeftShaded( const Rect& box, short width, const RGBColor& curr )
{
	Rect		l;
	l.top = box.top;
	l.left = box.left;
	l.right = box.right;
	l.bottom = box.top + width;
	ArithHighlight( l, curr );
	l.top += width;
	l.right = l.left + width;
	l.bottom = box.bottom;
	ArithHighlight( l, curr );
}

void UGraphics::FrameEraseRect( const Rect& box, short width, Boolean focussed )
{
	Rect lineRect;

	// top left -> top right
	lineRect.top = box.top;
	lineRect.left = box.left;
	lineRect.bottom = lineRect.top + width;
	lineRect.right = box.right;

	::EraseRect( &lineRect );
	if ( focussed )
		DarkenRect( lineRect );

	// bottom left -> bottom right
	lineRect.top = box.bottom - width;
	lineRect.bottom = box.bottom;
	::EraseRect( &lineRect );
	if ( focussed )
		DarkenRect( lineRect );

	// top left -> bottom left
	lineRect.top = box.top;
	lineRect.left = box.left;
	lineRect.bottom = box.bottom;
	lineRect.right = lineRect.left + width;
	::EraseRect( &lineRect );
	if ( focussed )
		DarkenRect( lineRect );

	// top right -> bottom right
	lineRect.left = box.right - width;
	lineRect.right = box.right;
	::EraseRect( &lineRect );
	if ( focussed )
		DarkenRect( lineRect );
}

void UGraphics::DarkenRect( const Rect& box )
{
	RGBColor darker = { 0, 0, 0};
	RGBColor addOn = { 10000, 10000, 10000 };
	SetIfColor( addOn );
	OpColor( &darker );
	PenMode( subPin );
	PenPat( &UQDGlobals::GetQDGlobals()->black );
	::PaintRect( &box );
	PenMode( patCopy );
}

Boolean	UGraphics::PointInBigRect(SPoint32 rectLoc, SDimension16 rectSize, SPoint32 loc)
{
	return ((loc.v > rectLoc.v) &&
		(loc.v < (rectLoc.v + rectSize.height)) &&
		(loc.h > rectLoc.h) &&
		(loc.h < (rectLoc.h + rectSize.width)));
}



RgnHandle UGraphics::sTempRegions[] = { NULL, NULL, NULL, NULL, NULL };
Boolean UGraphics::sRegionsUsed[] = { false, false, false, false, false };

//
// Get a temporary region from our cache (array, actually) of
// allocated regions, if possible.  A parallel array of booleans
// indicates which regions are in use; if all are unavailable,
// just allocate a new one.  Call ReleaseTempRegion() when youÕre
// done using the region.
//
RgnHandle UGraphics::GetTempRegion()
{
	for (short i = 0; i < kRegionCacheSize; i++)
	{
		if (sRegionsUsed[i] == false)
		{
			sRegionsUsed[i] = true;
			if (sTempRegions[i] == NULL)
				sTempRegions[i] = ::NewRgn();
			return sTempRegions[i];
			
		}
	}
	
	return (::NewRgn());
}

//
// Release a temporary region returned from GetTempRegion(). 
// We call SetEmptyRgn() here to save some memory, and because
// when the region is reused in GetTempRegion the caller will
// expect the ÒnewÓ region to be empty.  If the region wasnÕt
// found in the cache array, then it must have been freshly
// allocated by GetTempRegion because all cached regions were
// in use at the time, so just dispose of the region.
//
void UGraphics::ReleaseTempRegion(RgnHandle rgn)
{
	for (short i = 0; i < kRegionCacheSize; i++)
	{
		if (sTempRegions[i] == rgn)
		{
			sRegionsUsed[i] = false;
			SetEmptyRgn(rgn);
			return;
		}
	}
	
	::DisposeRgn(rgn);
}


/*****************************************************************************
 * class CEditBroadcaster
 * LEditField that notifies listeners about its changes
 *****************************************************************************/

CEditBroadcaster::CEditBroadcaster(LStream *inStream)
	:	CTSMEditField(inStream)
{
}

void CEditBroadcaster::UserChangedText()
{
	BroadcastMessage(msg_EditField, this);
}

/*****************************************************************************
 * class CGAEditBroadcaster
 * LGAEditField that notifies listeners about its changes
 *****************************************************************************/

CGAEditBroadcaster::CGAEditBroadcaster(LStream *inStream)
	:	LGAEditField(inStream)
{
}

void CGAEditBroadcaster::UserChangedText()
{
	BroadcastMessage(msg_EditField, this);
}

void
CGAEditBroadcaster::BroadcastValueMessage()
{
	BroadcastMessage(msg_EditField, this);
}

/*****************************************************************************
 * class CTextEdit
 * LTextEdit that keeps track if it has been modified
 *****************************************************************************/

CTextEdit::CTextEdit(LStream *inStream) : LTextEdit(inStream)
{
	fModified = FALSE;
}

void CTextEdit::UserChangedText()
{
	fModified = TRUE;
	LTextEdit::UserChangedText();
}

/*****************************************************************************
 * class CResPicture
 * LPicture with an associated resfile ID
 *****************************************************************************/
CResPicture::CResPicture(LStream *inStream)
	: LPicture(inStream)
{
	mResFileID = -1;
}

void CResPicture::DrawSelf()
{
	Int16 resfile = ::CurResFile();
	if (mResFileID != -1)
		::UseResFile(mResFileID);
	
	LPicture::DrawSelf();
	
	::UseResFile(resfile);
}


#ifdef PROFILE
#pragma profile off
#endif

