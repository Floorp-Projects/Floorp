/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
