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
/* Name:		<Xfe/CallProcs.c>										*/
/* Description:	Misc shared resource call procedures.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>
#include <Xfe/PrimitiveP.h>
#include <Xfe/ManagerP.h>

#define HOR_CURSOR	"sb_h_double_arrow"
#define VER_CURSOR	"sb_v_double_arrow"

/*----------------------------------------------------------------------*/
/*																		*/
/* Resource call procedures												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeCallProcCopyBackground(Widget w,int offset,XrmValue * value)
{
    static Pixel pixel;

	pixel = _XfeBackgroundPixel(w);
   
    value->addr = (XPointer) &pixel;
    value->size = sizeof(pixel);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeCallProcCopyForeground(Widget w,int offset,XrmValue * value)
{
    static Pixel pixel;

	assert( XfeIsPrimitive(w) || XfeIsManager(w) );

	if (XfeIsPrimitive(w))
	{
		pixel = _XfeForeground(w);
	}
	else
	{
		pixel = _XfemForeground(w);
	}
   
    value->addr = (XPointer) &pixel;
    value->size = sizeof(pixel);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeCallProcCopyShadowThickness(Widget w,int offset,XrmValue * value)
{
    static Dimension shadow_thickness;

	assert( XfeIsPrimitive(w) || XfeIsManager(w) );

	if (XfeIsPrimitive(w))
	{
		shadow_thickness = _XfeShadowThickness(w);
	}
	else
	{
		shadow_thickness = _XfemShadowThickness(w);
	}
   
    value->addr = (XPointer) &shadow_thickness;
    value->size = sizeof(shadow_thickness);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeCallProcSelectPixel(Widget w,int offset,XrmValue * value)
{
    static Pixel pixel;

	pixel = XfeSelectPixel(w,_XfeBackgroundPixel(w));
   
    value->addr = (XPointer) &pixel;
    value->size = sizeof(pixel);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeCallProcDefaultLabelFontList(Widget w,int offset,XrmValue * value)
{
    static XmFontList font_list;

	font_list = XmFontListCopy(_XmGetDefaultFontList(w,XmLABEL_FONTLIST));
    
    value->addr = (XPointer) &font_list;
    value->size = sizeof(font_list);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeCallProcDefaultTextFontList(Widget w,int offset,XrmValue * value)
{
    static XmFontList font_list;

	font_list = XmFontListCopy(_XmGetDefaultFontList(w,XmTEXT_FONTLIST));
    
    value->addr = (XPointer) &font_list;
    value->size = sizeof(font_list);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeCallProcOrientationCursor(Widget w,int offset,XrmValue * value)
{
    static Cursor		drag_cursor;
	XrmValue			xrm_from;
	XrmValue			xrm_to;
	unsigned char		orientation = XmVERTICAL;
	String				cursor_name;

	/*
	 * Determine the orientation.  Obviously, this will only work for 
	 * widgets that actually have a XmNorientation resource.
	 */
	XtVaGetValues(w,XmNorientation,&orientation,NULL);

	cursor_name = (orientation == XmVERTICAL) ? VER_CURSOR : HOR_CURSOR;

	/* From */
	xrm_from.addr	= (XPointer) cursor_name;
	xrm_from.size	= strlen(cursor_name);
	
	/* To */
	xrm_to.addr		= (XPointer) &drag_cursor;
	xrm_to.size		= sizeof(drag_cursor);

	/* Try to do the convertion */
	if (!XtConvertAndStore(w,XmRString,&xrm_from,XmRCursor,&xrm_to))
	{
		drag_cursor = None;
	}
	
	value->addr = (XPointer) &drag_cursor;
    value->size = sizeof(drag_cursor);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Synthetic resource call procedures									*/
/*																		*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Synthetic resource import procedures									*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmImportOperator
_XfeSyntheticSetResourceForChild(Widget w,int offset,XtArgVal * value)
{ 
    return XmSYNTHETIC_LOAD;
}
/*----------------------------------------------------------------------*/
