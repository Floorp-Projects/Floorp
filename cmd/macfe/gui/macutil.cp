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

/***********************************************************************************
 * macutil.cp
 *
 * Various mac utility routines
 *
 ***********************************************************************************/

#include <stdarg.h>

#include <Traps.h>
#include <LPlaceHolder.h>
#include <LHandleStream.h>
//#include <LTextEngine.h>
#include "macutil.h"
#include "resgui.h"
#include "uprefd.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "xp.h"
#include "cstring.h"
#include "xpassert.h"
#include "xp_trace.h"
#include "macgui.h"
#include "prefapi.h"
#include "MoreDesktopMgr.h"
extern "C" {
	#include "asyncCursors.h"
}

#include "mfinder.h" // needed for workaround function to bug in Apple's
					 // ::TruncText and ::TruncString - andrewb 6/20/97
					 
#include "MoreMixedMode.h"
#include "PascalString.h"

/*-----------------------------------------------------------------------------
	Layout Utilities
-----------------------------------------------------------------------------*/

// Another mlanett special
static void GetImageRect( LView* view, SPoint32* topLeft, SPoint32* botRight );
static void GetImageRect( LView* view, SPoint32* topLeft, SPoint32* botRight )
{
/*	
	Return the visible image of this view.
*/
	SPoint32		frameLocation;
	SDimension16	frameSize;
	SPoint32		imageLocation;

	view->GetFrameLocation( frameLocation );
	view->GetFrameSize( frameSize );
	view->GetImageLocation( imageLocation );

	// convert local rectangle to image rectangle
	topLeft->h 	= frameLocation.h - imageLocation.h; // frameLocation.h - portOrigin.h - imageLocation.h;
	botRight->h = topLeft->h + frameSize.width;
	topLeft->v 	= frameLocation.v - imageLocation.v; // frameLocation.v - portOrigin.v - imageLocation.v;
	botRight->v = topLeft->v + frameSize.height;
}

void RevealInImage( LView* view, SPoint32 selTopLeft, SPoint32 selBotRight )
{
/*
	Scroll the minimum amount necessary to reveal this image rectangle.
	In this diagram, the selection is scrolled above the visible rectangle.
	Therefore selection-top is less than image-top, so we need to scroll
	down by above-amount.
				/---\
			/---|   |
			|	\---/
			v
	/--------------\
	|              |
	|              |
	|              |
	|              |
	\--------------/
*/
	SPoint32	iTopLeft;
	SPoint32	iBotRight;

	GetImageRect( view, &iTopLeft, &iBotRight );
	
	Int32 aboveAmount = selTopLeft.v - 20 - iTopLeft.v;
	Int32 belowAmount = selTopLeft.v + 20 - iTopLeft.v;
	Int32 leftAmount = selTopLeft.h - iTopLeft.h;
	Int32 rightAmount = selBotRight.h - iBotRight.h;
	
	if ( aboveAmount < 0 )
		view->ScrollImageBy( 0, aboveAmount, TRUE );
	else if ( ( selBotRight.v - iBotRight.v ) > 0 )
	{
		// don't scroll the top off!
		// belowAmount and aboveAmount are negative
		// belowAmount must be (absolute) less than aboveAmount
		view->ScrollImageBy( 0, belowAmount > aboveAmount ? aboveAmount : belowAmount, TRUE );
	}
	
	if ( leftAmount < 0 )
		view->ScrollImageBy( leftAmount, 0, TRUE );
	else if ( rightAmount > 0 )
	{
		// don't scroll the left off!
		// belowAmount and leftAmount are negative
		// belowAmount must be (absolute) less than aboveAmount
		view->ScrollImageBy( rightAmount > leftAmount? leftAmount: rightAmount, 0, TRUE );
	}
}

Boolean TooDifferent( const Point& a, const Point& b )
{
	return abs( b.h - a.h ) >= MOUSE_HYSTERESIS || abs( b.v - a.v ) >= MOUSE_HYSTERESIS;
}

short WaitForMouseAction( const Point& initial, long clickStart, long delay )
{
	Point		current;
	long		now;
	
	while ( ::StillDown() )
	{
		::GetMouse( &current );
		if ( TooDifferent( initial, current ) )
			return MOUSE_DRAGGING;
		now = ::TickCount();
		if ( abs( now - clickStart ) > delay)
			return MOUSE_TIMEOUT;
	}
	
	return MOUSE_UP_EARLY;
}


int // offset
CalcCurrentSelected (lo_FormElementSelectData * selectData, Boolean fromDefaults)
{
	int offset = 0;
	
	lo_FormElementOptionData * optionData;
	PA_LOCK(optionData, lo_FormElementOptionData *, selectData->options);
	for (int i=0; i<selectData->option_cnt; i++) {
		char * itemName = NULL;
		PA_LOCK(itemName, char*, optionData[i].text_value);
		if (fromDefaults) {
			if (optionData[i].def_selected) {
				offset = i;
				break;
			}
		}
		else {
			if (optionData[i].selected) {
				offset = i;
				break;
			}
		}
		PA_UNLOCK(optionData[i].text_value);
	}
	PA_UNLOCK(selectData->options);
	
	return offset;
}

/*-----------------------------------------------------------------------------
	Generic Utilities
-----------------------------------------------------------------------------*/

Rect CenterInRect( Point thing, Rect destSpace )
{
	Point		destSize;
	Point		centeredSize;
	Rect		centeredThing;
	
	destSize.h = destSpace.right - destSpace.left;
	destSize.v = destSpace.bottom - destSpace.top;
	centeredSize.h = MIN( destSize.h, thing.h );
	centeredSize.v = MIN( destSize.v, thing.v );
	centeredThing.left = destSpace.left + ( destSize.h - centeredSize.h ) / 2;
	centeredThing.top = destSpace.top + ( destSize.v - centeredSize.v ) / 2;
	centeredThing.right = centeredThing.left + centeredSize.h;
	centeredThing.bottom = centeredThing.top + centeredSize.v;
	return centeredThing;
}

/*-----------------------------------------------------------------------------
	String Copying
-----------------------------------------------------------------------------*/

unsigned char* CopyString( unsigned char* destination, const unsigned char* source )
{
	BlockMove (source, destination, source[0]+1);
	return destination;
}

unsigned char* CopyString( unsigned char* destination, const char* source )
{
	destination[0] = strlen (source);
	BlockMove (source, destination + 1, destination[0]);
	return destination;
}

char* CopyString( char* destination, const unsigned char* source )
{
	BlockMove (source + 1, destination, source[0]);
	destination[source[0]] = 0;
	return destination;
}

char* CopyString( char* destination, const char* source )
{
	register int length = strlen (source);
	BlockMove (source, destination, length + 1);
	return destination;
}

void CopyAlloc(char ** destination, StringPtr source)
{
	// NET_SACopy frees a non-null destination itself
	//if (*destination != NULL)
	//	XP_FREE(*destination);
	p2cstr(source);
	
	// We don't really want to delete any existing storage for destination so borrow
	// some code from NET_SACopy and skip the XP_FREE of destination
	//NET_SACopy(destination, (char*)source);
    if (! source)
    {
    	*destination = NULL;
    }
    else
    {
    	*destination = (char *) XP_ALLOC (XP_STRLEN((const char *)source) + 1);
    	if (*destination != NULL)
    		XP_STRCPY (*destination, (const char *)source);
    }
    
	c2pstr((char*)source);
}

// Takes colons out of a string
void StripColons( CStr31& fileName )
{
	short count;
	
	for ( count = 1; count <= fileName.Length(); count++ )
		if ( fileName[count] == ':' )
			fileName[count] = ' ';
}

// strips single instances of ch from the given string.
// If ch appears twice in a row, the second one is not removed.
void StripChar( CStr255& str, char ch )
{
	int len = str.Length();
	for (short i = 1; i <= len; i++)
		if (str[i] == ch) {
			BlockMoveData(&str[i + 1], &str[i], len - i);
			i++;
			len -= 1;
		}
	str[0] = len;
}

// • Takes a string of text (dragged/pasted), and turns
// it into a URL (strips CR, whitespace preceeding/trailing <>
void CleanUpTextToURL(char* text)
{
	if (text == NULL)
		return;
	
	long len = XP_STRLEN(text);
			
	int i=0;
	
	if (text[0] == '<')	// Swallow the surrounding '<' and '>'
		::BlockMoveData(&text[1], &text[0], len);
	
	while (text[i] != 0) // Loop till whitespace
	{
		if (XP_IS_SPACE(text[i]))
			break;
		i++;
	}
	if (text[i-1] == '>')	// Swallow the ending '>'
		i--;

	text[i] = 0;	
}

void CleanUpLocationString (CStr255 & url)
{
//	Strips spaces & returns at end of url (happens often
//	when you paste into url box)
	int last;
	while ((last = url.Length()) != 0) {
		if (url[last] == '\n' || url[last] == ' ' || url[last] == '\r')
			url.Length()--;
		else break;
	}
}

// TRUE if the key is down. Bypasses the event loop
Boolean IsThisKeyDown(const UInt8 theKey)
{
	KeyMap map;
	
	GetKeys(map);
	
	return ((*((UInt8 *)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
} // IsThisKeyDown - don's improved non-union (as in struct) version



// A mess that figures out if cmd-. was pressed
/* Borrowed from tech note 263 */

#define kMaskModifiers  	0xFE00     	// we need the modifiers without the
                                   		// command key for KeyTrans
#define kMaskVirtualKey 	0x0000FF00 	// get virtual key from event message
                                   		// for KeyTrans
#define kUpKeyMask      	0x0080
#define kShiftWord      	8          	// we shift the virtual key to mask it
                                   		// into the keyCode for KeyTrans
#define kMaskASCII1     	0x00FF0000 	// get the key out of the ASCII1 byte
#define kMaskASCII2     	0x000000FF 	// get the key out of the ASCII2 byte
#define kPeriod         	0x2E       	// ascii for a period
Boolean CmdPeriod()
{
	EvQElPtr		eventQ = (EvQElPtr)LMGetEventQueue()->qHead;

	while (eventQ != NULL)
	{
		EventRecord * theEvent =  (EventRecord *)&eventQ->evtQWhat;

	  	UInt16    keyCode;
	  	UInt32	  state;
	  	long     virtualKey, keyInfo, lowChar, highChar, keyCId;
	  	Handle   hKCHR;
		Ptr 		KCHRPtr;
	
	
		if (((*theEvent).what == keyDown) || ((*theEvent).what == autoKey)) {
	
			// see if the command key is down.  If it is, find out the ASCII
			// equivalent for the accompanying key.
	
			if ((*theEvent).modifiers & cmdKey ) {
	
				virtualKey = ((*theEvent).message & kMaskVirtualKey) >> kShiftWord;
				// And out the command key and Or in the virtualKey
				keyCode    = short(((*theEvent).modifiers & kMaskModifiers) | virtualKey);
				state      = 0;
	
	
				hKCHR = nil;  /* set this to nil before starting */
			 	KCHRPtr = (Ptr)GetScriptManagerVariable(smKCHRCache);
	
				if ( !KCHRPtr ) {
					keyCId = ::GetScriptVariable(short(GetScriptManagerVariable(smKeyScript)), smScriptKeys);
	
					hKCHR   = GetResource('KCHR',short(keyCId));
					KCHRPtr = *hKCHR;
				}
				if (KCHRPtr) {
					keyInfo = KeyTranslate(KCHRPtr, keyCode, &state);
					if (hKCHR)
						ReleaseResource(hKCHR);
				} else
					keyInfo = (*theEvent).message;
	
				lowChar =  keyInfo &  kMaskASCII2;
				highChar = (keyInfo & kMaskASCII1) >> 16;
				if (lowChar == kPeriod || highChar == kPeriod)
					return TRUE;
	
			}  // end the command key is down
		}  // end key down event
		eventQ = (EvQElPtr)eventQ->qLink;
	}
	return FALSE;
}

// Sets button state and redraws it
void SetEnable( LPane* button, Boolean enable )
{
	if ( !button->IsEnabled() && enable )
		button->Enable();
	else if ( button->IsEnabled() && !enable )
		button->Disable();
}

// Changes the string to something acceptable to mac menu manager
void CreateMenuString( CStr255& itemName )
{
	itemName=NET_UnEscape(itemName);
	if ( itemName.Length() > 50 )	// Cut it down to reasonable size
	{
		itemName.Length() = 50;
		itemName += "...";
	}
	if ( itemName.Length() && itemName[ 1 ] == '-' )
		itemName[ 1 ] = '–';
}

// TextHandleToHardLineBreaks
// converts TextEdit handle to text handle that has hard carriage returns.

Handle TextHandleToHardLineBreaks( CSimpleTextView &inTextView )
{
	Handle theOldText = inTextView.GetTextHandle();
	ThrowIfNULL_(theOldText);
	
	SInt32 theTotalLength = inTextView.GetTextLength();
	
//	TextRangeT theTotalRange = 	inEngine->GetTotalRange();

//	Int32 theLineCount =  inEngine->CountParts(theTotalRange, cLine);

	Int32 theLineCount = inTextView.CountLines();
	
	// text will expand by at most as many line breaks as there are lines
	Handle theNewText = ::NewHandle(theTotalLength + theLineCount);
	ThrowIfNULL_(theNewText);

	// Lock down both text buffers
	StHandleLocker theOldLock(theOldText);
	StHandleLocker theNewLock(theNewText);
	
	Int32 theBreakCount = 0;
	Int32 theNewTextOffset = 0;
	SInt32	lineStart	 = 0;
	SInt32	lineEnd			= 0;
		
	Boolean bEatAgain = false;
	
	for (Int32 theIndex = 1; theIndex <= theLineCount; theIndex++)
		{
//		TextRangeT theLineRange;
//		inEngine->FindNthPart(theTotalRange, cLine, theIndex, &theLineRange);
			
			// Waste appears to be Zero based rather than 1
			inTextView.GetLineRange( theIndex - 1, &lineStart, &lineEnd );
					
			::BlockMoveData(*theOldText + lineStart, *theNewText + theNewTextOffset, lineEnd - lineStart );
			theNewTextOffset += lineEnd - lineStart;

			Boolean eatNextLine = ((theIndex + 1) < theLineCount) 					&& // This is to prevent overflow on end
								(((*theOldText)[lineStart] == '>' 		&&
								(*theOldText)[lineEnd + 1] != '>'))		||
								 bEatAgain;
								 
			if ((*theNewText)[theNewTextOffset -1 ] == CR)
				{
				(*theNewText)[theNewTextOffset - 1] = CR;
				bEatAgain = false;
				}
			else
				{
				if (eatNextLine)
					bEatAgain = true;	// Only
				else
					{
					(*theNewText)[++theNewTextOffset - 1] = CR;
					bEatAgain = false;
					}
				}
		}

	::SetHandleSize(theNewText, theNewTextOffset); // Reduce the size of the handle
	return theNewText;
}


void ConstrainTo( const long& min, const long& max, long& value )
{
	XP_WARN_ASSERT( min <= value && value <= max );
	if ( value < min )
		value = min;
	else if ( value > max )
		value = max;
}

void * StructCopy(const void * struc, UInt32 size)
{
	if (struc == NULL)
		return NULL;

	void * newStruct = XP_ALLOC(size);
	if (newStruct == NULL)
		return NULL;
	::BlockMoveData(struc, newStruct, size);
	return newStruct;
}


void SetMenuSize( LPopupButton* popup, short shouldBe )
{
	MenuHandle		menu;
	short			count;
	
	menu = popup->GetMacMenuH();
	count = ::CountMItems( menu );
	
	while ( shouldBe > count )
	{	
		InsertMenuItem( menu, "\p ", count );
		count++;
	}
	
	while ( shouldBe < count )
	{
		DeleteMenuItem( menu, count );
		count--;
	}
	
	popup->SetMinValue( 1 );
	popup->SetMaxValue( shouldBe );
}

void SetMenuSizeForLGAPopup( LGAPopup* popup, short shouldBe )
{
	MenuHandle		menu;
	short			count;
	
	menu = popup->GetMacMenuH();
	count = ::CountMItems( menu );
	
	while ( shouldBe > count )
	{	
		InsertMenuItem( menu, "\p ", count );
		count++;
	}
	
	while ( shouldBe < count )
	{
		DeleteMenuItem( menu, count );
		count--;
	}
	
	popup->SetMinValue( 1 );
	popup->SetMaxValue( shouldBe );
}

void SetMenuItem( CommandT whichItem, Boolean toState )
{
	ResIDT		menuID;
	MenuHandle	menu = NULL;
	Int16		item;
	LMenuBar*	currentMenuBar;

	currentMenuBar = LMenuBar::GetCurrentMenuBar();

	if (currentMenuBar)
		currentMenuBar->FindMenuItem( whichItem, menuID, menu, item );
 	if (menu)
 	{
 		if ( toState )
 			SetItemMark( menu, item, checkMark );
 		else
 			SetItemMark( menu, item, noMark );
 	}
}

// ---------------------------------------------------------------------------
//		• myStringToNum [static]
// ---------------------------------------------------------------------------
//	Utility used the USizeMenu routines to convert strings to numbers that
//	ignores anything that's not a number

#define	IsNumber_( x )	(( x ) >= '0' && ( x ) <= '9')

void myStringToNum( const CStr255& inString, Int32* outNum )
{
	CStr255		temp;
	short		j = 1;
	
	for ( short i = 1; i <= inString[ 0 ]; i++ )
	{
		if ( IsNumber_( inString[ i ] ) )
			temp[ j++ ] = inString[ i ];
	}
	
	temp[0] = j - 1;
	if (temp.Length() > 0)
		StringToNum( temp, outNum );
}

short PointsToPixels( short inPoints, short iRes )
{
	return ( (float)inPoints / 120.0 ) * iRes;
}

void SetCaptionDescriptor( LCaption* captionToSet, const CStr255& newText,
	const TruncCode trunc )
{
	XP_ASSERT( captionToSet );
	
	CStr255			tempText( newText );
	SDimension16	captionSize;
	
	captionToSet->GetFrameSize( captionSize );
	
	// DANGER! There is a hideous crashing bug in Apple's ::TruncString
	// (and ::TruncText) when called with TruncMiddle
	if (trunc == truncMiddle)
		MiddleTruncationThatWorks(tempText, captionSize.width - 4);
	else
		TruncString( captionSize.width - 4, tempText, trunc );
	captionToSet->SetDescriptor( tempText );
	captionToSet->Refresh();
}

void GetDateTimeString( CStr255& dateTime )
{
	unsigned long		dt;
	
	::GetDateTime( &dt );
	::DateString( dt, longDate, dateTime, NULL );
}

unsigned short GetTextLengthInPixels( LTextEdit* textEdit );
unsigned short GetTextLengthInPixels( LTextEdit* textEdit )
{
	XP_ASSERT( textEdit );
	
	unsigned short		width = 0;
	TEHandle			macTEH = NULL;
	Handle				text = NULL;
	short				length = 0;
	
	// • this sets up the current port with the right text traits
	if ( textEdit->FocusDraw() )
	{
		macTEH = textEdit->GetMacTEH();
		XP_ASSERT( macTEH );

		text = (*macTEH)->hText;		
		if ( text )
		{
			length = (*macTEH)->teLength;
			HLock( text );
			width = TextWidth( *text, 0, length );
			HUnlock( text );
		}
	}
	return width;
}

// Could be optimized
char * GetPaneDescriptor(LPane * pane)
{
	char * ret = nil;
	CStr255 ptempStr;
	pane->GetDescriptor(ptempStr);	
	cstring ctempStr((unsigned char*)ptempStr);
	if (ctempStr.length() > 0)
		StrAllocCopy(ret, ctempStr);
	return ret;
}

void TurnOn( LControl* control )
{
	control->SetValue( TRUE );
	control->BroadcastMessage( msg_ControlClicked, (void*)control );
}


//
// Why pass both the control _and_ the menu when one will do? This is an artifact of
// trying to get rid of the LGA* stuff from all files that don't need it. Once we can do
// that, we can return this routine to it's old glory of only taking the control (pinkerton).
//
Boolean SetMenuToNamedItem( LControl* inControl, MenuHandle inMenu, const CStr255& itemText )
{
	short menuSize = CountMItems( inMenu );
	
	for ( short i = 1; i <= menuSize; i++ )
	{
		CStr255 currItemName;
		::GetMenuItemText( inMenu, i, currItemName );
		if ( itemText == currItemName )
		{
			inControl->SetValue( i );
			return TRUE;
		}
	}
	return FALSE;
}


unsigned long GetFreeSpaceInBytes( short vRefNum )
{
	HVolumeParam	pb;
	OSErr			err;
	unsigned long	result;
	
	pb.ioCompletion = NULL;
	pb.ioVolIndex = 0;
	pb.ioNamePtr = NULL;
	pb.ioVRefNum = vRefNum;
	
	err = PBHGetVInfoSync( (HParmBlkPtr)&pb );
	
	if ( err == noErr )
		result = pb.ioVFrBlk * pb.ioVAlBlkSiz;
	else
		result = 0;
	
	return result;
}

// • force a window to appear on the desktop
void ForceWindowOnScreen( LWindow* window )
{
	// • get the desktop region
	StRegion		desktopRgn( LMGetGrayRgn() );
	
	Rect			windowFrame;
	window->CalcPortFrameRect( windowFrame );
	windowFrame = PortToGlobalRect( window, windowFrame );
	
	StRegion		windowRgn( windowFrame );

	StRegion		destRgn;
	Rect			newRect;
	
	::SectRgn( windowRgn, desktopRgn, destRgn );
	
	if ( ::EmptyRgn( destRgn ) ||
		!::EqualRgn( destRgn, windowRgn ) )
	{
		newRect = (**(static_cast<RgnHandle>(destRgn))).rgnBBox;
		
		// • add 20 for title, 20 for menu bar
		newRect.top = 40 + 2;
		newRect.bottom = newRect.top + ( windowFrame.bottom - windowFrame.top );
		newRect.left = 2;
		newRect.right = newRect.left + ( windowFrame.right - windowFrame.left );

		window->DoSetBounds( newRect );
	}

}

void GrowWindowToScreenHeight( LWindow* win )
{
	GDHandle		device;
	Rect			windowFrame;
	
	win->CalcPortFrameRect( windowFrame );
	windowFrame = PortToGlobalRect( win, windowFrame );

	device = WindowOnSingleDevice( win );
	if ( device )
	{
		windowFrame.bottom = (**device).gdRect.bottom - 5;
		win->DoSetBounds( windowFrame );
	}
}
	
static
Boolean WillBeVisible( Rect& windowRect )
{
	Rect			newRect;
	Boolean			result = FALSE;
	
	StRegion		desktopRgn( LMGetGrayRgn() );
	StRegion		windowRgn( windowRect );
	StRegion		destRgn;

	::SectRgn( windowRgn, desktopRgn, destRgn );

	newRect = (**static_cast<RgnHandle>(destRgn)).rgnBBox;
	
	if ( EmptyRgn( destRgn ) )
		result = FALSE;
	else if ( !EqualRect( &newRect, &windowRect ) )
		result = FALSE;
	else
		result = TRUE;
	
	return result;
}

void StaggerWindow( LWindow* win )
{
	WindowPtr		nextWindow;
	LWindow*		similarWindow;
	Rect			similarBounds;
	Rect			bounds;
	Boolean			changed = FALSE;
		
	win->CalcPortFrameRect( bounds );
	bounds = PortToGlobalRect( win, bounds );		
	// • find the window with the same ID
	//		if it has the same top left corner, offset our rect by 20, 5
	nextWindow = ::FrontWindow();
	while ( nextWindow )
	{
		similarWindow = LWindow::FetchWindowObject( nextWindow );
		nextWindow = (WindowPtr)( (WindowPeek)nextWindow )->nextWindow;
		if (	similarWindow &&
				similarWindow->GetPaneID() == win->GetPaneID() )
		{
			similarWindow->CalcPortFrameRect( similarBounds );
			similarBounds = PortToGlobalRect( similarWindow, similarBounds );
			
			if ( ( similarBounds.top == bounds.top )  &&
				 ( similarBounds.left == bounds.left ) )
			{
				::OffsetRect( &bounds, 5, 20) ;
				changed = TRUE;
				// • loop again,  because we might run into another window
				nextWindow = ::FrontWindow();
			}
		}
	}
	if ( changed )
		win->DoSetBounds( bounds );
}

GDHandle WindowOnSingleDevice( LWindow* win )
{
	// • loop thru all GDevices
	Rect		windowFrame;
	GDHandle	device;
	
	win->CalcPortFrameRect( windowFrame );
	windowFrame = PortToGlobalRect( win, windowFrame );

	device = GetDeviceList();
	
	while ( device )
	{
		if ( UDrawingUtils::IsActiveScreenDevice( device ) )
		{
			Rect	intersection;
			// • find intersection of Window with this active screen Device
			if ( ::SectRect( &windowFrame, &(**device).gdRect, &intersection ) )
			{
				// • window intersects this Device
				if ( ::EqualRect( &windowFrame, &intersection ) )
					return device;
			}
				
		}
		device = GetNextDevice( device );
	}
	return NULL;
}

#define SAVE_VERSION 23

// • saving/restoring of the window's default state
Boolean RestoreDefaultWindowState(LWindow* inWindow)
{
	if (inWindow == NULL)
		return false;
			
	// We are responsible for disposing of the window data
	Handle theWindowData = CPrefs::ReadWindowData(inWindow->GetPaneID());
	if (theWindowData == NULL)
		return false;
	
	// The destructor of the stream will kill the window data
	LHandleStream stream(theWindowData);

	Int32 version;
	stream.ReadData(&version, sizeof( version ) );
	if ( version != SAVE_VERSION )
		return false;
		
	Rect theWindowBounds;
	stream.ReadData(&theWindowBounds, sizeof(theWindowBounds));

	if (theWindowBounds.right > theWindowBounds.left &&
		theWindowBounds.bottom > theWindowBounds.top &&
		WillBeVisible(theWindowBounds) )
		inWindow->DoSetBounds(theWindowBounds);

	inWindow->RestorePlace(&stream);
	ForceWindowOnScreen(inWindow);

	return true;
}

void StoreDefaultWindowState( LWindow* win )
{
	if ( !win )
		return;
		
	LHandleStream		stream;
	Rect				windowBounds;
	Int32				version = SAVE_VERSION;
		
	stream.WriteData( &version, sizeof( version ) );

	win->EstablishPort();
	win->CalcPortFrameRect( windowBounds );
	windowBounds = PortToGlobalRect( win, windowBounds );
	stream.WriteData( &windowBounds, sizeof( windowBounds ) );

	win->SavePlace( &stream );

	CPrefs::WriteWindowData( stream.GetDataHandle(), win->GetPaneID() );
}

// Is front window modal? Used for command enabling
Boolean IsFrontWindowModal()
{
	return UDesktop::FrontWindowIsModal();
}

// • Fetch a window title resource and fill in the current profile name
void GetUserWindowTitle(short id, CStr255& title)
{
	char profileName[32];
	int len = 32;
	PREF_GetCharPref("profile.name", profileName, &len);

	::GetIndString( title, WINDOW_TITLES_RESID, id );
	
	StringParamText(title, profileName);
}

		
// Returns true if we are the front application
Boolean IsFrontApplication()
{
	ProcessSerialNumber 	netscapeProcess;
	ProcessSerialNumber		frontProcess;
	Boolean					inFront;

	OSErr err = ::GetCurrentProcess( &netscapeProcess );
	if ( err != noErr )
		return FALSE;
	err = ::GetFrontProcess( &frontProcess );
	if ( err != noErr )
		return FALSE;
	err = ::SameProcess( &frontProcess, &netscapeProcess, &inFront );
	if ( err != noErr )
		return FALSE;
	return inFront;
}

void MakeFrontApplication()
{
	ProcessSerialNumber 	netscapeProcess;
	OSErr err = ::GetCurrentProcess( &netscapeProcess );
	if (err == noErr)
		SetFrontProcess(&netscapeProcess);
}

void FrameSubpaneSubtle( LView* view, LPane* pane )
{
	Rect	subFrame;
	
	GetSubpaneRect( view, pane, subFrame );
	::InsetRect( &subFrame, -1, -1 );
	UGraphics::FrameRectSubtle( subFrame, true );
}

void GetSubpaneRgn( LView* view, LPane* pane, RgnHandle rgn )
{
	Rect	frame;
	GetSubpaneRect( view, pane, frame );
	::RectRgn( rgn, &frame );
}

void GetSubpaneRect( LView* view, LPane* pane, Rect& frame )
{
	XP_ASSERT( view );
	XP_ASSERT( pane );
	pane->CalcPortFrameRect( frame );
	view->PortToLocalPoint( *(Point*)&( frame.top ) );
	view->PortToLocalPoint( *(Point*)&( frame.bottom) );
}

void WriteVersionTag( LStream* stream, Int32 version )
{
	stream->WriteData( &version, sizeof( version ) );
}

Boolean ReadVersionTag( LStream* stream, Int32 version )
{
	Int32		versionTag;
	stream->ReadData( &versionTag, sizeof( versionTag ) );
	return ( versionTag == version );
}

void TrySetCursor( int whichCursor )
{
	Cursor** c = GetCursor( whichCursor );
	if ( c )
		SetCursor( *c );
#ifdef DEBUG
	else
		SysBeep( 1 );
#endif
}

void StripNewline( CStr255& msg )
{
	if ( msg[ msg.Length() ] == '\n' )
		msg[ msg.Length() ] = 0;
}

void StripDoubleCRs( CStr255& msg )
{
	Boolean lastCR = FALSE;
	for ( UInt32 i = 1; i < msg.Length(); i++ )
	{
		if ( msg[ i ] == CR)
		{
			if ( lastCR )
			{
				msg[ i - 1 ] = ' ';
				lastCR = FALSE;
			}
			else
				lastCR = TRUE;
		}
		else
			lastCR = FALSE;
	}
}

int ConvertCRtoLF(CStr255 & msg)
{
	int ret = 0;
	for ( long i = 1; i <= msg[0]; i++ )
		if ( msg[ i ] == LF )
		{
			msg[ i ] = CR;
			ret++;
		}
	return ret;
}

Rect RectFromTwoPoints( Point p1, Point p2 )
{
	Rect r;
	r.top = MIN( p1.v, p2.v );
	r.bottom = MAX( p1.v, p2.v );
	r.left = MIN( p1.h, p2.h );
	r.right = MAX( p1.h, p2.h );

	return r;
}

// Return true if inEnclosingRect encloses any portion of inCheckRect
Boolean RectInRect(const Rect *inCheckRect, const Rect *inEnclosingRect) {

	return (!::EmptyRect(inEnclosingRect) &&
				!((inCheckRect->right <= inEnclosingRect->left) || 
			 	  (inCheckRect->bottom <= inEnclosingRect->top) ||
		  	 	  (inCheckRect->left >= inEnclosingRect->right) || 
		  	 	  (inCheckRect->top >= inEnclosingRect->bottom)));
}

const RGBColor rgbBlack = { 0x0000, 0x0000, 0x0000 };
const RGBColor rgbWhite = { 0xFFFF, 0xFFFF, 0xFFFF };


void DrawAntsRect( Rect&r, short mode )
{
	::PenPat( &(UQDGlobals::GetQDGlobals()->gray) );
	::PenMode( mode );
	::RGBForeColor( &rgbBlack );
	::RGBBackColor( &rgbWhite );
	::FrameRect( &r );
}

FontInfo SafeGetFontInfo(ResIDT textTraits)
{
	StTextState textState;
	UTextTraits::SetPortTextTraits(textTraits);
	FontInfo fInfo;
	::GetFontInfo(&fInfo);
	return fInfo;
}

void FrameButton( const Rect& box, Boolean pushed )
{
	UGraphics::FrameRectSubtle(box, pushed);
}

void HiliteRect( const Rect& r )
{
	LMSetHiliteMode( LMGetHiliteMode() & ! ( 1 << hiliteBit ) ) ;
	::InvertRect( &r );
}

void HiliteRgn( const RgnHandle& r )
{
	LMSetHiliteMode( LMGetHiliteMode() & ! ( 1 << hiliteBit ) ) ;
	::InvertRgn( r );
}
	
LPane* FindPaneHitBy( const Point& globalMouse )
{
	Int16		part;
	WindowPtr	macWindowP;
	LPane*		inPane = NULL;

	part = ::FindWindow( globalMouse, &macWindowP );
	if ( macWindowP && part == inContent )
	{
		LWindow*	whichWin;
		whichWin = LWindow::FetchWindowObject( macWindowP );
		if ( whichWin )
		{
			Point		localMouse;
			LPane*		nextPane;
				
			localMouse = globalMouse;
			whichWin->GlobalToPortPoint( localMouse );
			inPane = whichWin->FindSubPaneHitBy( localMouse.h, localMouse.v );
			nextPane = inPane;
			while ( nextPane )
			{
				inPane = nextPane;
				nextPane = nextPane->FindSubPaneHitBy( localMouse.h, localMouse.v );
			}
		}
	}
	return inPane;
}

// ---------------------------------------------------------------
//		Good 'ol Mac File Utilities
// ---------------------------------------------------------------


/*----------------------------------------------------------------------
		Get a full pathname given a sig and type
		In: sig of process to look for  ('MOSS' for nav but should use emSignature instead)
				type of process to look for  (typically APPL)
		Out:Returns the file path as a nul-terminated allocated block
				that must be freed by the caller using XP_FREE.
			Returns NULL on error;
			
		Q: Should the path contain / for the root? i.e. should it be
			file:///MacHD/...
		or	file://MacHD/..		** this form used now.
------------------------------------------------------------------------*/
char *PathURLFromProcessSignature (OSType sig, OSType type)
{
	ProcessSerialNumber	targetPSN;			// return process number
	FSSpec 				targetFileSpec;		// return file spec
	char				*appPath = NULL;
	char				*urlPath = NULL;
	char				prefix[] = "file:/";
	OSErr 				err;
	
	// Get the file spec of our app
	err = FindProcessBySignature(sig, type, targetPSN, &targetFileSpec);
	if (err != noErr) return NULL;
	
	appPath = CFileMgr::EncodedPathNameFromFSSpec(targetFileSpec, true);
	if (appPath == NULL) return NULL;
	
	urlPath = (char *)XP_ALLOC(strlen(appPath) + strlen(prefix) + 1);
	if (urlPath == NULL) {
		XP_FREE(appPath);
		return NULL;
	}
	
	XP_STRCPY(urlPath, prefix);
	XP_STRCPY(urlPath + strlen(prefix), appPath), 
	
	XP_FREE(appPath);
	
	return urlPath;
}

// Given a process signature, gets its serial number
ProcessSerialNumber GetPSNBySig( OSType sig )
{
	OSErr					err;
	FSSpec					fileSpec;
	ProcessInfoRec			info;
	ProcessSerialNumber		psn;

	info.processAppSpec = &fileSpec;
	psn.highLongOfPSN = 0;
	psn.lowLongOfPSN  = kNoProcess;
	while ( GetNextProcess( &psn ) == noErr)
	{
		info.processInfoLength = sizeof( ProcessInfoRec );
		info.processName = NULL;
		err= GetProcessInformation( &psn, &info );
		if ( info.processSignature == sig )
			return psn;
	}
	Throw_( paramErr );
	return psn;
}

#define NO_PROCESS -1		// Constant used to indicate that there is no process in the PSN
// Creates an NoProcess serial number
ProcessSerialNumber MakeNoProcessPSN()
{
	ProcessSerialNumber psn;
	psn.highLongOfPSN = NO_PROCESS;
	psn.lowLongOfPSN = NO_PROCESS;
	return psn;
}

// Do we have a process
Boolean HasProcess( const ProcessSerialNumber& psn )
{
	return ( ( psn.highLongOfPSN != NO_PROCESS ) || 
			( psn.lowLongOfPSN!=NO_PROCESS ) );
}

// Given an application signature, finds the process
OSErr FindProcessBySignature(OSType sig,
							OSType processType,
							ProcessSerialNumber& psn,
							FSSpec* fileSpec)
{
	OSErr err;
	
	ProcessInfoRec info;

	psn.highLongOfPSN = 0;
	psn.lowLongOfPSN  = kNoProcess;
	do
	{
		err= GetNextProcess(&psn);
		if( err == noErr )
		{

			info.processInfoLength = sizeof(ProcessInfoRec);
			info.processName = NULL;
			info.processAppSpec = fileSpec;

			err= GetProcessInformation(&psn, &info);
		}
	} while( (err == noErr) && 
			((info.processSignature != sig) || 
			(info.processType != processType)));

	if( err == noErr )
		psn = info.processNumber;

	return err;
} // FindProcessBySignature 


extern Boolean HasAppleEventObjects();

// On the 68K, the OSL glue checks to see whether it is 
// installed. If it's not, the functions do nothing. On the PPC,
// if the Code Fragment Manager is unable to resolve an OSL
// stub, it performs an ExitToShell. Therefore we must
// insure that the OSL is available before calling any of
// its functions. These are called throughout PowerPlant's
// LModel* classes.

// gestaltObjectSupportLibraryInSystem = 1,
// gestaltObjectSupportLibraryPowerPCSupport = 2
// gestaltAppleEventsPresent
// gestaltScriptingSupport
// gestaltOSLInSystem √√√ not gestaltObjectSupportLibraryPowerPCSupport
Boolean HasAppleEventObjects()
{
	static Boolean isKnown = false, isInstalled = false;
	
	
	if (!isKnown) 
	{
		long attr = 0;
		Boolean ae = UEnvironment::HasGestaltAttribute(gestaltAppleEventsAttr, gestaltAppleEventsPresent);
		Boolean ppc = UEnvironment::HasGestaltAttribute(gestaltAppleEventsAttr, gestaltOSLInSystem);
#ifdef powerc
		isInstalled = ae && ppc;
#else
		isInstalled = 1; // we're linked with glue which always works
#endif
		isKnown = true;
	}
	return isInstalled;
}

/*----------------------------------------------------------------------------
	GetDropLocationDirectory
	
	Given an 'alis' drop location AEDesc, return the volume reference
	number and directory ID of the directory.
	
	Entry:	dropLocation = pointer to 'alis' AEDesc record.
	
	Exit:	function result = error code.
			*volumeID = volume reference number.
			*directoryID = directory ID.
			
	From Apple "HFS Drag Sample" sample code.
----------------------------------------------------------------------------*/
OSErr GetDropLocationDirectory (AEDesc *dropLocation, long *directoryID, short *volumeID)
{
	OSErr			result;
	AEDesc			targetDescriptor;	// 'fss ' descriptor for target directory
	FSSpec			targetLocation;		// FSSpec for target directory
	CInfoPBRec		getTargetInfo;		// paramBlock to get targetDirID
	
	// Coerce the 'alis' descriptor to a 'fss ' descriptor
	result = AECoerceDesc(dropLocation, typeFSS, &targetDescriptor);
	if (result != noErr)
		return (result); 
	
	// Extract the FSSpec from targetDescriptor
	BlockMoveData((Ptr)(*targetDescriptor.dataHandle), (Ptr)&targetLocation, sizeof(FSSpec));
	
	result = AEDisposeDesc(&targetDescriptor);
	if (result != noErr)
		return (result);
	
	// Use PBGetCatInfo to get the directoryID of the target directory from the FSSpec
	
	getTargetInfo.dirInfo.ioNamePtr = targetLocation.name;
	getTargetInfo.dirInfo.ioVRefNum = targetLocation.vRefNum;
	getTargetInfo.dirInfo.ioFDirIndex = 0;
	getTargetInfo.dirInfo.ioDrDirID = targetLocation.parID;
	
	result = PBGetCatInfoSync(&getTargetInfo);
	if (result != noErr)
		return (result);
	
	// return directory ID and volume reference number
	
	*directoryID = getTargetInfo.dirInfo.ioDrDirID;
	*volumeID = targetLocation.vRefNum;
	
	return result;
}

/*----------------------------------------------------------------------------
	DragTargetWasTrash
	
	Check to see if the target of a drag and drop was the Finder trashcan.
	
	Entry:	dragRef = drag reference.
	
	Exit:	function result = true if target was trash.
----------------------------------------------------------------------------*/

Boolean DragTargetWasTrash (DragReference dragRef)
{
	AEDesc dropLocation;
	OSErr err = noErr;
	long dropDirID, trashDirID;
	short dropVRefNum, trashVRefNum;
	
	err = GetDropLocation(dragRef, &dropLocation);
	if (err != noErr) return false;
	
	if (dropLocation.descriptorType == typeNull) goto exit;
	
	err = GetDropLocationDirectory(&dropLocation, &dropDirID, &dropVRefNum);
	if (err != noErr) goto exit;
	
	err = FindFolder(dropVRefNum, kTrashFolderType, false, &trashVRefNum,
		&trashDirID);
	if (err != noErr) goto exit;
	
	if (dropVRefNum != trashVRefNum || dropDirID != trashDirID) goto exit;

	AEDisposeDesc(&dropLocation);
	return true;

exit:

	AEDisposeDesc(&dropLocation);
	return false;
}


long LArrowControl::fLastTicks = 0;
long LArrowControl::fDelay = 20;
long LArrowControl::fHits = 0;

LArrowControl::LArrowControl( const SPaneInfo& inPaneInfo, MessageT valueMessage ):
	LControl( inPaneInfo, valueMessage, 0, 0, 0 )
{
	mCurrPict = mArrowsPict;
}

void LArrowControl::HiliteControl( Int16 inHotSpot, Boolean inCurrInside )
{
	if ( inCurrInside )
	{
		if ( inHotSpot == mTopHalf )
			mCurrPict = mArrowsTopLit;
		else if ( inHotSpot == mBottomHalf )
			mCurrPict = mArrowsBotLit;
	}
	else
		mCurrPict = mArrowsPict;
		
	DrawSelf();
}

void LArrowControl::DrawSelf()
{
	PicHandle		macPictureH = GetPicture( mCurrPict );
	if ( macPictureH )
	{
		Rect			pictureBounds;

		CalcLocalFrameRect( pictureBounds );		
		::DrawPicture( macPictureH, &pictureBounds );
		
	}
}

Boolean LArrowControl::PointInHotSpot( Point inPoint, Int16 inHotSpot )
{
	Rect		localFrame;
	Int16		midPoint;
	
	CalcLocalFrameRect( localFrame );
	midPoint = ( localFrame.bottom - localFrame.top ) / 2;
	
	if ( inHotSpot == mTopHalf )
		localFrame.bottom = midPoint;
	else
		localFrame.top = midPoint;

	return PtInRect( inPoint, &localFrame );	
}

Int16 LArrowControl::FindHotSpot( Point inPoint )
{
	Rect		localFrame;
	Int16		midPoint;
	
	CalcLocalFrameRect( localFrame );
	midPoint = ( localFrame.bottom - localFrame.top ) / 2;
	
	if ( inPoint.v < midPoint )
		return mTopHalf;
	else	
		return mBottomHalf;
}

void LArrowControl::HotSpotAction( Int16 inHotSpot, Boolean inCurrInside, Boolean inPrevInside )
{
	
	if ( inCurrInside != inPrevInside )
	{
		FocusDraw();
		HiliteControl( inHotSpot, inCurrInside );
	}
	
	if ( inCurrInside )
	{
		if ( ( ::TickCount() - fLastTicks ) < fDelay )
			return;
	
		if ( fDelay == 0 )
			fDelay = 20;
				
		fHits++;
		if ( fHits > 6 )
			fDelay = 5;
		
		fLastTicks = TickCount();
		
		BroadcastMessage( mValueMessage, &inHotSpot );
	}
	else
	{
		fHits = 0;
		fDelay = 20;
	}
}

void LArrowControl::HotSpotResult( Int16 /*inHotSpot*/ )
{
	mCurrPict = mArrowsPict;
	
	DrawSelf();
	
	fHits = 0;
	fDelay = 0;
}

/*=============================================================================
	File & Stream Classes Utilities
=============================================================================*/

/*-----------------------------------------------------------------------------
	StOpenResFile opens the resource fork and closes it later
-----------------------------------------------------------------------------*/

StOpenResFile::StOpenResFile (LFile *file, short perms)
{
	fFile = file;
	file->OpenResourceFork (perms);
}

StOpenResFile::~StOpenResFile ()
{
	// LAM may not be required to UpdateResFile
	// CloseResFile is supposed to call UpdateResFile
	//::UpdateResFile (fFile->GetResourceForkRefNum());
	fFile->CloseResourceFork();
}

/*-----------------------------------------------------------------------------
	StUseResFile uses an open resource fork and stops using it later.
-----------------------------------------------------------------------------*/

StUseResFile::StUseResFile( short refnum )
{
	fPrevResFile = CurResFile();
	
	if (refnum != -1)
		UseResFile( refnum );
}

StUseResFile::~StUseResFile()
{
	UseResFile( fPrevResFile );
}

//========================================================================================
// CLASS CStringListRsrc
//========================================================================================

typedef short**	IntegerHandle;
//----------------------------------------------------------------------------------------
// CStringListRsrc::GetString: 
//----------------------------------------------------------------------------------------

void CStringListRsrc::GetString(short index, CStr255& theString) const
{
	if (fStrListID != 0)
		GetIndString(theString, fStrListID, index);
} // CStringListRsrc::GetString 

//----------------------------------------------------------------------------------------
// CStringListRsrc::FindString: 
//----------------------------------------------------------------------------------------

short CStringListRsrc::FindString(const CStr255& theString, Boolean addString)
{
	CStr255 testString;
	short numStrings = this->CountStrings();

	for (short i = 1; i <= numStrings; ++i)
	{
		this->GetString(i, testString);

		if (testString == theString)
			return i;								// found it! return the index
	}
	
	// CString is not found, should we add it?
	if (addString == kAddString)
		return this->AppendString(theString);		// added it! return the index

	// CString not found...sigh!
	return 0;
} // CStringListRsrc::FindString 

//----------------------------------------------------------------------------------------
// CStringListRsrc::AppendString: 
//----------------------------------------------------------------------------------------

short CStringListRsrc::AppendString(const CStr255& theString)
{
	long result;
	short totalStringLength = theString.Length() + kLengthByte;
	Handle aHandle = Get1Resource('STR#', fStrListID);
	if (aHandle)
	{
		if (!*aHandle)
			::LoadResource(aHandle);
		
		if (*aHandle)
		{
			SInt8 flags = ::HGetState(aHandle);
			::HNoPurge(aHandle);
			Size itsSize = GetHandleSize(aHandle);
			if (itsSize > 0)
			{
				short numStrings = this->CountStrings();
				SetHandleSize(aHandle, itsSize + totalStringLength);
				(**((IntegerHandle) aHandle)) = ++numStrings;				// increment the count of strings in the STR# resource

				// needs a failure handler
				result = Munger(aHandle, itsSize, NULL, totalStringLength, &theString, totalStringLength);
				if (result > 0)
				{
					// mark it as changed
					ChangedResource(aHandle);
					::HSetState(aHandle, flags);

					return numStrings;
				}
				::HSetState(aHandle, flags);
			}
		}
	}
	else
	{
		// must create the resource...
		aHandle = NewHandle(sizeof(short) + totalStringLength);
		(**((IntegerHandle) aHandle)) = 1;				// set the count of strings in the STR# resource
		result = Munger(aHandle, sizeof(short), NULL, totalStringLength, &theString, totalStringLength);
		if (result > 0)
		{
			AddResource(aHandle, 'STR#', fStrListID, fStrListRsrcName);
			ThrowIfResError_();	// if fails, it's most likely because the file is read-only

			SetResAttrs(aHandle, resPurgeable);
	
			// Need the following ChangedResource call because the SetResAttrs call on the previous
			// line cleared the "resChanged" attribute of the resource.
			ChangedResource(aHandle);
	
			return 1;
		}
	}
	return 0;	
} // CStringListRsrc::AppendString 

//----------------------------------------------------------------------------------------
// CStringListRsrc::ClearAll: 
//----------------------------------------------------------------------------------------

void CStringListRsrc::ClearAll()
{
	Handle aHandle = Get1Resource('STR#', fStrListID);
	if (aHandle)
	{
		if (!*aHandle)
			::LoadResource(aHandle);
		
		if (*aHandle)
		{
			SInt8 flags = ::HGetState(aHandle);
			::HNoPurge(aHandle);
			SetHandleSize(aHandle, sizeof(short));
			(**((IntegerHandle) aHandle)) = 0;				// set the count of strings in the STR# resource

			// mark it as changed
			ChangedResource(aHandle);
			::HSetState(aHandle, flags);
		}
	}
} // CStringListRsrc::ClearAll 

//----------------------------------------------------------------------------------------
// CStringListRsrc::CountStrings: 
//----------------------------------------------------------------------------------------

short CStringListRsrc::CountStrings() const
{
	// return the leading integer in the STR# resource:
	//
	// type 'STR#' {
	//      integer = $$Countof(StringArray);
	//      array StringArray {
	//              pstring;
	//      };
	// };

	Handle aHandle = Get1Resource('STR#', fStrListID);
	if (aHandle)
	{
		if (!*aHandle)
			::LoadResource(aHandle);
		
		if (*aHandle)
			return **((IntegerHandle) aHandle);
	}
	return 0;
} // CStringListRsrc::CountStrings 

//----------------------------------------------------------------------------------------
// CStringListRsrc::GetListName: 
//----------------------------------------------------------------------------------------

void CStringListRsrc::GetListName(CStr255& theString)
{
	// Do we have the name in our field?
	if ( fStrListRsrcName.Length () > 0 )
		theString = fStrListRsrcName;
	else			//	We don't have the name so try and get it from the resource
	{
		short	itsRsrcID;
		CStr255	itsName;
		ResType	itsType;

		// Get the resource's handle
		Handle aHandle = Get1Resource('STR#', fStrListID);
		if (aHandle)
		{
			// Extract the resource info from the handle
			GetResInfo ( aHandle, &itsRsrcID, &itsType, itsName );
			if ( itsName.Length () > 0 )
			{
				theString = itsName;
				
				// Store the name in our field
				fStrListRsrcName = theString;
			}
			
			ReleaseResource ( aHandle );
		}
		else		// Sorry we don't have anything so return an empty CString
			theString = CStr255::sEmptyString;
	
	}
} // CStringListRsrc::GetListName 

//----------------------------------------------------------------------------------------
// CStringListRsrc::RemoveAt: 
//----------------------------------------------------------------------------------------

void CStringListRsrc::RemoveAt(short index)
{
	if ((index > 0) && (index <= this->CountStrings()))
	{
		Handle aHandle = Get1Resource('STR#', fStrListID);
		if (aHandle)
		{
			if (!*aHandle)
				::LoadResource(aHandle);
			
			if (*aHandle)
			{
				SInt8 flags = ::HGetState(aHandle);
				::HNoPurge(aHandle);
				
				Size originalSize = GetHandleSize(aHandle);

				// determine the accumulated size
				Size accumulatedSize = sizeof(short);	// the accumulated size is the size of the count
				for (short i = 1; i < index; ++i)
					accumulatedSize += (*((CStringPtr) (*aHandle + accumulatedSize))).Length() + kLengthByte;
				
				// determine the actual CString size
				Size stringSize = (*((CStringPtr) (*aHandle + accumulatedSize))).Length() + kLengthByte;

				// move the the data in the handle
				BlockMoveData(*aHandle + accumulatedSize + stringSize, *aHandle + accumulatedSize,
								originalSize - accumulatedSize - stringSize);

				// trim the handle down to its new size
				SetHandleSize(aHandle, originalSize - stringSize);

				// decrement count of strings in STR# resource
				--(**((IntegerHandle) aHandle));

				// mark it as changed
				ChangedResource(aHandle);
				
				::HSetState(aHandle, flags);
			}
#if qDebugMsg
			else
				fprintf(stderr, "CStringListRsrc::RemoveAt can't load STR# = %d.\n", fStrListID);
#endif
		}
#if qDebugMsg
		else
			fprintf(stderr, "CStringListRsrc::RemoveAt can't find STR# = %d.\n", fStrListID);
#endif
	}
#if qDebugMsg
	else
		fprintf(stderr, "CStringListRsrc::RemoveAt there's no CString at %d to remove!\n", index);
#endif
} // CStringListRsrc::RemoveAt 

//----------------------------------------------------------------------------------------
// CStringListRsrc::ReplaceAt: 
//----------------------------------------------------------------------------------------

void CStringListRsrc::ReplaceAt(const CStr255& theString, short index)
{
	if ((index > 0) && (index <= this->CountStrings()))
	{
		Handle aHandle = Get1Resource('STR#', fStrListID);
		if (aHandle)
		{
			if (!*aHandle)
				::LoadResource(aHandle);
			
			if (*aHandle)
			{
				SInt8 flags = ::HGetState(aHandle);
				::HNoPurge(aHandle);
				
				Size newStringSize = theString.Length() + kLengthByte;	// the actual CString size
				Size originalSize = GetHandleSize(aHandle);

				// determine the accumulated size of the valid portion of the resource
				Size accumulatedSize = sizeof(short);	// the accumulated size is the size of the count
				for (short i = 1; i < index; ++i)
					accumulatedSize += (*((CStringPtr) (*aHandle + accumulatedSize))).Length() + kLengthByte;
				
				// determine the actual CString size
				Size oldStringSize = (*((CStringPtr) (*aHandle + accumulatedSize))).Length() + kLengthByte;
				
				// determine the new handle size
				Size newHandleSize = originalSize + (newStringSize - oldStringSize);

				// determine whether to grow or shrink the handle?
				if (newStringSize > oldStringSize)
				{
					// grow the handle
					SetHandleSize(aHandle, newHandleSize);

					// move the bytes
					BlockMoveData(*aHandle + accumulatedSize + oldStringSize, *aHandle + accumulatedSize + newStringSize,
									originalSize - accumulatedSize - oldStringSize);
				}
				else if (oldStringSize > newStringSize)
				{
					// move the bytes
					BlockMoveData(*aHandle + accumulatedSize + oldStringSize, *aHandle + accumulatedSize + newStringSize,
									originalSize - accumulatedSize - oldStringSize);

					// shrink the handle
					SetHandleSize(aHandle, newHandleSize);
				}

				// assign the CString
				*((CStr255*) (*aHandle + accumulatedSize)) = theString;
				
				// mark it as changed
				ChangedResource(aHandle);
				
				::HSetState(aHandle, flags);
			}
#if qDebugMsg
			else
				fprintf(stderr, "CStringListRsrc::ReplaceAt can't load STR# = %d.\n", fStrListID);
#endif
		}
#if qDebugMsg
		else
			fprintf(stderr, "CStringListRsrc::ReplaceAt can't find STR# = %d.\n", fStrListID);
#endif
	}
#if qDebugMsg
	else
		fprintf(stderr, "CStringListRsrc::ReplaceAt there's no CString at %d to replace!\n", index);
#endif
} // CStringListRsrc::ReplaceAt 




StWatchCursor::StWatchCursor()	{ ::SetCursor( (CursPtr)*(::GetCursor(watchCursor)) ); }
StWatchCursor::~StWatchCursor()	{ ::SetCursor( &(UQDGlobals::GetQDGlobals()->arrow) ); }

//-----------------------------------
void StringParamText(
	CStr255& ioFormatString,
	SInt32 inReplaceNumber0,
	SInt32 inReplaceNumber1,
	SInt32 inReplaceNumber2,
	SInt32 inReplaceNumber3)
//	Replace the characters "^0" ..."^3"within ioString with numerals. 
//	If "^0" or "^1" is not found in ioString, the method does nothing.
//-----------------------------------
{
	char t0[15], t1[15], t2[15], t3[15];
	sprintf(t0, "%ld", inReplaceNumber0);
	sprintf(t1, "%ld", inReplaceNumber1);
	sprintf(t2, "%ld", inReplaceNumber2);
	sprintf(t3, "%ld", inReplaceNumber3);
	::StringParamText(ioFormatString, t0, t1, t2, t3);
}

//-----------------------------------
void StringParamText(
	CStr255& ioFormatString,
	const char* inReplaceText0,
	const char* inReplaceText1,
	const char* inReplaceText2,
	const char* inReplaceText3)
//	Replace the substrings "^0" ..."^3" within ioString with inReplaceText0 ... inReplaceText3. 
//	(substring is just removed if inReplaceTextn is null). 
//	If any of the substrings "^0"..."^3" is not found in ioString, nothing happens there.
//-----------------------------------
{
	LStr255 resultString = (ConstStringPtr)&ioFormatString;
	char target[] = "^0";
	const char* replaceString[] = {
					inReplaceText0,
					inReplaceText1,
					inReplaceText2,
					inReplaceText3 };
	for (int i = 0; i < 4; i++)
	{
		UInt8 position = resultString.Find(target, 1);
		if ( position )
		{
			const char* rs = replaceString[i];
			if ( rs )
				resultString.Replace(position, 2, rs, strlen(rs));
			else
				resultString.Remove(position, 2);
		}
		target[1]++; // morph it into "^1" etc.  ASCII standard supports this.
	}
	ioFormatString = (ConstStringPtr)resultString;
}

// This code is taken from June 97 Dev.CD
enum
{
	kPromisedFlavor				= 'fssP',
	kPromisedFlavorFindFile		= 'rWm1'
};

static Size MinimumBytesForFSSpec (const FSSpec *fss)
{
	//
	//	Some senders don't bother sending the unused bytes of the
	//	file name. This function helps make sure the FSSpec is
	//	minimally sane by computing the minimum number of bytes it
	//	would take to store it.
	//
	//	THIS FUNCTION CANNOT MOVE MEMORY.
	//

	return sizeof (*fss) - sizeof (fss->name) + *(fss->name) + 1;
}

OSErr GetHFSFlavorFromPromise
    (DragReference dragRef, ItemReference itemRef,
        HFSFlavor *hfs, Boolean isSupposedlyFromFindFile)
{
    OSErr             err = noErr;
    PromiseHFSFlavor  phfs;
    Size              size = sizeof (phfs);

    err = GetFlavorData
        (dragRef,itemRef,flavorTypePromiseHFS,&phfs,&size,0);

    if (!err)
    {
        if (size != sizeof (phfs))
            err = cantGetFlavorErr;
        else
        {
            Boolean isFromFindFile =
                phfs.promisedFlavor == kPromisedFlavorFindFile;

            if (isSupposedlyFromFindFile != isFromFindFile)
                err = paramErr;
            else
            {
                size = sizeof (hfs->fileSpec);
                err = GetFlavorData
                    (dragRef,itemRef,phfs.promisedFlavor,
                        &(hfs->fileSpec),&size,0);

                if (!err)
                {
                    Size minSize = MinimumBytesForFSSpec
                        (&(hfs->fileSpec));

                    if (size < minSize)
                        err = cantGetFlavorErr;
                    else
                    {
                        hfs->fileType     = phfs.fileType;
                        hfs->fileCreator  = phfs.fileCreator;
                        hfs->fdFlags      = phfs.fdFlags;
                    }
                }
            }
        }
    }
    return err;
}
// End of cut and paste

void GetDesktopIconSuiteFor( OSType inFileCreator, OSType inFileType, short inSize, Handle** ioHandle )
{
	Assert_( inSize == kLargeIcon || inSize == kSmallIcon  );

	Handle* h = *ioHandle;
	
	if (inFileCreator && inFileType )
	{
		Handle handle;
		OSErr err = ::NewIconSuite(h);
		short i = 0;
		if (!err && *h)
		{
			for ( i = 0; i< 3; i++ )
			{
					err = ::DTGetIcon(nil, 0, inSize , inFileCreator, inFileType, &handle);
				
					if (!err)
						err = ::AddIconToSuite(handle, *h, ::DTIconToResIcon( inSize  ) );
					inSize++;
					if( err )
						break;
			}
			if( !err && i == 0 ) // Didn't find at least the B&W icon
			{
				::DisposeIconSuite(*h, true);
				*h = nil;
			}					
		}
	}
	if (!*h) 
	{
		::GetIconSuite(h, 133, 
			inSize == kLargeIcon ?	kSelectorAllLargeData : kSelectorAllSmallData );
	}
}

StSpinningBeachBallCursor::StSpinningBeachBallCursor ()
{
	startAsyncCursors();
}

StSpinningBeachBallCursor::~StSpinningBeachBallCursor()
{
	stopAsyncCursors();
}
