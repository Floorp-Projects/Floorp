/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/Geometry.h>										*/
/* Description:	Xfe geometry public functions header.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeGeometry_h_							/* start Geometry.h		*/
#define _XfeGeometry_h_

#include <Xfe/BasicDefines.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* Access functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Dimension	XfeBorderWidth		(Widget w);
extern Dimension	XfeWidth			(Widget w);
extern Dimension	XfeHeight			(Widget w);
extern Dimension	XfeScreenWidth		(Widget w);
extern Dimension	XfeScreenHeight		(Widget w);
extern Position		XfeX				(Widget w);
extern Position		XfeY				(Widget w);
extern Position		XfeRootX			(Widget w);
extern Position		XfeRootY			(Widget w);

/*----------------------------------------------------------------------*/
/*																		*/
/* Biggest, widest and tallest functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeBiggestWidget				(Boolean		horizontal,
								 WidgetList		widgets,
								 Cardinal		n);
/*----------------------------------------------------------------------*/
extern Dimension
XfeVaGetWidestWidget			(Widget			w,
								 ...);
/*----------------------------------------------------------------------*/
extern Dimension
XfeVaGetTallestWidget			(Widget			w,
								 ...);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeMoveChildrenByOffset	Move all children of a manager by an 		*/
/*							(x,y) offset.								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeMoveChildrenByOffset			(Widget			w,
								 int			x_offset,
								 int			y_offset);
/*----------------------------------------------------------------------*/


XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Geometry.h		*/
