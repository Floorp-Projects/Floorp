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
/* Name:		PaneTestOne.c											*/
/* Description:	Test for XfePane widget.								*/
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
	Widget		main_pane;

	Widget		e_pane;
	Widget		w_pane;

	Widget		ne_pane;
	Widget		nw_pane;
	Widget		se_pane;
	Widget		sw_pane;

	XfeAppCreateSimple("PaneTestOne",&argc,argv,"Frame",&frame,&main_form);
	
	main_pane = XtVaCreateManagedWidget("MainPane",
										xfePaneWidgetClass,
										main_form,
										NULL);

	e_pane = XtVaCreateManagedWidget("EPane",
									 xfePaneWidgetClass,
										main_pane,
									 NULL);
	
	w_pane = XtVaCreateManagedWidget("WPane",
									 xfePaneWidgetClass,
									 main_pane,
									 NULL);

	ne_pane = XtVaCreateManagedWidget("NEPane",
									  xfePaneWidgetClass,
									  e_pane,
									  NULL);

	se_pane = XtVaCreateManagedWidget("NEPane",
									  xfePaneWidgetClass,
									  e_pane,
									  NULL);

	nw_pane = XtVaCreateManagedWidget("NWPane",
									  xfePaneWidgetClass,
									  w_pane,
									  NULL);

	sw_pane = XtVaCreateManagedWidget("SWPane",
									  xfePaneWidgetClass,
									  w_pane,
									  NULL);

 	XfeCreateFormAndButton(se_pane,"Form","SE1",5,True,NULL,0);
 	XfeCreateFormAndButton(se_pane,"Form","SE2",5,True,NULL,0);

 	XfeCreateFormAndButton(sw_pane,"Form","SW1",5,True,NULL,0);
 	XfeCreateFormAndButton(sw_pane,"Form","SW2",5,True,NULL,0);

 	XfeCreateFormAndButton(ne_pane,"Form","NE1",5,True,NULL,0);
 	XfeCreateFormAndButton(ne_pane,"Form","NE2",5,True,NULL,0);

 	XfeCreateFormAndButton(nw_pane,"Form","NW1",5,True,NULL,0);
 	XfeCreateFormAndButton(nw_pane,"Form","NW2",5,True,NULL,0);


	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
