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
/* Name:		ProgressBarTest.c										*/
/* Description:	Test for XfeProgressBar widget.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

static void	StartSweep		(Widget,int);
static void	SweepTimeout	(XtPointer,XtIntervalId *);

static void	StartMessages	(Widget,int);
static void	MessageTimeout	(XtPointer,XtIntervalId *);

#define SWEEP_INTERVAL		100
#define MESSAGE_INTERVAL	500

#define MAX_PERCENT		100
#define MIN_PERCENT		0

static String _messages[] = 
{
    "A News error occurred",
    "If you are unable to connect again later, contact",
    "If you are unable to connect again, contact the",
    "Netscape is unable to complete a socket connection",
    "Netscape is unable to complete a socket connection ",
    "Netscape is unable to connect to the server at",
    "Netscape is unable to connect to your proxy server.",
    "Netscape refuses to connect to this server.",
    "Netscape was unable to connect to the SMTP server.",
    "Netscape was unable to create a network socket connection.",
    "Netscape was unble to connect to the secure news server",
    "Netscape's network connection was refused by the server",
    "Please try your connection again)",
    "The server has disconnected.",
    "The server may not be accepting connections or the end of the world is upon us very soon.",
    "Try connecting again later or try restarting Netscape.",
    "Try connecting again later or try restarting Netscape. You can try to connect later if you like.",
    "Try connecting again later.",
    "Try connecting again.",
    "You will not be able to connect to this site securely.",
    "You will probably be unable to connect to this site securely, or you can risk being stolen from.",
    "server to establish a data connection.",
    "unable to connect to server (TCP Error"
};

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	form;
	Widget	frame;
    Widget	status_form;
    Widget	status_frame;

    Widget	progress_bar;
    Widget	status_bar;
    Widget	task_bar;

	XfeAppCreateSimple("ProgressBarTest",&argc,argv,"MainFrame",&frame,&form);
    
    status_frame = XfeCreateManagedFrame(form,"StatusFrame",NULL,0);

    status_form = XfeCreateManagedForm(status_frame,"StatusForm",NULL,0);

    task_bar = XtCreateManagedWidget("TaskBar",
									 xmPushButtonWidgetClass,
									 status_form,NULL,0);

    progress_bar = 
		XtVaCreateManagedWidget("ProgressBar",
								xfeProgressBarWidgetClass,
								status_form,
								XmNrightWidget,		task_bar,
								NULL);
	
    status_bar = 
		XtVaCreateWidget("StatusBar",
						 xfeLabelWidgetClass,
						 status_form,
						 XmNrightWidget,	progress_bar,
						 XmNtruncateProc,	XfeGetTruncateXmStringProc(),
						 XmNtruncateLabel,	True,
						 XmNshadowType,		XmSHADOW_IN,
						 NULL);

	XtManageChild(status_bar);
	
    StartSweep(progress_bar,SWEEP_INTERVAL);

    StartMessages(status_bar,MESSAGE_INTERVAL);

	XtPopup(frame,XtGrabNone);
    
    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static void
StartSweep(Widget w,int interval)
{
    XfeAppAddTimeOut(interval,SweepTimeout,(XtPointer) w);
}
/*----------------------------------------------------------------------*/
static void
SweepTimeout(XtPointer client_data,XtIntervalId * id)
{
    Widget		w = (Widget) client_data;
    static Boolean	direction = True;
    static int		percent = 0;
    
    char		buf[2048];
    XmString		xm_label = NULL;

    if (direction)
    {
	percent++;
	
	if (percent > MAX_PERCENT)
	{
	    percent = MAX_PERCENT;
	    direction = False;
	}
    }
    else
    {
	percent--;
	
	if (percent < MIN_PERCENT)
	{
	    percent = MIN_PERCENT;
	    direction = True;
	}
    }

    sprintf(buf,"%d%% of 1234k",percent);

    xm_label = XmStringCreateLocalized(buf);

    XfeProgressBarSetComponents(w,xm_label,0,percent);

    if (xm_label)
    {
	XmStringFree(xm_label);
    }

    StartSweep(w,SWEEP_INTERVAL);
}
/*----------------------------------------------------------------------*/
static void
StartMessages(Widget w,int interval)
{
    XfeAppAddTimeOut(interval,MessageTimeout,(XtPointer) w);
}
/*----------------------------------------------------------------------*/
static void
MessageTimeout(XtPointer client_data,XtIntervalId * id)
{
    Widget		w = (Widget) client_data;
    static int	i = 0;
    
    char		buf[2048];

	if (!(i % 2))
	{
		sprintf(buf,"%s",_messages[i++ % XtNumber(_messages)]);
	}
	else
	{
		buf[0] = '\0';
		i++;
	}

/*	XfeProgressBarSetLabelPSZ(w,buf);*/
	XfeLabelSetStringPSZ(w,buf);
    
    StartMessages(w,MESSAGE_INTERVAL);
}
/*----------------------------------------------------------------------*/
