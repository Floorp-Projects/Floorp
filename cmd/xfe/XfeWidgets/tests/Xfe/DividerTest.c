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
/* Name:		Divider.c												*/
/* Description:	Test for XfeDivider widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/XfeTest.h>

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget		frame;
	Widget		main_form;
	Widget		divider;

	XfeAppCreateSimple("Divider",&argc,argv,"Frame",&frame,&main_form);

	divider = XtVaCreateManagedWidget("MainDivider",
									  xfeDividerWidgetClass,
									  main_form,
									  NULL);

 	XfeCreateFormAndButton(divider,"FormOne","ButtonOne",5,True,NULL,0);
 	XfeCreateFormAndButton(divider,"FormTwo","ButtonTwo",5,True,NULL,0);

	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
