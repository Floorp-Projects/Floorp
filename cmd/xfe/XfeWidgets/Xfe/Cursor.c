/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/Cursor.c>											*/
/* Description:	Xfe widgets cursor utilities.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>

#define hand_width 16
#define hand_height 16

#define hand_x_hot 3
#define hand_y_hot 2

static char hand_bits[] = 
{
	0x80, 0x01, 0x58, 0x0e, 0x64, 0x12, 0x64, 0x52, 0x48, 0xb2, 0x48, 0x92,
	0x16, 0x90, 0x19, 0x80, 0x11, 0x40, 0x02, 0x40, 0x04, 0x40, 0x04, 0x20,
	0x08, 0x20, 0x10, 0x10, 0x20, 0x10, 0x20, 0x10
};

#define hand_mask_width 16
#define hand_mask_height 16

static char hand_mask_bits[] = 
{
	0xff, 0xff, 0x7f, 0xfe, 0x67, 0xf2, 0x67, 0xf2, 0x4f, 0xb2, 0x4f, 0x92,
	0x17, 0x90, 0x19, 0x80, 0x11, 0xc0, 0x03, 0xc0, 0x07, 0xc0, 0x07, 0xe0,
	0x0f, 0xe0, 0x1f, 0xf0, 0x3f, 0xf0, 0x3f, 0xf0

#if 0
	0x80, 0x01, 0xd8, 0x0f, 0xfc, 0x1f, 0xfc, 0x5f, 0xf8, 0xff, 0xf8, 0xff,
	0xfe, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xfe, 0x7f, 0xfc, 0x7f, 0xfc, 0x3f,
	0xf8, 0x3f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0, 0x1f
#endif
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Cursor																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeCursorDefine(Widget w,Cursor cursor)
{
    if (_XfeIsRealized(w) && _XfeCursorGood(cursor))
    {
		XDefineCursor(XtDisplay(w),_XfeWindow(w),cursor);
    }
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeCursorUndefine(Widget w)
{
    if (_XfeIsRealized(w))
    {
		XUndefineCursor(XtDisplay(w),_XfeWindow(w));
    }
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeCursorGood(Cursor cursor)
{
	return _XfeCursorGood(cursor);
}
/*----------------------------------------------------------------------*/
/* extern */ Cursor
XfeCursorGetDragHand(Widget w)
{
	static Cursor	hand_cursor = None;

	if (hand_cursor == None)
	{
		Pixmap		cursor_pixmap;
		Pixmap		cursor_mask;
		XColor		white_color;
		XColor		black_color;

		assert( _XfeIsAlive(w) );
		
		cursor_pixmap = 
			XCreatePixmapFromBitmapData(XtDisplay(w),
										DefaultRootWindow(XtDisplay(w)),
										hand_bits,
										hand_width,
										hand_height,
										BlackPixelOfScreen(_XfeScreen(w)),
										WhitePixelOfScreen(_XfeScreen(w)),
										1);
		
		
		assert( _XfePixmapGood(cursor_pixmap) );
		
		cursor_mask = 
			XCreatePixmapFromBitmapData(XtDisplay(w),
										DefaultRootWindow(XtDisplay(w)),
										hand_mask_bits,
										hand_mask_width,
										hand_mask_height,
										BlackPixelOfScreen(_XfeScreen(w)),
										WhitePixelOfScreen(_XfeScreen(w)),
										1);
		
		assert( _XfePixmapGood(cursor_mask) );
		
		white_color.red = white_color.green = white_color.blue = 0xffff;
		black_color.red = black_color.green = black_color.blue = 0x0;
		
		hand_cursor = XCreatePixmapCursor(XtDisplay(w),
										  cursor_pixmap,
										  cursor_mask,
										  &white_color,
										  &black_color,
										  hand_x_hot,
										  hand_y_hot);

		assert( hand_cursor != None );
	}


	return hand_cursor;
}
/*----------------------------------------------------------------------*/

