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
/* Name:		<Xfe/ShellUtil.h>										*/
/* Description:	Shell misc utilities header.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeShellUtil_h_						/* start ShellUtil.h	*/
#define _XfeShellUtil_h_

#include <X11/Intrinsic.h>						/* Xt public defs		*/

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Shell																*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeShellIsPoppedUp					(Widget			shell);
/*----------------------------------------------------------------------*/
extern void
XfeShellAddCloseCallback			(Widget			shell,
									 XtCallbackProc	callback,
									 XtPointer		data);
/*----------------------------------------------------------------------*/
extern void
XfeShellRemoveCloseCallback			(Widget			shell,
									 XtCallbackProc	callback,
									 XtPointer		data);
/*----------------------------------------------------------------------*/
extern Boolean
XfeShellGetDecorationOffset			(Widget			shell,
									 Position *		x_out,
									 Position *		y_out);
/*----------------------------------------------------------------------*/
extern int
XfeShellGetGeometryFromResource		(Widget			shell,
									 Position *		x_out,
									 Position *		y_out,
									 Dimension *	width_out,
									 Dimension *	height_out);
/*----------------------------------------------------------------------*/
extern void
XfeShellSetIconicState				(Widget			shell,
									 Boolean		state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeShellGetIconicState				(Widget			shell);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Shell placement														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeShellPlaceAtLocation				(Widget			shell,
									 Widget			relative,
									 unsigned char	location,
									 Dimension		dx,
									 Dimension		dy);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ShellUtil.h		*/
