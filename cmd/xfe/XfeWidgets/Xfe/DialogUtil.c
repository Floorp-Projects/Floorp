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
/* Name:		<Xfe/DialogUtil.h>										*/
/* Description:	Dialog utilities source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>
#include <Xfe/ManagerP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Dialogs																*/
/*																		*/
/*----------------------------------------------------------------------*/
/*
 *    From the Motif FAQ:
 *
 *    217) How can I force a dialog window to display?
 *
 *    I manage a "working" dialog, and do some computing, but the dialog window
 *    appears blank until the work has finished.  How can I force it to be
 *    displayed?
 *
 *    [Last modified: Dec '94]
 *
 *    Answer:  David Brooks <dbrooks@ics.com> writes: The dialog window won't
 *    get expose events until the window manager has fielded the map
 *    request, done the reparenting with all that entails, and finally
 *    convinced the server that the window is for real.  The safe way of
 *    doing it is [below].
 *
 *    Use this.  (David Brooks, Systems Engineering, Open Software Foundation)
 */

/*
 * This procedure will ensure that, if a dialog window is being mapped,
 * its contents become visible before returning.  It is intended to be
 * used just before a bout of computing that doesn't service the display.
 * You should still call XmUpdateDisplay() at intervals during this
 * computing if possible.
 *
 * The monitoring of window states is necessary because attempts to map
 * the dialog are redirected to the window manager (if there is one) and
 * this introduces a significant delay before the window is actually mapped
 * and exposed.  This code works under mwm, twm, uwm, and no-wm.  It
 * doesn't work (but doesn't hang) with olwm if the mainwindow is iconified.
 *
 * The argument to ForceDialog is any widget in the dialog (often it
 * will be the BulletinBoard child of a DialogShell).
 */
/* extern */ void
XfeUpdateDisplay(Widget shell)
{
	Widget diashell, topshell;
	Window diawindow, topwindow;
	Display *dpy;
	XWindowAttributes xwa;
	XEvent event;
	XtAppContext cxt;

	assert( _XfeIsAlive(shell) );

	/* Locate the shell we are interested in.  In a particular instance, you
	 * may know these shells already.
	 */

	for (diashell = shell;
		 !XtIsShell(diashell);
		 diashell = XtParent(diashell))
		;

	/* Locate its primary window's shell (which may be the same) */

	for (topshell = diashell;
		 !XtIsTopLevelShell(topshell);
		 topshell = XtParent(topshell))
		;

	if (XtIsRealized(diashell) && XtIsRealized(topshell)) {
		dpy = XtDisplay(topshell);
		diawindow = _XfeWindow(diashell);
		topwindow = _XfeWindow(topshell);
		cxt = XtWidgetToApplicationContext(diashell);

		/* Wait for the dialog to be mapped. 
		   It's guaranteed to become so unless... */

		while (XGetWindowAttributes(dpy, diawindow, &xwa),
			   xwa.map_state != IsViewable) {

			/* ...if the primary is (or becomes) unviewable or unmapped, it's
			   probably iconified, and nothing will happen. */

			if (XGetWindowAttributes(dpy, topwindow, &xwa),
				xwa.map_state != IsViewable)
				break;

			/* At this stage, we are guaranteed there will be an event of
			   some kind.
			   Beware; we are presumably in a callback, so this can recurse. */

			XtAppNextEvent(cxt, &event);
			XtDispatchEvent(&event);
		}
	}

	/* The next XSync() will get an expose event if the dialog was unmapped. */

	XmUpdateDisplay(topshell);
}

/*----------------------------------------------------------------------*/
/*																		*/
/* Find the parent dialog for a widget									*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeGetParentDialog(Widget w)
{
	Widget mainw = w;


	int i;
	/* If any modal dialog is already up we will cascade these dialogs. Thus this
	 * dialog will be the child of the last popped up modal dialog.
	 */
	i = _XfeNumPopups(mainw);

	while (i>0)
	{
		Widget popup = _XfePopupListIndex(mainw,i - 1);
		
		if ( XmIsDialogShell(popup) && 
			 XfeShellIsPoppedUp(popup) &&
			 (_XfemNumChildren(popup) > 0) )
		{
			Widget			newmainw = popup;
			Widget			popup_child = _XfeChildrenIndex(popup,0);
			unsigned char	dialog_style = XmDIALOG_MODELESS;
			
			XtVaGetValues(popup_child,
						  XmNdialogStyle, &dialog_style,
						  NULL);
			
			if (dialog_style != XmDIALOG_MODELESS)
			{
				mainw = newmainw;
				i = _XfeNumPopups(mainw);
			}
			else
			{
				i --;
			}
		}
		else
		{
			i--;
		}
	}
	
	return mainw;
}
/*----------------------------------------------------------------------*/
