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
/* Name:		TaskBarTestTwo.c										*/
/* Description:	Test for XfeTaskBar widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

static Widget	_docked_tb = NULL;

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	form;
	Widget	frame;

	XfeAppCreateSimple("TaskBarTestTwo",&argc,argv,"MainFrame",&frame,&form);

    _docked_tb = XfeCreateLoadedTaskBar(form,
										"DockedTaskBar",
										True,
										"task_",
										NULL,
										10,
										NULL);
	
	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
