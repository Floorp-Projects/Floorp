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

//	UTextBox.h

// Based on "The TextBox You've Always Wanted" by Bryan K. Ressler (Beaker) in develop #9

#pragma once

#ifndef __TEXTEDIT__
#include <TextEdit.h>
#endif

// DEFINES
#define ntbJustFull			128			/* Full justification */
#define kReturnChar			0x0d		/* Carriage return character */

// MACROS
#define MAXOF(a,b)	(((a) > (b)) ? (a) : (b))

class UTextBox
{
public:
	
	static void 			DrawTextBox( Ptr theText, unsigned long textLen,
									Rect *box, short just, short htCode );

	static short 			DrawTextBox( Ptr theText, unsigned long textLen,
									Rect *box, short just, short htCode, short *endY,
									short *lhUsed );

	// This added by aleks. Used to resize alert boxes in errmgr
	static short			TextBoxDialogHeight(Ptr theText, unsigned long textLen,
									Rect *box, short just, short htCode);

private:
	static unsigned short	TBLineHeight( Ptr theText, unsigned long textLen,
									Rect *wrapBox, short lhCode, short *startY );

	static void 			TBDraw( StyledLineBreakCode breakCode, Ptr lineStart,
									long lineBytes, Rect *wrapBox, short align,
									short curY, short boxWidth );

};
	