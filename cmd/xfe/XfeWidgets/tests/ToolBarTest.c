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
/* Name:		ToolBarTest.c											*/
/* Description:	Test for XfeToolBar widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/XfeTest.h>

static void		scale_cb			(Widget,XtPointer,XtPointer);
static void		hide_cb				(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget		form;
	Widget		frame;
    Widget		tool_bar;
    Widget		scale;
    Widget		hide;
    
	XfeAppCreateSimple("ToolBarTest",&argc,argv,"MainFrame",&frame,&form);
    
    tool_bar = XfeCreateLoadedToolBar(form,
									  "ToolBar",
									  "Tool",
									  50,
/* 									  20, */
									  0,
									  XfeArmCallback,
									  XfeDisarmCallback,
									  XfeActivateCallback,
									  NULL);

    scale = XtVaCreateManagedWidget("Scale",
                                    xmScaleWidgetClass,
                                    form,
									NULL);

	XtAddCallback(scale,XmNvalueChangedCallback,scale_cb,tool_bar);
	XtAddCallback(scale,XmNdragCallback,scale_cb,tool_bar);

    hide = XtVaCreateManagedWidget("Hide",
                                    xmPushButtonWidgetClass,
                                    form,
									NULL);

	XtAddCallback(hide,XmNactivateCallback,hide_cb,tool_bar);

	XtPopup(frame,XtGrabNone);
	
    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static void
scale_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	Widget		tool_bar = (Widget) client_data;
	int			value;

	assert( XfeIsAlive(tool_bar) );

	XmScaleGetValue(w,&value);

	value = value % 10;

	printf("%s(%s,%d)\n",__FUNCTION__,XtName(w),value);

	XtVaSetValues(tool_bar,XmNindicatorPosition,value,NULL);
}
/*----------------------------------------------------------------------*/
static void
hide_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	Widget		tool_bar = (Widget) client_data;

	assert( XfeIsAlive(tool_bar) );


	printf("%s(%s)\n",__FUNCTION__,XtName(w));

	XtVaSetValues(tool_bar,XmNindicatorPosition,-1,NULL);
}
/*----------------------------------------------------------------------*/
