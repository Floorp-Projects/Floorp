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
/* Name:		<Xfe/Tab.c>												*/
/* Description:	XfeTab widget source.									*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/TabP.h>
#include <Xfe/ManagerP.h>
#include <Xm/RowColumnP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1 "Widget is not a XfeButton."
#define MESSAGE2 "XmNsubMenuId is a read-only resource."
#define MESSAGE3 "XmNpoppedUp is a read-only resource."
#define MESSAGE4 "XmNmappingDelay must be at least 1."

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
static void	PrepareComponents	(Widget,int);
static void	DrawBackground		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeTab functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	DrawTab				(Widget,XEvent *,Region,XRectangle *);
static void	FillTabVertical		(Widget,Pixmap,Dimension,Dimension);
static void	FillTabHorizontal	(Widget,Pixmap,Dimension,Dimension);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab Resources														*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
    /* Resources */
    { 
		XmNbottomPixmap,
		XmCBottomPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . bottom_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNhorizontalPixmap,
		XmCHorizontalPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . horizontal_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNleftPixmap,
		XmCLeftPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . left_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNrightPixmap,
		XmCRightPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . right_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNtopPixmap,
		XmCTopPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . top_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNverticalPixmap,
		XmCVerticalPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . vertical_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNbottomRaisedPixmap,
		XmCBottomRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . bottom_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNhorizontalRaisedPixmap,
		XmCHorizontalRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . horizontal_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNleftRaisedPixmap,
		XmCLeftRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . left_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNrightRaisedPixmap,
		XmCRightRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . right_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNtopRaisedPixmap,
		XmCTopRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . top_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNverticalRaisedPixmap,
		XmCVerticalRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeTabRec , xfe_tab . vertical_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNorientation,
		XmCOrientation,
		XmROrientation,
		sizeof(unsigned char),
		XtOffsetOf(XfeTabRec , xfe_tab . orientation),
		XmRImmediate, 
		(XtPointer) XmVERTICAL
    },

	/* Force XmNraiseBorderThickness to 0 */
    { 
		XmNraiseBorderThickness,
		XmCRaiseBorderThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTabRec , xfe_button . raise_border_thickness),
		XmRImmediate, 
		(XtPointer) 0
    },

	/* Force XmNraiseOnEnter to False */
    { 
		XmNraiseOnEnter,
		XmCRaiseOnEnter,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeTabRec , xfe_button . raise_on_enter),
		XmRImmediate, 
		(XtPointer) False
    },

	/* Force XmNfillOnEnter to True */
    { 
		XmNfillOnEnter,
		XmCFillOnEnter,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeTabRec , xfe_button . fill_on_enter),
		XmRImmediate, 
		(XtPointer) True
    },

	/* Force XmNbuttonLayout to XmBUTTON_PIXMAP_ONLY */
    { 
		XmNbuttonLayout,
		XmCButtonLayout,
		XmRButtonLayout,
		sizeof(unsigned char),
		XtOffsetOf(XfeTabRec , xfe_button . button_layout),
		XmRImmediate, 
		(XtPointer) XmBUTTON_PIXMAP_ONLY
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab widget class record initialization							*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(tab,Tab) =
{
    {
		/* Core Part */
		(WidgetClass) &xfeButtonClassRec,		/* superclass         	*/
		"XfeTab",								/* class_name         	*/
		sizeof(XfeTabRec),						/* widget_size        	*/
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
		NULL,									/* syn resources      	*/
		0,										/* num syn_resources  	*/
		NULL,									/* extension          	*/
    },
	
    /* XfePrimitive Part */
    {
		XfeInheritBitGravity,					/* bit_gravity			*/
		PreferredGeometry,						/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		PrepareComponents,						/* prepare_components	*/
		XfeInheritLayoutComponents,				/* layout_components	*/
		DrawBackground,							/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		XfeInheritDrawComponents,				/* draw_components		*/
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

    /* XfeTab Part */
    {
		NULL,									/* extension            */
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeTabWidgetClass declaration.										*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(tab,Tab);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    /* Finish of initialization */
    _XfePrimitiveChainInitialize(rw,nw,xfeTabWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeTabPart *	np = _XfeTabPart(nw);
    XfeTabPart *	op = _XfeTabPart(ow);

    /* vertical_pixmap */
    if (np->vertical_pixmap != op->vertical_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_VERTICAL_PIXMAP;
    }

    /* horizontal_pixmap */
    if (np->horizontal_pixmap != op->horizontal_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_HORIZONTAL_PIXMAP;
    }

    /* top_pixmap */
    if (np->top_pixmap != op->top_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_TOP_PIXMAP;
    }

    /* bottom_pixmap */
    if (np->bottom_pixmap != op->bottom_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_BOTTOM_PIXMAP;
    }

    /* left_pixmap */
    if (np->left_pixmap != op->left_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_LEFT_PIXMAP;
    }

    /* right_pixmap */
    if (np->right_pixmap != op->right_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_RIGHT_PIXMAP;
    }

    /* vertical_raised_pixmap */
    if (np->vertical_raised_pixmap != op->vertical_raised_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_VERTICAL_RAISED_PIXMAP;
    }

    /* horizontal_raised_pixmap */
    if (np->horizontal_raised_pixmap != op->horizontal_raised_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_HORIZONTAL_RAISED_PIXMAP;
    }

    /* top_raised_pixmap */
    if (np->top_raised_pixmap != op->top_raised_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_TOP_RAISED_PIXMAP;
    }

    /* bottom_raised_pixmap */
    if (np->bottom_raised_pixmap != op->bottom_raised_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_BOTTOM_RAISED_PIXMAP;
    }

    /* left_raised_pixmap */
    if (np->left_raised_pixmap != op->left_raised_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_LEFT_RAISED_PIXMAP;
    }

    /* right_raised_pixmap */
    if (np->right_raised_pixmap != op->right_raised_pixmap)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;

		_XfePrepareFlags(nw) |= _XFE_PREPARE_TAB_RIGHT_RAISED_PIXMAP;
    }

    /* orientation */
    if (np->orientation != op->orientation)
    {
		_XfeConfigFlags(nw) |= XfeConfigGLE;
    }

    return _XfePrimitiveChainSetValues(ow,rw,nw,xfeTabWidgetClass);
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
    XfeTabPart *			tp = _XfeTabPart(w);
	XfeButtonWidgetClass	bwc = (XfeButtonWidgetClass) xfeButtonWidgetClass;
	Dimension				max;

	/* Explicit invoke of XfeButton's preferred_geometry() method */
	(*bwc->xfe_primitive_class.preferred_geometry)(w,width,height);

	/* Vertical */
	if (tp->orientation == XmVERTICAL)
	{
		max = XfeMax(tp->bottom_width,tp->horizontal_width);
		max = XfeMax(max,tp->top_width);
		max = XfeMax(max,*width);

		*width = max;
	}
	/* Horizontal */
	else
	{
		max = XfeMax(tp->left_height,tp->vertical_height);
		max = XfeMax(max,tp->right_height);
		max = XfeMax(max,*height);

		*height = max;
	}
}
/*----------------------------------------------------------------------*/
static void
PrepareComponents(Widget w,int flags)
{
    XfeTabPart * tp = _XfeTabPart(w);

    if (flags & _XFE_PREPARE_TAB_BOTTOM_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->bottom_pixmap,
						  &tp->bottom_width,
						  &tp->bottom_height,
						  XmNbottomPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_TOP_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->top_pixmap,
						  &tp->top_width,
						  &tp->top_height,
						  XmNtopPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_LEFT_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->left_pixmap,
						  &tp->left_width,
						  &tp->left_height,
						  XmNleftPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_RIGHT_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->right_pixmap,
						  &tp->right_width,
						  &tp->right_height,
						  XmNrightPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_VERTICAL_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->vertical_pixmap,
						  &tp->vertical_width,
						  &tp->vertical_height,
						  XmNverticalPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_HORIZONTAL_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->horizontal_pixmap,
						  &tp->horizontal_width,
						  &tp->horizontal_height,
						  XmNhorizontalPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_BOTTOM_RAISED_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->bottom_raised_pixmap,
						  NULL,
						  NULL,
						  XmNbottomRaisedPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_TOP_RAISED_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->top_raised_pixmap,
						  NULL,
						  NULL,
						  XmNtopRaisedPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_LEFT_RAISED_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->left_raised_pixmap,
						  NULL,
						  NULL,
						  XmNleftRaisedPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_RIGHT_RAISED_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->right_raised_pixmap,
						  NULL,
						  NULL,
						  XmNrightRaisedPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_VERTICAL_RAISED_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->vertical_raised_pixmap,
						  NULL,
						  NULL,
						  XmNverticalRaisedPixmap);
    }

    if (flags & _XFE_PREPARE_TAB_HORIZONTAL_RAISED_PIXMAP)
    {
		_XfePixmapPrepare(w,
						  &tp->horizontal_raised_pixmap,
						  NULL,
						  NULL,
						  XmNhorizontalRaisedPixmap);
    }
}
/*----------------------------------------------------------------------*/
static void
DrawBackground(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
	XfeButtonWidgetClass	bwc = (XfeButtonWidgetClass) xfeButtonWidgetClass;

	/* Call the XfeButtonClass' DrawBackground() */
	(*bwc->xfe_primitive_class.draw_background)(w,event,region,clip_rect);

	/* Draw the tab */
	DrawTab(w,event,region,clip_rect);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeTab functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DrawTab(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
    XfeTabPart *		tp = _XfeTabPart(w);
    XfeButtonPart *		bp = _XfeButtonPart(w);
	Boolean				raised = _XfePointerInside(w);

	switch(tp->orientation)
	{
	case XmVERTICAL:
	{
		Pixmap vertical_pixmap;
		Pixmap top_pixmap;
		Pixmap bottom_pixmap;

		vertical_pixmap = 
			(raised && _XfePixmapGood(tp->vertical_raised_pixmap)) ?
			tp->vertical_raised_pixmap :
			tp->vertical_pixmap;
		
		top_pixmap = 
			(raised && _XfePixmapGood(tp->top_raised_pixmap)) ?
			tp->top_raised_pixmap :
			tp->top_pixmap;
		
		bottom_pixmap = 
			(raised && _XfePixmapGood(tp->bottom_raised_pixmap)) ?
			tp->bottom_raised_pixmap :
			tp->bottom_pixmap;

		/* Draw vertical pixmap if needed */
		if (_XfePixmapGood(vertical_pixmap))
		{
			FillTabVertical(w,vertical_pixmap,
							tp->vertical_width,
							tp->vertical_height);
		}

		/* Draw top pixmap if needed */
		if (_XfePixmapGood(top_pixmap) && 
			tp->top_width && tp->top_height)
		{
			XSetClipMask(XtDisplay(w),bp->pixmap_GC,None);

			XCopyArea(XtDisplay(w),
					  top_pixmap,
					  _XfePrimitiveDrawable(w),
					  bp->pixmap_GC,
					  0,0,
					  tp->top_width,
					  tp->top_height,
					  (_XfeWidth(w) - tp->top_width) / 2,
					  _XfeRectY(w));
		}

		/* Draw bottom pixmap if needed */
		if (_XfePixmapGood(bottom_pixmap) && 
			tp->bottom_width && tp->bottom_height)
		{
			XSetClipMask(XtDisplay(w),bp->pixmap_GC,None);

			XCopyArea(XtDisplay(w),
					  bottom_pixmap,
					  _XfePrimitiveDrawable(w),
					  bp->pixmap_GC,
					  0,0,
					  tp->bottom_width,
					  tp->bottom_height,
					  (_XfeWidth(w) - tp->bottom_width) / 2,

					  _XfeHeight(w) - 
					  _XfeOffsetBottom(w) -
					  tp->bottom_height);
		}
	}
	break;

	case XmHORIZONTAL:
	{
		Pixmap horizontal_pixmap;
		Pixmap right_pixmap;
		Pixmap left_pixmap;

		horizontal_pixmap = 
			(raised && _XfePixmapGood(tp->horizontal_raised_pixmap)) ?
			tp->horizontal_raised_pixmap :
			tp->horizontal_pixmap;
		
		left_pixmap = 
			(raised && _XfePixmapGood(tp->left_raised_pixmap)) ?
			tp->left_raised_pixmap :
			tp->left_pixmap;
		
		right_pixmap = 
			(raised && _XfePixmapGood(tp->right_raised_pixmap)) ?
			tp->right_raised_pixmap :
			tp->right_pixmap;
		
		/* Draw horizontal pixmap if needed */
		if (_XfePixmapGood(horizontal_pixmap))
		{
			FillTabHorizontal(w,horizontal_pixmap,
							  tp->horizontal_width,
							  tp->horizontal_height);
		}

		/* Draw top pixmap if needed */
		if (_XfePixmapGood(left_pixmap) && 
			tp->left_width && tp->left_height)
		{
			XSetClipMask(XtDisplay(w),bp->pixmap_GC,None);

			XCopyArea(XtDisplay(w),
					  left_pixmap,
					  _XfePrimitiveDrawable(w),
					  bp->pixmap_GC,
					  0,0,
					  tp->left_width,
					  tp->left_height,
					  _XfeRectX(w),
					  (_XfeHeight(w) - tp->left_height) / 2);
		}

		/* Draw right pixmap if needed */
		if (_XfePixmapGood(right_pixmap) && 
			tp->right_width && tp->right_height)
		{
			XSetClipMask(XtDisplay(w),bp->pixmap_GC,None);

			XCopyArea(XtDisplay(w),
					  right_pixmap,
					  _XfePrimitiveDrawable(w),
					  bp->pixmap_GC,
					  0,0,
					  tp->right_width,
					  tp->right_height,
					  
					  _XfeWidth(w) - 
					  _XfeOffsetRight(w) -
					  tp->right_width,

					  (_XfeHeight(w) - tp->right_height) / 2);
		}

	}
		break;
	}

}
/*----------------------------------------------------------------------*/
static void
FillTabVertical(Widget w,Pixmap pixmap,Dimension width,Dimension height)
{
    XfeTabPart *		tp = _XfeTabPart(w);
    XfeButtonPart *		bp = _XfeButtonPart(w);
	Position			y;
	Boolean				done = False;
	Dimension			copy_height;
	Dimension			max_y = _XfeHeight(w) - _XfeOffsetBottom(w);

    if (_XfePixmapGood(tp->top_pixmap))
      y = _XfeOffsetTop(w) + tp->top_height;
    else
      y = _XfeOffsetTop(w);

	while(!done)
	{
		copy_height = height;

		if ((y + copy_height) > max_y)
		{
			copy_height -= (y + copy_height - max_y);
		}

		if (copy_height)
		{
			XSetClipMask(XtDisplay(w),bp->pixmap_GC,None);

			XCopyArea(XtDisplay(w),
					  pixmap,
					  _XfePrimitiveDrawable(w),
					  bp->pixmap_GC,
					  0,0,
					  width,
					  height,
					  (_XfeWidth(w) - width) / 2,
					  y);
		}

		done = (copy_height < height);

		y += height;
	}
}
/*----------------------------------------------------------------------*/
static void
FillTabHorizontal(Widget w,Pixmap pixmap,Dimension width,Dimension height)
{
    XfeTabPart *		tp = _XfeTabPart(w);
    XfeButtonPart *		bp = _XfeButtonPart(w);
	Position			x;
	Boolean				done = False;
	Dimension			copy_width;
	Dimension			max_x = _XfeWidth(w) - _XfeOffsetRight(w);

    if (_XfePixmapGood(tp->left_pixmap))
      x = _XfeOffsetLeft(w) + tp->left_width;
    else 
      x = _XfeOffsetLeft(w);

	while(!done)
	{
		copy_width = width;

		if ((x + copy_width) > max_x)
		{
			copy_width -= (x + copy_width - max_x);
		}

		if (copy_width)
		{
			XSetClipMask(XtDisplay(w),bp->pixmap_GC,None);

			XCopyArea(XtDisplay(w),
					  pixmap,
					  _XfePrimitiveDrawable(w),
					  bp->pixmap_GC,
					  0,0,
					  width,
					  height,
					  x,
					  (_XfeHeight(w) - height) / 2);
		}

		done = (copy_width < width);

		x += width;
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTab Public Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
Widget
XfeCreateTab(Widget parent,char *name,Arg *args,Cardinal count)
{
    return (XtCreateWidget(name,xfeTabWidgetClass,parent,args,count));
}
/*----------------------------------------------------------------------*/
