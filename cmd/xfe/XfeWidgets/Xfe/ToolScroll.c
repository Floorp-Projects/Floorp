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
/* Name:		<Xfe/ToolScroll.c>										*/
/* Description:	XfeToolScroll widget source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/ToolScrollP.h>
#include <Xfe/ToolBarP.h>

#include <Xm/DrawingA.h>
#include <Xfe/Arrow.h>
#include <Xfe/ToolBar.h>

#define MESSAGE1 "Widget is not an XfeToolScroll."
#define MESSAGE2 "XmNbackwardArrow is a read-only resource."
#define MESSAGE3 "XmNforwardArrow is a read-only resource."
#define MESSAGE4 "XmNtoolBar is a read-only resource."
#define MESSAGE5 "XmNclipArea is a read-only resource."

#define BACKWARD_ARROW_NAME		"BackwardArrow"
#define FORWARD_ARROW_NAME		"ForwardArrow"
#define TOOL_BAR_NAME			"ToolBar"
#define CLIP_AREA_NAME			"ClipArea"

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
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static void		DrawComponents		(Widget,XEvent *,Region,XRectangle *);
static void		LayoutComponents	(Widget);
static Boolean	AcceptChild			(Widget);
static Boolean	DeleteChild			(Widget);
static Boolean	InsertChild			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	EnterProc				(Widget,Widget,int,int);
static void 	LeaveProc				(Widget,Widget,int,int);
static void 	MotionProc				(Widget,Widget,int,int);
static void 	DragStart				(Widget,Widget,int,int);
static void 	DragEnd					(Widget,Widget,int,int);
static void 	DragMotion				(Widget,Widget,int,int);

static void 	DescendantMotion		(Widget,Widget,int,int);
static void 	DescendantDragStart		(Widget,Widget,int,int);
static void 	DescendantDragEnd		(Widget,Widget,int,int);
static void 	DescendantDragMotion	(Widget,Widget,int,int);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutVertical		(Widget);
static void		LayoutHorizontal	(Widget);
static void		LayoutToolBar		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolScroll Resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Arrow resources */
    { 
		XmNbackwardArrow,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . backward_arrow),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNforwardArrow,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . forward_arrow),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNarrowDisplayPolicy,
		XmCArrowDisplayPolicy,
		XmRArrowDisplayPolicy,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . arrow_display_policy),
		XmRImmediate, 
		(XtPointer) XmAS_NEEDED
    },
    { 
		XmNarrowPlacement,
		XmCArrowPlacement,
		XmRToolScrollArrowPlacement,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . arrow_placement),
		XmRImmediate, 
		(XtPointer) XmTOOL_SCROLL_ARROW_PLACEMENT_START
    },

	/* ClipArea resources */
    { 
		XmNclipArea,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . clip_area),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNclipShadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . clip_shadow_thickness),
		XmRCallProc, 
		(XtPointer) _XfeCallProcCopyShadowThickness
    },
    { 
		XmNclipShadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . clip_shadow_type),
		XmRImmediate, 
		(XtPointer) XmSHADOW_IN
    },

	/* Tool bar resources */
    { 
		XmNtoolBar,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . tool_bar),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNtoolBarPosition,
		XmCToolBarPosition,
		XmRPosition,
		sizeof(Position),
		XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . tool_bar_position),
		XmRImmediate, 
		(XtPointer) 0
    },

    /* Force XmNallowDrag to True */
	{ 
		XmNallowDrag,
		XmCBoolean,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolScrollRec , xfe_oriented . allow_drag),
		XmRImmediate, 
		(XtPointer) True
    },

	/* Force XmNspacing to 4 */
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolScrollRec , xfe_oriented . spacing),
		XmRImmediate, 
		(XtPointer) 4
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolScroll Synthetic Resources									*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
   { 
       XmNclipShadowThickness,
       sizeof(Dimension),
       XtOffsetOf(XfeToolScrollRec , xfe_tool_scroll . clip_shadow_thickness),
       _XmFromHorizontalPixels,
       _XmToHorizontalPixels 
   },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(toolscroll,ToolScroll) =
{
	{
		(WidgetClass) &xfeOrientedClassRec,		/* superclass       	*/
		"XfeToolScroll",						/* class_name       	*/
		sizeof(XfeToolScrollRec),				/* widget_size      	*/
		NULL,									/* class_initialize 	*/
		NULL,									/* class_part_initialize*/
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
		NULL,									/* resource list       	*/
		0,										/* num resources       	*/
		sizeof(XfeOrientedConstraintRec),		/* constraint size     	*/
		NULL,									/* init proc           	*/
		NULL,                                   /* destroy proc        	*/
		NULL,									/* set values proc     	*/
		NULL,                                   /* extension           	*/
	},

	/* XmManager Part */
	{
		XtInheritTranslations,					/* tm_table				*/
		syn_resources,							/* syn resources       	*/
		XtNumber(syn_resources),				/* num syn_resources   	*/
		NULL,                                   /* syn_cont_resources  	*/
		0,                                      /* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process      	*/
		NULL,                                   /* extension           	*/
	},

	/* XfeManager Part 	*/
	{
		XfeInheritBitGravity,					/* bit_gravity			*/
		PreferredGeometry,						/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		AcceptChild,							/* accept_child			*/
		InsertChild,							/* insert_child			*/
		DeleteChild,							/* delete_child			*/
		NULL,									/* change_managed		*/
		NULL,									/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		NULL,									/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		DrawComponents,							/* draw_components		*/
		False,									/* count_layable_children*/
		NULL,									/* child_is_layable		*/
		NULL,									/* extension          	*/
	},

	/* XfeOriented Part */
	{
		EnterProc,								/* enter				*/
		LeaveProc,								/* leave				*/
		MotionProc,								/* motion				*/
		DragStart,								/* drag_start			*/
		DragEnd,								/* drag_end				*/
		DragMotion,								/* drag_motion			*/
		XfeInheritDescendantEnter,				/* des_enter			*/
		XfeInheritDescendantLeave,				/* des_leave			*/
		DescendantMotion,						/* des_motion			*/
		DescendantDragStart,					/* des_drag_start		*/
		DescendantDragEnd,						/* des_drag_end			*/
		DescendantDragMotion,					/* des_drag_motion		*/
		NULL,									/* extension          	*/
	},

	/* XfeToolScroll Part */
	{
		NULL,									/* extension          	*/
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeToolScrollWidgetClassdeclaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(toolscroll,ToolScroll);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeToolScrollPart *		tp = _XfeToolScrollPart(nw);

    /* Make sure rep types are ok */
    XfeRepTypeCheck(nw,XmRArrowDisplayPolicy,&tp->arrow_display_policy,
					XmAS_NEEDED);

    XfeRepTypeCheck(nw,XmRToolScrollArrowPlacement,&tp->arrow_placement,
					XmTOOL_SCROLL_ARROW_PLACEMENT_START);

	tp->backward_arrow = 
		XtVaCreateManagedWidget(BACKWARD_ARROW_NAME,
								xfeArrowWidgetClass,
								nw,
								XmNbackground,		_XfeBackgroundPixel(nw),
								XmNforeground,		_XfemForeground(nw),
								XmNbackgroundPixmap,_XfeBackgroundPixmap(nw),
								NULL);

	tp->forward_arrow = 
		XtVaCreateManagedWidget(FORWARD_ARROW_NAME,
								xfeArrowWidgetClass,
								nw,
								XmNbackground,		_XfeBackgroundPixel(nw),
								XmNforeground,		_XfemForeground(nw),
								XmNbackgroundPixmap,_XfeBackgroundPixmap(nw),
								NULL);

	tp->clip_area = 
		XtVaCreateManagedWidget(CLIP_AREA_NAME,
								xmDrawingAreaWidgetClass,
								nw,
 								XmNbackground,		_XfeBackgroundPixel(nw),
								XmNforeground,		_XfemForeground(nw),
 								XmNbackgroundPixmap,_XfeBackgroundPixmap(nw),
								XmNmarginWidth,		0,
								XmNmarginHeight,	0,
 								XmNshadowThickness,	0,
 								XmNallowDrag,		True,
								NULL);

	tp->tool_bar = 
		XtVaCreateManagedWidget(TOOL_BAR_NAME,
								xfeToolBarWidgetClass,
								tp->clip_area,
								XmNbackground,		_XfeBackgroundPixel(nw),
								XmNforeground,		_XfemForeground(nw),
								XmNbackgroundPixmap,_XfeBackgroundPixmap(nw),
								XmNorientation,	   _XfeOrientedOrientation(nw),
 								XmNallowDrag,		True,
								NULL);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeToolScrollWidgetClass);
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
    XfeToolScrollPart *		np = _XfeToolScrollPart(nw);
    XfeToolScrollPart *		op = _XfeToolScrollPart(ow);

    /* backward_arrow */
    if (np->backward_arrow != op->backward_arrow)
    {
		_XfeWarning(nw,MESSAGE2);

		np->backward_arrow = op->backward_arrow;
    }

    /* forward_arrow */
    if (np->forward_arrow != op->forward_arrow)
    {
		_XfeWarning(nw,MESSAGE3);

		np->forward_arrow = op->forward_arrow;
    }

    /* tool_bar */
    if (np->tool_bar != op->tool_bar)
    {
		_XfeWarning(nw,MESSAGE4);

		np->tool_bar = op->tool_bar;
    }

    /* clip_area */
    if (np->clip_area != op->clip_area)
    {
		_XfeWarning(nw,MESSAGE5);

		np->clip_area = op->clip_area;
    }

	/* orientation */
    if (_XfeOrientedOrientation(nw) != _XfeOrientedOrientation(ow))
    {
		if (_XfeIsAlive(np->tool_bar))
		{
			XtVaSetValues(np->tool_bar,
						  XmNorientation,	_XfeOrientedOrientation(nw),
						  NULL);
		}
    }

	/* arrow_display_policy */
    if (np->arrow_display_policy != op->arrow_display_policy)
    {
		/* Make sure the new arrow display policy is ok */
		XfeRepTypeCheck(nw,XmRArrowDisplayPolicy,&np->arrow_display_policy,
						XmAS_NEEDED);

		_XfemConfigFlags(nw) |= XfeConfigGLE;
    }

	/* arrow_placement */
    if (np->arrow_placement != op->arrow_placement)
    {
		/* Make sure the new arrow display policy is ok */
		XfeRepTypeCheck(nw,XmRToolScrollArrowPlacement,&np->arrow_placement,
						XmTOOL_SCROLL_ARROW_PLACEMENT_START);

		_XfemConfigFlags(nw) |= XfeConfigGLE;
    }

    return _XfeManagerChainSetValues(ow,rw,nw,xfeToolScrollWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension * width,Dimension * height)
{
	XfeToolScrollPart *	tp = _XfeToolScrollPart(w);

	*width  =
		_XfemOffsetLeft(w) + 
		_XfemOffsetRight(w) + 
		2 * tp->clip_shadow_thickness +
		2 * _XfeOrientedSpacing(w);

	*height = 
		_XfemOffsetTop(w) + 
		_XfemOffsetBottom(w) +
		2 * tp->clip_shadow_thickness + 
		2 * _XfeOrientedSpacing(w);

	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		*width += _XfemPreferredWidth(tp->tool_bar);

		*height += 
			(_XfemPreferredHeight(tp->tool_bar) +
			 _XfeHeight(tp->backward_arrow));
	}
	else
	{
		*height += _XfemPreferredHeight(tp->tool_bar);

		*width += 
			(_XfemPreferredWidth(tp->tool_bar) +
			 _XfeWidth(tp->backward_arrow));
	}
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
	XfeToolScrollPart *	tp = _XfeToolScrollPart(w);

	if (tp->clip_shadow_thickness > 0)
	{
		XfeDrawShadowsAroundWidget(w,
								   tp->clip_area,
								   _XfemTopShadowGC(w),
								   _XfemBottomShadowGC(w),
								   _XfeOrientedSpacing(w),
								   tp->clip_shadow_thickness,
								   tp->clip_shadow_type);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		LayoutVertical(w);
	}
	/* Horizontal */
	else
	{
		LayoutHorizontal(w);
	}

	/* Layout the toolbar */
	LayoutToolBar(w);
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptChild(Widget child)
{
	return False;
}
/*----------------------------------------------------------------------*/
static Boolean
InsertChild(Widget child)
{
	Widget					w = XtParent(child);
/* 	XfeToolScrollPart *		tp = _XfeToolScrollPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteChild(Widget child)
{
	Widget					w = XtParent(child);
/* 	XfeToolScrollPart *		tp = _XfeToolScrollPart(w); */

	return True;
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
MotionProc(Widget w,Widget descendant,int x,int y)
{
}
/*----------------------------------------------------------------------*/
static void
DragStart(Widget w,Widget descendant,int x,int y)
{
	printf("DragStart(%s,%s,%d,%d)\n",
		   XtName(w),descendant ? XtName(descendant) : "NULL",x,y);

#if 0
	/* Make sure the coords are inside the sash */
	if (!SashContainsXY(w,x,y))
	{
		return;
	}

	SashDragStart(w,x,y,NULL);
#endif
}
/*----------------------------------------------------------------------*/
static void
DragEnd(Widget w,Widget descendant,int x,int y)
{
	printf("DragEnd(%s,%s,%d,%d) start = (%d,%d)\n",
		   XtName(w),
		   descendant ? XtName(descendant) : "NULL",
		   x,y,
		   _XfeOrientedDragStartX(w),
		   _XfeOrientedDragStartY(w));

#if 0
	SashDragEnd(w,x,y,NULL);
#endif
}
/*----------------------------------------------------------------------*/
static void
DragMotion(Widget w,Widget descendant,int x,int y)
{
	printf("DragMotion(%s,%s,%d,%d)\n",
		   XtName(w),descendant ? XtName(descendant) : "NULL",x,y);

#if 0
	SashDragMotion(w,x,y,NULL);
#endif
}
/*----------------------------------------------------------------------*/
static void
DescendantMotion(Widget w,Widget descendant,int x,int y)
{
#if 0
	printf("DescendantMotion(%s,%s,%d,%d)\n",
		   XtName(w),descendant ? XtName(descendant) : "NULL",x,y);
#endif
}
/*----------------------------------------------------------------------*/
static void
DescendantDragStart(Widget w,Widget descendant,int x,int y)
{
#if 0
	printf("DescendantDragStart(%s,%s,%d,%d)\n",
		   XtName(w),descendant ? XtName(descendant) : "NULL",x,y);
#endif

	printf("diff = %d\n",_XfeOrientedDragStartY(w) - y);

#if 0
	SashDragStart(w,x,y,DescendantFindAttachment(w,descendant));
#endif
}
/*----------------------------------------------------------------------*/
static void
DescendantDragEnd(Widget w,Widget descendant,int x,int y)
{
	printf("DescendantDragEnd(%s,%s,%d,%d) start = (%d,%d)\n",
		   XtName(w),
		   descendant ? XtName(descendant) : "NULL",
		   x,y,
		   _XfeOrientedDragStartX(w),
		   _XfeOrientedDragStartY(w));

#if 0
	SashDragEnd(w,x,y,DescendantFindAttachment(w,descendant));
#endif
}
/*----------------------------------------------------------------------*/
static void
DescendantDragMotion(Widget w,Widget descendant,int x,int y)
{
	XfeToolScrollPart *	tp = _XfeToolScrollPart(w);
	Position			new_pos;

#if 0
	printf("DescendantDragMotion(%s,%s,%d,%d)\n",
		   XtName(w),descendant ? XtName(descendant) : "NULL",x,y);
#endif

	new_pos = tp->tool_bar_position + (y - _XfeOrientedDragStartY(w)) * 1;

	_XfeMoveWidget(tp->tool_bar,0,new_pos);

#if 0
	SashDragMotion(w,x,y,DescendantFindAttachment(w,descendant));
#endif
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutVertical(Widget w)
{
	XfeToolScrollPart *	tp = _XfeToolScrollPart(w);
	int					backward_width = _XfemRectWidth(w) / 2;
	int					forward_width = _XfemRectWidth(w) - backward_width;


	/* The backward arrow */
	if (backward_width > 0)
	{
		XtVaSetValues(tp->backward_arrow,XmNarrowDirection,XmARROW_DOWN,NULL);
		
		_XfeConfigureWidget(tp->backward_arrow,
							_XfemRectX(w),
							_XfemRectY(w),
							backward_width,
							_XfeHeight(tp->backward_arrow));
	}
	
	/* The forward arrow */
	if (forward_width > 0)
	{
		XtVaSetValues(tp->backward_arrow,XmNarrowDirection,XmARROW_UP,NULL);
		
		_XfeConfigureWidget(tp->forward_arrow,
							
							_XfeX(tp->backward_arrow) + 
							_XfeWidth(tp->backward_arrow),
							
							_XfemRectY(w),
							
							forward_width,
							
							_XfeHeight(tp->forward_arrow));
	}
	
	if (backward_width || forward_width)
	{
		_XfeConfigureWidget(tp->clip_area,
							
							_XfemRectX(w) + 
							tp->clip_shadow_thickness +
							_XfeOrientedSpacing(w),
							
							_XfeY(tp->backward_arrow) + 
							_XfeHeight(tp->backward_arrow) +
							tp->clip_shadow_thickness +
							_XfeOrientedSpacing(w),
							
							_XfemRectWidth(w) - 
							2 * tp->clip_shadow_thickness -
							2 * _XfeOrientedSpacing(w),
								
							_XfemRectHeight(w) - 
							_XfeHeight(tp->backward_arrow) -
							2 * tp->clip_shadow_thickness -
							2 * _XfeOrientedSpacing(w));
		
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutHorizontal(Widget w)
{
	XfeToolScrollPart *	tp = _XfeToolScrollPart(w);
	int					backward_height = _XfemRectHeight(w) / 2;
	int					forward_height = _XfemRectHeight(w) - backward_height;
	
	/* The backward arrow */
	if (backward_height > 1)
	{
		XtVaSetValues(tp->backward_arrow,XmNarrowDirection,XmARROW_LEFT,NULL);
		
		_XfeConfigureWidget(tp->backward_arrow,
							_XfemRectX(w),
							_XfemRectY(w),
							_XfeWidth(tp->backward_arrow),
							backward_height);
	}
	
	/* The forward arrow */
	if (forward_height > 1)
	{
		XtVaSetValues(tp->forward_arrow,XmNarrowDirection,XmARROW_RIGHT,NULL);
		
		_XfeConfigureWidget(tp->forward_arrow,
							
							_XfemRectX(w),
							
							_XfeY(tp->backward_arrow) + 
							_XfeHeight(tp->backward_arrow),
							
							_XfeWidth(tp->forward_arrow),
							
							forward_height);
	}
	
	if (backward_height || forward_height)
	{
		_XfeConfigureWidget(tp->clip_area,
							
							_XfeX(tp->backward_arrow) + 
							_XfeWidth(tp->backward_arrow) +
							tp->clip_shadow_thickness +
							_XfeOrientedSpacing(w),
							
							_XfemRectY(w) + 
							tp->clip_shadow_thickness +
							_XfeOrientedSpacing(w),
							
							_XfemRectWidth(w) - 
							_XfeWidth(tp->backward_arrow) -
							2 * tp->clip_shadow_thickness -
							2 * _XfeOrientedSpacing(w),
							
							_XfemRectHeight(w) -
							2 * tp->clip_shadow_thickness -
							2 * _XfeOrientedSpacing(w));
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutToolBar(Widget w)
{
	XfeToolScrollPart *	tp = _XfeToolScrollPart(w);

	if (!_XfeIsAlive(tp->tool_bar))
	{
		return;
	}

/* 	tp->tool_bar_position = 20; */

	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		_XfeMoveWidget(tp->tool_bar,0,tp->tool_bar_position);
	}
	else
	{
		_XfeMoveWidget(tp->tool_bar,tp->tool_bar_position,0);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolScroll Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern*/ Widget
XfeCreateToolScroll(Widget pw,char * name,Arg * av,Cardinal ac)
{
   return XtCreateWidget(name,xfeToolScrollWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
