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
/* Name:		<Xfe/GcUtil.c>											*/
/* Description:	Xfe widgets graphic context utilities.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* GC functions															*/
/*																		*/
/*----------------------------------------------------------------------*/
GC
XfeAllocateStringGc(Widget		w,
					XmFontList	font_list,
					Pixel		fg,
					Pixel		bg,
					Boolean	 	sensitive)
/*----------------------------------------------------------------------*/
{
	XGCValues		values;
	XFontStruct *	fs = (XFontStruct *) NULL;
	XtGCMask		value_mask;

	value_mask = GCGraphicsExposures | GCForeground;
   
	values.graphics_exposures	= False;
	values.foreground			= fg;

	/* Add insensitive tile if needed */
	if (!sensitive)
	{
		value_mask |= (GCFillStyle | GCTile);
      
		values.fill_style	= FillTiled;

		values.tile = XfeInsensitiveTile(_XfeScreen(w),_XfeDepth(w),fg,bg);
	}

	/* Obtain the XFontStruct */
	_XmFontListGetDefaultFont(font_list,&fs);

	/* Add font if it is good */
	if (fs != NULL)
	{
		value_mask |= GCFont;
		values.font = fs->fid;
	}

	/* Allocate the string GC */
	return XtAllocateGC(w,_XfeDepth(w),value_mask,&values,0,0);
}
/*----------------------------------------------------------------------*/
GC
XfeAllocateColorGc(Widget w,Pixel fg,Pixel bg,Boolean sensitive)
{
	XGCValues	values;
	XtGCMask	value_mask;
	
	value_mask = GCGraphicsExposures | GCForeground;
	
	values.graphics_exposures	= False;
	values.foreground			= fg;
	
	/* Add insensitive tile if needed */
	if (!sensitive)
	{
		value_mask |= (GCFillStyle | GCTile);
		
		values.fill_style	= FillTiled;
		
		values.tile = XfeInsensitiveTile(_XfeScreen(w),_XfeDepth(w),fg,bg);
	}
	
	return XtGetGC(w,value_mask,&values);
}
/*----------------------------------------------------------------------*/
GC
XfeAllocateTileGc(Widget w,Pixmap tile_pixmap)
{
	XGCValues	values;
	XtGCMask	value_mask;
	
	value_mask = GCGraphicsExposures;
	
	values.graphics_exposures	= False;

	/* Use the tile pixmap only if it is good */
	if (_XfePixmapGood(tile_pixmap))
	{
		value_mask |= GCFillStyle | GCTile;

		values.fill_style 	= FillTiled;
		values.tile			= tile_pixmap;
	}

	return XtGetGC(w,value_mask,&values);
}
/*----------------------------------------------------------------------*/
GC
XfeAllocateTransparentGc(Widget w)
{
	XGCValues	values;
	XtGCMask	value_mask = GCGraphicsExposures;
	XtGCMask	dynamic_mask = GCClipMask | GCClipXOrigin | GCClipYOrigin;

	values.graphics_exposures = False;

	return XtAllocateGC(w,_XfeDepth(w),value_mask,&values,dynamic_mask,0);
}
/*----------------------------------------------------------------------*/
GC
XfeAllocateSelectionGc(Widget		w,
					   Dimension	thickness,
					   Pixel		fg,
					   Pixel		bg)
{
	XGCValues	values;
	XtGCMask	value_mask;

	value_mask = 
		GCGraphicsExposures | GCForeground | GCLineWidth | 
		GCFunction | GCSubwindowMode;

	values.graphics_exposures	= False;
	values.line_width			= thickness;
	values.function				= GXxor;
	values.subwindow_mode		= IncludeInferiors;
	values.foreground			= fg ^ bg;

	return XtGetGC(w,value_mask,&values);
}
/*----------------------------------------------------------------------*/
GC
XfeAllocateCopyGc(Widget w)
{
	XGCValues	values;
	XtGCMask	value_mask;

	value_mask = GCGraphicsExposures;

	values.graphics_exposures	= False;

	return XtGetGC(w,value_mask,&values);
}
/*----------------------------------------------------------------------*/
