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
/* Name:		<Xfe/Caption.c>											*/
/* Description:	XfeCaption widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/CaptionP.h>
#include <Xfe/Button.h>

#define MESSAGE1 "Widget is not an XfeCaption."
#define MESSAGE2 "XmNtitleDirection is not XmSTRING_DIRECTION_L_TO_R or XmSTRING_DIRECTION_R_TO_L."

#define TITLE_NAME						"CaptionTitle"
#define DEFAULT_SUB_TITLE_NAME			"CaptionSubTitle"

#define DEFAULT_MAX_CHILD_WIDTH			65535
#define DEFAULT_MAX_CHILD_HEIGHT		65535

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ClassInitialize		(void);
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
static void		PreferredGeometry		(Widget,Dimension *,Dimension *);
static Boolean	AcceptStaticChild		(Widget);
static Boolean	InsertStaticChild		(Widget);
static Boolean	DeleteStaticChild		(Widget);
static void		LayoutStaticChildren	(Widget);
static void		DrawComponents			(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Activate			(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Title functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget	TitleCreate			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Title callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			TitleActivateCB	(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* Layout functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutChildOnBottom		(Widget);
static void		LayoutChildOnLeft		(Widget);
static void		LayoutChildOnRight		(Widget);
static void		LayoutChildOnTop		(Widget);
static void		LayoutTitleOnly			(Widget);
static void		LayoutChildOnly			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* RepType registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		CaptionRegisterRepTypes				(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{
	/* Layout resources */
    { 
		XmNcaptionLayout,
		XmCCaptionLayout,
		XmRCaptionLayout,
		sizeof(unsigned char),
		XtOffsetOf(XfeCaptionRec , xfe_caption . caption_layout),
		XmRImmediate, 
		(XtPointer) XmCAPTION_CHILD_ON_RIGHT
    },
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeCaptionRec , xfe_caption . spacing),
		XmRImmediate, 
		(XtPointer) 2
    },

	/* Title resources */
	{
		XmNtitle,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeCaptionRec , xfe_caption . title),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNtitleHorizontalAlignment,
		XmCCaptionHorizontalAlignment,
		XmRCaptionHorizontalAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfeCaptionRec , xfe_caption . title_hor_alignment),
		XmRImmediate, 
		(XtPointer) XmCAPTION_HORIZONTAL_ALIGNMENT_LEFT
    },
    { 
		XmNtitleVerticalAlignment,
		XmCCaptionVerticalAlignment,
		XmRCaptionVerticalAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfeCaptionRec , xfe_caption . title_ver_alignment),
		XmRImmediate, 
		(XtPointer) XmCAPTION_VERTICAL_ALIGNMENT_CENTER
    },
    { 
		XmNtitleDirection,
		XmCTitleDirection,
		XmRStringDirection,
		sizeof(unsigned char),
		XtOffsetOf(XfeCaptionRec , xfe_caption . title_direction),
		XmRImmediate, 
		(XtPointer) XmSTRING_DIRECTION_L_TO_R
    },
    { 
		XmNtitleFontList,
		XmCTitleFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfeCaptionRec , xfe_caption . title_font_list),
		XmRCallProc, 
		(XtPointer) _XfeCallProcDefaultLabelFontList
    },
    { 
		XmNtitleString,
		XmCTitleString,
		XmRXmString,
		sizeof(XmString),
		XtOffsetOf(XfeCaptionRec , xfe_caption . title_string),
		XmRImmediate, 
		(XtPointer) NULL
    },

	/* Child resources */
	{
		XmNchild,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeCaptionRec , xfe_caption . child),
		XmRImmediate, 
		(XtPointer) NULL
	},
    { 
		XmNchildHorizontalAlignment,
		XmCCaptionHorizontalAlignment,
		XmRCaptionHorizontalAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfeCaptionRec , xfe_caption . child_hor_alignment),
		XmRImmediate, 
		(XtPointer) XmCAPTION_HORIZONTAL_ALIGNMENT_LEFT
    },
    { 
		XmNchildVerticalAlignment,
		XmCCaptionVerticalAlignment,
		XmRCaptionVerticalAlignment,
		sizeof(unsigned char),
		XtOffsetOf(XfeCaptionRec , xfe_caption . child_ver_alignment),
		XmRImmediate, 
		(XtPointer) XmCAPTION_VERTICAL_ALIGNMENT_CENTER
    },
    { 
		XmNchildResize,
		XmCChildResize,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeCaptionRec , xfe_caption . child_resize),
		XmRImmediate, 
		(XtPointer) False
    },

	/* Dimension resources */
    { 
		XmNmaxChildHeight,
		XmCMaxChildHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeCaptionRec , xfe_caption . max_child_height),
		XmRImmediate, 
		(XtPointer) DEFAULT_MAX_CHILD_HEIGHT
    },
    { 
		XmNmaxChildWidth,
		XmCMaxChildWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeCaptionRec , xfe_caption . max_child_width),
		XmRImmediate, 
		(XtPointer) DEFAULT_MAX_CHILD_WIDTH
    },

};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption synthetic resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
	{ 
		XmNmaxChildHeight,
		sizeof(Dimension),
		XtOffsetOf(XfeCaptionRec , xfe_caption . max_child_height),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
	},
	{ 
		XmNmaxChildWidth,
		sizeof(Dimension),
		XtOffsetOf(XfeCaptionRec , xfe_caption . max_child_width),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
	{ 
		XmNspacing,
		sizeof(Dimension),
		XtOffsetOf(XfeCaptionRec , xfe_caption . spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption extra translations										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfeCaptionExtraTranslations[] ="\
<BtnUp>:					Activate()\n\
<BtnDown>:					Activate()";

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "Activate",				Activate				},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(caption,Caption) =
{
    {
		(WidgetClass) &xfeManagerClassRec,		/* superclass			*/
		"XfeCaption",							/* class_name			*/
		sizeof(XfeCaptionRec),					/* widget_size			*/
		ClassInitialize,						/* class_initialize		*/
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
		_XfeLiberalGeometryManager,				/* geometry_manager		*/
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
		XfeInheritUpdateBoundary,					/* update_boundary				*/
		XfeInheritUpdateChildrenInfo,			/* update_children_info		*/
		XfeInheritLayoutWidget,					/* layout_widget			*/
		AcceptStaticChild,						/* accept_static_child		*/
		InsertStaticChild,						/* insert_static_child		*/
		DeleteStaticChild,						/* delete_static_child		*/
		LayoutStaticChildren,					/* layout_static_children	*/
		NULL,									/* change_managed			*/
		NULL,									/* prepare_components		*/
		NULL,									/* layout_components		*/
		NULL,									/* draw_background			*/
		XfeInheritDrawShadow,					/* draw_shadow				*/
		DrawComponents,							/* draw_components			*/
		NULL,									/* extension				*/
    },

    /* XfeCaption Part */
    {
		NULL,									/* activate				*/
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeCaptionWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(caption,Caption);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassInitialize()
{
	/* Register XfeCaption Representation Types */
	CaptionRegisterRepTypes();
}
/*----------------------------------------------------------------------*/
static void
ClassPartInit(WidgetClass wc)
{
    XfeCaptionWidgetClass cc = (XfeCaptionWidgetClass) wc;
    XfeCaptionWidgetClass sc = (XfeCaptionWidgetClass) wc->core_class.superclass;

    _XfeResolve(cc,sc,xfe_caption_class,activate,XfeInheritCaptionActivate);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeCaptionPart *		pp = _XfeCaptionPart(nw);

    /* Create private components */
	pp->title		= TitleCreate(nw);

	/* Initialize private members */

	/* Add extra caption translations */
	XfeOverrideTranslations(nw,_XfeCaptionExtraTranslations);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeCaptionWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfeCaptionPart *		pp = _XfeCaptionPart(w); */
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
/*     XfeCaptionPart *		np = _XfeCaptionPart(nw); */
/*     XfeCaptionPart *		op = _XfeCaptionPart(ow); */

	/* Common pixel/pixmap/sensitive resources */
	_XfeManagerPropagateSetValues(ow,nw,True);

	/* shadow_thickness */
	if (_XfemShadowThickness(nw) != _XfemShadowThickness(ow))
	{
		_XfemConfigFlags(nw) |= XfeConfigLE;
	}
	
    return _XfeManagerChainSetValues(ow,rw,nw,xfeCaptionWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
/*     XfeCaptionPart *		pp = _XfeCaptionPart(w); */
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
    XfeCaptionPart *	pp = _XfeCaptionPart(w);

	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);
	
	/* Both */
	if (_XfeChildIsShown(pp->title) && _XfeChildIsShown(pp->child))
	{
		if (pp->caption_layout == XmCAPTION_CHILD_ON_BOTTOM ||
			pp->caption_layout == XmCAPTION_CHILD_ON_TOP)
		{
			*width += XfeMax(_XfeWidth(pp->title),_XfeWidth(pp->child));

			*height += 
				(_XfeHeight(pp->title) + 
				 pp->spacing + 
				 _XfeHeight(pp->child));
		}
		else if (pp->caption_layout == XmCAPTION_CHILD_ON_LEFT ||
				 pp->caption_layout == XmCAPTION_CHILD_ON_RIGHT)
		{
			*width += 
				(_XfeWidth(pp->title) + 
				 pp->spacing + 
				 _XfeWidth(pp->child));

			*height += XfeMax(_XfeHeight(pp->title),_XfeHeight(pp->child));
		}
#ifdef DEBUG_ramiro
		else
		{
			assert( 0 );
		}
#endif		
	}
	/* Title only */
	else if (_XfeChildIsShown(pp->title))
	{
		*width += _XfeWidth(pp->title);

		*height += _XfeHeight(pp->title);
	}
	/* Child only */
	else if (_XfeChildIsShown(pp->child))
	{
		*width += _XfeWidth(pp->child);

		*height += _XfeHeight(pp->child);
	}
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
/*     XfeCaptionPart *		pp = _XfeCaptionPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteStaticChild(Widget child)
{
/*     Widget					w = XtParent(child); */
/*     XfeCaptionPart *		pp = _XfeCaptionPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static void
LayoutStaticChildren(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);

	/* Both */
	if (_XfeChildIsShown(pp->title) && _XfeChildIsShown(pp->child))
	{
		if (pp->caption_layout == XmCAPTION_CHILD_ON_BOTTOM)
		{
			LayoutChildOnBottom(w);
		}
		else if (pp->caption_layout == XmCAPTION_CHILD_ON_LEFT)
		{
			LayoutChildOnLeft(w);
		}
		else if (pp->caption_layout == XmCAPTION_CHILD_ON_RIGHT)
		{
			LayoutChildOnRight(w);
		}
		else if (pp->caption_layout == XmCAPTION_CHILD_ON_TOP)
		{
			LayoutChildOnTop(w);
		}
	}
	/* Title only */
	else if (_XfeChildIsShown(pp->title))
	{
		LayoutTitleOnly(w);
	}
	/* Child only */
	else if (_XfeChildIsShown(pp->child))
	{
		LayoutChildOnly(w);
	}
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
/*     XfeCaptionPart *	pp = _XfeCaptionPart(w); */

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption action procedures											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Activate(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	_XfeCaptionActivate(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Title functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget
TitleCreate(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);
	Widget				title = NULL;
	Arg					av[20];
	Cardinal			ac = 0;

/* 	XtSetArg(av[ac],XmNbackground,			pp->title_background); ac++; */
/* 	XtSetArg(av[ac],XmNforeground,			pp->title_foreground); ac++; */
/*    	XtSetArg(av[ac],XmNalignment,			pp->title_alignment); ac++; */

	XtSetArg(av[ac],XmNshadowThickness,			0); ac++;
	XtSetArg(av[ac],XmNstringDirection,			pp->title_direction); ac++;
	XtSetArg(av[ac],XmNraiseBorderThickness,	0); ac++;
	XtSetArg(av[ac],XmNraiseOnEnter,			False); ac++;
	XtSetArg(av[ac],XmNfillOnArm,				False); ac++;
	XtSetArg(av[ac],XmNarmOffset,				0); ac++;

	if (pp->title_font_list != NULL)
	{
		XtSetArg(av[ac],XmNfontList,		pp->title_font_list); ac++;
	}

	if (pp->title_string != NULL)
	{
		XtSetArg(av[ac],XmNlabelString,		pp->title_string); ac++;
	}

	title = XtCreateManagedWidget(TITLE_NAME,
								  xfeButtonWidgetClass,
								  w,
								  av,
								  ac);

	XtAddCallback(title,XmNactivateCallback,TitleActivateCB,w);

	return title;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Title callbacks														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
TitleActivateCB(Widget title,XtPointer client_data,XtPointer call_data)
{
	Widget				w = (Widget) client_data;

	_XfeCaptionActivate(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Layout functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutChildOnBottom(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);

	assert( _XfeChildIsShown(pp->title) );
	assert( _XfeChildIsShown(pp->child) );

}
/*----------------------------------------------------------------------*/
static void
LayoutChildOnLeft(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);

	assert( _XfeChildIsShown(pp->title) );
	assert( _XfeChildIsShown(pp->child) );

}
/*----------------------------------------------------------------------*/
static void
LayoutChildOnRight(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);
	int					title_x;
	int					title_y;
	int					child_x;
	int					child_y;
	int					child_width;
	int					child_height;

	assert( _XfeChildIsShown(pp->title) );
	assert( _XfeChildIsShown(pp->child) );

	if (pp->child_resize)
	{
		child_width = 
 			_XfemBoundaryWidth(w) -
			_XfeWidth(pp->title) - 
			pp->spacing;

/*  		child_width = XfeMax(_XfeWidth(pp->child),child_width); */
	}
	else
	{
		child_width = _XfeWidth(pp->child);
	}

	if (child_width > pp->max_child_width)
	{
		child_width = pp->max_child_width;
	}

	child_height = _XfeHeight(pp->child);

	/* Child x */
	if (pp->child_hor_alignment == XmCAPTION_HORIZONTAL_ALIGNMENT_LEFT)
	{
		child_x = 
			_XfeWidth(w) - 
			_XfemOffsetRight(w) - 
			child_width;

	}
	else if (pp->child_hor_alignment == XmCAPTION_HORIZONTAL_ALIGNMENT_CENTER)
	{
		child_x = 
			_XfeWidth(w) - 
			_XfemOffsetRight(w) - 
			child_width;

	}
	else if (pp->child_hor_alignment == XmCAPTION_HORIZONTAL_ALIGNMENT_RIGHT)
	{
		child_x = 
			_XfeWidth(w) - 
			_XfemOffsetRight(w) - 
			child_width;
	}

	/* Child y */
	if (pp->child_ver_alignment == XmCAPTION_VERTICAL_ALIGNMENT_TOP)
	{
		child_y = _XfemOffsetTop(w);
	}
	else if (pp->child_ver_alignment == XmCAPTION_VERTICAL_ALIGNMENT_CENTER)
	{
		child_y = (_XfeHeight(w) - child_height) / 2;
	}
	else if (pp->child_ver_alignment == XmCAPTION_VERTICAL_ALIGNMENT_BOTTOM)
	{
		child_y = _XfeHeight(w) - _XfemOffsetBottom(w) - child_height;
	}

	/* Title x */
	if (pp->title_hor_alignment == XmCAPTION_HORIZONTAL_ALIGNMENT_LEFT)
	{
		title_x = _XfemOffsetLeft(w);
	}
	else if (pp->title_hor_alignment == XmCAPTION_HORIZONTAL_ALIGNMENT_CENTER)
	{
		title_x = _XfemOffsetLeft(w);
	}
	else if (pp->title_hor_alignment == XmCAPTION_HORIZONTAL_ALIGNMENT_RIGHT)
	{
		title_x = _XfemOffsetLeft(w);
	}

	/* Title y */
	if (pp->title_ver_alignment == XmCAPTION_VERTICAL_ALIGNMENT_TOP)
	{
		title_y = _XfemOffsetTop(w);
	}
	else if (pp->title_ver_alignment == XmCAPTION_VERTICAL_ALIGNMENT_CENTER)
	{
		title_y = (_XfeHeight(w) - _XfeHeight(pp->title)) / 2;
	}
	else if (pp->title_ver_alignment == XmCAPTION_VERTICAL_ALIGNMENT_BOTTOM)
	{
		title_y = _XfeHeight(w) - _XfemOffsetBottom(w) - _XfeHeight(pp->title);
	}

	_XfeMoveWidget(pp->title,title_x,title_y);
/* 	_XfeMoveWidget(pp->child,child_x,child_y); */

 	_XfeConfigureWidget(pp->child,child_x,child_y,child_width,child_height);
}
/*----------------------------------------------------------------------*/
static void
LayoutChildOnTop(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);
	
	assert( _XfeChildIsShown(pp->title) );
	assert( _XfeChildIsShown(pp->child) );

}
/*----------------------------------------------------------------------*/
static void
LayoutTitleOnly	(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);

	assert( _XfeChildIsShown(pp->title) );

	printf("LayoutChildTitleOnly(%s)\n",XtName(w));
}
/*----------------------------------------------------------------------*/
static void
LayoutChildOnly	(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);
	
	assert( _XfeChildIsShown(pp->child) );

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* RepType registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CaptionRegisterRepTypes(void)
{
    static String child_names[] = 
    { 
		"caption_child_on_bottom",
		"caption_child_on_left",
		"caption_child_on_right",
		"caption_child_on_top",
		NULL
    };
    
    static String vertical_names[] = 
    { 
		"caption_vertical_alignment_left",
		"caption_vertical_alignment_center",
		"caption_vertical_alignment_right",
		NULL
    };
    
    static String horizontal_names[] = 
    { 
		"caption_vertical_alignment_bottom",
		"caption_vertical_alignment_center",
		"caption_vertical_alignment_top",
		NULL
    };
    
    XfeRepTypeRegister(XmRCaptionLayout,child_names);
    XfeRepTypeRegister(XmRCaptionVerticalAlignment,vertical_names);
    XfeRepTypeRegister(XmRCaptionVerticalAlignment,horizontal_names);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc functions														*/
/*																		*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeCaptionActivate(Widget w)
{
	XfeCaptionWidgetClass	mc = (XfeCaptionWidgetClass) XtClass(w);

	if (mc->xfe_caption_class.activate)
	{
		(*mc->xfe_caption_class.activate)(w);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateCaption(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfeCaptionWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
extern Dimension
XfeCaptionMaxChildWidth(Widget w)
{
    XfeCaptionPart *	pp = _XfeCaptionPart(w);
	Dimension			max_child_width = 0;

	assert( _XfeIsAlive(w) );
	
	/* Both */
	if (_XfeChildIsShown(pp->title) && _XfeChildIsShown(pp->child))
	{
		if (pp->caption_layout == XmCAPTION_CHILD_ON_BOTTOM ||
			pp->caption_layout == XmCAPTION_CHILD_ON_TOP)
		{
			max_child_width = _XfemBoundaryWidth(w);
		}
		else if (pp->caption_layout == XmCAPTION_CHILD_ON_LEFT ||
				 pp->caption_layout == XmCAPTION_CHILD_ON_RIGHT)
		{
			max_child_width =
				_XfemBoundaryWidth(w) -
				_XfeWidth(pp->title) - 
				pp->spacing;
		}
	}
	/* Title only */
	else if (_XfeChildIsShown(pp->title))
	{
		max_child_width = 0;
	}
	/* Child only */
	else if (_XfeChildIsShown(pp->child))
	{
		max_child_width = _XfemBoundaryWidth(w);
	}
	
	return max_child_width;
}
/*----------------------------------------------------------------------*/
