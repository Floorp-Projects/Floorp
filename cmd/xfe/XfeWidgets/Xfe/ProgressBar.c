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
/* Name:		<Xfe/ProgressBar.c>										*/
/* Description:	XfeProgressBar widget source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include <Xfe/ProgressBarP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE1  "Widget is not a XfeProgressBar."
#define MESSAGE2  "XmNstartPercent must be >= than 0."
#define MESSAGE3  "XmNstartPercent must be <= than 100."
#define MESSAGE4  "XmNendPercent must be >= than 0."
#define MESSAGE5  "XmNendPercent must be <= than 100."
#define MESSAGE6  "XmNendPercent must be greater than XmNstartPercent."
#define MESSAGE7  "XmNcylonRunning is a read-only resource."
#define MESSAGE8  "XmNcylonWidth must be >= 1."
#define MESSAGE9  "XmNcylonWidth must be <= 50."
#define MESSAGE10 "XmNcylonOffset must be >= 1."
#define MESSAGE11 "XmNcylonOffset must be <= 50."

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize	(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy		(Widget);
static Boolean	SetValues	(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	LayoutComponents	(Widget);
static void	DrawComponents		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeProgressBar functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	BarDraw				(Widget,Region,XRectangle *);
static void	BarLayout			(Widget);
static void	CheckPercentages	(Widget);
static void	CheckCylonWidth		(Widget);
static void	CheckCylonOffset	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc cylon functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			CylonTimeout		(XtPointer,XtIntervalId *);
static void			RemoveTimeout		(Widget);
static void			AddTimeout			(Widget);
static void			CheckCylonWidth		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeProgressBar Resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
    /* Resources */
    { 
		XmNbarColor,
		XmCBarColor,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfeProgressBarRec , xfe_progress_bar . bar_color),
		XmRCallProc, 
		(XtPointer) _XfeCallProcSelectPixel
    },
    { 
		XmNstartPercent,
		XmCStartPercent,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeProgressBarRec , xfe_progress_bar . start_percent),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNendPercent,
		XmCEndPercent,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeProgressBarRec , xfe_progress_bar . end_percent),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNcylonInterval,
		XmCCylonInterval,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeProgressBarRec , xfe_progress_bar . cylon_interval),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_CYLON_INTERVAL
    },
    { 
		XmNcylonOffset,
		XmCCylonOffset,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeProgressBarRec , xfe_progress_bar . cylon_offset),
		XmRImmediate, 
		(XtPointer) 2
    },
    { 
		XmNcylonRunning,
		XmCReadOnly,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeProgressBarRec , xfe_progress_bar . cylon_running),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNcylonWidth,
		XmCCylonWidth,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeProgressBarRec , xfe_progress_bar . cylon_width),
		XmRImmediate, 
		(XtPointer) 20
    },

	/* Force margins to 0 */
	{ 
		XmNmarginBottom,
		XmCMarginBottom,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeProgressBarRec , xfe_primitive . margin_bottom),
		XmRImmediate, 
		(XtPointer) 0
	},
	{ 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeProgressBarRec , xfe_primitive . margin_left),
		XmRImmediate, 
		(XtPointer) 0
	},
	{ 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeProgressBarRec , xfe_primitive . margin_right),
		XmRImmediate, 
		(XtPointer) 0
	},
	{ 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeProgressBarRec , xfe_primitive . margin_top),
		XmRImmediate, 
		(XtPointer) 0
	},
    
    /* Force the shadow type to XmSHADOW_IN */
    { 
		XmNshadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfeProgressBarRec , xfe_primitive . shadow_type),
		XmRImmediate, 
		(XtPointer) XmSHADOW_IN
	},

    /* Force buffer type to XmBUFFER_PRIVATE */
    { 
		XmNbufferType,
		XmCBufferType,
		XmRBufferType,
		sizeof(unsigned char),
		XtOffsetOf(XfeProgressBarRec , xfe_primitive . buffer_type),
		XmRImmediate, 
		(XtPointer) XmBUFFER_PRIVATE
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeProgressBar widget class record initialization					*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(progressbar,ProgressBar) =
{
    {
		/* Core Part */
		(WidgetClass) &xfeLabelClassRec,		/* superclass         	*/
		"XfeProgressBar",						/* class_name			*/
		sizeof(XfeProgressBarRec),				/* widget_size        	*/
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
		XmInheritBorderHighlight,				/* border_highlight 	*/
		XmInheritBorderUnhighlight,				/* border_unhighlight 	*/
		XtInheritTranslations,                  /* translations       	*/
		NULL,									/* arm_and_activate   	*/
		NULL,									/* syn resources      	*/
		0,										/* num syn_resources  	*/
		NULL,									/* extension          	*/
    },

    /* XfePrimitive Part */
    {
		XfeInheritBitGravity,					/* bit_gravity			*/
		XfeInheritPreferredGeometry,			/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		NULL,									/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		XfeInheritDrawBackground,				/* draw_background		*/
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

    /* XfeProgressBar Part */
    {
		NULL,									/* extension            */
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeProgressBarWidgetClass declaration.								*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(progressbar,ProgressBar);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeProgressBarPart * pp = _XfeProgressBarPart(nw);

	/* Check the percentages */
	CheckPercentages(nw);

	/* Check cylon width */
	CheckCylonWidth(nw);

	/* Check cylon offset */
	CheckCylonOffset(nw);

    /* Allocate the bar GCs */
    pp->bar_GC = XfeAllocateColorGc(nw,pp->bar_color,None,True);
    
    pp->bar_insens_GC = XfeAllocateColorGc(nw,pp->bar_color,
										   _XfeBackgroundPixel(nw),
										   False);

	/* Initialize private members */
	pp->timer_id		= 0;
	pp->cylon_direction	= True;
	pp->cylon_index		= 0;

    /* Finish of initialization */
    _XfePrimitiveChainInitialize(rw,nw,xfeProgressBarWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeProgressBarPart * pp = _XfeProgressBarPart(w);

    /* Release GCs */
    XtReleaseGC(w,pp->bar_GC);
    XtReleaseGC(w,pp->bar_insens_GC);

	/* Remove the timeout if needed */
	RemoveTimeout(w);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeProgressBarPart *	np = _XfeProgressBarPart(nw);
    XfeProgressBarPart *	op = _XfeProgressBarPart(ow);
    Boolean					bar_gc_flag = False;
    Boolean					bar_insens_gc_flag = False;
	Boolean					stop_cylon = False;

    /* cylon_running */
    if (np->cylon_running != op->cylon_running)
    {
		np->cylon_running = op->cylon_running;

		_XfeWarning(nw,MESSAGE7);
    }

    /* cylon_width */
    if (np->cylon_width != op->cylon_width)
    {
		/* Check new cylon width */
		CheckCylonWidth(nw);

		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* cylon_offset */
    if (np->cylon_offset != op->cylon_offset)
    {
		/* Check new cylon offset */
		CheckCylonOffset(nw);

		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* bar_color */
    if (np->bar_color != op->bar_color)
    {
		bar_gc_flag = True;   
		bar_insens_gc_flag = True;   

		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* background */
    if (_XfeBackgroundPixel(nw) != _XfeBackgroundPixel(ow))
    {
		bar_insens_gc_flag = True;   

		if (!_XfeSensitive(nw))
		{
			_XfeConfigFlags(nw) |= XfeConfigExpose;
		}
    }

    /* start_percent */
    if (np->start_percent != op->start_percent)
    {
		_XfeConfigFlags(nw) |= (XfeConfigExpose|XfeConfigLayout);   

		CheckPercentages(nw);

		/* Stop the cylon since a change in percentage occured */
		stop_cylon = True;
    }

    /* end_percent */
    if (np->end_percent != op->end_percent)
    {
		_XfeConfigFlags(nw) |= (XfeConfigExpose|XfeConfigLayout);   

		CheckPercentages(nw);

		/* Stop the cylon since a change in percentage occured */
		stop_cylon = True;
    }

	/* Stop the cylon if needed */
	if (stop_cylon)
	{
		XfeProgressBarCylonStop(nw);
	}

    /* Update the bar GC */
    if (bar_gc_flag)
    {
		/* Release the old bar GC */
		XtReleaseGC(nw,np->bar_GC);
	
	/* Allocate the new bar GC */
		np->bar_GC = XfeAllocateColorGc(nw,np->bar_color,None,True);
    }

    /* Update the bar insensitive GC */
    if (bar_insens_gc_flag)
    {
		/* Release the old bar insensitive GC */
		XtReleaseGC(nw,np->bar_insens_GC);
	
		/* Allocate the new bar insensitive GC */
		np->bar_insens_GC = XfeAllocateColorGc(nw,np->bar_color,
											   _XfeBackgroundPixel(nw),
											   False);
    }

    return _XfePrimitiveChainSetValues(ow,rw,nw,xfeProgressBarWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
	/* Invoke layout_string method */
	_XfeLabelLayoutString(w);

	/* Layout the bar */
    BarLayout(w);
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
	/* Draw the bar first */
    BarDraw(w,region,clip_rect);

	/* Invoke draw_string method */
	_XfeLabelDrawString(w,event,region,clip_rect);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeProgressBar functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CheckPercentages(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

    /* Check start percent */
    if (pp->start_percent < 0)
    {
		pp->start_percent = 0;
		_XfeWarning(w,MESSAGE2);
    }
    else if (pp->start_percent > 100)
    {
		pp->start_percent = 100;
		_XfeWarning(w,MESSAGE3);
    }

    /* Check end percent */
    if (pp->end_percent < 0)
    {
		pp->end_percent = 0;
		_XfeWarning(w,MESSAGE4);
    }
    else if (pp->end_percent > 100)
    {
		pp->end_percent = 100;
		_XfeWarning(w,MESSAGE5);
    }

    /* Check whether end > start */
    if (pp->end_percent < pp->start_percent)
    {
		pp->end_percent = pp->start_percent;
		_XfeWarning(w,MESSAGE4);
    }
}
/*----------------------------------------------------------------------*/
static void
CheckCylonWidth(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

    /* Check start percent */
    if (pp->cylon_width < 1)
    {
		pp->cylon_width = 1;

		_XfeWarning(w,MESSAGE8);
    }
    else if (pp->cylon_width > 50)
    {
		pp->cylon_width = 50;

		_XfeWarning(w,MESSAGE9);
    }
}
/*----------------------------------------------------------------------*/
static void
CheckCylonOffset(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

    /* Check start percent */
    if (pp->cylon_offset < 1)
    {
		pp->cylon_offset = 1;

		_XfeWarning(w,MESSAGE10);
    }
    else if (pp->cylon_offset > 50)
    {
		pp->cylon_offset = 50;

		_XfeWarning(w,MESSAGE11);
    }
}
/*----------------------------------------------------------------------*/
static void
BarDraw(Widget w,Region region,XRectangle * clip_rect)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

    /* Draw the bar if needed */
    if (pp->start_percent != pp->end_percent)
    {
		XFillRectangle(XtDisplay(w),
					   _XfePrimitiveDrawable(w),
					   _XfeSensitive(w) ? pp->bar_GC : pp->bar_insens_GC,
					   pp->bar_rect.x,
					   pp->bar_rect.y,
					   pp->bar_rect.width,
					   pp->bar_rect.height);
    }
}
/*----------------------------------------------------------------------*/
static void
BarLayout(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);
    float		x1;
    float		x2;

#if 0
    x1 = (float) pp->start_percent / 100.0 * (float) _XfeRectWidth(w);
    x2 = (float) pp->end_percent / 100.0 * (float) _XfeRectWidth(w);
#else
    x1 = ((float) pp->start_percent * (float) _XfeRectWidth(w)) / 100.0;
    x2 = ((float) pp->end_percent * (float) _XfeRectWidth(w)) / 100.0;
#endif

    pp->bar_rect.x = _XfeRectX(w) + (int) x1;
    pp->bar_rect.y = _XfeRectY(w);

    pp->bar_rect.width = (int) (x2 - x1);
    pp->bar_rect.height =_XfeRectHeight(w);

#if 0
	printf("x1 = %f,x2 = %f\t(x2 - x1) = %f =(int) %d\n",
		   x1,
		   x2,
		   (x2 - x1),
		   (int) (x2 - x1));
#endif
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc cylon functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CylonTimeout(XtPointer closure,XtIntervalId * id)
{
    Widget					w = (Widget) closure;
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);
	
	/*
	 *    Always clear timer_id, so we don't kill someone else's timer
	 *    by accident later.
	 */
	pp->timer_id = 0;

	/* Make sure the widget is still alive */
	if (!_XfeIsAlive(w))
	{
		return;
	}

	/* Make sure the cylon is running */
	if (!pp->cylon_running)
	{
		return;
	}

	/* Tick once */
	XfeProgressBarCylonTick(w);

	/* Add the timeout again */
	AddTimeout(w);
}
/*----------------------------------------------------------------------*/
static void
AddTimeout(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

	assert( _XfeIsAlive(w) );

	pp->timer_id = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
								   pp->cylon_interval,
								   CylonTimeout,
								   (XtPointer) w);
}
/*----------------------------------------------------------------------*/
static void
RemoveTimeout(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

	if (pp->timer_id)
	{
		XtRemoveTimeOut(pp->timer_id);

		pp->timer_id = 0;
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeProgressBar public functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
/*extern*/ Widget
XfeCreateProgressBar(Widget parent,char *name,Arg *args,Cardinal count)
{
    return (XtCreateWidget(name,xfeProgressBarWidgetClass,parent,args,count));
}
/*----------------------------------------------------------------------*/
/*extern*/ void	
XfeProgressBarSetPercentages(Widget w,int start,int end)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

    assert( _XfeIsAlive(w) );
    assert( XfeIsProgressBar(w));
    assert( start >= 0 );
    assert( end <= 100 );
    assert( end >= start );

	/* Stop the cylon if needed */
	if (pp->cylon_running)
	{
		XfeProgressBarCylonStop(w);
	}

    /* Assign the new percentages */
    pp->start_percent = start;
    pp->end_percent = end;

    /* Make sure the new percentages are ok */
    CheckPercentages(w);

    /* Relayout the widget */
    _XfePrimitiveLayoutComponents(w);
	
    /* Erase the old stuff */
    _XfePrimitiveClearBackground(w);

	/* Make sure the widget is realized before drawing anything */
	if (!XtIsRealized(w))
	{
		return;
	}

    /* Redraw the everything */
    _XfePrimitiveDrawEverything(w,NULL,NULL);
	
    /* Expose the widget */
    XfeExpose(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
/*extern*/ void	
XfeProgressBarSetComponents(Widget w,XmString xm_label,int start,int end)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);
    XfeLabelPart *			lp = _XfeLabelPart(w);
	
    assert( _XfeIsAlive(w) );
    assert( XfeIsProgressBar(w));
    assert( start >= 0 );
    assert( end <= 100 );
    assert( end >= start );
    assert( xm_label != NULL );

	/* Stop the cylon if needed */
	if (pp->cylon_running)
	{
		XfeProgressBarCylonStop(w);
	}

    /* Free the old string */
    XmStringFree(lp->label_string);

    /* Make a copy of the given string */
	if (xm_label)
	{
		lp->label_string = XfeXmStringCopy(w,xm_label,XtName(w));
	}
	else
	{
		lp->label_string = XmStringCreate("",XmFONTLIST_DEFAULT_TAG);
	}

    /* Assign the new percentages */
    pp->start_percent = start;
    pp->end_percent = end;

    /* Make sure the new percentages are ok */
    CheckPercentages(w);

    /* Prepare the widget */
	_XfePrimitivePrepareComponents(w,XfePrepareAll);

    /* Relayout the widget */
    _XfePrimitiveLayoutComponents(w);

	/* Make sure the widget is realized before drawing anything */
	if (!XtIsRealized(w))
	{
		return;
	}

    /* Erase the old stuff */
    _XfePrimitiveClearBackground(w);

    /* Redraw the buffer */
    _XfePrimitiveDrawEverything(w,NULL,NULL);

    /* Draw the buffer onto the window */
    XfeExpose(w,NULL,NULL);
}
/*----------------------------------------------------------------------*/
void
XfeProgressBarCylonStart(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

	assert( _XfeIsAlive(w) );
	assert( XfeIsProgressBar(w) );

	/* Make sure the cylon is not already running */
	if (pp->cylon_running)
	{
		return;
	}

	pp->cylon_running = True;

	/* Update the percentages */
	XfeProgressBarSetPercentages(w,
								 pp->cylon_index,
								 pp->cylon_index + pp->cylon_width);

	/* Add the timeout */
 	AddTimeout(w);
}
/*----------------------------------------------------------------------*/
void
XfeProgressBarCylonStop(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

	assert( _XfeIsAlive(w) );
	assert( XfeIsProgressBar(w) );

	/* Make sure the cylon is running */
	if (!pp->cylon_running)
	{
		return;
	}

	pp->cylon_running = False;

	/* Remove the timeout if needed */
 	RemoveTimeout(w);
}
/*----------------------------------------------------------------------*/
void
XfeProgressBarCylonReset(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);

	assert( _XfeIsAlive(w) );
	assert( XfeIsProgressBar(w) );

	pp->cylon_index = 0;

	/* Update the percentages */
	XfeProgressBarSetPercentages(w,
								 pp->cylon_index,
								 pp->cylon_index + pp->cylon_width);
}
/*----------------------------------------------------------------------*/
void
XfeProgressBarCylonTick(Widget w)
{
    XfeProgressBarPart *	pp = _XfeProgressBarPart(w);
	
	assert( _XfeIsAlive(w) );
	assert( XfeIsProgressBar(w) );

	/* If the cylon is not running, start it */
	if (!pp->cylon_running)
	{
		XfeProgressBarCylonStart(w);
	}

	/* Update the cylon position */
	if (pp->cylon_direction)
	{
		pp->cylon_index += pp->cylon_offset;
	}
	else
	{
		pp->cylon_index -= pp->cylon_offset;
	}

	/* Check for cylon edge positions */
	if (pp->cylon_index > (100 - pp->cylon_width))
	{
   		pp->cylon_index = 100 - pp->cylon_width;
		
		pp->cylon_direction = False;
	}
	else if (pp->cylon_index < 0)
	{
  		pp->cylon_index = 0;

		pp->cylon_direction = True;
	}

	/* Update the percentages */
	XfeProgressBarSetPercentages(w,
								 pp->cylon_index,
								 pp->cylon_index + pp->cylon_width);
}
/*----------------------------------------------------------------------*/
