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
/* Name:		TaskBarTestOne.c										*/
/* Description:	Test for XfeTaskBar widget.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>


#include <Xm/MwmUtil.h>


#include <X11/Shell.h>

#include <Xfe/XfeP.h>


static String names[] =
{
    "One",
    "Two",
    "Three",
    "Four",
    "Five",
    "Six"
};

static String icons_large[] =
{
	"icons/bar_web.xpm",
    "icons/bar_news.xpm",
    "icons/bar_mail.xpm",
	"icons/bar_web.xpm",
    "icons/bar_news.xpm",
    "icons/bar_mail.xpm",
};

#define LOGO_PIXMAP		"icons/logo.xpm"
#define DRAG_PIXMAP		"icons/drag_tile.xpm"
#define UNDOCK_PIXMAP	"icons/undock.xpm"

static void		undock_callback			(Widget,XtPointer,XtPointer);
static void		dock_callback			(Widget,XtPointer,XtPointer);
static void		tool_active_callback	(Widget,XtPointer,XtPointer);

static Widget	create_docked_task_bar	(String,Widget);
static Widget	create_foating_task_bar	(String,Widget);

static Widget	_floating_tb = NULL;
static Widget	_floating_shell = NULL;
static Widget	_docked_tb = NULL;

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	form;
	Widget	frame;

	XfeAppCreateSimple("TaskBarTestOne",&argc,argv,"MainFrame",&frame,&form);

    _docked_tb = create_docked_task_bar("DockedTaskBar",form);

    _floating_tb = create_foating_task_bar("FloatingShell",_docked_tb);

    XtAddCallback(_docked_tb,XmNactionCallback,undock_callback,NULL);


#if 0
    XtAddCallback(_floating_tb,XmNactionCallback,dock_callback,NULL);
#else
    XtAddCallback(_floating_tb,XmNcloseCallback,dock_callback,NULL);
#endif

	XtManageChild(_docked_tb);

	XtPopup(frame,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_docked_task_bar(String name,Widget parent)
{
    Widget		tb;
    Cardinal	i;
    Pixmap		action_pixmap;

    tb = XtVaCreateWidget(name,
						  xfeTaskBarWidgetClass,
						  parent,
						  XmNshowActionButton, True,
						  NULL);

    action_pixmap = XfeGetPixmapFromFile(tb,UNDOCK_PIXMAP);

    XtVaSetValues(tb,
				  XmNactionPixmap,	action_pixmap,
				  NULL);

    for (i = 0; i < XtNumber(names); i++)
    {
		Widget w;
		Pixmap p_normal;
		Pixmap p_raised;
		Pixmap p_armed;
		
		w = XtVaCreateManagedWidget(names[i],
									xfeButtonWidgetClass,
									tb,
									NULL);
	
		p_normal = XfeGetPixmapFromFile(w,icons_large[i]);
		p_raised = XfeGetPixmapFromFile(w,icons_large[i]);
		p_armed  = XfeGetPixmapFromFile(w,icons_large[i]);
		
		XtVaSetValues(w,
					  XmNpixmap,		p_normal,
					  XmNraisedPixmap,	p_raised,
					  XmNarmedPixmap,	p_armed,
					  NULL);

		XtAddCallback(w,XmNactivateCallback,tool_active_callback,NULL);
    }

	return tb;
}
/*----------------------------------------------------------------------*/
static Widget
create_foating_task_bar(String name,Widget parent)
{
    Widget		tb;
    Pixmap		action_pixmap;
    Pixmap		drag_pixmap;

	int			mask;
	int			mode;
	int			func;

	mask = MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MENU;
	mode = MWM_INPUT_PRIMARY_APPLICATION_MODAL;
	func = MWM_FUNC_CLOSE | MWM_FUNC_MOVE;

/*	mask = 0;*/

    _floating_shell = 
		XtVaCreatePopupShell(name,
							 xmDialogShellWidgetClass,
							 XfeAncestorFindTopLevelShell(parent),
							 XmNmwmDecorations,			mask,
							 XmNmwmInputMode,			mode,
							 XmNmwmFunctions,			func,
							 XmNallowShellResize,		True,
							 XmNdeleteResponse,			XmDO_NOTHING,
							 NULL);

    tb = create_docked_task_bar("FloatingTaskBar",_floating_shell);

	XtVaSetValues(tb,XmNshowActionButton,False,NULL);

    action_pixmap = XfeGetPixmapFromFile(tb,LOGO_PIXMAP);
    drag_pixmap = XfeGetPixmapFromFile(tb,DRAG_PIXMAP);

#if 0	
    XtVaSetValues(tb,
				  XmNactionPixmap,	action_pixmap,
				  XmNdragPixmap,	drag_pixmap,
				  NULL);
#endif

	return tb;
}
/*----------------------------------------------------------------------*/
static void
tool_active_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
	/* I want more siblings */
	XtVaCreateManagedWidget("New Task",
							xfeButtonWidgetClass,
							XtParent(w),
							NULL);
}
/*----------------------------------------------------------------------*/
static void
undock_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
	static first_map = True;

	XtUnmanageChild(_docked_tb);

#if 0
	XtResizeWidget(_floating_shell,
				   _XfeWidth(_floating_tb),
				   _XfeHeight(_floating_tb),
				   0);
#endif

	if (first_map)
	{
		Widget		top_shell = XfeAncestorFindTopLevelShell(_docked_tb);

		Position	x = 
			XfeRootX(top_shell) + 
			XfeWidth(top_shell) - 
			XfeWidth(_floating_tb) - 
			0;

		Position	y = 
			XfeRootY(top_shell) + 
			XfeHeight(top_shell) - 
			XfeHeight(_floating_tb) - 
			0;

		XtVaSetValues(_floating_shell,
					  XmNx,		x,
					  XmNy,		y,
					  NULL);

		first_map = False;
	}

	XtManageChild(_floating_tb);
}
/*----------------------------------------------------------------------*/
static void
dock_callback(Widget w,XtPointer client_data,XtPointer call_data)
{
	XtManageChild(_docked_tb);

	XtUnmanageChild(_floating_tb);
}
/*----------------------------------------------------------------------*/
