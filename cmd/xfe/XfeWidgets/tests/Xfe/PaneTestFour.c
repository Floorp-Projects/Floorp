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
/* Name:		PaneTestFour.c											*/
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

	Widget		nav_center_pane;
	Widget		rdf_pane;
	Widget		selector;
	Widget		content;
	Widget		tree;

	XfeAppCreateSimple("PaneTestFour",&argc,argv,"Frame",&frame,&main_form);
	
	nav_center_pane = 
		XtVaCreateManagedWidget("mainPane",
								xfePaneWidgetClass,
								main_form,
								XmNorientation,		XmHORIZONTAL,
								NULL);

	rdf_pane = 
		XtVaCreateManagedWidget("rdfPane",
								xfePaneWidgetClass,
								nav_center_pane,
								XmNorientation,		XmHORIZONTAL,
								NULL);

	selector = 
		XtVaCreateManagedWidget("rdfPane",
								xfeToolScrollWidgetClass,
								rdf_pane,
								NULL);

	tree = 
		XtVaCreateManagedWidget("tree",
								xmFormWidgetClass,
								rdf_pane,
								NULL);


	content = 
		XtVaCreateManagedWidget("content",
								xmTextWidgetClass,
								nav_center_pane,
								NULL);

	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
