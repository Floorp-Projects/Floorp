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
/* Name:		<Xfe/Manager.c>											*/
/* Description:	XfeDynamicManager widget source.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <stdio.h>
#include <assert.h>

#include <Xfe/DynamicManagerP.h>

#define MESSAGE5 "Widget is not an XfeDynamicManager"
#define MESSAGE14 "Cannot accept new child '%s'."
#define MESSAGE21 "XmNlinkNode is a read-only resource."
#define MESSAGE70 "XmNdynamicChildren is a read-only resource."
#define MESSAGE71 "XmNnumDynamicChildren is a read-only resource."
#define MESSAGE72 "XmNmaxDynamicWidth is a read-only resource."
#define MESSAGE73 "XmNmaxDynamicHeight is a read-only resource."
#define MESSAGE74 "XmNminDynamicWidth is a read-only resource."
#define MESSAGE75 "XmNminDynamicHeight is a read-only resource."
#define MESSAGE76 "XmNtotalDynamicWidth is a read-only resource."
#define MESSAGE77 "XmNtotalDynamicHeight is a read-only resource."

#define MIN_LAYOUT_WIDTH	10
#define MIN_LAYOUT_HEIGHT	10

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		CoreInitialize		(Widget,Widget,ArgList,Cardinal *);
static void		CoreClassPartInit	(WidgetClass);
static void		CoreDestroy			(Widget);
static Boolean	CoreSetValues		(Widget,Widget,Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* static void				InsertChild		(Widget); */
/* static void				DeleteChild		(Widget); */
/* static void				ChangeManaged	(Widget); */
/* static XtGeometryResult GeometryManager	(Widget,XtWidgetGeometry *, */
/* 										 XtWidgetGeometry *); */

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		CompositeInsertChild	(Widget);
static void		CompositeDeleteChild	(Widget);
static void		ConstraintInitialize	(Widget,Widget,ArgList,Cardinal *);
static Boolean	ConstraintSetValues		(Widget,Widget,Widget,ArgList,
										 Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		LayoutWidget			(Widget);
static void		UpdateChildrenInfo		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager class methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		GetChildDimensions		(Widget,Dimension *,Dimension *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Layable children functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
#if 0
static void		LayableInfoUpdate		(Widget);
static void		LayableInfoFree			(Widget);
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Dynamic children info functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		UpdateDynamicChildrenInfo	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager resources											*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
	/* Dynamic children resources */
	{ 
		XmNdynamicChildren,
		XmCReadOnly,
		XmRLinkedChildren,
		sizeof(XfeLinked),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . dynamic_children),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNnumDynamicChildren,
		XmCReadOnly,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . num_dyn_children),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNnumManagedDynamicChildren,
		XmCReadOnly,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . num_managed_dyn_children),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNmaxDynamicChildrenWidth,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . max_dyn_width),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNmaxDynamicChildrenHeight,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . max_dyn_height),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNminDynamicChildrenWidth,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . min_dyn_width),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNminDynamicChildrenHeight,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . min_dyn_height),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNtotalDynamicChildrenWidth,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . total_dyn_width),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNtotalDynamicChildrenHeight,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeDynamicManagerRec , xfe_dynamic_manager . total_dyn_height),
		XmRImmediate, 
		(XtPointer) 0
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager constraint resources								*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource constraint_resources[] = 
{
    { 
		XmNpositionIndex,
		XmCPositionIndex,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeDynamicManagerConstraintRec , xfe_dynamic_manager . position_index),
		XmRImmediate,
		(XtPointer) XmLAST_POSITION
    },
    { 
		XmNlinkNode,
		XmCReadOnly,
		XmRPointer,
		sizeof(XfeLinkNode),
		XtOffsetOf(XfeDynamicManagerConstraintRec , xfe_dynamic_manager . link_node),
		XmRImmediate,
		(XtPointer) NULL
    },
};   
/*----------------------------------------------------------------------*/

/* Widget class record initialization. */
_XFE_WIDGET_CLASS_RECORD(dynamicmanager,DynamicManager) =
{
    {
		(WidgetClass) &xfeManagerClassRec,		/* superclass			*/
		"XfeDynamicManager",					/* class_name			*/
		sizeof(XfeDynamicManagerRec),			/* widget_size			*/
		NULL,									/* class_initialize		*/
		CoreClassPartInit,						/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		CoreInitialize,							/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		NULL,									/* actions				*/
		0,										/* num_actions			*/
		resources,								/* resources			*/
		XtNumber(resources),					/* num_resources		*/
		NULLQUARK,								/* xrm_class			*/
		TRUE,									/* compress_motion		*/
		XtExposeCompressMaximal,				/* compress_exposure	*/
		TRUE,									/* compress_enterleave	*/
		FALSE,									/* visible_interest		*/
		CoreDestroy,							/* destroy				*/
		XtInheritResize,						/* resize				*/
		XtInheritExpose,						/* expose				*/
		CoreSetValues,							/* set_values			*/
		NULL,									/* set_values_hook		*/
		XtInheritSetValuesAlmost,				/* set_values_almost	*/
		NULL,									/* get_values_hook		*/
		NULL,									/* accept_focus			*/
		XtVersion,								/* version				*/
		NULL,									/* callback_private		*/
		XtInheritTranslations,					/* tm_table				*/
		XtInheritQueryGeometry,					/* query_geometry		*/
		XtInheritDisplayAccelerator,			/* display accel		*/
		NULL,									/* extension			*/
    },

    /* Composite Part */
    {
		XtInheritGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		CompositeInsertChild,					/* insert_child			*/
		CompositeDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },
    
    /* Constraint Part */
    {
		constraint_resources,					/* resource list		*/
		XtNumber(constraint_resources),			/* num resources		*/
		sizeof(XfeDynamicManagerConstraintRec),	/* constraint size		*/
		ConstraintInitialize,					/* init proc			*/
		NULL,									/* destroy proc			*/
		ConstraintSetValues,					/* set values proc		*/
		NULL,									/* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		NULL,									/* syn resources		*/
		0,										/* num syn_resources	*/
		NULL,									/* syn_cont_resources	*/
		0,										/* num_syn_cont_res		*/
		XmInheritParentProcess,					/* parent_process		*/
		NULL,									/* extension			*/
	},

	/* XfeManager Part */
    {
		XfeInheritBitGravity,					/* bit_gravity				*/
		NULL,									/* preferred_geometry		*/
		XfeInheritUpdateBoundary,				/* update_boundary			*/
		UpdateChildrenInfo,						/* update_children_info		*/
		LayoutWidget,							/* layout_widget			*/
		NULL,									/* accept_static_child		*/
		NULL,									/* insert_static_child		*/
		NULL,									/* delete_static_child		*/
		NULL,									/* layout_static_children	*/
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
		GetChildDimensions,						/* get_child_dimensions		*/
		NULL,									/* extension				*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeManagerWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(dynamicmanager,DynamicManager);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CoreClassPartInit(WidgetClass wc)
{
    XfeDynamicManagerWidgetClass cc = (XfeDynamicManagerWidgetClass) wc;
    XfeDynamicManagerWidgetClass sc = (XfeDynamicManagerWidgetClass) wc->core_class.superclass;

	/* Dynamic children methods */
    _XfeResolve(cc,sc,xfe_dynamic_manager_class,accept_dynamic_child,
				XfeInheritAcceptDynamicChild);

    _XfeResolve(cc,sc,xfe_dynamic_manager_class,insert_dynamic_child,
				XfeInheritInsertDynamicChild);

    _XfeResolve(cc,sc,xfe_dynamic_manager_class,delete_dynamic_child,
				XfeInheritDeleteDynamicChild);

    _XfeResolve(cc,sc,xfe_dynamic_manager_class,layout_dynamic_children,
				XfeInheritLayoutDynamicChildren);

    _XfeResolve(cc,sc,xfe_dynamic_manager_class,get_child_dimensions,
				XfeInheritGetChildDimensions);
}
/*----------------------------------------------------------------------*/
static void
CoreInitialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
	/* Create the dynamic children list */
	_XfemDynamicChildren(nw) = XfeLinkedConstruct();
	
    /* Finish of initialization */
	_XfeManagerChainInitialize(rw,nw,xfeDynamicManagerWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
CoreDestroy(Widget w)
{
	/* Destroy the dynamic children list if needed */
	if (_XfemDynamicChildren(w) != NULL)
	{
		XfeLinkedDestroy(_XfemDynamicChildren(w),NULL,NULL);
	}
}
/*----------------------------------------------------------------------*/
static Boolean
CoreSetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
	/* XmNdynamicChildren */
	if (_XfemDynamicChildren(nw) != _XfemDynamicChildren(ow))
	{
		_XfemDynamicChildren(nw) = _XfemDynamicChildren(ow);
      
		_XfeWarning(nw,MESSAGE70);
	}

	/* XmNnumDynamicChildren */
	if (_XfemNumDynamicChildren(nw) != _XfemNumDynamicChildren(ow))
	{
		_XfemNumDynamicChildren(nw) = _XfemNumDynamicChildren(ow);
      
		_XfeWarning(nw,MESSAGE71);
	}

	/* XmNmaxDynamicWidth */
	if (_XfemMaxDynamicWidth(nw) != _XfemMaxDynamicWidth(ow))
	{
		_XfemMaxDynamicWidth(nw) = _XfemMaxDynamicWidth(ow);
      
		_XfeWarning(nw,MESSAGE72);
	}

	/* XmNmaxDynamicHeight */
	if (_XfemMaxDynamicHeight(nw) != _XfemMaxDynamicHeight(ow))
	{
		_XfemMaxDynamicHeight(nw) = _XfemMaxDynamicHeight(ow);
      
		_XfeWarning(nw,MESSAGE73);
	}

	/* XmNminDynamicWidth */
	if (_XfemMinDynamicWidth(nw) != _XfemMinDynamicWidth(ow))
	{
		_XfemMinDynamicWidth(nw) = _XfemMinDynamicWidth(ow);
      
		_XfeWarning(nw,MESSAGE74);
	}

	/* XmNminDynamicHeight */
	if (_XfemMinDynamicHeight(nw) != _XfemMinDynamicHeight(ow))
	{
		_XfemMinDynamicHeight(nw) = _XfemMinDynamicHeight(ow);
      
		_XfeWarning(nw,MESSAGE75);
	}

	/* XmNtotalDynamicWidth */
	if (_XfemTotalDynamicWidth(nw) != _XfemTotalDynamicWidth(ow))
	{
		_XfemTotalDynamicWidth(nw) = _XfemTotalDynamicWidth(ow);
      
		_XfeWarning(nw,MESSAGE76);
	}

	/* XmNtotalDynamicHeight */
	if (_XfemTotalDynamicHeight(nw) != _XfemTotalDynamicHeight(ow))
	{
		_XfemTotalDynamicHeight(nw) = _XfemTotalDynamicHeight(ow);
      
		_XfeWarning(nw,MESSAGE77);
	}

	return _XfeManagerChainSetValues(ow,rw,nw,xfeDynamicManagerWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CompositeInsertChild(Widget child)
{
    Widget w = XtParent(child);

	/*
	 * Component & Static children
	 */
    if (_XfemComponentFlag(w) || 
		(_XfeConstraintManagerChildType(child) == XmMANAGER_COMPONENT_CHILD) ||
		_XfeManagerAcceptStaticChild(child))
	{
		(*xfeManagerClassRec.composite_class.insert_child)(child);
	}
	/*
	 * Dynamic children
	 */
	else if (_XfeDynamicManagerAcceptDynamicChild(child))
	{
		/* Mark the child STATIC */
		_XfeConstraintManagerChildType(child) = XmMANAGER_DYNAMIC_CHILD;

		/* Add the child to the dynamic children list */
		XfeLinkedInsertAtTail(_XfemDynamicChildren(w),child);

		/* Update the dynamic children count */
		_XfemNumDynamicChildren(w) = 
			XfeLinkedCount(_XfemDynamicChildren(w));

        /* Call XmManager's XtComposite InsertChild to do the Xt magic */
        (*xmManagerClassRec.composite_class.insert_child)(child);
		
        /* Insert the dynamic child and relayout if needed */
        if (_XfeDynamicManagerInsertDynamicChild(child))
        {
            XfeManagerLayout(w);
        }
	}
	else
 	{
        _XfeArgWarning(w,MESSAGE14,XtName(child));
    }
}
/*----------------------------------------------------------------------*/
static void
CompositeDeleteChild(Widget child)
{
    Widget w = XtParent(child);

	/*
	 * Component & Static children
	 */
	if (_XfeConstraintManagerChildType(child) == XmMANAGER_COMPONENT_CHILD ||
		_XfeConstraintManagerChildType(child) == XmMANAGER_STATIC_CHILD)
	{
		(*xfeManagerClassRec.composite_class.delete_child)(child);
	}
	/*
	 * Dynamic children:
	 */
	else if (_XfeConstraintManagerChildType(child) == XmMANAGER_DYNAMIC_CHILD)
	{
		XfeLinkNode node;
		Boolean		need_layout = False;
		Boolean		is_managed = _XfeIsManaged(child);

		assert( _XfemDynamicChildren(w) != NULL );
		
		node = XfeLinkedFindNodeByItem(_XfemDynamicChildren(w),child);

		assert( node != NULL );

		if (node != NULL)
		{
			XfeLinkedRemoveNode(_XfemDynamicChildren(w),node);

			/* Update the dynamic children count */
			_XfemNumDynamicChildren(w) = 
				XfeLinkedCount(_XfemDynamicChildren(w));
		}
		
        /* Delete the dynamic child */
        need_layout = _XfeDynamicManagerDeleteDynamicChild(child);
		
		/* Call XmManager's CompositeDeleteChild to do the Xt magic */
        (*xmManagerClassRec.composite_class.delete_child)(child);
		
		/* Do layout if needed */
		if (need_layout && is_managed)
		{
			XfeManagerLayout(w);
		}
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ConstraintInitialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
/* 	Widget						w = XtParent(nw); */
/*	XfeDynamicManagerConstraintPart *	cp = _XfeDynamicManagerConstraintPart(nw);*/

	/* Finish constraint initialization */
	_XfeConstraintChainInitialize(rw,nw,xfeDynamicManagerWidgetClass);
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
/* 	Widget								w = XtParent(nw); */
 	XfeDynamicManagerConstraintPart *	np = _XfeDynamicManagerConstraintPart(nw);
 	XfeDynamicManagerConstraintPart *	op = _XfeDynamicManagerConstraintPart(ow);

	/* XmNlinkNode */
	if (np->link_node != op->link_node)
	{
		np->link_node = op->link_node;

		_XfeWarning(nw,MESSAGE21);
	}

	/* XmNpositionIndex */
	if (np->position_index != op->position_index)
	{
		/* Do something magical */
	}

	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(ow,rw,nw,xfeDynamicManagerWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
LayoutWidget(Widget w)
{
    /* Layout the components */
    _XfeManagerLayoutComponents(w);

    /* Layout the dynamic children */
	_XfeDynamicManagerLayoutDynamicChildren(w);

    /* Layout the static children */
    _XfeManagerLayoutStaticChildren(w);
}
/*----------------------------------------------------------------------*/
static void
UpdateChildrenInfo(Widget w)
{
	/* Update the component children */
	_XfeManagerUpdateComponentChildrenInfo(w);

	/* Update the static children */
	_XfeManagerUpdateStaticChildrenInfo(w);

	/* Update the dynamic children */
	UpdateDynamicChildrenInfo(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager class methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
GetChildDimensions(Widget child,Dimension * width_out,Dimension * height_out)
{
	*width_out	= _XfeWidth(child);
	*height_out = _XfeHeight(child);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Dynamic children info functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* static */ void
UpdateDynamicChildrenInfo(Widget w)
{
	XfeDynamicManagerWidgetClass mc = (XfeDynamicManagerWidgetClass) XtClass(w);

	if (_XfemDynamicChildren(w) == NULL)
	{
		return;
	}

/* 	printf("UpdateDynamicChildrenInfo(w)\n"); */

	_XfeManagerGetChildrenInfo(w,
                               _XfemDynamicChildren(w),
							   XfeCHILDREN_INFO_ALIVE|XfeCHILDREN_INFO_MANAGED,
							   mc->xfe_dynamic_manager_class.get_child_dimensions,
							   &_XfemMaxDynamicWidth(w),
							   &_XfemMaxDynamicHeight(w),
							   &_XfemMinDynamicWidth(w),
							   &_XfemMinDynamicHeight(w),
							   &_XfemTotalDynamicWidth(w),
							   &_XfemTotalDynamicHeight(w),
							   &_XfemNumManagedDynamicChildren(w));

/* 	_XfemNumDynamicChildren(w) = XfeLinkedCount(_XfemDynamicChildren(w)); */

#if 0
	printf("UpdateDynamicChildrenInfo(%s): max = (%d,%d)\t\ttotal = (%d,%d)\n",
           XtName(w),
		   _XfemMaxDynamicWidth(w),
		   _XfemMaxDynamicHeight(w),
		   _XfemTotalDynamicWidth(w),
		   _XfemTotalDynamicHeight(w));
#endif
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager method invocation functions						*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeDynamicManagerAcceptDynamicChild(Widget child)
{
	Boolean					result = XtIsManaged(child);
	Widget					w = XtParent(child);
	XfeDynamicManagerWidgetClass	mc = (XfeDynamicManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_dynamic_manager_class.accept_dynamic_child)
	{
		result = (*mc->xfe_dynamic_manager_class.accept_dynamic_child)(child);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeDynamicManagerInsertDynamicChild(Widget child)
{
	Boolean					result = XtIsManaged(child);
	Widget					w = XtParent(child);
	XfeDynamicManagerWidgetClass	mc = (XfeDynamicManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_dynamic_manager_class.insert_dynamic_child)
	{
		result = (*mc->xfe_dynamic_manager_class.insert_dynamic_child)(child);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeDynamicManagerDeleteDynamicChild(Widget child)
{
	Boolean					result = XtIsManaged(child);
	Widget					w = XtParent(child);
	XfeDynamicManagerWidgetClass	mc = (XfeDynamicManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_dynamic_manager_class.delete_dynamic_child)
	{
		result = (*mc->xfe_dynamic_manager_class.delete_dynamic_child)(child);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeDynamicManagerLayoutDynamicChildren(Widget w)
{
	XfeDynamicManagerWidgetClass mc = (XfeDynamicManagerWidgetClass) XtClass(w);
	
 	if ((_XfeWidth(w) <= MIN_LAYOUT_WIDTH) || 
		(_XfeHeight(w) <= MIN_LAYOUT_HEIGHT))
	{
		return;
	}
	
	if (mc->xfe_dynamic_manager_class.layout_dynamic_children)
	{
		(*mc->xfe_dynamic_manager_class.layout_dynamic_children)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeDynamicManagerGetChildDimensions(Widget			child,
									 Dimension *	width_out,
									 Dimension *	height_out)
{
	Widget							w = XtParent(child);
	XfeDynamicManagerWidgetClass	mc = 
		(XfeDynamicManagerWidgetClass) XtClass(w);

	assert( width_out != NULL );
	assert( height_out != NULL );
	assert( child != NULL );
	assert( XfeIsDynamicManager(w) );
	
	if (mc->xfe_dynamic_manager_class.get_child_dimensions)
	{
		(*mc->xfe_dynamic_manager_class.get_child_dimensions)(child,
															  width_out,
															  height_out);
	}
}
/*----------------------------------------------------------------------*/

#if 0
/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		_XfeDynamicManagerChildrenInfo()								*/
/*																		*/
/* Purpose:		Obtain information about the manager's children			*/
/*																		*/
/* Ret Val:		void													*/
/*																		*/
/* Args in:		w					The manager widget.					*/
/*																		*/
/* Args out:	max_width_out		Max managed child width				*/
/*				max_height_out		Max managed child height			*/
/*				num_managed_out		Num managed children				*/
/*				num_components_out	Num component children				*/
/*																		*/
/* Comments:	Output args can be NULL - for which no value is			*/
/*				assigned or returned.									*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeDynamicManagerChildrenInfo(Widget			w,
						Dimension *		max_width_out,
						Dimension *		max_height_out,
						Dimension *		total_width_out,
						Dimension *		total_height_out,
						Cardinal *		num_managed_out,
						Cardinal *		num_components_out)
{
	Dimension			max_width = 0;
	Dimension			max_height = 0;
	Dimension			total_width = 0;
	Dimension			total_height = 0;
	Cardinal			num_managed = 0;
	Cardinal			num_components = 0;
	Widget				child;
	Cardinal			i;

	if (!XfeIsManager(w))
	{
		_XfeWarning(w,MESSAGE5);

		return;
	}

	/* Iterate through all the items */
	for (i = 0; i < _XfemNumChildren(w); i++)
	{
		child = _XfemChildren(w)[i];

		/* Check for private components */
		if (_XfeDynamicManagerPrivateComponent(child))
		{
				num_components++;
		}
		else
		{
			/* Make sure widget is shown */
			if (_XfeChildIsShown(child))
			{
				Dimension width;
				Dimension height;

				if (XfeIsPrimitive(child))
				{
					width = _XfePreferredWidth(child);
				}
				else
				{
					width = _XfeWidth(child);
				}

				if (XfeIsPrimitive(child))
				{
					height = _XfePreferredHeight(child);
				}
				else
				{
					height = _XfeHeight(child);
				}

				/* Keep track of largest width */
				if (width > max_width)
				{
					max_width = width;
				}


				/* Keep track of largest height */
				if (height > max_height)
				{
					max_height = height;
				}

				total_width  += width;
				total_height += height;
				
				num_managed++;
			}
		}
	}

	/* Assign only required arguments */
	if (max_width_out) 
	{
		*max_width_out = max_width;
	}

	if (max_height_out) 
	{
		*max_height_out = max_height;
	}

	if (total_width_out) 
	{
		*total_width_out = total_width;
	}

	if (total_height_out) 
	{
		*total_height_out = total_height;
	}

	if (num_managed_out) 
	{
		*num_managed_out = num_managed;
	}

	if (num_components_out) 
	{
		*num_components_out = num_components;
	}
}
/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		_XfeDynamicManagerComponentInfo()								*/
/*																		*/
/* Purpose:		Obtain information about the manager's components		*/
/*																		*/
/* Ret Val:		void													*/
/*																		*/
/* Args in:		w					The manager widget.					*/
/*																		*/
/* Args out:	max_width_out		Max managed child width				*/
/*				max_height_out		Max managed child height			*/
/*																		*/
/* Comments:	Output args can be NULL - for which no value is			*/
/*				assigned or returned.									*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeDynamicManagerComponentInfo(Widget				w,
						 Dimension *		max_width_out,
						 Dimension *		max_height_out)
{
	Dimension			max_width = 0;
	Dimension			max_height = 0;
	Widget				child;
	Cardinal			i;

	if (!XfeIsManager(w))
	{
		_XfeWarning(w,MESSAGE5);

		return;
	}

	/* Iterate through all the items */
	for (i = 0; i < _XfemNumChildren(w); i++)
	{
		child = _XfemChildren(w)[i];

		/* Check for private components */
		if (_XfeDynamicManagerPrivateComponent(child) &&
			XtIsManaged(child) && 
			_XfeIsAlive(child))
		{
			/* Keep track of largest width */
			if (_XfeWidth(child) > max_width)
			{
				max_width = _XfeWidth(child);
			}

			/* Keep track of largest height */
			if (_XfeHeight(child) > max_height)
			{
				max_height = _XfeHeight(child);
			}
		}
	}

	/* Assign only required arguments */
	if (max_width_out) 
	{
		*max_width_out = max_width;
	}

	if (max_height_out) 
	{
		*max_height_out = max_height;
	}
}
/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		_XfeDynamicManagerGetLayableChildren()							*/
/*																		*/
/* Purpose:		Obtain a list of layable children						*/
/*																		*/
/* Ret Val:		void													*/
/*																		*/
/* Args in:		w							The manager widget.			*/
/*																		*/
/* Args out:	layable_children_out		Array of layable children	*/
/*				num_layable_children_out	Size of the array			*/
/*				max_width_out				Max layable children width.	*/
/*				max_height_out				Max layable children height.*/
/*																		*/
/* Comments:	Its up to the caller to XtFree() the value returned in	*/
/*              layable_children_out (if its not NULL)					*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeDynamicManagerGetLayableChildrenInfo(Widget						w,
								  XfeDynamicManagerChildrenInfoRec *	info)
{
	Widget				child;
	Cardinal			i;

	assert( _XfeIsAlive(w) );
	assert( XfeIsManager(w) );

	assert( info != NULL );

	/* Initialize the results */
	info->children = NULL;
	info->num_children = 0;

	info->max_width = 0;
	info->max_height = 0;

	info->min_width = 0;
	info->min_height = 0;

	info->total_width = 0;
	info->total_height = 0;
	
	/* Proceed only if children exist */
	if (_XfemNumChildren(w) > 0)
	{
		/* Allocate a layable children linked list */
		info->children = XfeLinkedConstruct();
		
		/* Iterate through all the items */
		for (i = 0; i < _XfemNumChildren(w); i++)
		{
			child = _XfeChildrenIndex(w,i);

			/* Look for layable children and add them to the list */
			if (_XfeDynamicManagerChildIsLayable(child))
			{
				/* Add a node to the list for this child */
				XfeLinkNode node = 
					XfeLinkedInsertAtTail(info->children,child);

				/* Point child's link node constraint resource to the node */
				_XfeDynamicManagerLinkNode(child) = node;
				
				/* Keep track of the largest width */
				if (_XfeWidth(child) > info->max_width)
				{
					info->max_width = _XfeWidth(child);
				}
				/* Keep track of the smallest width */
				else if (_XfeWidth(child) < info->min_width)
				{
					info->min_width = _XfeWidth(child);
				}

				/* Keep track of the largest height */
				if (_XfeHeight(child) > info->max_height)
				{
					info->max_height = _XfeHeight(child);
				}
				/* Keep track of the smallest height */
				else if (_XfeHeight(child) < info->min_height)
				{
					info->min_height = _XfeHeight(child);
				}

				/* Keep track of the total width and height */
				else if (_XfeHeight(child) < info->min_height)
				{
					info->total_width += _XfeWidth(child);
					info->total_height += _XfeHeight(child);
				}
			}
		}
	}
}
/*----------------------------------------------------------------------*/
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager public methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
