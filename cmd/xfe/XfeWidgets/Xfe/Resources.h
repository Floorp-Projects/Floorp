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
/* Name:		<Xfe/Resources.h>										*/
/* Description:	Xfe widgets resources utilities.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeResources_h_						/* start Resources.h	*/
#define _XfeResources_h_

#include <Xm/Xm.h>								/* Motif public defs	*/

XFE_BEGIN_CPLUSPLUS_PROTECTION

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

/*----------------------------------------------------------------------*/
/*																		*/
/* Obtain a single resource directly from a widget.						*/
/*																		*/
/*----------------------------------------------------------------------*/
extern String
XfeSubResourceGetWidgetStringValue	(Widget		w,
									 String		resource_name,
									 String		resource_class);
/*----------------------------------------------------------------------*/
extern XmString
XfeSubResourceGetWidgetXmStringValue(Widget		w,
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

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end Resources.h		*/
