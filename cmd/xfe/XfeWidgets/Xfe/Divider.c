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
/* Name:		<Xfe/Divider.c>											*/
/* Description:	XfeDivider widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/DividerP.h>

#include <stdio.h>

#define MESSAGE1 "Widget is not an XfeDivider."
#define MESSAGE2 "XmNdividerChildType can only be set at creationg time."
#define MESSAGE3 "Only one child attachment can be XmNalwaysVisible."
#define MESSAGE4 "The descendant for XfeDividerAddDragDescendant() is invalid."
#define MESSAGE5 "Cannot find a valid ancestor attachment for '%s'."

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		CoreClassInitialize	(void);
static void 	CoreInitialize		(Widget,Widget,ArgList,Cardinal *);
static void 	CoreDestroy			(Widget);
static Boolean	CoreSetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry			(Widget,Dimension *,Dimension *);
static Boolean	AcceptStaticChild			(Widget);
static Boolean	InsertStaticChild			(Widget);
static Boolean	DeleteStaticChild			(Widget);
static void		LayoutStaticChildren		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Static layout functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutStaticVertical		(Widget);
static void		LayoutStaticHorizontal		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Rep type registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		DividerRegisterRepTypes	(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDivider resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static const XtResource resources[] = 	
{					

	/* Divider resources */
    { 
		XmNdividerOffset,
		XmCDividerOffset,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeDividerRec , xfe_divider . divider_offset),
		XmRImmediate, 
		(XtPointer) 100
    },
    { 
		XmNdividerPercentage,
		XmCDividerPercentage,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeDividerRec , xfe_divider . divider_percentage),
		XmRImmediate, 
		(XtPointer) 20
    },
    { 
		XmNdividerTarget,
		XmCDividerTarget,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeDividerRec , xfe_divider . divider_target),
		XmRImmediate, 
		(XtPointer) 0
    },
	{
		XmNdividerType,
		XmCDividerType,
		XmRDividerType,
		sizeof(unsigned char),
		XtOffsetOf(XfeDividerRec , xfe_divider . divider_type),
		XmRImmediate, 
		(XtPointer) XmDIVIDER_PERCENTAGE
	},

	/* Force XmNusePreferredHeight and XmNusePreferredWidth to False */
	{
		XmNusePreferredHeight,
		XmCUsePreferredHeight,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeDividerRec , xfe_manager . use_preferred_height),
		XmRImmediate, 
		(XtPointer) False
	},
	{
		XmNusePreferredWidth,
		XmCUsePreferredWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeDividerRec , xfe_manager . use_preferred_width),
		XmRImmediate, 
		(XtPointer) False
	},
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(divider,Divider) =
{
    {
		(WidgetClass) &xfeOrientedClassRec,		/* superclass			*/
		"XfeDivider",							/* class_name			*/
		sizeof(XfeDividerRec),					/* widget_size			*/
		CoreClassInitialize,					/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		CoreInitialize,							/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		NULL,									/* actions				*/
		0,										/* num_actions			*/
		(XtResource *)resources,				/* resources			*/
		XtNumber(resources),                    /* num_resources		*/
		NULLQUARK,                              /* xrm_class			*/
		TRUE,                                   /* compress_motion		*/
		XtExposeCompressMaximal,                /* compress_exposure	*/
		TRUE,                                   /* compress_enterleave	*/
		FALSE,                                  /* visible_interest		*/
		CoreDestroy,							/* destroy				*/
		XtInheritResize,                        /* resize				*/
		XtInheritExpose,						/* expose				*/
		CoreSetValues,							/* set_values			*/
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
		XtInheritGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },

    /* Constraint Part */
    {
		NULL,									/* constraint res		*/
		0,										/* num constraint res	*/
		sizeof(XfeOrientedConstraintRec),		/* constraint size		*/
		NULL,									/* init proc			*/
		NULL,									/* destroy proc			*/
		NULL,									/* set values proc		*/
		NULL,                                   /* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		NULL,									/* syn resources		*/
		0,										/* num syn_resources	*/
		NULL,									/* syn_cont_resources	*/
		0,										/* num_syn_cont_resource*/
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
		NULL,									/* layout_components		*/
		NULL,									/* draw_background			*/
		XfeInheritDrawShadow,					/* draw_shadow				*/
		NULL,									/* draw_components			*/
		NULL,									/* extension				*/
    },

	/* XfeDynamicManager Part */
    {
		NULL,									/* accept_dynamic_child		*/
		NULL,									/* insert_dynamic_child		*/
		NULL,									/* delete_dynamic_child		*/
		NULL,									/* layout_dynamic_children	*/
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
	
    /* XfeDivider Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeDividerWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(divider,Divider);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CoreClassInitialize()
{
	/* Register XfeDivider Representation Types */
    DividerRegisterRepTypes();
}
/*----------------------------------------------------------------------*/
static void
CoreInitialize(Widget rw,Widget nw,ArgList av,Cardinal * ac)
{
    XfeDividerPart *		dp = _XfeDividerPart(nw);

    /* Make sure rep types are ok */
 	XfeRepTypeCheck(nw,XmRDividerType,&dp->divider_type,XmDIVIDER_PERCENTAGE);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeDividerWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
CoreDestroy(Widget w)
{
    XfeDividerPart *		dp = _XfeDividerPart(w);

}
/*----------------------------------------------------------------------*/
static Boolean
CoreSetValues(Widget ow,Widget rw,Widget nw,ArgList av,Cardinal * ac)
{
    XfeDividerPart *		np = _XfeDividerPart(nw);
    XfeDividerPart *		op = _XfeDividerPart(ow);

	/* XmNdividerOffset */
	if (np->divider_offset != op->divider_offset)
	{
		_XfemConfigFlags(nw) |= XfeConfigLayout;
	}

	/* XmNdividerPercentage */
	if (np->divider_percentage != op->divider_percentage)
	{
		_XfemConfigFlags(nw) |= XfeConfigLayout;
	}


	/* XmNdividerTarget */
	if (np->divider_target != op->divider_target)
	{
		_XfemConfigFlags(nw) |= XfeConfigLayout;
	}

	/* XmNdividerType */
	if (np->divider_type != op->divider_type)
	{
		_XfemConfigFlags(nw) |= XfeConfigGLE;

		/* Make sure the new divider type is ok */
		XfeRepTypeCheck(nw,XmRDividerType,&np->divider_type,
						XmDIVIDER_PERCENTAGE);
	}

    return _XfeManagerChainSetValues(ow,rw,nw,xfeDividerWidgetClass);
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
/* 	XfeDividerPart *		dp = _XfeDividerPart(w); */

	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
 		*width += _XfemMaxStaticWidth(w);
		*height += _XfemTotalStaticWidth(w);
	}
	/* Horizontal */
	else
	{
 		*width += _XfemTotalStaticWidth(w);
		*height += _XfemMaxStaticWidth(w);
	}
}
/*----------------------------------------------------------------------*/
static Boolean
AcceptStaticChild(Widget child)
{
    Widget w = _XfeParent(child);

	/* We can accept up to 2 children */
	return (_XfemNumStaticChildren(w) < 2);
}
/*----------------------------------------------------------------------*/
static Boolean
InsertStaticChild(Widget child)
{
/*     Widget						w = _XfeParent(child); */
/*     XfeDividerPart *			dp = _XfeDividerPart(w); */
/* 	XfeDividerConstraintPart *	cp = _XfeDividerConstraintPart(child); */

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteStaticChild(Widget child)
{
/*     Widget				w = XtParent(child); */
/*     XfeDividerPart *		dp = _XfeDividerPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static void
LayoutStaticChildren(Widget w)
{
    XfeDividerPart *	dp = _XfeDividerPart(w);

	/* Vertical */
	if (_XfeOrientedOrientation(w) == XmVERTICAL)
	{
		LayoutStaticVertical(w);
	}
	/* Horizontal */
	else
	{
		LayoutStaticHorizontal(w);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Static layout functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutStaticVertical(Widget w)
{
    XfeDividerPart *	dp = _XfeDividerPart(w);
    Widget				one = _XfemStaticChildrenIndex(w,0);
    Widget				two = _XfemStaticChildrenIndex(w,1);

	int			one_x;
	int			one_y;
	int			one_width;
	int			one_height;

	int			two_x;
	int			two_y;
	int			two_width;
	int			two_height;
	
	assert( dp->divider_target == 0 || dp->divider_target == 1 );
	assert( dp->divider_offset > 0 );
	assert( dp->divider_offset <= _XfeWidth(w) );
	assert( dp->divider_percentage >= 0 );
	assert( dp->divider_percentage <= 100 );

	/* children[0] is the target */
	if (dp->divider_target == 0)
	{
		/* Offset */
		if (dp->divider_type == XmDIVIDER_OFFSET)
		{
			one_height = dp->divider_offset;
			one_width = _XfemBoundaryWidth(w);
			one_x = _XfemBoundaryX(w);
			one_y = _XfemBoundaryY(w);

			two_height = _XfemBoundaryHeight(w) - one_height;
			two_width = _XfemBoundaryWidth(w);
			two_x = _XfemBoundaryY(w);
			two_y = _XfemBoundaryY(w) + _XfemBoundaryHeight(w) - two_height;
		}
		/* Percentage */
		else
		{
			one_height = (dp->divider_percentage * _XfemBoundaryHeight(w)) / 100;
			one_width = _XfemBoundaryWidth(w);
			one_x = _XfemBoundaryX(w);
			one_y = _XfemBoundaryY(w);

			two_height = _XfemBoundaryHeight(w) - one_height;
			two_width = _XfemBoundaryWidth(w);
			two_x = _XfemBoundaryX(w);
			two_y = _XfemBoundaryY(w) + _XfemBoundaryHeight(w) - two_height;
		}
	}
	/* children[1] is the target */
	else
	{
		/* Offset */
		if (dp->divider_type == XmDIVIDER_OFFSET)
		{
			two_height = dp->divider_offset;
			two_width = _XfemBoundaryWidth(w);
			two_x = _XfemBoundaryY(w);
			two_y = _XfemBoundaryY(w) + _XfemBoundaryHeight(w) - two_height;

			one_height = _XfemBoundaryHeight(w) - two_height;
			one_width = _XfemBoundaryWidth(w);
			one_x = _XfemBoundaryX(w);
			one_y = _XfemBoundaryY(w);
		}
		/* Percentage */
		else
		{
			two_height = (dp->divider_percentage * _XfemBoundaryHeight(w)) / 100;
			two_width = _XfemBoundaryWidth(w);
			two_x = _XfemBoundaryX(w);
			two_y = _XfemBoundaryY(w) + _XfemBoundaryHeight(w) - two_height;

			one_height = _XfemBoundaryHeight(w) - two_height;
			one_width = _XfemBoundaryWidth(w);
			one_x = _XfemBoundaryX(w);
			one_y = _XfemBoundaryY(w);
		}
	}

	if (_XfeIsAlive(one))
	{
		_XfeConfigureWidget(one,one_x,one_y,one_width,one_height);
	}

	if (_XfeIsAlive(two))
	{
		_XfeConfigureWidget(two,two_x,two_y,two_width,two_height);
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutStaticHorizontal(Widget w)
{
    XfeDividerPart *	dp = _XfeDividerPart(w);
    Widget				one = _XfemStaticChildrenIndex(w,0);
    Widget				two = _XfemStaticChildrenIndex(w,1);

	int			one_x;
	int			one_y;
	int			one_width;
	int			one_height;

	int			two_x;
	int			two_y;
	int			two_width;
	int			two_height;
	
	assert( dp->divider_target == 0 || dp->divider_target == 1 );
	assert( dp->divider_offset > 0 );
	assert( dp->divider_offset <= _XfeWidth(w) );
	assert( dp->divider_percentage >= 0 );
	assert( dp->divider_percentage <= 100 );

	/* children[0] is the target */
	if (dp->divider_target == 0)
	{
		/* Offset */
		if (dp->divider_type == XmDIVIDER_OFFSET)
		{
			one_width = dp->divider_offset;
			one_height = _XfemBoundaryHeight(w);
			one_x = _XfemBoundaryX(w);
			one_y = _XfemBoundaryY(w);

			two_width = _XfemBoundaryWidth(w) - one_width;
			two_height = _XfemBoundaryHeight(w);
			two_x = _XfemBoundaryX(w) + _XfemBoundaryWidth(w) - two_width;
			two_y = _XfemBoundaryY(w);
		}
		/* Percentage */
		else
		{
			one_width = (dp->divider_percentage * _XfemBoundaryWidth(w)) / 100;
			one_height = _XfemBoundaryHeight(w);
			one_x = _XfemBoundaryX(w);
			one_y = _XfemBoundaryY(w);

			two_width = _XfemBoundaryWidth(w) - one_width;
			two_height = _XfemBoundaryHeight(w);
			two_x = _XfemBoundaryX(w) + _XfemBoundaryWidth(w) - two_width;
			two_y = _XfemBoundaryY(w);
		}
	}
	/* children[1] is the target */
	else
	{
		/* Offset */
		if (dp->divider_type == XmDIVIDER_OFFSET)
		{
			two_width = dp->divider_offset;
			two_height = _XfemBoundaryHeight(w);
			two_x = _XfemBoundaryX(w) + _XfemBoundaryWidth(w) - two_width;
			two_y = _XfemBoundaryY(w);

			one_width = _XfemBoundaryWidth(w) - two_width;
			one_height = _XfemBoundaryHeight(w);
			one_x = _XfemBoundaryX(w);
			one_y = _XfemBoundaryY(w);
		}
		/* Percentage */
		else
		{
			two_width = (dp->divider_percentage * _XfemBoundaryWidth(w)) / 100;
			two_height = _XfemBoundaryHeight(w);
			two_x = _XfemBoundaryX(w) + _XfemBoundaryWidth(w) - two_width;
			two_y = _XfemBoundaryY(w);

			one_width = _XfemBoundaryWidth(w) - two_width;
			one_height = _XfemBoundaryHeight(w);
			one_x = _XfemBoundaryX(w);
			one_y = _XfemBoundaryY(w);
		}
	}

	if (_XfeIsAlive(one))
	{
		_XfeConfigureWidget(one,one_x,one_y,one_width,one_height);
	}

	if (_XfeIsAlive(two))
	{
		_XfeConfigureWidget(two,two_x,two_y,two_width,two_height);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Rep type registration functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
DividerRegisterRepTypes(void)
{
    static String divider_names[] = 
    { 
		"divider_offset",
		"divider_percentage",
		NULL
    };

    XfeRepTypeRegister(XmRDividerType,divider_names);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDivider public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreateDivider(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfeDividerWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
