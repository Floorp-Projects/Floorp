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
/* Name:		<Xfe/PrefGroup.c>										*/
/* Description:	XfePrefGroup widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/PrefGroupP.h>
#include <Xm/Label.h>
#include <Xm/Frame.h>

#define MESSAGE1 "Widget is not an XfePrefGroup."
#define MESSAGE2 "XmNtitleDirection is not XmSTRING_DIRECTION_L_TO_R or XmSTRING_DIRECTION_R_TO_L."

#define DEFAULT_TITLE_NAME			"PrefGroupTitle"
#define DEFAULT_SUB_TITLE_NAME		"PrefGroupSubTitle"

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
static Boolean	AcceptStaticChild			(Widget);
static Boolean	InsertStaticChild			(Widget);
static Boolean	DeleteStaticChild			(Widget);
static void		LayoutComponents	(Widget);
static void		LayoutStaticChildren		(Widget);
static void		DrawComponents		(Widget,XEvent *,Region,XRectangle *);

static Widget	TitleCreate			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroup resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
	/* Frame resources */
	{ 
		XmNframeType,
		XmCFrameType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . frame_type),
		XmRImmediate, 
		(XtPointer) XmSHADOW_ETCHED_IN
	},
	{ 
		XmNframeThickness,
		XmCFrameThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . frame_thickness),
		XmRImmediate, 
		(XtPointer) 2
	},

	/* Title resources */
	{
		XmNtitle,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNtitleAlignment,
		XmCTitleAlignment,
		XmRAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title_alignment),
		XmRImmediate, 
		(XtPointer) XmALIGNMENT_BEGINNING
    },
    { 
		XmNtitleDirection,
		XmCTitleDirection,
		XmRStringDirection,
		sizeof(unsigned char),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title_direction),
		XmRImmediate, 
		(XtPointer) XmSTRING_DIRECTION_L_TO_R
    },
    { 
		XmNtitleFontList,
		XmCTitleFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title_font_list),
		XmRCallProc, 
		(XtPointer) _XfeCallProcDefaultLabelFontList
    },
	{ 
		XmNtitleOffset,
		XmCTitleOffset,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title_offset),
		XmRImmediate, 
		(XtPointer) 20
	},
    { 
		XmNtitleString,
		XmCTitleString,
		XmRXmString,
		sizeof(XmString),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title_string),
		XmRImmediate, 
		(XtPointer) NULL
    },


	/* Widget names */
    { 
		XmNtitleWidgetName,
		XmCReadOnly,
		XmRString,
		sizeof(String),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title_widget_name),
		XmRString, 
		"PrefGroupTitle"
    },
    { 
		XmNframeWidgetName,
		XmCReadOnly,
		XmRString,
		sizeof(String),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . frame_widget_name),
		XmRString, 
		"PrefGroupFrame"
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroup synthetic resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
	{ 
		XmNtitleOffset,
		sizeof(Dimension),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title_offset),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
	{ 
		XmNtitleSpacing,
		sizeof(Dimension),
		XtOffsetOf(XfePrefGroupRec , xfe_pref_group . title_spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(prefgroup,PrefGroup) =
{
    {
		(WidgetClass) &xfeManagerClassRec,		/* superclass			*/
		"XfePrefGroup",							/* class_name			*/
		sizeof(XfePrefGroupRec),				/* widget_size			*/
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
		synthetic_resources,					/* syn resources		*/
		XtNumber(synthetic_resources),			/* num syn_resources	*/
		NULL,                                   /* syn_cont_resources  	*/
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
		AcceptStaticChild,						/* accept_static_child		*/
		InsertStaticChild,						/* insert_static_child		*/
		DeleteStaticChild,						/* delete_static_child		*/
		LayoutStaticChildren,					/* layout_static_children	*/
		NULL,									/* change_managed			*/
		NULL,									/* prepare_components		*/
		LayoutComponents,						/* layout_components		*/
		NULL,									/* draw_background			*/
		XfeInheritDrawShadow,					/* draw_shadow				*/
		DrawComponents,							/* draw_components			*/
		NULL,									/* extension				*/
    },

    /* XfePrefGroup Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfePrefGroupWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(prefgroup,PrefGroup);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfePrefGroupPart *		pp = _XfePrefGroupPart(nw);

    /* Create private components */
	pp->title		= TitleCreate(nw);

	/* Initialize private members */

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfePrefGroupWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfePrefGroupPart *		pp = _XfePrefGroupPart(w); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
/*     XfePrefGroupPart *		np = _XfePrefGroupPart(nw); */
/*     XfePrefGroupPart *		op = _XfePrefGroupPart(ow); */

	/* shadow_thickness */
	if (_XfemShadowThickness(nw) != _XfemShadowThickness(ow))
	{
		_XfemConfigFlags(nw) |= XfeConfigLE;
	}
	

    return _XfeManagerChainSetValues(ow,rw,nw,xfePrefGroupWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
/*     XfePrefGroupPart *		pp = _XfePrefGroupPart(w); */
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
/*     XfePrefGroupPart *		pp = _XfePrefGroupPart(w); */

}
/*----------------------------------------------------------------------*/
static Boolean
AcceptStaticChild(Widget child)
{
	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
InsertStaticChild(Widget child)
{
/*     Widget					w = XtParent(child); */
/*     XfePrefGroupPart *		pp = _XfePrefGroupPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteStaticChild(Widget child)
{
/*     Widget					w = XtParent(child); */
/*     XfePrefGroupPart *		pp = _XfePrefGroupPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static void
LayoutStaticChildren(Widget w)
{
/*     XfePrefGroupPart *	pp = _XfePrefGroupPart(w); */
	
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
    XfePrefGroupPart *	pp = _XfePrefGroupPart(w);

	if (_XfeChildIsShown(pp->title))
	{
		int x;
		int y;

		/* Left to right */
		if (pp->title_direction == XmSTRING_DIRECTION_L_TO_R)
		{
			x = _XfemBoundaryX(w) + pp->title_offset;
		}
		/* Right to left */
		else if (pp->title_direction == XmSTRING_DIRECTION_R_TO_L)
		{
			x = _XfeWidth(w) - 
				_XfemOffsetRight(w) - 
				_XfeWidth(pp->title) -
				pp->title_offset;
		}
		else
		{
			_XfeWarning(w,MESSAGE2);
		}
		
		_XfeConfigureWidget(pp->title,
							x,
							y,
							_XfeWidth(pp->title),
							_XfeHeight(pp->title));

		if (_XfeIsRealized(pp->title))
		{
			XRaiseWindow(XtDisplay(pp->title),_XfeWindow(pp->title));
		}
	}
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    XfePrefGroupPart *	pp = _XfePrefGroupPart(w);
	Position			x;
	Position			y;
	Dimension			width;
	Dimension			height;

	/* Make sure the frame has at least a thickness of 1 */
	if (pp->frame_thickness < 1)
 	{
 		return;
 	}

	x = _XfemOffsetLeft(w);
	y = _XfemOffsetTop(w);

	width = _XfemBoundaryWidth(w);
	height = _XfemBoundaryHeight(w);

	/* The frame should render right through the child */
	if (_XfeChildIsShown(pp->title))
	{
		int dy = (_XfeHeight(pp->title) / 2) - (pp->frame_thickness / 2);

		y += dy;
		
		height -= dy;
	}

    /* Draw the shadow */
    _XmDrawShadows(XtDisplay(w),
				   _XfeWindow(w),
				   _XfemTopShadowGC(w),_XfemBottomShadowGC(w),
				   x,y,
				   width,height,
				   pp->frame_thickness,
				   pp->frame_type);
}
/*----------------------------------------------------------------------*/

static Widget
TitleCreate(Widget w)
{
    XfePrefGroupPart *	pp = _XfePrefGroupPart(w);
	Widget				title = NULL;
	Arg					av[20];
	Cardinal			ac = 0;

#if 0
 	XtSetArg(av[ac],XmNbackground,			_XfeBackgroundPixel(w)); ac++;
 	XtSetArg(av[ac],XmNforeground,			_XfemForeground(w)); ac++;
#endif
	XtSetArg(av[ac],XmNshadowThickness,		0); ac++;
	XtSetArg(av[ac],XmNstringDirection,		pp->title_direction); ac++;
 	XtSetArg(av[ac],XmNalignment,			pp->title_alignment); ac++;

	if (pp->title_font_list != NULL)
	{
		XtSetArg(av[ac],XmNfontList,		pp->title_font_list); ac++;
	}

	title = XtCreateManagedWidget(pp->title_widget_name,
								  xmLabelWidgetClass,
								  w,
								  av,ac);

	return title;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroup Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreatePrefGroup(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfePrefGroupWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
