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

//	UTextBox.cp

// Based on "The TextBox You've Always Wanted" by Bryan K. Ressler (Beaker) in develop #9

#ifdef PowerPlant_PCH
	#include PowerPlant_PCH
#endif

#include "UTextBox.h"

#include <Gestalt.h>
#include <Fonts.h>
#include <FixMath.h>

// ---------------------------------------------------------------------------
//		е UTextBox
// ---------------------------------------------------------------------------
//	Constructor

/*
void
UTextBox::UTextBox( void )
{
	long gestResult;

	::Gestalt( gestaltFontMgrAttr, &gestResult );
	hasTrueType = ( gestResult & gestaltOutlineFonts );
}
*/

// ---------------------------------------------------------------------------
//		е TBLineHeight
// ---------------------------------------------------------------------------
//	Figures line height
//
// Input:	theText		the entire text that was given to the DrawTextBox call
//			textLen		the length in bytes of the text
//			lhCode		the line height code that was passed to DrawTextBox
//			startY		VAR - we return the starting vertical pen location here
//
// Output:	returns the line height to use
//

unsigned short
UTextBox::TBLineHeight( Ptr theText, unsigned long textLen,
						Rect *wrapBox, short lhCode, short *startY )
{
	short			asc,desc;	/* Used in the OutlineMetrics calls */
	FontInfo		fInfo;		/* The old-style font information record */
	Point			frac;		/* The fraction for the TrueType calls */
	unsigned short	lineHeight;	/* The return value */
	Boolean			hasTrueType;
	long			gestResult;

	::Gestalt( gestaltFontMgrAttr, &gestResult );
	hasTrueType = ( gestResult & gestaltOutlineFonts );

	::GetFontInfo( &fInfo );
	if (lhCode < 0) {
		frac.h = frac.v = 1;
		if ( hasTrueType && ::IsOutline(frac, frac) ) {
			::OutlineMetrics( (short)textLen, theText, frac, frac, &asc, &desc,
				nil, nil, nil );
			lineHeight = MAXOF(fInfo.ascent, asc) + MAXOF(fInfo. descent,-desc) +
				fInfo.leading;
			*startY = wrapBox->top + MAXOF(fInfo.ascent, asc);
			*startY += fInfo.leading;
		} else {
			lineHeight = fInfo.ascent + fInfo.descent + fInfo.leading;
			*startY = wrapBox->top + fInfo.ascent + fInfo.leading;
		}
	} else if (lhCode == 0) {
		lineHeight = fInfo.ascent + fInfo.descent + fInfo.leading;
		*startY = wrapBox->top + fInfo.ascent + fInfo.leading;
	} else {
		lineHeight = lhCode;
		*startY = wrapBox->top + lhCode + fInfo.leading;
	}

	return(lineHeight);
}

// ---------------------------------------------------------------------------
//		е TBDraw
// ---------------------------------------------------------------------------
//	Draws a line with appropriate justification
//
// Input:	breakCode	the break code that was returned from StyledLineBreak
//			lineStart	pointer to the beginning of the text for the current line
//			lineBytes	the length in bytes of the the text for this line
//			wrapBox		the box within which we're wrapping
//			align		the text alignment as specified by the user
//			curY		our current vertical pen coordinate
//			boxWidth	the width of wrapBox (since DrawTextBox already calculated it)
//
// Output:	none (draws on the screen)
//

void
UTextBox::TBDraw( StyledLineBreakCode breakCode, Ptr lineStart,
				long lineBytes, Rect *wrapBox, short align, short curY, short boxWidth )
{
	unsigned long	blackLen;	/* Length of non-white characters */
	short			slop;		/* Number of pixels of slop for full just */

	blackLen = ::VisibleLength( (Ptr)lineStart, lineBytes);
	
	if (align == ntbJustFull) {
		slop = boxWidth - ::TextWidth(lineStart, 0, blackLen);
		::MoveTo(wrapBox->left, curY);
		if (breakCode == smBreakOverflow ||
			*(lineStart + (lineBytes - 1)) == kReturnChar)
			align = ::GetSysDirection();
		else
			::DrawJust( (Ptr)lineStart, blackLen, slop );
	}

	switch(align) {
		case teForceLeft:
		case teJustLeft:
			::MoveTo(wrapBox->left, curY);
			break;
		case teJustRight:
			::MoveTo(wrapBox->right - TextWidth(lineStart, 0, blackLen), curY);
			break;
		case teJustCenter:
			::MoveTo(wrapBox->left + (boxWidth - ::TextWidth(lineStart, 0, blackLen)) / 2,
				curY);
			break;
	}
	if (align != ntbJustFull)
		::DrawText(lineStart, 0, lineBytes);
}

// ---------------------------------------------------------------------------
//		е DrawTextBox
// ---------------------------------------------------------------------------
//	Word-wraps text inside a given box
//
// Input:	theText		the text we need to wrap
//			textLen		the length in bytes of the text
//			wrapBox		the box within which we're wrapping
//			align		the text alignment
//							teForceLeft, teFlushLeft	left justified
//							teJustCenter, teCenter		center justified
//							teJustRight, teFlushRight	right justified
//							ntbJustFull					full justified
//							teJustLeft, teFlushDefault	system justified
//			lhCode		the line height code that was passed to DrawTextBox
//							< 0		variable - based on tallest character
//							0		default - based on GetFontInfo
//							> 0		fixed - use lhCode as the line height
//			endY		VAR - if non-nil, the vertical coord of the last line
//			lhUsed		VAR - if non-nil, the line height used to draw the text
//
// Output:	returns the number of line drawn total (even if they drew outside of
//			the boundries of wrapBox)
//			

short
UTextBox::DrawTextBox( Ptr theText, unsigned long textLen, Rect *wrapBox,
					short align, short lhCode, short *endY, short *lhUsed)
{
	StyledLineBreakCode	breakCode;		/* Returned code from StyledLineBreak */
	Fixed				fixedMax;		/* boxWidth converted to fixed point */
	Fixed				wrapWid;		/* Width to wrap to */
	short				boxWidth;		/* Width of the wrapBox */
	long				lineBytes;		/* Number of bytes in one line */
	unsigned short		lineHeight;		/* Calculated line height */
	short				curY;			/* Current vertical pen location */
	unsigned short		lineCount;		/* Number of lines we've drawn */
	long				textLeft;		/* Pointer to remaining bytes of text */
	Ptr					lineStart;		/* Pointer to beginning of a line */
	Ptr					textEnd;		/* Pointer to the end of input text */

	boxWidth = wrapBox->right - wrapBox->left;
	fixedMax = ::Long2Fix((long)boxWidth);
	if (align == teFlushDefault)
		align = ::GetSysDirection();

	lineHeight = UTextBox::TBLineHeight(theText, textLen, wrapBox, lhCode, &curY);
	lineCount = 0;
	lineStart = theText;
	textEnd = theText + textLen;
	textLeft = textLen;
	
	do {
		lineBytes = 1;
		wrapWid = fixedMax;

		breakCode = ::StyledLineBreak( (Ptr)lineStart, textLeft, 0, textLeft, 0,
									&wrapWid, &lineBytes );
		
		UTextBox::TBDraw(breakCode, lineStart, lineBytes, wrapBox, align, curY, boxWidth);
		curY += lineHeight;
		lineStart += lineBytes;
		textLeft -= lineBytes;
		lineCount++;
		
	} while (lineStart < textEnd);
	
	if (endY)
		*endY = curY - lineHeight;
	if (lhUsed)
		*lhUsed = lineHeight;

	return(lineCount);
}

// ---------------------------------------------------------------------------
//		е DrawTextBox
// ---------------------------------------------------------------------------
//	Word-wraps text inside a given box
//
// Input:	theText		the text we need to wrap
//			textLen		the length in bytes of the text
//			wrapBox		the box within which we're wrapping
//			align		the text alignment
//							teForceLeft, teFlushLeft	left justified
//							teJustCenter, teCenter		center justified
//							teJustRight, teFlushRight	right justified
//							ntbJustFull					full justified
//							teJustLeft, teFlushDefault	system justified
//			lhCode		the line height code that was passed to DrawTextBox
//							< 0		variable - based on tallest character
//							0		default - based on GetFontInfo
//							> 0		fixed - use lhCode as the line height
//
//			

void
UTextBox::DrawTextBox( Ptr theText, unsigned long textLen, Rect *wrapBox,
					short align, short lhCode )
{
	RgnHandle			oldClip;		/* Saved clipping region */
	StyledLineBreakCode	breakCode;		/* Returned code from StyledLineBreak */
	Fixed				fixedMax;		/* boxWidth converted to fixed point */
	Fixed				wrapWid;		/* Width to wrap to */
	short				boxWidth;		/* Width of the wrapBox */
	long				lineBytes;		/* Number of bytes in one line */
	unsigned short		lineHeight;		/* Calculated line height */
	short				curY;			/* Current vertical pen location */
	unsigned short		lineCount;		/* Number of lines we've drawn */
	long				textLeft;		/* Pointer to remaining bytes of text */
	Ptr					lineStart;		/* Pointer to beginning of a line */
	Ptr					textEnd;		/* Pointer to the end of input text */

	::GetClip((oldClip = ::NewRgn()));
	::ClipRect(wrapBox);
	boxWidth = wrapBox->right - wrapBox->left;
	fixedMax = ::Long2Fix((long)boxWidth);
	if (align == teFlushDefault)
		align = ::GetSysDirection();

	lineHeight = UTextBox::TBLineHeight(theText, textLen, wrapBox, lhCode, &curY);
	lineCount = 0;
	lineStart = theText;
	textEnd = theText + textLen;
	textLeft = textLen;
	
	do {
		lineBytes = 1;
		wrapWid = fixedMax;

		breakCode = ::StyledLineBreak( (Ptr)lineStart, textLeft, 0, textLeft, 0,
									&wrapWid, &lineBytes );
		
		UTextBox::TBDraw(breakCode, lineStart, lineBytes, wrapBox, align, curY, boxWidth);
		
		curY += lineHeight;
		lineStart += lineBytes;
		textLeft -= lineBytes;
		lineCount++;
		
	} while ((lineStart < textEnd) & ( curY <= (wrapBox->bottom + lineHeight) ));
	
	::SetClip(oldClip);
	::DisposeRgn(oldClip);

}

// Hacked up, copied version of DrawTextBox. Returns the height needed for the lines
// DESTROYS THE ORIGINAL STRING
// pkc (6/6/96) added scan for '\n' to determine line breaks since StyledLineBreak doesn't
// consider '\n' to be a line break.
short UTextBox::TextBoxDialogHeight( Ptr theText, unsigned long textLen, Rect *wrapBox,
					short align, short lhCode)
{
	StyledLineBreakCode	breakCode;		/* Returned code from StyledLineBreak */
	Fixed				fixedMax;		/* boxWidth converted to fixed point */
	Fixed				wrapWid;		/* Width to wrap to */
	short				boxWidth;		/* Width of the wrapBox */
	long				lineBytes;		/* Number of bytes in one line */
	unsigned short		lineHeight;		/* Calculated line height */
	short				curY;			/* Current vertical pen location */
	unsigned short		lineCount;		/* Number of lines we've drawn */
	long				textLeft;		/* Pointer to remaining bytes of text */
	Ptr					lineStart;		/* Pointer to beginning of a line */
	Ptr					textEnd;		/* Pointer to the end of input text */
	Ptr					scanner;		/* Pointer to scan for '\n' */

	boxWidth = wrapBox->right - wrapBox->left;
	fixedMax = ::Long2Fix((long)boxWidth);
	if (align == teFlushDefault)
		align = ::GetSysDirection();

	lineHeight = UTextBox::TBLineHeight(theText, textLen, wrapBox, lhCode, &curY);
	lineCount = 0;
	lineStart = theText;
	textEnd = theText + textLen;
	textLeft = textLen;
	
	// еее HACK
	
	for (int i=0; i< textLen; i++)
	{
		if (theText[i] == '/')
			theText[i] = 'w';
	}
	do {
		lineBytes = 1;
		wrapWid = fixedMax;

		breakCode = ::StyledLineBreak( (Ptr)lineStart, textLeft, 0, textLeft, 0,
									&wrapWid, &lineBytes );
		// pkc (6/6/96) now scan for '\n' because StyledLineBreak doesn't take '\n' into account
		scanner = lineStart;
		while( *scanner != '\n' && scanner < textEnd )
			++scanner;
		// pkc (6/6/96) move past '\n'
		++scanner;
		// pkc (6/6/96) if there is a '\n' before where StyledLineBreak says that there should be
		// a break, we should break at the '\n'
		if( scanner < textEnd && scanner - lineStart < lineBytes )
			lineBytes = scanner - lineStart;
		curY += lineHeight;
		lineStart += lineBytes;
		textLeft -= lineBytes;
		lineCount++;
		
	} while (lineStart < textEnd);
	

	return(lineCount * lineHeight);
}
