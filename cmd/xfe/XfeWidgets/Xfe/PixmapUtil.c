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
/* Name:		<Xfe/PixmapUtil.c>										*/
/* Description:	Xfe widgets pixmap utilities.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>

#define MESSAGE1  "The pixmap needs to have the same depth as the widget."
#define MESSAGE2  "Cannot obtain geometry for pixmap."

/*----------------------------------------------------------------------*/
/*																		*/
/* Double buffer global variables										*/
/*																		*/
/*----------------------------------------------------------------------*/
static Pixmap		_pixmap_buffer = XmUNSPECIFIED_PIXMAP;
static Cardinal		_pixmap_buffer_depth = 0;
static Dimension	_pixmap_buffer_width = 0;
static Dimension	_pixmap_buffer_height = 0;
/*----------------------------------------------------------------------*/

static Pixmap	BufferCreate		(Widget,Dimension,Dimension);
static void		BufferUpdate		(Widget,Pixmap,
									 Dimension,Dimension);
static Boolean	BufferCheck			(Widget);
static void		BufferFree			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Double buffer utilities												*/
/*																		*/
/*----------------------------------------------------------------------*/
Pixmap
_XfePixmapBufferAllocate(Widget w)
{
    Pixmap buffer = XmUNSPECIFIED_PIXMAP;

    assert( w != NULL );

    /* Check whether a buffer already exists */
    if (_XfePixmapGood(_pixmap_buffer))
    {
		assert( _pixmap_buffer_depth == _XfeDepth(w));

		/* Check whether this widget can use the buffer */
		if (BufferCheck(w))
		{
			buffer = _pixmap_buffer;
		}
		else
		{
			Dimension new_width = XfeMax(_XfeWidth(w),_pixmap_buffer_width);
			Dimension new_height = XfeMax(_XfeHeight(w),_pixmap_buffer_height);

			BufferFree(w);

			buffer = BufferCreate(w,new_width,new_height);
	    
			assert( _XfePixmapGood(buffer) );
	    
			BufferUpdate(w,buffer,new_width,new_height);
		}
    }
    /* Create a buffer pixmap for the first time */
    else
    {
		buffer = BufferCreate(w,_XfeWidth(w),_XfeHeight(w));

		assert( _XfePixmapGood(buffer) );

		BufferUpdate(w,buffer,_XfeWidth(w),_XfeHeight(w));
    }

    return buffer;
}
/*----------------------------------------------------------------------*/
void
_XfePixmapBufferRelease(Widget w,Pixmap buffer)
{
}
/*----------------------------------------------------------------------*/
Pixmap
_XfePixmapBufferAccess()
{
    return _pixmap_buffer;
}
/*----------------------------------------------------------------------*/
static void
BufferFree(Widget w)
{
    assert( _XfePixmapGood(_pixmap_buffer) );

    /* Free the buffer pixmap */
    XFreePixmap(XtDisplay(w),_pixmap_buffer);
}
/*----------------------------------------------------------------------*/
static Pixmap
BufferCreate(Widget w,Dimension width,Dimension height)
{
    assert( w != NULL );

    return XCreatePixmap(XtDisplay(w),
						 DefaultRootWindow(XtDisplay(w)),
						 width,
						 height,
						 _XfeDepth(w));
}
/*----------------------------------------------------------------------*/
static void
BufferUpdate(Widget w,Pixmap buffer,Dimension width,Dimension height)
{
    assert( _XfePixmapGood(buffer) );

    _pixmap_buffer	  = buffer;
    _pixmap_buffer_depth  = _XfeDepth(w);
    _pixmap_buffer_width  = width;
    _pixmap_buffer_height = height;
}
/*----------------------------------------------------------------------*/
static Boolean
BufferCheck(Widget w)
{
    assert( w != NULL );

    return ((_XfeWidth(w) <= _pixmap_buffer_width) &&
			(_XfeHeight(w) <= _pixmap_buffer_height) &&
			(_XfeDepth(w) == _pixmap_buffer_depth));
}
/*----------------------------------------------------------------------*/



/*----------------------------------------------------------------------*/
/*																		*/
/* Pixmap																*/
/*																		*/
/*----------------------------------------------------------------------*/
Boolean
XfePixmapExtent(Display *	dpy,
				Pixmap		pixmap,
				Dimension *	width,
				Dimension *	height,
				Cardinal *	depth)
{
    Boolean result = False;
    
    assert( dpy != NULL );
    
    if (_XfePixmapGood(pixmap))
    {
		unsigned int	borderWidth;
		Window		rootWindow;
		int 		x;
		int 		y;
		unsigned int	pixmap_width;
		unsigned int	pixmap_height;
		unsigned int	pixmap_depth;

		/* Query Server for Pixmap info */
		result = XGetGeometry(dpy,pixmap,&rootWindow,&x,&y,
							  &pixmap_width,&pixmap_height,
							  &borderWidth,&pixmap_depth);
	

		/* Verify that the pixmap extent was obtained */
		if(result)
		{
			/* Assign the required components */
			if (width)
			{
				*width = pixmap_width;
			}
	    
			if (height)
			{
				*height = pixmap_height;
			}
	    
			if (depth)
			{
				*depth = pixmap_depth;
			}
		}
		else
		{
			/* Make them NULL as an extra warning */
			if (width)
			{
				*width = 0;
			}
	    
			if (height)
			{
				*height = 0;
			}
	    
			if (depth)
			{
				*depth = 0;
			}
		}
    }
    
    return result;
}
/*----------------------------------------------------------------------*/
Boolean
XfePixmapGood(Pixmap pixmap)
{
   return _XfePixmapGood(pixmap);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Stipple / Tile functions						*/
/*																		*/
/*----------------------------------------------------------------------*/
Pixmap
XfeInsensitiveTile(Screen * screen,int depth,Pixel fg,Pixel bg)
{
   return XmGetPixmapByDepth(screen,"50_foreground",fg,bg,depth);
}
/*----------------------------------------------------------------------*/
Pixmap
XfeInsensitiveStipple(Screen * screen,int depth,Pixel fg,Pixel bg)
{
   return XmGetPixmapByDepth(screen,"50_foreground",fg,bg,depth);
}
/*----------------------------------------------------------------------*/
void
XfePixmapClear(Display *	dpy,
			   Pixmap		pixmap,
			   GC			gc,
			   Dimension	width,
			   Dimension	height)
{
    assert( dpy != NULL );
    assert( _XfePixmapGood(pixmap) );
    assert( gc != None );
    assert( width > 0 );
    assert( height > 0 );

    XFillRectangle(dpy,pixmap,gc,0,0,width,height);
}
/*----------------------------------------------------------------------*/
Pixmap
XfePixmapCheck(Widget		w,
			   Pixmap		pixmap,
			   Dimension *	width_out,
			   Dimension *	height_out)
{
	Pixmap		result = XmUNSPECIFIED_PIXMAP;
	Dimension	width = 0;
	Dimension	height = 0;

    /* Check the pixmap only if it is good */
    if (_XfePixmapGood(pixmap))
    {
		Cardinal depth;

		if (XfePixmapExtent(XtDisplay(w),
							pixmap,
							&width,
							&height,
							&depth))
		{
			/* Make sure the pixmap's depth matches the widget */
			if (_XfeDepth(w) == depth)
			{
				result = pixmap;
			}
			else
			{
				_XfeWarning(w,MESSAGE1);
			}
		}
		else
		{
			_XfeWarning(w,MESSAGE2);
		}
    }

	if (width_out)
	{
		*width_out = width;
	}

	if (height_out)
	{
		*height_out = height;
	}

	return pixmap;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Preparation utlities													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfePixmapPrepare(Widget		w,
				  Pixmap *		pixmap_in_out,
				  Dimension *	width_out,
				  Dimension *	height_out,
				  String		name)
{
	Dimension width = 0;
	Dimension height = 0;

/* 	assert( width_out != NULL ); */
/* 	assert( height_out != NULL ); */
	assert( pixmap_in_out != NULL );
	assert( _XfeIsAlive(w) );
			
    /* Obtain geometry info for the pixmap if its good */
    if (_XfePixmapGood(*pixmap_in_out))
    {
		Cardinal depth;
		
		if (XfePixmapExtent(XtDisplay(w),
							*pixmap_in_out,
							&width,
							&height,
							&depth))
		{
			/* Make sure the pixmap's depth matches the widget */
			if (_XfeDepth(w) != depth)
			{
				width = 0;
				height = 0;

				*pixmap_in_out = XmUNSPECIFIED_PIXMAP;

				_XfeArgWarning(w,MESSAGE1,name);
			}
		}
		/* Could not get geometry for the pixmap */
		else
		{
			*pixmap_in_out = XmUNSPECIFIED_PIXMAP;

			_XfeArgWarning(w,MESSAGE2,name);
		}
    }

    /* Assign the dimensions if needed */
    if (width_out)
	{
		*width_out = width;
	}

    if (height_out)
	{
		*height_out = height;
	}
}
/*----------------------------------------------------------------------*/
