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
/* Name:		<Xfe/Resources.h>										*/
/* Description:	Xfe widgets resources utilities.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeResources_h_						/* start Resources.h	*/
#define _XfeResources_h_

#include <Xm/Xm.h>								/* Motif public defs	*/

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Read a single value from resource database.							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeSubResourceGetBooleanValue		(Widget		parent,
									 String		widget_name,
									 String		widget_class_name,
									 String		resource_name,
									 String		resource_class,
									 Boolean	default_value);
/*----------------------------------------------------------------------*/
extern XmString
XfeSubResourceGetXmStringValue		(Widget		parent,
									 String		widget_name,
									 String		widget_class_name,
									 String		resource_name,
									 String		resource_class,
									 XmString	default_value);
/*----------------------------------------------------------------------*/
extern String
XfeSubResourceGetStringValue		(Widget		parent,
									 String		widget_name,
									 String		widget_class_name,
									 String		resource_name,
									 String		resource_class,
									 String		default_value);
/*----------------------------------------------------------------------*/
extern Pixel
XfeSubResourceGetPixelValue			(Widget		parent,
									 String		widget_name,
									 String		widget_class,
									 String		resource_name,
									 String		resource_class,
									 Pixel		default_value);
/*----------------------------------------------------------------------*/
extern String
XfeSubResourceGetWidgetStringValue	(Widget		w,
									 String		resource_name,
									 String		resource_class);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Check whether a child is enabled.									*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeChildIsEnabled					(Widget		parent,
									 String		child_name,
									 String		child_class,
									 Boolean	default_value);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Resources.h		*/
