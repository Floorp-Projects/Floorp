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
/* Name:		FontChooserTest.c										*/
/* Description:	Test for XfeFontChooser widget.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include <Xfe/XfeTest.h>

static String font_names[] = 
{
	"-adobe-courier-medium-r-normal--20-140-100-100-m-110-iso8859-1",
	"-adobe-helvetica-medium-r-normal--20-140-100-100-p-100-iso8859-1",
	"-adobe-new century schoolbook-medium-r-normal--20-140-100-100-p-103-iso8859-1",
	"-adobe-symbol-medium-r-normal--20-140-100-100-p-107-adobe-fontspecific",
	"-adobe-times-medium-r-normal--20-140-100-100-p-96-iso8859-1",
	"-b&h-lucida-medium-r-normal-sans-20-140-100-100-p-114-iso8859-1",
	"-b&h-lucidabright-medium-r-normal--20-140-100-100-p-114-iso8859-1",
	"-b&h-lucidatypewriter-medium-r-normal-sans-20-140-100-100-m-120-iso8859-1",
	"-misc-fixed-medium-r-normal--20-140-100-100-c-100-iso8859-1",
};

static String label_names[] = 
{
	"Courier",
	"Helvetica",
	"New Century Schoolbook",
	"Symbol",
	"Times",
	"Lucida",
	"Lucidabright",
	"Lucidatypewriter",
	"Fixed",
};

#define num_font_names XtNumber(font_names)

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget			form;
	Widget			frame;
    Widget			font_chooser;
	XmString *		xm_string_table;
	XmFontList *	font_list_table;
    
	XfeAppCreateSimple("FontChooserTest",&argc,argv,"MainFrame",&frame,&form);

	xm_string_table = XfeGetXmStringTable(label_names,num_font_names);

	font_list_table = XfeGetFontListTable(font_names,num_font_names);
    
    font_chooser = XtVaCreateManagedWidget("FontChooser",
										   xfeFontChooserWidgetClass,
										   form,
										   NULL);

    XtVaSetValues(font_chooser,
				  XmNnumFontItems,		num_font_names,
				  XmNfontItemLabels,	xm_string_table,
				  XmNfontItemFonts,		font_list_table,
				  NULL);
	
	XtPopup(frame,XtGrabNone);
	
    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
