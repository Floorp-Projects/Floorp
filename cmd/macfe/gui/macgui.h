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

#pragma once
/*
	GUI Library
	Classes for customizing the GUI of a program. There are subclasses of
	various PowerPlant classes in lieu of a more dynamic mechanism
	CS
	Color Scheme Library
*/
#include <LTextEdit.h>
#include <LGAEditField.h>
#include "CFontReference.h"
//#include "VEditField.h"
#include "CTSMEditField.h"
#include "uprefd.h"
#include "xp_hash.h"
#include "lo_ele.h"
#include <LPicture.h>

typedef struct LO_TextAttr_struct LO_TextAttr;

/*******************************************************************************
 * StPrepareForDialog is a simple class that ensures application is in the
 * foreground, and desktop deactivated.
 * Insert into functions that will open Toolbox dialog boxes.
 * Used only for side-effects
 *******************************************************************************/
class StPrepareForDialog
{
public:
	StPrepareForDialog();
	~StPrepareForDialog();
};


/*-----------------------------------------------------------------------------
	UGraphics is full of little graphics utilities
-----------------------------------------------------------------------------*/
class UGraphics
{
public:

	// Note, the numbering of these enums is reflected in pref.r
	// Be sure to reflect changes there as well
	static void 	Initialize();
	
	static const RGBColor&	Get( CPrefs::PrefEnum r );
	static void 			SetFore( CPrefs::PrefEnum r );
	static void 			SetBack( CPrefs::PrefEnum r );
	static inline void		SetIfColor( const RGBColor& color ); // Use instead of RGBForeColor
	static inline void		SetIfBkColor( const RGBColor& color );	// Use instead of RGBBackColor -- optimized
	static inline Boolean	EqualColor( const RGBColor& c1, const RGBColor& c2 );
	static inline RGBColor	MakeRGBColor( UInt8 red, UInt8 green, UInt8 blue );
	static inline RGBColor	MakeRGBColor( LO_Color color );
	static inline LO_Color	MakeLOColor(const RGBColor& color);
	static void 			VertLine( int x, int top, int height, CPrefs::PrefEnum r );
	static void 			HorizLineAtWidth( int vertical, int left, int width, 
								CPrefs::PrefEnum r );
	static CGrafPtr			IsColorPort( GrafPtr port );			// TRUE if GrafPtr is a color port
	static void				SetWindowColors( LWindow *window );
	static void				SetWindowColor( GrafPort *window, short field, const RGBColor& color );


	static void				DrawLine( Int16 top, Int16 left, Int16 bottom, Int16 right,
								CPrefs::PrefEnum color );
	static void				FrameRectMotif( const Rect& box, Boolean inset );
	static void				FrameRectSubtle (const Rect& box, Boolean inset);
	static void				FrameRectBevel (const Rect& box, Boolean inset, const RGBColor &lighter, const RGBColor &darker );
	static void				FrameTopLeftShaded( const Rect& box, short width, const RGBColor& curr );
	static void				FrameRectShaded( const Rect& box, Boolean inset );
	static void				FrameEraseRect(const Rect& box, short width, Boolean focussed);	// Erases rect in background patterns
	static void				DarkenRect(const Rect & box);
	// Used in pane calculations
	static Boolean			PointInBigRect( SPoint32 rectLoc, SDimension16 rectSize, SPoint32 loc );
	static void				FrameCircleShaded( const Rect& box, Boolean inset );
	
	static RgnHandle		GetTempRegion();					// Get a region for a while
	static void				ReleaseTempRegion(RgnHandle rgn);	// Return the region to the pile

private:
	enum { kRegionCacheSize = 5 };								// If you change this constant, change the static array initializer in macgui.c!
	static RgnHandle		sTempRegions[kRegionCacheSize];		// Cached regions for use by UGraphics:GetTempRegion
	static Boolean			sRegionsUsed[kRegionCacheSize];		// Which cached regions are in use?
};

inline Boolean UGraphics::EqualColor( const RGBColor& c1, const RGBColor& c2 )
{
	return ((c1.red == c2.red) && (c1.green == c2.green) && (c1.blue == c2.blue));
}

inline void UGraphics::SetIfColor( const RGBColor& newColor )
{
	RGBColor currentColor = ((CGrafPtr)qd.thePort)->rgbFgColor;
	if ( !EqualColor( currentColor, newColor ) )
		::RGBForeColor( &newColor );
}

inline void UGraphics::SetIfBkColor( const RGBColor& newColor )
{
	CGrafPtr cgp = (CGrafPtr)qd.thePort;
	RGBColor currentColor = cgp->rgbBkColor;
	if ( !EqualColor( currentColor, newColor ) )
		::RGBBackColor( &newColor );
}

inline RGBColor UGraphics::MakeRGBColor( UInt8 red, UInt8 green, UInt8 blue )
{
	RGBColor		temp;
	temp.red	= (red   == 255 ? 65535 : (((unsigned short)red) << 8));
	temp.green	= (green == 255 ? 65535 : (((unsigned short)green) << 8));
	temp.blue	= (blue  == 255 ? 65535 : (((unsigned short)blue) << 8));
	return temp;
}

inline RGBColor UGraphics::MakeRGBColor( LO_Color color )
{
	RGBColor		temp;
	temp.red	= (color.red   == 255 ? 65535 : (((unsigned short)color.red) << 8));
	temp.green	= (color.green == 255 ? 65535 : (((unsigned short)color.green) << 8));
	temp.blue	= (color.blue  == 255 ? 65535 : (((unsigned short)color.blue) << 8));
	return temp;
}

inline LO_Color UGraphics::MakeLOColor(const RGBColor& color )
{
	LO_Color		temp;
	temp.red	= (((unsigned short)color.red) >> 8);
	temp.green	= (((unsigned short)color.green) >> 8);
	temp.blue	= (((unsigned short)color.blue) >> 8);
	return temp;
}


/*-----------------------------------------------------------------------------
	HyperStyle is a Mac-oriented style sheet. It is produced once we've
	done all the mapping from HTML. It keeps things in native format, so
	even though Color comes from a restricted set, it's in here as RGB,
	and Strikethrough, logically a style, is kept out of the style field.
	The goal is to make setting text attributes fast. Creating them can
	be done via a caching table.
-----------------------------------------------------------------------------*/
class HyperStyle
{
public:
	static void		InitHyperStyle();
	static void		FinishHyperStyle();
/*
					HyperStyle( HyperStyle * s );
*/
					HyperStyle( MWContext *context,
							const CCharSet* charSet, 
							LO_TextAttr* attr,
							Boolean onlyMeasuring = FALSE,
							LO_TextStruct* element = NULL );
/*
					~HyperStyle() { if (fFontReference) delete fFontReference; }
*/

	Rect			InvalBackground( Point where, char* text,
							int start,
							int end, Boolean blinks );
							
	inline void 	Apply();
	void			GetFontInfo();

					// draw text using these attributes
	void			DrawText(	Point where,
								char* text,
								int start,
								int end );

	short			TextWidth(	char* text, 
								int firstByte, 
								int byteCount);
	
	// Actual style elements
	CFontReference	*fFontReference;
	RGBColor		rgbFgColor;
	RGBColor		rgbBkColor;
	Boolean			strike;
	Int32			fInlineInput;
	LO_TextStruct*	fElement;
	Boolean			fOnlyMeasuring;

// Font info hashing stuff
	FontInfo		fFontInfo;
private:
	LO_TextAttr		fAttr;
		
		// save/restore the pen and text state to be a good neighbor
	StColorPenState fSavedPen;
	StTextState		fSavedText;

	static XP_HashTable	sFontHash;
	static Boolean	sUnderlineLinks;
	static int		SetUnderlineLinks(const char * newpref, void * stuff);
	static uint32	StyleHash( const void* hyperStyle );
	static int		StyleCompFunction( const void* style1, const void* style2 );
};

inline void HyperStyle::Apply()
{
	fFontReference->Apply();
	UGraphics::SetIfColor( rgbFgColor );
	UGraphics::SetIfBkColor( rgbBkColor );
}



/*****************************************************************************
 * class CCoolBackground
 * Sets up the background color for drawing chrome in the main window
 * It is either gray, or utility pattern, depending upon your preferences
 ****************************************************************************/
class CCoolBackground
{
private:
	static PixPatHandle sPixPat;
	PixPatHandle fOldPixPat;
public:
	CCoolBackground();
	virtual ~CCoolBackground();
};

const MessageT msg_EditField = 2002;	// Edit field has been modified

/*****************************************************************************
 * class CEditBroadcaster
 * CTSMEditField that notifies listeners about its changes
 *****************************************************************************/
class CEditBroadcaster	: public CTSMEditField, public LBroadcaster
{
public:

	// ¥¥ Constructors/destructors
	enum { class_ID = 'ebro' };
	CEditBroadcaster(LStream *inStream);

	// ¥¥ Misc
	// Broadcast
	void UserChangedText();
};


#if 0
/*****************************************************************************
 * class CEditBroadcaster
 * LGAEditField that notifies listeners about its changes
 *****************************************************************************/
class CEditBroadcaster	: public LEditField
{
public:

	// ¥¥ Constructors/destructors
	enum { class_ID = 'Gebr' };
	CEditBroadcaster(LStream *inStream);

	// ¥¥ Misc
	// Broadcast
	void BroadcastValueMessage();
	void UserChangedText();
};
#endif


/*****************************************************************************
 * class CTextEdit
 * LTextEdit, keeps track if it has been modified
 *****************************************************************************/
class CTextEdit	: public LTextEdit, public LBroadcaster
{
	Boolean fModified;		// Has user modified this field
public:

	// ¥¥ Constructors/destructors
	enum { class_ID = 'etxt' };
	CTextEdit(LStream *inStream);
	
	// ¥¥ Misc
	Boolean	IsModified(){return fModified;};
	virtual void UserChangedText();
};

/*****************************************************************************
 * class CResPicture
 * LPicture with an associated resfile ID
 *****************************************************************************/
class CResPicture : public LPicture
{
public:
	enum { class_ID = 'rpic' };
					CResPicture(LStream *inStream);
					
	virtual void	DrawSelf();
	void			SetResFileID(short id) { mResFileID = id; };

private:
	Int16			mResFileID;
};

/******************************************************************************
 * class StTempClipRgn
 *	creates a temporary clip region. Old region is restored on exit
 * CANNOT BE NESTED
 *****************************************************************************/
class StTempClipRgn
{
	static RgnHandle sOldClip;	// Saved clip	
	static RgnHandle sMergedNewClip;	// New clip regions	
public:
	StTempClipRgn (RgnHandle newClip);
	~StTempClipRgn();
};
