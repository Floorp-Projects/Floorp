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
/* Name:		<Xfe/Draw.h>											*/
/* Description:	Xfe widgets drawing functions header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeDraw_h_								/* start Draw.h			*/
#define _XfeDraw_h_

#include <Xm/Xm.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Drawing functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeDrawRectangle			(Display *		dpy,
							 Drawable		d,
							 GC				gc,
							 Position		x,
							 Position		y,
							 Dimension		width,
							 Dimension		height,
							 Dimension		thickness);
/*----------------------------------------------------------------------*/
extern void
XfeClearRectangle			(Display *		dpy,
							 Drawable		d,
							 Position		x,
							 Position		y,
							 Dimension		width,
							 Dimension		height,
							 Dimension		thickness,
							 Boolean		exposures);
/*----------------------------------------------------------------------*/
extern void
XfeDrawCross				(Display *		dpy,
							 Drawable		d,
							 GC				gc,
							 Position		x,
							 Position		y,
							 Dimension		width,
							 Dimension		height,
							 Dimension		thickness);
/*----------------------------------------------------------------------*/
extern void
XfeDrawArrow				(Display *		dpy,
							 Drawable		d,
							 GC				gc,
							 Position		x,
							 Position		y,
							 Dimension		width,
							 Dimension		height,
							 unsigned char	direction);
/*----------------------------------------------------------------------*/
extern void
XfeDrawMotifArrow			(Display *		dpy,
							 Drawable		d,
							 GC				top_gc,
							 GC				bottom_gc,
							 GC				center_gc,
							 Position		x,
							 Position		y,
							 Dimension		width,
							 Dimension		height,
							 unsigned char	direction,
							 Dimension		shadow_thickness,
							 Boolean		swap);
/*----------------------------------------------------------------------*/
extern void
XfeDrawVerticalLine			(Display *		dpy,
							 Drawable		d,
							 GC				gc,
							 Position		x,
							 Position		y,
							 Dimension		height,
							 Dimension		thickness);
/*----------------------------------------------------------------------*/
extern void
XfeDrawHorizontalLine		(Display *		dpy,
							 Drawable		d,
							 GC				gc,
							 Position		x,
							 Position		y,
							 Dimension		width,
							 Dimension		thickness);
/*----------------------------------------------------------------------*/
extern void
XfeStippleRectangle			(Display *		dpy,
							 Drawable		d,
							 GC				gc,
							 Position		x,
							 Position		y,
							 Dimension		width,
							 Dimension		height,
							 Dimension		step);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Drawable functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeDrawableGetGeometry		(Display *		dpy,
							 Drawable		d,
							 Position *		x_out,
							 Position *		y_out,
							 Dimension *	width_out,
							 Dimension *	height_out);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Shadow functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeDrawShadowsAroundWidget	(Widget			parent,
							 Widget			child,
							 GC				top_gc,
							 GC				bottom_gc,
							 Dimension		offset,
							 Dimension		shadow_thickness,
							 unsigned char	shadow_type);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Draw.h			*/
