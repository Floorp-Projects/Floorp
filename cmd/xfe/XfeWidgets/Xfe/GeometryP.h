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
/* Name:		<Xfe/GeometryP.h>										*/
/* Description:	Xfe geometry functions private header.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeGeometryP_h_						/* start GeometryP.h	*/
#define _XfeGeometryP_h_

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

#include <Xm/XmP.h>
#include <Xfe/Geometry.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeGeometry															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	int x;
	int y;
	int width;
	int height;
} XfeGeometryRec,*XfeGeometry;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDimensions														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	int width;
	int height;
} XfeDimensionsRec,*XfeDimensions;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePosition															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	int x;
	int y;
} XfePositionRec,*XfePosition;

/*----------------------------------------------------------------------*/
/*																		*/
/* Geometry defines														*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeGEOMETRY_INVALID_DIMENSION -1

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc																	*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Dimension
_XfeHeightCenter				(Widget			one,
								 Widget			two);
/*----------------------------------------------------------------------*/
extern Dimension
_XfeWidthCenter					(Widget			one,
								 Widget			two);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Geometry																*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void						
_XfeConfigureWidget				(Widget			w,
								 int			x,
								 int			y,
								 int			width,
								 int			height);
/*----------------------------------------------------------------------*/
extern void						
_XfeConfigureOrHideWidget		(Widget			w,
								 int			x,
								 int			y,
								 int			width,
								 int			height);
/*----------------------------------------------------------------------*/
extern void
_XfeResizeWidget				(Widget			w,
								 int			width,
								 int			height);
/*----------------------------------------------------------------------*/
extern void
_XfeMoveWidget					(Widget			w,
								 int			x,
								 int			y);
/*----------------------------------------------------------------------*/
extern void
_XfePreferredGeometry			(Widget			w,
								 Dimension *	width_out,
								 Dimension *	height_out);
/*----------------------------------------------------------------------*/
extern void
_XfeMinimumGeometry				(Widget			w,
								 Dimension *	width_out,
								 Dimension *	height_out);
/*----------------------------------------------------------------------*/


#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end GeometryP.h		*/
