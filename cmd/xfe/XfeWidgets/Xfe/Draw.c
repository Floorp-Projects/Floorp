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
/* Name:		<Xfe/Draw.c>											*/
/* Description:	Xfe widgets drawing functions header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>
#include <Xfe/ManagerP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Drawing functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDrawRectangle(Display *		dpy,
				 Drawable		d,
				 GC				gc,
				 Position		x,
				 Position		y,
				 Dimension		width,
				 Dimension		height,
				 Dimension		thickness)
{
    static XSegment *	segs = NULL;
    static Cardinal	nsegs = 0;
    XSegment *		sp;
    Cardinal		i;
    Cardinal		count;
   
    assert(dpy != NULL);
    assert(d != None);
    assert(gc != NULL);

    /* Make sure a thickness is given */
    if (!thickness || !width || !height) return;

    /* Number of segments to draw */
    count = thickness * 4;

    /* Make sure we have enough segments */
    if (nsegs < count)
    {
		nsegs = count;
		segs = (XSegment *) XtRealloc((char *) segs,sizeof(XSegment) * nsegs);
    }

    for (i = 0; i < thickness; i++)
    {
		/* top */
		sp = segs + i;

		sp->x1 = x;
		sp->y1 = sp->y2 = y + i;
		sp->x2 = x + width - 0;

		/* bottom */
		sp = segs + i + thickness;

		sp->x1 = x;
		sp->y1 = sp->y2 = y + height - i - 1;
		sp->x2 = x + width - 0;

		/* left */
		sp = segs + i + 2 * thickness;

		sp->x1 = sp->x2 = x + i;
		sp->y1 = y + thickness;
		sp->y2 = y + height - thickness - 0;

		/* right */
		sp = segs + i + 3 * thickness;

		sp->x1 = sp->x2 = x + width - i - 1;
		sp->y1 = y + thickness;
		sp->y2 = y + height - thickness - 0;
    }

    XDrawSegments(dpy,d,gc,segs,count);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeClearRectangle(Display *	dpy,
				  Drawable	d,
				  Position	x,
				  Position	y,
				  Dimension	width,
				  Dimension	height,
				  Dimension	thickness,
				  Boolean	exposures)
{
    Dimension rw;
    
    assert(dpy != NULL);
    assert(d != None);
    
    /* Make sure a thickness is given */
    if (!thickness || !width || !height) return;
    
    rw = width - (thickness / 2);
    
    /* Left */
    XClearArea(dpy,d,x,y,thickness,height,exposures);
    
    /* Right */
    XClearArea(dpy,d,x + width - thickness,y,thickness,height,exposures);
    
    /* Top */
    XClearArea(dpy,d,x + thickness,y,rw,thickness,exposures);
    
    /* Bottom */
    XClearArea(dpy,d,x+thickness,y + height - thickness,rw,thickness,exposures);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDrawCross(Display *	dpy,
			 Drawable	d,
			 GC		gc,
			 Position	x,
			 Position	y,
			 Dimension	width,
			 Dimension	height,
			 Dimension	thickness)
{
    static XPoint	pts[17];

    int		w = width;
    int		h = height;
    int		w2 = width / 2;
    int		h2 = height / 2;
    float	rad2 = 1.41421356237;
    int		c = (int) ((float) thickness / rad2);

    /* Make sure c is at least 1 */
    if (c < 1)
    {
		c = 1;
    }

    assert(dpy != NULL);
    assert(d != None);
    assert(gc != NULL);

    /* Make sure a thickness is given */
    if (!thickness || !width || !height) return;

    pts[0].x = x + 0;
    pts[0].y = y + 0;

    pts[1].x = x + c;
    pts[1].y = y + 0;

    pts[2].x = x + w2;
    pts[2].y = y + h2 - c;

    pts[3].x = x + w - c;
    pts[3].y = y + 0;

    pts[4].x = x + w;
    pts[4].y = y + 0;

    pts[5].x = x + w;
    pts[5].y = y + c;

    pts[6].x = x + w2 + c;
    pts[6].y = y + h2;

    pts[7].x = x + w;
    pts[7].y = y + h - c;

    pts[8].x = x + w;
    pts[8].y = y + h;

    pts[9].x = x + w - c;
    pts[9].y = y + h;

    pts[10].x = x + w2;
    pts[10].y = y + h2 + c;

    pts[11].x = x + c;
    pts[11].y = y + h;

    pts[12].x = x + 0;
    pts[12].y = y + h;

    pts[13].x = x + 0;
    pts[13].y = y + h - c;

    pts[14].x = x + w2 - c;
    pts[14].y = y + h2;

    pts[15].x = x + 0;
    pts[15].y = y + c;

    pts[16].x = x + 0;
    pts[16].y = y + 0;

    XFillPolygon(dpy,d,gc,pts,17,CoordModeOrigin,Complex);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDrawArrow(Display *		dpy,
			 Drawable		d,
			 GC				gc,
			 Position		x,
			 Position		y,
			 Dimension		width,
			 Dimension		height,
			 unsigned char	direction)
{
    static XPoint	pts[3];
    Dimension		dx;
    Dimension		dy;
    
    assert( dpy != NULL );
    assert( d != None );
    assert( gc != NULL );
    
    /* Make sure a thickness is given */
    if (!width || !height) return;
    
    dx = width / 2;
    dy = height / 2;
    
    switch (direction)
    {
    case XmARROW_UP:
		XfePointSet(&pts[0],x,y + height);
		XfePointSet(&pts[1],x + width,y + height);
		XfePointSet(&pts[2],x + dx,y);
		break;
	
    case XmARROW_DOWN:
		XfePointSet(&pts[0],x,y);
		XfePointSet(&pts[1],x + dx,y + height);
		XfePointSet(&pts[2],x + width,y);
		break;
	
    case XmARROW_RIGHT:
		XfePointSet(&pts[0],x,y);
		XfePointSet(&pts[1],x,y + height);
		XfePointSet(&pts[2],x + width,y + dy);
		break;
	
    case XmARROW_LEFT:
		XfePointSet(&pts[0],x + width,y);
		XfePointSet(&pts[1],x + width,y + height);
		XfePointSet(&pts[2],x,y + dy);
		break;
    }
    
    /* Fill the Arrow */
    XFillPolygon(dpy,d,gc,pts,3,Convex,CoordModeOrigin);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDrawMotifArrow(Display *		dpy,
				  Drawable		d,
				  GC			top_gc,
				  GC			bottom_gc,
				  GC			center_gc,
				  Position		x,
				  Position		y,
				  Dimension		width,
				  Dimension		height,
				  unsigned char	direction,
				  Dimension		shadow_thickness,
				  Boolean		swap)
{

    assert( dpy != NULL );
    assert( d != None );
/*     assert( top_gc != NULL ); */
/*     assert( bottom_gc != NULL ); */
    assert( center_gc != NULL );

	if (swap) 
	{
		GC tmp_gc = bottom_gc;
		bottom_gc = top_gc;
		top_gc = tmp_gc;
	}
	
	_XmDrawArrow(dpy,d,
				 top_gc,bottom_gc,center_gc,
				 x,y,
				 width,height,
				 shadow_thickness,
				 direction);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDrawVerticalLine(Display *		dpy,
					Drawable		d,
					GC				gc,
					Position		x,
					Position		y,
					Dimension		height,
					Dimension		thickness)
{
    static XSegment *	segs = NULL;
    static Cardinal	nsegs = 0;
    XSegment *		sp;
    Cardinal		i;
    Cardinal		count;
   
    assert(dpy != NULL);
    assert(d != None);
    assert(gc != NULL);

    /* Make sure a thickness is given */
    if (!thickness || !height) return;

    /* Number of segments to draw */
    count = thickness;

    /* Make sure we have enough segments */
    if (nsegs < count)
    {
		nsegs = count;
		segs = (XSegment *) XtRealloc((char *) segs,sizeof(XSegment) * nsegs);
    }

    for (i = 0; i < thickness; i++)
    {
		sp = segs + i;

		sp->x1 = sp->x2 = x + i;
		sp->y1 = y + thickness;
		sp->y2 = y + height - thickness - 1;
    }

    XDrawSegments(dpy,d,gc,segs,count);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDrawHorizontalLine(Display *		dpy,
					  Drawable		d,
					  GC			gc,
					  Position		x,
					  Position		y,
					  Dimension		width,
					  Dimension		thickness)
{
    static XSegment *	segs = NULL;
    static Cardinal	nsegs = 0;
    XSegment *		sp;
    Cardinal		i;
    Cardinal		count;
   
    assert(dpy != NULL);
    assert(d != None);
    assert(gc != NULL);

    /* Make sure a thickness is given */
    if (!thickness || !width) return;

    /* Number of segments to draw */
    count = thickness;

    /* Make sure we have enough segments */
    if (nsegs < count)
    {
		nsegs = count;
		segs = (XSegment *) XtRealloc((char *) segs,sizeof(XSegment) * nsegs);
    }

    for (i = 0; i < thickness; i++)
    {
		sp = segs + i;

		sp->x1 = x;
		sp->y1 = sp->y2 = y + i;
		sp->x2 = x + width - 1;
    }

    XDrawSegments(dpy,d,gc,segs,count);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeStippleRectangle(Display *	dpy,
					Drawable	d,
					GC			gc,
					Position	x,
					Position	y,
					Dimension	width,
					Dimension	height,
					Dimension	step)
{
	int			i;
	int			j;
	Dimension	half_step;

    assert( dpy != NULL );
    assert( d != None );
    assert( gc != NULL );
    assert( width > 0 );
    assert( height > 0 );
    assert( step > 1 );
    assert( XfeEven(step) );
    
    /* Make sure a step is given */
    if (!step || !width || !height) return;

	half_step = step / 2;

	/* Vertical */
	for (i = y; i <= (y + height); i += step)
	{
		for (j = x; j <= (x + width); j += step)
		{
			XFillRectangle(dpy,d,gc,j,i,half_step,half_step);
		}
	}

	/* Horizontal */
	for (i = (y + half_step); i <= (y + height - half_step); i += step)
	{
		for (j = (x + half_step); j <= (x + width - half_step); j += step)
		{
			XFillRectangle(dpy,d,gc,j,i,half_step,half_step);
		}
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Drawable functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeDrawableGetGeometry(Display *	dpy,
					   Drawable		d,
					   Position *	x_out,
					   Position *	y_out,
					   Dimension *	width_out,
					   Dimension *	height_out)
{
	int				x = 0;
	int				y = 0;
	unsigned int	width = 0;
	unsigned int	height = 0;
	unsigned int	border;
	unsigned int	depth;
	Window			root;
	Boolean			result = False;

	assert( dpy != NULL );
	assert( d != None );
	
	result = XGetGeometry(dpy,d,&root,&x,&y,&width,&height,&border,&depth);

	if (x_out)
		*x_out = (Position) x;
	
	if (y_out)
		*y_out = (Position) y;
	
	if (width_out)
		*width_out = (Dimension) width;
	
	if (height_out)
		*height_out = (Dimension) height;
	
	return True;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Shadow functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDrawShadowsAroundWidget(Widget			parent,
						   Widget			child,
						   GC				top_gc,
						   GC				bottom_gc,
						   Dimension		offset,
						   Dimension		shadow_thickness,
						   unsigned char	shadow_type)
{
	assert( _XfeIsAlive(parent) );
	assert( _XfeIsAlive(child) );
	assert( shadow_thickness > 0 );
	assert( XmIsManager(parent) );

	_XmDrawShadows(XtDisplay(parent),
				   _XfeWindow(parent),
				   _XfemTopShadowGC(parent),
				   _XfemBottomShadowGC(parent),
				   _XfeX(child) - shadow_thickness - offset,
				   _XfeY(child) - shadow_thickness - offset,
				   _XfeWidth(child) + 2 * shadow_thickness + 2 * offset,
				   _XfeHeight(child) + 2 * shadow_thickness + 2 * offset,
				   shadow_thickness,
				   shadow_type);
}
/*----------------------------------------------------------------------*/
