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
/* Name:		<Xfe/Oriented.c>										*/
/* Description:	XfeToolScroll widget source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/OrientedP.h>
#include <Xfe/Primitive.h>

#define MESSAGE1 "Widget is not an XfeOriented."
#define MESSAGE2 "XmNcursorOn is a read-only resource."
#define MESSAGE3 "The XfeOriented widget is not alive."
#define MESSAGE4 "The descendant '%s' is not alive."

#define DESCENDANT_CURSOR_EVENTS	(EnterWindowMask |		\
									 LeaveWindowMask |		\
									 PointerMotionMask)

#define DESCENDANT_DRAG_EVENTS		(ButtonPressMask | 		\
									 ButtonReleaseMask | 	\
									 Button1MotionMask)

#define CURSOR_EVENTS				(EnterWindowMask |		\
									 LeaveWindowMask |		\
									 PointerMotionMask)

#define DRAG_EVENTS					(ButtonPressMask | 		\
									 ButtonReleaseMask | 	\
									 Button1MotionMask)

#define HOR_CURSOR		"sb_h_double_arrow"
#define VER_CURSOR		"sb_v_double_arrow"

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void		ClassPartInit	(WidgetClass);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ConstraintInitialize(Widget,Widget,ArgList,Cardinal *);
static Boolean	ConstraintSetValues	(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	EnterProc				(Widget,Widget,int,int);
static void 	LeaveProc				(Widget,Widget,int,int);
static void 	DescendantEnterProc		(Widget,Widget,int,int);
static void 	DescendantLeaveProc		(Widget,Widget,int,int);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc functions														*/
/*																		*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Event handlers														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	CursorEH			(Widget,XtPointer,XEvent *,Boolean *);
static void		DragEH				(Widget,XtPointer,XEvent *,Boolean *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Descendant event handlers											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	DescendantCursorEH	(Widget,XtPointer,XEvent *,Boolean *);
static void 	DescendantDragEH	(Widget,XtPointer,XEvent *,Boolean *);

static void 	DescendantPrimitiveCursorCB	(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented Resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Orientation resources */
    { 
		XmNorientation,
		XmCOrientation,
		XmROrientation,
		sizeof(unsigned char),
		XtOffsetOf(XfeOrientedRec , xfe_oriented . orientation),
		XmRImmediate, 
		(XtPointer) XmHORIZONTAL
    },

    /* Drag resources */
	{ 
		XmNallowDrag,
		XmCBoolean,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeOrientedRec , xfe_oriented . allow_drag),
		XmRImmediate, 
		(XtPointer) False
    },
	{ 
		XmNdragInProgress,
		XmCDragInProgress,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeOrientedRec , xfe_oriented . drag_in_progress),
		XmRImmediate, 
		(XtPointer) False
    },

	{ 
		XmNcursorOn,
		XmCReadOnly,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeOrientedRec , xfe_oriented . cursor_on),
		XmRImmediate, 
		(XtPointer) False
    },

    /* Cursor resources */
	{ 
		XmNverticalCursor,
		XmCVerticalCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfeOrientedRec , xfe_oriented . vertical_cursor),
		XmRString, 
		VER_CURSOR
    },
	{ 
		XmNhorizontalCursor,
		XmCHorizontalCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfeOrientedRec , xfe_oriented . horizontal_cursor),
		XmRString, 
		HOR_CURSOR
    },

	/* Spacing resources */
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeOrientedRec , xfe_oriented . spacing),
		XmRImmediate, 
		(XtPointer) 1
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
	{ 
		XmNspacing,
		sizeof(Dimension),
		XtOffsetOf(XfeOrientedRec , xfe_oriented . spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented constraint resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource constraint_resources[] = 
{
    { 
		XmNallowDrag,
		XmCBoolean,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeOrientedConstraintRec , xfe_oriented . allow_drag),
		XmRImmediate,
		(XtPointer) False
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(oriented,Oriented) =
{
	{
		(WidgetClass) &xfeManagerClassRec,		/* superclass       	*/
		"XfeOriented",							/* class_name       	*/
		sizeof(XfeOrientedRec),					/* widget_size      	*/
		NULL,									/* class_initialize 	*/
		ClassPartInit,							/* class_part_initialize*/
		FALSE,                                  /* class_inited     	*/
		Initialize,                             /* initialize       	*/
		NULL,                                   /* initialize_hook  	*/
		XtInheritRealize,						/* realize          	*/
		NULL,									/* actions          	*/
		0,										/* num_actions			*/
		resources,                              /* resources        	*/
		XtNumber(resources),                    /* num_resources    	*/
		NULLQUARK,                              /* xrm_class        	*/
		TRUE,                                   /* compress_motion  	*/
		XtExposeCompressMaximal,                /* compress_exposure	*/
		TRUE,                                   /* compress_enterleave	*/
		FALSE,                                  /* visible_interest 	*/
		Destroy,								/* destroy          	*/
		XtInheritResize,                        /* resize           	*/
		XtInheritExpose,						/* expose           	*/
		SetValues,                              /* set_values       	*/
		NULL,                                   /* set_values_hook  	*/
		XtInheritSetValuesAlmost,				/* set_values_almost	*/
		NULL,									/* get_values_hook  	*/
		NULL,                                   /* accexfe_focus     	*/
		XtVersion,                              /* version          	*/
		NULL,                                   /* callback_private 	*/
		XtInheritTranslations,					/* tm_table				*/
		XtInheritQueryGeometry,					/* query_geometry   	*/
		XtInheritDisplayAccelerator,            /* display accel    	*/
		NULL,                                   /* extension        	*/
	},

	/* Composite Part */
	{
		XtInheritGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
	},

	/* Constraint Part */
	{
		constraint_resources,					/* constraint res		*/
		XtNumber(constraint_resources),			/* num constraint res	*/
		sizeof(XfeOrientedConstraintRec),		/* constraint size		*/
		ConstraintInitialize,					/* init proc			*/
		NULL,									/* destroy proc			*/
		ConstraintSetValues,					/* set values proc		*/
		NULL,                                   /* extension			*/
	},

	/* XmManager Part */
	{
		XtInheritTranslations,					/* tm_table				*/
		syn_resources,							/* syn resources		*/
		XtNumber(syn_resources),				/* num syn_resources	*/
		NULL,                                   /* syn_cont_resources  	*/
		0,                                      /* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process      	*/
		NULL,                                   /* extension           	*/
	},

	/* XfeManager Part 	*/
	{
		XfeInheritBitGravity,					/* bit_gravity			*/
		XfeInheritPreferredGeometry,			/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		NULL,									/* accept_child			*/
		NULL,									/* insert_child			*/
		NULL,									/* delete_child			*/
		NULL,									/* change_managed		*/
		NULL,									/* prepare_components	*/
		NULL,									/* layout_components	*/
		NULL,									/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		NULL,									/* draw_components		*/
		False,									/* count_layable_children*/
		NULL,									/* child_is_layable		*/
		NULL,									/* extension          	*/
	},

	/* XfeOriented Part */
	{
		EnterProc,								/* enter				*/
		LeaveProc,								/* leave				*/
		NULL,									/* motion				*/
		NULL,									/* drag_start			*/
		NULL,									/* drag_end				*/
		NULL,									/* drag_motion			*/
		DescendantEnterProc,					/* des_enter			*/
		DescendantLeaveProc,					/* des_leave			*/
		NULL,									/* des_motion			*/
		NULL,									/* des_drag_start		*/
		NULL,									/* des_drag_end			*/
		NULL,									/* des_drag_motion		*/
		NULL,									/* extension          	*/
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeOrientedWidgetClassdeclaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(oriented,Oriented);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassPartInit(WidgetClass wc)
{
    XfeOrientedWidgetClass cc = (XfeOrientedWidgetClass) wc;
    XfeOrientedWidgetClass sc = (XfeOrientedWidgetClass) wc->core_class.superclass;

    _XfeResolve(cc,sc,xfe_oriented_class,enter,
				XfeInheritEnter);

    _XfeResolve(cc,sc,xfe_oriented_class,leave,
				XfeInheritLeave);

    _XfeResolve(cc,sc,xfe_oriented_class,motion,
				XfeInheritMotion);

    _XfeResolve(cc,sc,xfe_oriented_class,drag_start,
				XfeInheritDragStart);

    _XfeResolve(cc,sc,xfe_oriented_class,drag_end,
				XfeInheritDragEnd);

    _XfeResolve(cc,sc,xfe_oriented_class,drag_motion,
				XfeInheritDragMotion);
   
    _XfeResolve(cc,sc,xfe_oriented_class,descendant_enter,
				XfeInheritDescendantEnter);

    _XfeResolve(cc,sc,xfe_oriented_class,descendant_leave,
				XfeInheritDescendantLeave);

    _XfeResolve(cc,sc,xfe_oriented_class,descendant_motion,
				XfeInheritDescendantMotion);

    _XfeResolve(cc,sc,xfe_oriented_class,descendant_drag_start,
				XfeInheritDescendantDragStart);

    _XfeResolve(cc,sc,xfe_oriented_class,descendant_drag_end,
				XfeInheritDescendantDragEnd);

    _XfeResolve(cc,sc,xfe_oriented_class,descendant_drag_motion,
				XfeInheritDescendantDragMotion);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeOrientedPart *		op = _XfeOrientedPart(nw);

    /* Make sure rep types are ok */
    XfeRepTypeCheck(nw,XmROrientation,&op->orientation,XmHORIZONTAL);

	/* Set the allow_drag state for the first time */
	XfeOrientedSetAllowDrag(nw,op->allow_drag);

    /* Initialize other private members */
	op->drag_start_x = 0;
	op->drag_start_y = 0;

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeOrientedWidgetClass);
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
    XfeOrientedPart *		np = _XfeOrientedPart(nw);
    XfeOrientedPart *		op = _XfeOrientedPart(ow);

	/* cursor_on */
    if (np->cursor_on != op->cursor_on)
    {
		np->cursor_on = op->cursor_on;

		_XfeWarning(nw,MESSAGE2);
    }

	/* orientation */
    if (np->orientation != op->orientation)
    {
		/* Make sure the new orientation is ok */
		XfeRepTypeCheck(nw,XmROrientation,&np->orientation,XmHORIZONTAL);

		_XfemConfigFlags(nw) |= XfeConfigGLE;
    }

	/* spacing */
    if (np->spacing != op->spacing)
    {
		_XfemConfigFlags(nw) |= XfeConfigGLE;
    }

	/* allow_drag */
    if (np->allow_drag != op->allow_drag)
    {
		XfeOrientedSetAllowDrag(nw,np->allow_drag);
    }

    return _XfeManagerChainSetValues(ow,rw,nw,xfeOrientedWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ConstraintInitialize(Widget rc,Widget nc,ArgList av,Cardinal * ac)
{
  	Widget						w = _XfeParent(nc);
	XfeOrientedConstraintPart *	cp = _XfeOrientedConstraintPart(nc);

	XfeOrientedDescendantSetAllowDrag(w,nc,cp->allow_drag);

	/* Finish constraint initialization */
	_XfeConstraintChainInitialize(rc,nc,xfeOrientedWidgetClass);
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValues(Widget oc,Widget rc,Widget nc,ArgList av,Cardinal * ac)
{
	Widget						w = XtParent(nc);
 	XfeOrientedConstraintPart *	ncp = _XfeOrientedConstraintPart(nc);
 	XfeOrientedConstraintPart *	ocp = _XfeOrientedConstraintPart(oc);

	/* allow_drag */
	if (ncp->allow_drag != ocp->allow_drag)
	{
		XfeOrientedDescendantSetAllowDrag(w,nc,ncp->allow_drag);
	}

	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(oc,rc,nc,xfeOrientedWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
EnterProc(Widget w,Widget descendant,int x,int y)
{
	_XfeOrientedSetCursorState(w,True);
}
/*----------------------------------------------------------------------*/
static void
LeaveProc(Widget w,Widget descendant,int x,int y)
{
	_XfeOrientedSetCursorState(w,False);
}
/*----------------------------------------------------------------------*/
static void
DescendantEnterProc(Widget w,Widget descendant,int x,int y)
{
	_XfeOrientedSetCursorState(w,True);
}
/*----------------------------------------------------------------------*/
static void
DescendantLeaveProc(Widget w,Widget descendant,int x,int y)
{
	_XfeOrientedSetCursorState(w,False);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Misc functions														*/
/*																		*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Dragging event handlers												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CursorEH(Widget			oriented,
		 XtPointer		client_data,
		 XEvent *		event,
		 Boolean *		cont)
{
    Widget				w = (Widget) client_data;
    XfeOrientedPart *	op = _XfeOrientedPart(w);
	int					x;
	int					y;

	/* If a drag is in progress, dont change the cursor */
	if (op->drag_in_progress)
	{
		return;
	}

	/* Obtain the coords for the event */
	XfeEventGetXY(event,&x,&y);

	/* Enter */
	if (event->type == EnterNotify)
	{
		_XfeOrientedEnter(w,NULL,x,y);
	}
	/* Leave */
	else if (event->type == LeaveNotify)
	{
		_XfeOrientedLeave(w,NULL,x,y);
	}
	/* Motion */
	else if (event->type == MotionNotify)
	{
		_XfeOrientedMotion(w,NULL,x,y);
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/
static void
DragEH(Widget		oriented,
	   XtPointer	client_data,
	   XEvent *		event,
	   Boolean *	cont)
{
    Widget				w = (Widget) client_data;
    XfeOrientedPart *	op = _XfeOrientedPart(w);
	int					x;
	int					y;
	
	/* Obtain the coords for the event */
	XfeEventGetXY(event,&x,&y);

	switch(event->type)
	{
	case ButtonPress:

		/* Make sure a drag is not already in progress */
		if (!op->drag_in_progress)
		{
			op->drag_in_progress = True;

			op->drag_start_x = x;
			op->drag_start_y = y;

			_XfeOrientedDragStart(w,NULL,x,y);
		}

		break;
		
	case ButtonRelease:

		/* Make sure a drag is indeed in progress */
		if (op->drag_in_progress)
		{
			op->drag_in_progress = False;
			
			_XfeOrientedDragEnd(w,NULL,x,y);
		}
		
		break;
		
	case MotionNotify:

		/* Make sure a drag is indeed in progress */
		if (op->drag_in_progress)
		{
			op->drag_in_progress = True;

			_XfeOrientedDragMotion(w,NULL,x,y);
		}
		
		break;
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Descendant event handlers											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DescendantCursorEH(Widget		descendant,
				   XtPointer	client_data,
				   XEvent *		event,
				   Boolean *	cont)
{
    Widget				w = (Widget) client_data;
    XfeOrientedPart *	op = _XfeOrientedPart(w);
	int					root_x;
	int					root_y;
	int					x;
	int					y;

	/* If a drag is in progress, dont change the cursor */
	if (op->drag_in_progress)
	{
		return;
	}

	/* Obtain the root coords for the event */
	XfeEventGetRootXY(event,&root_x,&root_y);

	/* Compute coords relative to the oriented widget */
	x = root_x - XfeRootX(w);
	y = root_y - XfeRootY(w);

	/* Enter */
	if (event->type == EnterNotify)
	{
		_XfeOrientedDescendantEnter(w,descendant,x,y);
	}
	/* Leave */
	else if (event->type == LeaveNotify)
	{
		_XfeOrientedDescendantLeave(w,descendant,x,y);
	}
	/* Motion */
	else if (event->type == MotionNotify)
	{
		_XfeOrientedDescendantMotion(w,descendant,x,y);
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/
static void
DescendantDragEH(Widget		descendant,
				 XtPointer	client_data,
				 XEvent *	event,
				 Boolean *	cont)
{
    Widget				w = (Widget) client_data;
    XfeOrientedPart *	op = _XfeOrientedPart(w);
	int					root_x;
	int					root_y;
	int					x;
	int					y;
	
	/* Obtain the root coords for the event */
	XfeEventGetRootXY(event,&root_x,&root_y);

	/* Compute coords relative to the oriented widget */
	x = root_x - XfeRootX(w);
	y = root_y - XfeRootY(w);
		
	switch(event->type)
	{
	case ButtonPress:

		/* Make sure a drag is not already in progress */
		if (!op->drag_in_progress)
		{
			op->drag_in_progress = True;

			op->drag_start_x = x;
			op->drag_start_y = y;

			_XfeOrientedDescendantDragStart(w,descendant,x,y);
		}

		break;
		
	case ButtonRelease:

		/* Make sure a drag is indeed in progress */
		if (op->drag_in_progress)
		{
			op->drag_in_progress = False;
			
			_XfeOrientedDescendantDragEnd(w,descendant,x,y);
		}

		break;
		
	case MotionNotify:

		/* Make sure a drag is indeed in progress */
		if (op->drag_in_progress)
		{
			op->drag_in_progress = True;

			_XfeOrientedDescendantDragMotion(w,descendant,x,y);
		}
		
		break;
	}

	*cont = True;
}
/*----------------------------------------------------------------------*/
static void
DescendantPrimitiveCursorCB(Widget		descendant,
							XtPointer	client_data,
							XtPointer	call_data)
{
    Widget					w = (Widget) client_data;
    XfeOrientedPart *		op = _XfeOrientedPart(w);

	XmAnyCallbackStruct *	data = (XmAnyCallbackStruct *) call_data;
	XEvent *				event = data->event;

	int						root_x;
	int						root_y;
	int						x;
	int						y;

	/* If a drag is in progress, dont change the cursor */
	if (op->drag_in_progress)
	{
		return;
	}

	/* Obtain the root coords for the event */
	XfeEventGetRootXY(event,&root_x,&root_y);

	/* Compute coords relative to the oriented widget */
	x = root_x - XfeRootX(w);
	y = root_y - XfeRootY(w);

	/* Enter */
	if (data->reason == XmCR_ENTER)
	{
		_XfeOrientedDescendantEnter(w,descendant,x,y);
	}
	/* Leave */
	else if (data->reason == XmCR_LEAVE)
	{
		_XfeOrientedDescendantLeave(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedEnter(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.enter)
	{
		(*oc->xfe_oriented_class.enter)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedLeave(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.leave)
	{
		(*oc->xfe_oriented_class.leave)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedMotion(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.motion)
	{
		(*oc->xfe_oriented_class.motion)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDragStart(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.drag_start)
	{
		(*oc->xfe_oriented_class.drag_start)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDragEnd(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.drag_end)
	{
		(*oc->xfe_oriented_class.drag_end)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDragMotion(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.drag_motion)
	{
		(*oc->xfe_oriented_class.drag_motion)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDescendantEnter(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.descendant_enter)
	{
		(*oc->xfe_oriented_class.descendant_enter)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDescendantLeave(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.descendant_leave)
	{
		(*oc->xfe_oriented_class.descendant_leave)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDescendantMotion(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.descendant_motion)
	{
		(*oc->xfe_oriented_class.descendant_motion)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDescendantDragStart(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.descendant_drag_start)
	{
		(*oc->xfe_oriented_class.descendant_drag_start)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDescendantDragEnd(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.descendant_drag_end)
	{
		(*oc->xfe_oriented_class.descendant_drag_end)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedDescendantDragMotion(Widget w,Widget descendant,int x,int y)
{
	XfeOrientedWidgetClass	oc = (XfeOrientedWidgetClass) XtClass(w);

	if (oc->xfe_oriented_class.descendant_drag_motion)
	{
		(*oc->xfe_oriented_class.descendant_drag_motion)(w,descendant,x,y);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented private Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeOrientedSetCursorState(Widget w,Boolean state)
{
    XfeOrientedPart *	op = _XfeOrientedPart(w);
	Cursor				cursor;

	assert( XfeIsOriented(w) );
	assert( _XfeIsAlive(w) );

	/* Vertical */
	if (op->orientation == XmVERTICAL)
	{
		cursor = op->vertical_cursor;
	}
	/* Horizontal */
	else
	{
		cursor = op->horizontal_cursor;
	}

	/* Make sure the cursor is good */
	if (!_XfeCursorGood(cursor))
	{
		return;
	}

	/* On */
	if (state)
	{
		/* Turn cursor on if needed */
		if (!op->cursor_on)
		{
			op->cursor_on = True;

			XfeCursorDefine(w,cursor);
		}
	}
	/* Off */
	else
	{
		/* Turn cursor off if needed */
		if (op->cursor_on)
		{
			op->cursor_on = False;

			XfeCursorUndefine(w);
		}
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeOrientedSetAllowDrag(Widget w,Boolean allow_drag)
{
	assert( XfeIsOriented(w) );
	assert( _XfeIsAlive(w) );

	/* Make sure the oriented is alive */
	if (!_XfeIsAlive(w))
	{
		_XfeWarning(w,MESSAGE3);
		
		return False;
	}

	/* First remove any event handlers that might have been added */
	XtRemoveEventHandler(w,
						 CURSOR_EVENTS,
						 True,
						 CursorEH,
						 w);
	
	XtRemoveEventHandler(w,
						 DRAG_EVENTS,
						 True,
						 DragEH,
						 w);
	
	/* Add the event handlers if needed */
	if (allow_drag)
	{
		XtAddEventHandler(w,
						  CURSOR_EVENTS,
						  True,
						  CursorEH,
						  w);
		
		XtAddEventHandler(w,
						  DRAG_EVENTS,
						  True,
						  DragEH,
						  w);
	}

	return True;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeOrientedDescendantSetAllowDrag(Widget	w,
								  Widget	descendant,
								  Boolean	allow_drag)
{
	assert( XfeIsOriented(w) );
	assert( _XfeIsAlive(w) );
	assert( _XfeIsAlive(descendant) );

	/* Make sure the oriented is alive */
	if (!_XfeIsAlive(w))
	{
		_XfeWarning(w,MESSAGE3);
		
		return False;
	}

	/* Make sure the descendant is alive */
	if (!_XfeIsAlive(descendant))
	{
		_XfeArgWarning(w,MESSAGE4,descendant);
		
		return False;
	}

	/* First remove any event handlers that might have been added */
	if (XfeIsPrimitive(descendant))
	{
		XtRemoveCallback(descendant,
						 XmNenterCallback,
						 DescendantPrimitiveCursorCB,
						 w);

		XtRemoveCallback(descendant,
						 XmNleaveCallback,
						 DescendantPrimitiveCursorCB,
						 w);
	}
	else
	{
		XtRemoveEventHandler(descendant,
							 DESCENDANT_CURSOR_EVENTS,
							 True,
							 DescendantCursorEH,
							 w);
	}
	
	/* Add the event handlers if needed */
	if (allow_drag)
	{
		if (XfeIsPrimitive(descendant))
		{
			XtAddCallback(descendant,
						  XmNenterCallback,
						  DescendantPrimitiveCursorCB,
						  w);
			
			XtAddCallback(descendant,
						  XmNleaveCallback,
						  DescendantPrimitiveCursorCB,
						  w);
		}
		else
		{
			XtAddEventHandler(descendant,
							  DESCENDANT_CURSOR_EVENTS,
							  True,
							  DescendantCursorEH,
							  w);
		}

		XtAddEventHandler(descendant,
						  DESCENDANT_DRAG_EVENTS,
						  True,
						  DescendantDragEH,
						  w);
	}


	return True;
}
/*----------------------------------------------------------------------*/
static void
ApplyChildSetAllowDrag(Widget w,Widget child,XtPointer client_data)
{
	XfeOrientedDescendantSetAllowDrag(w,child,(Boolean) client_data);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeOrientedChildrenSetAllowDrag(Widget w,Boolean allow_drag)
{
	assert( XfeIsOriented(w) );
	assert( _XfeIsAlive(w) );

	/* Make sure the oriented is alive */
	if (!_XfeIsAlive(w))
	{
		_XfeWarning(w,MESSAGE3);
		
		return;
	}
	
	XfeManagerApply(w,ApplyChildSetAllowDrag,(XtPointer) allow_drag,False);
}
/*----------------------------------------------------------------------*/
