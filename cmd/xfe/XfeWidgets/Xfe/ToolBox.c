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
/* Name:		<Xfe/ToolBox.c>											*/
/* Description:	XfeToolBox widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/ToolBoxP.h>
#include <Xfe/Button.h>
#include <Xfe/Tab.h>
#include <Xm/Form.h>

#define MESSAGE1 "Widget is not an XfeToolBox."
#define MESSAGE2 "XmNitemCount is a read-only resource."
#define MESSAGE3 "XfeToolBox can only have XfeToolItem childred."
#define MESSAGE4 "XmNswapThreshold is to large."
#define MESSAGE5 "XmNpositionIndex out of range for %s"
#define MESSAGE6 "XmNclosedTabs is a read-only resource."
#define MESSAGE7 "XmNopenedTabs is a read-only resource."

#define OPENED_TAB_BUTTON_NAME			"Tab"
#define CLOSED_TAB_BUTTON_NAME			"Tab"

#define DRAG_EVENTS (ButtonPressMask | ButtonReleaseMask | Button1MotionMask)

#define DIRECTION_NONE		0
#define DIRECTION_DOWN		1
#define DIRECTION_UP		-1
#define FAR_AWAY		-1000

#define CHECK_RANGE(tp,i) \
( ( (i) >= 0 ) && ( i < (tp) -> item_count ) )

#define IS_OUR_TAB(w,child) \
(XfeIsTab(child) && (_XfeParent(child) == w))

#define _XfeSwap(_x,_y,_t)						\
{												\
	_t __tmp__ = _x;							\
												\
	_x = _y;									\
	_y = __tmp__;								\
}

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
/* Constraint Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			ConstraintInitialize	(Widget,Widget,ArgList,Cardinal *);
static Boolean		ConstraintSetValues		(Widget,Widget,Widget,ArgList,
											 Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static void		MinimumGeometry	(Widget,Dimension *,Dimension *);
static void		LayoutComponents	(Widget);
static void		LayoutChildren		(Widget);
static Boolean	AcceptChild			(Widget);
static Boolean	DeleteChild			(Widget);
static Boolean	InsertChild			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeToolBox functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget		CreateTab				(Widget,Boolean);
static Cardinal		ActiveItemCount			(Widget);
static Dimension	MaxItemWidth			(Widget);
static int			ItemToIndex				(Widget);
static int			TabToIndex				(Widget);
static void			CheckSwapThreshold		(Widget);


static void			DragStart				(Widget,XEvent *,int,int);
static void			DragEnd					(Widget,XEvent *,int,int);
static void			DragMotion				(Widget,XEvent *,int,int);

static Boolean		MoveItem				(Widget,Cardinal,int);
static int			MaxItemY				(Widget);
static void			SwapItems				(Widget,int,int);


static void			RaiseItem				(Widget,int);


static int			FindItemUnder			(Widget,int);
static Boolean		ItemOverItem			(Widget,int,int,Dimension);
static void			SnapItemInPlace			(Widget,int);


static int			FirstManagedIndex		(Widget);
static int			LastManagedIndex		(Widget);
static int			NextManagedIndex		(Widget,int);
static int			PreviousManagedIndex	(Widget,int);

static void			InvokeSwapCallbacks		(Widget,XEvent *,Widget,
											 Widget,int,int);
static void			InvokeCallbacks			(Widget,XtCallbackList,int,
											 XEvent *,Widget,Widget,Widget,int);
static void			UpdatePositionIndeces	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Child functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			ChildSetDraggable		(Widget,Widget,Boolean);

/*----------------------------------------------------------------------*/
/*																		*/
/* Item functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			UpdateItems				(Widget);
static void			UpdatePixmaps			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* TaskBar callbacks													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	TabActivateCB				(Widget,XtPointer,XtPointer);
static void	TabArmCB					(Widget,XtPointer,XtPointer);
static void	TabDisarmCB					(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Descendant event handlers											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	DescendantEH		(Widget,XtPointer,XEvent *,Boolean *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox Resources											        */
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Callback resources */     	
    { 
		XmNcloseCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . close_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNopenCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . open_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNdragAllowCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . drag_allow_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNdragStartCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . drag_start_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNdragEndCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . drag_end_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNdragMotionCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . drag_motion_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNnewItemCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . new_item_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNswapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . swap_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNsnapCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . snap_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },


    /* Item resources */
    { 
		XmNitems,
		XmCReadOnly,
		XmRWidgetList,
		sizeof(WidgetList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . items),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNitemCount,
		XmCReadOnly,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . item_count),
		XmRImmediate, 
		(XtPointer) 0
    },

    /* Tab resources */
    { 
		XmNclosedTabs,
		XmCReadOnly,
		XmRWidgetList,
		sizeof(WidgetList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . closed_tabs),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNopenedTabs,
		XmCReadOnly,
		XmRWidgetList,
		sizeof(WidgetList),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . opened_tabs),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNtabOffset,
		XmCOffset,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . tab_offset),
		XmRImmediate, 
		(XtPointer) 10
    },

    /* Spacing resources */
    { 
		XmNverticalSpacing,
		XmCSpacing,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . vertical_spacing),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNhorizontalSpacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . horizontal_spacing),
		XmRImmediate, 
		(XtPointer) 0
    },


    /* Drag resources */
	{ 
		XmNdragCursor,
		XmCDragCursor,
		XmRCursor,
		sizeof(Widget),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . drag_cursor),
		XmRString, 
		XfeDEFAULT_TOOL_BOX_DRAG_CURSOR
    },
	{ 
		XmNdragButton,
		XmCDragButton,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . drag_button),
		XmRImmediate, 
		(XtPointer) Button1
    },
	{ 
		XmNdragThreshold,
		XmCDragThreshold,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . drag_threshold),
		XmRImmediate, 
		(XtPointer) 10
    },

    /* Swap resources */
    { 
		XmNswapThreshold,
		XmCSwapThreshold,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . swap_threshold),
		XmRImmediate, 
		(XtPointer) 10
    },

	/* Pixmap resources */
    { 
		XmNbottomPixmap,
		XmCBottomPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . bottom_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNhorizontalPixmap,
		XmCHorizontalPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . horizontal_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNleftPixmap,
		XmCLeftPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . left_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNrightPixmap,
		XmCRightPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . right_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNtopPixmap,
		XmCTopPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . top_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNverticalPixmap,
		XmCVerticalPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . vertical_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNbottomRaisedPixmap,
		XmCBottomRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . bottom_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNhorizontalRaisedPixmap,
		XmCHorizontalRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . horizontal_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNleftRaisedPixmap,
		XmCLeftRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . left_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNrightRaisedPixmap,
		XmCRightRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . right_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNtopRaisedPixmap,
		XmCTopRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . top_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },
    { 
		XmNverticalRaisedPixmap,
		XmCVerticalRaisedPixmap,
		XmRPixmap,
		sizeof(Pixmap),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . vertical_raised_pixmap),
		XmRImmediate, 
		(XtPointer) XmUNSPECIFIED_PIXMAP
    },

	/* Force XmNusePreferredHeight to True and XmNusePreferredWidth to False */
	{
		XmNusePreferredHeight,
		XmCUsePreferredHeight,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBoxRec , xfe_manager . use_preferred_height),
		XmRImmediate, 
		(XtPointer) True
	},
	{
		XmNusePreferredWidth,
		XmCUsePreferredWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBoxRec , xfe_manager . use_preferred_width),
		XmRImmediate, 
		(XtPointer) False
	},
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBoxBar Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
	{ 
		XmNverticalSpacing,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . vertical_spacing),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
	},
	{ 
		XmNhorizontalSpacing,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . horizontal_spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
	{ 
		XmNswapThreshold,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . swap_threshold),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
	},
	{ 
		XmNdragThreshold,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . drag_threshold),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
	},
	{ 
 		XmNtabOffset,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBoxRec , xfe_tool_box . tab_offset),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox constraint resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource constraint_resources[] = 
{
    { 
		XmNopen,
		XmCOpen,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBoxConstraintRec , xfe_tool_box . open),
		XmRImmediate,
		(XtPointer) True
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(toolbox,ToolBox) =
{
	{
		(WidgetClass) &xfeManagerClassRec,		/* superclass       	*/
		"XfeToolBox",							/* class_name       	*/
		sizeof(XfeToolBoxRec),					/* widget_size      	*/
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
		NULL,									/* get_values_hook		*/
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
		_XfeLiberalGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
	},

	/* Constraint Part */
	{
		constraint_resources,					/* constraint res		*/
		XtNumber(constraint_resources),			/* num constraint res	*/
		sizeof(XfeToolBoxConstraintRec),		/* constraint size     	*/
		ConstraintInitialize,					/* init proc			*/
		NULL,                                   /* destroy proc        	*/
		ConstraintSetValues,					/* set values proc		*/
		NULL,                                   /* extension           	*/
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
		PreferredGeometry,						/* preferred_geometry	*/
		MinimumGeometry,						/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		AcceptChild,							/* accept_child			*/
		InsertChild,							/* insert_child			*/
		DeleteChild,							/* delete_child			*/
		NULL,									/* change_managed		*/
		NULL,									/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		LayoutChildren,							/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		NULL,									/* draw_components		*/
		False,									/* count_layable_children*/
		NULL,									/* child_is_layable		*/
		NULL,									/* extension          	*/
	},

	/* XfeToolBox Part */
	{
		NULL,									/* extension          	*/
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeToolBoxWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(toolbox,ToolBox);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeToolBoxPart *	tp = _XfeToolBoxPart(nw);

    /* Make sure read only resoures are not set */
    if (tp->item_count > 0)
    {
		tp->item_count = 0;

		_XfeWarning(nw,MESSAGE2);
    }

    if (tp->closed_tabs != NULL)
    {
		tp->closed_tabs = NULL;

		_XfeWarning(nw,MESSAGE7);
    }

    if (tp->opened_tabs != NULL)
    {
		tp->opened_tabs = NULL;

		_XfeWarning(nw,MESSAGE7);
    }


	tp->last_moved_item		= NULL;

	tp->dragging_tab		= False;
	tp->clicking_tab		= False;

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeToolBoxWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeToolBoxPart * tp = _XfeToolBoxPart(w);

	/* Free the tabs if needed */
	if (tp->closed_tabs)
	{
		XtFree((char *) tp->closed_tabs);
	}

	if (tp->opened_tabs)
	{
		XtFree((char *) tp->opened_tabs);
	}

	/* Free the items if needed */
	if (tp->items)
	{
		XtFree((char *) tp->items);
	}
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeToolBoxPart *		np = _XfeToolBoxPart(nw);
    XfeToolBoxPart *		op = _XfeToolBoxPart(ow);
	Boolean					update_pixmaps = False;

    /* item_count */
    if (np->item_count != op->item_count)
    {
		np->item_count = op->item_count;

		_XfeWarning(nw,MESSAGE2);
    }

    /* closed_tabs */
    if (np->closed_tabs != op->closed_tabs)
    {
		np->closed_tabs = op->closed_tabs;

		_XfeWarning(nw,MESSAGE6);
    }

    /* opened_tabs */
    if (np->opened_tabs != op->opened_tabs)
    {
		np->opened_tabs = op->opened_tabs;

		_XfeWarning(nw,MESSAGE7);
    }

	/* tab_offset */
	if (np->tab_offset != op->tab_offset)
	{
		_XfemConfigFlags(nw) |= XfeConfigGLE;
	}

	/* tab_offset */
	if (np->tab_offset != op->tab_offset)
	{
		_XfemConfigFlags(nw) |= XfeConfigGLE;
	}

	/* left_pixmap */
	if ((np->left_pixmap				!= op->left_pixmap) || 
		(np->right_pixmap				!= op->right_pixmap) || 
		(np->top_pixmap					!= op->top_pixmap) || 
		(np->bottom_pixmap				!= op->bottom_pixmap) || 
		(np->vertical_pixmap			!= op->vertical_pixmap) || 
		(np->horizontal_pixmap			!= op->horizontal_pixmap) ||
		(np->left_raised_pixmap			!= op->left_raised_pixmap) || 
		(np->right_raised_pixmap		!= op->right_raised_pixmap) || 
		(np->top_raised_pixmap			!= op->top_raised_pixmap) || 
		(np->bottom_raised_pixmap		!= op->bottom_raised_pixmap) || 
		(np->vertical_raised_pixmap		!= op->vertical_raised_pixmap) || 
		(np->horizontal_raised_pixmap	!= op->horizontal_raised_pixmap))
	{
		update_pixmaps = True;
	}

	/* drag_cursor */
	if (np->drag_cursor != op->drag_cursor)
	{
		/* Update drag cursor */
	}

	/* swap_threshold */
	if (np->swap_threshold != op->swap_threshold)
	{
		/* Make sure the new swap threshold is ok */
		CheckSwapThreshold(nw);
	}

	/* Update pixmaps if needed */
	if (update_pixmaps)
	{
		UpdatePixmaps(nw);
	}

    return _XfeManagerChainSetValues(ow,rw,nw,xfeToolBoxWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ConstraintInitialize(Widget rc,Widget nc,ArgList args,Cardinal *nargs)
{
/* 	Widget						w = XtParent(nc); */

/* 	printf("ConstraintInitialize(%s) for %s\n",XtName(w),XtName(nc)); */

/* 	Widget						w = XtParent(nc); */
/*	XfeManagerConstraintPart *	cp = _XfeManagerConstraintPart(nc);*/


	/* Finish constraint initialization */
	_XfeConstraintChainInitialize(rc,nc,xfeToolBoxWidgetClass);
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValues(Widget oc,Widget rc,Widget nc,ArgList args,Cardinal *nargs)
{
	Widget						w = XtParent(nc);
    XfeToolBoxPart *			tp = _XfeToolBoxPart(w);
 	XfeToolBoxConstraintPart *	ncp = _XfeToolBoxConstraintPart(nc);
 	XfeToolBoxConstraintPart *	ocp = _XfeToolBoxConstraintPart(oc);

	/* position_index */
	if (_XfeManagerPositionIndex(nc) != _XfeManagerPositionIndex(oc))
	{
		/* Make sure the new position index is within range */
		if ((_XfeManagerPositionIndex(nc) < 0) || 
			(_XfeManagerPositionIndex(nc) >= tp->item_count))
		{
			_XfeManagerPositionIndex(nc) = _XfeManagerPositionIndex(oc);

			_XfeArgWarning(w,MESSAGE5,XtName(nc));
		}
		else
		{
			/* Swap the items */
			SwapItems(w,
					  _XfeManagerPositionIndex(nc),
					  _XfeManagerPositionIndex(oc));


/* 			UpdatePositionIndeces(w); */
			
 			_XfemConfigFlags(w) |= XfeConfigLayout;
		}
	}

	/* open */
	if (ncp->open != ocp->open)
	{
/*  		printf("%s,%s: change in open\n",XtName(nc),XtName(w)); */

		_XfemConfigFlags(w) |= XfeConfigGLE;
	}

	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(oc,rc,nc,xfeToolBoxWidgetClass);
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
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Dimension				total_tab_width = 0;
	Dimension				total_tab_height = 0;
	Cardinal				i;

	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w) + MaxItemWidth(w);
	*height = _XfemOffsetTop(w)  + _XfemOffsetBottom(w);

	for (i = 0; i < tp->item_count; i++)
	{
		Widget item = tp->items[i];

		/* Shown */
		if (_XfeChildIsShown(item))
		{
			/* Open */
			if (_XfeToolBoxChildOpen(item))
			{
				int dy = _XfeHeight(item) + tp->vertical_spacing;

				*height += dy;
			}
			/* Closed */
			else
			{
				total_tab_width += (_XfeHeight(item) + tp->horizontal_spacing);
				total_tab_height = _XfeHeight(tp->closed_tabs[i]);
			}
		}
	}

	*height += total_tab_height;

	/* Make sure the tool box is at least wide enough to hold the tabs */
	if (*width < total_tab_width)
	{
		*width = total_tab_width;
	}
}
/*----------------------------------------------------------------------*/
static void
MinimumGeometry(Widget w,Dimension * width,Dimension * height)
{
/* 	XfeToolBoxPart *		tp = _XfeToolBoxPart(w); */

	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
    XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	Cardinal			i;
	int					x = _XfemRectX(w);
	int					y = _XfemRectY(w);

/* 	if ((_XfeWidth(w) <= 10) || (_XfeHeight(w) <= 10)) */
/* 	{ */
/* 		return; */
/* 	} */

/* 	printf("LayoutComponents(%s)\n",XtName(w)); */

	for (i = 0; i < tp->item_count; i++)
	{
		/* Item is managed */
		if (_XfeChildIsShown(tp->items[i]))
		{
			/* Open */
			if (_XfeToolBoxChildOpen(tp->items[i]))
			{
				_XfeConfigureWidget(tp->opened_tabs[i],
									
									_XfemRectX(w),
									
									y,

									_XfeWidth(tp->opened_tabs[i]),
									
									_XfeHeight(tp->items[i]));
				
				y += (_XfeHeight(tp->items[i]) + tp->vertical_spacing);

				_XfeMoveWidget(tp->closed_tabs[i],FAR_AWAY,FAR_AWAY);
			}
			/* Closed */
			else
			{
				Dimension width = _XfeHeight(tp->items[i]);
				
				_XfeConfigureWidget(tp->closed_tabs[i],
									
									x,
									
									_XfeHeight(w) - 
									_XfemOffsetBottom(w) -
									_XfeHeight(tp->closed_tabs[i]),
									
									width,
								
									_XfeHeight(tp->closed_tabs[i]));
				
				x += width;

				_XfeMoveWidget(tp->opened_tabs[i],FAR_AWAY,FAR_AWAY);
			}
		}
		/* Item is unmanaged */
		else
		{
			_XfeMoveWidget(tp->closed_tabs[i],FAR_AWAY,FAR_AWAY);
			_XfeMoveWidget(tp->opened_tabs[i],FAR_AWAY,FAR_AWAY);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutChildren(Widget w)
{
    XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;
	int						y = _XfemRectY(w);

	for (i = 0; i < tp->item_count; i++)
	{
		Widget item = tp->items[i];

		if (_XfeIsAlive(item))
		{
			/* Item is managed */
			if (_XfeChildIsShown(item))
			{
				/* Open */
				if (_XfeToolBoxChildOpen(item))
				{
					int width;
					int height;

					width = _XfemRectWidth(w) - _XfeWidth(tp->opened_tabs[i]);
					height = _XfeHeight(item);

					_XfeConfigureWidget(item,
										
										_XfemRectX(w) + 
										_XfeWidth(tp->opened_tabs[i]),
										
										y,
										
										width,
										
										height);
						   
					y += (_XfeHeight(item) + tp->vertical_spacing);
				}
				/* Closed */
				else
				{
					_XfeMoveWidget(item,FAR_AWAY,FAR_AWAY);
				}
			}
		}
	}
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptChild(Widget child)
{
	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
InsertChild(Widget child)
{
	Widget					w = XtParent(child);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Widget					opened_tab;
	Widget					closed_tab;
	Cardinal				index;

/*  	printf("InsertChild(%s) for %s\n",XtName(w),XtName(child)); */

	/* Increase the item count */
	tp->item_count++;

	/* The new tab's index */
	index = tp->item_count - 1;

	/* For every tool item, we need 2 tab buttoms: (1 opened & 1 closed) */
	closed_tab = CreateTab(w,False);
	opened_tab = CreateTab(w,True);

	/* Add callbacks to tabs */
	XtAddCallback(closed_tab,XmNactivateCallback,TabActivateCB,NULL);
	XtAddCallback(closed_tab,XmNarmCallback,TabArmCB,NULL);
	XtAddCallback(closed_tab,XmNdisarmCallback,TabDisarmCB,NULL);

	XtAddCallback(opened_tab,XmNactivateCallback,TabActivateCB,NULL);
	XtAddCallback(opened_tab,XmNarmCallback,TabArmCB,NULL);
	XtAddCallback(opened_tab,XmNdisarmCallback,TabDisarmCB,NULL);

	/* Allocate more space for the new tabs */
	tp->closed_tabs = (WidgetList) XtRealloc((char *) tp->closed_tabs,
											 sizeof(Widget) * tp->item_count);
	
	tp->opened_tabs = (WidgetList) XtRealloc((char *) tp->opened_tabs,
											 sizeof(Widget) * tp->item_count);
	
	/* Place the new tabs in the tab lists */
	tp->closed_tabs[index] = closed_tab;
	tp->opened_tabs[index] = opened_tab;
	
	/* Allocate more space for the new item */
	tp->items = (WidgetList) XtRealloc((char *) tp->items,
									   sizeof(Widget) * tp->item_count);

	/* Place the new tab in the tab tab list */
	tp->items[index] = child;

	/* Add item event handlers */
	ChildSetDraggable(w,child,True);

	/* Make sure the swap threshold is ok */
	CheckSwapThreshold(w);

	/* Update the position_index contraint member of the items */
	UpdatePositionIndeces(w);

	/* Invoke new item callbacks */
	InvokeCallbacks(w,
					tp->new_item_callback,
					XmCR_TOOL_BOX_NEW_ITEM,
					NULL,
					child,
					closed_tab,
					opened_tab,
					index);

	/* Do layout only of the new child is managed */
	return XtIsManaged(child);
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteChild(Widget child)
{
	Widget					w = XtParent(child);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	int						index = ItemToIndex(child);

	assert( CHECK_RANGE(tp,index) );

	/* Remove the corresponding tabs */
	XtDestroyWidget(tp->closed_tabs[index]);
	XtDestroyWidget(tp->opened_tabs[index]);

	/* Mark the item and tab NULL */
	tp->items[index]		= NULL;
	tp->closed_tabs[index]  = NULL;
	tp->opened_tabs[index]  = NULL;

	/* Update the item and tab lists */
	UpdateItems(w);

	/* Decrease the item count */
	tp->item_count++;

	return XtIsManaged(child);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeToolBox functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget
CreateTab(Widget w,Boolean opened)
{
    XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	Widget				tab;
	char				name_buf[20];
	Arg					av[10];
	Cardinal			ac = 0;

	XtSetArg(av[ac],XmNprivateComponent,		True); ac++;

	/* Opened - vertical */
	if (opened)
	{
		sprintf(name_buf,"%s%d",OPENED_TAB_BUTTON_NAME,tp->item_count - 1);

		XtSetArg(av[ac],XmNorientation,				XmVERTICAL); ac++;
		
		XtSetArg(av[ac],XmNtopPixmap,				tp->top_pixmap); ac++;
		XtSetArg(av[ac],XmNtopRaisedPixmap,			tp->top_raised_pixmap); ac++;

		XtSetArg(av[ac],XmNbottomPixmap,			tp->bottom_pixmap); ac++;
		XtSetArg(av[ac],XmNbottomRaisedPixmap,		tp->bottom_raised_pixmap); ac++;

		XtSetArg(av[ac],XmNverticalPixmap,			tp->vertical_pixmap); ac++;
		XtSetArg(av[ac],XmNverticalRaisedPixmap,	tp->vertical_raised_pixmap); ac++;
	}
	/* Closed - horizontal */
	else
	{
		sprintf(name_buf,"%s%d",CLOSED_TAB_BUTTON_NAME,tp->item_count - 1);

		XtSetArg(av[ac],XmNorientation,				XmHORIZONTAL); ac++;

		XtSetArg(av[ac],XmNleftPixmap,				tp->left_pixmap); ac++;
		XtSetArg(av[ac],XmNleftRaisedPixmap,		tp->left_raised_pixmap); ac++;

		XtSetArg(av[ac],XmNrightPixmap,				tp->right_pixmap); ac++;
		XtSetArg(av[ac],XmNrightRaisedPixmap,		tp->right_raised_pixmap); ac++;

		XtSetArg(av[ac],XmNhorizontalPixmap,		tp->horizontal_pixmap); ac++;
		XtSetArg(av[ac],XmNhorizontalRaisedPixmap,	tp->horizontal_raised_pixmap); ac++;
	}

	tab = XtCreateWidget(name_buf,xfeTabWidgetClass,w,av,ac);

	/* Allow the only the opened tab to drag the item */
	if (opened)
	{
		ChildSetDraggable(w,tab,True);
	}

	XtManageChild(tab);

	return tab;
}
/*----------------------------------------------------------------------*/
static Cardinal
ActiveItemCount(Widget w)
{
    XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				managed_item_count = 0;
	Cardinal				i;

	for (i = 0; i < tp->item_count; i++)
	{
		Widget item = tp->items[i];

		/* Shown and open */
		if (_XfeChildIsShown(item) && _XfeToolBoxChildOpen(item))
		{
			managed_item_count++;
		}
	}

	return managed_item_count;
}
/*----------------------------------------------------------------------*/
static Dimension
MaxItemWidth(Widget w)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Dimension				max_item_width = 0;
	Cardinal				i;
	
	for (i = 0; i < tp->item_count; i++)
	{
		/* Shown */
		if (_XfeChildIsShown(tp->items[i]))
		{
			if (_XfeWidth(tp->items[i]) > max_item_width)
			{
				max_item_width = _XfeWidth(tp->items[i]);
			}
		}
	}

	return max_item_width;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Tab callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
TabActivateCB(Widget button,XtPointer client_data,XtPointer call_data)
{
    Widget					w = XtParent(button);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	int						index = TabToIndex(button);

	assert( CHECK_RANGE(tp,index) );

/* 	printf("TabActivateCB(%s = %d)\n",XtName(tp->items[index]),index); */

	/* Make sure that this callback was not the result of a drag */
	if (tp->dragging_tab)
	{
		tp->dragging_tab = False;

		return;
	}

	XfeToolBoxItemToggleOpen(w,tp->items[index]);

	/* Invoke open callbacks */
	if (_XfeToolBoxChildOpen(tp->items[index]))
	{
/* 		printf("Open(%s = %d)\n",XtName(tp->items[index]),index); */
		InvokeCallbacks(w,
						tp->open_callback,
						XmCR_TOOL_BOX_OPEN,
						NULL,
						tp->items[index],
						tp->closed_tabs[index],
						tp->opened_tabs[index],
						index);
	}
	/* Invoke close callbacks */
	else
	{
/* 		printf("Close(%s = %d)\n",XtName(tp->items[index]),index); */
		InvokeCallbacks(w,
						tp->close_callback,
						XmCR_TOOL_BOX_CLOSE,
						NULL,
						tp->items[index],
						tp->closed_tabs[index],
						tp->opened_tabs[index],
						index);
	}
}
/*----------------------------------------------------------------------*/
static void
TabArmCB(Widget button,XtPointer client_data,XtPointer call_data)
{
    Widget					w = XtParent(button);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);

/*  	printf("TabArmCB(%s)\n",XtName(w)); */

	tp->clicking_tab = True;
}
/*----------------------------------------------------------------------*/
static void
TabDisarmCB(Widget button,XtPointer client_data,XtPointer call_data)
{
    Widget					w = XtParent(button);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);

/*  	printf("TabDisarmCB(%s)\n",XtName(w)); */

	tp->clicking_tab = False;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Descendant event handlers											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DescendantEH(Widget			descendant,
			 XtPointer		client_data,
			 XEvent *		event,
			 Boolean *		cont)
{
	Widget				w = (Widget) client_data;
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
    Widget				target = descendant;
	Widget				item = NULL;
	Position			y;

	assert( _XfeIsAlive(descendant) );
	assert( _XfeIsAlive(w) );

	/* Ignore drag request from sensitive XfeButtons */
	if (XfeIsButton(descendant) && 
		_XfeIsSensitive(descendant) && 
		!XfeIsTab(descendant))
	{
		return;
	}

	/* Find the ancestor item for the given descendant */
	if (XfeIsTab(descendant))
	{
		int index = TabToIndex(descendant);

		item = tp->items[index];
	}
	else
	{
		while (!item)
		{
			if (XfeIsToolBox(XtParent(target)))
			{
				item = target;
			}
			else
			{
				target = XtParent(target);
			}
		}
	}

	switch(event->type) 
	{
	case ButtonPress:

		if (event->xbutton.button == tp->drag_button)
		{
			y = event->xbutton.y;

			if (event->xbutton.y > _XfeHeight(w))
			{
				y -= (event->xbutton.y - _XfeHeight(w));
			}
			else if (event->xbutton.y < 0)
			{
				y -= event->xbutton.y;
			}

			DragStart(item,event,y,event->xbutton.y_root);

			/* Clear the dragging tab flag if needed */
			if (XfeIsTab(descendant))
			{
				tp->dragging_tab = False;
			}
		}

		break;

	case ButtonRelease:

		if (event->xbutton.button == tp->drag_button)
		{
			y = event->xbutton.y;
			
			if (event->xbutton.y > _XfeHeight(w))
			{
				y -= (event->xbutton.y - _XfeHeight(w));
			}
			else if (event->xbutton.y < 0)
			{
				y -= event->xbutton.y;
			}
			
			DragEnd(item,event,y,event->xbutton.y_root);
		}

		break;
		
	case MotionNotify:

		if (!(event->xmotion.state & Button1Mask))
		{
			return;
		}

		y = event->xmotion.y;
		

		if (event->xmotion.y > _XfeHeight(w))
		{
			y -= (event->xmotion.y - _XfeHeight(w));
		}
		else if (event->xmotion.y < 0)
		{
			y -= event->xmotion.y;
		}
		
		DragMotion(item,event,event->xmotion.y,event->xmotion.y_root);

		/* Set the dragging tab flag if needed */
		if (XfeIsTab(descendant))
		{
			tp->clicking_tab = False;
			tp->dragging_tab = True;
		}
		
		break;
	}


	*cont = True;
}
/*----------------------------------------------------------------------*/
static void
DragStart(Widget item,XEvent * event,int y,int root_y)
{
	Widget					w = XtParent(item);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	int						index;

/* 	printf("DragStart(%s)\n",XtName(w)); */

	/* Make sure there are at least 2 items managed before dragging */
	if (ActiveItemCount(w) <= 1)
	{
		return;
	}

	index = ItemToIndex(item);

	assert( CHECK_RANGE(tp,index) );

	if (tp->drag_cursor != None)
	{
		XfeCursorDefine(w,tp->drag_cursor);
	}

	tp->dragging = True;

	tp->start_drag_y = root_y;

	tp->original_y = _XfeY(tp->items[index]);

	tp->last_y = _XfeY(tp->items[index]);

	tp->last_moved_item = NULL;

	tp->drag_direction = DIRECTION_NONE;

	/* Make sure the item being dragged is on top of other items */
	RaiseItem(w,index);
}
/*----------------------------------------------------------------------*/
static void
DragEnd(Widget item,XEvent * event,int y,int root_y)
{
	Widget					w = XtParent(item);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(XtParent(item));
	int						index;

/* 	printf("DragEnd(%s)\n",XtName(w)); */

	tp->dragging = False;

	index = ItemToIndex(item);

	assert( CHECK_RANGE(tp,index) );

	if (tp->drag_cursor != None)
	{
		XfeCursorUndefine(w);
	}

	if (!tp->clicking_tab)
	{
		/* Snap the item in place */
		SnapItemInPlace(w,index);
	}

	LayoutComponents(w);
	LayoutChildren(w);
}
/*----------------------------------------------------------------------*/
static void
DragMotion(Widget item,XEvent * event,int y,int root_y)
{
	Widget					w = XtParent(item);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	int						dy;
	int						index;
	int						new_y;
	int						old_drag_direction = tp->drag_direction;
	int						under_index;

/* 	printf("DragMotion(%s)\n",XtName(w)); */

	index = ItemToIndex(item);

	assert( CHECK_RANGE(tp,index) );

	/* Make sure we are dragging */
	if (!tp->dragging)
	{
		return;
	}

	/* Compute the change in position */
	dy = root_y - tp->start_drag_y;

	/* Make sure some change in y occurred before moving item */
	if (!dy)
	{
		return;
	}

	/* Compute the new item's position */
	new_y = tp->original_y + dy;

	/* Make sure the item moved only a reasonable amount to avoid holes */

	/* Check down */
	if (new_y > tp->last_y)
	{
		if ((new_y - tp->last_y) > tp->drag_threshold)
		{
			new_y = tp->last_y + tp->drag_threshold;
		}
	}
	/* Check up */
	else
	{
		if ((tp->last_y - new_y) > tp->drag_threshold)
		{
			new_y = tp->last_y - tp->drag_threshold;
		}
	}
		
	/* Make sure the new y position is within range */
	if (new_y < _XfemOffsetTop(w))
	{
		new_y = _XfemOffsetTop(w);
	}
	else if (new_y > MaxItemY(tp->items[index]))
	{
		new_y = MaxItemY(tp->items[index]);
	}

	/* Make sure the new y position is different */
	if (_XfeY(item) == new_y)
	{
		return;
	}

	/* Move the item to its new position */
	MoveItem(w,index,new_y);

	/* Compute the new drag direction */
	if (new_y > tp->last_y)
	{
		tp->drag_direction = DIRECTION_DOWN;
	}
	else
	{
		tp->drag_direction = DIRECTION_UP;
	}	

	/* Find out if an item is underneath the item */
	under_index = FindItemUnder(w,index);

	if (CHECK_RANGE(tp,under_index))
	{
		Widget		under_item = tp->items[under_index];
		Position	empty_slot_y;

		/* Make sure the item underneath was not already swapped */
		if (under_item != tp->last_moved_item)
		{
			/* Up */
			if (tp->drag_direction == DIRECTION_UP)
			{
				if (index == LastManagedIndex(w))
				{
					empty_slot_y = MaxItemY(tp->items[under_index]);
				}
				else
				{
					int next_index = NextManagedIndex(w,index);
					
					empty_slot_y = 
						_XfeY(tp->items[next_index]) - 
						_XfeHeight(tp->items[under_index]) -
						tp->vertical_spacing;
				}
			}
			/* Down */
			else
			{
				if (index == FirstManagedIndex(w))
				{
					empty_slot_y = _XfemRectY(w);
				}
				else
				{
					int prev_index = PreviousManagedIndex(w,index);

					empty_slot_y = 
						_XfeY(tp->items[prev_index]) +
						_XfeHeight(tp->items[prev_index]) +
						tp->vertical_spacing;
				}
			}

			/* Swap the item being dragged with the underneath item */
			MoveItem(w,under_index,empty_slot_y);
			
			SwapItems(w,index,under_index);
			
			tp->last_moved_item = under_item;

			/* Update the position_index contraint member of the items */
			UpdatePositionIndeces(w);

/* 			printf("Swap(%s = %d)\n",XtName(tp->items[index]),index); */

			/* Invoke the swap callbacks */
			InvokeSwapCallbacks(w,event,item,under_item,index,under_index);
		}
	}

	/* If the direction changed within a drag, rest the last moved item */
	if (old_drag_direction != tp->drag_direction)
	{
		if (_XfeIsAlive(tp->last_moved_item))
		{
			tp->last_moved_item = NULL;
		}
	}

	tp->last_y = new_y;
}
/*----------------------------------------------------------------------*/
static Boolean
MoveItem(Widget w,Cardinal index,int y)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);

	if (_XfeY(tp->items[index]) != y)
	{
		/* Move the item */
		_XfeMoveWidget(tp->items[index],_XfeX(tp->items[index]),y);
		
		/* Move the item's tab button */
		_XfeMoveWidget(tp->opened_tabs[index],_XfeX(tp->opened_tabs[index]),y);

		return True;
	}

	return False;
}
/*----------------------------------------------------------------------*/
static int
MaxItemY(Widget item)
{
	Widget					w = XtParent(item);
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Dimension				max_y  = _XfeHeight(w) - _XfemOffsetBottom(w);

	/* Include the closed tab button height if any are active */
	if (ActiveItemCount(w) < tp->item_count)
	{
		max_y -= _XfeHeight(tp->closed_tabs[tp->item_count - 1]);
	}

	max_y -= _XfeHeight(item);

	return max_y;
}
/*----------------------------------------------------------------------*/
static void
SwapItems(Widget w,int i,int j)
{
	XfeToolBoxPart *			tp = _XfeToolBoxPart(w);
 	XfeToolBoxConstraintPart *	cpi;
 	XfeToolBoxConstraintPart *	cpj;

	assert( CHECK_RANGE(tp,i) );
	assert( CHECK_RANGE(tp,j) );

	/* Swap the item */
	_XfeSwap(tp->items[i],tp->items[j],Widget);

	/* Swap the tab buttons */
	_XfeSwap(tp->closed_tabs[i],tp->closed_tabs[j],Widget);
	_XfeSwap(tp->opened_tabs[i],tp->opened_tabs[j],Widget);

	/* Swap the item state */
	cpi = _XfeToolBoxConstraintPart(tp->items[i]);
	cpj = _XfeToolBoxConstraintPart(tp->items[j]);
	
	_XfeSwap(cpi->open,cpj->open,Boolean);
}
/*----------------------------------------------------------------------*/
static int
ItemToIndex(Widget item)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(XtParent(item));
	Cardinal				i = 0;

	for (i = 0; i < tp->item_count; i++)
	{
		if (item == tp->items[i])
		{
			return i;
		}
	}

	return XmTOOL_BOX_NOT_FOUND;
}
/*----------------------------------------------------------------------*/
static int
TabToIndex(Widget tab)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(XtParent(tab));
	Cardinal				i = 0;

	for (i = 0; i < tp->item_count; i++)
	{
		if ((tab == tp->closed_tabs[i]) || (tab == tp->opened_tabs[i]))
		{
			return i;
		}
	}

	return XmTOOL_BOX_NOT_FOUND;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Item functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
UpdateItems(Widget w)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;
	Cardinal				new_count = 0;

	/* First, count the number of items that are alive */
	for (i = 0; i < tp->item_count; i++)
	{
		if (_XfeIsAlive(tp->items[i]))
		{
			new_count++;
		}
	}

	if (!new_count)
	{
		if (tp->items)
		{
			XtFree((char *) tp->items);
		}

		if (tp->closed_tabs)
		{
			XtFree((char *) tp->closed_tabs);
		}

		if (tp->opened_tabs)
		{
			XtFree((char *) tp->opened_tabs);
		}

		tp->items		= NULL;
		tp->closed_tabs = NULL;
		tp->opened_tabs	= NULL;
		tp->item_count	= 0;
	}
	else
	{
		WidgetList	new_items = 
			(WidgetList) XtMalloc(sizeof(Widget) * new_count);

		WidgetList	new_closed_tabs  = 
			(WidgetList) XtMalloc(sizeof(Widget) * new_count);

		WidgetList	new_opened_tabs  = 
			(WidgetList) XtMalloc(sizeof(Widget) * new_count);

		Cardinal	index = 0;
		
		for (i = 0; i < tp->item_count; i++)
		{
			if (_XfeIsAlive(tp->items[i]))
			{
				new_items[index]		= tp->items[i];
				new_closed_tabs[index]	= tp->closed_tabs[i];
				new_opened_tabs[index]	= tp->opened_tabs[i];

				index++;
			}
		}

		if (tp->items)
		{
			XtFree((char *) tp->items);
		}

		if (tp->closed_tabs)
		{
			XtFree((char *) tp->closed_tabs);
		}

		if (tp->opened_tabs)
		{
			XtFree((char *) tp->opened_tabs);
		}

		tp->items		= new_items;
		tp->closed_tabs	= new_closed_tabs;
		tp->opened_tabs	= new_opened_tabs;
		tp->item_count	= new_count;
	}
}
/*----------------------------------------------------------------------*/

static int
FirstManagedIndex(Widget w)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;

	for (i = 0; i < tp->item_count; i++)
	{
		if (XtIsManaged(tp->items[i]))
		{
			return i;
		}
	}

	return XmTOOL_BOX_NOT_FOUND;
}
/*----------------------------------------------------------------------*/
static int
LastManagedIndex(Widget w)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;

	for (i = (tp->item_count - 1); i >= 0; i--)
	{
		if (XtIsManaged(tp->items[i]))
		{
			return i;
		}
	}

	return XmTOOL_BOX_NOT_FOUND;
}
/*----------------------------------------------------------------------*/
static int
NextManagedIndex(Widget w,int index)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;

	assert( CHECK_RANGE(tp,index) );
	assert( CHECK_RANGE(tp,index + 1) );

	for (i = (index + 1); i < tp->item_count; i++)
	{
		if (XtIsManaged(tp->items[i]))
		{
			return i;
		}
	}

	return XmTOOL_BOX_NOT_FOUND;
}
/*----------------------------------------------------------------------*/
static int
PreviousManagedIndex(Widget w,int index)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;

	assert( CHECK_RANGE(tp,index) );
	assert( CHECK_RANGE(tp,index - 1) );

	for (i = (index - 1); i >= 0; i--)
	{
		if (XtIsManaged(tp->items[i]))
		{
			return i;
		}
	}

	return XmTOOL_BOX_NOT_FOUND;
}
/*----------------------------------------------------------------------*/
static void
RaiseItem(Widget w,int index)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);

	assert( CHECK_RANGE(tp,index) );
	assert( _XfeIsAlive(tp->items[index]) );
	assert( _XfeIsAlive(tp->opened_tabs[index]) );

	XRaiseWindow(XtDisplay(w),_XfeWindow(tp->items[index]));
	XRaiseWindow(XtDisplay(w),_XfeWindow(tp->opened_tabs[index]));
}
/*----------------------------------------------------------------------*/
static void
SnapItemInPlace(Widget w,int index)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	int						last_managed_index;
	int						first_managed_index;
	Boolean					moved = False;

	assert( CHECK_RANGE(tp,index) );
	assert( _XfeIsAlive(tp->items[index]) );
	assert( _XfeIsAlive(tp->opened_tabs[index]) );

 	first_managed_index = FirstManagedIndex(w);
	last_managed_index = LastManagedIndex(w);

	/* Check for the first item */
	if (index == first_managed_index)
	{
		moved = MoveItem(w,index,_XfemRectY(w));
	}
	/* Check for the last item */
	else if (index == last_managed_index)
	{
		moved = MoveItem(w,index,MaxItemY(tp->items[index]));
	}
	/* Other in-between items */
	else
	{
		int next_managed_index = NextManagedIndex(w,index);
		int previous_managed_index = PreviousManagedIndex(w,index);

		assert( CHECK_RANGE(tp,previous_managed_index) );
		assert( CHECK_RANGE(tp,next_managed_index) );

		/* Check previous item */
		if (ItemOverItem(w,index,previous_managed_index,0))
		{
			moved = MoveItem(w,
							 index,
							 _XfeY(tp->items[previous_managed_index]) + 
							 _XfeHeight(tp->items[previous_managed_index]) + 
							 tp->vertical_spacing);
		}
		/* Check next item */
		/*else if (ItemOverItem(w,index,next_managed_index,0))*/
		else
		{
			moved = MoveItem(w,
							 index,
							 _XfeY(tp->items[next_managed_index]) - 
							 _XfeHeight(tp->items[index]) - 
							 tp->vertical_spacing);
		}
	}

	/* Invoke snap callbacks only if the item actually moved */
	if (moved)
	{
		InvokeCallbacks(w,
						tp->snap_callback,
						XmCR_TOOL_BOX_SNAP,
						NULL,
						tp->items[index],
						tp->closed_tabs[index],
						tp->opened_tabs[index],
						index);
	}
}
/*----------------------------------------------------------------------*/
static int
FindItemUnder(Widget w,int over_index)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;

	assert( CHECK_RANGE(tp,over_index) );

	for (i = 0; i < tp->item_count; i++)
	{
		/* Ignore the over item */
		if (i != over_index)
		{
			if (XtIsManaged(tp->items[i]))
			{
				if (ItemOverItem(w,over_index,i,tp->swap_threshold))
				{
					return i;
				}
			}
		}
	}

	return XmTOOL_BOX_NOT_FOUND;
}
/*----------------------------------------------------------------------*/
static Boolean
ItemOverItem(Widget w,int over_index,int under_index,Dimension threshold)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Widget					over_item;
	Widget					under_item;
	Position				over_y0;
	Position				over_y1;
	Position				under_y0;
	Position				under_y1;

	assert( CHECK_RANGE(tp,over_index) );
	assert( CHECK_RANGE(tp,under_index) );

	over_item = tp->items[over_index];
	under_item = tp->items[under_index];

	over_y0 = _XfeY(over_item);
	over_y1 = _XfeY(over_item) + _XfeHeight(over_item);

	under_y0 = _XfeY(under_item);
	under_y1 = _XfeY(under_item) + _XfeHeight(under_item);

	assert( _XfeIsAlive(over_item) );
	assert( _XfeIsAlive(under_item) );

	return (

		((over_y1 > under_y1) && (over_y0 < under_y0)) ||
		((over_y1 > (under_y0 + threshold)) && (over_y1 < under_y1)) ||
		((over_y0 > under_y0) && (over_y0 < (under_y1 - threshold))) 

		);
}
/*----------------------------------------------------------------------*/
static void
CheckSwapThreshold(Widget w)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;

	return;

	for (i = 0; i < tp->item_count; i++)
	{
		if (tp->swap_threshold > (_XfeHeight(tp->items[i]) - 4))
		{
			tp->swap_threshold = (_XfeHeight(tp->items[i]) - 4);

			_XfeWarning(w,MESSAGE4);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
UpdatePixmaps(Widget w)
{
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	Cardinal			i;
	Arg					oav[6];
	Cardinal			oac = 0;
	Arg					cav[6];
	Cardinal			cac = 0;

	/* Opened - vertical */
	XtSetArg(oav[oac],XmNtopPixmap,				tp->top_pixmap); oac++;
	XtSetArg(oav[oac],XmNtopRaisedPixmap,		tp->top_raised_pixmap); oac++;
	
	XtSetArg(oav[oac],XmNbottomPixmap,			tp->bottom_pixmap); oac++;
	XtSetArg(oav[oac],XmNbottomRaisedPixmap,	tp->bottom_raised_pixmap); oac++;
	
	XtSetArg(oav[oac],XmNverticalPixmap,		tp->vertical_pixmap); oac++;
	XtSetArg(oav[oac],XmNverticalRaisedPixmap,	tp->vertical_raised_pixmap); oac++;
	
	/* Closed - horizontal */
	XtSetArg(cav[cac],XmNleftPixmap,			tp->left_pixmap); cac++;
	XtSetArg(cav[cac],XmNleftRaisedPixmap,		tp->left_raised_pixmap); cac++;
	
	XtSetArg(cav[cac],XmNrightPixmap,			tp->right_pixmap); cac++;
	XtSetArg(cav[cac],XmNrightRaisedPixmap,		tp->right_raised_pixmap); cac++;
	
	XtSetArg(cav[cac],XmNhorizontalPixmap,		tp->horizontal_pixmap); cac++;
	XtSetArg(cav[cac],XmNhorizontalRaisedPixmap,tp->horizontal_raised_pixmap); cac++;

	for (i = 0; i < tp->item_count; i++)
	{
		XtSetValues(tp->opened_tabs[i],oav,oac);
		XtSetValues(tp->closed_tabs[i],cav,cac);
	}
}
/*----------------------------------------------------------------------*/
static void
InvokeSwapCallbacks(Widget		w,
					XEvent *	event,
					Widget		swapped,
					Widget		displaced,
					int			from_index,
					int			to_index)
{
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);

	if (tp->swap_callback)
	{
		XfeToolBoxSwapCallbackStruct cbs;

		cbs.reason		= XmCR_TOOL_BOX_SWAP;
		cbs.event		= event;
		cbs.swapped		= swapped;
		cbs.displaced	= displaced;
		cbs.from_index	= from_index;
		cbs.to_index	= to_index;

		/* Invoke the Callback List */
		XtCallCallbackList(w,tp->swap_callback,&cbs);
	}
}
/*----------------------------------------------------------------------*/
static void
InvokeCallbacks(Widget			w,
				XtCallbackList	list,
				int				reason,
				XEvent *		event,
				Widget			item,
				Widget			closed_tab,
				Widget			opened_tab,
				int				index)
{
	if (list)
	{
		XfeToolBoxCallbackStruct cbs;

		cbs.reason		= reason;
		cbs.event		= event;
		cbs.item		= item;
		cbs.closed_tab	= closed_tab;
		cbs.opened_tab	= opened_tab;
		cbs.index		= index;

		XFlush(XtDisplay(w));

		/* Invoke the Callback List */
		XtCallCallbackList(w,list,&cbs);
	}
}
/*----------------------------------------------------------------------*/
static void
UpdatePositionIndeces(Widget w)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;
	Cardinal				index = 0;

	/* Update the position_index contraint resource of the items */
	for (i = 0; i < tp->item_count; i++)
	{
		if (_XfeIsAlive(tp->items[i]))
		{
			_XfeManagerPositionIndex(tp->items[i]) = index;

			index++;
		}
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Child functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ChildSetDraggable(Widget w,Widget child,Boolean draggable)
{
	if (_XfeIsAlive(child))
	{
		/* First remove any event handler that might have been added */
		XtRemoveEventHandler(child,DRAG_EVENTS,True,DescendantEH,w);

		/* Add the event handler if needed */
		if (draggable)
		{
			XtAddEventHandler(child,DRAG_EVENTS,True,DescendantEH,w);
		}
	}
}
/*----------------------------------------------------------------------*/

#if 0
static void
UpdatePositionIndeces(Widget w)
{
	XfeToolBoxPart *		tp = _XfeToolBoxPart(w);
	Cardinal				i;

	/* Update the position_index contraint resource of the items */
	for (i = 0; i < tp->item_count; i++)
	{
		
		assert( _XfeIsAlive(tp->items[i]) );

		_XfeManagerPositionIndex(tp->items[i]) = i;
	}


}
/*----------------------------------------------------------------------*/
#endif
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateToolBox(Widget parent,char *name,Arg *args,Cardinal count)
{
   return (XtCreateWidget(name,xfeToolBoxWidgetClass,parent,args,count));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox drag descendant											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolBoxAddDragDescendant(Widget w,Widget descendant)
{
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );
	assert( _XfeIsAlive(descendant) );

	ChildSetDraggable(w,descendant,True);
}
/*----------------------------------------------------------------------*/
/* extern */ extern void
XfeToolBoxRemoveDragDescendant(Widget w,Widget descendant)
{
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );

	ChildSetDraggable(w,descendant,False);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox index methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ int
XfeToolBoxItemGetIndex(Widget w,Widget item)
{
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	Cardinal			i;
	
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );
	assert( _XfeIsAlive(item) );

	for (i = 0; i < tp->item_count; i++)
	{
		if (item == tp->items[i])
		{
			return i;
		}
	}

	return XmTOOL_BOX_NOT_FOUND;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeToolBoxItemGetByIndex(Widget w,Cardinal index)
{
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );

	if (index >= tp->item_count)
	{
		return NULL;
	}

	return tp->items[index];
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeToolBoxItemGetTab(Widget w,Widget item,Boolean opened)
{
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	Cardinal			i = XfeToolBoxItemGetIndex(w,item);

	if (i != XmTOOL_BOX_NOT_FOUND)
	{
		return opened ? tp->opened_tabs[i] : tp->closed_tabs[i];
	}

	return NULL;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox position methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolBoxItemSetPosition(Widget w,Widget item,int pos)
{
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	int					old_pos;
	
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );
	assert( tp->item_count > 1 );

	if (pos == XmLAST_POSITION)
	{
		pos = tp->item_count - 1;
	}

 	assert( pos < tp->item_count );
	assert( pos >= 0 );

	if ((tp->item_count < 2) ||
		(pos < 0) || 
		(pos >= tp->item_count) || 
		(pos == _XfeManagerPositionIndex(item)))
	{
		return;
	}

	/* Last position */
	if (pos == (tp->item_count - 1))
	{
		old_pos = pos - 1;
	}
	/* Between 0 and last */
	else
	{
		old_pos = pos + 1;
	}

	SwapItems(w,pos,old_pos);

	/* Update the position_index contraint member of the items */
	UpdatePositionIndeces(w);
	
	LayoutComponents(w);
	LayoutChildren(w);

/* 	printf("Swap(%s = %d)\n",XtName(tp->items[pos]),pos); */

	/* Invoke the swap callbacks */
	InvokeSwapCallbacks(w,NULL,tp->items[pos],tp->items[old_pos],pos,old_pos);
}
/*----------------------------------------------------------------------*/
/* extern */ int
XfeToolBoxItemGetPosition(Widget w,Widget item)
{
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );

	return _XfeManagerPositionIndex(item);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBox open methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolBoxItemSetOpen(Widget w,Widget item,Boolean open)
{
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	int					index;
	
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );
	assert( _XfeIsAlive(item) );
	assert( tp->item_count >= 1 );

	index = ItemToIndex(item);

	assert( CHECK_RANGE(tp,index) );
	
	/* Make sure open is different */
	if (_XfeToolBoxChildOpen(tp->items[index]) == open)
	{
		return;
	}

#if 0
	printf("XtVaSetValues(%s,XmNopen,%s,NULL)\n",
		   XtName(item),
		   open ? "open" : "closed");
#endif

	XtVaSetValues(item,XmNopen,open,NULL);
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeToolBoxItemGetOpen(Widget w,Widget item)
{
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );

	return _XfeToolBoxChildOpen(item);
}
/*----------------------------------------------------------------------*/
/* extern */  void
XfeToolBoxItemToggleOpen(Widget w,Widget item)
{
	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );

	XfeToolBoxItemSetOpen(w,item,!_XfeToolBoxChildOpen(item));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBoxIsNeeded()													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeToolBoxIsNeeded(Widget w)
{
	XfeToolBoxPart *	tp = _XfeToolBoxPart(w);
	Cardinal			i;

	assert( _XfeIsAlive(w) );
	assert( XfeIsToolBox(w) );

	if (tp->item_count)
	{
		for (i = 0; i < tp->item_count; i++)
		{
			/* If at least one child is shown, the tool box is needed */
			if (_XfeChildIsShown(tp->items[i]))
			{
				return True;
			}
		}
	}
	
	return False;
}
/*----------------------------------------------------------------------*/
