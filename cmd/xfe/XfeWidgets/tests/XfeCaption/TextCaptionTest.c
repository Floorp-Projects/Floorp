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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		TextCaptionTest.c										*/
/* Description:	Test for XfeTextCaption widget.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/
 
#include <Xfe/XfeTest.h>
#include <Xfe/TextCaption.h>

static Widget	create_caption	(Widget,String);

static Widget	_captions[3];

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	form;
	Widget	frame;
	
	XfeAppCreateSimple("TextCaptionTest",&argc,argv,"MainFrame",&frame,&form);

	_captions[0] = create_caption(form,"Caption1");
	_captions[1] = create_caption(form,"Caption2");
	_captions[2] = create_caption(form,"Caption3");

	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_caption(Widget pw,String name)
{
	Widget caption;

	caption = XtVaCreateManagedWidget(name,
									  xfeTextCaptionWidgetClass,
									  pw,
									  NULL);

	return caption;
}
/*----------------------------------------------------------------------*/
