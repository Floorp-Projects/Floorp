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
/* Name:		CascadeTest.c											*/
/* Description:	Test for XfeCascade widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include <Xfe/XfeTest.h>

static String names[] =
{
    "One",
    "Two",
    "Three",
    "Four",
    "Five",
    "Six",
    "Seven",
    "Eight",
    "Nine",
    "Ten",
};

static Cardinal delays[] =
{
	0,
	200,
	0,
	0,
	300,
	1000,
	500,
	5000,
	400,
	0,
};

static Cardinal counts[] =
{
	0,
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128,
	1,
};

static void		activate_callback		(Widget,XtPointer,XtPointer);
static void		arm_callback			(Widget,XtPointer,XtPointer);
static void		disarm_callback			(Widget,XtPointer,XtPointer);
static void		popup_callback			(Widget,XtPointer,XtPointer);
static void		popdown_callback		(Widget,XtPointer,XtPointer);
static void		cascading_callback		(Widget,XtPointer,XtPointer);
static void		populate_callback		(Widget,XtPointer,XtPointer);

static Widget	create_button			(Widget,String,Cardinal,Cardinal);
static void		add_items				(Widget,Cardinal);
static Widget	add_item				(Widget,String);
static Widget	get_sub_menu_id			(Widget);

#define MAX_J 10

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget		form;
	Widget		frame;
    Widget		tool_bar;
    Cardinal	i;
    
	XfeAppCreateSimple("CascadeTest",&argc,argv,"MainFrame",&frame,&form);
    
    tool_bar = XtVaCreateManagedWidget("ToolBar",
									   xfeToolBarWidgetClass,
									   form,
									   NULL);
	
    for (i = 0; i < XtNumber(names); i++)
    {
		create_button(tool_bar,names[i],counts[i],delays[i]);
    }

	XtPopup(frame,XtGrabNone);
	
    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static void
activate_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
    XfeButtonCallbackStruct * cbs = (XfeButtonCallbackStruct *) call_data;

	printf("Activate(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
static void
arm_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
    XfeButtonCallbackStruct * cbs = (XfeButtonCallbackStruct *) call_data;

    printf("Arm(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
static void
disarm_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
    printf("Disarm(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
static void
popup_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
    printf("Popup(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
static void
popdown_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
    printf("Popdown(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
static void
populate_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
	Widget		submenu;
	Cardinal	num_children;

	XtVaGetValues(w,XmNsubMenuId,&submenu,NULL);

	assert( XfeIsAlive(submenu) );

	XfeChildrenGet(submenu,NULL,&num_children);


    printf("Populate(%s) Have %d menu items\n\n",XtName(w),num_children);

	if (strcmp(XtName(w),"One") == 0)
	{
		XfeCascadeDestroyChildren(w);
		/*XfeChildrenDestroy(submenu);*/

		XSync(XtDisplay(w),True);

		XmUpdateDisplay(w);

		add_items(w,30);
	}
}
/*----------------------------------------------------------------------*/
static void
cascading_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
	Widget		submenu;
	Cardinal	num_children;

	XtVaGetValues(w,XmNsubMenuId,&submenu,NULL);

	assert( XfeIsAlive(submenu) );

	XfeChildrenGet(submenu,NULL,&num_children);

    printf("Cascading(%s) Have %d menu items\n\n",XtName(w),num_children);
}
/*----------------------------------------------------------------------*/
static Widget
create_button(Widget parent,String name,Cardinal item_count,Cardinal delay)
{
	Widget		button;
	Cardinal	i;
	char		buf[100];

	static String items[] =
	{
		"Item One",
		"Item Two",
		"Item Three",
		"Item Four",
		"Item Five",
		"Item Six",
		"Item Seven",
		"Item Eight",
		"Item Nine",
		"Item Ten",
	};
	
	button = XtVaCreateManagedWidget(name,
									 xfeCascadeWidgetClass,
									 parent,
									 XmNmappingDelay,		delay,
									 NULL);

	XtAddCallback(button,XmNactivateCallback,activate_callback,NULL);
	XtAddCallback(button,XmNarmCallback,arm_callback,NULL);
	XtAddCallback(button,XmNcascadingCallback,cascading_callback,NULL);
	XtAddCallback(button,XmNdisarmCallback,disarm_callback,NULL);
	XtAddCallback(button,XmNpopdownCallback,popdown_callback,NULL);
	XtAddCallback(button,XmNcascadingCallback,populate_callback,NULL);
	XtAddCallback(button,XmNpopupCallback,popup_callback,NULL);

	add_items(button,item_count);

	return button;
}
/*----------------------------------------------------------------------*/
static void
add_items(Widget parent,Cardinal item_count)
{

	if (item_count > 0)
	{
		Cardinal	i;
		char		buf[100];
		Widget		sub_menu_id = get_sub_menu_id(parent);
		
		assert( XfeIsAlive(sub_menu_id) );

		for (i = 0; i < item_count; i++)
		{
			sprintf(buf,"Item %-2d",i);

			add_item(sub_menu_id,buf);
		}
	}
}
/*----------------------------------------------------------------------*/
static Widget
add_item(Widget parent,String name)
{
	Widget item;

	item = XtVaCreateManagedWidget(name,
								   /*xmPushButtonGadgetClass,*/
								   xmPushButtonWidgetClass,
								   parent,
								   NULL);

	return item;
}
/*----------------------------------------------------------------------*/
static Widget
get_sub_menu_id(Widget w)
{
	Widget sub_menu_id;

	XtVaGetValues(w,XmNsubMenuId,&sub_menu_id,NULL);

	return sub_menu_id;
}
/*----------------------------------------------------------------------*/
