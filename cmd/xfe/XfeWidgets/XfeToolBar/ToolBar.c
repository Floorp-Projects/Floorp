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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/ToolBar.c>											*/
/* Description:	XfeToolBar widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <stdio.h>

#include <Xfe/ToolBarP.h>

/* Possible children classes */
#include <Xfe/Label.h>
#include <Xfe/Button.h>
#include <Xfe/Cascade.h>

#include <Xm/Separator.h>
#include <Xm/SeparatoG.h>
#include <Xm/TextF.h>

#define MESSAGE1 "Widget is not an XfeToolBar."
#define MESSAGE2 "XfeToolbar can only have XfeButton & XmSeparator children."
#define MESSAGE3 "XmNmaxChildHeight is a read-only resource."
#define MESSAGE4 "XmNmaxChildWidth is a read-only resource."
#define MESSAGE5 "XmNindicatorPosition is set but XmNnumChildren is 0."
#define MESSAGE6 "XmNindicatorPosition is less than 0."
#define MESSAGE7 "XmNindicatorPosition is more than XmNnumChildren."
#define MESSAGE8 "No valid edit text found in toolbar."
#define MESSAGE9 "XmNindicatorThreshold bust be greater than 0."
#define MESSAGE14 "Cannot accept new child '%s'."

#define DEFAULT_MAX_CHILD_HEIGHT		0
#define DEFAULT_MAX_CHILD_WIDTH			0
#define DEFAULT_INDICATOR_THRESHOLD		10

#define INDICATOR_NAME				"Indicator"
#define EDIT_TEXT_NAME				"EditText"
#define FAR_AWAY					-1000

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassInitialize		(void);
static void		ClassPartInit	(WidgetClass);
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtGeometryResult GeometryManager	(Widget,XtWidgetGeometry *,
										 XtWidgetGeometry *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static void		DrawComponents		(Widget,XEvent *,Region,XRectangle *);
static void		ChangeManaged		(Widget);
static void		PrepareComponents	(Widget,int);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager class methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean	AcceptDynamicChild		(Widget);
static Boolean	InsertDynamicChild		(Widget);
static Boolean	DeleteDynamicChild		(Widget);
static void		LayoutDynamicChildren	(Widget);
static void		GetChildDimensions		(Widget,Dimension *,Dimension *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DrawRaiseBorder		(Widget,XEvent *,Region,XRectangle *);
static void		LayoutIndicator		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Btn3Down			(Widget,XEvent *,char **,Cardinal *);
static void 	Btn3Up				(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Geometry functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredVertical		(Widget,Dimension *,Dimension *);
static void		PreferredHorizontal		(Widget,Dimension *,Dimension *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Layout functions functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutHorizontal			(Widget);
static void		LayoutVertical				(Widget);


static Boolean	IsValidChild		(Widget);
static Boolean	IsButtonChild		(Widget);
static Boolean	IsSeparatorChild	(Widget);
static void		InvokeCallbacks		(Widget,XtCallbackList,Widget,int,XEvent *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar children apply client procedures							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ApplyButtonLayout		(Widget,Widget,XtPointer);
static void		ApplyChildUsePrefWidth	(Widget,Widget,XtPointer);
static void		ApplyChildUsePrefHeight	(Widget,Widget,XtPointer);
static void		ApplySelectionModifiers	(Widget,Widget,XtPointer);
static void		ApplyActiveEnabled		(Widget,Widget,XtPointer);
static void		ApplySelectedEnabled	(Widget,Widget,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Button functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ButtonSetActiveEnabled		(Widget,Widget,Boolean);
static void		ButtonSetSelectedEnabled	(Widget,Widget,Boolean);
static void		ButtonSetActiveWidget		(Widget,Widget,Boolean,XEvent *);
static void		ButtonSetSelectedWidget		(Widget,Widget,Boolean,XEvent *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Button callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ButtonActiveCB		(Widget,XtPointer,XtPointer);
static void		ButtonSelectedCB	(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Indicator functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		IndicatorCreate				(Widget);
static void		IndicatorCheckPosition		(Widget);
static Widget	IndicatorGetTarget			(Widget);
static int		IndicatorGetTargetPosition	(Widget);
static Boolean	IndicatorIsShown			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Edit text functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		EditTextCreate			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Rep type registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void ToolBarRegisterRepTypes(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar Resources                                                 */
/*																		*/
/*----------------------------------------------------------------------*/
static const XtResource resources[] = 	
{					
    /* Callback resources */     	
    { 
		XmNbutton3DownCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . button_3_down_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNbutton3UpCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . button_3_up_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNselectionChangedCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . selection_changed_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNvalueChangedCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . value_changed_callback),
		XmRImmediate, 
		(XtPointer) NULL
    },

    /* Button resources */
    { 
		XmNbuttonLayout,
		XmCButtonLayout,
		XmRButtonLayout,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . button_layout),
		XmRImmediate, 
		(XtPointer) XmBUTTON_LABEL_ONLY
    },

    /* Separator resources */
    { 
		XmNseparatorThickness,
		XmCSeparatorThickness,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . separator_thickness),
		XmRImmediate, 
		(XtPointer) 50
    },

	/* Raised resources */
    { 
		XmNraised,
		XmCRaised,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . raised),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNraiseBorderThickness,
		XmCRaiseBorderThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . raise_border_thickness),
		XmRImmediate, 
		(XtPointer) 0
    },
	{
		XmNchildUsePreferredHeight,
		XmCChildUsePreferredHeight,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . child_use_pref_height),
		XmRImmediate, 
		(XtPointer) True
	},
	{
		XmNchildUsePreferredWidth,
		XmCChildUsePreferredWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . child_use_pref_width),
		XmRImmediate, 
		(XtPointer) True
	},
    { 
		XmNchildForceHeight,
		XmCChildForceHeight,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . child_force_height),
		XmRImmediate, 
		(XtPointer) True
    },
    { 
		XmNchildForceWidth,
		XmCChildForceWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . child_force_width),
		XmRImmediate, 
		(XtPointer) True
    },

	/* Radio resources */
    { 
		XmNradioBehavior,
		XmCRadioBehavior,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . radio_behavior),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNtoggleBehavior,
		XmCToggleBehavior,
		XmRToolBarToggleBehavior,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . toggle_behavior),
		XmRImmediate, 
		(XtPointer) XmTOOL_BAR_TOGGLE_ZERO_OR_ONE
    },
    { 
		XmNactiveButton,
		XmCActiveButton,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . active_button),
		XmRImmediate, 
		(XtPointer) NULL
    },

	/* Selection resources */
    { 
		XmNselectedButton,
		XmCSelectedButton,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . selected_button),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNselectionModifiers,
		XmCSelectionModifiers,
		XmRModifiers,
		sizeof(Modifiers),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . selection_modifiers),
		XmRImmediate, 
		(XtPointer) None
    },
    { 
		XmNselectionPolicy,
		XmCSelectionPolicy,
		XmRToolBarSelectionPolicy,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . selection_policy),
		XmRImmediate, 
		(XtPointer) XmTOOL_BAR_SELECT_NONE
    },

	/* Wrapping resources */
    { 
		XmNallowWrap,
		XmCBoolean,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . allow_wrap),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNmaxNumColumns,
		XmCMaxNumColumns,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . max_num_columns),
		XmRImmediate, 
		(XtPointer) 10
    },
    { 
		XmNmaxNumRows,
		XmCMaxNumRows,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . max_num_rows),
		XmRImmediate, 
		(XtPointer) 10
    },

	/* Indicator resources */
    { 
		XmNindicatorLocation,
		XmCIndicatorLocation,
		XmRToolBarIndicatorLocation,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . indicator_location),
		XmRImmediate, 
		(XtPointer) XmINDICATOR_LOCATION_BEGINNING
    },
    { 
		XmNindicatorPosition,
		XmCIndicatorPosition,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . indicator_position),
		XmRImmediate, 
		(XtPointer) XmINDICATOR_DONT_SHOW
    },
    { 
		XmNindicatorThreshold,
		XmCIndicatorThreshold,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . indicator_threshold),
		XmRImmediate, 
		(XtPointer) DEFAULT_INDICATOR_THRESHOLD
    },
    { 
		XmNdynamicIndicator,
		XmCDynamicIndicator,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . dynamic_indicator),
		XmRImmediate, 
		(XtPointer) True
    },

#if 0
	/* Geometry resources */
	{ 
		XmNmaxChildHeight,
		XmCReadOnly,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . max_child_height),
		XmRImmediate, 
		(XtPointer) DEFAULT_MAX_CHILD_HEIGHT
    },
	{ 
		XmNmaxChildWidth,
		XmCReadOnly,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . max_child_width),
		XmRImmediate, 
		(XtPointer) DEFAULT_MAX_CHILD_WIDTH
    },
#endif
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static const XmSyntheticResource syn_resources[] =
{
    { 
		XmNraiseBorderThickness,
		sizeof(Dimension),
		XtOffsetOf(XfeToolBarRec , xfe_tool_bar . raise_border_thickness),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
};


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "Btn3Down",	Btn3Down		},
    { "Btn3Up",		Btn3Up			},
};


/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(toolbar,ToolBar) =
{
    {
		(WidgetClass) &xfeOrientedClassRec,		/* superclass			*/
		"XfeToolBar",							/* class_name			*/
		sizeof(XfeToolBarRec),					/* widget_size			*/
		ClassInitialize,						/* class_initialize		*/
		ClassPartInit,							/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		actions,								/* actions            	*/
		XtNumber(actions),						/* num_actions        	*/
		(XtResource *)resources,				/* resources			*/
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
/* When XfeManager bugs in GeometryManager() are fixed, I can use it to! */
#if 0
		XtInheritGeometryManager,				/* geometry_manager		*/
#else
		GeometryManager,						/* geometry_manager		*/
#endif
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },

    /* Constraint Part */
    {
		NULL,									/* resource list		*/
		0,										/* num resources		*/
		sizeof(XfeToolBarConstraintRec),		/* constraint size		*/
		NULL,									/* init proc			*/
		NULL,                                   /* destroy proc			*/
		NULL,									/* set values proc		*/
		NULL,                                   /* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		(XmSyntheticResource *)syn_resources,	/* syn resources		*/
		XtNumber(syn_resources),				/* num syn_resources	*/
		NULL,                                   /* syn_cont_resources	*/
		0,                                      /* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process		*/
		NULL,                                   /* extension			*/
    },
    
    /* XfeManager Part 	*/
	{
		XfeInheritBitGravity,					/* bit_gravity				*/
		PreferredGeometry,						/* preferred_geometry		*/
		XfeInheritUpdateBoundary,				/* update_boundary			*/
		XfeInheritUpdateChildrenInfo,			/* update_children_info		*/
		XfeInheritLayoutWidget,					/* layout_widget			*/
		NULL,									/* accept_static_child		*/
		NULL,									/* insert_static_child		*/
		NULL,									/* delete_static_child		*/
		NULL,									/* layout_static_children	*/
		NULL,									/* change_managed			*/
		NULL,									/* prepare_components		*/
		NULL,									/* layout_components		*/
		NULL,									/* draw_background			*/
		XfeInheritDrawShadow,					/* draw_shadow				*/
		DrawComponents,							/* draw_components			*/
		NULL,									/* extension				*/
    },

	/* XfeDynamicManager Part */
    {
		AcceptDynamicChild,						/* accept_dynamic_child		*/
		InsertDynamicChild,						/* insert_dynamic_child		*/
		DeleteDynamicChild,						/* delete_dynamic_child		*/
		LayoutDynamicChildren,					/* layout_dynamic_children	*/
#if 0
		GetChildDimensions,						/* get_child_dimensions		*/
#else
		XfeInheritGetChildDimensions,			/* get_child_dimensions		*/
#endif
		NULL,									/* extension				*/
    },

	/* XfeOriented Part */
	{
		NULL,									/* enter				*/
		NULL,									/* leave				*/
		NULL,									/* motion				*/
		NULL,									/* drag_start			*/
		NULL,									/* drag_end				*/
		NULL,									/* drag_motion			*/
		NULL,									/* des_enter			*/
		NULL,									/* des_leave			*/
		NULL,									/* des_motion			*/
		NULL,									/* des_drag_start		*/
		NULL,									/* des_drag_end			*/
		NULL,									/* des_drag_motion		*/
		NULL,									/* extension          	*/
	},

    /* XfeToolBar Part 	*/
    {
		DrawRaiseBorder,						/* draw_raise_border	*/
		LayoutIndicator,						/* layout_indicator		*/
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeToolBarWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(toolbar,ToolBar);

/*----------------------------------------------------------------------*/
/*																		*/
/* Rep type registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ToolBarRegisterRepTypes(void)
{
    static String select_names[] = 
    { 
		"tool_bar_select_none",
		"tool_bar_select_single",
		"tool_bar_select_multiple",
		NULL
    };
	
    static String toggle_names[] = 
    { 
		"tool_bar_toggle_one_or_more",
		"tool_bar_toggle_only_one",
		"tool_bar_toggle_zero_or_more",
		"tool_bar_toggle_zero_or_one",
		NULL
    };

    static String location_names[] = 
    { 
		"indicator_location_none",
		"indicator_location_beginning",
		"indicator_location_end",
		"indicator_location_middle",
		NULL
    };
	
    XfeRepTypeRegister(XmRToolBarSelectionPolicy,select_names);
    XfeRepTypeRegister(XmRToolBarToggleBehavior,toggle_names);
    XfeRepTypeRegister(XmRToolBarIndicatorLocation,location_names);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassInitialize()
{
	/* Register XfeButton representation types */
	XfeButtonRegisterRepTypes();

	/* Register XfeToolBar representation types */
	ToolBarRegisterRepTypes();
}
/*----------------------------------------------------------------------*/
static void
ClassPartInit(WidgetClass wc)
{
	XfeToolBarWidgetClass cc = (XfeToolBarWidgetClass) wc;
	XfeToolBarWidgetClass sc = (XfeToolBarWidgetClass) wc->core_class.superclass;

	/* Resolve inheritance of all XfeToolBar class methods */
	_XfeResolve(cc,sc,xfe_tool_bar_class,draw_raise_border,
				XfeInheritDrawRaiseBorder);

	_XfeResolve(cc,sc,xfe_tool_bar_class,layout_indicator,
				XfeInheritLayoutIndicator);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
	XfeToolBarPart *		tp = _XfeToolBarPart(nw);

    /* Make sure rep types are ok */
    XfeRepTypeCheck(nw,XmRToolBarSelectionPolicy,&tp->selection_policy,
					XmTOOL_BAR_SELECT_NONE);

    XfeRepTypeCheck(nw,XmRToolBarIndicatorLocation,&tp->indicator_location,
					XmINDICATOR_LOCATION_BEGINNING);

#if 0
	/* max_child_height */
	if (tp->max_child_height > DEFAULT_MAX_CHILD_HEIGHT)
	{
		tp->max_child_height = DEFAULT_MAX_CHILD_HEIGHT;
      
		_XfeWarning(nw,MESSAGE3);
	}

	/* max_child_width */
	if (tp->max_child_width > DEFAULT_MAX_CHILD_WIDTH)
	{
		tp->max_child_width = DEFAULT_MAX_CHILD_WIDTH;
      
		_XfeWarning(nw,MESSAGE4);
	}
#endif

	/* indicator_threshold */
	if (tp->indicator_threshold == 0)
	{
		tp->indicator_threshold = DEFAULT_INDICATOR_THRESHOLD;
      
		_XfeWarning(nw,MESSAGE9);
	}

	/* Add Button3 translations */
	XfeOverrideTranslations(nw,_XfeToolBarExtraTranslations);

    /* Initialize private members */
#if 0
    tp->num_managed				= 0;
    tp->num_components			= 0;
	tp->total_children_width	= 0;
	tp->total_children_height	= 0;
#endif

	tp->indicator				= NULL;
	tp->indicator_target		= NULL;

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeToolBarWidgetClass);
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
    XfeToolBarPart *		np = _XfeToolBarPart(nw);
    XfeToolBarPart *		op = _XfeToolBarPart(ow);
	Boolean					layout_indicator = False;

#if 0
	/* max_child_height */
	if (np->max_child_height != op->max_child_height)
	{
		np->max_child_height = op->max_child_height;
      
		_XfeWarning(nw,MESSAGE3);
	}

	/* max_child_width */
	if (np->max_child_width != op->max_child_width)
	{
		np->max_child_width = op->max_child_width;
      
		_XfeWarning(nw,MESSAGE4);
	}
#endif
    
    /* button_layout */
    if (np->button_layout != op->button_layout)
    {
        XfeManagerApplyLinked(nw,
							  XmMANAGER_DYNAMIC_CHILD,
							  XfeCHILDREN_INFO_ANY,
							  ApplyButtonLayout,
							  (XtPointer) nw,
							  True);

		_XfemConfigFlags(nw) |= XfeConfigGLE;

		_XfemPrepareFlags(nw) |= _XFE_PREPARE_MAX_CHILD_DIMENSIONS;
    }

    /* child_use_pref_width */
    if (np->child_use_pref_width != op->child_use_pref_width)
    {
      XfeManagerApplyLinked(nw,
                            XmMANAGER_DYNAMIC_CHILD,
                            XfeCHILDREN_INFO_ANY,
                            ApplyChildUsePrefWidth,
                            (XtPointer) nw,
                            True);

		_XfemConfigFlags(nw) |= XfeConfigGLE;

		_XfemPrepareFlags(nw) |= _XFE_PREPARE_MAX_CHILD_DIMENSIONS;
    }

    /* child_use_pref_height */
    if (np->child_use_pref_height != op->child_use_pref_height)
    {
      XfeManagerApplyLinked(nw,
                            XmMANAGER_DYNAMIC_CHILD,
                            XfeCHILDREN_INFO_ANY,
                            ApplyChildUsePrefHeight,
                            (XtPointer) nw,
                            True);

		_XfemConfigFlags(nw) |= XfeConfigGLE;

		_XfemPrepareFlags(nw) |= _XFE_PREPARE_MAX_CHILD_DIMENSIONS;
    }

	/* separator_thickness */
	if (np->separator_thickness != op->separator_thickness)
	{
		_XfemConfigFlags(nw) |= XfeConfigLED;
	}

	/* raise_border_thickness */
	if (np->raise_border_thickness != op->raise_border_thickness)
	{
		_XfemConfigFlags(nw) |= XfeConfigGLED;
	}

	/* raised */
	if (np->raised != op->raised)
	{
		_XfemConfigFlags(nw) |= XfeConfigED;
	}

	/* active_button */
	if (np->active_button != op->active_button)
	{
		ButtonSetActiveWidget(nw,np->active_button,False,NULL);
	}

	/* radio_behavior */
	if (np->radio_behavior != op->radio_behavior)
	{
		if (np->radio_behavior)
		{
			ButtonSetActiveWidget(nw,np->active_button,False,NULL);
		}
		else
		{
			ButtonSetActiveWidget(nw,NULL,False,NULL);
		}

        XfeManagerApplyLinked(nw,
                              XmMANAGER_DYNAMIC_CHILD,
                              XfeCHILDREN_INFO_ANY,
                              ApplyActiveEnabled,
                              (XtPointer) nw,
                              True);
	}

	/* selected_button */
	if (np->selected_button != op->selected_button)
	{
		ButtonSetSelectedWidget(nw,np->selected_button,False,NULL);
	}

	/* selection_policy */
	if (np->selection_policy != op->selection_policy)
	{
		/* Make sure new selection policy is ok */
		XfeRepTypeCheck(nw,XmRToolBarSelectionPolicy,&np->selection_policy,
                      XmTOOL_BAR_SELECT_NONE);

		if (np->selection_policy == XmTOOL_BAR_SELECT_SINGLE)
		{
			ButtonSetSelectedWidget(nw,np->selected_button,False,NULL);
		}
		else if (np->selection_policy == XmTOOL_BAR_SELECT_NONE)
		{
			ButtonSetSelectedWidget(nw,NULL,False,NULL);
		}

        XfeManagerApplyLinked(nw,
                              XmMANAGER_DYNAMIC_CHILD,
                              XfeCHILDREN_INFO_ANY,
                              ApplySelectedEnabled,
                              (XtPointer) nw,
                              True);
	}

    /* selection_modifiers */
    if (np->selection_modifiers != op->selection_modifiers)
    {
      XfeManagerApplyLinked(nw,
                            XmMANAGER_DYNAMIC_CHILD,
                            XfeCHILDREN_INFO_ANY,
                            ApplySelectionModifiers,
                            (XtPointer) nw,
                            True);
    }

    /* indicator_position */
    if (np->indicator_position != op->indicator_position)
    {
		/* Create the indicator if it is not already alive */
		if (np->indicator_position != XmINDICATOR_DONT_SHOW)
		{
			if (!np->indicator)
			{
				IndicatorCreate(nw);

				IndicatorCheckPosition(nw);
			}
		}

		layout_indicator = True;
	}

    /* indicator_location */
    if (np->indicator_location != op->indicator_location)
    {
		/* Make sure new indicator policy is ok */
		XfeRepTypeCheck(nw,XmRToolBarIndicatorLocation,&np->indicator_location,
						XmINDICATOR_LOCATION_BEGINNING);

		layout_indicator = True;
	}

    /* indicator_threshold */
    if (np->indicator_threshold != op->indicator_threshold)
    {
		/* indicator_threshold */
		if (np->indicator_threshold == 0)
		{
			np->indicator_threshold = op->indicator_threshold;

			_XfeWarning(nw,MESSAGE9);
		}
	}
    
	/* Layout the indicator if needed */ 
    if (layout_indicator)
	{
		if (np->dynamic_indicator)
		{
			_XfemConfigFlags(nw) |= XfeConfigLayout;
		}
		else
		{
			if (!(_XfemConfigFlags(nw) & XfeConfigLayout))
			{ 
				_XfeToolBarLayoutIndicator(nw); 
			}
		}
	}
	
    return _XfeManagerChainSetValues(ow,rw,nw,xfeToolBarWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtGeometryResult
GeometryManager(Widget child,XtWidgetGeometry *request,XtWidgetGeometry *reply)
{
	Widget				w = XtParent(child);
    XfeToolBarPart *	tp = _XfeToolBarPart(w);
	XtGeometryMask		mask = request->request_mode;
	XtGeometryResult	our_result = XtGeometryNo;

/*  	printf("GeometryManager(%s) - child = %s\n",XtName(w),XtName(child)); */
	
	/* Ignore x changes */
	if (mask & CWX)
	{
		mask |= ~CWX;

		/*_XfeWarning(w,MESSAGE11);*/
	}

	/* Ignore y changes */
	if (mask & CWY)
	{
		mask |= ~CWY;
		
		/*_XfeWarning(w,MESSAGE12);*/
	}

	/* Ignore border_width changes */
	if (mask & CWBorderWidth)
	{
		mask |= ~CWBorderWidth;

		/*_XfeWarning(w,MESSAGE13);*/
	}

	/* Check for something else being requested */
	if ((mask & CWWidth) || (mask & CWHeight))
	{
		Boolean				change_width = False;
		Boolean				change_height = False;
		Dimension			old_width;
		Dimension			old_height;
		Dimension			old_max_child_width;
		Dimension			old_max_child_height;
		Dimension			pref_width;
		Dimension			pref_height;

		/* Remember the child's old dimensions */
		old_width = _XfeWidth(child);
		old_height = _XfeHeight(child);

		/* Adjust the child's dimensions _TEMPORARLY_ */
		if (mask & CWWidth)
		{
			_XfeWidth(child) = request->width;
		}

		if (mask & CWHeight)
		{
			_XfeHeight(child) = request->height;
		}

#if 1
		/* Remember our max child dimensions */
		old_max_child_width = _XfemMaxDynamicWidth(w);
		old_max_child_height = _XfemMaxDynamicHeight(w);

		if (_XfeWidth(child) > _XfemMaxDynamicWidth(w))
		{
			_XfemMaxDynamicWidth(w) = _XfeWidth(child);
		}

		if (_XfeHeight(child) > _XfemMaxDynamicHeight(w))
		{
			_XfemMaxDynamicHeight(w) = _XfeHeight(child);
		}
#endif

#if 0
		/* Update the children info */
		_XfeManagerUpdateChildrenInfo(w);
#endif

		/* Obtain the preferred geometry to support the new child */
		_XfeManagerPreferredGeometry(w,&pref_width,&pref_height);

		/* Restore the child's dimensions */
		_XfeWidth(child) = old_width;
		_XfeHeight(child) = old_height;

#if 0
		/* Update the children info */
		_XfeManagerUpdateChildrenInfo(w);
#endif
			
#if 1
		/* Restore our max children dimensions */
		_XfemMaxDynamicWidth(w) = old_max_child_width;
		_XfemMaxDynamicHeight(w) = old_max_child_height;
#endif

		/* Check for changes in preferred width if needed */
		if (_XfemUsePreferredWidth(w) && (_XfeWidth(w) != pref_width))
		{
			change_width = True;
		}

		/* Check for changes in preferred height if needed */
		if (_XfemUsePreferredHeight(w) && (_XfeHeight(w) != pref_height))
		{
			change_height = True;
		}

		/* Check for geometry changes to ourselves */
		if (change_width || change_height)
		{
			XtGeometryResult	parent_result;
			XtWidgetGeometry	parent_request;
				
			parent_request.request_mode = 0;
				
			/* Request a width change */
			if (change_width)
			{
				parent_request.width = pref_width;
				parent_request.request_mode |= CWWidth;
			}
				
			/* Request a width height */
			if (change_height)
			{
				parent_request.height = pref_height;
				parent_request.request_mode |= CWHeight;
			}

			/* Make the request */
			parent_result = _XmMakeGeometryRequest(w,&parent_request);

			/* Adjust geometry accordingly */
			if (parent_result == XtGeometryYes)
			{
				/* Goody */
				if (mask & CWWidth)
				{
					_XfeWidth(child) = request->width;

					if (_XfeWidth(child) > _XfemMaxDynamicWidth(w))
					{
						_XfemMaxDynamicWidth(w) = _XfeWidth(child);
					}

					_XfemPreferredWidth(w) = pref_width;
				}

				if (mask & CWHeight)
				{
					_XfeHeight(child) = request->height;

					if (_XfeHeight(child) > _XfemMaxDynamicHeight(w))
					{
						_XfemMaxDynamicHeight(w) = _XfeHeight(child);
					}
						
					_XfemPreferredHeight(w) = pref_height;
				}

				XfeResize(w);

				our_result = XtGeometryYes;

			}
			else if(parent_result == XtGeometryNo)
			{
				/* Too bad */
			}
		}
	}

	return our_result;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager class methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean
AcceptDynamicChild(Widget child)
{
	return IsValidChild(child);
}
/*----------------------------------------------------------------------*/
static Boolean
InsertDynamicChild(Widget child)
{
    Widget					w = XtParent(child);
    XfeToolBarPart *		tp = _XfeToolBarPart(w);

	assert( IsValidChild(child) );

	/* Buttons */
	if (IsButtonChild(child))
	{
		Arg			xargs[10];
		Cardinal	n = 0;
		
		XtSetArg(xargs[n],XmNbuttonLayout,tp->button_layout); n++;
		XtSetArg(xargs[n],XmNusePreferredWidth,tp->child_use_pref_width); n++;
		XtSetArg(xargs[n],XmNusePreferredHeight,tp->child_use_pref_height); n++;
		
		if (tp->radio_behavior)
		{
			XtSetArg(xargs[n],XmNbuttonType,XmBUTTON_TOGGLE); n++;
		}
		
		if (tp->selection_policy != XmTOOL_BAR_SELECT_NONE)
		{
			XtSetArg(xargs[n],XmNselectionModifiers,tp->selection_modifiers); n++;
		}
		
		XtSetValues(child,xargs,n);
		
		if (tp->radio_behavior)
		{
			ButtonSetActiveEnabled(w,child,True);
		}
		
		if (tp->selection_policy != XmTOOL_BAR_SELECT_NONE)
		{
			ButtonSetSelectedEnabled(w,child,True);
		}
	}

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteDynamicChild(Widget child)
{
	return True;
}
/*----------------------------------------------------------------------*/
static void
ChangeManaged(Widget w)
{
/*     XfeToolBarPart *	tp = _XfeToolBarPart(w); */

	PrepareComponents(w,_XFE_PREPARE_MAX_CHILD_DIMENSIONS);
}
/*----------------------------------------------------------------------*/
static void
PrepareComponents(Widget w,int flags)
{
/*     XfeToolBarPart *	tp = _XfeToolBarPart(w); */

/* 	flags = _XFE_PREPARE_MAX_CHILD_DIMENSIONS; */

#if 1
    if (flags & _XFE_PREPARE_MAX_CHILD_DIMENSIONS)
    {
		_XfeManagerUpdateChildrenInfo(w);
    }
#endif
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
	_XfeToolBarDrawRaiseBorder(w,event,region,clip_rect);
}
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension * width,Dimension * height)
{
	/* Horizontal */
	if (_XfeOrientedOrientation(w) == XmHORIZONTAL)
	{
		PreferredHorizontal(w,width,height);
	}
	/* Vertical */
	else
	{
		PreferredVertical(w,width,height);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutDynamicChildren(Widget w)
{
	/* Horizontal */
	if (_XfeOrientedOrientation(w) == XmHORIZONTAL)
	{
		LayoutHorizontal(w);
	}
	/* Vertcial */
	else
	{
		LayoutVertical(w);
	}
	
	/* The indicator requires the children to be layed out first */
	_XfeToolBarLayoutIndicator(w);
}
/*----------------------------------------------------------------------*/
static void
GetChildDimensions(Widget child,Dimension * width_out,Dimension * height_out)
{
	Widget				w = _XfeParent(child);
    XfeToolBarPart *	tp = _XfeToolBarPart(w);
	Dimension			width = 0;
	Dimension			height = 0;

	assert( width_out != NULL );
	assert( height_out != NULL );

	/* Horizontal */
    if (_XfeOrientedOrientation(w) == XmHORIZONTAL)
	{
		if (IsButtonChild(child))
		{
			/* The button's width */
			width = 
				tp->child_force_width ? 
				_XfemMaxDynamicWidth(w) : 
				_XfeWidth(child);
				
			/* The button's height */
			height = 
				tp->child_force_height ? 
				(_XfemBoundaryHeight(w) - 2 * tp->raise_border_thickness) : 
				_XfeHeight(child);
		}
		else if (IsSeparatorChild(child))
		{
			if (_XfeChildIsShown(child) && 
				_XfeWidth(child) && 
				_XfeHeight(child))
			{
				width = _XfeWidth(child);
				height = (_XfeHeight(w) * tp->separator_thickness) / 100;
			}
		}
	}
	/* Vertical */
	else if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		if (IsButtonChild(child))
		{
			/* The button's width */
			width = 
				tp->child_force_width ? 
				(_XfemBoundaryWidth(w) - 2 * tp->raise_border_thickness) : 
				_XfeWidth(child);
			
			/* The button's height */
			height = 
				tp->child_force_height ? 
				_XfemMaxDynamicHeight(w) : 
				_XfeHeight(child);
		}
		else if (IsSeparatorChild(child))
		{
			if (_XfeChildIsShown(child) && 
				_XfeWidth(child) > 0 && 
				_XfeHeight(child) > 0)
			{
				width = (_XfeWidth(w) * tp->separator_thickness) / 100;
				height = _XfeHeight(child);
			}
			else
			{
				assert( 0 );
			}
		}
	}

	*width_out = width;
	*height_out = height;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DrawRaiseBorder(Widget w,XEvent *event,Region region,XRectangle * clip_rect)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

	/* Make sure there is a highlight to draw */
	if (!tp->raise_border_thickness || !tp->raised)
	{
		return;
	}

	/* The shadow thickness can be used to tweak the raised effect */
	switch(tp->raise_border_thickness)
	{
	case 2:
	case 4:

		XfeDrawRectangle(XtDisplay(w),
						 _XfeWindow(w),
						 _XfemHighlightGC(w),
						 0,
						 0,
						 _XfeWidth(w),
						 _XfeHeight(w),
						 tp->raise_border_thickness / 2);

		_XmDrawShadows(XtDisplay(w),
					   _XfeWindow(w),
					   _XfemTopShadowGC(w),
					   _XfemBottomShadowGC(w),
					   tp->raise_border_thickness / 2,
					   tp->raise_border_thickness / 2,
					   _XfeWidth(w) - 2 * (tp->raise_border_thickness / 2),
					   _XfeHeight(w) - 2 * (tp->raise_border_thickness / 2),
					   tp->raise_border_thickness / 2,
					   XmSHADOW_OUT);

		break;

	default:

		XfeDrawRectangle(XtDisplay(w),
						 _XfeWindow(w),
						 _XfemHighlightGC(w),
						 0,
						 0,
						 _XfeWidth(w),
						 _XfeHeight(w),
						 tp->raise_border_thickness);
		break;
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutIndicator(Widget w)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);
	int					x;
	int					y;

	/* If a previous indicator target exits and its a cascade, un raise it */
	if (_XfeIsAlive(tp->indicator_target) && 
		XfeIsCascade(tp->indicator_target))
	{
		XtVaSetValues(tp->indicator_target,XmNraised,False,NULL);
	}

	/* Reset the indicator target */
	tp->indicator_target = NULL;

	if (!_XfeIsAlive(tp->indicator))
	{
		return;
	}
	
	if (tp->indicator_position == XmINDICATOR_DONT_SHOW)
	{
		_XfeMoveWidget(tp->indicator,FAR_AWAY,FAR_AWAY);
		
		return;
	}

	tp->indicator_target = IndicatorGetTarget(w);

	/* Make sure the tp->indicator_target is alive */
	if (!_XfeIsAlive(tp->indicator_target))
	{
		tp->indicator_target = NULL;

		_XfeMoveWidget(tp->indicator,FAR_AWAY,FAR_AWAY);
		
		return;
	}

	/* Horizontal */
    if (_XfeOrientedOrientation(w) == XmHORIZONTAL)
	{
		/* XfeCascade items */
		if (XfeIsCascade(tp->indicator_target))
		{
			/* Beginning */
			if (tp->indicator_location == XmINDICATOR_LOCATION_BEGINNING)
			{
				if (tp->dynamic_indicator)
				{
					x = _XfeX(tp->indicator_target) -
						_XfeOrientedSpacing(w) -
						_XfeWidth(tp->indicator);
				}
				else
				{
					x = _XfeX(tp->indicator_target) -  
						_XfeWidth(tp->indicator) / 2; 
				}
			}
			/* End */
			else if (tp->indicator_location == XmINDICATOR_LOCATION_END)
			{
				if (tp->dynamic_indicator)
				{
					x = _XfeX(tp->indicator_target) + 
						_XfeWidth(tp->indicator_target) + 
						_XfeOrientedSpacing(w);
				}
				else
				{
					x = _XfeX(tp->indicator_target) +  
						_XfeWidth(tp->indicator_target) -  
						_XfeWidth(tp->indicator) / 2; 
				}
			}
			/* Middle */
			else if (tp->indicator_location == XmINDICATOR_LOCATION_MIDDLE)
			{
				_XfeMoveWidget(tp->indicator,FAR_AWAY,FAR_AWAY);

				XtVaSetValues(tp->indicator_target,XmNraised,True,NULL);

				return;
			}
		}
		/* Other items */
		else
		{
			/* Beginning */
			if (tp->indicator_location == XmINDICATOR_LOCATION_BEGINNING)
			{
				if (tp->dynamic_indicator)
				{
					x = _XfeX(tp->indicator_target) -
						_XfeOrientedSpacing(w) -
						_XfeWidth(tp->indicator);
				}
				else
				{
					x = _XfeX(tp->indicator_target) -  
						_XfeWidth(tp->indicator) / 2; 
				}
			}
			/* End */
			else if (tp->indicator_location == XmINDICATOR_LOCATION_END)
			{
				if (tp->dynamic_indicator)
				{
					x = _XfeX(tp->indicator_target) + 
						_XfeWidth(tp->indicator_target) + 
						_XfeOrientedSpacing(w);
				}
				else
				{
					x = _XfeX(tp->indicator_target) +  
						_XfeWidth(tp->indicator_target) -  
						_XfeWidth(tp->indicator) / 2; 
				}
			}
			/* Middle */
			else if (tp->indicator_location == XmINDICATOR_LOCATION_MIDDLE)
			{
				/*
				 * This should not happen - items other than cascade 
				 * cannot have indicators in the middle.
				 */
				assert( 0 );
			}
		}

		y = _XfeHeight(w) / 2 - 
			_XfeHeight(tp->indicator) / 2;
	}
	/* Vertical */
	else if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		if (XfeIsCascade(tp->indicator_target))
		{
			/* Beginning */
			if (tp->indicator_location == XmINDICATOR_LOCATION_BEGINNING)
			{
				if (tp->dynamic_indicator)
				{
					y = _XfeY(tp->indicator_target) - 
						_XfeHeight(tp->indicator) / 2;
				}
				else
				{
					y = _XfeY(tp->indicator_target) -  
						_XfeHeight(tp->indicator) / 2; 
				}
			}
			/* End */
			else if (tp->indicator_location == XmINDICATOR_LOCATION_END)
			{
				if (tp->dynamic_indicator)
				{
					y = _XfeY(tp->indicator_target) + 
						_XfeHeight(tp->indicator_target) - 
						_XfeHeight(tp->indicator) / 2;
				}
				else
				{
					y = _XfeY(tp->indicator_target) +  
						_XfeHeight(tp->indicator_target) -  
						_XfeHeight(tp->indicator) / 2; 
				}
			}
			/* Middle */
			else if (tp->indicator_location == XmINDICATOR_LOCATION_MIDDLE)
			{
				_XfeMoveWidget(tp->indicator,FAR_AWAY,FAR_AWAY);
				
				XtVaSetValues(tp->indicator_target,XmNraised,True,NULL);

				return;
			}
		}
		else
		{
			/* Beginning */
			if (tp->indicator_location == XmINDICATOR_LOCATION_BEGINNING)
			{
				if (tp->dynamic_indicator)
				{
					y = _XfeY(tp->indicator_target) - 
						_XfeHeight(tp->indicator) / 2;
				}
				else
				{
					y = _XfeY(tp->indicator_target) -  
						_XfeHeight(tp->indicator) / 2; 
				}
			}
			/* End */
			else if (tp->indicator_location == XmINDICATOR_LOCATION_END)
			{
				if (tp->dynamic_indicator)
				{
					y = _XfeY(tp->indicator_target) + 
						_XfeHeight(tp->indicator_target) - 
						_XfeHeight(tp->indicator) / 2;
				}
				else
				{
					y = _XfeY(tp->indicator_target) +  
						_XfeHeight(tp->indicator_target) -  
						_XfeHeight(tp->indicator) / 2; 
				}
			}
			/* Middle */
			else if (tp->indicator_location == XmINDICATOR_LOCATION_MIDDLE)
			{
				/*
				 * This should not happen - items other than cascade 
				 * cannot have indicators in the middle.
				 */
				assert( 0 );
			}
		}

		x = _XfeWidth(w) / 2 - 
			_XfeWidth(tp->indicator) / 2;
	}

	_XfeMoveWidget(tp->indicator,x,y);

	XtManageChild(tp->indicator);

	/* Raise the indicator so that it is always on top of tool items */
	if (_XfeIsRealized(tp->indicator))
	{
		XRaiseWindow(XtDisplay(w),_XfeWindow(tp->indicator));
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Btn3Down(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

/* 	printf("Btn3Down(%s)\n",XtName(w)); */

	_XfeInvokeCallbacks(w,tp->button_3_down_callback,
						XmCR_BUTTON_3_DOWN,event,False);
}
/*----------------------------------------------------------------------*/
static void
Btn3Up(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

/* 	printf("Btn3Up(%s)\n",XtName(w)); */

	_XfeInvokeCallbacks(w,tp->button_3_up_callback,
						XmCR_BUTTON_3_UP,event,False);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Geometry functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PreferredVertical(Widget w,Dimension * width,Dimension * height)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

	assert (_XfemDynamicChildren(w) != NULL );
	
	*height = 
		_XfemOffsetTop(w)  + _XfemOffsetBottom(w) +
		2 * tp->raise_border_thickness;

	*width  = 
		_XfemOffsetLeft(w) + _XfemOffsetRight(w) + 
		2 * tp->raise_border_thickness;

	if (_XfemNumDynamicChildren(w) > 0)
	{
		XfeLinkNode	node;

		/* Traverse the dynamic children */
		for (node = XfeLinkedHead(_XfemDynamicChildren(w));
			 node != NULL;
			 node = XfeLinkNodeNext(node))
		{
			/* Widget		child = layable_children[i]; */
			Widget		child = (Widget) XfeLinkNodeItem(node);
			Dimension	child_width = 0;
			Dimension	child_height = 0;

			if (_XfeChildIsShown(child))
			{
				/* Obtain the dimensions for the child */
				GetChildDimensions(child,&child_width,&child_height);
				
/* 				assert( child_width > 0 ); */
/* 				assert( child_height > 0 ); */
				
				*height += (child_height + _XfeOrientedSpacing(w));
			}
		}
	}

	*width += _XfemMaxDynamicWidth(w);

	/* Indicator */
	if (IndicatorIsShown(w) && tp->dynamic_indicator)
	{
 		*height += (_XfeHeight(tp->indicator) + _XfeOrientedSpacing(w));
	}
}
/*----------------------------------------------------------------------*/
static void
PreferredHorizontal(Widget w,Dimension * width,Dimension * height)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

	assert (_XfemDynamicChildren(w) != NULL );
	
	*height = 
		_XfemOffsetTop(w)  + _XfemOffsetBottom(w) + 
		2 * tp->raise_border_thickness;

	*width  = 
		_XfemOffsetLeft(w) + _XfemOffsetRight(w) + 
		2 * tp->raise_border_thickness;

#if 1
	if (_XfemNumDynamicChildren(w) > 0)
	{
		XfeLinkNode	node;

		/* Traverse the dynamic children */
		for (node = XfeLinkedHead(_XfemDynamicChildren(w));
			 node != NULL;
			 node = XfeLinkNodeNext(node))
		{
			/* Widget		child = layable_children[i]; */
			Widget		child = (Widget) XfeLinkNodeItem(node);
			Dimension	child_width = 0;
			Dimension	child_height = 0;

			if (_XfeChildIsShown(child))
			{
				/* Obtain the dimensions for the child */
				GetChildDimensions(child,&child_width,&child_height);
				
/* 				assert( child_width > 0 ); */
/* 				assert( child_height > 0 ); */

				*width += (child_width + _XfeOrientedSpacing(w));

/* 				printf("%s: width = %d\n",XtName(child),child_width); */
			}
		}
	}
#endif

	*height += _XfemMaxDynamicHeight(w);

/*    	*width += _XfemTotalDynamicWidth(w); */

	/* Indicator */
	if (IndicatorIsShown(w) && tp->dynamic_indicator)
	{
		*width += (_XfeWidth(tp->indicator) + _XfeOrientedSpacing(w));
	}
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Layout functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutHorizontal(Widget w)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

	assert (_XfemDynamicChildren(w) != NULL );

	if (_XfemNumDynamicChildren(w) > 0)
	{
		XfeLinkNode	node;
		Position	x = _XfemOffsetLeft(w) + tp->raise_border_thickness;
		Position	y = _XfemOffsetTop(w) + tp->raise_border_thickness;

		/* Traverse the dynamic children */
		for (node = XfeLinkedHead(_XfemDynamicChildren(w));
			 node != NULL;
			 node = XfeLinkNodeNext(node))
		{
			/* Widget		child = layable_children[i]; */
			Widget		child = (Widget) XfeLinkNodeItem(node);
			Dimension	width = 0;
			Dimension	height = 0;

/*  		assert( _XfeChildIsShown(child) ); */

			if (_XfeChildIsShown(child))
			{
				/* Obtain the dimensions for the child */
				GetChildDimensions(child,&width,&height);
				
/* 				assert( width > 0 ); */
/* 				assert( height > 0 ); */

				if ((width > 0) && (height > 0))
				{
					_XfeConfigureWidget(child,x,y,width,height);
				}
				
				x += (_XfeWidth(child) + _XfeOrientedSpacing(w));
			}
		}
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutVertical(Widget w)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

	assert (_XfemDynamicChildren(w) != NULL );

	if (_XfemNumDynamicChildren(w) > 0)
	{
		XfeLinkNode	node;
		Position	x = _XfemOffsetLeft(w) + tp->raise_border_thickness;
		Position	y = _XfemOffsetTop(w) + tp->raise_border_thickness;

		/* Traverse the dynamic children */
		for (node = XfeLinkedHead(_XfemDynamicChildren(w));
			 node != NULL;
			 node = XfeLinkNodeNext(node))
		{
			Widget		child = (Widget) XfeLinkNodeItem(node);
			Dimension	width = 0;
			Dimension	height = 0;
			
/*  		assert( _XfeChildIsShown(child) ); */

			if (_XfeChildIsShown(child))
			{
				/* Obtain the dimensions for the child */
				GetChildDimensions(child,&width,&height);

/* 				assert( width > 0 ); */
/* 				assert( height > 0 ); */

				if ((width > 0) && (height > 0))
				{
					_XfeConfigureWidget(child,x,y,width,height);
				}

				y += (_XfeHeight(child) + _XfeOrientedSpacing(w));
			}
		}
	}
}
/*----------------------------------------------------------------------*/
static Boolean
IsValidChild(Widget child)
{
	return (IsButtonChild(child) || IsSeparatorChild(child));
}
/*----------------------------------------------------------------------*/
static Boolean
IsButtonChild(Widget child)
{
	return (XfeIsButton(child) || XfeIsCascade(child));
}
/*----------------------------------------------------------------------*/
static Boolean
IsSeparatorChild(Widget child)
{
	return (XmIsSeparator(child) || 
			XmIsSeparatorGadget(child) ||
			XmIsLabel(child) || 
			XmIsLabelGadget(child) ||
			XfeIsLabel(child));
}
/*----------------------------------------------------------------------*/
static void
InvokeCallbacks(Widget			w,
				XtCallbackList	list,
				Widget			button,
				int				reason,
				XEvent *		event)
{
/*     XfeToolBarPart *	tp = _XfeToolBarPart(w); */

    /* Invoke the callbacks only if needed */
	if (list)
    {
		XfeToolBarCallbackStruct cbs;
	
		cbs.event		= event;
		cbs.reason		= reason;

		cbs.armed		= False;
		cbs.selected	= False;

		if (_XfeIsAlive(button))
		{
			XtVaSetValues(button,
						  XmNarmed,		&cbs.armed,
						  XmNselected,	&cbs.selected,
						  NULL);
		}

		/* Flush the display */
		XFlush(XtDisplay(w));
	
		/* Invoke the Callback List */
		XtCallCallbackList(w,list,&cbs);
    }
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Button functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ButtonSetActiveEnabled(Widget w,Widget button,Boolean state)
{
	assert( XfeIsToolBar(w) );

	XtRemoveCallback(button,XmNactivateCallback,ButtonActiveCB,w);

	if (state)
	{
		XtAddCallback(button,XmNactivateCallback,ButtonActiveCB,w);
	}
}
/*----------------------------------------------------------------------*/
static void
ButtonSetSelectedEnabled(Widget w,Widget button,Boolean state)
{
	assert( XfeIsToolBar(w) );

	XtRemoveCallback(button,XmNselectionChangedCallback,ButtonSelectedCB,w);

	if (state)
	{
		XtAddCallback(button,XmNselectionChangedCallback,ButtonSelectedCB,w);
	}
}
/*----------------------------------------------------------------------*/
static void
ButtonSetActiveWidget(Widget	w,
					  Widget	button,
					  Boolean	invoke_callbacks,
					  XEvent *	event)
{
	XfeToolBarPart *	tp = _XfeToolBarPart(w);
	Cardinal			i;
	Widget				new_active_button = NULL;

#ifdef DEBUG_ramiro
    printf("ButtonSetActiveWidget(%s,%s)\n",
           XtName(w),
           _XfeIsAlive(button) ? XtName(button) : "NULL");
#endif

	for (i = 0; i < _XfemNumChildren(w); i++)
	{
		Widget child = _XfemChildren(w)[i];
	    
		if (IsButtonChild(child) && _XfeIsAlive(child))
		{
			if (child == button)
			{
				XtVaSetValues(child,XmNarmed,True,NULL);

				new_active_button = child;
			}
			else
			{
				XtVaSetValues(child,XmNarmed,False,NULL);
			}
		}
	}

	if (new_active_button != tp->active_button)
	{
		tp->active_button = new_active_button;

		if (invoke_callbacks)
		{
			InvokeCallbacks(w,
							tp->value_changed_callback,
							tp->active_button,
							XmCR_TOOL_BAR_VALUE_CHANGED,
							event);
		}
	}
}
/*----------------------------------------------------------------------*/
static void
ButtonSetSelectedWidget(Widget		w,
						Widget		button,
						Boolean		invoke_callbacks,
						XEvent *	event)
{
	XfeToolBarPart *	tp = _XfeToolBarPart(w);
	Cardinal			i;
	Widget				new_selected_button = NULL;

	for (i = 0; i < _XfemNumChildren(w); i++)
	{
		Widget child = _XfemChildren(w)[i];
	    
		if (IsButtonChild(child) && _XfeIsAlive(child))
		{
			if (child == button)
			{
				XtVaSetValues(child,XmNselected,True,NULL);

				new_selected_button = child;
			}
			else
			{
				XtVaSetValues(child,XmNselected,False,NULL);
			}
		}
	}

	if (new_selected_button != tp->selected_button)
	{
		tp->selected_button = new_selected_button;

		if (invoke_callbacks)
		{
			InvokeCallbacks(w,
							tp->value_changed_callback,
							tp->selected_button,
							XmCR_TOOL_BAR_VALUE_CHANGED,
							event);
		}
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Button callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ButtonActiveCB(Widget button,XtPointer client_data,XtPointer call_data)
{
    Widget						w = (Widget) client_data;
	XfeButtonCallbackStruct *	data = (XfeButtonCallbackStruct *) call_data;

	ButtonSetActiveWidget(w,button,True,data->event);
}
/*----------------------------------------------------------------------*/
static void
ButtonSelectedCB(Widget button,XtPointer client_data,XtPointer call_data)
{
    Widget										w = (Widget) client_data;
	XfeLabelSelectionChangedCallbackStruct *	data = 
		(XfeLabelSelectionChangedCallbackStruct *) call_data;

	ButtonSetSelectedWidget(w,button,True,data->event);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar children apply client procedures							*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ApplyButtonLayout(Widget w,Widget child,XtPointer client_data)
{
    XfeToolBarPart *	tp = _XfeToolBarPart((Widget) client_data);

	if (IsButtonChild(child))
	{
		Arg			xargs[3];
		Cardinal	n = 0;
		
		XtSetArg(xargs[n],XmNbuttonLayout,tp->button_layout); n++;

		XtSetValues(child,xargs,n);
	}
}
/*----------------------------------------------------------------------*/
static void
ApplyChildUsePrefWidth(Widget w,Widget child,XtPointer client_data)
{
    XfeToolBarPart *	tp = _XfeToolBarPart((Widget) client_data);
	Arg					xargs[1];

	XtSetArg(xargs[0],XmNusePreferredWidth,tp->child_use_pref_width);

	if (XfeIsPrimitive(child))
	{
		XtSetValues(child,xargs,1);
	}
}
/*----------------------------------------------------------------------*/
static void
ApplyChildUsePrefHeight(Widget w,Widget child,XtPointer client_data)
{
    XfeToolBarPart *	tp = _XfeToolBarPart((Widget) client_data);
	Arg					xargs[1];

	XtSetArg(xargs[0],XmNusePreferredHeight,tp->child_use_pref_height);

	if (XfeIsPrimitive(child))
	{
		XtSetValues(child,xargs,1);
	}
}
/*----------------------------------------------------------------------*/
static void
ApplySelectionModifiers(Widget w,Widget child,XtPointer client_data)
{
    XfeToolBarPart *	tp = _XfeToolBarPart((Widget) client_data);

	if (IsButtonChild(child))
	{
		Arg xargs[1];
		
		XtSetArg(xargs[0],XmNselectionModifiers,tp->selection_modifiers);

		XtSetValues(child,xargs,1);
	}
}
/*----------------------------------------------------------------------*/
static void
ApplyActiveEnabled(Widget w,Widget child,XtPointer client_data)
{
	XfeToolBarPart *	tp = _XfeToolBarPart(w);

	if (IsButtonChild(child))
	{
		ButtonSetActiveEnabled(w,child,tp->radio_behavior);
	}
}
/*----------------------------------------------------------------------*/
static void
ApplySelectedEnabled(Widget w,Widget child,XtPointer client_data)
{
	XfeToolBarPart *	tp = _XfeToolBarPart(w);

	if (IsButtonChild(child))
	{
		ButtonSetSelectedEnabled(w,child,tp->selection_policy != XmTOOL_BAR_SELECT_NONE);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Indicator functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
IndicatorCreate(Widget w)
{
	XfeToolBarPart *	tp = _XfeToolBarPart(w);

	assert( !_XfeIsAlive(tp->indicator ) );

	if (tp->dynamic_indicator)
	{
		tp->indicator = XtVaCreateManagedWidget(
			INDICATOR_NAME,
			xfeButtonWidgetClass,
			w,
/* 			XmNprivateComponent,	True, */
 			XmNraised,				True,
			NULL);
	}
	else
	{
		tp->indicator = XtVaCreateManagedWidget(
			INDICATOR_NAME,
			xmSeparatorWidgetClass,
			w,
/* 			XmNprivateComponent,	True, */
			XmNwidth,				8,
			XmNshadowThickness,		4,
			NULL);
	}
}
/*----------------------------------------------------------------------*/
static void
IndicatorCheckPosition(Widget w)
{
	XfeToolBarPart *	tp = _XfeToolBarPart(w);
	
	assert( _XfeIsAlive(tp->indicator) );
	
	/* No children */
	if (_XfemNumChildren(w) == 0)
	{
		tp->indicator_position = XmINDICATOR_DONT_SHOW;

		_XfeWarning(w,MESSAGE5);
		
		return;
	}

	/* XmLAST_POSITION */
	if (tp->indicator_position == XmLAST_POSITION)
	{
		tp->indicator_position = _XfemNumChildren(w) - 1;

		return;
	}

	if (tp->indicator_position < 0)
	{
		tp->indicator_position = 0;

		_XfeWarning(w,MESSAGE6);

	}

	if (tp->indicator_position > _XfemNumChildren(w))
	{
		tp->indicator_position = _XfemNumChildren(w) - 1;

		_XfeWarning(w,MESSAGE6);
	}
}
/*----------------------------------------------------------------------*/
static Boolean
IndicatorIsShown(Widget w)
{
	XfeToolBarPart *	tp = _XfeToolBarPart(w);
	Boolean				shown = False;
	
	if (_XfeIsAlive(tp->indicator))
	{
		if (tp->indicator_position != XmINDICATOR_DONT_SHOW)
		{
			Widget target = IndicatorGetTarget(w);
			
			if (_XfeIsAlive(target))
			{
				/* Cascade */
				if (XfeIsCascade(target))
				{
					shown = (tp->indicator_location != 
							 XmINDICATOR_LOCATION_MIDDLE);
				}
				else
				{
					shown = True;
				}
			}
		}
	}

	return shown;
}
/*----------------------------------------------------------------------*/
static Widget
IndicatorGetTarget(Widget w)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);
	Widget				target = NULL;
	
	if (tp->indicator_position != XmINDICATOR_DONT_SHOW)
	{
		target = _XfeChildrenIndex(w,tp->indicator_position);

		if (!_XfeIsAlive(target))
		{
			target = NULL;
		}
	}

	return target;
}
/*----------------------------------------------------------------------*/
static int
IndicatorGetTargetPosition(Widget w)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);
	Widget				target = IndicatorGetTarget(w);

	if (_XfeIsAlive(target))
	{
		if (XfeIsCascade(target) && 
			(tp->indicator_location == XmINDICATOR_LOCATION_MIDDLE))
		{
			return XmINDICATOR_DONT_SHOW;
		}

		if (tp->indicator_location == XmINDICATOR_LOCATION_BEGINNING)
		{
			return tp->indicator_position;
		}
		else if (tp->indicator_location == XmINDICATOR_LOCATION_END)
		{
			return tp->indicator_position + 1;
		}
	}

	return XmINDICATOR_DONT_SHOW;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* EditText functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
EditTextCreate(Widget w)
{
	XfeToolBarPart *	tp = _XfeToolBarPart(w);

	assert( !_XfeIsAlive(tp->edit_text ) );

	tp->edit_text = XtVaCreateWidget(EDIT_TEXT_NAME,
									 xmTextFieldWidgetClass,
									 w,
/* 									 XmNprivateComponent,	True, */
									 NULL);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar Method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeToolBarLayoutIndicator(Widget w)
{
	XfeToolBarWidgetClass tc = (XfeToolBarWidgetClass) XtClass(w);
	
	if (tc->xfe_tool_bar_class.layout_indicator)
	{
		(*tc->xfe_tool_bar_class.layout_indicator)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeToolBarDrawRaiseBorder(Widget		w,
						  XEvent *		event,
						  Region		region,
						  XRectangle *	clip_rect)
{
	XfeToolBarWidgetClass tc = (XfeToolBarWidgetClass) XtClass(w);

	if (tc->xfe_tool_bar_class.draw_raise_border)
	{
		(*tc->xfe_tool_bar_class.draw_raise_border)(w,event,region,clip_rect);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateToolBar(Widget pw,char * name,Arg * av,Cardinal ac)
{
   return XtCreateWidget(name,xfeToolBarWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeToolBarSetActiveButton(Widget w,Widget button)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );
	assert( _XfeIsAlive(button) );
	assert( _XfeParent(button) == w );

	/* Make sure they are alive */
	if (!_XfeIsAlive(w) || !_XfeIsAlive(button))
	{
		return False;
	}

	/* Make sure the active button has changed */
	if (button == tp->active_button)
	{
		return False;
	}

	ButtonSetActiveWidget(w,button,False,NULL);

	return True;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeToolBarSetSelectedButton(Widget w,Widget button)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);

	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );
	assert( _XfeIsAlive(button) );
	assert( _XfeParent(button) == w );

	/* Make sure they are alive */
	if (!_XfeIsAlive(w) || !_XfeIsAlive(button))
	{
		return False;
	}

	/* Make sure the selection policy is ok */
	if (tp->selection_policy == XmTOOL_BAR_SELECT_NONE)
	{
		return False;
	}

	/* Make sure the selected button has changed */
	if (button == tp->selected_button)
	{
		return False;
	}

	ButtonSetSelectedWidget(w,button,False,NULL);

	return True;
}
/*----------------------------------------------------------------------*/
/* extern */ unsigned char
XfeToolBarXYToIndicatorLocation(Widget w,Widget item,int x,int y)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);
	unsigned char		result = XmINDICATOR_LOCATION_NONE;
	int					start_pos;
	int					end_pos;
	int					coord;
	
	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );
	assert( _XfeIsAlive(item) );
	assert( _XfeParent(item) == w );

	/* Horizontal */
    if (_XfeOrientedOrientation(w) == XmHORIZONTAL)
	{
		if (XfeIsCascade(item))
		{
			start_pos = _XfeWidth(item) / tp->indicator_threshold;
			end_pos = _XfeWidth(item) - start_pos;
		}
		else
		{
			start_pos = _XfeWidth(item) / 2;
			end_pos = start_pos;
		}

		coord = x;
	}
	/* Vertical */
	else if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		if (XfeIsCascade(item))
		{
			start_pos = _XfeHeight(item) / tp->indicator_threshold;
			end_pos = _XfeHeight(item) - start_pos;
		}
		else
		{
			start_pos = _XfeHeight(item) / 2;
			end_pos = start_pos;
		}

		coord = y;
	}

	/* Beginning */
	if (coord <= start_pos)
	{
		result = XmINDICATOR_LOCATION_BEGINNING;
	}
	/* End */
	else if (coord >= end_pos)
	{
		result = XmINDICATOR_LOCATION_END;
	}
	/* Middle */
	else if (coord > start_pos && coord < end_pos)
	{
		result = XmINDICATOR_LOCATION_MIDDLE;
	}
	
	return result;
}
/*----------------------------------------------------------------------*/

/* These two first and last functions should be replaced by a general
   purpse XmNlayableChildren resource that can be queried by the user.
   
   This resource should probably go in XfeManager, since it could be
   used by other widgets as well.  This will be an issue when we add
   support for the XmNpositionIndex XfeManager constraint resource
   (currently not working).
*/

/* extern */ Widget
XfeToolBarGetFirstItem(Widget w)
{
#if 0
	Widget * wp;
	
	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );

	/* Look forward at children until a layable one is found */
	for(wp = _XfemChildren(w);
		*wp != _XfeChildrenIndex(w,_XfemNumChildren(w) - 1);
		wp++)
	{
		if (ChildIsLayable(*wp) && !_XfeManagerPrivateComponent(*wp))
		{
			return *wp;
		}
	}
	
	return NULL;
#endif

	XfeLinkNode	node;

	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );
	
	/* Forward search dynamic children until a shown one is found */
	for (node = XfeLinkedHead(_XfemDynamicChildren(w));
		 node != NULL;
		 node = XfeLinkNodeNext(node))
	{
		Widget child = (Widget) XfeLinkNodeItem(node);

		if (_XfeChildIsShown(child))
		{
			return child;
		}
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeToolBarGetLastItem(Widget w)
{
#if 0
	Widget * wp;

	/* Look backwards at children until a layable one is found */
	for(wp = _XfemChildren(w) + (_XfemNumChildren(w) - 1);
		*wp != _XfeChildrenIndex(w,0);
		wp--)
	{
		if (ChildIsLayable(*wp) && !_XfeManagerPrivateComponent(*wp))
		{
			return *wp;
		}
	}

	return NULL;
#endif

	XfeLinkNode	node;
	
	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );

	/* Backward search dynamic children until a shown one is found */
	for (node = XfeLinkedTail(_XfemDynamicChildren(w));
		 node != NULL;
		 node = XfeLinkNodePrev(node))
	{
		Widget child = (Widget) XfeLinkNodeItem(node);

		if (_XfeChildIsShown(child))
		{
			return child;
		}
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeToolBarGetIndicatorItem(Widget w)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);
	
	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );

	/* Make sure the indicator exists */
    if (!tp->indicator)
    {
		IndicatorCreate(w);
    }

    return tp->indicator;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar item editting functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeToolBarGetEditText(Widget w)
{
    XfeToolBarPart *	tp = _XfeToolBarPart(w);
	
	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );

	/* Make sure the edit text exists */
    if (!tp->edit_text)
    {
		EditTextCreate(w);
    }

	/* Make sure the edit text is realized */
    if (!_XfeIsRealized(tp->edit_text))
    {
		XtRealizeWidget(tp->edit_text);
    }

    return tp->edit_text;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolBarEditItem(Widget	w,
				   Widget	item,
				   int		label_x,
				   int		label_y,
				   int		label_width,
				   int		label_height)
{
/*     XfeToolBarPart *	tp = _XfeToolBarPart(w); */
	Widget				text = NULL;

	assert( XfeIsToolBar(w) );
	assert( _XfeIsAlive(w) );
	assert( _XfeIsAlive(item) );
	assert( _XfeParent(item) == w );

	text = XfeToolBarGetEditText(w);
	
	if (!_XfeIsAlive(text))
	{
		_XfeWarning(w,MESSAGE8);
		
		return;
	}

	/* Make the text look like the target item */
	XtVaSetValues(text,
				  XmNbackground,			_XfeBackgroundPixel(item),
				  XmNbackgroundPixmap,		_XfeBackgroundPixmap(item),
				  XmNforeground,			_XfeForeground(item),
				  XmNfontList,				XfeFastAccessFontList(item),
				  XmNshadowThickness,		1,
				  XmNborderWidth,			0,
				  XmNeditable,				True,
				  XmNcursorPositionVisible,	True,
				  XmNmarginWidth,			3,
				  XmNmarginHeight,			1,
				  XmNresizeWidth,			False,
				  NULL);	

	/* Set the edit text's string */
	{
		XmString	xmstr = XfeFastAccessLabelString(item);
		String		str = XfeXmStringGetPSZ(xmstr,XmFONTLIST_DEFAULT_TAG);

		if (str)
		{
			XmTextFieldSetString(text,str);
			
			/* Free the psz String */
			XtFree(str);
		}
	}

	/* Place the edit text above the label */
	{
		int			x;
		int			y;
		int			width;
		int			height;
		Dimension	text_margin_width;
		Dimension	text_margin_height;
		Dimension	text_margin_top;
		Dimension	text_margin_bottom;

		XtVaGetValues(text,
					  XmNmarginWidth,	&text_margin_width,
					  XmNmarginHeight,	&text_margin_height,
					  XmNmarginTop,		&text_margin_top,
					  XmNmarginBottom,	&text_margin_bottom,
					  NULL);

		x = 
			_XfeX(item) +
			label_x - 
			text_margin_width - 
			_XfeShadowThickness(text) -
			_XfeHighlightThickness(text);

		y = 
			_XfeY(item) +
			label_y - 
			text_margin_top -
			text_margin_height - 
			_XfeShadowThickness(text) -
			_XfeHighlightThickness(text);
		
		width = 
			label_width + 
			2 * text_margin_width +
			2 * _XfeShadowThickness(text) + 
			2 * _XfeHighlightThickness(text);

		height = 
			label_height + 
			text_margin_bottom + text_margin_top +
			2 * text_margin_height +
			2 * _XfeShadowThickness(text) +
			2 * _XfeHighlightThickness(text);

		_XfeConfigureWidget(text,x,y,width,height);

		XtManageChild(text);

		XRaiseWindow(XtDisplay(item),_XfeWindow(text));

		XmProcessTraversal(text,XmTRAVERSE_CURRENT);	
	}
}
/*----------------------------------------------------------------------*/
