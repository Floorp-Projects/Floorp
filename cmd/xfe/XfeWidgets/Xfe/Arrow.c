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
/* Name:		<Xfe/Arrow.c>											*/
/* Description:	XfeArrow widget source.									*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/ArrowP.h>
#include <Xfe/ManagerP.h>
#include <Xm/RowColumnP.h>

#define ARROW_GC(w,ap) \
(_XfeIsSensitive(w) ? _XfeLabelGetLabelGC(w) : ap->arrow_insens_GC)

#define ARMED_GC(w,bp) \
((bp->fill_on_arm && bp->armed) ? bp->armed_GC : _XfeBackgroundGC(w))

#define RAISED_GC(w,bp) \
((bp->raise_on_enter && bp->raised) ? bp->label_raised_GC : _XfeBackgroundGC(w))

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1 "Widget is not a XfeButton."

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	PreferredGeometry	(Widget,Dimension *,Dimension *);
static void	DrawBackground		(Widget,XEvent *,Region,XRectangle *);
static void	DrawComponents		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeArrow functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	DrawArrow				(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Component Preparation codes											*/
/*																		*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow Resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
    /* Arrow resources */
    { 
		XmNarrowDirection,
		XmCArrowDirection,
		XmRArrowDirection,
		sizeof(unsigned char),
		XtOffsetOf(XfeArrowRec , xfe_arrow . arrow_direction),
		XmRImmediate, 
		(XtPointer) XmARROW_DOWN
    },
    { 
		XmNarrowWidth,
		XmCArrowWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , xfe_arrow . arrow_width),
		XmRImmediate, 
		(XtPointer) 12
    },
    { 
		XmNarrowHeight,
		XmCArrowHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , xfe_arrow . arrow_height),
		XmRImmediate, 
		(XtPointer) 12
    },

    /* Force XmNbuttonLayout to XmBUTTON_PIXMAP_ONLY */
    { 
		XmNbuttonLayout,
		XmCButtonLayout,
		XmRButtonLayout,
		sizeof(unsigned char),
		XtOffsetOf(XfeArrowRec , xfe_button . button_layout),
		XmRImmediate, 
		(XtPointer) XmBUTTON_PIXMAP_ONLY
    },

	/* Force margins to 2 */
	{ 
		XmNmarginBottom,
		XmCMarginBottom,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , xfe_primitive . margin_bottom),
		XmRImmediate, 
		(XtPointer) 2
	},
	{ 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , xfe_primitive . margin_left),
		XmRImmediate, 
		(XtPointer) 2
	},
	{ 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , xfe_primitive . margin_right),
		XmRImmediate, 
		(XtPointer) 2
	},
	{ 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , xfe_primitive . margin_top),
		XmRImmediate, 
		(XtPointer) 2
	},

	/* Force XmNshadowThickness to 0 */
	{ 
		XmNshadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , primitive . shadow_thickness),
		XmRImmediate, 
		(XtPointer) 0
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
    { 
		XmNarrowHeight,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , xfe_arrow . arrow_height),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNarrowWidth,
		sizeof(Dimension),
		XtOffsetOf(XfeArrowRec , xfe_arrow . arrow_width),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow widget class record initialization							*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(arrow,Arrow) =
{
    {
		/* Core Part */
		(WidgetClass) &xfeButtonClassRec,		/* superclass         	*/
		"XfeArrow",								/* class_name         	*/
		sizeof(XfeArrowRec),					/* widget_size        	*/
		NULL,									/* class_initialize   	*/
		NULL,									/* class_part_initialize*/
		FALSE,                                  /* class_inited       	*/
		Initialize,                             /* initialize         	*/
		NULL,                                   /* initialize_hook    	*/
		XtInheritRealize,                       /* realize            	*/
		NULL,									/* actions            	*/
		0,										/* num_actions        	*/
		resources,                              /* resources          	*/
		XtNumber(resources),                    /* num_resources      	*/
		NULLQUARK,                              /* xrm_class          	*/
		TRUE,                                   /* compress_motion    	*/
		XtExposeCompressMaximal,                /* compress_exposure  	*/
		TRUE,                                   /* compress_enterleave	*/
		FALSE,                                  /* visible_interest   	*/
		Destroy,                                /* destroy            	*/
		XtInheritResize,						/* resize             	*/
		XtInheritExpose,						/* expose             	*/
		SetValues,                              /* set_values         	*/
		NULL,                                   /* set_values_hook    	*/
		XtInheritSetValuesAlmost,				/* set_values_almost  	*/
		NULL,									/* get_values_hook		*/
		NULL,                                   /* accept_focus       	*/
		XtVersion,                              /* version            	*/
		NULL,                                   /* callback_private   	*/
		XtInheritTranslations,					/* tm_table           	*/
		XtInheritQueryGeometry,					/* query_geometry     	*/
		XtInheritDisplayAccelerator,            /* display accel      	*/
		NULL,                                   /* extension          	*/
    },

    /* XmPrimitive Part */
    {
		XmInheritBorderHighlight,				/* border_highlight		*/
		XmInheritBorderUnhighlight,				/* border_unhighlight 	*/
		XtInheritTranslations,                  /* translations       	*/
		XmInheritArmAndActivate,				/* arm_and_activate   	*/
		syn_resources,							/* syn resources      	*/
		XtNumber(syn_resources),				/* num syn_resources  	*/
		NULL,									/* extension          	*/
    },
	
    /* XfePrimitive Part */
    {
		XfeInheritBitGravity,					/* bit_gravity			*/
		PreferredGeometry,						/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		NULL,									/* prepare_components	*/
		XfeInheritLayoutComponents,				/* layout_components	*/
		DrawBackground,							/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		DrawComponents,							/* draw_components		*/
		NULL,									/* extension            */
    },

    /* XfeLabel Part */
    {
		XfeInheritLayoutString,					/* layout_string		*/
		XfeInheritDrawString,					/* draw_string			*/
		XfeInheritDrawSelection,				/* draw_selection		*/
		XfeInheritGetLabelGC,					/* get_label_gc			*/
		XfeInheritGetSelectionGC,				/* get_selection_gc		*/
		NULL,									/* extension            */
    },

    /* XfeButton Part */
    {
		XfeInheritLayoutPixmap,					/* layout_pixmap		*/
		XfeInheritDrawPixmap,					/* draw_pixmap			*/
		XfeInheritDrawRaiseBorder,				/* draw_raise_border	*/
		XfeInheritArmTimeout,					/* arm_timeout			*/
		NULL,									/* extension            */
    },

    /* XfeArrow Part */
    {
		NULL,									/* extension            */
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeArrowWidgetClass declaration.										*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(arrow,Arrow);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeArrowPart *	ap = _XfeArrowPart(nw);

    /* Make sure rep types are ok */
    XfeRepTypeCheck(nw,XmRArrowDirection,&ap->arrow_direction,XmARROW_DOWN);

    /* Allocate the insensitve GC  */
	ap->arrow_insens_GC = XfeAllocateColorGc(nw,_XfeForeground(nw),_XfeBackgroundPixel(nw),False);

    /* Finish of initialization */
    _XfePrimitiveChainInitialize(rw,nw,xfeArrowWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeArrowPart *	ap = _XfeArrowPart(w);

	XtReleaseGC(w,ap->arrow_insens_GC);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeArrowPart *	np = _XfeArrowPart(nw);
    XfeArrowPart *	op = _XfeArrowPart(ow);

    /* arrow_direction */
    if (np->arrow_direction != op->arrow_direction)
    {
		/* Make sure arrow direction is ok */
		XfeRepTypeCheck(nw,XmRArrowDirection,&np->arrow_direction,
						XmARROW_DOWN);

		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* arrow_width */
    if ((np->arrow_width != op->arrow_width) || 
		(np->arrow_height != op->arrow_height))
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }

    /* foreground or background */
    if ((_XfeForeground(nw) != _XfeForeground(ow)) ||
		(_XfeBackgroundPixel(nw) != _XfeBackgroundPixel(ow)))
    {
		XtReleaseGC(nw,np->arrow_insens_GC);

		np->arrow_insens_GC = XfeAllocateColorGc(nw,_XfeForeground(nw),_XfeBackgroundPixel(nw),False);
		
    }

    return _XfePrimitiveChainSetValues(ow,rw,nw,xfeArrowWidgetClass);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension *width,Dimension *height)
{
    XfeArrowPart *	ap = _XfeArrowPart(w);
    XfeButtonPart *	bp = _XfeButtonPart(w);

    *width  = 
		_XfeOffsetLeft(w) + _XfeOffsetRight(w) + ap->arrow_width;

    *height = 
		_XfeOffsetTop(w)  + _XfeOffsetBottom(w) + ap->arrow_height;

	/* Include the raise_border_thickenss if needed */
	if (bp->raise_on_enter)
	{
		*width  += (2 * bp->raise_border_thickness);
		*height += (2 * bp->raise_border_thickness);
	}
}
/*----------------------------------------------------------------------*/
static void
DrawBackground(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
    XfeArrowPart *	ap = _XfeArrowPart(w);
    XfeButtonPart *	bp = _XfeButtonPart(w);

	if (bp->emulate_motif)
	{
		return;
	}

#if 0	
	(*xfeButtonClassRec.xfe_primitive_class.draw_background)(w,
															 event,
															 region,
															 clip_rect);
#else
    /* Fill the background if needed */
    if (bp->fill_on_arm && bp->armed)
    {
		XFillRectangle(XtDisplay(w),
					   _XfePrimitiveDrawable(w),
					   bp->armed_GC,
					   0,0,
					   _XfeWidth(w),_XfeHeight(w));
    }
    else if (bp->fill_on_enter && (bp->raised || _XfePointerInside(w)))
    {
		XFillRectangle(XtDisplay(w),
					   _XfePrimitiveDrawable(w),
					   bp->label_raised_GC,
					   0,0,
					   _XfeWidth(w),_XfeHeight(w));
    }
#endif
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
	/* Invoke draw_string method */
	_XfeLabelDrawString(w,event,region,clip_rect);

	/* Invoke draw_pixmap method */
	_XfeButtonDrawPixmap(w,event,region,clip_rect);

	/* Invoke draw_border method */
	_XfeButtonDrawRaiseBorder(w,event,region,clip_rect);

	/* Draw the arrow */
	DrawArrow(w,event,region,clip_rect);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeArrow functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DrawArrow(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
    XfeArrowPart *		ap = _XfeArrowPart(w);
    XfeButtonPart *		bp = _XfeButtonPart(w);
	Dimension			width = XfeMin(ap->arrow_width,_XfeRectWidth(w));
	Dimension			height = XfeMin(ap->arrow_height,_XfeRectHeight(w));
	Position			x = (_XfeWidth(w) - width) / 2;
	Position			y = (_XfeHeight(w) - height) / 2;

	/* Draw the arrow as an XmArrowButton would */
	if (bp->emulate_motif)
	{
		GC gc = _XfeBackgroundGC(w);

		if (bp->raised)
		{
			if (bp->raise_on_enter && bp->raised)
			{
				gc = bp->label_raised_GC;
			}
		}
		else if (bp->armed)
		{
			if (bp->fill_on_arm && bp->armed)
			{
				gc = bp->armed_GC;
			}
		}

		XfeDrawMotifArrow(XtDisplay(w),
						  _XfePrimitiveDrawable(w), 
						  _XfeTopShadowGC(w),
						  _XfeBottomShadowGC(w),
						  gc,
						  x,
						  y, 
						  width,
						  height,
						  ap->arrow_direction,
						  2, /* hard coded to be compatible with Motif */
						  bp->armed);
	}
	/* Draw the arrow as an XfeButton would */
	else
	{
#if 1
		XfeDrawArrow(XtDisplay(w),
					 _XfePrimitiveDrawable(w),
					 ARROW_GC(w,ap),
					 x,y,
					 width,height,
					 ap->arrow_direction);
#else
		XfeDrawMotifArrow(XtDisplay(w),
						  _XfePrimitiveDrawable(w), 
						  NULL,
						  NULL,
						  ARROW_GC(w,ap),
						  x,y, 
						  width,height,
						  ap->arrow_direction,
						  0,
						  False);
#endif
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow Public Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
Widget
XfeCreateArrow(Widget pw,char * name,Arg * av,Cardinal ac)
{
    return XtCreateWidget(name,xfeArrowWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
