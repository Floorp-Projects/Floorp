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
/* Name:		<XfeTest/TestCallbacks.c>								*/
/* Description:	Xfe widget tests callbacks.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

/*----------------------------------------------------------------------*/
/*extern */ void
XfeActivateCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Activate(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeArmCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Arm(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeDisarmCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Disarm(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeLowerCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Lower(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeRaiseCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Raise(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeEnterCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Enter(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeLeaveCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Leave(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeMapCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Map(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeUnmapCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Unmap(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeDockCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Dock(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeUndockCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Undock(%s)\n\n",XtName(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeExitCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	printf("Exit(%s)\n\n",XtName(w));

	XtCloseDisplay(XtDisplay(w));

	XtDestroyWidget(XfeAncestorFindApplicationShell(w));

	exit(0);
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeDestroyCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	Widget target = (Widget) client_data;

	printf("Destroy(%s)\n\n",XtName(target));

	XtDestroyWidget(target);
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeResizeCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	assert( XfeIsAlive(w) );

	printf("Resize(%s) to (%d,%d)\n\n",XtName(w),XfeWidth(w),XfeHeight(w));
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeLabelSelectionChangedCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	XfeLabelSelectionChangedCallbackStruct * data = 
		(XfeLabelSelectionChangedCallbackStruct *) call_data;

	assert( XfeIsAlive(w) );

	printf("SelectionChanged(%s,%s)\n\n",
		   XtName(w),data->selected ? "True" : "False");
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeFreeDataCallback(Widget w,XtPointer client_data,XtPointer call_data)
{
	if (client_data)
	{
		XtFree((char *) client_data);
	}
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeToolBoxOpenCallback(Widget w,XtPointer clien_data,XtPointer call_data)
{
	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) call_data;
	
	printf("XfeToolBoxOpenCallback(tb = %s,item = %s,i = %d)\n",
		   XtName(w),
		   XtName(cbs->item),
		   cbs->index);
}
/*----------------------------------------------------------------------*/
/*extern */ void
XfeToolBoxCloseCallback(Widget w,XtPointer clien_data,XtPointer call_data)
{
	XfeToolBoxCallbackStruct *	cbs = (XfeToolBoxCallbackStruct *) call_data;
	
	printf("XfeToolBoxCloseCallback(tb = %s,item = %s,i = %d)\n",
		   XtName(w),
		   XtName(cbs->item),
		   cbs->index);
}
/*----------------------------------------------------------------------*/
