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
/* Name:		<Xfe/TempTwo.c>											*/
/* Description:	XfeTempTwo widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/TempTwoP.h>

/* Possible children classes */
#include <Xfe/Label.h>
#include <Xfe/Button.h>
#include <Xfe/Cascade.h>

#include <Xm/Separator.h>
#include <Xm/SeparatoG.h>

#define MESSAGE1 "Widget is not an XfeTempTwo."
#define MESSAGE2 "XfeTemptwo can only have XfeButton & XmSeparator children."

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassPartInitialize	(WidgetClass);
static void 	Initialize			(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy				(Widget);
static Boolean	SetValues			(Widget,Widget,Widget,ArgList,Cardinal *);
static void		GetValuesHook		(Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ConstraintInitialize(Widget,Widget,ArgList,Cardinal *);
static Boolean	ConstraintSetValues	(Widget,Widget,Widget,ArgList,Cardinal *);
static void 	ConstraintDestroy	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static Boolean	AcceptChild			(Widget);
static Boolean	InsertChild			(Widget);
static Boolean	DeleteChild			(Widget);
static void		ChangeManaged		(Widget);
static void		PrepareComponents	(Widget,int);
static void		LayoutComponents	(Widget);
static void		LayoutChildren		(Widget);
static void		DrawComponents		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	LayoutString			(Widget);
static void	DrawString				(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	BtnUp				(Widget,XEvent *,char **,Cardinal *);
static void 	BtnDown				(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Resource callprocs													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	DefaultSashColor		(Widget,int,XrmValue *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Cursor resources */
	{
		XmNhorizontalCursor,
		XmCHorizontalCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . horizontal_cursor),
		XmRString, 
		"sb_h_double_arrow"
	},
	{
		XmNverticalCursor,
		XmCVerticalCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . vertical_cursor),
		XmRString, 
		"sb_v_double_arrow"
	},

    /* Color resources */
	{
		XmNsashColor,
		XmCSashColor,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . sash_color),
		XmRCallProc, 
		(XtPointer) _XfeCallProcCopyForeground
	},


    { 
		XmNseparatorHeight,
		XmCSeparatorHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . separator_height),
		XmRImmediate, 
		(XtPointer) 4
    },
    { 
		XmNseparatorThickness,
		XmCSeparatorThickness,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . separator_thickness),
		XmRImmediate, 
		(XtPointer) 50
    },
    { 
		XmNseparatorType,
		XmCSeparatorType,
		XmRSeparatorType,
		sizeof(unsigned char),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . separator_type),
		XmRImmediate, 
		(XtPointer) XmSHADOW_ETCHED_IN
    },
    { 
		XmNseparatorWidth,
		XmCSeparatorWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . separator_width),
		XmRImmediate, 
		(XtPointer) 4
    },
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . spacing),
		XmRImmediate, 
		(XtPointer) 1
    },

    { 
		XmNbuttonLayout,
		XmCButtonLayout,
		XmRButtonLayout,
		sizeof(unsigned char),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . button_layout),
		XmRImmediate, 
		(XtPointer) XmBUTTON_LABEL_ONLY
    },
    { 
		XmNorientation,
		XmCOrientation,
		XmROrientation,
		sizeof(unsigned char),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . orientation),
		XmRImmediate, 
		(XtPointer) XmHORIZONTAL
    },

    { 
		XmNraised,
		XmCRaised,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . raised),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNraiseBorderThickness,
		XmCRaiseBorderThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . raise_border_thickness),
		XmRImmediate, 
		(XtPointer) 0
    },
	{
		XmNchildUsePreferredHeight,
		XmCChildUsePreferredHeight,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . child_use_pref_height),
		XmRImmediate, 
		(XtPointer) False
	},
	{
		XmNchildUsePreferredWidth,
		XmCChildUsePreferredWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . child_use_pref_width),
		XmRImmediate, 
		(XtPointer) False
	},

    /* Force all the margins to 0 */
    { 
		XmNmarginBottom,
		XmCMarginBottom,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_manager . margin_bottom),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_manager . margin_left),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_manager . margin_right),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_manager . margin_top),
		XmRImmediate, 
		(XtPointer) 0
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo synthetic resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
	{ 
		XmNspacing,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
    { 
		XmNraiseBorderThickness,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . raise_border_thickness),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
	{ 
		XmNseparatorHeight,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . separator_height),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
	},
	{ 
		XmNseparatorWidth,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoRec , xfe_temp_two . separator_width),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo constraint resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource constraint_resources[] = 
{
    { 
		XmNmaxHeight,
		XmCMaxHeight,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoConstraintRec , xfe_temp_two . max_height),
		XmRImmediate,
		(XtPointer) 65535
    },
    { 
		XmNmaxWidth,
		XmCMaxWidth,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoConstraintRec , xfe_temp_two . max_width),
		XmRImmediate,
		(XtPointer) 65535
    },
    { 
		XmNminHeight,
		XmCMinHeight,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoConstraintRec , xfe_temp_two . min_height),
		XmRImmediate,
		(XtPointer) 0
    },
    { 
		XmNminWidth,
		XmCMinWidth,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoConstraintRec , xfe_temp_two . min_width),
		XmRImmediate,
		(XtPointer) 0
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo constraint synthetic resources							*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource constraint_synthetic_resources[] =
{
    { 
		XmNmaxWidth,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoConstraintRec , xfe_temp_two . max_width),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNmaxWidth,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoConstraintRec , xfe_temp_two . max_width),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNmaxHeight,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoConstraintRec , xfe_temp_two . max_height),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
    },
    { 
		XmNmaxHeight,
		sizeof(Dimension),
		XtOffsetOf(XfeTempTwoConstraintRec , xfe_temp_two . max_height),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "BtnDown",			BtnDown				},
    { "BtnUp",				BtnUp				},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(temptwo,TempTwo) =
{
    {
		(WidgetClass) &xfeManagerClassRec,		/* superclass			*/
		"XfeTempTwo",							/* class_name			*/
		sizeof(XfeTempTwoRec),					/* widget_size			*/
		NULL,									/* class_initialize		*/
		ClassPartInitialize,					/* class_part_initialize*/
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
		constraint_resources,					/* syn resources		*/
		XtNumber(constraint_resources),			/* num syn_resources	*/
		sizeof(XfeTempTwoConstraintRec),		/* constraint size		*/
		ConstraintInitialize,					/* init proc			*/
		ConstraintDestroy,						/* destroy proc			*/
		ConstraintSetValues,					/* set values proc		*/
		NULL,                                   /* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		synthetic_resources,					/* syn resources		*/
		XtNumber(synthetic_resources),			/* num syn_resources	*/
		constraint_synthetic_resources,			/* syn resources		*/
		XtNumber(constraint_synthetic_resources),/* num syn_resources	*/
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
		ChangeManaged,							/* change_managed		*/
		PrepareComponents,						/* prepare_components	*/
		LayoutComponents,						/* layout_components	*/
		LayoutChildren,							/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		DrawComponents,							/* draw_components		*/
		NULL,									/* extension			*/
    },

    /* XfeTempTwo Part */
    {
		LayoutString,							/* layout_string		*/
		DrawString,								/* draw_string			*/
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeTempTwoWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(temptwo,TempTwo);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo resource call procedures									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DefaultSashColor(Widget w,int offset,XrmValue * value)
{
	XfeTempTwoPart *		ttp = _XfeTempTwoPart(w);
    static Pixel			sash_color;

	sash_color = _XfemForeground(w);
   
    value->addr = (XPointer) &sash_color;
    value->size = sizeof(sash_color);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassPartInitialize(WidgetClass wc)
{
	XfeTempTwoWidgetClass cc = (XfeTempTwoWidgetClass) wc;
	XfeTempTwoWidgetClass sc = (XfeTempTwoWidgetClass) _XfeSuperClass(wc);

	/* Resolve inheritance of all XfeTempTwo class methods */
	_XfeResolve(cc,sc,xfe_temp_two_class,layout_string,XfeInheritLayoutString);
	_XfeResolve(cc,sc,xfe_temp_two_class,draw_string,XfeInheritDrawString);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeTempTwoPart *		ttp = _XfeTempTwoPart(nw);

    /* Initialize private members */
	ttp->temp_GC = XfeAllocateSelectionGc(nw,
										  _XfemShadowThickness(nw),
										  ttp->sash_color,
										  _XfeBackgroundPixel(nw));

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeTempTwoWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeTempTwoPart *		ttp = _XfeTempTwoPart(w);

	XtReleaseGC(w,ttp->temp_GC);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeTempTwoPart *		np = _XfeTempTwoPart(nw);
    XfeTempTwoPart *		op = _XfeTempTwoPart(ow);
	Boolean					update_sash_gc = False;

	/* shadow_thickness */
	if (_XfemShadowThickness(nw) != _XfemShadowThickness(ow))
	{
		_XfemConfigFlags(nw) |= XfeConfigLE;

		update_sash_gc = True;
	}
	
	/* sash_color */
	if (np->sash_color != op->sash_color)
	{
		_XfemConfigFlags(nw) |= XfeConfigLE;

		update_sash_gc = True;
	}

	/* Update the sash gc if needed */
	if (update_sash_gc)
	{
		XtReleaseGC(nw,np->temp_GC);

		np->temp_GC = XfeAllocateSelectionGc(nw,
											 _XfemShadowThickness(nw),
											 np->sash_color,
											 _XfeBackgroundPixel(nw));
	}

    return _XfeManagerChainSetValues(ow,rw,nw,xfeTempTwoWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
    XfeTempTwoPart *		ttp = _XfeTempTwoPart(w);
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
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ConstraintInitialize(Widget rc,Widget nc,ArgList args,Cardinal *nargs)
{
 	Widget						w = _XfeParent(nc);
    XfeTempTwoPart *			ttp = _XfeTempTwoPart(w);

	/* Finish constraint initialization */
	_XfeConstraintChainInitialize(rc,nc,xfeTempTwoWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
ConstraintDestroy(Widget c)
{
 	Widget						w = XtParent(c);
    XfeTempTwoPart *			ttp = _XfeTempTwoPart(w);
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValues(Widget oc,Widget rc,Widget nc,ArgList args,Cardinal *nargs)
{
	Widget						w = XtParent(nc);
    XfeTempTwoPart *			ttp = _XfeTempTwoPart(w);

 	XfeTempTwoConstraintPart *	ncp = _XfeTempTwoConstraintPart(nc);
 	XfeTempTwoConstraintPart *	ocp = _XfeTempTwoConstraintPart(oc);

	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(oc,rc,nc,xfeTempTwoWidgetClass);
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
    XfeTempTwoPart *		ttp = _XfeTempTwoPart(w);

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
    XfeTempTwoPart *		ttp = _XfeTempTwoPart(w);

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteChild(Widget child)
{
    Widget					w = XtParent(child);
    XfeTempTwoPart *		ttp = _XfeTempTwoPart(w);

	return True;
}
/*----------------------------------------------------------------------*/
static void
ChangeManaged(Widget w)
{
    XfeTempTwoPart *		ttp = _XfeTempTwoPart(w);
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
    XfeTempTwoPart *	ttp = _XfeTempTwoPart(w);
	
}
/*----------------------------------------------------------------------*/
static void
LayoutChildren(Widget w)
{
    XfeTempTwoPart *	ttp = _XfeTempTwoPart(w);
	
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeTempTwoPart *	ttp = _XfeTempTwoPart(w);

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutString(Widget w)
{
    XfeTempTwoPart *	ttp = _XfeTempTwoPart(w);

}
/*----------------------------------------------------------------------*/
static void
DrawString(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfeTempTwoPart *	ttp = _XfeTempTwoPart(w);

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
BtnDown(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeTempTwoPart *	ttp = _XfeTempTwoPart(w);
}
/*----------------------------------------------------------------------*/
static void
BtnUp(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
    XfeTempTwoPart *	ttp = _XfeTempTwoPart(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTempTwo Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateTempTwo(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfeTempTwoWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
