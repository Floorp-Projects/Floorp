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
/* Name:		<Xfe/FancyBox.c>										*/
/* Description:	XfeFancyBox widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/FancyBoxP.h>
#include <Xfe/BypassShellP.h>
#include <Xfe/ListUtilP.h>

#include <Xm/TextF.h>
#include <Xm/List.h>

#include <Xfe/Button.h>
#include <Xfe/Arrow.h>

#define MESSAGE1 "Widget is not an XfeFancyBox."
#define MESSAGE2 "XmNtitle is a read-only resource."
#define MESSAGE3 "XmNlist is a read-only resource."
#define MESSAGE4 "XmNshell is a read-only resource."
#define MESSAGE5 "XmNarrow is a read-only resource."
#define MESSAGE6 "XmNfancyBoxType is a creation-only resource."
#define MESSAGE7 "No valid XfeBypassShell found to share for XmNshell."

#define LIST_NAME		"FancyList"
#define SHELL_NAME		"FancyShell"
#define TITLE_NAME		"FancyTitle"
#define ARROW_NAME		"FancyArrow"

#define CB_OFFSET_BOTTOM(w,cp)	(cp->highlight_thickness+_XfemOffsetBottom(w))
#define CB_OFFSET_LEFT(w,cp)	(cp->highlight_thickness+_XfemOffsetLeft(w))
#define CB_OFFSET_RIGHT(w,cp)	(cp->highlight_thickness+_XfemOffsetRight(w))
#define CB_OFFSET_TOP(w,cp)		(cp->highlight_thickness+_XfemOffsetTop(w))

#define CB_RECT_X(w,cp)			(_XfemRectX(w) + cp->highlight_thickness)
#define CB_RECT_Y(w,cp)			(_XfemRectY(w) + cp->highlight_thickness)
#define CB_RECT_WIDTH(w,cp)		(_XfemRectWidth(w)-2*cp->highlight_thickness)
#define CB_RECT_HEIGHT(w,cp)	(_XfemRectHeight(w)-2*cp->highlight_thickness)

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
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
static Boolean	InsertChild			(Widget);
static Boolean	DeleteChild			(Widget);
static void		LayoutComponents	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox class methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutTitle			(Widget);
static void		DrawTitleShadow		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeFancyBox functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			DrawHighlight		(Widget,XEvent *,Region,XRectangle *);

static Widget		TitleCreate			(Widget);
static void			LayoutTitle			(Widget);
static void			TitleConfigure		(Widget);

static Widget		TitleTextCreate		(Widget);
static Widget		TitleLabelCreate	(Widget);

static void			IconLayout			(Widget);

static Widget		ShellCreate			(Widget);
static void			ShellLayout			(Widget);

static Widget		ArrowCreate			(Widget);
static void			ArrowLayout			(Widget);


static Widget		ListCreate			(Widget);
static void			ListLayout			(Widget);

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
static void			ArrowArmCB			(Widget,XtPointer,XtPointer);
static void			ArrowDisarmCB		(Widget,XtPointer,XtPointer);
static void			ArrowActivateCB		(Widget,XtPointer,XtPointer);

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
extern void _DtFancyBoxGetArrowSize		SYN_RESOURCE_AA;
extern void _DtFancyBoxGetLabelString		SYN_RESOURCE_AA;
extern void _DtFancyBoxGetListItemCount		SYN_RESOURCE_AA;
extern void _DtFancyBoxGetListItems		SYN_RESOURCE_AA;
extern void _DtFancyBoxGetListFontList		SYN_RESOURCE_AA;
extern void _DtFancyBoxGetListMarginHeight	SYN_RESOURCE_AA;
extern void _DtFancyBoxGetListMarginWidth	SYN_RESOURCE_AA;
extern void _DtFancyBoxGetListSpacing		SYN_RESOURCE_AA;
extern void _DtFancyBoxGetListTopItemPosition	SYN_RESOURCE_AA;
extern void _DtFancyBoxGetListVisibleItemCount	SYN_RESOURCE_AA;
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBox resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
#if 0
    /* Title resources */
	{
		XmNtitle,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . title),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNcomboBoxType,
		XmCComboBoxType,
		XmRComboBoxType,
		sizeof(unsigned char),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . combo_box_type),
		XmRImmediate, 
		(XtPointer) XmCOMBO_BOX_READ_ONLY
    },
    { 
		XmNtitleFontList,
		XmCTitleFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . title_font_list),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{
		XmNtitleShadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . title_shadow_thickness),
		XmRCallProc, 
		(XtPointer) DefaultTitleShadowThickness
	},
	{
		XmNtitleShadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . title_shadow_type),
		XmRImmediate, 
		(XtPointer) XmSHADOW_IN
	},
#endif

    /* Icon resources */
	{
		XmNicon,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . icon),
		XmRImmediate, 
		(XtPointer) NULL
	},


#if 0
	/* List resources */
	{
		XmNlist,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . list),
		XmRImmediate, 
		(XtPointer) NULL
	},
	{
		XmNitems,
		XmCItems,
		XmRXmStringTable,
		sizeof(XmStringTable),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . items),
		XmRImmediate, 
		(XtPointer) NULL
	},
	{
		XmNitemCount,
		XmCItemCount,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . item_count),
		XmRImmediate, 
		(XtPointer) 0
	},
    { 
		XmNlistFontList,
		XmCListFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . list_font_list),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNlistMarginHeight,
		XmCListMarginHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . list_margin_height),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_COMBO_BOX_LIST_MARGIN_HEIGHT
    },
    { 
		XmNlistMarginWidth,
		XmCListMarginWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . list_margin_width),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_COMBO_BOX_LIST_MARGIN_WIDTH
    },
    { 
		XmNlistSpacing,
		XmCListSpacing,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . list_spacing),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_COMBO_BOX_LIST_SPACING
    },
	{
		XmNtopItemPosition,
		XmCTopItemPosition,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . top_item_position),
		XmRImmediate, 
		(XtPointer) 1
	},
	{
		XmNvisibleItemCount,
		XmCVisibleItemCount,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . visible_item_count),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_COMBO_BOX_LIST_VISIBLE_ITEM_COUNT
	},

	/* Shell resources */
	{
		XmNshell,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . shell),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNshareShell,
		XmCShareShell,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . share_shell),
		XmRImmediate, 
		(XtPointer) True
    },
    { 
		XmNpoppedUp,
		XmCReadOnly,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . popped_up),
		XmRImmediate, 
		(XtPointer) False
    },


	/* Arrow resources */
	{
		XmNarrow,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . arrow),
		XmRImmediate, 
		(XtPointer) NULL
	},

	/* Traversal resources */
	{
		XmNhighlightThickness,
		XmCHighlightThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . highlight_thickness),
		XmRImmediate, 
		(XtPointer) 2
	},
    { 
		XmNtraversalOn,
		XmCTraversalOn,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . traversal_on),
		XmRImmediate, 
		(XtPointer) True
    },

    /* Force the XmNshadowThickness to the default */
    { 
		XmNshadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeFancyBoxRec , manager . shadow_thickness),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_SHADOW_THICKNESS
    },

	/* Force XmNmarginLeft and XmNmarginRight to 4 */
    { 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeFancyBoxRec , xfe_manager . margin_left),
		XmRImmediate, 
		(XtPointer) 4
    },
    { 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeFancyBoxRec , xfe_manager . margin_right),
		XmRImmediate, 
		(XtPointer) 4
    },
#endif

};   

#if 0
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFancyBox synthetic resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
	{ 
		XmNitems,
		sizeof(XmStringTable),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . items),
		SyntheticGetListItems,
		_XfeSyntheticSetResourceForChild 
	},
	{ 
		XmNitemCount,
		sizeof(int),
		XtOffsetOf(XfeFancyBoxRec , xfe_fancy_box . item_count),
		SyntheticGetListItemCount,
		_XfeSyntheticSetResourceForChild 
	},
};
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(fancybox,FancyBox) =
{
    {
		(WidgetClass) &xfeComboBoxClassRec,		/* superclass			*/
		"XfeFancyBox",							/* class_name			*/
		sizeof(XfeFancyBoxRec),					/* widget_size			*/
		NULL,									/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		NULL,									/* actions            	*/
		0,										/* num_actions        	*/
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
#if 0
		synthetic_resources,					/* syn resources		*/
		XtNumber(synthetic_resources),			/* num syn_resources	*/
#else
		NULL,									/* syn resources		*/
		0,										/* num syn_resources	*/
#endif
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
		InsertChild,							/* insert_child			*/
		DeleteChild,							/* delete_child			*/
		NULL,									/* change_managed		*/
		NULL,									/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		NULL,									/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		XfeInheritDrawComponents,				/* draw_components		*/
		NULL,									/* extension			*/
    },

    /* XfeComboBox Part */
    {
		LayoutTitle,							/* layout_title			*/
		XfeInheritLayoutArrow,					/* layout_arrow			*/
		XfeInheritDrawHighlight,				/* draw_highlight		*/
		DrawTitleShadow,						/* draw_title_shadow	*/
		NULL,									/* extension			*/
    },

    /* XfeFancyBox Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeFancyBoxWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(fancybox,FancyBox);

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
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeFancyBoxPart *		fp = _XfeFancyBoxPart(nw);

#if 0
    /* Make sure rep types are ok */
	XfeRepTypeCheck(nw,XmRShadowType,&fp->title_shadow_type,
					XmSHADOW_IN);

	XfeRepTypeCheck(nw,XmRComboBoxType,&fp->combo_box_type,
					XmCOMBO_BOX_READ_ONLY);

	/* Make sure read-only resources aren't set */
	if (fp->title)
	{
		_XmWarning(nw,MESSAGE2);

		fp->title = NULL;
	}

	if (fp->list)
	{
		_XmWarning(nw,MESSAGE3);

		fp->list = NULL;
	}

	if (fp->shell)
	{
		_XmWarning(nw,MESSAGE4);

		fp->shell = NULL;
	}

	if (fp->arrow)
	{
		_XmWarning(nw,MESSAGE5);

		fp->arrow = NULL;
	}
	
    /* Create components */
	fp->arrow		= ArrowCreate(nw);
	fp->title		= TitleCreate(nw);
	fp->shell		= ShellCreate(nw);
	fp->list		= ListCreate(nw);

	/* Configure the title */
	TitleConfigure(nw);

    /* Initialize private members */
	fp->highlighted = False;


	/* Manage the children */
	XtManageChild(fp->title);
	XtManageChild(fp->list);
	XtManageChild(fp->arrow);

/*  	XfeOverrideTranslations(nw,_XfeFancyBoxExtraTranslations); */
#endif

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeFancyBoxWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfeFancyBoxPart *		fp = _XfeFancyBoxPart(w); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeFancyBoxPart *		np = _XfeFancyBoxPart(nw);
    XfeFancyBoxPart *		op = _XfeFancyBoxPart(ow);


    return _XfeManagerChainSetValues(ow,rw,nw,xfeFancyBoxWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
/*     XfeFancyBoxPart *		fp = _XfeFancyBoxPart(w); */
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
    XfeFancyBoxPart *		fp = _XfeFancyBoxPart(w);
    XfeComboBoxPart *		cp = _XfeComboBoxPart(w);

	/* Invoke the super class' preferred_geometry method */
	(*xfeComboBoxClassRec.xfe_manager_class.preferred_geometry)(w,
																width,
																height);

	/* Add the icon widht if needed */
	if (_XfeChildIsShown(fp->icon))
	{
		*width  += (cp->spacing + _XfeWidth(fp->icon));
	}
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptChild(Widget child)
{
	Widget					w = _XfeParent(child);
    XfeFancyBoxPart *		fp = _XfeFancyBoxPart(w);

	return (!_XfeIsAlive(fp->icon) && XfeIsButton(child));
}
/*----------------------------------------------------------------------*/
static Boolean
InsertChild(Widget child)
{
	Widget					w = _XfeParent(child);
    XfeFancyBoxPart *		fp = _XfeFancyBoxPart(w);

	fp->icon = child;

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteChild(Widget child)
{
	Widget					w = _XfeParent(child);
    XfeFancyBoxPart *		fp = _XfeFancyBoxPart(w);

	fp->icon = NULL;

	return True;
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
	/* Layout the arrow */
	_XfeComboBoxLayoutArrow(w);

	/* Layout the title */
	_XfeComboBoxLayoutTitle(w);

	/* Layout the icon if needed */
	IconLayout(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeFancyBox functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DrawTitleShadow(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeFancyBoxPart *	fp = _XfeFancyBoxPart(w);
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	/* Make sure the shadow is needed */
	if (!cp->title_shadow_thickness)
	{
		return;
	}

	if (_XfeIsAlive(fp->icon))
	{
		Dimension icon_x1 = _XfeX(fp->icon) - cp->spacing;
		Dimension title_x2 = _XfeX(cp->title) + _XfeWidth(cp->title);

		/* Draw the shadow around the icon and text */
		_XmDrawShadows(XtDisplay(w),
					   _XfeWindow(w),
					   _XfemTopShadowGC(w),_XfemBottomShadowGC(w),
					   
					   CB_OFFSET_LEFT(w,cp),
					   
					   _XfeY(cp->title) - 
					   cp->title_shadow_thickness,

					   (title_x2 - icon_x1) + 
					   2 * cp->title_shadow_thickness,
					   
					   _XfeHeight(cp->title) + 
					   2 * cp->title_shadow_thickness,
					   
					   cp->title_shadow_thickness,
					   cp->title_shadow_type);
	}
	else
	{
		/* Draw the shadow around the text only */
		XfeDrawShadowsAroundWidget(w,
								   cp->title,
								   _XfemTopShadowGC(w),
								   _XfemBottomShadowGC(w),
								   0,
								   cp->title_shadow_thickness,
								   cp->title_shadow_type);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutTitle(Widget w)
{
    XfeFancyBoxPart *	fp = _XfeFancyBoxPart(w);
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);
	int					x;
	int					total_icon_width = 0;

	if (_XfeIsAlive(fp->icon))
	{
		x = _XfeX(fp->icon) + _XfeWidth(fp->icon);

		total_icon_width = _XfeWidth(fp->icon) + cp->spacing;
	}
	else
	{
		x = CB_OFFSET_LEFT(w,cp) + cp->title_shadow_thickness;
	}

	_XfeConfigureWidget(cp->title,

						x,

						(_XfeHeight(w) - _XfeHeight(cp->title)) / 2,
						
						CB_RECT_WIDTH(w,cp) - 
						cp->spacing -
						_XfeWidth(cp->arrow) -
						total_icon_width -
						2 * cp->title_shadow_thickness,
						
						CB_RECT_HEIGHT(w,cp) - 
						2 * cp->title_shadow_thickness);
}
/*----------------------------------------------------------------------*/
static void
IconLayout(Widget w)
{
    XfeFancyBoxPart *	fp = _XfeFancyBoxPart(w);
    XfeComboBoxPart *	cp = _XfeComboBoxPart(w);

	if (!_XfeIsAlive(fp->icon))
	{
		return;
	}

	_XfeConfigureWidget(fp->icon,

						CB_OFFSET_LEFT(w,cp) + 
						cp->title_shadow_thickness +
						cp->spacing,

						(_XfeHeight(w) - _XfeHeight(fp->icon)) / 2,

						_XfeWidth(fp->icon),

						_XfeHeight(fp->icon));
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
/* XfeFancyBox Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateFancyBox(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfeFancyBoxWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
