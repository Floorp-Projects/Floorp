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
/* Name:		<Xfe/Private.c>											*/
/* Description:	Xfe widgets private utilities.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>

#include <Xfe/XfeP.h>

#define MESSAGE1 "%s needs to have the same depth as the widget."
#define MESSAGE2 "Cannot obtain geometry info for %s."

/*----------------------------------------------------------------------*/
/*																		*/
/* Callbacks															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeInvokeCallbacks(Widget			w,
					XtCallbackList	list,
					int				reason,
					XEvent *		event,
					Boolean			flush_display)
{
	/* Make sure widget is alive and callback list is not NULL */
	if (_XfeIsAlive(w) && list)
	{
		XmAnyCallbackStruct cbs;
		
		cbs.event 	= event;
		cbs.reason	= reason;

		/* Flush the display before invoking callback if needed */
		if (flush_display)
		{
			XFlush(XtDisplay(w));
		}

		/* Invoke the Callback List */
		XtCallCallbackList(w,list,&cbs);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeSimpleWarning(Widget w,String warning)
{
	static char buf[2048];

	sprintf(buf,
			"\n\t%s: %s\n\t%s: %s\n\t%s\n",
			"Name",XtName(w),
			"Class",w->core.widget_class->core_class.class_name,
			warning);

	XtAppWarning(XtWidgetToApplicationContext(w),buf);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeExtraWarning(Widget		w,
				 String		warning,
				 String		filename,
				 Cardinal	lineno)
{
	static char buf[2048];
	static char line_buf[32];
	static char window_buf[32];

	if (lineno)
	{
		sprintf(line_buf,"%d",lineno);
	}
	else
	{
		sprintf(line_buf,"%s","unknown");
	}

	if (XtIsRealized(w))
	{
		Widget windowed = XfeWindowedWidget(w);

		if (_XfeWindow(windowed) && (_XfeWindow(windowed) != None))
		{
			if (w == windowed)
			{
				sprintf(window_buf,"0x%x",(int) _XfeWindow(windowed));
			}
			else
			{
				sprintf(window_buf,"0x%x (parent's)",(int) _XfeWindow(windowed));
			}
		}
		else
		{
			sprintf(window_buf,"%s","invalid");
		}

		
	}
	else
	{
		sprintf(window_buf,"%s","unrealized");
	}

	sprintf(buf,
			"\n  %-14s %s\n  %-14s %s\n  %-14s %s\n  %-14s %s\n  %-14s %s\n  %s",
			"Filename:",		filename ? filename : "unknown",
			"Line Number:",		line_buf,
			"Widget Name:",		XtName(w),
			"Widget Class:",	w->core.widget_class->core_class.class_name,
			"Widget Window:",	window_buf,
			warning);

	XtAppWarning(XtWidgetToApplicationContext(w),buf);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeArgumentWarning(Widget		w,
					String		format,
					XtPointer	argument,
					String		filename,
					Cardinal	lineno)
{
	static char buf[2048];

	sprintf(buf,format,argument);

	_XfeExtraWarning(w,buf,filename,lineno);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Actions																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XtActionProc
_XfeGetActionProc(WidgetClass wc,String name)
{
	XtActionProc	proc = NULL;
	XtActionsRec *	action_list = NULL;
	Cardinal		num_actions;
	Cardinal		i;

	assert( wc != NULL );

	XtGetActionList(wc,&action_list,&num_actions);

	if (action_list && num_actions)
	{
		for(i = 0; i < num_actions; i++)
		{
			if (strcmp(action_list[i].string,name) == 0)
			{
				proc = action_list[i].proc;
			}
		}

		XtFree((char *) action_list);
	}

	return proc;
}
/*----------------------------------------------------------------------*/
