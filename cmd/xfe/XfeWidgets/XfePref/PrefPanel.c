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
/* Name:		<Xfe/PrefPanel.c>										*/
/* Description:	XfePrefPanel widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/PrefPanelP.h>
#include <Xm/Label.h>

#define MESSAGE1 "Widget is not an XfePrefPanel."
#define MESSAGE2 "XmNtitleDirection is not XmSTRING_DIRECTION_L_TO_R or XmSTRING_DIRECTION_R_TO_L."

#define DEFAULT_TITLE_NAME			"PrefPanelTitle"
#define DEFAULT_SUB_TITLE_NAME		"PrefPanelSubTitle"

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

static Widget		SubTitleCreate	(Widget);
static Widget		TitleCreate		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanel resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
	/* Color resources */
	{
		XmNtitleBackground,
		XmCTitleBackground,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_background),
		XmRCallProc, 
		(XtPointer) _XfeCallProcCopyForeground
	},
	{
		XmNtitleForeground,
		XmCTitleForeground,
		XmRPixel,
		sizeof(Pixel),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_foreground),
		XmRCallProc, 
		(XtPointer) _XfeCallProcCopyBackground
	},

	/* i18n resources */
    { 
		XmNtitleDirection,
		XmCTitleDirection,
		XmRStringDirection,
		sizeof(unsigned char),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_direction),
		XmRImmediate, 
		(XtPointer) XmSTRING_DIRECTION_L_TO_R
    },
    { 
		XmNtitleSpacing,
		XmCTitleSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_spacing),
		XmRImmediate, 
		(XtPointer) 20
    },

	/* Title resources */
	{
		XmNtitle,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNtitleAlignment,
		XmCTitleAlignment,
		XmRAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_alignment),
		XmRImmediate, 
		(XtPointer) XmALIGNMENT_BEGINNING
    },
    { 
		XmNtitleFontList,
		XmCTitleFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_font_list),
		XmRCallProc, 
		(XtPointer) _XfeCallProcDefaultLabelFontList
    },
    { 
		XmNtitleString,
		XmCTitleString,
		XmRXmString,
		sizeof(XmString),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_string),
		XmRImmediate, 
		(XtPointer) NULL
    },

	/* Sub title resources */
	{
		XmNsubTitle,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . sub_title),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNsubTitleAlignment,
		XmCSubTitleAlignment,
		XmRAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . sub_title_alignment),
		XmRImmediate, 
		(XtPointer) XmALIGNMENT_END
    },
    { 
		XmNsubTitleFontList,
		XmCSubTitleFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . sub_title_font_list),
		XmRCallProc, 
		(XtPointer) _XfeCallProcDefaultLabelFontList
    },
    { 
		XmNsubTitleString,
		XmCSubTitleString,
		XmRXmString,
		sizeof(XmString),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . sub_title_string),
		XmRImmediate, 
		(XtPointer) NULL
    },

	/* Widget names */
    { 
		XmNtitleWidgetName,
		XmCReadOnly,
		XmRString,
		sizeof(String),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_widget_name),
		XmRString, 
		"PrefPanelTitle"
    },
    { 
		XmNsubTitleWidgetName,
		XmCReadOnly,
		XmRString,
		sizeof(String),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . sub_title_widget_name),
		XmRString, 
		"PrefPanelSubTitle"
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanel synthetic resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
	{ 
		XmNtitleSpacing,
		sizeof(Dimension),
		XtOffsetOf(XfePrefPanelRec , xfe_pref_panel . title_spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(prefpanel,PrefPanel) =
{
    {
		(WidgetClass) &xfeManagerClassRec,		/* superclass			*/
		"XfePrefPanel",							/* class_name			*/
		sizeof(XfePrefPanelRec),				/* widget_size			*/
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
		XtInheritInsertChild,					/* insert_static_child			*/
		XtInheritDeleteChild,					/* delete_static_child			*/
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

    /* XfePrefPanel Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfePrefPanelWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(prefpanel,PrefPanel);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfePrefPanelPart *		pp = _XfePrefPanelPart(nw);

    /* Create private components */
	pp->title		= TitleCreate(nw);
	pp->sub_title	= SubTitleCreate(nw);

	/* Initialize private members */

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfePrefPanelWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfePrefPanelPart *		pp = _XfePrefPanelPart(w); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
/*     XfePrefPanelPart *		np = _XfePrefPanelPart(nw); */
/*     XfePrefPanelPart *		op = _XfePrefPanelPart(ow); */

	/* shadow_thickness */
	if (_XfemShadowThickness(nw) != _XfemShadowThickness(ow))
	{
		_XfemConfigFlags(nw) |= XfeConfigLE;
	}
	

    return _XfeManagerChainSetValues(ow,rw,nw,xfePrefPanelWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
/*     XfePrefPanelPart *		pp = _XfePrefPanelPart(w); */
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
/*     XfePrefPanelPart *		pp = _XfePrefPanelPart(w); */

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
/*     XfePrefPanelPart *		pp = _XfePrefPanelPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteStaticChild(Widget child)
{
/*     Widget					w = XtParent(child); */
/*     XfePrefPanelPart *		pp = _XfePrefPanelPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
    XfePrefPanelPart *	pp = _XfePrefPanelPart(w);
	int					title_x;
	int					sub_title_x;
	int					title_width = 0;
	int					sub_title_width = 0;
	int					max_height = 0;

	Widget				title = 
		_XfeChildIsShown(pp->title) ? pp->title : NULL;

	Widget				sub_title = 
		_XfeChildIsShown(pp->sub_title) ? pp->sub_title : NULL;

	/* Determine the max height */
	if (title)
	{
		max_height = _XfeHeight(title);
	}

	if (sub_title)
	{
		max_height = XfeMax(max_height,_XfeHeight(sub_title));
	}

	/* Left to right */
	if (pp->title_direction == XmSTRING_DIRECTION_L_TO_R)
	{
		/* Configure the title */
		if (title)
		{
			title_x		= _XfemBoundaryX(w);
			title_width	= _XfeWidth(title);

            printf("title_width = %d\n",title_width);
		}
		
		/* Configure the sub title */
		if (sub_title)
		{
			sub_title_width = _XfeWidth(sub_title);

			sub_title_x = 
				_XfeWidth(w) - 
				_XfemOffsetRight(w) - 
				sub_title_width;
		}
	}
	/* Right to left */
	else if (pp->title_direction == XmSTRING_DIRECTION_R_TO_L)
	{
		/* Configure the title */
		if (title)
		{
			title_x	= _XfeWidth(w) - _XfemOffsetRight(w) - _XfeWidth(title);
			title_width	= _XfeWidth(title);
		}
		
		/* Configure the sub title */
		if (sub_title)
		{
			sub_title_x = _XfemBoundaryX(w);
			sub_title_width = title_x - sub_title_x;
		}
	}
	else
	{
		_XfeWarning(w,MESSAGE2);
	}

	if (title_width > 0)
	{
		_XfeConfigureWidget(title,
							title_x,
							_XfemBoundaryY(w),
							title_width,
							max_height);
	}

	if (sub_title_width > 0)
	{
		_XfeConfigureWidget(sub_title,
							sub_title_x,
							_XfemBoundaryY(w),
							sub_title_width,
							max_height);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutStaticChildren(Widget w)
{
/*     XfePrefPanelPart *	pp = _XfePrefPanelPart(w); */
	
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
/*     XfePrefPanelPart *	pp = _XfePrefPanelPart(w); */

}
/*----------------------------------------------------------------------*/

static Widget
TitleCreate(Widget w)
{
    XfePrefPanelPart *	pp = _XfePrefPanelPart(w);
	Widget				title = NULL;
	Arg					av[20];
	Cardinal			ac = 0;

	XtSetArg(av[ac],XmNbackground,			pp->title_background); ac++;
	XtSetArg(av[ac],XmNforeground,			pp->title_foreground); ac++;
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
static Widget
SubTitleCreate(Widget w)
{
    XfePrefPanelPart *	pp = _XfePrefPanelPart(w);
	Widget				sub_title = NULL;
	Arg					av[20];
	Cardinal			ac = 0;

	XtSetArg(av[ac],XmNbackground,			pp->title_background); ac++;
	XtSetArg(av[ac],XmNforeground,			pp->title_foreground); ac++;
	XtSetArg(av[ac],XmNshadowThickness,		0); ac++;
	XtSetArg(av[ac],XmNstringDirection,		pp->title_direction); ac++;
 	XtSetArg(av[ac],XmNalignment,			pp->sub_title_alignment); ac++;

	if (pp->sub_title_font_list != NULL)
	{
		XtSetArg(av[ac],XmNfontList,		pp->sub_title_font_list); ac++;
	}

	sub_title = XtCreateManagedWidget(pp->sub_title_widget_name,
									  xmLabelWidgetClass,
									  w,
									  av,ac);

	return sub_title;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanel Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreatePrefPanel(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfePrefPanelWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
