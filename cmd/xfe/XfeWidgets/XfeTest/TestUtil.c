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
/* Name:		<XfeTest/TestUtil.c>									*/
/* Description:	Xfe widget misc utils.									*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeTest.h>
#include <ctype.h>

static XmString fe_StringChopCreate(char *,char *,XmFontList,int);

/*----------------------------------------------------------------------*/
Pixel
XfeButtonRaiseBackground(Widget w)
{
	Pixel raise_background;

	assert( XfeIsButton(w) );

	XtVaGetValues(w,XmNraiseBackground,&raise_background,NULL);

	return raise_background;
}
/*----------------------------------------------------------------------*/
Pixel
XfeButtonArmBackground(Widget w)
{
	Pixel arm_background;

	assert( XfeIsButton(w) );

	XtVaGetValues(w,XmNarmBackground,&arm_background,NULL);

	return arm_background;
}
/*----------------------------------------------------------------------*/
XfeTruncateXmStringProc
XfeGetTruncateXmStringProc(void)
{
	return fe_StringChopCreate;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeWidgetToggleManaged(Widget w)
{
	assert( XfeIsAlive(w) );

	if (XtIsManaged(w))
	{
		XtUnmanageChild(w);
	}
	else
	{
		XtManageChild(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeResourceToggleBoolean(Widget w,String name)
{
	Boolean value;

	assert( XfeIsAlive(w) );

	XtVaGetValues(w,name,&value,NULL);

	XtVaSetValues(w,name,!value,NULL);
}
/*----------------------------------------------------------------------*/

static Cardinal	_flash_num = 0;
static Boolean	_flash_on = False;
static Cardinal	_flash_ms = 0;
static GC		_flash_gc = NULL;
static Pixel	_flash_bg = 0;
static Pixel	_flash_fg = 0;

/*----------------------------------------------------------------------*/
static void
FlashTimeout(XtPointer client_data,XtIntervalId * id)
{
	Widget		w = (Widget) client_data;

	if (!_flash_on || !_flash_num || !XfeIsAlive(w))
	{
		_flash_on = False;
		_flash_num = 0;

		return;
	}

	XFillRectangle(XtDisplay(w),XtWindow(w),_flash_gc,
				   0,0,XfeWidth(w),XfeHeight(w));

	_flash_num--;

	XtAppAddTimeOut(XtWidgetToApplicationContext(w),
					_flash_ms,FlashTimeout,(XtPointer) w);	
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeWidgetFlash(Widget w,Cardinal ms,Cardinal n)
{
	assert( XfeIsAlive(w) );
	assert( n > 0 );
	assert( ms > 0 );

	if (_flash_on)
	{
		return;
	}

	if (!XfeIsAlive(w) || !XtIsManaged(w))
	{
		return;
	}

	_flash_num = n * 2;
	_flash_ms = ms;
	_flash_on = True;
	_flash_bg = XfeBackground(w);
	_flash_fg = XfeForeground(XtParent(w));

	if (_flash_gc)
	{
		XtReleaseGC(w,_flash_gc);
	}

 	_flash_gc = XfeAllocateSelectionGc(w,1,_flash_bg,_flash_fg);
	
	XtAppAddTimeOut(XtWidgetToApplicationContext(w),
					ms,FlashTimeout,(XtPointer) w);	
}
/*----------------------------------------------------------------------*/
/* extern */ XmFontList
XfeGetFontList(String name)
{
	XmFontList		font_list = NULL;
	XmFontListEntry	entry;

	assert( name != NULL );
	assert( XfeIsAlive(XfeAppShell()) );

	/* Load (ie, create) a font list entry */
	entry = XmFontListEntryLoad(XtDisplay(XfeAppShell()),
								name,
								XmFONT_IS_FONT,
								XmFONTLIST_DEFAULT_TAG);
	
	if (entry)
	{
		font_list = XmFontListAppendEntry(NULL,entry);

		XmFontListEntryFree(&entry);
	}

	return font_list;
}
/*----------------------------------------------------------------------*/
/* extern */ XmString
XfeGetXmString(String name)
{
	assert( name != NULL );

	return XmStringCreateLtoR(name,XmFONTLIST_DEFAULT_TAG);
}
/*----------------------------------------------------------------------*/
/* extern */ XmFontList *
XfeGetFontListTable(String * names,Cardinal n)
{
	XmFontList *	xm_font_list_table = NULL;
	Cardinal		i;

	assert( names != NULL );
	assert( n > 0 );

	xm_font_list_table = (XmFontList *) XtMalloc(sizeof(XmFontList) * n);	

	for(i = 0; i < n; i++)
	{
		assert( names[i] != NULL );

		xm_font_list_table[i] = XfeGetFontList(names[i]);
	}

	return xm_font_list_table;
}
/*----------------------------------------------------------------------*/
/* extern */ XmString *
XfeGetXmStringTable(String * names,Cardinal n)
{
	XmString *		xm_string_table = NULL;
	Cardinal		i;

	assert( names != NULL );
	assert( n > 0 );

	xm_string_table = (XmString *) XtMalloc(sizeof(XmString) * n);	

	for(i = 0; i < n; i++)
	{
		assert( names[i] != NULL );

		xm_string_table[i] = XfeGetXmString(names[i]);
	}

	return xm_string_table;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeFreeXmStringTable(XmString * table,Cardinal n)
{
	Cardinal i;

	assert( table != NULL );
	assert( n > 0 );

	for(i = 0; i < n; i++)
	{
		assert( table[i] != NULL );
		
		XmStringFree(table[i]);
	}

	XtFree((char *) table);
}
/*----------------------------------------------------------------------*/
/* extern */ Pixel
XfeGetPixel(String name)
{
	Pixel			pixel;
	XrmValue		from;
	XrmValue		to;

	assert( name != NULL );
	assert( XfeIsAlive(XfeAppShell()) );

	from.addr	= name;
	from.size	= strlen(name);

	to.addr		= (XPointer) &pixel;
	to.size		= sizeof(pixel);

	if (!XtConvertAndStore(XfeAppShell(),XmRString,&from,XmRPixel,&to))
	{
		pixel = WhitePixelOfScreen(XtScreen(XfeAppShell()));
	}

	return pixel;
}
/*----------------------------------------------------------------------*/
/* extern */ Cursor
XfeGetCursor(String name)
{
	Cursor			cursor;
	XrmValue		from;
	XrmValue		to;

	assert( name != NULL );
	assert( XfeIsAlive(XfeAppShell()) );

	from.addr	= name;
	from.size	= strlen(name);

	to.addr		= (XPointer) &cursor;
	to.size		= sizeof(cursor);

	if (!XtConvertAndStore(XfeAppShell(),XmRString,&from,XmRCursor,&to))
	{
		cursor = None;
	}

	return cursor;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeStringToUpper(String s)
{
	char *pc = s;

	while(*pc != '\0')
	{
		*pc = toupper(*pc);

		pc++;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeStringToLower(String s)
{
	char *pc = s;

	while(*pc != '\0')
	{
		*pc = tolower(*pc);

		pc++;
	}
}
/*----------------------------------------------------------------------*/


static XmString
fe_StringChopCreate(char* message, char* tag, XmFontList font_list,
		    int maxwidth)
{
  XmString label = XmStringCreate ((char *) message, tag);
  int string_width;

  if (!font_list) return label;

  string_width = XmStringWidth(font_list, label);
  if (string_width >= maxwidth) {
    /* The string is bigger than the label.  Mid-truncate it. */

    XmString try;
    int length = 0;
    int maxlength = strlen(message);
    int tlen;
    int hlen;
    char* text = XtMalloc(maxlength + 10);
    char* end = message + maxlength;
    if (!text) return label;
      
    string_width = 0;
    while (string_width < maxwidth && length < maxlength) {
      length++;
      tlen = length / 2;
      hlen = length - tlen;
      strncpy(text, message, hlen);
      strncpy(text + hlen, "...", 3);
      strncpy(text + hlen + 3, end - tlen, tlen);
      text[length + 3] = '\0';
      try = XmStringCreate(text, tag);
      if (!try) break;
      string_width = XmStringWidth(font_list, try);
      if (string_width >= maxwidth) {
	XmStringFree(try);
      } else {
	XmStringFree(label);
	label = try;
      }
      try = 0;
    }

    XtFree (text);
  }

  return label;
}
