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
static void		location_cb			(Widget,XtPointer,XtPointer);

static Widget	_tool_bar = NULL;

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget		form;
	Widget		frame;
    Widget		scale;
    Widget		hide;
    Widget		location_tool_bar;
    
	XfeAppCreateSimple("ToolBarTest",&argc,argv,"MainFrame",&frame,&form);
    
    _tool_bar = XfeCreateLoadedToolBar(form,
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

	XtAddCallback(scale,XmNvalueChangedCallback,scale_cb,NULL);
	XtAddCallback(scale,XmNdragCallback,scale_cb,NULL);

    hide = XtVaCreateManagedWidget("Hide",
                                    xmPushButtonWidgetClass,
                                    form,
									NULL);

	XtAddCallback(hide,XmNactivateCallback,hide_cb,NULL);

    location_tool_bar = XfeCreateLoadedToolBar(form,
											   "LocationToolBar",
											   "Item",
											   4,
											   0,
											   NULL,
											   NULL,
											   location_cb,
											   NULL);

	XtPopup(frame,XtGrabNone);
	
    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static void
scale_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	int			value;

	assert( XfeIsAlive(_tool_bar) );

	XmScaleGetValue(w,&value);

	value = value / 10;

	printf("%s(%s,%d)\n",__FUNCTION__,XtName(w),value);

	XtVaSetValues(_tool_bar,XmNindicatorPosition,value,NULL);
}
/*----------------------------------------------------------------------*/
static void
hide_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(_tool_bar) );

	printf("%s(%s)\n",__FUNCTION__,XtName(w));

	XtVaSetValues(_tool_bar,XmNindicatorPosition,XmINDICATOR_DONT_SHOW,NULL);
}
/*----------------------------------------------------------------------*/
static void
location_cb(Widget w,XtPointer client_data,XtPointer call_data)
{
	unsigned char location = XmINDICATOR_LOCATION_NONE;

	assert( XfeIsAlive(_tool_bar) );

	if (strcmp(XtName(w),"Item1") == 0)
	{
		location = XmINDICATOR_LOCATION_NONE;
	}
	else if (strcmp(XtName(w),"Item2") == 0)
	{
		location = XmINDICATOR_LOCATION_BEGINNING;
	}
	else if (strcmp(XtName(w),"Item3") == 0)
	{
		location = XmINDICATOR_LOCATION_END;
	}
	else if (strcmp(XtName(w),"Item4") == 0)
	{
		location = XmINDICATOR_LOCATION_MIDDLE;
	}

	printf("%s(%s) location = %s\n",
		   __FUNCTION__,
		   XtName(w),
		   XfeDebugRepTypeValueToName(XmRToolBarIndicatorLocation,location));

	XtVaSetValues(_tool_bar,XmNindicatorLocation,location,NULL);
}
/*----------------------------------------------------------------------*/
