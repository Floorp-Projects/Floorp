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
/* Name:		DashBoardTest.c											*/
/* Description:	Test for XfeDashBoard widget.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include <Xfe/XfeTest.h>

static Widget	create_frame_shell			(Widget,String);

int
main(int argc,char *argv[])
{
    Widget	frame1;
    Widget	frame2;
    Widget	frame3;
    Widget	frame4;
    
	XfeAppCreate("DashBoardTest",&argc,argv);
    
	frame1 = create_frame_shell(XfeAppShell(),"Frame1");

	frame2 = create_frame_shell(XfeAppShell(),"Frame2");

	frame3 = create_frame_shell(XfeAppShell(),"Frame3");

	frame4 = create_frame_shell(XfeAppShell(),"Frame4");

	XtPopup(frame1,XtGrabNone);

	XtPopup(frame2,XtGrabNone);

	XtPopup(frame3,XtGrabNone);

	XtPopup(frame4,XtGrabNone);

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_frame_shell(Widget parent,String name)
{
    Widget	frame;
	Widget	form;
    Widget	docked_task_bar;
    Widget	dash_board;

	static Widget	floating_shell = NULL;
	static Boolean	first_instance = True;

    frame = XtVaCreatePopupShell(name,
								 xfeFrameShellWidgetClass,
								 parent,
								 NULL);

	XfeAddEditresSupport(frame);

    form = XtVaCreateManagedWidget("Form",
								   xmFormWidgetClass,
								   frame,
								   NULL);

	if (first_instance)
	{
		int			mask;
		int			func;
		
		
		Widget floating_task_bar;
		
		first_instance = False;
		
		mask = MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MENU;
		func = MWM_FUNC_CLOSE | MWM_FUNC_MOVE;

		floating_shell =
			XtVaCreatePopupShell("FloatingShell",
								 xmDialogShellWidgetClass,
								 XfeAncestorFindApplicationShell(parent),
								 XmNvisual,				XfeVisual(parent),
								 XmNcolormap,			XfeColormap(parent),
								 XmNdepth,				XfeDepth(parent),
								 XmNmwmDecorations,		mask,
								 XmNmwmFunctions,		func,
								 XmNallowShellResize,	True,
								 XmNdeleteResponse,		XmDO_NOTHING,
								 NULL);

		
		floating_task_bar = XfeCreateLoadedTaskBar(floating_shell,
												   "FloatingTaskBar",
												   True,
												   "Task_",
												   NULL,
												   4,
												   NULL);
	}

    dash_board = XtVaCreateManagedWidget("DashBoard",
										 xfeDashBoardWidgetClass,
										 form,
										 NULL);

	XtAddCallback(dash_board,XmNdockCallback,XfeDockCallback,NULL);
	XtAddCallback(dash_board,XmNundockCallback,XfeUndockCallback,NULL);

    XtVaSetValues(dash_board,
				  XmNfloatingShell,	floating_shell,
				  NULL);
	
	/* Create the progress bar */
	XtVaCreateManagedWidget("ProgressBar",
							xfeProgressBarWidgetClass,
							dash_board,
							XmNusePreferredHeight,		True,
							XmNusePreferredWidth,		True,
							XmNshadowThickness,			1,
							XmNshadowType,				XmSHADOW_IN,
							XmNhighlightThickness,		0,
							XmNtraversalOn,				False,
							NULL);

    /* Create the status bar */
	XtVaCreateWidget("StatusBar",
					 xfeLabelWidgetClass,
					 dash_board,
					 XmNusePreferredHeight,		False,
					 XmNusePreferredWidth,		True,
					 XmNshadowThickness,		2,
					 XmNshadowType,				XmSHADOW_IN,
					 XmNhighlightThickness,		0,
					 XmNtraversalOn,			False,
					 NULL);
	
    /* Create the task bar */
	docked_task_bar = XfeCreateLoadedTaskBar(dash_board,
											 "DockedTaskBar",
											 False,
											 "T",
											 NULL,
											 4,
											 NULL);

	XtVaSetValues(docked_task_bar,
				  XmNorientation,			XmHORIZONTAL,
				  XmNusePreferredWidth,		True,
				  XmNusePreferredHeight,	True,
				  XmNhighlightThickness,	0,
				  XmNshadowThickness,		0,
				  XmNtraversalOn,			False,
				  NULL);
	
	return frame;
}
/*----------------------------------------------------------------------*/
