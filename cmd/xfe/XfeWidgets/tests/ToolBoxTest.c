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
/* Name:		ToolBoxTest.c											*/
/* Description:	Test for XfeToolBox widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeTest.h>

static Widget	create_tool_item		(Widget,String,Cardinal);
static void		close_cb				(Widget,XtPointer,XtPointer);
static void		open_cb				(Widget,XtPointer,XtPointer);
static void		new_item_cb				(Widget,XtPointer,XtPointer);
static void		snap_cb					(Widget,XtPointer,XtPointer);
static void		swap_cb					(Widget,XtPointer,XtPointer);

static void		button_cb				(Widget,XtPointer,XtPointer);


static String tool_names[] = 
{
	"red",
	"yellow",
	"green",
#if 1
	"blue",
	"orange",
	"purple",
	"pink",
	"aliceblue"
#endif
};


static int tool_heights[] = 
{
	2,
	4,
	6,
	8,
	10,
	12,
	10,
	8
};

#define NUM_TOOLS XtNumber(tool_names)

static Widget tool_widgets[NUM_TOOLS];

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	form;
	Widget	frame;

    Widget		tb;
	Cardinal	i;

	XfeAppCreateSimple("ToolBoxTest",&argc,argv,"MainFrame",&frame,&form);
    
    tb = XfeCreateLoadedToolBox(form,"ToolBox",NULL,0);

	for (i = 0; i < NUM_TOOLS; i++)
	{
		tool_widgets[i] = create_tool_item(tb,
										   tool_names[i],
										   tool_heights[i]);
	}

	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_tool_item(Widget parent,String name,Cardinal nitems)
{
    Widget		tool_item;
    Widget		tool_bar;
	Widget		logo;

    tool_item = 
		XtVaCreateManagedWidget(name,
								xfeToolItemWidgetClass,
								parent,
								XmNusePreferredHeight,	True,
								XmNusePreferredWidth,	False,
								XmNbackground,			XfeGetPixel(name),
								NULL);

    logo = 
		XtVaCreateManagedWidget("Logo",
								xfeLogoWidgetClass,
								tool_item,
								XmNbackground,			XfeGetPixel(name),
								NULL);
	
    tool_bar = XfeCreateLoadedToolBar(tool_item,
									  "ToolBar",
									  "Item",
									  nitems,
									  nitems / 10,
									  XfeArmCallback,
									  XfeDisarmCallback,
									  XfeActivateCallback,
									  NULL);

	XtVaSetValues(tool_bar,XmNbackground,XfeGetPixel(name),NULL);
	
	XfeToolBoxAddDragDescendant(parent,tool_item);
	XfeToolBoxAddDragDescendant(parent,tool_bar);
	XfeToolBoxAddDragDescendant(parent,logo);

    return tool_item;
}
/*----------------------------------------------------------------------*/
static void
snap_cb(Widget w,XtPointer clien_data,XtPointer call_data)
{
	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) call_data;

	printf("snap      (%s at %d)\n",XtName(cbs->item),cbs->index);
}
/*----------------------------------------------------------------------*/
static void
new_item_cb(Widget w,XtPointer clien_data,XtPointer call_data)
{
	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) call_data;

	printf("new_item  (%s at %d)\n",XtName(cbs->item),cbs->index);
}
/*----------------------------------------------------------------------*/
static void
close_cb(Widget w,XtPointer clien_data,XtPointer call_data)
{
	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) call_data;

	printf("close  (%s at %d)\n",XtName(cbs->item),cbs->index);
}
/*----------------------------------------------------------------------*/
static void
open_cb(Widget w,XtPointer clien_data,XtPointer call_data)
{
	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) call_data;

	printf("open    (%s at %d)\n",XtName(cbs->item),cbs->index);
}
/*----------------------------------------------------------------------*/
static void
swap_cb(Widget w,XtPointer clien_data,XtPointer call_data)
{
	XfeToolBoxSwapCallbackStruct *	cbs = 
		(XfeToolBoxSwapCallbackStruct *) call_data;
	
	printf("swap      (%s with %s from %d to %d)\n",
		   XtName(cbs->swapped),XtName(cbs->displaced),
		   cbs->from_index,cbs->to_index);
}
/*----------------------------------------------------------------------*/
