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
/* Name:		<Xfe/ToolTipShell.c>									*/
/* Description:	XfeToolTipShell widget source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/ToolTipShellP.h>

#define MESSAGE1 "Widget is not an XfeToolTipShell."
#define MESSAGE2 "XmNtoolTupWidget is a read-only resource."

#define TOOL_TIP_LABEL_NAME		"ToolTipLabel"

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassInitialize		(void);
static void 	Initialize			(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy				(Widget);
static Boolean	SetValues			(Widget,Widget,Widget,ArgList,Cardinal *);
static void		GetValuesHook		(Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* ToolTipShell widget functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget		LabelCreate				(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Screen functions functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static int			ScreenGetSpaceBelow			(Widget);
static int			ScreenGetSpaceAbove			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Rep type registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			ToolTipShellRegisterRepTypes	(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShell resources											*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Tool tip resources */
	{
		XmNtoolTipLabel,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolTipShellRec , xfe_tool_tip_shell . tool_tip_label),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNfontList,
		XmCFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfeToolTipShellRec , xfe_tool_tip_shell . font_list),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNtoolTipTimeout,
		XmCToolTipTimeout,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeToolTipShellRec , xfe_tool_tip_shell . tool_tip_timeout),
		XmRImmediate, 
 		(XtPointer) XfeDEFAULT_TOOL_TIP_TIMEOUT
    },

	/* Enumeration resources */
    { 
		XmNtoolTipType,
		XmCToolTipType,
		XmRToolTipType,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolTipShellRec , xfe_tool_tip_shell . tool_tip_type),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNtoolTipPlacement,
		XmCToolTipPlacement,
		XmRToolTipPlacement,
		sizeof(unsigned char),
		XtOffsetOf(XfeToolTipShellRec , xfe_tool_tip_shell . tool_tip_placement),
		XmRImmediate, 
		(XtPointer) XmTOOL_TIP_PLACE_BOTTOM
    },

	/* Offset resources */
    { 
		XmNtoolTipHorizontalOffset,
		XmCOffset,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeToolTipShellRec , xfe_tool_tip_shell . horizontal_offset),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNtoolTipVerticalOffset,
		XmCOffset,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeToolTipShellRec , xfe_tool_tip_shell . vertical_offset),
		XmRImmediate, 
		(XtPointer) 0
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShell widget class record initialization					*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(tooltipshell,ToolTipShell) =
{
    {
		(WidgetClass) &xfeBypassShellClassRec,	/* superclass			*/
		"XfeToolTipShell",						/* class_name			*/
		sizeof(XfeToolTipShellRec),				/* widget_size			*/
		ClassInitialize,						/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		False,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		NULL,									/* actions            	*/
		0,										/* num_actions        	*/
		resources,                              /* resources			*/
		XtNumber(resources),                    /* num_resources		*/
		NULLQUARK,								/* xrm_class			*/
		True,									/* compress_motion		*/
		XtExposeCompressMaximal,				/* compress_exposure	*/
		True,									/* compress_enterleave	*/
		False,									/* visible_interest		*/
		Destroy,								/* destroy				*/
		XtInheritResize,						/* resize				*/
		XtInheritExpose,						/* expose				*/
		SetValues,                              /* set_values			*/
		NULL,                                   /* set_values_hook		*/
		NULL,									/* set_values_almost	*/
		NULL,									/* get_values_hook		*/
		NULL,                                   /* access_focus			*/
		XtVersion,                              /* version				*/
		NULL,                                   /* callback_private		*/
		XtInheritTranslations,					/* tm_table				*/
		NULL,									/* query_geometry		*/
		NULL,									/* display accelerator	*/
		NULL,									/* extension			*/
    },
    
    /* Composite Part */
    {
		XtInheritGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },

    /* Shell */
    {
		NULL,									/* extension			*/
    },

    /* WMShell */
    {
		NULL,									/* extension			*/
    },

    /* VendorShell */
    {
		NULL,									/* extension			*/
    },

    /* XfeBypassShell Part */
    {
		NULL,									/* extension			*/
    },

    /* XfeToolShell Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Rep type registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ToolTipShellRegisterRepTypes(void)
{
    static String place_names[] = 
    { 
		"tool_tip_place_bottom",
		"tool_tip_place_left",
		"tool_tip_place_right",
		"tool_tip_place_top",
        NULL
    };

    static String tip_names[] = 
    { 
		"tool_tip_editable",
		"tool_tip_read_only",
        NULL
    };

    XfeRepTypeRegister(XmRToolTipPlacement,place_names);
    XfeRepTypeRegister(XmRToolTipType,tip_names);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeToolTipShellWidgetClass declaration.								*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(tooltipshell,ToolTipShell);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassInitialize()
{
	/* Register XfeToolTip Representation Types */
    ToolTipShellRegisterRepTypes();
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeToolTipShellPart *		tp = _XfeToolTipShellPart(nw);

    /* Make sure rep types are ok */
	XfeRepTypeCheck(nw,XmRToolTipType,&tp->tool_tip_type,
					0);

	XfeRepTypeCheck(nw,XmRToolTipPlacement,&tp->tool_tip_type,
					0);

	/* Make sure read-only resources aren't set */
	if (tp->tool_tip_label)
	{
		_XmWarning(nw,MESSAGE2);

		tp->tool_tip_label = NULL;
	}

    /* Create components */
	tp->tool_tip_label		= LabelCreate(nw);

    /* Initialize private members */
	tp->tool_tip_timer_id		= 0;

	/* Manage the children */
	XtManageChild(tp->tool_tip_label);

/*  	XfeOverrideTranslations(nw,_XfeToolTipShellExtraTranslations); */
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfeToolTipShellPart *		tp = _XfeToolTipShellPart(w); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeToolTipShellPart *		np = _XfeToolTipShellPart(nw);
    XfeToolTipShellPart *		op = _XfeToolTipShellPart(ow);

	/* XmNtoolTipWidget */
	if (np->tool_tip_label != op->tool_tip_label)
	{
		_XmWarning(nw,MESSAGE2);

		np->tool_tip_label = op->tool_tip_label;
	}

	/* XmNtoolTipType */
	if (np->tool_tip_type != op->tool_tip_type)
	{
	}

	/* XmNtoolTipPlacement */
	if (np->tool_tip_placement != op->tool_tip_placement)
	{
	}

	/* XmNfontList */
	if (np->font_list != op->font_list)
	{
	}

    return False;
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
/*     XfeToolTipShellPart *		tp = _XfeToolTipShellPart(w); */
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
/* XfeToolTipShell action procedures									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ActionPopup(Widget item,XEvent * event,char ** params,Cardinal * nparams)
{
	Widget				w = XfeIsToolTipShell(item) ? item : _XfeParent(item);
    XfeToolTipShellPart *	tp = _XfeToolTipShellPart(w);

}
/*----------------------------------------------------------------------*/
static void
ActionPopdown(Widget item,XEvent * event,char ** params,Cardinal * nparams)
{
	Widget				w = XfeIsToolTipShell(item) ? item : _XfeParent(item);
    XfeToolTipShellPart *	tp = _XfeToolTipShellPart(w);

}
/*----------------------------------------------------------------------*/
static void
ActionHighlight(Widget item,XEvent * event,char ** params,Cardinal * nparams)
{
	Widget w = XfeIsToolTipShell(item) ? item : _XfeParent(item);

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* ToolTipShell widget functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget
LabelCreate(Widget w)
{
    XfeToolTipShellPart *	tp = _XfeToolTipShellPart(w);
	Widget				widget = NULL;
	Arg					av[20];
	Cardinal			ac = 0;

/*   	XtSetArg(av[ac],XmNbackground,			_XfeBackgroundPixel(w)); ac++; */
/* 	XtSetArg(av[ac],XmNforeground,			tp->title_foreground); ac++; */
/*    	XtSetArg(av[ac],XmNalignment,			tp->title_alignment); ac++; */

/* 	XtSetArg(av[ac],XmNshadowThickness,			0); ac++; */
/* 	XtSetArg(av[ac],XmNstringDirection,			tp->title_direction); ac++; */
	XtSetArg(av[ac],XmNraiseBorderThickness,	0); ac++;
	XtSetArg(av[ac],XmNraiseOnEnter,			False); ac++;
	XtSetArg(av[ac],XmNfillOnArm,				False); ac++;
	XtSetArg(av[ac],XmNarmOffset,				0); ac++;
	XtSetArg(av[ac],XmNusePreferredWidth,		True); ac++;
	XtSetArg(av[ac],XmNusePreferredHeight,		True); ac++;
	XtSetArg(av[ac],XmNbuttonType,				XmBUTTON_NONE); ac++;

	if (tp->font_list != NULL)
	{
		XtSetArg(av[ac],XmNfontList,		tp->font_list); ac++;
	}

	widget = XtCreateManagedWidget(TOOL_TIP_LABEL_NAME,
							xfeButtonWidgetClass,
							w,
							av,
							ac);

/*  	XtAddCallback(widget,XmNactivateCallback,TitleActivateCB,w); */

	return widget;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShell Public Methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateToolTipShell(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreatePopupShell(name,xfeToolTipShellWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeToolTipShellGetLabel(Widget w)
{
    XfeToolTipShellPart *	tp = _XfeToolTipShellPart(w);

	assert( XfeIsToolTipShell(w) );

	assert( _XfeIsAlive(tp->tool_tip_label) );

	return tp->tool_tip_label;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToolTipShellSetString(Widget w,XmString string)
{
    XfeToolTipShellPart *	tp = _XfeToolTipShellPart(w);
    XfeBypassShellPart *	bp = _XfeBypassShellPart(w);
	Dimension				extra_thickness;

	assert( XfeIsToolTipShell(w) );
	assert( _XfeIsAlive(w) );
	assert( _XfeIsAlive(tp->tool_tip_label) );

	XtVaSetValues(tp->tool_tip_label,XmNlabelString,string,NULL);

	extra_thickness = 
		_XfeBorderWidth(tp->tool_tip_label) +
		2 * bp->shadow_thickness;

	_XfeResizeWidget(w,
					 _XfeWidth(tp->tool_tip_label) + extra_thickness,
					 _XfeHeight(tp->tool_tip_label) + extra_thickness);

}
/*----------------------------------------------------------------------*/
