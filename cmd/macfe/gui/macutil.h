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

/*
	Random Utilities
*/
#pragma once

#include <Drag.h>
#include <LCaption.h>
#include <LControl.h>
#include <LPane.h>
#include <LGAPopup.h>
#include <Processes.h>

#include "PascalString.h"
#include "CWindowMediator.h"
#include "CSimpleTextView.h"

class LTextEngine;
class LStdPopupMenu;
class LWindow;

#define		MOUSE_HYSTERESIS		6
#define		MOUSE_DRAGGING			1
#define		MOUSE_TIMEOUT			2
#define		MOUSE_UP_EARLY			3

typedef struct LO_ImageStruct_struct LO_ImageStruct;
typedef union LO_Element_struct LO_Element;
typedef struct lo_FormElementSelectData_struct lo_FormElementSelectData;

// Layout Utilities

Boolean TooDifferent( const Point& a, const Point& b );

short WaitForMouseAction( const Point& initial, long clickStart, long delay );

SPoint32 LocalToImagePoint( LView* view, const Point& local );

void RevealInImage( LView* view, SPoint32 selTopLeft, SPoint32 selBotRight );

int CalcCurrentSelected( lo_FormElementSelectData* selectData, Boolean fromDefaults );

// Utilities

void DrawBorder( const Rect& frame, Boolean topleft, int size );

Rect CenterInRect( Point thing, Rect destSpace );

/*-----------------------------------------------------------------------------
	Panes/Views Coordinate Translation
-----------------------------------------------------------------------------*/
inline Rect PortToLocalRect( LView* view, const Rect& portRect )
{
	Rect		localRect;
	localRect = portRect;
	view->PortToLocalPoint( topLeft( localRect ) );
	view->PortToLocalPoint( botRight( localRect ) );
	return localRect;
}

inline Rect LocalToPortRect( LPane* self, const Rect& localRect )
{
	Rect		portRect;
	portRect = localRect;
	self->LocalToPortPoint( topLeft( portRect ) );
	self->LocalToPortPoint( botRight( portRect ) );
	return portRect;
}

inline Rect PortToGlobalRect( LPane* self, const Rect& portRect )
{
	Rect		globalRect;
	globalRect = portRect;
	self->PortToGlobalPoint( topLeft( globalRect ) );
	self->PortToGlobalPoint( botRight( globalRect ) );
	return globalRect;
}

inline Rect GlobalToPortRect( LPane* self, const Rect& globalRect )
{
	Rect		portRect;
	portRect = globalRect;
	self->GlobalToPortPoint( topLeft( portRect ) );
	self->GlobalToPortPoint( botRight( portRect ) );
	return portRect;
}

void StringParamText(CStr255& ioFormatString, // containing ^0, ^1, etc
	const char* ioReplaceText0,
	const char* inReplaceText1 = nil,
	const char* inReplaceText2 = nil,
	const char* inReplaceText3 = nil
	);

void StringParamText(CStr255& ioFormatString,
	SInt32 inReplaceNumber0,
	SInt32 inReplaceNumber1,
	SInt32 inReplaceNumber2,
	SInt32 inReplaceNumber3
	); // as above, but calls ::NumToString on the parameters first.

/*-----------------------------------------------------------------------------
	String Copying
	
	CopyString will copy strings. The format is (destination, source) which is
	backwards from the way many functions have it but happens to be the same
	order as operator= does it. IE since most copies look like this...
		foo = bar
	...I wanted the string assignment/copy to look the same.
		CopyString (foo, bar)
	Also it makes converting between CString-using and Str255-using code easy.
	
	This stuff is overloaded for all pascal/c combinations.
-----------------------------------------------------------------------------*/
const short kCommandKey = 55;
const short kCtlKey = 0x3B;
const short kOptionKey = 58;
const short kShiftKey = 56;

unsigned char* CopyString( unsigned char* destination, const unsigned char* source );
unsigned char* CopyString( unsigned char* destination, const char* source );
char* CopyString( char* destination, const unsigned char* source );
char* CopyString( char* destination, const char* source );

// ¥ also frees the original if not NULL
void CopyAlloc( char** destination, StringPtr source );

// ¥Êtakes colons out of a string, useful for saving files
void StripColons( CStr31& fileName );

// ¥Êstrips the given character from a string; if it
// appears twice in a row the second char isn't removed
void StripChar( CStr255& str, char ch );

// ¥ Takes a string of text (dragged/pasted), and turns
// it into a URL (strips CR, whitespace preceeding/trailing <>
void CleanUpTextToURL(char* text);

// ¥ strips spaces & returns at end of url (happens often
void CleanUpLocationString( CStr255& url );

// ¥ TRUE if the key is down. Bypasses the event loop
Boolean IsThisKeyDown( const UInt8 theKey );

// ¥ CmdPeriod -  returns true if cmd-. is pressed
Boolean CmdPeriod();

// ¥Êenable/disable pane and refresh it
void SetEnable( LPane* button, Boolean enable );

// ¥ changes the string to something acceptable to mac menu manager
void CreateMenuString( CStr255& itemName );

// ¥ converts TextEdit handle to text handle that has hard carriage returns. 
Handle TextHandleToHardLineBreaks(CSimpleTextView &inTextView);

// ¥Êsets the number of entries in popup to be shouldBe
void SetMenuSize( LStdPopupMenu* popup, short shouldBe );
void SetMenuSizeForLGAPopup( LGAPopup* popup, short shouldBe );

void SetMenuItem( CommandT whichItem, Boolean toState );
// ¥Êconstrain value to be between min and max
void ConstrainTo( const long& min, const long& max, long& value );

// ¥ makes copy of the struct. Throws on failure
void * StructCopy(const void * struc, UInt32 size);

// ¥ return the free space available on the volume referenced by vRefNum in bytes
unsigned long GetFreeSpaceInBytes( short vRefNum );

// ¥Êsets the std poup to the named item
Boolean SetPopupToNamedItem( LStdPopupMenu* whichMenu, const CStr255& itemText );
Boolean SetLGAPopupToNamedItem( LGAPopup* whichMenu, const CStr255& itemText );

// ¥
void TurnOn( LControl* control );

// ¥
void myStringToNum( const CStr255& inString, Int32* outNum );

// ¥Êsets the captions descriptor to newText
void SetCaptionDescriptor( LCaption* captionToSet, const CStr255& newText,
	const TruncCode	truncWhere );

// ¥ get the descriptor as char *
char* GetPaneDescriptor( LPane* pane );

// ¥ Use to display the NetHelp window, given the appropriate help topic
void ShowHelp ( const char* inHelpTopic );

// ¥ force the window onto the desktop
void ForceWindowOnScreen( LWindow* window );

// ¥ grow the window to the height of the screen it is on
void GrowWindowToScreenHeight( LWindow* win );

// ¥Êwill all of the window be visible?
Boolean WillBeVisible( const Rect& windowRect );

// ¥ stagger the window
void StaggerWindow( LWindow* win );

// ¥ is the window fully on a single device?
GDHandle WindowOnSingleDevice( LWindow* win );

// ¥ Saving/restoring of the window's default state.
Boolean RestoreDefaultWindowState( LWindow* win );
void StoreDefaultWindowState( LWindow* win );

// ¥ Is front window modal? Used for command enabling
Boolean IsFrontWindowModal();

// ¥ÊFetch a window title resource and fill in the current profile name
void GetUserWindowTitle(short id, CStr255& title);

// ¥Êreturns the number of pixels (which is inPoints / 120 * iRes)
short PointsToPixels( short inPoints, short iRes );

// ¥ returns true if we are the front application
Boolean IsFrontApplication();

// Makes Netscape the frontmost application
void MakeFrontApplication();

void FrameSubpaneSubtle( LView* view, LPane* pane );

// ¥Êgets the are of window occupied by pane as a rgn
void GetSubpaneRgn( LView* view, LPane* pane, RgnHandle rgn );

void GetSubpaneRect( LView* view, LPane* pane, Rect& frame );

void WriteVersionTag( LStream* stream, Int32 version );

Boolean ReadVersionTag( LStream* stream, Int32 version );

// ¥ tries to set a cursor
void TrySetCursor( int whichCursor );

void StripNewline( CStr255& msg );
void StripDoubleCRs( CStr255& msg );
// ¥ Converts LF to CR in strings (used to cleanup Netlib stuff). Returns the number of conversions
int ConvertCRtoLF( CStr255& msg );

// ¥ Gets font info in a safe way
FontInfo SafeGetFontInfo(ResIDT textTraits);

void DrawAntsRect( Rect&r, short mode );
Rect RectFromTwoPoints( Point p1, Point p2 );
Boolean RectInRect(const Rect *inCheckRect, const Rect *inEnclosingRect);

void GetDateTimeString( CStr255& dateTime );

void FrameButton( const Rect& box, Boolean pushed );
void HiliteRect( const Rect& r );
void HiliteRgn( const RgnHandle& r );
LPane* FindPaneHitBy( const Point& globalMouse );


//		Good 'ol Mac File Utilities
#if 0
StringPtr	PathnameFromWD (short VRef, StringPtr s);
StringPtr	PathnameFromDirID (long DirID, short VRef, StringPtr s);
#endif

char*		PathURLFromProcessSignature (OSType sig, OSType type);
	
// This code is taken from June 97 Dev.CD
OSErr GetHFSFlavorFromPromise
    (DragReference dragRef, ItemReference itemRef,
        HFSFlavor *hfs, Boolean isSupposedlyFromFindFile);

// ¥ given a process signature, gets its serial number
ProcessSerialNumber GetPSNBySig( OSType sig );
// ¥ creates an NoProcess serial number
ProcessSerialNumber MakeNoProcessPSN();
// ¥ do we have a process
Boolean HasProcess( const ProcessSerialNumber& psn );
// ¥ given an application signature, finds the process if it is running. From MacApp
OSErr FindProcessBySignature( OSType sig, OSType processType, 
					ProcessSerialNumber& psn, FSSpec* fileSpec );
// ¥ creates an apple event 
OSErr CreateAppleEvent( OSType suite, OSType id, AppleEvent &event, ProcessSerialNumber targetPSN );
// ¥ do we have an error reply in this event (noErr means that there is no error)
OSErr EventHasErrorReply( AppleEvent& event );
// Drag'n'drop

// finds where the file goes in drag to Finder
OSErr GetDropLocationDirectory (AEDesc *dropLocation, long *directoryID, short *volumeID);
// Did we drag to the trash
Boolean DragTargetWasTrash (DragReference dragRef);

#define		mTopHalf		1
#define		mBottomHalf		2
#define		mArrowsPict		-4047
#define		mArrowsTopLit	-4046
#define		mArrowsBotLit	-4045
class LArrowControl: public LControl
{
public:
							LArrowControl( const SPaneInfo& inPaneInfo, MessageT valueMessage );
	void					HotSpotAction( Int16 inHotSpot, Boolean inCurrInside, Boolean inPrevInside );
	void					HiliteControl( Int16 inHotSpot, Boolean inCurrInside );
	Boolean					PointInHotSpot( Point inPoint, Int16 inHotSpot );
	Int16					FindHotSpot( Point inPoint );
	void					HotSpotResult( Int16 inHotSpot );

	void					DrawSelf();
protected:
	Int16					mCurrPict;
	static long				fLastTicks;
	static long				fDelay;
	static long				fHits;
};

#if 0
/******************************************************************************
 * class CWindowIterator.
 * Iterates over all the windows
 *****************************************************************************/
class CWindowIterator
{
public:
				CWindowIterator( OSType sig );	// If '****', iterates over all windows
	
	Boolean		Next( LWindow*& outWindow );
protected:
	virtual Boolean		DoThisWindow( const LWindow* window );
	
	WindowPtr	fNextWindow;
	OSType		fSig;
};

class CHyperWin;

/******************************************************************************
 * class CHyperIterator.
 * Legacy class that iterates over all the hyperwindows
 *****************************************************************************/
class CHyperIterator: public CWindowIterator
{
public:
				CHyperIterator();

	Boolean		Next( CHyperWin*& outWindow );
};

class CHyperMailNewsIterator: public CHyperIterator
{
public:
				CHyperMailNewsIterator();
protected:
	virtual Boolean		DoThisWindow( const LWindow* window );
};

#endif

//----------------------------------------------------------------------------------------
// CStringListRsrc: A simple class for managing and accessing strings in STR# resources.
//
// Limitations: this class has no notion of which resource file to be used for accessing
// the strings from the CString list resource.  This could be added as an enhancement.
//----------------------------------------------------------------------------------------

class CStringListRsrc {

protected:
	short		fStrListID;					// The ID of the STR# rsrc.
	
	CStr255		fStrListRsrcName;			// The name of the STR# rsrc, this is
											// only used when adding the STR# rsrc.

public:
	//----------------------------------------------------------------------------------------
	// class-scoped constants
	//----------------------------------------------------------------------------------------
	enum { kDontAddString, kAddString };

	//----------------------------------------------------------------------------------------
	// constructors
	//----------------------------------------------------------------------------------------
	CStringListRsrc(short strListID, const CStr255& strListRsrcName) :
			fStrListID(strListID),
			fStrListRsrcName(strListRsrcName)
			{}
		// inline constructor

	CStringListRsrc(short strListID) :
			fStrListID(strListID)
			{ fStrListRsrcName = CStr255::sEmptyString; }
		// inline constructor

	//----------------------------------------------------------------------------------------
	// accessors
	//----------------------------------------------------------------------------------------
	short AppendString(const CStr255& theString);
		// append theString to the STR# resource whose id is fStrListID and return its index
	
	void ClearAll();
		// clears all strings in the STR# resource

	short CountStrings() const;
		// returns the number of strings in the STR# resource whose id is fStrListID

	short FindString(const CStr255& theString, Boolean addString = kDontAddString);
		// find theString in the STR# resource whose id is fStrListID and return its index
		// if theString is not found in the STR# resource, add it if addString is kAddString
	
	void GetListName(CStr255& itsName);
		//	Returns the name of the STR# list either from its field or if that is empty
		//	from the actual resource

	void GetString(short index, CStr255& theString) const;
		// return in theString the "index" CString in the STR# resource whose id is fStrListID

	void RemoveAt(short index);
		// removes the CString at the specified index.
	
	void ReplaceAt(const CStr255& theString, short index);
		// replace the CString at "index" with "theString"
};

/*-----------------------------------------------------------------------------
	StOpenResFile opens the resource fork and closes it later
-----------------------------------------------------------------------------*/

class StOpenResFile
{
public:
					StOpenResFile( LFile* file, short perms );
					~StOpenResFile ();
	operator		short () { return fFile->GetResourceForkRefNum(); }
protected:
	LFile*			fFile;
};

/*-----------------------------------------------------------------------------
	StUseResFile uses an open resource fork and stops using it later.
-----------------------------------------------------------------------------*/

class StUseResFile
{
public:
					StUseResFile( short refnum );
					~StUseResFile();
protected:
	short			fPrevResFile;
};



/* sets to watch on construction, sets to arrow on destruction */
class StWatchCursor
{
public:
	StWatchCursor();
	~StWatchCursor();
};

/* sets to beachball on construction, spin asynchronously, sets to arrow on destruction */
class StSpinningBeachBallCursor
{
public:
	StSpinningBeachBallCursor();
	~StSpinningBeachBallCursor();
};
// Routine for making an icon suite for a file given the creator, type and wether you want
// Large,small/. Note that these are DTIcon Types
void GetDesktopIconSuiteFor( OSType inFileCreator, OSType inFileType, short inSize, Handle** ioHandle );
