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
/* Name:		<Xfe/WmUtil.c>											*/
/* Description:	Window manager utilities source.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* OpenLook																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeIsOpenLookRunning(Widget w)
{
	Widget			app_shell = XfeAncestorFindApplicationShell(w);
	Display *		dpy = XtDisplay(app_shell);
	int				result;
	Atom			actual_type = 0;
	int				actual_format = 0;
	unsigned long	nitems = 0;
	unsigned long	bytes_after = 0;
	unsigned char *	data = 0;
	
	Atom proto_atom = XInternAtom(dpy, "_SUN_WM_PROTOCOLS", True);

    /* If the atom isn't interned, OLWM can't be running. */
	if (proto_atom == None)
	{
		return False;
	}

	result=XGetWindowProperty(dpy,
							  RootWindowOfScreen(DefaultScreenOfDisplay(dpy)),
							  proto_atom,
							  0, 0,            /* read 0 bytes */
							  False,           /* don't delete */
							  AnyPropertyType, /* type expected */
							  &actual_type,    /* returned values */
							  &actual_format,
							  &nitems,
							  &bytes_after,
							  &data);
	
	/* At most, this will be 1 allocated byte. */
	if (data)
	{
		XtFree(data);
	}
		
	/* If any of these are true, then OLWM isn't running. */
	if (result != Success ||            /* no such property */
		actual_type != XA_ATOM ||       /* didn't contain type Atom */
		bytes_after <= 0)               /* zero length */
	{
		return False;
	}
	
	/*
	 * Otherwise, it sure looks like OLWM is running.  If it's not, 
	 * then it's some other program that is imitating it well. 
	 */
	
	return True;
}
/*----------------------------------------------------------------------*/
