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
/* Name:		<Xfe/ComboBox.c>										*/
/* Description:	XfeComboBox widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/ComboBoxP.h>
#include <Xfe/ListUtilP.h>

#include <Xfe/FrameShell.h>
#include <Xfe/BypassShell.h>

#include <Xm/TextF.h>
#include <Xm/List.h>

#include <Xfe/Button.h>
#include <Xfe/Arrow.h>

#define MESSAGE1 "Widget is not an XfeComboBox."
#define MESSAGE2 "XmNtitle is a read-only resource."
#define MESSAGE3 "XmNlist is a read-only resource."
#define MESSAGE4 "XmNshell is a read-only resource."
#define MESSAGE5 "XmNarrow is a read-only resource."
#define MESSAGE6 "XmNcomboBoxType is a creation-only resource."
#define MESSAGE7 "No valid XfeBypassShell found to share for XmNshell."

#define LIST_NAME		"ComboList"
#define SHELL_NAME		"ComboShell"
#define TITLE_NAME		"ComboTitle"
#define ARROW_NAME		"ComboArrow"

#define CB_OFFSET_BOTTOM(w,cp)	(cp->highlight_thickness+_XfemOffsetBottom(w))
#define CB_OFFSET_LEFT(w,cp)	(cp->highlight_thickness+_XfemOffsetLeft(w))
#define CB_OFFSET_RIGHT(w,cp)	(cp->highlight_thickness+_XfemOffsetRight(w))
#define CB_OFFSET_TOP(w,cp)		(cp->highlight_thickness+_XfemOffsetTop(w))

#define CB_RECT_X(w,cp)			(_XfemRectX(w) + cp->highlight_thickness)
#define CB_RECT_Y(w,cp)			(_XfemRectY(w) + cp->highlight_thickness)
#define CB_RECT_WIDTH(w,cp)		(_XfemRectWidth(w)-2*cp->highlight_thickness)
#define CB_RECT_HEIGHT(w,cp)	(_XfemRectHeight(w)-2*cp->highlight_thickness)

#define STICK_DELAY 200

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassPartInit		(WidgetClass);
static void 	Initialize			(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy				(Widget);
static Boolean	SetValues			(Widget,Widget,Widget,ArgList,Cardinal *);
static void		GetValuesHook		(Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static Boolean	AcceptChild			(Widget);
static void		LayoutComponents	(Widget);
static void		DrawShadow			(Widget,XEvent *,Region,XRectangle *);
static void		DrawComponents		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutTitle			(Widget);
static void		LayoutArrow			(Widget);
static void		DrawHighlight		(Widget,XEvent *,Region,XRectangle *);
static void		DrawTitleShadow		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	ActionPopdown		(Widget,XEvent *,char **,Cardinal *);
static void 	ActionPopup			(Widget,XEvent *,char **,Cardinal *);
static void 	ActionHighlight		(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeComboBox functions											*/
/*																		*/
/*----------------------------------------------------------------------*/

static Widget		TitleCreate			(Widget);
static void			TitleConfigure		(Widget);

static Widget		TitleTextCreate		(Widget);
static Widget		TitleLabelCreate	(Widget);

static Widget		ArrowCreate			(Widget);


/*----------------------------------------------------------------------*/
/*																		*/
/* List functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget		ListCreate			(Widget);
static void			ListLayout			(Widget);
static void			ListManage			(Widget);
static void			ListUnmanage		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Stick functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		StickTimeout		(XtPointer,XtIntervalId *);
static void		StickAddTimeout		(Widget);
static void		StickRemoveTimeout	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Shell functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget		ShellCreate			(Widget);
static void			ShellLayout			(Widget);
static void			ShellPopup			(Widget);
static void			ShellPopdown		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Screen functions functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static int			ScreenGetSpaceBelow	(Widget);
static int			ScreenGetSpaceAbove	(Widget);


/*----------------------------------------------------------------------*/
/*																		*/
/* Text callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			TextFocusCB			(Widget,XtPointer,XtPointer);
static void			TextLosingFocusCB	(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Arrow callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			ArrowActivateCB		(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Resource Callprocs													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			DefaultTitleShadowThickness	(Widget,int,XrmValue *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Synthetic resource Callprocs											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			SyntheticGetListItems		(Widget,int, XtArgVal *);
static void			SyntheticGetListItemCount	(Widget,int, XtArgVal *);

#if 0
/*
 * External definitions of syn_resources for our list widget.
 */
#define SYN_RESOURCE_AA AA((Widget w, int resource_offset, XtArgVal *value))
extern void _DtComboBoxGetArrowSize		SYN_RESOURCE_AA;
extern void _DtComboBoxGetLabelString		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListItemCount		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListItems		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListFontList		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListMarginHeight	SYN_RESOURCE_AA;
extern void _DtComboBoxGetListMarginWidth	SYN_RESOURCE_AA;
extern void _DtComboBoxGetListSpacing		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListTopItemPosition	SYN_RESOURCE_AA;
extern void _DtComboBoxGetListVisibleItemCount	SYN_RESOURCE_AA;
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Title resources */
	{
		XmNtitle,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . title),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNcomboBoxType,
		XmCComboBoxType,
		XmRComboBoxType,
		sizeof(unsigned char),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . combo_box_type),
		XmRImmediate, 
		(XtPointer) XmCOMBO_BOX_READ_ONLY
    },
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . spacing),
		XmRImmediate, 
		(XtPointer) 4
    },
    { 
		XmNtitleFontList,
		XmCTitleFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . title_font_list),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{
		XmNtitleShadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . title_shadow_thickness),
		XmRCallProc, 
		(XtPointer) DefaultTitleShadowThickness
	},
	{
		XmNtitleShadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . title_shadow_type),
		XmRImmediate, 
		(XtPointer) XmSHADOW_IN
	},

	/* List resources */
	{
		XmNlist,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . list),
		XmRImmediate, 
		(XtPointer) NULL
	},
	{
		XmNitems,
		XmCItems,
		XmRXmStringTable,
		sizeof(XmStringTable),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . items),
		XmRImmediate, 
		(XtPointer) NULL
	},
	{
		XmNitemCount,
		XmCItemCount,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . item_count),
		XmRImmediate, 
		(XtPointer) 0
	},
    { 
		XmNlistFontList,
		XmCListFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . list_font_list),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNlistMarginHeight,
		XmCListMarginHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . list_margin_height),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_COMBO_BOX_LIST_MARGIN_HEIGHT
    },
    { 
		XmNlistMarginWidth,
		XmCListMarginWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . list_margin_width),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_COMBO_BOX_LIST_MARGIN_WIDTH
    },
    { 
		XmNlistSpacing,
		XmCListSpacing,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . list_spacing),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_COMBO_BOX_LIST_SPACING
    },
	{
		XmNtopItemPosition,
		XmCTopItemPosition,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . top_item_position),
		XmRImmediate, 
		(XtPointer) 1
	},
	{
		XmNvisibleItemCount,
		XmCVisibleItemCount,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . visible_item_count),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_COMBO_BOX_LIST_VISIBLE_ITEM_COUNT
	},

	/* Shell resources */
	{
		XmNshell,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . shell),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNshareShell,
		XmCShareShell,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . share_shell),
		XmRImmediate, 
		(XtPointer) True
    },
    { 
		XmNpoppedUp,
		XmCReadOnly,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . popped_up),
		XmRImmediate, 
		(XtPointer) False
    },


	/* Arrow resources */
	{
		XmNarrow,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . arrow),
		XmRImmediate, 
		(XtPointer) NULL
	},

	/* Traversal resources */
	{
		XmNhighlightThickness,
		XmCHighlightThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . highlight_thickness),
		XmRImmediate, 
		(XtPointer) 2
	},
    { 
		XmNtraversalOn,
		XmCTraversalOn,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . traversal_on),
		XmRImmediate, 
		(XtPointer) True
    },

    /* Force the XmNshadowThickness to the default */
    { 
		XmNshadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , manager . shadow_thickness),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_SHADOW_THICKNESS
    },

	/* Force XmNmarginLeft and XmNmarginRight to 4 */
    { 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_manager . margin_left),
		XmRImmediate, 
		(XtPointer) 4
    },
    { 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_manager . margin_right),
		XmRImmediate, 
		(XtPointer) 4
    },

};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox synthetic resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
	{ 
		XmNspacing,
		sizeof(Dimension),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
	{ 
		XmNitems,
		sizeof(XmStringTable),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . items),
		SyntheticGetListItems,
		_XfeSyntheticSetResourceForChild 
	},
	{ 
		XmNitemCount,
		sizeof(int),
		XtOffsetOf(XfeComboBoxRec , xfe_combo_box . item_count),
		SyntheticGetListItemCount,
		_XfeSyntheticSetResourceForChild 
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "Popup",						ActionPopup				},
    { "Popdown",					ActionPopdown			},
    { "Highlight",					ActionHighlight			},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(combobox,ComboBox) =
{
    {
		(WidgetClass) &xfeManagerClassRec,		/* superclass			*/
		"XfeComboBox",							/* class_name			*/
		sizeof(XfeComboBoxRec),					/* widget_size			*/
		NULL,									/* class_initialize		*/
		ClassPartInit,							/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		actions,								/* actions            	*/
		XtNumber(actions),						/* num_actions        	*/
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
		GetValuesHook,							/* get_values_hook		*/
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
		NULL,									/* syn resources		*/
		0,										/* num syn_resources	*/
		sizeof(XfeManagerConstraintRec),		/* constraint size		*/
		NULL,									/* init proc			*/
		NULL,									/* destroy proc			*/
		NULL,									/* set values proc		*/
		NULL,                                   /* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		synthetic_resources,					/* syn resources		*/
		XtNumber(synthetic_resources),			/* num syn_resources	*/
		NULL,                                   /* syn_cont_resources  	*/
		0,                                      /* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process		*/
		NULL,                                   /* extension			*/
    },
    
    /* XfeManager Part 	*/
    {
		XfeInheritBitGravity,					/* bit_gravity			*/
		PreferredGeometry,						/* preferred_geometry	*/
		XfeInheritMinimumGeometry,				/* minimum_geometry		*/
		XfeInheritUpdateRect,					/* update_rect			*/
		AcceptChild,							/* accept_child			*/
		NULL,									/* insert_child			*/
		NULL,									/* delete_child			*/
		NULL,									/* change_managed		*/
		NULL,									/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		NULL,									/* layout_children		*/
		NULL,									/* draw_background		*/
		DrawShadow,								/* draw_shadow			*/
		DrawComponents,							/* draw_components		*/
		False,									/* count_layable_children*/
		NULL,									/* child_is_layable		*/
		NULL,									/* extension			*/
    },

    /* XfeComboBox Part */
    {
		LayoutTitle,							/* layout_title			*/
		LayoutArrow,							/* layout_arrow			*/
		DrawHighlight,							/* draw_highlight		*/
		DrawTitleShadow,						/* draw_title_shadow	*/
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeComboBoxWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(combobox,ComboBox);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox resource call procedures									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DefaultTitleShadowThickness(Widget w,int offset,XrmValue * value)
{
	XfeComboBoxPart *		cp = _XfeComboBoxPart(w);
    static Dimension		shadow_thickness;

	if (cp->combo_box_type == XmCOMBO_BOX_EDITABLE)
	{
		shadow_thickness = _XfemShadowThickness(w);
	}
	else
	{
		shadow_thickness = 0;
	}

    value->addr = (XPointer) &shadow_thickness;
    value->size = sizeof(shadow_thickness);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Synthetic resource Callprocs											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
SyntheticGetListItems(Widget w,int offset, XtArgVal * value)
{
}
/*----------------------------------------------------------------------*/
static void
SyntheticGetListItemCount(Widget w,int offset, XtArgVal * value)
{
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassPartInit(WidgetClass wc)
{
    XfeComboBoxWidgetClass cc = (XfeComboBoxWidgetClass) wc;
    XfeComboBoxWidgetClass sc = (XfeComboBoxWidgetClass) wc->core_class.superclass;

    _XfeResolve(cc,sc,xfe_combo_box_class,layout_title,
				XfeInheritLayoutTitle);

    _XfeResolve(cc,sc,xfe_combo_box_class,layout_arrow,
				XfeInheritLayoutArrow);

    _XfeResolve(cc,sc,xfe_combo_box_class,draw_highlight,
				XfeInheritDrawHighlight);
   
    _XfeResolve(cc,sc,xfe_combo_box_class,draw_title_shadow,
				XfeInheritDrawTitleShadow);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeComboBoxPart *		cp = _XfeComboBoxPart(nw);

    /* Make sure rep types are ok */
	XfeRepTypeCheck(nw,XmRShadowType,&cp->title_shadow_type,
					XmSHADOW_IN);

	XfeRepTypeCheck(nw,XmRComboBoxType,&cp->combo_box_type,
					XmCOMBO_BOX_READ_ONLY);

	/* Make sure read-only resources aren't set */
	if (cp->title)
	{
		_XmWarning(nw,MESSAGE2);

		cp->title = NULL;
	}

	if (cp->list)
	{
		_XmWarning(nw,MESSAGE3);

		cp->list = NULL;
	}

	if (cp->shell)
	{
		_XmWarning(nw,MESSAGE4);

		cp->shell = NULL;
	}

	if (cp->arrow)
	{
		_XmWarning(nw,MESSAGE5);

		cp->arrow = NULL;
	}
	
    /* Create components */
	cp->arrow		= ArrowCreate(nw);
	cp->title		= TitleCreate(nw);
	cp->shell		= ShellCreate(nw);
	cp->list		= ListCreate(nw);

	/* Configure the title */
	TitleConfigure(nw);

    /* Initialize private members */
	cp->highlighted			= False;
	cp->delay_timer_id		= 0;

	/* Manage the children */
	XtManageChild(cp->title);
	XtManageChild(cp->list);
	XtManageChild(cp->arrow);

 	XfeOverrideTranslations(nw,_XfeComboBoxExtraTranslations);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeComboBoxWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfeComboBoxPart *		cp = _XfeComboBoxPart(w); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeComboBoxPart *		np = _XfeComboBoxPart(nw);
    XfeComboBoxPart *		op = _XfeComboBoxPart(ow);

	/* title */
	if (np->title != op->title)
	{
		_XmWarning(nw,MESSAGE2);

		np->title = op->title;
	}

	/* list */
	if (np->list != op->list)
	{
		_XmWarning(nw,MESSAGE3);

		np->list = op->list;
	}

	/* shell */
	if (np->shell != op->shell)
	{
		_XmWarning(nw,MESSAGE4);

		np->shell = op->shell;
	}

	/* combo_box_type */
	if (np->combo_box_type != op->combo_box_type)
	{
		TitleConfigure(nw);

		_XfemConfigFlags(nw) |= XfeConfigExpose;

#if 0
		_XmWarning(nw,MESSAGE6);

		np->combo_box_type = op->combo_box_type;
#endif
	}

	/* title_shadow_thickness */
	if (np->title_shadow_thickness != op->title_shadow_thickness)
	{
		_XfemConfigFlags(nw) |= XfeConfigGLE;
	}

	/* title_shadow_type */
	if (np->title_shadow_type != op->title_shadow_type)
	{
		/* Make sure the new shadow type is ok */
		XfeRepTypeCheck(nw,XmRShadowType,&np->title_shadow_type,XmSHADOW_IN);

		_XfemConfigFlags(nw) |= XfeConfigExpose;
	}

    return _XfeManagerChainSetValues(ow,rw,nw,xfeComboBoxWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
/*     XfeComboBoxPart *		cp = _XfeComboBoxPart(w); */
    Cardinal				i;
    
    for (i = 0; i < *nargs; i++)
    {
#if 0
		/* label_string */
		if (strcmp(args[i].name,XmNlabelString) == 0)
		{
			*((XtArgVal *) args[i].value) = 
				(XtArgVal) XmStringCopy(lp->label_string);
		}
		/* font_list */
		else if (strcmp(args[i].name,XmNfontList) == 0)
		{
			*((XtArgVal *) args[i].value) = 
				(XtArgVal) XmFontListCopy(lp->font_list);
		}      
#endif
    }
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
    XfeComboBoxPart *		cp = _XfeComboBoxPart(w);
	
	*height = 
		CB_OFFSET_TOP(w,cp)  + CB_OFFSET_BOTTOM(w,cp) +
		XfeMax(_XfeHeight(cp->title),_XfeHeight(cp->arrow)) +
		2 * cp->title_shadow_thickness;

	*width  = 
		CB_OFFSET_LEFT(w,cp) + CB_OFFSET_RIGHT(w,cp) + 
		_XfeWidth(cp->title) +
		cp->spacing +
		_XfeWidth(cp->arrow) +
		2 * cp->title_shadow_thickness;
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptChild(Widget child)
{
	return False;
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
	_XfeComboBoxLayoutArrow(w);

	_XfeComboBoxLayoutTitle(w);
}
/*----------------------------------------------------------------------*/
static void
DrawShadow(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	if (!_XfemShadowThickness(w))
 	{
 		return;
 	}

    /* Draw the shadow */
    _XmDrawShadows(XtDisplay(w),
				   _XfeWindow(w),
				   _XfemTopShadowGC(w),_XfemBottomShadowGC(w),
				   cp->highlight_thickness,
				   cp->highlight_thickness,
				   _XfeWidth(w) - 2 * cp->highlight_thickness,
				   _XfeHeight(w) - 2 * cp->highlight_thickness,
				   _XfemShadowThickness(w),
				   _XfemShadowType(w));
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
	/* Draw the highlight */
	_XfeComboBoxDrawHighlight(w,event,region,clip_rect);

	/* Draw the title shadow */
	_XfeComboBoxDrawTitleShadow(w,event,region,clip_rect);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ActionPopup(Widget item,XEvent * event,char ** params,Cardinal * nparams)
{
	Widget				w = XfeIsComboBox(item) ? item : _XfeParent(item);
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);
	int					space_below = ScreenGetSpaceBelow(w);
	int					space_above = ScreenGetSpaceAbove(w);

#if 1
	printf("ActionPopup(%s,above = %d,below = %d)\n",
		   XtName(w),
		   space_above,
		   space_below);
#endif

	/* Check if we are already popped up */
	if (cp->popped_up)
	{
		printf("already popped up\n");

		StickRemoveTimeout(w);

		ListUnmanage(w);
		
		XtVaSetValues(cp->arrow,XmNarmed,False,NULL);

		return;
	}

	space_below = 200;

  	_XfeConfigureWidget(cp->shell,
						XfeRootX(w),
						XfeRootY(w) + _XfeHeight(w),
						_XfeWidth(w),
						space_below);

/*   	XtVaSetValues(cp->list, */
/* 				  XmNlistMarginWidth, 100, */
/* 				  NULL); */

/* 	XtVaSetValues(cp->arrow,XmCArmed,True,NULL); */

/* 	XtPopup(cp->shell,XtGrabNone); */
/* 	XMapRaised(XtDisplay(w),_XfeWindow(cp->shell)); */
/* 	XMapRaised(XtDisplay(w),_XfeWindow(cp->shell)); */

 	cp->remain_popped_up = False;

#if 1
	StickAddTimeout(w);
#endif

 	XtVaSetValues(cp->arrow,XmNarmed,True,NULL);

	ListManage(w);
}
/*----------------------------------------------------------------------*/
static void
ActionPopdown(Widget item,XEvent * event,char ** params,Cardinal * nparams)
{
	Widget				w = XfeIsComboBox(item) ? item : _XfeParent(item);
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

#if 1
	printf("ActionPopdown(%s)\n",XtName(w));
#endif

	if (cp->delay_timer_id)
	{
		StickRemoveTimeout(w);
	}
	else
	{
		ListUnmanage(w);

	}
	
	XtVaSetValues(cp->arrow,XmNarmed,False,NULL);

/* 	if (!cp->remain_popped_up) */
/* 	{ */
/* 		XtPopdown(cp->shell); */
/* 	} */
	
/*   	cp->remain_popped_up = False; */

/*  	ListUnmanage(w); */

/* 	XUnmapWindow(XtDisplay(w),_XfeWindow(cp->shell)); */
}
/*----------------------------------------------------------------------*/
static void
ActionHighlight(Widget item,XEvent * event,char ** params,Cardinal * nparams)
{
	Widget w = XfeIsComboBox(item) ? item : _XfeParent(item);
	
	XmProcessTraversal(w,XmTRAVERSE_CURRENT);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeComboBox functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DrawHighlight(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	/* Make sure the highlight is needed */
	if (!cp->highlight_thickness || !_XfeIsRealized(w))
	{
		return;
	}

	if (cp->highlighted)
	{
		_XmDrawSimpleHighlight(XtDisplay(w),
							   _XfeWindow(w), 
							   _XfemHighlightGC(w),
							   0,0, 
							   _XfeWidth(w),_XfeHeight(w),
							   cp->highlight_thickness);
	}
	else
	{
		assert( XmIsManager(_XfeParent(w)) );

		_XmDrawSimpleHighlight(XtDisplay(w),
							   _XfeWindow(w), 
							   _XfemBackgroundGC(_XfeParent(w)),
							   0,0, 
							   _XfeWidth(w),_XfeHeight(w),
							   cp->highlight_thickness);
	}
}
/*----------------------------------------------------------------------*/
static void
DrawTitleShadow(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	/* Make sure the shadow is needed */
	if (!cp->title_shadow_thickness)
	{
		return;
	}

	/* Draw the shadow around the text only */
	XfeDrawShadowsAroundWidget(w,
							   cp->title,
							   _XfemTopShadowGC(w),
							   _XfemBottomShadowGC(w),
							   0,
							   cp->title_shadow_thickness,
							   cp->title_shadow_type);
}
/*----------------------------------------------------------------------*/
static Widget
TitleCreate(Widget w)
{
	XfeComboBoxPart *	cp  = _XfeComboBoxPart(w);
	Widget				title = NULL;

	if (cp->combo_box_type == XmCOMBO_BOX_EDITABLE)
	{
		title = TitleTextCreate(w);
	}
	else
	{
		title = TitleLabelCreate(w);
	}

	return title;
}
/*----------------------------------------------------------------------*/
static void
TitleConfigure(Widget w)
{
    XfeComboBoxPart *	cp  = _XfeComboBoxPart(w);
	Arg					av[10];
	Cardinal			ac = 0;

	assert( _XfeIsAlive(cp->title) );

	if (cp->combo_box_type == XmCOMBO_BOX_EDITABLE)
	{
		XfeOverrideTranslations(cp->title,
								_XfeComboBoxTextEditableTranslations);

		XtSetArg(av[ac],XmNeditable,True); ac++;
		XtSetArg(av[ac],XmNcursorPositionVisible,True); ac++;
	}
	else
	{
		XfeOverrideTranslations(cp->title,
								_XfeComboBoxTextReadOnlyTranslations);

		XtSetArg(av[ac],XmNeditable,False); ac++;
		XtSetArg(av[ac],XmNcursorPositionVisible,False); ac++;
	}

	XtSetValues(cp->title,av,ac);
}
/*----------------------------------------------------------------------*/
static Widget
TitleTextCreate(Widget w)
{
/*     XfeComboBoxPart *	cp  = _XfeComboBoxPart(w); */
	Widget				text = NULL;

	text = XtVaCreateWidget(TITLE_NAME,
							xmTextFieldWidgetClass,
							w,
							XmNbackground,			_XfeBackgroundPixel(w),
							XmNbackgroundPixmap,	_XfeBackgroundPixmap(w),
							XmNhighlightThickness,	0,
							XmNshadowThickness,		0,
							NULL);


	XtAddCallback(text,XmNfocusCallback,TextFocusCB,w);
	XtAddCallback(text,XmNlosingFocusCallback,TextLosingFocusCB,w);

	return text;
}
/*----------------------------------------------------------------------*/
static Widget
TitleLabelCreate(Widget w)
{
/*     XfeComboBoxPart *	cp  = _XfeComboBoxPart(w); */
	Widget				label = NULL;

	label = XtVaCreateWidget(TITLE_NAME,
							 xfeLabelWidgetClass,
							 w,
							 XmNbackground,			_XfeBackgroundPixel(w),
							 XmNbackgroundPixmap,	_XfeBackgroundPixmap(w),
							 XmNshadowThickness,		0,
							 XmNlabelAlignment,		XmALIGNMENT_BEGINNING,
							 NULL);

	/* Add the extra combo box translations to the label */
/* 	XfeOverrideTranslations(label,_XfeComboBoxExtraTranslations); */

	return label;
}
/*----------------------------------------------------------------------*/
static void
LayoutTitle(Widget w)
{
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	_XfeConfigureWidget(cp->title,

						CB_OFFSET_LEFT(w,cp) + cp->title_shadow_thickness,

						(_XfeHeight(w) - _XfeHeight(cp->title)) / 2,
						
						CB_RECT_WIDTH(w,cp) - 
						cp->spacing -
						_XfeWidth(cp->arrow) -
						2 * cp->title_shadow_thickness,
						
						CB_RECT_HEIGHT(w,cp) - 
						2 * cp->title_shadow_thickness);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* List functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget
ListCreate(Widget w)
{
    XfeComboBoxPart *	cp  = _XfeComboBoxPart(w);
	Widget				list = NULL;
	Arg					av[10];
	Cardinal			ac = 0;

	assert( _XfeIsAlive(cp->shell) );

	XtSetArg(av[ac],XmNbackground,		_XfeBackgroundPixel(w));	ac++;
	XtSetArg(av[ac],XmNforeground,		_XfemForeground(w));		ac++;
	XtSetArg(av[ac],XmNshadowThickness,	_XfemShadowThickness(w));	ac++;

	list = XmCreateScrolledList(cp->shell,LIST_NAME,av,ac);

  	XtManageChild(list);

	XtUnmanageChild(_XfeParent(list));

/* 	_XfeResizeWidget(list,400,500); */
/* 	_XfeResizeWidget(cp->shell,400,500); */

	return list;
}
/*----------------------------------------------------------------------*/
static void
ListManage(Widget w)
{
    XfeComboBoxPart *	cp  = _XfeComboBoxPart(w);

	assert( _XfeIsAlive(cp->shell) );
	assert( _XfeIsAlive(cp->list) );

	printf("ListManage(%s)\n",XtName(w));

/* 	cp->remain_popped_up = True; */

	/* popped up */
 	cp->popped_up = True;

	/* Manage the scrolled window */
	XtManageChild(_XfeParent(cp->list));

	ShellPopup(w);

#if 0
	XmUpdateDisplay(w);
	XFlush(XtDisplay(w));
#endif

/* 	_XmPopupSpringLoaded(w); */
}
/*----------------------------------------------------------------------*/
static void
ListUnmanage(Widget w)
{
    XfeComboBoxPart *	cp  = _XfeComboBoxPart(w);

	assert( _XfeIsAlive(cp->shell) );
	assert( _XfeIsAlive(cp->list) );

	/* not popped up */
 	cp->popped_up = False;

	/* Unmanage the scrolled window */
	XtUnmanageChild(_XfeParent(cp->list));

	ShellPopdown(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Shell functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget
ShellCreate(Widget w)
{
	Widget		shell = NULL;
	Widget		frame_shell;

	/* Look for a XfeFrameShell ancestor */
	frame_shell = XfeAncestorFindByClass(w,
										 xfeFrameShellWidgetClass,
										 XfeFIND_ANY);

	/* If found, use it */
	if (_XfeIsAlive(frame_shell))
	{
		shell = XfeFrameShellGetBypassShell(frame_shell);
	}
	else
	{
		shell = XfeCreateBypassShell(w,SHELL_NAME,NULL,0);

		XtRealizeWidget(shell);
	}

	assert( _XfeIsAlive(shell) );


	if (!_XfeIsRealized(shell))
	{
		XtRealizeWidget(shell);
	}

	return shell;
}
/*----------------------------------------------------------------------*/
static void
ShellPopup(Widget w)
{
	XfeComboBoxPart *	cp  = _XfeComboBoxPart(w);

	assert( _XfeIsAlive(cp->shell) );

/* 	_XmPopupSpringLoaded(cp->shell); */
	_XmPopup(cp->shell,XtGrabNone);
}
/*----------------------------------------------------------------------*/
static void
ShellPopdown(Widget w)
{
	XfeComboBoxPart *	cp  = _XfeComboBoxPart(w);

	assert( _XfeIsAlive(cp->shell) );

	_XmPopdown(cp->shell);
}
/*----------------------------------------------------------------------*/
static Widget
ArrowCreate(Widget w)
{
/*     XfeComboBoxPart *	cp  = _XfeComboBoxPart(w); */
	Widget				arrow = NULL;

	arrow = XtVaCreateWidget(ARROW_NAME,
							 xfeArrowWidgetClass,
							 w,
							 XmNbackground,			_XfeBackgroundPixel(w),
							 XmNbackgroundPixmap,	_XfeBackgroundPixmap(w),
 							 XmNshadowThickness,		1,
/*  							 XmNemulateMotif,			False, */
 							 XmNraiseBorderThickness,	0,
							 XmNraiseOnEnter,			False,
 							 XmNfillOnEnter,			False,
 							 XmNshadowThickness,		0,

							 XmNbuttonType,				XmBUTTON_TOGGLE,

							 NULL);


	XfeOverrideTranslations(arrow,_XfeComboBoxArrowTranslations);

#if 0
	XtAddCallback(arrow,XmNarmCallback,ArrowArmCB,w);
	XtAddCallback(arrow,XmNdisarmCallback,ArrowDisarmCB,w);
	XtAddCallback(arrow,XmNactivateCallback,ArrowActivateCB,w);
#endif

/* 	XtAddCallback(arrow,XmNactivateCallback,ArrowActivateCB,w); */

	return arrow;
}
/*----------------------------------------------------------------------*/
static void
LayoutArrow(Widget w)
{
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	_XfeConfigureWidget(cp->arrow,

						_XfeWidth(w) - 
						CB_OFFSET_RIGHT(w,cp) - 
						_XfeWidth(cp->arrow),

						(_XfeHeight(w) - _XfeHeight(cp->arrow)) / 2,

						_XfeWidth(cp->arrow),

						_XfeHeight(cp->arrow));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Text callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
TextFocusCB(Widget text,XtPointer client_data,XtPointer call_data)
{
	Widget					w = (Widget) client_data;
    XfeComboBoxPart *		cp = _XfeComboBoxPart(w);

	cp->highlighted = True;

	DrawHighlight(w,NULL,NULL,&_XfemWidgetRect(w));
}
/*----------------------------------------------------------------------*/
static void
TextLosingFocusCB(Widget text,XtPointer client_data,XtPointer call_data)
{
	Widget					w = (Widget) client_data;
    XfeComboBoxPart *		cp = _XfeComboBoxPart(w);

	cp->highlighted = False;

	DrawHighlight(w,NULL,NULL,&_XfemWidgetRect(w));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Arrow callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ArrowActivateCB(Widget text,XtPointer client_data,XtPointer call_data)
{
	Widget					w = (Widget) client_data;
	XfeComboBoxPart *		cp = _XfeComboBoxPart(w);

#if 0
	printf("ArrowActivate(%s)\n",XtName(w));
#endif

/* 	cp->remain_popped_up = True; */

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc Arrow functions													*/
/*																		*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Screen functions functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static int
ScreenGetSpaceBelow(Widget w)
{
	return (int) XfeScreenHeight(w) - (int) (XfeRootY(w) + _XfeHeight(w));
}
/*----------------------------------------------------------------------*/
static int
ScreenGetSpaceAbove(Widget w)
{
	return (int) XfeRootY(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Stick functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
StickTimeout(XtPointer client_data,XtIntervalId * id)
{
	Widget					w = (Widget) client_data;
	XfeComboBoxPart *		cp = _XfeComboBoxPart(w);

	cp->delay_timer_id = 0;

	cp->remain_popped_up = True;

#if 1
 	printf("StickTimeout(%s)\n",XtName(w));
#endif
}
/*----------------------------------------------------------------------*/
static void
StickAddTimeout(Widget w)
{
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	cp->delay_timer_id = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
										 STICK_DELAY,
										 StickTimeout,
										 (XtPointer) w);
}
/*----------------------------------------------------------------------*/
static void
StickRemoveTimeout(Widget w)
{
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	/* Remove the timer if its still has not triggered */
	if (cp->delay_timer_id)
	{
		XtRemoveTimeOut(cp->delay_timer_id);
	}

	/* Reset the timer */
	cp->delay_timer_id = 0;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeComboBoxLayoutTitle(Widget w)
{
	XfeComboBoxWidgetClass cc = (XfeComboBoxWidgetClass) XtClass(w);
	
	if (cc->xfe_combo_box_class.layout_title)
	{
		(*cc->xfe_combo_box_class.layout_title)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeComboBoxLayoutArrow(Widget w)
{
	XfeComboBoxWidgetClass cc = (XfeComboBoxWidgetClass) XtClass(w);
	
	if (cc->xfe_combo_box_class.layout_arrow)
	{
		(*cc->xfe_combo_box_class.layout_arrow)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeComboBoxDrawHighlight(Widget			w,
						  XEvent *			event,
						  Region			region,
						  XRectangle *		rect)
{
	XfeComboBoxWidgetClass cc = (XfeComboBoxWidgetClass) XtClass(w);
	
	if (cc->xfe_combo_box_class.draw_highlight)
	{
		(*cc->xfe_combo_box_class.draw_highlight)(w,event,region,rect);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeComboBoxDrawTitleShadow(Widget			w,
							XEvent *		event,
							Region			region,
							XRectangle *	rect)
{
	XfeComboBoxWidgetClass cc = (XfeComboBoxWidgetClass) XtClass(w);
	
	if (cc->xfe_combo_box_class.draw_title_shadow)
	{
		(*cc->xfe_combo_box_class.draw_title_shadow)(w,event,region,rect);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateComboBox(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfeComboBoxWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeComboBoxAddItem(Widget w,XmString item,int position)
{
    XfeComboBoxPart *		cp = _XfeComboBoxPart(w);
/*     XmListPart *			lp = _XfeXmListPart(w); */

	assert( _XfeIsAlive(w) );
	assert( XfeIsComboBox(w) );

	XmListAddItem(cp->list,item,position);


#if 0
    DtComboBoxWidget combo = (DtComboBoxWidget)combo_w;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    XmStringTable list_items = ((XmListWidget)combo_p->list)->list.items;
    int i;

    if (item && ((XmListWidget)combo_p->list)->list.itemCount) {
	for (i = 0; i < combo_p->item_count; i++)
	    if (XmStringCompare(item, list_items[i]))
		break;
	if ((i < combo_p->item_count) && unique)
	    return;
    }

    XmListAddItem(combo_p->list, item, pos);
    SyncWithList(combo_p);

    if (combo_p->label) {
	SetMaximumLabelSize(combo_p);
	if (combo_p->type == XmDROP_DOWN_LIST_BOX) {
	    ClearShadow(combo, TRUE);
	    if (combo_p->recompute_size)
		SetComboBoxSize(combo);
	    LayoutChildren(combo);
	    DrawShadow(combo);
	}
    }
    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	SetTextFieldData(combo_p, NULL);
    else
	SetLabelData(combo_p, NULL, FALSE);
#endif

}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeComboBoxAddItemUnique(Widget w,XmString item,int position)
{
/*     XfeComboBoxPart *		cp = _XfeComboBoxPart(w); */

	assert( _XfeIsAlive(w) );
	assert( XfeIsComboBox(w) );
}
/*----------------------------------------------------------------------*/
