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
/* Name:		<Xfe/TextCaption.c>										*/
/* Description:	XfeTextCaption widget source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/TextCaptionP.h>
#include <Xm/TextF.h>

#define MESSAGE1 "Widget is not an XfeTextCaption."
#define MESSAGE2 "XmNtitleDirection is not XmSTRING_DIRECTION_L_TO_R or XmSTRING_DIRECTION_R_TO_L."

#define TEXT_NAME					"CaptionText"
#define DEFAULT_SUB_TITLE_NAME		"TextCaptionSubTitle"

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

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		Activate			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Text functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget	TextCreate			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
	/* Text resources */
    { 
		XmNtextFontList,
		XmCTextFontList,
		XmRFontList,
		sizeof(XmFontList),
		XtOffsetOf(XfeTextCaptionRec , xfe_text_caption . text_font_list),
		XmRCallProc, 
		(XtPointer) _XfeCallProcDefaultLabelFontList
    },
    { 
		XmNtextString,
		XmCTextString,
		XmRXmString,
		sizeof(XmString),
		XtOffsetOf(XfeTextCaptionRec , xfe_text_caption . text_string),
		XmRImmediate, 
		(XtPointer) NULL
    },
};   

#if 0
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption synthetic resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
	{ 
		XmNtitleSpacing,
		sizeof(Dimension),
		XtOffsetOf(XfeTextCaptionRec , xfe_text_caption . title_spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
};
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(textcaption,TextCaption) =
{
    {
		(WidgetClass) &xfeCaptionClassRec,		/* superclass			*/
		"XfeTextCaption",						/* class_name			*/
		sizeof(XfeTextCaptionRec),				/* widget_size			*/
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
		NULL,									/* syn resources      	*/
		0,										/* num syn_resources  	*/
#endif
		NULL,                                   /* syn_cont_resources  	*/
		0,                                      /* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process		*/
		NULL,                                   /* extension			*/
    },
    
    /* XfeManager Part 	*/
	{
		XfeInheritBitGravity,					/* bit_gravity				*/
		XfeInheritPreferredGeometry,			/* preferred_geometry		*/
		XfeInheritUpdateBoundary,				/* update_boundary			*/
		XfeInheritUpdateChildrenInfo,			/* update_children_info		*/
		XfeInheritLayoutWidget,					/* layout_widget			*/
		XfeInheritAcceptStaticChild,			/* accept_static_child		*/
		XfeInheritInsertStaticChild,			/* insert_static_child		*/
		XfeInheritDeleteStaticChild,			/* delete_static_child		*/
		XfeInheritLayoutStaticChildren,			/* layout_static_children	*/
		NULL,									/* change_managed			*/
		NULL,									/* prepare_components		*/
		NULL,									/* layout_components		*/
		NULL,									/* draw_background			*/
		XfeInheritDrawShadow,					/* draw_shadow				*/
		XfeInheritDrawComponents,				/* draw_components			*/
		XfeInheritDrawAccentBorder,				/* draw_accent_border		*/
		NULL,									/* extension				*/
    },

    /* XfeCaption Part */
    {
		Activate,								/* activate				*/
		NULL,									/* extension			*/
    },

    /* XfeTextCaption Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeTextCaptionWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(textcaption,TextCaption);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeCaptionPart *		pp = _XfeCaptionPart(nw);
/*     XfeTextCaptionPart *	tp = _XfeTextCaptionPart(nw); */

    /* Create private components */
	pp->child		= TextCreate(nw);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeTextCaptionWidgetClass);
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

	/* shadow_thickness */
	if (_XfemShadowThickness(nw) != _XfemShadowThickness(ow))
	{
		_XfemConfigFlags(nw) |= XfeConfigLE;
	}
	

    return _XfeManagerChainSetValues(ow,rw,nw,xfeTextCaptionWidgetClass);
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
/* XfeCaption class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Activate(Widget w)
{
	XfeCaptionPart *		pp = _XfeCaptionPart(w);
	
	/* Traverse to the child if its alive */
	if (_XfeChildIsShown(pp->child))
	{
		XmProcessTraversal(pp->child,XmTRAVERSE_CURRENT);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Text functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget
TextCreate(Widget w)
{
    XfeTextCaptionPart *	tp = _XfeTextCaptionPart(w);
	Widget					text = NULL;
	Arg						av[20];
	Cardinal				ac = 0;

/* 	XtSetArg(av[ac],XmNbackground,			tp->text_background); ac++; */
/* 	XtSetArg(av[ac],XmNforeground,			tp->text_foreground); ac++; */
/* 	XtSetArg(av[ac],XmNshadowThickness,		0); ac++; */
/* 	XtSetArg(av[ac],XmNstringDirection,		tp->text_direction); ac++; */
/*  	XtSetArg(av[ac],XmNalignment,			tp->text_alignment); ac++; */

	if (tp->text_font_list != NULL)
	{
		XtSetArg(av[ac],XmNfontList,		tp->text_font_list); ac++;
	}

	text = XtCreateManagedWidget(TEXT_NAME,
								 xmTextFieldWidgetClass,
								 w,
								 av,
								 ac);

	return text;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeCaption Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateTextCaption(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfeTextCaptionWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
