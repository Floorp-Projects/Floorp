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
/* Name:		ToolScrollTest.c										*/
/* Description:	Test for XfeToolScroll widget.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeTest.h>

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget		form;
	Widget		frame;
    Widget		tool_scroll;
    Widget		tool_bar;
    
	XfeAppCreateSimple("ToolScrollTest",&argc,argv,"MainFrame",&frame,&form);
    
    tool_scroll = XtVaCreateManagedWidget("ToolScroll",
										  xfeToolScrollWidgetClass,
										  form,
										  NULL);

    XtVaGetValues(tool_scroll,XmNtoolBar,&tool_bar,NULL);

	XfeLoadToolBar(tool_bar,
				   "Tool",
				   15,
				   3,
				   NULL,
				   NULL,
				   NULL,
				   NULL);

	XfeOrientedChildrenSetAllowDrag(tool_bar,True);

	XtPopup(frame,XtGrabNone);
	
    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
