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
/* Name:		<Xfe/ToolItem.c>										*/
/* Description:	XfeToolItem widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/ToolItemP.h>

#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xfe/Label.h>
#include <Xfe/Button.h>

#define MESSAGE1 "Widget is not an XfeToolItem."
#define MESSAGE2 "XmNitem is a read-only resource."
#define MESSAGE3 "XmNlogo is a read-only resource."

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize		(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy			(Widget);
static Boolean	SetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static void		LayoutComponents	(Widget);
static Boolean	AcceptChild			(Widget);
static Boolean	DeleteChild			(Widget);
static Boolean	InsertChild			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolItem Resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Components */
    { 
		XmNlogo,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolItemRec , xfe_tool_item . logo),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNitem,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeToolItemRec , xfe_tool_item . item),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeToolItemRec , xfe_tool_item . spacing),
		XmRImmediate, 
		(XtPointer) 2
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolItem Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
   { 
       XmNspacing,
       sizeof(Dimension),
       XtOffsetOf(XfeToolItemRec , xfe_tool_item . spacing),
       _XmFromHorizontalPixels,
       _XmToHorizontalPixels 
   },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(toolitem,ToolItem) =
{
	{
		(WidgetClass) &xfeManagerClassRec,		/* superclass       	*/
		"XfeToolItem",							/* class_name       	*/
		sizeof(XfeToolItemRec),					/* widget_size      	*/
		NULL,									/* class_initialize 	*/
		NULL,									/* class_part_initialize*/
		FALSE,                                  /* class_inited     	*/
		Initialize,                             /* initialize       	*/
		NULL,                                   /* initialize_hook  	*/
		XtInheritRealize,						/* realize          	*/
		NULL,									/* actions          	*/
		0,										/* num_actions			*/
		resources,                              /* resources        	*/
		XtNumber(resources),                    /* num_resources    	*/
		NULLQUARK,                              /* xrm_class        	*/
		TRUE,                                   /* compress_motion  	*/
		XtExposeCompressMaximal,                /* compress_exposure	*/
		TRUE,                                   /* compress_enterleave	*/
		FALSE,                                  /* visible_interest 	*/
		Destroy,								/* destroy          	*/
		XtInheritResize,                        /* resize           	*/
		XtInheritExpose,						/* expose           	*/
		SetValues,                              /* set_values       	*/
		NULL,                                   /* set_values_hook  	*/
		XtInheritSetValuesAlmost,				/* set_values_almost	*/
		NULL,									/* get_values_hook  	*/
		NULL,                                   /* accexfe_focus     	*/
		XtVersion,                              /* version          	*/
		NULL,                                   /* callback_private 	*/
		XtInheritTranslations,					/* tm_table				*/
		XtInheritQueryGeometry,					/* query_geometry   	*/
		XtInheritDisplayAccelerator,            /* display accel    	*/
		NULL,                                   /* extension        	*/
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
		NULL,									/* resource list       	*/
		0,										/* num resources       	*/
		sizeof(XfeManagerConstraintRec),		/* constraint size     	*/
		NULL,									/* init proc           	*/
		NULL,                                   /* destroy proc        	*/
		NULL,									/* set values proc     	*/
		NULL,                                   /* extension           	*/
	},

	/* XmManager Part */
	{
		XtInheritTranslations,					/* tm_table				*/
		syn_resources,							/* syn resources       	*/
		XtNumber(syn_resources),				/* num syn_resources   	*/
		NULL,                                   /* syn_cont_resources  	*/
		0,                                      /* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process      	*/
		NULL,                                   /* extension           	*/
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
		NULL,									/* draw_components		*/
		NULL,									/* extension          	*/
	},

	/* XfeToolItem Part */
	{
		NULL,									/* extension          	*/
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeToolItemWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(toolitem,ToolItem);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeToolItemWidgetClass);
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
    XfeToolItemPart *		np = _XfeToolItemPart(nw);
    XfeToolItemPart *		op = _XfeToolItemPart(ow);

    /* item */
    if (np->item != op->item)
    {
		_XfeWarning(nw,MESSAGE2);
		np->item = op->item;
    }

    /* logo */
    if (np->logo != op->logo)
    {
		_XfeWarning(nw,MESSAGE3);
		np->logo = op->logo;
    }

    if (np->spacing != op->spacing)
    {
		_XfemConfigFlags(nw) |= XfeConfigGLE;
    }

    return _XfeManagerChainSetValues(ow,rw,nw,xfeToolItemWidgetClass);
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
	XfeToolItemPart *	tp = _XfeToolItemPart(w);

	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);

	if (_XfeChildIsShown(tp->item))
	{
		*height += _XfeHeight(tp->item);
		*width  += _XfeWidth(tp->item);
	}

	if (_XfeChildIsShown(tp->logo))
	{
		*width  += (_XfeWidth(tp->logo) + tp->spacing);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
	XfeToolItemPart *	tp = _XfeToolItemPart(w);

	/* Configure the logo if needed */
	if (_XfeChildIsShown(tp->logo))
	{
		/* Place the logo on the far right */
		_XfeConfigureWidget(tp->logo,
							
							_XfeWidth(w) - 
							_XfemOffsetRight(w) -
							_XfeWidth(tp->logo),
							
							(_XfeHeight(w) - _XfeHeight(tp->logo)) / 2,
							
							_XfeWidth(tp->logo),
							
							_XfeHeight(tp->logo));
	}

	/* Configure the tool bar */
	if (_XfeChildIsShown(tp->item))
	{
		Dimension width = _XfemRectWidth(w);

		if (_XfeChildIsShown(tp->logo))
		{
			width -= (_XfeWidth(tp->logo) + tp->spacing);
		}

		/* Place the logo on the far left and use all the space available */
		_XfeConfigureWidget(tp->item,
							
							_XfemOffsetLeft(w),

							(_XfeHeight(w) - XfeHeight(tp->item)) / 2,
							
 							width,

							_XfeHeight(tp->item));
	}		
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptChild(Widget child)
{
	Widget					w = XtParent(child);
	XfeToolItemPart *		tp = _XfeToolItemPart(w);
	Boolean					accept = False;

	/* logo */
	if (XfeIsLogo(child))
	{
		accept = !tp->logo;
	}
	/* Item */
	else
	{
		accept = !tp->item;
	}

	return accept;
}
/*----------------------------------------------------------------------*/
static Boolean
InsertChild(Widget child)
{
	Widget					w = XtParent(child);
	XfeToolItemPart *		tp = _XfeToolItemPart(w);

	/* Logo for logo */
	if (XfeIsLogo(child))
	{
		tp->logo = child;
	}
	/* Item */
	else
	{
		tp->item = child;
	}

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteChild(Widget child)
{
	Widget					w = XtParent(child);
	XfeToolItemPart *		tp = _XfeToolItemPart(w);

	/* Logo */
	if (child == tp->logo)
	{
		tp->logo = NULL;
	}
	/* Item */
	else if (child == tp->item)
	{
		tp->item = NULL;
	}

	return True;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolItem Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern*/ Widget
XfeCreateToolItem(Widget pw,char * name,Arg * av,Cardinal ac)
{
   return XtCreateWidget(name,xfeToolItemWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
