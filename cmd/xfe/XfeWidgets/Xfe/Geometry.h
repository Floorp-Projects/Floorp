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
/* Name:		<Xfe/Geometry.h>										*/
/* Description:	Xfe geometry public functions header.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeGeometry_h_							/* start Geometry.h		*/
#define _XfeGeometry_h_

#include <Xm/Xm.h>								/* Motif public defs	*/

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

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
XfeVaGetWidestWidget			(Widget			widget,
								 ...);
/*----------------------------------------------------------------------*/
extern Dimension
XfeVaGetTallestWidget			(Widget			widget,
								 ...);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Geometry.h		*/
