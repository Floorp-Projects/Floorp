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
/* Name:		<Xfe/Resources.c>										*/
/* Description:	Xfe widgets resources utilities.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>

#define XmNisEnabled "isEnabled"
#define XmCIsEnabled "IsEnabled"

/*----------------------------------------------------------------------*/
/*																		*/
/* Private functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtPointer
SubResourceGetValue			(Widget		parent,
							 String		widget_name,
							 String		widget_class,
							 String		resource_name,
							 String		resource_class,
							 String		resource_type,
							 Cardinal	resource_size,
							 XtPointer	default_addr);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Public functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeSubResourceGetBooleanValue(Widget	parent,
							  String	widget_name,
							  String	widget_class,
							  String	resource_name,
							  String	resource_class,
							  Boolean	default_value)
{
	XtPointer value;
	XtPointer default_ptr = default_value ? (XtPointer)True : (XtPointer)False;

	value = SubResourceGetValue(parent,
								widget_name,
								widget_class,
								resource_name,
								resource_class,
								XmRBoolean,
								sizeof(Boolean),
								default_ptr);

	return (value != NULL) ? True : False;
}
/*----------------------------------------------------------------------*/
/* extern */ XmString
XfeSubResourceGetXmStringValue(Widget	parent,
							   String	widget_name,
							   String	widget_class,
							   String	resource_name,
							   String	resource_class,
							   XmString	default_value)
{
	return (XmString) SubResourceGetValue(parent,
										  widget_name,
										  widget_class,
										  resource_name,
										  resource_class,
										  XmRXmString,
										  sizeof(XmString),
										  (XtPointer) default_value);
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfeSubResourceGetStringValue(Widget		parent,
							 String		widget_name,
							 String		widget_class,
							 String		resource_name,
							 String		resource_class,
							 String		default_value)
{
	return (String) SubResourceGetValue(parent,
										widget_name,
										widget_class,
										resource_name,
										resource_class,
										XmRString,
										sizeof(String),
										(XtPointer) default_value);
}
/*----------------------------------------------------------------------*/
/* extern */ Pixel
XfeSubResourceGetPixelValue(Widget		parent,
							String		widget_name,
							String		widget_class,
							String		resource_name,
							String		resource_class,
							Pixel		default_value)
{
	/* Pixel default_pixel = WhitePixelOfScreen(_XfeScreen(parent)); */

	return (Pixel) SubResourceGetValue(parent,
									   widget_name,
									   widget_class,
									   resource_name,
									   resource_class,
									   XmRPixel,
									   sizeof(Pixel),
									   (XtPointer) default_value);
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfeSubResourceGetWidgetStringValue(Widget		w,
								   String		resource_name,
								   String		resource_class)
{
	return XfeSubResourceGetStringValue(_XfeParent(w),
										XtName(w),
										XfeClassNameForWidget(w),
										resource_name,
										resource_class,
										NULL);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Check whether a child is enabled.									*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeChildIsEnabled(Widget	parent,
				  String	child_name,
				  String	child_class,
				  Boolean	default_value)
{
	return XfeSubResourceGetBooleanValue(parent,
										 child_name,
										 child_class,
										 XmNisEnabled,
										 XmCIsEnabled,
										 default_value);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Private functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtPointer
SubResourceGetValue(Widget		parent,
					String		widget_name,
					String		widget_class,
					String		resource_name,
					String		resource_class,
					String		resource_type,
					Cardinal	resource_size,
					XtPointer	default_addr)
{
	static XtResource	_resource;

	XtPointer			value = NULL;

    _resource.resource_name		= resource_name;
    _resource.resource_class	= resource_class;
    _resource.resource_type		= resource_type;
    _resource.resource_size		= resource_size;
    _resource.resource_offset	= 0;
    _resource.default_type		= XtRImmediate;
    _resource.default_addr		= default_addr;

    XtGetSubresources(parent,				/* w				*/
					  &value,				/* base				*/
					  widget_name,			/* name				*/
					  widget_class,			/* class			*/
					  &_resource,			/* resources		*/
					  1,					/* num_resources	*/
					  NULL,					/* args				*/
					  0);					/* num_args			*/

	return value;
}
/*----------------------------------------------------------------------*/
