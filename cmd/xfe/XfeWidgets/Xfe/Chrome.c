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
/* Name:		<Xfe/Chrome.c>											*/
/* Description:	XfeChrome widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/ChromeP.h>

/* Possible children classes */
#include <Xfe/Label.h>
#include <Xfe/Button.h>
#include <Xfe/Cascade.h>

#include <Xm/Separator.h>
#include <Xm/DrawingA.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>

#define MESSAGE1 "Widget is not an XfeChrome."
#define MESSAGE2 "The XmNmenuBar widget cannot be changed."
#define MESSAGE3 "The XmNtoolBox widget cannot be changed."
#define MESSAGE4 "The XmNdashBoard widget cannot be changed."

#define IS_MENU_BAR(w)		(_XfeIsAlive(w) && XmIsRowColumn(w))
#define IS_TOOL_BOX(w)		(_XfeIsAlive(w) && XfeIsToolBox(w))
#define IS_DASH_BOARD(w)	(_XfeIsAlive(w) && XfeIsDashBoard(w))

#define SHOW_BOTTOM_VIEW(cp)	(_XfeChildIsShown((cp)->bottom_view))
#define SHOW_CENTER_VIEW(cp)	(_XfeChildIsShown((cp)->center_view))
#define SHOW_DASH_BOARD(cp)		(_XfeChildIsShown((cp)->dash_board))
#define SHOW_LEFT_VIEW(cp)		(_XfeChildIsShown((cp)->left_view))
#define SHOW_MENU_BAR(cp)		(_XfeChildIsShown((cp)->menu_bar))
#define SHOW_RIGHT_VIEW(cp)		(_XfeChildIsShown((cp)->right_view))
#define SHOW_TOOL_BOX(cp)		(_XfeChildIsShown((cp)->tool_box))
#define SHOW_TOP_VIEW(cp)		(_XfeChildIsShown((cp)->top_view))

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize			(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy				(Widget);
static Boolean	SetValues			(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ConstraintInitialize(Widget,Widget,ArgList,Cardinal *);
static Boolean	ConstraintSetValues	(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static Boolean	AcceptChild			(Widget);
static Boolean	InsertChild			(Widget);
static Boolean	DeleteChild			(Widget);
static void		LayoutChildren		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeChrome functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutBottomView	(Widget);
static void		LayoutCenterView	(Widget);
static void		LayoutDashBoard		(Widget);
static void		LayoutLeftView		(Widget);
static void		LayoutMenuBar		(Widget);
static void		LayoutRightView		(Widget);
static void		LayoutToolBox		(Widget);
static void		LayoutTopView		(Widget);
								   
/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint resource callprocs										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DefaultChromeChildType			(Widget,int,XrmValue *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChrome resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 	
{					
    /* Resources */
	{ 
		XmNmenuBar,
		XmCWidget,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeChromeRec , xfe_chrome . menu_bar),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNtoolBox,
		XmCWidget,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeChromeRec , xfe_chrome . tool_box),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNcenterView,
		XmCWidget,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeChromeRec , xfe_chrome . center_view),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNtopView,
		XmCWidget,
		XmCWidget,
		sizeof(Widget),
		XtOffsetOf(XfeChromeRec , xfe_chrome . top_view),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNbottomView,
		XmCWidget,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeChromeRec , xfe_chrome . bottom_view),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNleftView,
		XmCWidget,
		XmCWidget,
		sizeof(Widget),
		XtOffsetOf(XfeChromeRec , xfe_chrome . left_view),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNrightView,
		XmCWidget,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeChromeRec , xfe_chrome . right_view),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNdashBoard,
		XmCWidget,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfeChromeRec , xfe_chrome . dash_board),
		XmRImmediate, 
		(XtPointer) NULL
    },
    { 
		XmNspacing,
		XmCSpacing,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeChromeRec , xfe_chrome . spacing),
		XmRImmediate, 
		(XtPointer) 1
    },

    /* Force all the margins to 0 */
    { 
		XmNmarginBottom,
		XmCMarginBottom,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeChromeRec , xfe_manager . margin_bottom),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeChromeRec , xfe_manager . margin_left),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeChromeRec , xfe_manager . margin_right),
		XmRImmediate, 
		(XtPointer) 0
    },
    { 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeChromeRec , xfe_manager . margin_top),
		XmRImmediate, 
		(XtPointer) 0
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChrome synthetic resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource synthetic_resources[] =
{
   { 
       XmNspacing,
       sizeof(Dimension),
       XtOffsetOf(XfeChromeRec , xfe_chrome . spacing),
       _XmFromHorizontalPixels,
       _XmToHorizontalPixels 
   },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChrome constraint resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource constraint_resources[] = 
{
    { 
		XmNchromeChildType,
		XmCChromeChildType,
		XmRChromeChildType,
		sizeof(unsigned char),
		XtOffsetOf(XfeChromeConstraintRec , xfe_chrome . chrome_child_type),
		XmRCallProc,
		(XtPointer) DefaultChromeChildType
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(chrome,Chrome) =
{
    {
		(WidgetClass) &xfeManagerClassRec,		/* superclass			*/
		"XfeChrome",							/* class_name			*/
		sizeof(XfeChromeRec),					/* widget_size			*/
		NULL,									/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		NULL,									/* actions				*/
		0,										/* num_actions			*/
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
		_XfeLiberalGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },

    /* Constraint Part */
    {
		constraint_resources,					/* constraint res		*/
		XtNumber(constraint_resources),			/* num constraint res	*/
		sizeof(XfeChromeConstraintRec),			/* constraint size		*/
		ConstraintInitialize,					/* init proc			*/
		NULL,									/* destroy proc			*/
		ConstraintSetValues,					/* set values proc		*/
		NULL,                                   /* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		synthetic_resources,					/* syn resources		*/
		XtNumber(synthetic_resources),			/* num syn_resources	*/
		NULL,									/* syn resources		*/
		0,										/* num syn_resources	*/
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
		NULL,									/* layout_components	*/
		LayoutChildren,							/* layout_children		*/
		NULL,									/* draw_background		*/
		XfeInheritDrawShadow,					/* draw_shadow			*/
		NULL,									/* draw_components		*/
		NULL,									/* extension			*/
    },

    /* XfeChrome Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeChromeWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(chrome,Chrome);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint resource callprocs										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DefaultChromeChildType(Widget child,int offset,XrmValue * value)
{
	Widget					w = _XfeParent(child);
	XfeChromePart *			cp = _XfeChromePart(w);
    static unsigned char	chrome_child_type;

	/* menu_bar */
	if (IS_MENU_BAR(child))
	{
		chrome_child_type = XmCHROME_MENU_BAR;
	}
	/* tool_box */
	else if (IS_TOOL_BOX(child))
	{
		chrome_child_type = XmCHROME_TOOL_BOX;
	}
	/* dash_board */
	else if (IS_DASH_BOARD(child))
	{
		chrome_child_type = XmCHROME_DASH_BOARD;
	}
	/* center_view */
	else if (!_XfeIsAlive(cp->center_view))
	{
		chrome_child_type = XmCHROME_CENTER_VIEW;
	}
	/* top_view */
	else if (!_XfeIsAlive(cp->top_view))
	{
		chrome_child_type = XmCHROME_TOP_VIEW;
	}
	/* bottom_view */
	else if (!_XfeIsAlive(cp->bottom_view))
	{
		chrome_child_type = XmCHROME_BOTTOM_VIEW;
	}
	/* left_view */
	else if (!_XfeIsAlive(cp->left_view))
	{
		chrome_child_type = XmCHROME_LEFT_VIEW;
	}
	/* right_view */
	else if (!_XfeIsAlive(cp->right_view))
	{
		chrome_child_type = XmCHROME_RIGHT_VIEW;
	}

    value->addr = (XPointer) &chrome_child_type;
    value->size = sizeof(chrome_child_type);
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
    XfeChromePart *		cp = _XfeChromePart(nw);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeChromeWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    XfeChromePart *		cp = _XfeChromePart(w);

}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfeChromePart *		np = _XfeChromePart(nw);
    XfeChromePart *		op = _XfeChromePart(ow);

	/* spacing */
	if (np->spacing != op->spacing)
	{
		_XfemConfigFlags(nw) |= XfeConfigLayout;
	}

	/* menu_bar */
	if (np->menu_bar != op->menu_bar)
	{
		np->menu_bar = op->menu_bar;

		_XfeWarning(nw,MESSAGE2);
	}

	/* tool_box */
	if (np->tool_box != op->tool_box)
	{
		np->tool_box = op->tool_box;

		_XfeWarning(nw,MESSAGE3);
	}

	/* dash_board */
	if (np->dash_board != op->dash_board)
	{
		np->dash_board = op->dash_board;

		_XfeWarning(nw,MESSAGE4);
	}

	/* center_view */
	if (np->center_view != op->center_view)
	{
		_XfemConfigFlags(nw) |= XfeConfigLayout;
	}

	/* top_view */
	if (np->top_view != op->top_view)
	{
		_XfemConfigFlags(nw) |= XfeConfigLayout;
	}

	/* bottom_view */
	if (np->bottom_view != op->bottom_view)
	{
		_XfemConfigFlags(nw) |= XfeConfigLayout;
	}

    return _XfeManagerChainSetValues(ow,rw,nw,xfeChromeWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ConstraintInitialize(Widget rc,Widget nc,ArgList av,Cardinal * ac)
{
  	Widget						w = _XfeParent(nc);
	XfeChromeConstraintPart *	cp = _XfeChromeConstraintPart(nc);

    /* Make sure the chrome child type is ok */
    XfeRepTypeCheck(nc,XmRChromeChildType,&cp->chrome_child_type,
					XmCHROME_IGNORE);

	/* Finish constraint initialization */
	_XfeConstraintChainInitialize(rc,nc,xfeChromeWidgetClass);
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValues(Widget oc,Widget rc,Widget nc,ArgList av,Cardinal * ac)
{
	Widget						w = XtParent(nc);
 	XfeChromeConstraintPart *	ncp = _XfeChromeConstraintPart(nc);
 	XfeChromeConstraintPart *	ocp = _XfeChromeConstraintPart(oc);

	/* chrome_child_type */
	if (ncp->chrome_child_type != ocp->chrome_child_type)
	{
		/* Make sure the new chrome child type is ok */
		XfeRepTypeCheck(nc,XmRChromeChildType,&ncp->chrome_child_type,
						XmCHROME_IGNORE);
	}

	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(oc,rc,nc,xfeChromeWidgetClass);
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
    XfeChromePart *		cp = _XfeChromePart(w);
	Dimension			max_width = 0;
	Dimension			max_height = 0;

	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w)  + _XfemOffsetBottom(w);

	/* center_view */
	if (SHOW_CENTER_VIEW(cp))
	{
		max_width = _XfeWidth(cp->center_view);
		max_height = _XfeHeight(cp->center_view);
	}

	/* bottom_view */
	if (SHOW_BOTTOM_VIEW(cp))
	{
		max_width = XfeMax(max_width,_XfeWidth(cp->bottom_view));
		
		*height += _XfeHeight(cp->bottom_view);
	}

	/* top_view */
	if (SHOW_TOP_VIEW(cp))
	{
		max_width = XfeMax(max_width,_XfeWidth(cp->top_view));
		
		*height += _XfeHeight(cp->top_view);
	}

	/* menu_bar */
	if (SHOW_MENU_BAR(cp))
	{
		max_width = XfeMax(max_width,_XfeWidth(cp->menu_bar));
		
		*height += _XfeHeight(cp->menu_bar);
	}

	/* tool_box */
	if (SHOW_TOOL_BOX(cp))
	{
		max_width = XfeMax(max_width,_XfeWidth(cp->tool_box));
		
		*height += _XfeHeight(cp->tool_box);
	}

	/* dash_board */
	if (SHOW_DASH_BOARD(cp))
	{
		max_width = XfeMax(max_width,_XfeWidth(cp->dash_board));
		
		*height += _XfeHeight(cp->dash_board);
	}

	/* left_view */
	if (SHOW_LEFT_VIEW(cp))
	{
		*width += _XfeWidth(cp->left_view);
		
		max_height = XfeMax(max_height,_XfeHeight(cp->left_view));
	}

	/* right_view */
	if (SHOW_RIGHT_VIEW(cp))
	{
		*width += _XfeWidth(cp->right_view);
		
		max_height = XfeMax(max_height,_XfeHeight(cp->right_view));
	}

	*width += max_width;
	*height += max_height;


    /*
     * HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY
     *
     * This is a complete HACK.  Hardcode the dimensions to 640x480
     * until I write some clever code to compute dimensions from
     * resources, command line, children preferred geometries, and
     * other magical things.
     *
     * HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY HACKERY
     */
    if (*width <= 2)
    {
      *width = 600;
    }
    
    if (*height <= 2)
    {
      *height = 480;
    }
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptChild(Widget child)
{
	Widget				w = XtParent(child);
	XfeChromePart *		cp = _XfeChromePart(w);
	Boolean				accept = True;

	/* menu_bar */
	if (IS_MENU_BAR(child))
	{
		accept = !cp->menu_bar;
	}
	/* tool_box */
	else if (IS_TOOL_BOX(child))
	{
		accept = !cp->tool_box;
	}
	/* dash_board */
	else if (IS_DASH_BOARD(child))
	{
		accept = !cp->dash_board;
	}
	/* view / top_view / bottom_view */
	else if (!cp->center_view ||
			 !cp->top_view ||
			 !cp->bottom_view ||
			 !cp->left_view ||
			 !cp->right_view)
	{
		accept = True;
	}

	return accept;
}
/*----------------------------------------------------------------------*/
static Boolean
InsertChild(Widget child)
{
	Widget				w = XtParent(child);
	XfeChromePart *		cp = _XfeChromePart(w);
	Boolean				layout = False;

	/* menu_bar */
	if (IS_MENU_BAR(child))
	{
		cp->menu_bar = child;

		layout = True;
	}
	/* tool_box */
	else if (IS_TOOL_BOX(child))
	{
		cp->tool_box = child;

		layout = True;
	}
	/* dash_board */
	else if (IS_DASH_BOARD(child))
	{
		cp->dash_board = child;

		layout = True;
	}
	/* center_view */
	else if (!cp->center_view)
	{
		cp->center_view = child;

		layout = True;
	}
	/* top_view */
	else if (!cp->top_view)
	{
		cp->top_view = child;

		layout = True;
	}
	/* bottom_view */
	else if (!cp->bottom_view)
	{
		cp->bottom_view = child;

		layout = True;
	}
	/* left_view */
	else if (!cp->left_view)
	{
		cp->left_view = child;

		layout = True;
	}
	/* right_view */
	else if (!cp->right_view)
	{
		cp->right_view = child;

		layout = True;
	}

	return layout;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteChild(Widget child)
{
	Widget				w = XtParent(child);
	XfeChromePart *		cp = _XfeChromePart(w);

	/* menu_bar */
	if (child == cp->menu_bar)
	{
		cp->menu_bar = NULL;
	}
	/* tool_box */
	else if (child == cp->tool_box)
	{
		cp->tool_box = NULL;
	}
	/* dash_board */
	else if (child == cp->dash_board)
	{
		cp->dash_board = NULL;
	}
	/* center_view */
	else if (child == cp->center_view)
	{
		cp->center_view = NULL;
	}
	/* top_view */
	else if (child == cp->top_view)
	{
		cp->top_view = NULL;
	}
	/* bottom_view */
	else if (child == cp->bottom_view)
	{
		cp->bottom_view = NULL;
	}
	/* left_view */
	else if (child == cp->left_view)
	{
		cp->left_view = NULL;
	}
	/* right_view */
	else if (child == cp->right_view)
	{
		cp->right_view = NULL;
	}
	
	return True;
}
/*----------------------------------------------------------------------*/
static void
LayoutChildren(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);

	/* menu_bar */
	if (SHOW_MENU_BAR(cp))
	{
		LayoutMenuBar(w);
	}

	/* tool_box */
	if (SHOW_TOOL_BOX(cp))
	{
		LayoutToolBox(w);
	}

	/* dash_board */
	if (SHOW_DASH_BOARD(cp))
	{
		LayoutDashBoard(w);
	}

	/* top_view */
	if (SHOW_TOP_VIEW(cp))
	{
		LayoutTopView(w);
	}

	/* left_view */
	if (SHOW_LEFT_VIEW(cp))
	{
		LayoutLeftView(w);
	}

	/* bottom_view */
	if (SHOW_BOTTOM_VIEW(cp))
	{
		LayoutBottomView(w);
	}

	/* right_view */
	if (SHOW_RIGHT_VIEW(cp))
	{
		LayoutRightView(w);
	}

	/* center_view */
	if (SHOW_CENTER_VIEW(cp))
	{
		LayoutCenterView(w);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfeChrome functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutMenuBar(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);

	assert( SHOW_MENU_BAR(cp) );

	/* Place the menu bar on the very top and occupy the full width */
	_XfeConfigureWidget(cp->menu_bar,
						_XfemRectX(w),
						_XfemRectY(w),
						_XfemRectWidth(w),
						_XfeHeight(cp->menu_bar));
}
/*----------------------------------------------------------------------*/
static void
LayoutToolBox(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);

	assert( SHOW_TOOL_BOX(cp) );

	/* Menu bar is shown */
	if (SHOW_MENU_BAR(cp))
	{
		/* Place the tool box right after the menu bar */
		_XfeConfigureWidget(cp->tool_box,

							_XfemRectX(w),
							
							_XfeY(cp->menu_bar) +
							_XfeHeight(cp->menu_bar),
							
							_XfemRectWidth(w),

							_XfeHeight(cp->tool_box));
	}
	/* Menu bar is not shown */
	else
	{
		/* Place the tool box on the very top */
		_XfeConfigureWidget(cp->tool_box,

							_XfemRectX(w),
							
							_XfemRectY(w),
							
							_XfemRectWidth(w),

							_XfeHeight(cp->tool_box));
	}

}
/*----------------------------------------------------------------------*/
static void
LayoutDashBoard(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);

	assert( SHOW_DASH_BOARD(cp) );

	/* Make sure the dash board looks right */
	XtVaSetValues(cp->dash_board,
				  XmNusePreferredWidth,		False,
				  XmNusePreferredHeight,	True,
				  NULL);

	/* Place the dash board the very bottom and occupy the full width */
	_XfeConfigureWidget(cp->dash_board,

						_XfemRectX(w),
						
						_XfeHeight(w) - 
						_XfemOffsetBottom(w) -
						_XfeHeight(cp->dash_board),

						_XfemRectWidth(w),

						_XfeHeight(cp->dash_board));

}
/*----------------------------------------------------------------------*/
static void
LayoutTopView(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);
	Widget			top = NULL;
	Widget			bottom = NULL;
	int				y;
	int				x;
	int				width;
	int				height;

	assert( SHOW_TOP_VIEW(cp) );

	/* tool_box is shown */
	if (SHOW_TOOL_BOX(cp))
	{
		top = cp->tool_box;
	}
	/* menu_bar is shown */
	else if (SHOW_MENU_BAR(cp))
	{
		top = cp->menu_bar;
	}

	if (_XfeIsAlive(top))
	{
		y = _XfeY(top) + _XfeHeight(top) + cp->spacing;
	}
	else
	{
		y = _XfemRectY(w);
	}

	/* left_view is shown */
	if (SHOW_LEFT_VIEW(cp))
	{
		x = _XfemRectX(w) + _XfeWidth(cp->left_view);

		width = _XfemRectWidth(w) - _XfeWidth(cp->left_view);
	}
	else
	{
		x = _XfemRectX(w);

		width = _XfemRectWidth(w);
	}
	
	/* right_view is shown */
	if (SHOW_RIGHT_VIEW(cp))
	{
		width -= _XfeWidth(cp->right_view);
	}

	height = _XfeHeight(cp->top_view);
	
	/* Place the top_view */
	_XfeConfigureWidget(cp->top_view,x,y,width,height);
}
/*----------------------------------------------------------------------*/
static void
LayoutBottomView(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);
	Widget			top = NULL;
	Widget			bottom = NULL;
	int				y;
	int				x;
	int				width;
	int				height;

	assert( SHOW_BOTTOM_VIEW(cp) );

	/* dash_board is shown */
	if (SHOW_DASH_BOARD(cp))
	{
		bottom = cp->dash_board;
	}

	if (_XfeIsAlive(bottom))
	{
		y = _XfeY(bottom) - cp->spacing;
	}
	else
	{
		y = _XfeHeight(w) - _XfemOffsetBottom(w);
	}

	y -= _XfeHeight(cp->bottom_view);

	/* left_view is shown */
	if (SHOW_LEFT_VIEW(cp))
	{
		x = _XfemRectX(w) + _XfeWidth(cp->left_view);

		width = _XfemRectWidth(w) - _XfeWidth(cp->left_view);
	}
	else
	{
		x = _XfemRectX(w);

		width = _XfemRectWidth(w);
	}
	
	/* right_view is shown */
	if (SHOW_RIGHT_VIEW(cp))
	{
		width -= _XfeWidth(cp->right_view);
	}

	height = _XfeHeight(cp->bottom_view);

	/* Place the bottom_view */
	_XfeConfigureWidget(cp->bottom_view,x,y,width,height);
}
/*----------------------------------------------------------------------*/
static void
LayoutCenterView(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);
	Widget			top = NULL;
	Widget			bottom = NULL;
	int				y;
	int				y2;
	int				x;
	int				width;
	int				height;

	assert( SHOW_CENTER_VIEW(cp) );

	/* top_view is shown */
	if (SHOW_TOP_VIEW(cp))
	{
		top = cp->top_view;
	}
	/* tool_box is shown */
	else if (SHOW_TOOL_BOX(cp))
	{
		top = cp->tool_box;
	}
	/* menu_bar is shown */
	else if (SHOW_MENU_BAR(cp))
	{
		top = cp->menu_bar;
	}

	/* bottom_view is shown */
	if (SHOW_BOTTOM_VIEW(cp))
	{
		bottom = cp->bottom_view;
	}
	/* dash_board is shown */
	else if (SHOW_DASH_BOARD(cp))
	{
		bottom = cp->dash_board;
	}

	if (_XfeIsAlive(top))
	{
		y = _XfeY(top) + _XfeHeight(top) + cp->spacing;
	}
	else
	{
		y = _XfemRectY(w);
	}

	if (_XfeIsAlive(bottom))
	{
		y2 = _XfeY(bottom) - cp->spacing;
	}
	else
	{
		y2 = _XfeHeight(w) - _XfemOffsetBottom(w);
	}

	/* left_view is shown */
	if (SHOW_LEFT_VIEW(cp))
	{
		x = _XfemRectX(w) + _XfeWidth(cp->left_view);

		width = _XfemRectWidth(w) - _XfeWidth(cp->left_view);
	}
	else
	{
		x = _XfemRectX(w);

		width = _XfemRectWidth(w);
	}
	
	/* right_view is shown */
	if (SHOW_RIGHT_VIEW(cp))
	{
		width -= _XfeWidth(cp->right_view);
	}

	height = y2 - y;

	/* Place the center_view */
	_XfeConfigureWidget(cp->center_view,x,y,width,height);
}
/*----------------------------------------------------------------------*/
static void
LayoutLeftView(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);
	Widget			top = NULL;
	Widget			bottom = NULL;
	Position		y1;
	Position		y2;

	assert( SHOW_LEFT_VIEW(cp) );

#if 0
	/* tool_box is shown */
	if (SHOW_TOOL_BOX(cp))
	{
		top = cp->tool_box;
	}
	/* menu_bar is shown */
	else if (SHOW_MENU_BAR(cp))
	{
		top = cp->menu_bar;
	}

	if (_XfeIsAlive(top))
	{
		y1 = _XfeY(top) + _XfeHeight(top) + cp->spacing;
	}
	else
	{
		y1 = _XfemRectY(w);
	}

	/* Place the top_view */
	_XfeConfigureWidget(cp->top_view,
						
						_XfemRectX(w),
							
						y1,
							
						_XfemRectWidth(w),

						_XfeHeight(cp->top_view));

#endif

}
/*----------------------------------------------------------------------*/
static void
LayoutRightView(Widget w)
{
    XfeChromePart *	cp = _XfeChromePart(w);
	Widget			top = NULL;
	Widget			bottom = NULL;
	Position		y1;
	Position		y2;

	assert( SHOW_RIGHT_VIEW(cp) );

#if 0
	/* tool_box is shown */
	if (SHOW_TOOL_BOX(cp))
	{
		top = cp->tool_box;
	}
	/* menu_bar is shown */
	else if (SHOW_MENU_BAR(cp))
	{
		top = cp->menu_bar;
	}

	if (_XfeIsAlive(top))
	{
		y1 = _XfeY(top) + _XfeHeight(top) + cp->spacing;
	}
	else
	{
		y1 = _XfemRectY(w);
	}

	/* Place the top_view */
	_XfeConfigureWidget(cp->top_view,
						
						_XfemRectX(w),
							
						y1,
							
						_XfemRectWidth(w),

						_XfeHeight(cp->top_view));

#endif

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeChrome Public Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
Widget
XfeCreateChrome(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfeChromeWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeChromeGetComponent(Widget chrome,unsigned char component)
{
    XfeChromePart *		cp = _XfeChromePart(chrome);
	Widget				cw = NULL;

	switch(component)
	{
	case XmCHROME_MENU_BAR:
		cw = cp->menu_bar;
		break;

	case XmCHROME_TOOL_BOX:
		cw = cp->tool_box;
		break;

	case XmCHROME_TOP_VIEW:
		cw = cp->top_view;
		break;

	case XmCHROME_CENTER_VIEW:
		cw = cp->center_view;
		break;

	case XmCHROME_BOTTOM_VIEW:
		cw = cp->bottom_view;
		break;

	case XmCHROME_DASH_BOARD:
		cw = cp->dash_board;
		break;
	}

	return cw;
}
/*----------------------------------------------------------------------*/
