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
/* Name:		<Xfe/Pane.c>											*/
/* Description:	XfePane widget source.									*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/PaneP.h>

#include <stdio.h>

#define MESSAGE1 "Widget is not an XfePane."
#define MESSAGE2 "XmNpaneChildType can only be set at creationg time."
#define MESSAGE3 "Only one child attachment can be XmNalwaysVisible."
#define MESSAGE4 "The descendant for XfePaneAddDragDescendant() is invalid."
#define MESSAGE5 "Cannot find a valid ancestor attachment for '%s'."

#define DEFAULT_SASH_POSITION 300

#define TWO_TIMES_SST(sp) ((sp)->sash_shadow_thickness << 1)

#define ALWAYS_VISIBLE(w) \
( _XfeChildIsShown(w) && (_XfePaneConstraintPart(w) -> always_visible) )

#define CONSTRAINT_ALLOW_EXPAND(w) \
(_XfeIsAlive(w) && (_XfePaneConstraintPart(w) -> allow_expand))

#define CONSTRAINT_ALLOW_RESIZE(w) \
(_XfeIsAlive(w) && (_XfePaneConstraintPart(w) -> allow_resize))

#define SASH_SPACING(cp) \
(sp->sash_thickness ? sp->sash_spacing : 0)

#define CHOOSE_X_OR_Y(o,x,y) \
(((o) == XmVERTICAL) ? (y) : (x))

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize			(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy				(Widget);
static Boolean	SetValues			(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ConstraintInitialize(Widget,Widget,ArgList,Cardinal *);
static Boolean	ConstraintSetValues	(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static void		MinimumGeometry	(Widget,Dimension *,Dimension *);
static Boolean	AcceptChild			(Widget);
static Boolean	InsertChild			(Widget);
static Boolean	DeleteChild			(Widget);
static void		PrepareComponents	(Widget,int);
static void		LayoutComponents	(Widget);
static void		LayoutChildren		(Widget);
static void		DrawComponents		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeOriented class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	EnterProc				(Widget,Widget,int,int);
static void 	MotionProc				(Widget,Widget,int,int);
static void 	DragStart				(Widget,Widget,int,int);
static void 	DragEnd					(Widget,Widget,int,int);
static void 	DragMotion				(Widget,Widget,int,int);
static void 	DescendantDragStart		(Widget,Widget,int,int);
static void 	DescendantDragEnd		(Widget,Widget,int,int);
static void 	DescendantDragMotion	(Widget,Widget,int,int);

/*----------------------------------------------------------------------*/
/*																		*/
/* Child functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean		ChildShouldExpand			(Widget,Widget);
static void			ChildLayout					(Widget,Widget,Widget,Widget,
												 Widget,XfeGeometry);
static void			ChildOneLayoutVertical		(Widget);
static void			ChildOneLayoutHorizontal	(Widget);
static void			ChildTwoLayoutVertical		(Widget);
static void			ChildTwoLayoutHorizontal	(Widget);

static Boolean		ChildOneAttachmentsAreFull	(Widget);
static Boolean		ChildTwoAttachmentsAreFull	(Widget);

static Dimension	ChildMaxSize				(Widget,Widget);
static Dimension	ChildMinSize				(Widget,Widget);

static Boolean		ChildIsAttachment			(Widget,Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Descendant functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget		DescendantFindAttachment	(Widget,Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Attachment functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		AttachmentLayout			(Widget,Widget,XfeGeometry);
static void		AttachmentSetMappedAll		(Widget,Widget,Boolean);
static void		AttachmentSetMappedChild	(Widget,Widget,Widget);
static Widget	AttachmentGetVisibleChild	(Widget,Widget);
static void		AttachmentLoadAll			(Widget,Widget,Widget *);
static Boolean	AttachmentContainsXY		(Widget,int,int);

/*----------------------------------------------------------------------*/
/*																		*/
/* Sash functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		SashLayout					(Widget,Position,XRectangle *);
static Boolean	SashContainsXY				(Widget,int,int);
static void		SashDrawDragging			(Widget,XRectangle *);
static void		SashDrawShadow				(Widget,XRectangle *,Boolean);
static void		SashDragStart				(Widget,int,int,Widget);
static void		SashDragEnd					(Widget,int,int,Widget);
static void		SashDragMotion				(Widget,int,int,Widget);
static Position	SashMinPosition				(Widget);
static Position	SashMaxPosition				(Widget);
static void		SashVerifyPosition			(Widget,Dimension,Dimension);

static Position	SashMinPositionVertical		(Widget);
static Position	SashMaxPositionHorizontal	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfePane functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean		HasChildOneAttachment		(Widget);
static Boolean		HasChildTwoAttachment		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* IsAttachment test function.											*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean	FindTestIsAttachment	(Widget,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint resource callprocs										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DefaultPaneChildType			(Widget,int,XrmValue *);
static void		DefaultPaneChildAttachment		(Widget,int,XrmValue *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePane resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Sash resources */
	{
		XmNsashAlwaysVisible,
		XmCSashAlwaysVisible,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_always_visible),
		XmRImmediate, 
		(XtPointer) True
	},
	{
		XmNsashColor,
		XmCSashColor,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_color),
		XmRCallProc, 
		(XtPointer) _XfeCallProcCopyForeground
	},
	{
		XmNsashOffset,
		XmCOffset,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_offset),
		XmRImmediate, 
		(XtPointer) 0
	},
    { 
		XmNsashPosition,
		XmCPosition,
		XmRPosition,
		sizeof(Position),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_position),
		XmRImmediate, 
		(XtPointer) DEFAULT_SASH_POSITION
    },
	{
		XmNsashShadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_shadow_thickness),
		XmRCallProc, 
		(XtPointer) _XfeCallProcCopyShadowThickness
	},
	{
		XmNsashShadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_shadow_type),
		XmRImmediate, 
		(XtPointer) XmSHADOW_OUT
	},
	{
		XmNsashSpacing,
		XmCSashSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_spacing),
		XmRImmediate, 
		(XtPointer) 2
	},
	{
		XmNsashThickness,
		XmCSashThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_thickness),
		XmRImmediate, 
		(XtPointer) 10
	},


	/* Drag resources */
	{
		XmNpaneSashType,
		XmCPaneSashType,
		XmRPaneSashType,
		sizeof(unsigned char),
		XtOffsetOf(XfePaneRec , xfe_pane . pane_sash_type),
		XmRImmediate, 
		(XtPointer) XmPANE_SASH_DOUBLE_LINE
	},
	{
		XmNpaneDragMode,
		XmCPaneDragMode,
		XmRPaneDragMode,
		sizeof(unsigned char),
		XtOffsetOf(XfePaneRec , xfe_pane . pane_drag_mode),
		XmRImmediate, 
		(XtPointer) XmPANE_DRAG_PRESERVE_ONE
	},
	{ 
		XmNdragThreshold,
		XmCDragThreshold,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . drag_threshold),
		XmRImmediate, 
		(XtPointer) 1
    },

    /* Child one resources */
    { 
		XmNchildOne,
		XmCChildOne,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . child_one),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNattachmentOneBottom,
		XmCAttachmentOneBottom,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . attachment_one_bottom),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNattachmentOneLeft,
		XmCAttachmentOneLeft,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . attachment_one_left),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNattachmentOneRight,
		XmCAttachmentOneRight,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . attachment_one_right),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNattachmentOneTop,
		XmCAttachmentOneTop,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . attachment_one_top),
		XmRImmediate, 
		(XtPointer) NULL
    },

    /* Child two resources */
    { 
		XmNchildTwo,
		XmCChildTwo,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . child_two),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNattachmentTwoBottom,
		XmCAttachmentTwoBottom,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . attachment_two_bottom),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNattachmentTwoLeft,
		XmCAttachmentTwoLeft,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . attachment_two_left),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNattachmentTwoRight,
		XmCAttachmentTwoRight,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . attachment_two_right),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNattachmentTwoTop,
		XmCAttachmentTwoTop,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePaneRec , xfe_pane . attachment_two_top),
		XmRImmediate, 
		(XtPointer) NULL
    },

    /* Force XmNallowDrag to True */
	{ 
		XmNallowDrag,
		XmCBoolean,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePaneRec , xfe_oriented . allow_drag),
		XmRImmediate, 
		(XtPointer) True
    },

	/* Force XmNusePreferredHeight and XmNusePreferredWidth to False */
	{
		XmNusePreferredHeight,
		XmCUsePreferredHeight,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePaneRec , xfe_manager . use_preferred_height),
		XmRImmediate, 
		(XtPointer) False
	},
	{
		XmNusePreferredWidth,
		XmCUsePreferredWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePaneRec , xfe_manager . use_preferred_width),
		XmRImmediate, 
		(XtPointer) False
	},
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePane synthetic resources											*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
    { 
		XmNsashOffset,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_offset),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNsashShadowThickness,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_shadow_thickness),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNsashThickness,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_thickness),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNsashSpacing,
		sizeof(Dimension),
		XtOffsetOf(XfePaneRec , xfe_pane . sash_spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePane constraint resources											*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource constraint_resources[] = 
{
    { 
		XmNpaneMaximum,
		XmCPaneMaximum,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . pane_maximum),
		XmRImmediate,
		(XtPointer) 65535
    },
    { 
		XmNpaneMinimum,
		XmCPaneMinimum,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . pane_minimum),
		XmRImmediate,
		(XtPointer) 40
    },
    { 
		XmNpaneChildType,
		XmCPaneChildType,
		XmRPaneChildType,
		sizeof(unsigned char),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . pane_child_type),
		XmRCallProc,
		(XtPointer) DefaultPaneChildType
    },
    { 
		XmNpaneChildAttachment,
		XmCPaneChildAttachment,
		XmRPaneChildAttachment,
		sizeof(unsigned char),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . pane_child_attachment),
		XmRCallProc,
		(XtPointer) DefaultPaneChildAttachment
    },
    { 
		XmNalwaysVisible,
		XmCAlwaysVisible,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . always_visible),
		XmRImmediate,
		(XtPointer) False
    },
    { 
		XmNallowExpand,
		XmCBoolean,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . allow_expand),
		XmRImmediate,
		(XtPointer) True
    },
    { 
		XmNallowResize,
		XmCBoolean,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . allow_resize),
		XmRImmediate,
		(XtPointer) True
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePane constraint synthetic resources								*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource constraint_synthetic_resources[] =
{
    { 
		XmNpaneMaximum,
		sizeof(Dimension),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . pane_maximum),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNpaneMinimum,
		sizeof(Dimension),
		XtOffsetOf(XfePaneConstraintRec , xfe_pane . pane_minimum),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(pane,Pane) =
{
    {
		(WidgetClass) &xfeOrientedClassRec,		/* superclass			*/
		"XfePane",								/* class_name			*/
		sizeof(XfePaneRec),						/* widget_size			*/
		NULL,									/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		NULL,									/* actions				*/
		0,										/* num_actions			*/
		resources,                              /* resources			*/
		XtNumber(resources),                    /* num_resources		*/
		NULLQUARK,                              /* xrm_class			*/
		TRUE,                                   /* compress_motion		*/
		XtExposeCompressMaximal,                /* compress_exposure	*/
		TRUE,                                   /* compress_enterleave	*/
		FALSE,                                  /* visible_interest		*/
		Destroy,								/* destroy				*/
		XtInheritResize,                        /* resize				*/
		XtInheritExpose,						/* expose				*/
		SetValues,                              /* set_values			*/
		NULL,                                   /* set_values_hook		*/
		XtInheritSetValuesAlmost,				/* set_values_almost	*/
		NULL,									/* get_values_hook		*/
		NULL,                                   /* access_focus			*/
		XtVersion,                              /* version				*/
		NULL,                                   /* callback_private		*/
		XtInheritTranslations,					/* tm_table				*/
		XtInheritQueryGeometry,					/* query_geometry		*/
		XtInheritDisplayAccelerator,            /* display accelerator	*/
		NULL,                                   /* extension			*/
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
		sizeof(XfePaneConstraintRec),			/* constraint size		*/
		ConstraintInitialize,					/* init proc			*/
		NULL,									/* destroy proc			*/
		ConstraintSetValues,					/* set values proc		*/
		NULL,                                   /* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		synthetic_resources,					/* syn resources		*/
		XtNumber(synthetic_resources),			/* num syn_resources	*/
		constraint_synthetic_resources,			/* syn_cont_resources  	*/
		XtNumber(constraint_synthetic_resources),/* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process		*/
		NULL,                                   /* extension			*/
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
		PrepareComponents,						/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		LayoutChildren,							/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		DrawComponents,							/* draw_components		*/
		False,									/* count_layable_children*/
		NULL,									/* child_is_layable		*/
		NULL,									/* extension			*/
    },

	/* XfeOriented Part */
	{
		EnterProc,								/* enter				*/
		XfeInheritLeave,						/* leave				*/
		MotionProc,								/* motion				*/
		DragStart,								/* drag_start			*/
		DragEnd,								/* drag_end				*/
		DragMotion,								/* drag_motion			*/
		XfeInheritDescendantEnter,				/* des_enter			*/
		XfeInheritDescendantLeave,				/* des_leave			*/
		XfeInheritDescendantMotion,				/* des_motion			*/
		DescendantDragStart,					/* des_drag_start		*/
		DescendantDragEnd,						/* des_drag_end			*/
		DescendantDragMotion,					/* des_drag_motion		*/
		NULL,									/* extension          	*/
	},
	
    /* XfePane Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfePaneWidgetClass declaration.										*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(pane,Pane);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint resource callprocs										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DefaultPaneChildType(Widget child,int offset,XrmValue * value)
{
	Widget					w = _XfeParent(child);
	XfePanePart *			sp = _XfePanePart(w);
    static unsigned char	pane_child_type;

	/* Initialize the child type to none */
	pane_child_type = XmPANE_CHILD_NONE;

	/* Child one is alive */
	if (_XfeIsAlive(sp->child_one))
	{
		/* Look for child two if not alive */
		if (!_XfeIsAlive(sp->child_two))
		{
			pane_child_type = XmPANE_CHILD_WORK_AREA_TWO;
		}
		/* Look for child one attachment if not full */
		else if (!ChildOneAttachmentsAreFull(w))
		{
			pane_child_type = XmPANE_CHILD_ATTACHMENT_ONE;
		}
		else if (!ChildTwoAttachmentsAreFull(w))
		{
			pane_child_type = XmPANE_CHILD_ATTACHMENT_TWO;
		}
	}
	/* Child two is alive */
	else if (_XfeIsAlive(sp->child_two))
	{
		/* Child one must not be alive at this point */
		pane_child_type = XmPANE_CHILD_WORK_AREA_ONE;
	}
	/* Neither child one or two are alive */
	else
	{
		pane_child_type = XmPANE_CHILD_WORK_AREA_ONE;
	}

    value->addr = (XPointer) &pane_child_type;
    value->size = sizeof(pane_child_type);
}
/*----------------------------------------------------------------------*/
static void
DefaultPaneChildAttachment(Widget child,int offset,XrmValue * value)
{
	Widget					w = _XfeParent(child);
	XfePanePart *			sp = _XfePanePart(w);
	XfePaneConstraintPart *	cp = _XfePaneConstraintPart(child);
    static unsigned char	pane_child_attachment;

	/* Initialize the child attachment to none */
    pane_child_attachment = XmPANE_CHILD_ATTACH_NONE;

	if (cp->pane_child_type == XmPANE_CHILD_ATTACHMENT_ONE)
	{
		if (!_XfeIsAlive(sp->attachment_one_top))
		{
			pane_child_attachment = XmPANE_CHILD_ATTACH_TOP;
		}
		else if (!_XfeIsAlive(sp->attachment_one_bottom))
		{
			pane_child_attachment = XmPANE_CHILD_ATTACH_BOTTOM;
		}
		else if (!_XfeIsAlive(sp->attachment_one_left))
		{
			pane_child_attachment = XmPANE_CHILD_ATTACH_LEFT;
		}
		else if (!_XfeIsAlive(sp->attachment_one_right))
		{
			pane_child_attachment = XmPANE_CHILD_ATTACH_RIGHT;
		}
	}
	else if (cp->pane_child_type == XmPANE_CHILD_ATTACHMENT_TWO)
	{
		if (!_XfeIsAlive(sp->attachment_two_top))
		{
			pane_child_attachment = XmPANE_CHILD_ATTACH_TOP;
		}
		else if (!_XfeIsAlive(sp->attachment_two_bottom))
		{
			pane_child_attachment = XmPANE_CHILD_ATTACH_BOTTOM;
		}
		else if (!_XfeIsAlive(sp->attachment_two_left))
		{
			pane_child_attachment = XmPANE_CHILD_ATTACH_LEFT;
		}
		else if (!_XfeIsAlive(sp->attachment_two_right))
		{
			pane_child_attachment = XmPANE_CHILD_ATTACH_RIGHT;
		}
	}

    value->addr = (XPointer) &pane_child_attachment;
    value->size = sizeof(pane_child_attachment);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList av,Cardinal * ac)
{
    XfePanePart *		sp = _XfePanePart(nw);

    /* Make sure rep types are ok */
 	XfeRepTypeCheck(nw,XmRShadowType,&sp->sash_shadow_type,XmSHADOW_OUT);

 	XfeRepTypeCheck(nw,XmRPaneSashType,&sp->pane_sash_type,
					XmPANE_SASH_DOUBLE_LINE);

 	XfeRepTypeCheck(nw,XmRPaneDragMode,&sp->pane_drag_mode,
					XmPANE_DRAG_PRESERVE_ONE);

    /* Allocate sash gc */
	sp->sash_GC = XfeAllocateSelectionGc(nw,1,sp->sash_color,
										 _XfeBackgroundPixel(nw));

    /* Initialize private members */
	sp->old_width			= _XfeWidth(nw);
	sp->old_height			= _XfeHeight(nw);
	sp->old_sash_position	= sp->sash_position;
	
    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfePaneWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfePanePart *		sp = _XfePanePart(w);

	XtReleaseGC(w,sp->sash_GC);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList av,Cardinal * ac)
{
    XfePanePart *		np = _XfePanePart(nw);
    XfePanePart *		op = _XfePanePart(ow);
	Boolean				update_pane_gc = False;

	/* sash_shadow_type */
	if (np->sash_shadow_type != op->sash_shadow_type)
	{
		_XfemConfigFlags(nw) |= XfeConfigExpose;

		/* Make sure the new sash_shadow_type is ok */
		XfeRepTypeCheck(nw,XmRShadowType,&np->sash_shadow_type,XmSHADOW_OUT);
	}

	/* pane_sash_type */
	if (np->pane_sash_type != op->pane_sash_type)
	{
		_XfemConfigFlags(nw) |= XfeConfigExpose;

		/* Make sure the new pane_sash_type is ok */
		XfeRepTypeCheck(nw,XmRPaneSashType,&np->pane_sash_type,
						XmPANE_SASH_DOUBLE_LINE);
	}

	/* pane_drag_mode */
	if (np->pane_drag_mode != op->pane_drag_mode)
	{
		_XfemConfigFlags(nw) |= XfeConfigExpose;

		/* Make sure the new pane_drag_mode is ok */
		XfeRepTypeCheck(nw,XmRPaneDragMode,&np->pane_drag_mode,
						XmPANE_DRAG_PRESERVE_ONE);
	}

	/* sash_shadow_thickness */
	if (np->sash_shadow_thickness != op->sash_shadow_thickness)
	{
		_XfemConfigFlags(nw) |= XfeConfigLED;
	}

	/* sash_offset, sash_spacing, sash_thickness, sash_position */
	if ( (np->sash_offset != op->sash_offset) ||
		 (np->sash_spacing != op->sash_spacing) ||
		 (np->sash_thickness != op->sash_thickness) )
	{
		_XfemConfigFlags(nw) |= XfeConfigLED;
	}

	/* sash_position */
	if (np->sash_position != op->sash_position)
	{
		np->old_sash_position = op->sash_position;

		_XfemConfigFlags(nw) |= XfeConfigLED;
	}
	
	/* sash_color */
	if (np->sash_color != op->sash_color)
	{
		_XfemConfigFlags(nw) |= XfeConfigLE;

		update_pane_gc = True;
	}

	/* Update the pane gc if needed */
	if (update_pane_gc)
	{
		XtReleaseGC(nw,np->sash_GC);

		np->sash_GC = XfeAllocateSelectionGc(nw,1,np->sash_color,
											 _XfeBackgroundPixel(nw));
	}

    return _XfeManagerChainSetValues(ow,rw,nw,xfePaneWidgetClass);
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
/*  	Widget					w = _XfeParent(nc); */
/*     XfePanePart *			sp = _XfePanePart(w); */
	XfePaneConstraintPart *	cp = _XfePaneConstraintPart(nc);

    /* Make sure the pane child type is ok */
    XfeRepTypeCheck(nc,XmRPaneChildType,&cp->pane_child_type,
					XmPANE_CHILD_WORK_AREA_ONE);

	/* Finish constraint initialization */
	_XfeConstraintChainInitialize(rc,nc,xfePaneWidgetClass);
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValues(Widget oc,Widget rc,Widget nc,ArgList av,Cardinal * ac)
{
	Widget					w = XtParent(nc);
/*     XfePanePart *			sp = _XfePanePart(w); */
 	XfePaneConstraintPart *	ncp = _XfePaneConstraintPart(nc);
 	XfePaneConstraintPart *	ocp = _XfePaneConstraintPart(oc);

	/* pane_child_type */
	if (ncp->pane_child_type != ocp->pane_child_type)
	{
		ncp->pane_child_type = ocp->pane_child_type;

		_XfeWarning(nc,MESSAGE2);
	}

	/* pane_minimum */
	if (ncp->pane_minimum != ocp->pane_minimum)
	{
		_XfemConfigFlags(w) |= XfeConfigLayout;
	}

	/* pane_maximum */
	if (ncp->pane_maximum != ocp->pane_maximum)
	{
		_XfemConfigFlags(w) |= XfeConfigLayout;
	}

	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(oc,rc,nc,xfePaneWidgetClass);
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
	XfePanePart *		sp = _XfePanePart(w);

 	Dimension			one_total_width = 0;
 	Dimension			one_total_height = 0;

 	Dimension			two_total_width = 0;
 	Dimension			two_total_height = 0;

	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);

	if (_XfeChildIsShown(sp->child_one))
	{
		one_total_width =+ _XfeWidth(sp->child_one);
		one_total_height =+ _XfeHeight(sp->child_one);
	}

	if (_XfeChildIsShown(sp->child_two))
	{
		two_total_width =+ _XfeWidth(sp->child_two);
		two_total_height =+ _XfeHeight(sp->child_two);
	}

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
 		*height += (one_total_height + sp->sash_thickness + two_total_height);
 		*width += XfeMax(one_total_width,two_total_width);
	}
	/* Horizontal */
	else
	{
 		*width += (one_total_width + sp->sash_thickness + two_total_width);
 		*height += XfeMax(one_total_height,two_total_height);
	}
}
/*----------------------------------------------------------------------*/
static void
MinimumGeometry(Widget w,Dimension * width,Dimension * height)
{
	/* XfePanePart *		sp = _XfePanePart(w); */

	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptChild(Widget child)
{
    Widget					w = _XfeParent(child);
    XfePanePart *			sp = _XfePanePart(w);
	XfePaneConstraintPart *	cp = _XfePaneConstraintPart(child);

#if 0
	printf("AcceptChild(%-10s , %-25s , %s)\n",
		   XtName(child),
		   XfeDebugRepTypeValueName(XmRPaneChildType,cp->pane_child_type),
		   XfeDebugRepTypeValueName(XmRPaneChildAttachment,cp->pane_child_attachment));
#endif

	/* Child one */
	if ((cp->pane_child_type == XmPANE_CHILD_WORK_AREA_ONE) && 
		!_XfeIsAlive(sp->child_one))
	{
		return True;
	}

	/* Child two */
	if ((cp->pane_child_type == XmPANE_CHILD_WORK_AREA_TWO) && 
		!_XfeIsAlive(sp->child_two))
	{
		return True;
	}

	/* Attachment one */
	if ((cp->pane_child_type == XmPANE_CHILD_ATTACHMENT_ONE) && 
		!ChildOneAttachmentsAreFull(w))
	{
		return True;
	}

	/* Attachment two */
	if ((cp->pane_child_type == XmPANE_CHILD_ATTACHMENT_TWO) && 
		!ChildTwoAttachmentsAreFull(w))
	{
		return True;
	}

	return False;
}
/*----------------------------------------------------------------------*/
static Boolean
InsertChild(Widget child)
{
    Widget					w = _XfeParent(child);
    XfePanePart *			sp = _XfePanePart(w);
	XfePaneConstraintPart *	cp = _XfePaneConstraintPart(child);

#if 0
	printf("InsertChild(%-10s , %-25s , %s)\n",
		   XtName(child),
		   XfeDebugRepTypeValueName(XmRPaneChildType,cp->pane_child_type),
		   XfeDebugRepTypeValueName(XmRPaneChildAttachment,cp->pane_child_attachment));
#endif

	/* Child one */
	if (cp->pane_child_type == XmPANE_CHILD_WORK_AREA_ONE)
	{
		sp->child_one = child;
	}
	/* Child two */
	else if (cp->pane_child_type == XmPANE_CHILD_WORK_AREA_TWO)
	{
		sp->child_two = child;

		if (sp->pane_drag_mode == XmPANE_DRAG_PRESERVE_TWO)
		{
			/* Vertical */
			if (_XfeOrientedOrientation(w) == XmVERTICAL)
			{
				
			}
			/* Horizontal */
			else
			{
			}
		}
	}
	/* Child one attachment */
	else if (cp->pane_child_type == XmPANE_CHILD_ATTACHMENT_ONE)
	{
		if (cp->pane_child_attachment == XmPANE_CHILD_ATTACH_BOTTOM)
		{
			sp->attachment_one_bottom = child;
		}
		else if (cp->pane_child_attachment == XmPANE_CHILD_ATTACH_TOP)
		{
			sp->attachment_one_top = child;
		}
		else if (cp->pane_child_attachment == XmPANE_CHILD_ATTACH_LEFT)
		{
			sp->attachment_one_left = child;
		}
		else if (cp->pane_child_attachment == XmPANE_CHILD_ATTACH_RIGHT)
		{
			sp->attachment_one_right = child;
		}
		else
		{
			assert( 0 );
		}
	}
	/* Child two attachment */
	else if (cp->pane_child_type == XmPANE_CHILD_ATTACHMENT_TWO)
	{
		if (cp->pane_child_attachment == XmPANE_CHILD_ATTACH_BOTTOM)
		{
			sp->attachment_two_bottom = child;
		}
		else if (cp->pane_child_attachment == XmPANE_CHILD_ATTACH_TOP)
		{
			sp->attachment_two_top = child;
		}
		else if (cp->pane_child_attachment == XmPANE_CHILD_ATTACH_LEFT)
		{
			sp->attachment_two_left = child;
		}
		else if (cp->pane_child_attachment == XmPANE_CHILD_ATTACH_RIGHT)
		{
			sp->attachment_two_right = child;
		}
		else
		{
			assert( 0 );
		}
	}

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteChild(Widget child)
{
    Widget				w = XtParent(child);
    XfePanePart *		sp = _XfePanePart(w);

	/* Child one */
	if (sp->child_one == child)
	{
		sp->child_one = NULL;
	}
	/* Child two */
	else if (sp->child_two == child)
	{
		sp->child_two = NULL;
	}
	/* Child one bottom attachment */
	else if (sp->attachment_one_bottom == child)
	{
		sp->attachment_one_bottom = NULL;
	}
	/* Child one top attachment */
	else if (sp->attachment_two_top == child)
	{
		sp->attachment_one_top = NULL;
	}
	/* Child one left attachment */
	else if (sp->attachment_one_left == child)
	{
		sp->attachment_one_left = NULL;
	}
	/* Child one right attachment */
	else if (sp->attachment_one_right == child)
	{
		sp->attachment_one_right = NULL;
	}
	/* Child two bottom attachment */
	else if (sp->attachment_two_bottom == child)
	{
		sp->attachment_two_bottom = NULL;
	}
	/* Child two top attachment */
	else if (sp->attachment_two_top == child)
	{
		sp->attachment_two_top = NULL;
	}
	/* Child two left attachment */
	else if (sp->attachment_two_left == child)
	{
		sp->attachment_two_left = NULL;
	}
	/* Child two right attachment */
	else if (sp->attachment_two_right == child)
	{
		sp->attachment_two_right = NULL;
	}

	return True;
}
/*----------------------------------------------------------------------*/
static void
PrepareComponents(Widget w,int flags)
{
    if (flags & _XFE_PREPARE_LABEL_STRING)
    {
    }
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
    XfePanePart *	sp = _XfePanePart(w);
	Position		sash_position = sp->sash_position;

	/* This is a hack and will be fixed later using MinimumGeometry() */
	int min_width = 4;
	int min_height = 4;

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		/* Determine if we need to tweak the sash position */
		if ( (_XfemOldHeight(w) != _XfeHeight(w)) &&
			 (_XfemOldHeight(w) > min_height) &&
			 (_XfeHeight(w) > min_height) &&
			 (sp->old_sash_position == sp->sash_position) )
		{
			
			if (sp->pane_drag_mode == XmPANE_DRAG_PRESERVE_ONE)
			{
			}
			else if (sp->pane_drag_mode == XmPANE_DRAG_PRESERVE_TWO)
			{
				int diff = _XfemOldHeight(w) - _XfeHeight(w);
				int two_height;

#if 0
				printf("LayoutComponents(%s) old = %d , new = %d, diff = %d\n",
					   XtName(w),
					   _XfemOldHeight(w),
					   _XfeHeight(w),
					   diff);
#endif				

/* 				printf("LayoutComponents(%s) diff = %d\n", */
/* 					   XtName(w),diff); */

				sash_position -= diff;

#if 0
				two_height = 
					_XfemOldHeight(w) -
					_XfemOffsetBottom(w) -
					(sp->sash_position + sp->sash_thickness);

				sash_position = 
					_XfeHeight(w) - 
					_XfemOffsetBottom(w) -
					two_height;
#endif

#if 0				
				printf("adjusting sash position from %d to %d\n",
					   sp->sash_position,sash_position);
#endif

#if 0
				int x;
				int new_pos;
				int two_height;

				printf("num children = %d\n",_XfemNumChildren(w));

				two_height = 
					sp->old_height -
					_XfemOffsetBottom(w) -
					(sp->old_sash_position + sp->sash_thickness);

				sash_position = 
					_XfeHeight(w) - 
					_XfemOffsetBottom(w) -
					_XfeHeight(sp->child_two);

				printf("changing sash position to %d from %d\n",
					   sash_position,sp->sash_position);
#endif					   
			}
			else if (sp->pane_drag_mode == XmPANE_DRAG_PRESERVE_RATIO)
			{
			}
		}
	}
	/* Horizontal */
	else
	{
		/* Determine if we need to tweak the sash position */
		if ((sp->old_width != _XfeWidth(w)) && 
			(sp->old_width >= 2) && 
			(_XfeWidth(w) >= 2) &&
			(sp->old_sash_position == sp->sash_position))
		{
			if (sp->pane_drag_mode == XmPANE_DRAG_PRESERVE_ONE)
			{
			}
			else if (sp->pane_drag_mode == XmPANE_DRAG_PRESERVE_TWO)
			{
			}
			else if (sp->pane_drag_mode == XmPANE_DRAG_PRESERVE_RATIO)
			{
			}
		}
	}

	/* Layout the sash */
	SashLayout(w,sash_position,&sp->sash_rect);

	sp->old_sash_position = sp->sash_position;
	sp->sash_position = sash_position;
}
/*----------------------------------------------------------------------*/
static void
LayoutChildren(Widget w)
{
    XfePanePart *		sp = _XfePanePart(w);
	Boolean				one_on = False;
	Boolean				two_on = False;

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		/* Child one */
		if (_XfeChildIsShown(sp->child_one))
		{
			ChildOneLayoutVertical(w);

			one_on = True;
		}

		/* Child two */
		if (_XfeChildIsShown(sp->child_two))
		{
			ChildTwoLayoutVertical(w);

			two_on = True;
		}
	}
	/* Horizontal */
	else
	{
		/* Child one */
		if (_XfeChildIsShown(sp->child_one))
		{
			ChildOneLayoutHorizontal(w);

			one_on = True;
		}
		
		/* Child two */
		if (_XfeChildIsShown(sp->child_two))
		{
			ChildTwoLayoutHorizontal(w);

			two_on = True;
		}
	}

	/* Both childs on - turn on all 8 attachments if needed */
	if (one_on && two_on)
	{
		AttachmentSetMappedAll(w,sp->child_one,True);
		AttachmentSetMappedAll(w,sp->child_two,True);
	}
	/* Both childs off - turn off all 8 attachments if needed */
	else if (!one_on && !two_on)
	{
		AttachmentSetMappedAll(w,sp->child_one,False);
		AttachmentSetMappedAll(w,sp->child_two,False);
	}
	/* Only one child on */
	else
	{
		Widget		aw;
		Boolean		should_expand;
		Widget		target;
		Widget		opposite;

		/* Child one on */
		if (one_on)
		{
			target		= sp->child_one;
			opposite	= sp->child_two;
		}
		/* Child two on */
		else
		{
			target		= sp->child_two;
			opposite	= sp->child_one;
		}

		aw				= AttachmentGetVisibleChild(w,opposite);
		should_expand	= ChildShouldExpand(w,target);

		/* Turn on all the attachments for the target child that is on */
		AttachmentSetMappedAll(w,target,True);
		
		if (should_expand && _XfeIsAlive(aw))
		{
			/* Turn on only the visible when off attachment.  All others off */
			AttachmentSetMappedChild(w,opposite,aw);
		}
		else
		{
			/* Turn off all attachments for the opposite child */
			AttachmentSetMappedAll(w,opposite,False);
		}
		
	}

    /* Update old dimensions */
	sp->old_width			= _XfeWidth(w);
	sp->old_height			= _XfeHeight(w);
	sp->old_sash_position	= sp->sash_position;
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfePanePart *	sp = _XfePanePart(w);

	/* Draw the sash */
	SashDrawShadow(w,&sp->sash_rect,False);
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
	/* Turn cursor on if needed */
	if (SashContainsXY(w,x,y))
	{
		_XfeOrientedSetCursorState(w,True);
	}
	/* Otherwise turn it off */
	else
	{
		_XfeOrientedSetCursorState(w,False);
	}
}
/*----------------------------------------------------------------------*/
static void
MotionProc(Widget w,Widget descendant,int x,int y)
{
	EnterProc(w,descendant,x,y);
}
/*----------------------------------------------------------------------*/
static void
DragStart(Widget w,Widget descendant,int x,int y)
{
	/* Make sure the coords are inside the sash */
	if (!SashContainsXY(w,x,y))
	{
		return;
	}

	SashDragStart(w,x,y,NULL);
}
/*----------------------------------------------------------------------*/
static void
DragEnd(Widget w,Widget descendant,int x,int y)
{
	SashDragEnd(w,x,y,NULL);
}
/*----------------------------------------------------------------------*/
static void
DragMotion(Widget w,Widget descendant,int x,int y)
{
	SashDragMotion(w,x,y,NULL);
}
/*----------------------------------------------------------------------*/
static void
DescendantDragStart(Widget w,Widget descendant,int x,int y)
{
	SashDragStart(w,x,y,DescendantFindAttachment(w,descendant));
}
/*----------------------------------------------------------------------*/
static void
DescendantDragEnd(Widget w,Widget descendant,int x,int y)
{
	SashDragEnd(w,x,y,DescendantFindAttachment(w,descendant));
}
/*----------------------------------------------------------------------*/
static void
DescendantDragMotion(Widget w,Widget descendant,int x,int y)
{
	SashDragMotion(w,x,y,DescendantFindAttachment(w,descendant));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Child functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean
ChildShouldExpand(Widget w,Widget child)
{
    XfePanePart *			sp = _XfePanePart(w);
	Boolean					should_expand = False;

	if (_XfeChildIsShown(child))
	{
		if (child == sp->child_one)
		{
			should_expand = (!_XfeChildIsShown(sp->child_two) && 
							 CONSTRAINT_ALLOW_EXPAND(sp->child_one));
		}
		else
		{
			should_expand = (!_XfeChildIsShown(sp->child_one) && 
							 CONSTRAINT_ALLOW_EXPAND(sp->child_two));
		}
	}

	return should_expand;
}
/*----------------------------------------------------------------------*/
static void
ChildLayout(Widget				child,
			Widget				top,
			Widget				bottom,
			Widget				left,
			Widget				right,
			XfeGeometry			pg)
{
    XfePanePart *			sp = _XfePanePart(XtParent(child));
	int	x;
	int	y;
	int	width;
	int	height;

	assert( _XfeChildIsShown(child) );
	assert( pg != NULL );

	x = pg->x;
	y = pg->y;
	width = pg->width;
	height = pg->height;
	
	/* Top attachment */
	if (_XfeChildIsShown(top))
	{
		_XfeConfigureOrHideWidget(top,
								  pg->x,
								  pg->y,
								  pg->width,
								  _XfeHeight(top));

		y += _XfeHeight(top);
		height -= _XfeHeight(top);
	}

	/* Bottom attachment */
	if (_XfeChildIsShown(bottom))
	{
		_XfeConfigureOrHideWidget(bottom,
								  pg->x,
								  pg->y + pg->height - _XfeHeight(bottom),
								  pg->width,
								  _XfeHeight(bottom));

		height -= _XfeHeight(bottom);
	}

	/* Left attachment */
	if (_XfeChildIsShown(left))
	{
		_XfeConfigureOrHideWidget(left,
								  pg->x,
								  y,
								  _XfeWidth(left),
								  height);
		
		x += _XfeWidth(left);
		width -= _XfeWidth(left);
	}

	/* Right attachment */
	if (_XfeChildIsShown(right))
	{
		_XfeConfigureOrHideWidget(right,
								  
								  pg->x + 
								  pg->width - 
								  _XfeWidth(right),
								  
								  y,
								  _XfeWidth(right),
								  height);
		
		width -= _XfeWidth(right);
	}

#ifdef DEBUG_ramiro
	/* Child */
	if (child == sp->child_two)
		printf("child_two(new y pos = %d\n",y);
#endif

	_XfeConfigureOrHideWidget(child,x,y,width,height);
}
/*----------------------------------------------------------------------*/
static void
ChildOneLayoutVertical(Widget w)
{
    XfePanePart *		sp = _XfePanePart(w);
	XfeGeometryRec		geom;

	geom.x		= _XfemRectX(w);
	geom.y		= _XfemRectY(w);
	geom.width	= _XfemRectWidth(w);

	/* Check whether the child can resize */
	if (CONSTRAINT_ALLOW_RESIZE(sp->child_one))
	{
		/* Check whether the child should expand */
		if (ChildShouldExpand(w,sp->child_one))
		{
			/* Look for a visible attachment */
			Widget aw = AttachmentGetVisibleChild(w,sp->child_two);
			
			/* Start by asuming that we can use the full extent of the pane */
			geom.height = _XfemRectHeight(w);
			
			/* Layout a visible attachment if needed */
			if (_XfeIsAlive(aw))
			{
				geom.height -= _XfeHeight(aw);
				
				AttachmentLayout(w,aw,&geom);
			}
		}
		else
		{
			geom.height = sp->sash_rect.y - _XfemRectY(w) - SASH_SPACING(sp);
		}	
	}
	else
	{
		geom.height = _XfeHeight(sp->child_one);
	}

	ChildLayout(sp->child_one,
				sp->attachment_one_top,
				sp->attachment_one_bottom,
				sp->attachment_one_left,
				sp->attachment_one_right,
				&geom);
}
/*----------------------------------------------------------------------*/
static void
ChildTwoLayoutVertical(Widget w)
{
    XfePanePart *		sp = _XfePanePart(w);
	XfeGeometryRec		geom;

	geom.x		= _XfemRectX(w);
	geom.width	= _XfemRectWidth(w);

	/* Check whether the child can resize */
	if (CONSTRAINT_ALLOW_RESIZE(sp->child_two))
	{
		if (ChildShouldExpand(w,sp->child_two))
		{
			Widget aw = AttachmentGetVisibleChild(w,sp->child_one);
			
			/* Start by asuming that we can use the full extent of the pane */
			geom.y		= _XfemRectY(w);
			geom.height = _XfemRectHeight(w);
			
			/* Look for a visible attachment */
			if (_XfeIsAlive(aw))
			{
				geom.height -= _XfeHeight(aw);
				
				geom.y		+= _XfeHeight(aw);
				
				AttachmentLayout(w,aw,&geom);
			}
		}
		else
		{
			geom.y		= 
				sp->sash_rect.y + 
				sp->sash_rect.height + 
				SASH_SPACING(sp);
			
			geom.height	= 
				_XfemRectY(w) + 
				_XfemRectHeight(w) - 
				geom.y -
				SASH_SPACING(sp);
		}	
	}
	else
	{
		geom.height = _XfeHeight(sp->child_two);
		geom.y		= _XfeHeight(w) - _XfemOffsetBottom(w) - geom.height;
	}

	ChildLayout(sp->child_two,
				sp->attachment_two_top,
				sp->attachment_two_bottom,
				sp->attachment_two_left,
				sp->attachment_two_right,
				&geom);
}
/*----------------------------------------------------------------------*/
static void
ChildOneLayoutHorizontal(Widget w)
{
    XfePanePart *		sp = _XfePanePart(w);
	XfeGeometryRec		geom;

	geom.x		= _XfemRectX(w);
	geom.y		= _XfemRectY(w);
	geom.height = _XfemRectHeight(w);

	if (ChildShouldExpand(w,sp->child_one))
	{
		Widget aw = AttachmentGetVisibleChild(w,sp->child_two);

		/* Start by asuming that we can use the full extent of the pane */
		geom.width = _XfemRectWidth(w);

		/* Look for a visible attachment */
		if (_XfeIsAlive(aw))
		{
			geom.width -= _XfeWidth(aw);

			AttachmentLayout(w,aw,&geom);
		}
	}
	else
	{
		geom.width = sp->sash_rect.x - _XfemRectX(w) - SASH_SPACING(sp);
	}	

	ChildLayout(sp->child_one,
				sp->attachment_one_top,
				sp->attachment_one_bottom,
				sp->attachment_one_left,
				sp->attachment_one_right,
				&geom);
}
/*----------------------------------------------------------------------*/
static void
ChildTwoLayoutHorizontal(Widget w)
{
    XfePanePart *		sp = _XfePanePart(w);
	XfeGeometryRec		geom;

	geom.y		= _XfemRectY(w);
	geom.height = _XfemRectHeight(w);

	if (ChildShouldExpand(w,sp->child_two))
	{
		Widget aw = AttachmentGetVisibleChild(w,sp->child_one);

		/* Start by asuming that we can use the full extent of the pane */
		geom.x		= _XfemRectX(w);
		geom.width	= _XfemRectWidth(w);

		/* Look for a visible attachment */
		if (_XfeIsAlive(aw))
		{
			geom.width -= _XfeWidth(aw);

			geom.x		+= _XfeWidth(aw);

			AttachmentLayout(w,aw,&geom);
		}
	}
	else
	{
		geom.x		= 
			sp->sash_rect.x + 
			sp->sash_rect.width +
			SASH_SPACING(sp);

		geom.width	= 
			_XfemRectX(w) + 
			_XfemRectWidth(w) - 
			geom.x -
			SASH_SPACING(sp);
	}	

	ChildLayout(sp->child_two,
				sp->attachment_two_top,
				sp->attachment_two_bottom,
				sp->attachment_two_left,
				sp->attachment_two_right,
				&geom);
}
/*----------------------------------------------------------------------*/
static Dimension
ChildMaxSize(Widget w,Widget child)
{
	Dimension pane_maximum = 0;

	if (_XfeIsAlive(child))
	{
		XfePaneConstraintPart * cp = _XfePaneConstraintPart(child);

		pane_maximum = cp->pane_maximum;
	}

	return pane_maximum;
}
/*----------------------------------------------------------------------*/
static Dimension
ChildMinSize(Widget w,Widget child)
{
	Dimension pane_minimum = 0;

	if (_XfeIsAlive(child))
	{
		XfePaneConstraintPart * cp = _XfePaneConstraintPart(child);

		pane_minimum = cp->pane_minimum;
	}

	return pane_minimum;
}
/*----------------------------------------------------------------------*/
static Boolean
ChildOneAttachmentsAreFull(Widget w)
{
    XfePanePart * sp = _XfePanePart(w);
	
	return (_XfeIsAlive(sp->attachment_one_bottom) &&
			_XfeIsAlive(sp->attachment_one_left) &&
			_XfeIsAlive(sp->attachment_one_right) &&
			_XfeIsAlive(sp->attachment_one_top));
}
/*----------------------------------------------------------------------*/
static Boolean
ChildTwoAttachmentsAreFull(Widget w)
{
    XfePanePart * sp = _XfePanePart(w);
	
	return (_XfeIsAlive(sp->attachment_two_bottom) &&
			_XfeIsAlive(sp->attachment_two_left) &&
			_XfeIsAlive(sp->attachment_two_right) &&
			_XfeIsAlive(sp->attachment_two_top));
}
/*----------------------------------------------------------------------*/
static Boolean
ChildIsAttachment(Widget w,Widget child)
{
    XfePanePart * sp = _XfePanePart(w);
	
	return (child == sp->attachment_one_bottom ||
			child == sp->attachment_one_top ||
			child == sp->attachment_one_left ||
			child == sp->attachment_one_right ||
			child == sp->attachment_two_bottom ||
			child == sp->attachment_two_top ||
			child == sp->attachment_two_left ||
			child == sp->attachment_two_right);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Descendant functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget
DescendantFindAttachment(Widget w,Widget descendant)
{
	Widget attachment = NULL;

	if (_XfeIsAlive(w) && _XfeIsAlive(descendant))
	{
		/* Try to find an attachment ancestor */
		attachment = XfeAncestorFindByFunction(descendant,
											   FindTestIsAttachment,
											   XfeFIND_ANY,
											   (XtPointer) w);
	}

	return attachment;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Attachment functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
AttachmentLayout(Widget w,Widget aw,XfeGeometry pg)
{
    XfePanePart *		sp = _XfePanePart(w);
	int					x;
	int					y;
	int					width;
	int					height;
	
	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		if ((aw == sp->attachment_one_top) || 
			(aw == sp->attachment_one_bottom))
		{
			x		= pg->x;
			y		= _XfemRectY(w);
			width	= pg->width;
			height	= _XfeHeight(aw);
		}
		else if ((aw == sp->attachment_two_top) || 
				 (aw == sp->attachment_two_bottom))
		{
			x		= pg->x;
			y		= _XfemRectY(w) + _XfemRectHeight(w) - _XfeHeight(aw);
			width	= pg->width;
			height	= _XfeHeight(aw);
		}
	}
	/* Horizontal */
	else
	{
		if ((aw == sp->attachment_one_left) || 
			(aw == sp->attachment_one_right))
		{
			x		= _XfemRectY(w);
			y		= pg->y;
			width	= _XfeWidth(aw);
			height	= pg->height;
		}
		else if ((aw == sp->attachment_two_left) || 
				 (aw == sp->attachment_two_right))
		{
			x		= _XfemRectX(w) + _XfemRectWidth(w) - _XfeWidth(aw);
			y		= pg->y;
			width	= _XfeWidth(aw);
			height	= pg->height;
		}
	}

	/* Attachment */
	_XfeConfigureWidget(aw,x,y,width,height);
}
/*----------------------------------------------------------------------*/
static void
AttachmentSetMappedChild(Widget w,Widget child,Widget aw)
{
	Widget			al[4];
	Cardinal		i;

	AttachmentLoadAll(w,child,al);

	for(i = 0; i < 4; i++)
	{
		_XfeSetMappedWhenManaged(al[i],al[i] == aw);
	}
}
/*----------------------------------------------------------------------*/
static void
AttachmentSetMappedAll(Widget w,Widget child,Boolean state)
{
	Widget			al[4];
	Cardinal		i;

	AttachmentLoadAll(w,child,al);
	
	for(i = 0; i < 4; i++)
	{
		_XfeSetMappedWhenManaged(al[i],state);
	}
}
/*----------------------------------------------------------------------*/
static void
AttachmentLoadAll(Widget w,Widget child,Widget * al)
{
    XfePanePart *		sp = _XfePanePart(w);

	assert( child == sp->child_one || child == sp->child_two );
	assert( al != NULL );
			
	/* Child one */
	if (child == sp->child_one)
	{
		/* Vertical */
		if (_XfeOrientedOrientation(w) == XmVERTICAL)
		{
			al[0] = sp->attachment_one_top;
			al[1] = sp->attachment_one_bottom;
			al[2] = sp->attachment_one_left;
			al[3] = sp->attachment_one_right;
		}
		/* Horizontal */
		else
		{
			al[0] = sp->attachment_one_left;
			al[1] = sp->attachment_one_right;
			al[2] = sp->attachment_one_top;
			al[3] = sp->attachment_one_bottom;
		}
	}
	/* Child two */
	else if (child == sp->child_two)
	{
		/* Vertical */
		if (_XfeOrientedOrientation(w) == XmVERTICAL)
		{
			al[0] = sp->attachment_two_top;
			al[1] = sp->attachment_two_bottom;
			al[2] = sp->attachment_two_left;
			al[3] = sp->attachment_two_right;
		}
		/* Horizontal */
		else
		{
			al[0] = sp->attachment_two_left;
			al[1] = sp->attachment_two_right;
			al[2] = sp->attachment_two_top;
			al[3] = sp->attachment_two_bottom;
		}
	}
}
/*----------------------------------------------------------------------*/
static Widget
AttachmentGetVisibleChild(Widget w,Widget child)
{
	Widget			al[4];
	Cardinal		i;

	AttachmentLoadAll(w,child,al);

	/*
	 * Look only in the first two - so that only top/bottom are caught for
	 * vertical and left/right for horizontal 
	 */
	for(i = 0; i < 4; i++)
	{
		if (ALWAYS_VISIBLE(al[i]))
		{
			return al[i];
		}
	}
			
	return NULL;
}
/*----------------------------------------------------------------------*/
static Boolean
AttachmentContainsXY(Widget aw,int x,int y)
{
	if (!_XfeChildIsShown(aw))
	{
		return False;
	}

	return ( (x >= _XfeX(aw)) &&
			 (x <= (_XfeX(aw) + _XfeWidth(aw))) &&
			 (y >= _XfeY(aw)) &&
			 (y <= (_XfeY(aw) + _XfeHeight(aw))) );
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Sash functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
SashLayout(Widget w,Position sash_position,XRectangle * sash_rect)
{
	XfePanePart *		sp = _XfePanePart(w);

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		sash_rect->x = _XfemRectX(w) + sp->sash_offset;
		sash_rect->y = _XfemRectY(w) + sash_position;  
		
		sash_rect->width = _XfemRectWidth(w) - 2 * sp->sash_offset;
		sash_rect->height = sp->sash_thickness;
	}
	/* Horizontal */
	else
	{
		sash_rect->x = _XfemRectX(w) + sash_position; 
		sash_rect->y = _XfemRectY(w) + sp->sash_offset; 

		sash_rect->width = sp->sash_thickness;
		sash_rect->height = _XfemRectHeight(w) - 2 * sp->sash_offset;
	}
}
/*----------------------------------------------------------------------*/
static void
SashDrawDragging(Widget w,XRectangle * sash_rect)
{
    XfePanePart *		sp = _XfePanePart(w);

	/* The line thickness needs to be at least one */
	Dimension line_thickness = XfeMax(sp->sash_shadow_thickness,1);
	
	switch(sp->pane_sash_type)
	{
	case XmPANE_SASH_DOUBLE_LINE:
		
		/* Vertical */
		if (_XfeOrientedOrientation(w) == XmVERTICAL)
		{
			XfeDrawHorizontalLine(XtDisplay(w),
								  _XfeWindow(w),
								  sp->sash_GC,
								  
								  sash_rect->x,
								  
								  sash_rect->y,
								  
								  sash_rect->width,
								  
								  line_thickness);

			XfeDrawHorizontalLine(XtDisplay(w),
								  _XfeWindow(w),
								  sp->sash_GC,

								  sash_rect->x,

								  sash_rect->y +
								  sash_rect->height -
								  line_thickness,
								  
								  sash_rect->width,
								  
								  line_thickness);
			
		}
		/* Horizontal */
		else
		{
			XfeDrawVerticalLine(XtDisplay(w),
								_XfeWindow(w),
								sp->sash_GC,
									
								sash_rect->x,
								
								sash_rect->y,
								
								sash_rect->height,

								line_thickness);
			
			XfeDrawVerticalLine(XtDisplay(w),
								_XfeWindow(w),
								sp->sash_GC,
								
								sash_rect->x +
								sash_rect->width -
								line_thickness,
								
								sash_rect->y,
								
								sash_rect->height,

								line_thickness);
		}
		
		break;	
		
	case XmPANE_SASH_RECTANGLE:
		
		XfeDrawRectangle(XtDisplay(w),
						 _XfeWindow(w),
						 sp->sash_GC,
						 sash_rect->x,
						 sash_rect->y,
						 sash_rect->width,
						 sash_rect->height,
						 sp->sash_shadow_thickness);
		
		break;
		
	case XmPANE_SASH_FILLED_RECTANGLE:
		
		XFillRectangle(XtDisplay(w),
					   _XfeWindow(w),
					   sp->sash_GC,
					   sash_rect->x,
					   sash_rect->y,
					   sash_rect->width,
					   sash_rect->height);
		
		break;
		
	case XmPANE_SASH_SINGLE_LINE:
		
		/* Vertical */
		if (_XfeOrientedOrientation(w) == XmVERTICAL)
		{
			XfeDrawHorizontalLine(XtDisplay(w),
								  _XfeWindow(w),
								  sp->sash_GC,
								  
								  sash_rect->x,
								  
								  sash_rect->y +
								  (sash_rect->height / 2) -
								  (line_thickness / 2),
								  
								  sash_rect->width,
								  
								  line_thickness);
		}
		/* Horizontal */
		else
		{
			XfeDrawVerticalLine(XtDisplay(w),
								_XfeWindow(w),
								sp->sash_GC,
								
								sash_rect->x +
								(sash_rect->width / 2) -
								(line_thickness / 2),
								
								sash_rect->y,
								
								sash_rect->height,
								
								line_thickness);
		}
		
		break;
		
		/* Nothing is needed for this one */
	case XmPANE_SASH_LIVE:
		break;
	}
}
/*----------------------------------------------------------------------*/
static void
SashDrawShadow(Widget w,XRectangle * sash_rect,Boolean clear_body)
{
    XfePanePart *	sp = _XfePanePart(w);

	/* Make sure the sash has dimensions > 0 */
	if (sash_rect->width && sash_rect->height)
 	{
		_XmDrawShadows(XtDisplay(w),
					   _XfeWindow(w),
					   _XfemTopShadowGC(w),
					   _XfemBottomShadowGC(w),
					   sash_rect->x,
					   sash_rect->y,
					   sash_rect->width,
					   sash_rect->height,
					   sp->sash_shadow_thickness,
					   sp->sash_shadow_type);

		
		/* Clear the sash body if needed */
 		if (clear_body &&
 			(sash_rect->width > (sp->sash_shadow_thickness * 2)) &&
 			(sash_rect->height > (sp->sash_shadow_thickness * 2)))
 		{
			XClearArea(XtDisplay(w),
					   _XfeWindow(w),
					   sash_rect->x + sp->sash_shadow_thickness,
					   sash_rect->y + sp->sash_shadow_thickness,
					   sash_rect->width - 2 * sp->sash_shadow_thickness,
					   sash_rect->height - 2 * sp->sash_shadow_thickness,
					   False);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
SashDragStart(Widget w,int x,int y,Widget aw)
{
	XfePanePart *		sp = _XfePanePart(w);

	/* Save the original sash position */
	sp->sash_original_position = sp->sash_position;

	/* If the attachment is valid, use it as the dragging rect */
	if (_XfeIsAlive(aw))
	{
		XfeRectSet(&sp->sash_dragging_rect,
				   _XfeX(aw),
				   _XfeY(aw),
				   _XfeWidth(aw),
				   _XfeHeight(aw));
	}
	/* Otherwise use the sash's rect */
	else
	{
		XfeRectCopy(&sp->sash_dragging_rect,&sp->sash_rect);
	}

	/* Assign the last dragging coordinate for the first time */
	sp->sash_dragging_last = CHOOSE_X_OR_Y(_XfeOrientedOrientation(w),x,y);

	/* Draw the first dragging sash if needed */
	if (sp->pane_sash_type != XmPANE_SASH_LIVE)
	{
		SashDrawDragging(w,&sp->sash_dragging_rect);
	}
}
/*----------------------------------------------------------------------*/
static void
SashDragEnd(Widget w,int x,int y,Widget aw)
{
	XfePanePart *		sp = _XfePanePart(w);
	Position			new_pos;

	/* Compute the new position */
	new_pos = CHOOSE_X_OR_Y(_XfeOrientedOrientation(w),

							sp->sash_original_position + 
							(x - sp->sash_dragging_last),

							sp->sash_original_position + 
							(y - sp->sash_dragging_last));

	/* Erase the previous sash if needed */
	if (sp->pane_sash_type != XmPANE_SASH_LIVE)
	{
		SashDrawDragging(w,&sp->sash_dragging_rect);
	}


			
	/* Update the sash position */
	sp->sash_position = new_pos;
	
	/* Layout the components */
	_XfeManagerLayoutComponents(w);
	
	/* Layout the children */
	_XfeManagerLayoutChildren(w);
	
	/* Draw components */
	SashDrawShadow(w,&sp->sash_rect,True);
}
/*----------------------------------------------------------------------*/
static void
SashDragMotion(Widget w,int x,int y,Widget aw)
{
	XfePanePart *		sp = _XfePanePart(w);
 	Position			new_pos;
 	Position			max_pos;
 	Position			min_pos;

	/* Compute the new position */
	new_pos = CHOOSE_X_OR_Y(_XfeOrientedOrientation(w),

							sp->sash_original_position + 
							(x - sp->sash_dragging_last),

							sp->sash_original_position + 
							(y - sp->sash_dragging_last));


	/* Make sure the diff in positions is greater than the drag threshold */
	if (XfeAbs(new_pos - sp->sash_position) < sp->drag_threshold)
	{
		return;
	}

#if 1
	max_pos = SashMaxPosition(w);
	min_pos = SashMinPosition(w);

	if (new_pos < min_pos)
	{
		new_pos = min_pos;
	}
	else if (new_pos > max_pos)
	{
		new_pos = max_pos;
	}
#endif

	/* Erase the previous sash if needed */
	if (sp->pane_sash_type != XmPANE_SASH_LIVE)
	{
		SashDrawDragging(w,&sp->sash_dragging_rect);
	}

	/* If the attachment is valid, use it as the new dragging rect */
	if (_XfeIsAlive(aw))
	{
		Position rect_x = _XfeX(aw);
		Position rect_y = _XfeY(aw);

		/* Vertical */
		if (_XfeOrientedOrientation(w) == XmVERTICAL)
		{
			rect_y += (new_pos - sp->sash_original_position);
		}
		/* Horizontal */
		else
		{
			rect_x += (new_pos - sp->sash_original_position);
		}

		XfeRectSet(&sp->sash_dragging_rect,
				   rect_x,
				   rect_y,
				   _XfeWidth(aw),
				   _XfeHeight(aw));
	}
	/* Otherwise use the new sash's rect */
	else
	{
		SashLayout(w,new_pos,&sp->sash_dragging_rect);
	}

	/* Draw the new sash if needed */
	if (sp->pane_sash_type != XmPANE_SASH_LIVE)
	{
		SashDrawDragging(w,&sp->sash_dragging_rect);
	}

	if (sp->pane_sash_type == XmPANE_SASH_LIVE)
	{
		/* Update the sash position */
		sp->sash_position = new_pos;
		
		/* Layout the components */
		_XfeManagerLayoutComponents(w);
		
		/* Layout the children */
		_XfeManagerLayoutChildren(w);
		
		/* Draw components */
		SashDrawShadow(w,&sp->sash_rect,True);
	}

/*   	printf("DragMotion(%s,%d,%d)\n",XtName(w),form_x,form_y); */
}
/*----------------------------------------------------------------------*/
static Position
SashMinPosition(Widget w)
{
	XfePanePart *		sp = _XfePanePart(w);
	Position			min_pos;
	Dimension			pane_minimum_one = ChildMinSize(w,sp->child_one);

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
/* 		printf("min height for 1 is %d\n",pane_minimum_one); */

		min_pos = _XfemOffsetTop(w);
	}
	/* Horizontal */
	else
	{
/*  		printf("min width for 1 is %d\n",pane_minimum_one);  */

		if (pane_minimum_one == 0)
		{
			min_pos = _XfemOffsetLeft(w);
		}
		else
		{
			min_pos = 
				_XfemOffsetLeft(w) + 
				pane_minimum_one;/*  + */
/* 				_XfemOffsetRight(w); */
		}
	}

	return min_pos;
}
/*----------------------------------------------------------------------*/
static Position
SashMaxPosition(Widget w)
{
	XfePanePart *		sp = _XfePanePart(w);
	Position			max_pos;

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		max_pos = 
			_XfeHeight(w) - 
			_XfemOffsetBottom(w) - 
			sp->sash_rect.height;
	}
	/* Horizontal */
	else
	{
		Dimension pane_minimum_two = ChildMinSize(w,sp->child_two);

		max_pos = 
			_XfemRectWidth(w) - 
			pane_minimum_two -
			sp->sash_rect.width;
	}

	return max_pos;
}
/*----------------------------------------------------------------------*/
static Position
SashMinPositionVertical(Widget w)
{
	XfePanePart *		sp = _XfePanePart(w);
	Position			min_pos;
	Dimension			pane_minimum_one = ChildMinSize(w,sp->child_one);
	
	min_pos = _XfemOffsetTop(w);

	return min_pos;
}
/*----------------------------------------------------------------------*/
static Position
SashMaxPositionX(Widget w)
{
	XfePanePart *		sp = _XfePanePart(w);
	Position			max_pos;

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		max_pos = 
			_XfeHeight(w) - 
			_XfemOffsetBottom(w) - 
			sp->sash_rect.height;
	}
	/* Horizontal */
	else
	{
		Dimension pane_minimum_two = ChildMinSize(w,sp->child_two);

		max_pos = 
			_XfemRectWidth(w) - 
			pane_minimum_two -
			sp->sash_rect.width;
	}

	return max_pos;
}
/*----------------------------------------------------------------------*/
static void
SashVerifyPosition(Widget		w,
				   Dimension	form_pref_width,
				   Dimension	form_pref_height)
{
 	XfePanePart *		sp = _XfePanePart(w);

	/* Setup the sash position if it still is the default */
	if (sp->sash_position == DEFAULT_SASH_POSITION)
	{
		Dimension	sash_thickness;
		Dimension	pane_thickness;

		/* Vertical */
		if (_XfeOrientedOrientation(w) == XmVERTICAL)
		{
			if (_XfeHeight(w) == XfeMANAGER_DEFAULT_HEIGHT)
			{
				pane_thickness = 0;
			}
			else
			{
				pane_thickness = _XfeHeight(w);
			}

			sash_thickness = form_pref_height + TWO_TIMES_SST(sp);
		}
		/* Horizontal */
		else
		{
			if (_XfeWidth(w) == XfeMANAGER_DEFAULT_WIDTH)
			{
				pane_thickness = 0;
			}
			else
			{
				pane_thickness = _XfeWidth(w);
			}

			sash_thickness = form_pref_width + TWO_TIMES_SST(sp);
		}

/* 		printf("%s: pane_thickness = %d\n",XtName(w),pane_thickness); */

		if (pane_thickness != 0)
		{
			sp->sash_position = (pane_thickness - sash_thickness) / 2;
		}
	}
}
/*----------------------------------------------------------------------*/
static Boolean
SashContainsXY(Widget w,int x,int y)
{
 	XfePanePart *		sp = _XfePanePart(w);

	return ( (x >= sp->sash_rect.x) &&
			 (x <= (sp->sash_rect.x + sp->sash_rect.width)) &&
			 (y >= sp->sash_rect.y) &&
			 (y <= (sp->sash_rect.y + sp->sash_rect.height)) );
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfePane functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean
HasChildOneAttachment(Widget w)
{
    XfePanePart * sp = _XfePanePart(w);
	
	return (_XfeIsAlive(sp->attachment_one_bottom) ||
			_XfeIsAlive(sp->attachment_one_left) ||
			_XfeIsAlive(sp->attachment_one_right) ||
			_XfeIsAlive(sp->attachment_one_top));
}
/*----------------------------------------------------------------------*/
static Boolean
HasChildTwoAttachment(Widget w)
{
    XfePanePart * sp = _XfePanePart(w);
	
	return (_XfeIsAlive(sp->attachment_two_bottom) ||
			_XfeIsAlive(sp->attachment_two_left) ||
			_XfeIsAlive(sp->attachment_two_right) ||
			_XfeIsAlive(sp->attachment_two_top));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* IsAttachment test function.											*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean
FindTestIsAttachment(Widget child,XtPointer data)
{
	Widget pane = (Widget) data;

	if (_XfeIsAlive(pane))
	{
		return ChildIsAttachment(pane,child);
	}

	return False;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePane Public Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreatePane(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfePaneWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
