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
/* Name:		<XfeTest/TestPixmap.c>									*/
/* Description:	Xfe widget pixmap test funcs.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeTest.h>

#if defined(LINUX) || defined(IRIX)
#include <X11/xpm.h>
#else
#include <xpm.h>
#endif

/*----------------------------------------------------------------------*/
Boolean
XfeAllocatePixmapFromFile(char *			filename,
						  Display *			dpy,
						  Drawable			d,
						  Colormap			colormap,
						  Cardinal			closeness,
						  Cardinal			depth,
						  Pixel				bg,
						  Pixmap *			pixmap,
						  Pixmap *			mask)
{
    XpmAttributes		attrib;
	Boolean				result = True;
	XpmColorSymbol		symbols[1];

    assert( dpy != NULL );
    assert( d != None );
    assert( colormap != None );
    assert( pixmap != NULL );
    assert( mask != NULL );
    assert( depth > 0 );
	assert( filename != NULL );
	assert( access(filename,F_OK | R_OK) == 0 );

	/*  Set up the transparent symbol */
	symbols[0].name		= NULL;
	symbols[0].value	= "none";
	symbols[0].pixel	= bg;

    /* Set up the Xfem Attributes mask and strucutre */
    attrib.valuemask = 
		XpmCloseness | XpmDepth | XpmColormap | XpmColorSymbols;

    attrib.colorsymbols		= symbols;
    attrib.numsymbols		= 1;
    attrib.closeness		= closeness;
    attrib.colormap			= colormap;
    attrib.depth			= depth;

    /* Try to read the xpm file */
    if (XpmReadFileToPixmap(dpy,d,filename,pixmap,mask,&attrib) != XpmSuccess)
    {
		*pixmap = XmUNSPECIFIED_PIXMAP;
		*mask = XmUNSPECIFIED_PIXMAP;
		
		result = False;
    }

    return result;
}
/*----------------------------------------------------------------------*/
Boolean
XfeAllocatePixmapFromData(char **			data,
						  Display *			dpy,
						  Drawable			d,
						  Colormap			colormap,
						  Cardinal			closeness,
						  Cardinal			depth,
						  Pixel				bg,
						  Pixmap *			pixmap,
						  Pixmap *			mask)
{
    XpmAttributes		attrib;
	Boolean				result = True;
	XpmColorSymbol		symbols[1];

    assert( dpy != NULL );
    assert( d != None );
    assert( colormap != None );
    assert( pixmap != NULL );
    assert( mask != NULL );
    assert( depth > 0 );
	assert( data != NULL );

	/*  Set up the transparent symbol */
	symbols[0].name		= NULL;
	symbols[0].value	= "none";
	symbols[0].pixel	= bg;

    /* Set up the Xfem Attributes mask and strucutre */
    attrib.valuemask = 
		XpmCloseness | XpmDepth | XpmColormap | XpmColorSymbols;
	
    attrib.colorsymbols		= symbols;
    attrib.numsymbols		= 1;
    attrib.closeness		= closeness;
    attrib.colormap			= colormap;
    attrib.depth			= depth;

    if (XpmCreatePixmapFromData(dpy,d,data,pixmap,mask,&attrib) != XpmSuccess)
    {
		*pixmap = XmUNSPECIFIED_PIXMAP;
		*mask = XmUNSPECIFIED_PIXMAP;
		
		result = False;
    }

    return result;
}
/*----------------------------------------------------------------------*/
Pixmap
XfeGetPixmapFromFile(Widget w,char * filename)
{
    Pixmap pixmap = XmUNSPECIFIED_PIXMAP;
    Pixmap mask;

	assert( filename != NULL );
	assert( 0 );

    XfeAllocatePixmapFromFile(filename,
							  XtDisplay(w),
							  DefaultRootWindow(XtDisplay(w)),
							  XfeColormap(w),
							  40000,
							  XfeDepth(w),
							  XfeBackground(w),
							  &pixmap,
							  &mask);
	
	if (XfePixmapGood(mask))
	{
		XFreePixmap(XtDisplay(w),mask);
	}
	
    return pixmap;
}
/*----------------------------------------------------------------------*/
Pixmap
XfeGetPixmapFromData(Widget w,char ** data)
{
    Pixmap pixmap = XmUNSPECIFIED_PIXMAP;
    Pixmap mask;

	assert( data != NULL );
/* 	assert( 0 ); */

    XfeAllocatePixmapFromData(data,
							  XtDisplay(w),
							  DefaultRootWindow(XtDisplay(w)),
							  XfeColormap(w),
							  40000,
							  XfeDepth(w),
							  XfeBackground(w),
							  &pixmap,
							  &mask);
	
	if (XfePixmapGood(mask))
	{
		XFreePixmap(XtDisplay(w),mask);
	}
	
    return pixmap;
}
/*----------------------------------------------------------------------*/
XfePixmapTable
XfeAllocatePixmapTable(Widget w,String * files,Cardinal num_files)
{
	XfePixmapTable	table = NULL;
	Cardinal		i;

	assert( files != NULL );
	assert( num_files > 0 );

	table = (XfePixmapTable) XtMalloc(sizeof(Pixmap) * num_files);

	assert( table != NULL );

	for(i = 0; i < num_files; i++)
	{
		table[i] = XfeGetPixmapFromFile(w,files[i]);
	}

    return table;
}
/*----------------------------------------------------------------------*/
