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
/* Description:	XfeManager widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <stdio.h>
#include <assert.h>

#include <Xfe/ManagerP.h>
#include <Xfe/PrimitiveP.h>

#define MESSAGE0 "XfeManager is an abstract class and cannot be instanciated."
#define MESSAGE1 "No XmNorderFunction installed."
#define MESSAGE2 "XmNselectedChildren is a Read-Only resource."
#define MESSAGE3 "XmNnumSelected is a Read-Only resource."
#define MESSAGE4 "XmNindex is a read-only reasource."
#define MESSAGE5 "Widget is not an XfeManager"
#define MESSAGE6 "XmNpreferredHeight is a read-only resource."
#define MESSAGE7 "XmNpreferredWidth is a read-only resource."
#define MESSAGE8 "XmNnumPopupChildren is a read-only resource."
#define MESSAGE9 "XmNpopupChildren is a read-only resource."
#define MESSAGE11 "The XmNx position of a child cannot be set explicitly."
#define MESSAGE12 "The XmNy position of a child cannot be set explicitly."
#define MESSAGE13 "The XmNborderWidth of a child cannot be set explicitly."
#define MESSAGE14 "Cannot accept new child '%s'."
#define MESSAGE15 "XmNnumPrivateComponents is a read-only resource."
#define MESSAGE16 "XmNlayableChildren is a read-only resource."
#define MESSAGE17 "XmNnumLayableChildren is a read-only resource."
#define MESSAGE18 "The %s class does not support XmNlayableChildren."
#define MESSAGE19 "The %s class does not support XmNnumLayableChildren."
#define MESSAGE20 "XmNmanagerChildType is a read-only resource."
#define MESSAGE21 "XmNlinkNode is a read-only resource."

#define MESSAGE50 "XmNcomponentChildren is a read-only resource."
#define MESSAGE51 "XmNnumComponentChildren is a read-only resource."
#define MESSAGE52 "XmNmaxComponentWidth is a read-only resource."
#define MESSAGE53 "XmNmaxComponentHeight is a read-only resource."
#define MESSAGE54 "XmNtotalComponentWidth is a read-only resource."
#define MESSAGE55 "XmNtotalComponentHeight is a read-only resource."

#define MESSAGE60 "XmNstaticChildren is a read-only resource."
#define MESSAGE61 "XmNnumStaticChildren is a read-only resource."
#define MESSAGE62 "XmNmaxStaticWidth is a read-only resource."
#define MESSAGE63 "XmNmaxStaticHeight is a read-only resource."
#define MESSAGE64 "XmNtotalStaticWidth is a read-only resource."
#define MESSAGE65 "XmNtotalStaticHeight is a read-only resource."

#define MIN_LAYOUT_WIDTH	10
#define MIN_LAYOUT_HEIGHT	10

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void	CoreInitialize		(Widget,Widget,ArgList,Cardinal *);
static void	CoreClassInitialize	();
static void	CoreClassPartInit	(WidgetClass);
static void	CoreDestroy			(Widget);
static void	CoreResize			(Widget);
static void	CoreExpose			(Widget,XEvent *,Region);
static void	CoreRealize			(Widget,XtValueMask *,XSetWindowAttributes *);

static Boolean CoreSetValues	(Widget,Widget,Widget,ArgList,Cardinal *);

static XtGeometryResult CoreQueryGeometry	(Widget,XtWidgetGeometry *,
											 XtWidgetGeometry *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void				CompositeInsertChild		(Widget);
static void				CompositeDeleteChild		(Widget);
static void				CompositeChangeManaged		(Widget);
static XtGeometryResult CompositeGeometryManager	(Widget,
													 XtWidgetGeometry *,
													 XtWidgetGeometry *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite XtOrderProc for XmNinsertPosition							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Cardinal			CompositeInsertPosition		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ConstraintInitialize	(Widget,Widget,ArgList,Cardinal *);
static Boolean	ConstraintSetValues		(Widget,Widget,Widget,ArgList,
										 Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry		(Widget,Dimension *,Dimension *);
static void		UpdateBoundary			(Widget);
static void		LayoutWidget			(Widget);
static void		UpdateChildrenInfo		(Widget);
static void		DrawShadow				(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite hook functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		CoreInitializePostHook			(Widget,Widget);
static Boolean	CoreSetValuesPostHook			(Widget,Widget,Widget);

static void		ConstraintInitializePostHook	(Widget,Widget);
static Boolean	ConstraintSetValuesPostHook		(Widget,Widget,Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
	/* Callback resources */
    { 
		XmNchangeManagedCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeManagerRec , xfe_manager . change_managed_callback),
		XmRImmediate, 
		(XtPointer) NULL,
    },
    { 
		XmNrealizeCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeManagerRec , xfe_manager . realize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
    },
    { 
		XmNresizeCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeManagerRec , xfe_manager . resize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
    },

	/* Busy resources */
    { 
		XmNbusy,
		XmCBusy,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeManagerRec , xfe_manager . busy),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNbusyCursor,
		XmCBusyCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfeManagerRec , xfe_manager . busy_cursor),
		XmRString,
		"watch"
    },
    { 
		XmNbusyCursorOn,
		XmCBusyCursorOn,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeManagerRec , xfe_manager . busy_cursor_on),
		XmRImmediate, 
		(XtPointer) False
    },


	/* Shadow resources */
    { 
		XmNshadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfeManagerRec , xfe_manager . shadow_type),
		XmRImmediate, 
		(XtPointer) XmSHADOW_OUT
    },

	/* Layout resources */
    { 
		XmNlayoutFrozen,
		XmCLayoutFrozen,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeManagerRec , xfe_manager . layout_frozen),
		XmRImmediate, 
		(XtPointer) False
    },

	/* Geometry resources */
	{ 
		XmNpreferredHeight,
		XmCReadOnly,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . preferred_height),
		XmRImmediate, 
		(XtPointer) True
    },
    { 
		XmNpreferredWidth,
		XmCReadOnly,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . preferred_width),
		XmRImmediate, 
		(XtPointer) True
    },
	{
		XmNusePreferredHeight,
		XmCUsePreferredHeight,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeManagerRec , xfe_manager . use_preferred_height),
		XmRImmediate, 
		(XtPointer) True
	},
	{
		XmNusePreferredWidth,
		XmCUsePreferredWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeManagerRec , xfe_manager . use_preferred_width),
		XmRImmediate, 
		(XtPointer) True
	},
    { 
		XmNminWidth,
		XmCMinWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . min_width),
		XmRImmediate, 
		(XtPointer) 2
    },
    { 
		XmNminHeight,
		XmCMinHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . min_height),
		XmRImmediate, 
		(XtPointer) 2
    },

	/* Margin resources */
    { 
		XmNmarginBottom,
		XmCMarginBottom,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_bottom),
		XmRImmediate, 
		(XtPointer) 2
    },
    { 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_left),
		XmRImmediate, 
		(XtPointer) 2
    },
    { 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_right),
		XmRImmediate, 
		(XtPointer) 2
    },
    { 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_top),
		XmRImmediate, 
		(XtPointer) 2
    },

	/* For c++ usage */
	{ 
		XmNinstancePointer,
		XmCInstancePointer,
		XmRPointer,
		sizeof(XtPointer),
		XtOffsetOf(XfeManagerRec , xfe_manager . instance_pointer),
		XmRImmediate, 
		(XtPointer) NULL
	},

	/* Component children resources */
	{ 
		XmNcomponentChildren,
		XmCReadOnly,
		XmRLinkedChildren,
		sizeof(XfeLinked),
		XtOffsetOf(XfeManagerRec , xfe_manager . component_children),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNnumComponentChildren,
		XmCReadOnly,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeManagerRec , xfe_manager . num_component_children),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNmaxComponentChildrenWidth,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . max_component_width),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNmaxComponentChildrenHeight,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . max_component_height),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNtotalComponentChildrenWidth,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . total_component_width),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNtotalComponentChildrenHeight,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . total_component_height),
		XmRImmediate, 
		(XtPointer) 0
    },

	/* Static children resources */
	{ 
		XmNstaticChildren,
		XmCReadOnly,
		XmRLinkedChildren,
		sizeof(XfeLinked),
		XtOffsetOf(XfeManagerRec , xfe_manager . static_children),
		XmRImmediate, 
		(XtPointer) NULL
    },
	{ 
		XmNnumStaticChildren,
		XmCReadOnly,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeManagerRec , xfe_manager . num_static_children),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNmaxStaticChildrenWidth,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . max_static_width),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNmaxStaticChildrenHeight,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . max_static_height),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNtotalStaticChildrenWidth,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . total_static_width),
		XmRImmediate, 
		(XtPointer) 0
    },
	{ 
		XmNtotalStaticChildrenHeight,
		XmCDimension,
		XmRDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . total_static_height),
		XmRImmediate, 
		(XtPointer) 0
    },

#ifdef DEBUG
	/* Debug resources */
	{
		"debugTrace",
		"DebugTrace",
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeManagerRec , xfe_manager . debug_trace),
		XmRImmediate, 
		(XtPointer) False
	},
#endif

	/* Popup children resources */
    { 
		XmNnumPopupChildren,
		XmCReadOnly,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(CoreRec,core . num_popups),
		XmRImmediate,
		(XtPointer) 0,
    },
    { 
		XmNpopupChildren,
		XmCReadOnly,
		XmRWidgetList,
		sizeof(WidgetList),
		XtOffsetOf(CoreRec,core . popup_list),
		XmRImmediate,
		(XtPointer) NULL,
    },

    /* Force the shadow_thickness to 0 */
    { 
		XmNshadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , manager . shadow_thickness),
		XmRImmediate, 
		(XtPointer) 0
    },

	/* Force the width and height to the preffered values */
	{ 
		XmNwidth,
		XmCWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , core . width),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_PREFERRED_WIDTH
	},
	{ 
		XmNheight,
		XmCHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , core . height),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_PREFERRED_HEIGHT
	},

    /* XmNinsertPosition */
	{ 
		XmNinsertPosition,
		XmCInsertPosition,
		XmRFunction,
		sizeof(XtOrderProc),
		XtOffsetOf(XfeManagerRec , composite . insert_position),
		XmRImmediate, 
		(XtPointer) CompositeInsertPosition
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
    { 
		XmNmarginBottom,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_bottom),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
    },
    {
		XmNmarginLeft,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_left),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNmarginRight,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_right),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNmarginTop,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_top),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
    },
    { 
		XmNminWidth,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . min_width),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
    { 
		XmNminHeight,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . min_height),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
    },
};
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Constraint Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource constraint_resources[] = 
{
    { 
		XmNmanagerChildType,
		XmCReadOnly,
		XmRManagerChildType,
		sizeof(unsigned char),
		XtOffsetOf(XfeManagerConstraintRec , xfe_manager . manager_child_type),
		XmRImmediate,
		(XtPointer) XmMANAGER_COMPONENT_INVALID
    }
};   
/*----------------------------------------------------------------------*/

/* Widget class record initialization. */
_XFE_WIDGET_CLASS_RECORD(manager,Manager) =
{
    {
		(WidgetClass) &xmManagerClassRec,		/* superclass			*/
		"XfeManager",							/* class_name			*/
		sizeof(XfeManagerRec),					/* widget_size			*/
		CoreClassInitialize,					/* class_initialize		*/
		CoreClassPartInit,						/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		CoreInitialize,							/* initialize			*/
		NULL,									/* initialize_hook		*/
		CoreRealize,							/* realize				*/
		NULL,									/* actions				*/
		0,										/* num_actions			*/
		(XtResource *)resources,				/* resources			*/
		XtNumber(resources),					/* num_resources		*/
		NULLQUARK,								/* xrm_class			*/
		TRUE,									/* compress_motion		*/
		XtExposeCompressMaximal,				/* compress_exposure	*/
		TRUE,									/* compress_enterleave	*/
		FALSE,									/* visible_interest		*/
		CoreDestroy,							/* destroy				*/
		CoreResize,								/* resize				*/
		CoreExpose,								/* expose				*/
		CoreSetValues,							/* set_values			*/
		NULL,									/* set_values_hook		*/
		XtInheritSetValuesAlmost,				/* set_values_almost	*/
		NULL,									/* get_values_hook		*/
		NULL,									/* accept_focus			*/
		XtVersion,								/* version				*/
		NULL,									/* callback_private		*/
		XtInheritTranslations,					/* tm_table				*/
		CoreQueryGeometry,						/* query_geometry		*/
		XtInheritDisplayAccelerator,			/* display accel		*/
		NULL,									/* extension			*/
    },

    /* Composite Part */
    {
		CompositeGeometryManager,				/* geometry_manager		*/
		CompositeChangeManaged,					/* change_managed		*/
		CompositeInsertChild,					/* insert_child			*/
		CompositeDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },
    
    /* Constraint Part */
    {
		(XtResource *)constraint_resources,		/* resource list		*/
		XtNumber(constraint_resources),			/* num resources		*/
		sizeof(XfeManagerConstraintRec),		/* constraint size		*/
		ConstraintInitialize,					/* init proc			*/
		NULL,									/* destroy proc			*/
		ConstraintSetValues,					/* set values proc		*/
		NULL,									/* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		(XmSyntheticResource *)syn_resources,	/* syn resources		*/
		XtNumber(syn_resources),				/* num syn_resources	*/
		NULL,									/* syn_cont_resources	*/
		0,										/* num_syn_cont_res		*/
		XmInheritParentProcess,					/* parent_process		*/
		NULL,									/* extension			*/
	},

    /* XfeManager Part */
    {
		ForgetGravity,							/* bit_gravity				*/
		PreferredGeometry,						/* preferred_geometry		*/
		UpdateBoundary,							/* update_boundary			*/
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
		DrawShadow,								/* draw_shadow				*/
		NULL,									/* draw_components			*/
		NULL,									/* extension				*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfeManagerWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(manager,Manager);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CoreClassInitialize()
{
	/* Register Xfe Converters */
    XfeRegisterConverters();

    /* Register Representation Types */
/*     XfeRegisterRepresentationTypes(); */
}
/*----------------------------------------------------------------------*/
static void
CoreClassPartInit(WidgetClass wc)
{
    XfeManagerWidgetClass cc = (XfeManagerWidgetClass) wc;
    XfeManagerWidgetClass sc = (XfeManagerWidgetClass) wc->core_class.superclass;

	/* Bit gravity */
    _XfeResolve(cc,sc,xfe_manager_class,bit_gravity,
				XfeInheritBitGravity);
   
	/* Geometry methods */
    _XfeResolve(cc,sc,xfe_manager_class,preferred_geometry,
				XfeInheritPreferredGeometry);

    _XfeResolve(cc,sc,xfe_manager_class,update_boundary,
				XfeInheritUpdateBoundary);

    _XfeResolve(cc,sc,xfe_manager_class,update_children_info,
				XfeInheritUpdateChildrenInfo);

    _XfeResolve(cc,sc,xfe_manager_class,layout_widget,
				XfeInheritLayoutWidget);

	/* Static children methods */
    _XfeResolve(cc,sc,xfe_manager_class,accept_static_child,
				XfeInheritAcceptStaticChild);

    _XfeResolve(cc,sc,xfe_manager_class,insert_static_child,
				XfeInheritInsertStaticChild);

    _XfeResolve(cc,sc,xfe_manager_class,delete_static_child,
				XfeInheritDeleteStaticChild);

    _XfeResolve(cc,sc,xfe_manager_class,layout_static_children,
				XfeInheritLayoutStaticChildren);

	/* Change managed method */
    _XfeResolve(cc,sc,xfe_manager_class,change_managed,
				XfeInheritChangeManaged);

	/* Component methods */
    _XfeResolve(cc,sc,xfe_manager_class,layout_components,
				XfeInheritLayoutComponents);

	/* Rendering methods */
    _XfeResolve(cc,sc,xfe_manager_class,draw_background,
				XfeInheritDrawBackground);
   
    _XfeResolve(cc,sc,xfe_manager_class,draw_shadow,
				XfeInheritDrawShadow);
   
    _XfeResolve(cc,sc,xfe_manager_class,draw_components,
				XfeInheritDrawComponents);
}
/*----------------------------------------------------------------------*/
static void
CoreInitialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    /* Initialize private members */
    _XfemComponentFlag(nw)			= True;
  
    /* Make sure the shadow is ok */
    XfeRepTypeCheck(nw,XmRShadowType,&_XfemShadowType(nw),XmSHADOW_OUT);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeManagerWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
CoreRealize(Widget w,XtValueMask *mask,XSetWindowAttributes* wa)
{
    XSetWindowAttributes attr;
    
    /* Make sure only subclasses of XfeManager get instanciated */
    if ((XtClass(w) == xfeManagerWidgetClass))
    {
		_XfeWarning(w,MESSAGE0);

		return;
    }

	/* Let Manager Create Window */
    (*xmManagerWidgetClass->core_class.realize)(w,mask,wa);
    
    /* Set the Bit Gravity to Forget Gravity */
    attr.bit_gravity = _XfeManagerAccessBitGravity(w);
    
    XChangeWindowAttributes(XtDisplay(w), _XfeWindow(w), CWBitGravity, &attr);

    /* Invoke realize callbacks */
	_XfeInvokeCallbacks(w,_XfemRealizeCallback(w),XmCR_REALIZE,NULL,False);
}
/*----------------------------------------------------------------------*/
static void
CoreDestroy(Widget w)
{
#ifdef DEBUG
	XfeDebugPrintfFunction(w,"Destroy",NULL);
#endif

    /* Remove all CallBacks */
    /* XtRemoveAllCallbacks(w,XmNlayoutCallback); */
    /* XtRemoveAllCallbacks(w,XmNresizeCallback); */
    /* XtRemoveAllCallbacks(w,XmNexposeCallback); */


	/* Destroy the component children list if needed */
	if (_XfemComponentChildren(w) != NULL)
	{
		XfeLinkedDestroy(_XfemComponentChildren(w),NULL,NULL);
	}

	/* Destroy the static children list if needed */
	if (_XfemStaticChildren(w) != NULL)
	{
		XfeLinkedDestroy(_XfemStaticChildren(w),NULL,NULL);
	}
}
/*----------------------------------------------------------------------*/
static void
CoreResize(Widget w)
{
#ifdef DEBUG
/* 	XfeDebugPrintfFunction(w,"CoreResize",NULL); */
#endif

    /* Obtain the Prefered Geometry */
    _XfeManagerPreferredGeometry(w,
								 &_XfemPreferredWidth(w),
								 &_XfemPreferredHeight(w));

    /* Force the preferred dimensions if required */
    if (_XfemUsePreferredWidth(w))
    {
		_XfeWidth(w) = _XfemPreferredWidth(w);
    }
    
    if (_XfemUsePreferredHeight(w))
    {
		_XfeHeight(w) = _XfemPreferredHeight(w);
    }
    
    /* Update the widget boundary */
    _XfeManagerUpdateBoundary(w);
    
    /* Layout the widget */
    _XfeManagerLayoutWidget(w);

    /* Invoke Resize Callbacks */
	_XfeInvokeCallbacks(w,_XfemResizeCallback(w),XmCR_RESIZE,NULL,False);
}
/*----------------------------------------------------------------------*/
static void
CoreExpose(Widget w,XEvent *event,Region region)
{
	/* Make sure the widget is realized before drawing ! */
	if (!XtIsRealized(w)) return;
   
	/* Draw Background */ 
	_XfeManagerDrawBackground(w,event,region,&_XfemBoundary(w));

	/* Draw Shadow */ 
	_XfeManagerDrawShadow(w,event,region,&_XfemBoundary(w));

	/* Draw Components */ 
	_XfeManagerDrawComponents(w,event,region,&_XfemBoundary(w));

	/* Redisplay Gadgets */
	_XmRedisplayGadgets(w,event,region);
}
/*----------------------------------------------------------------------*/
static Boolean
CoreSetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
   /* Reset the Configuration Flags */
	_XfemConfigFlags(nw) = XfeConfigNone;

	/* Reset the preparation Flags */
	_XfemPrepareFlags(nw) = XfePrepareNone;

	/* preferred_height */
	if (_XfemPreferredHeight(nw) != _XfemPreferredHeight(ow))
	{
		_XfemPreferredHeight(nw) = _XfemPreferredHeight(ow);
      
		_XfeWarning(nw,MESSAGE6);
	}

	/* preferred_width */
	if (_XfemPreferredWidth(nw) != _XfemPreferredWidth(ow))
	{
		_XfemPreferredWidth(nw) = _XfemPreferredWidth(ow);
      
		_XfeWarning(nw,MESSAGE7);
	}

	/* num_popups */
	if (_XfeNumPopups(nw) != _XfeNumPopups(ow))
	{
		_XfeNumPopups(nw) = _XfeNumPopups(ow);
      
		_XfeWarning(nw,MESSAGE8);
	}

	/* popup_list */
	if (_XfePopupList(nw) != _XfePopupList(ow))
	{
		_XfePopupList(nw) = _XfePopupList(ow);
      
		_XfeWarning(nw,MESSAGE9);
	}

	/* XmNcomponentChildren */
	if (_XfemComponentChildren(nw) != _XfemComponentChildren(ow))
	{
		_XfemComponentChildren(nw) = _XfemComponentChildren(ow);
      
		_XfeWarning(nw,MESSAGE50);
	}

	/* XmNnumComponentChildren */
	if (_XfemNumComponentChildren(nw) != _XfemNumComponentChildren(ow))
	{
		_XfemNumComponentChildren(nw) = _XfemNumComponentChildren(ow);
      
		_XfeWarning(nw,MESSAGE51);
	}

	/* XmNmaxComponentWidth */
	if (_XfemMaxComponentWidth(nw) != _XfemMaxComponentWidth(ow))
	{
		_XfemMaxComponentWidth(nw) = _XfemMaxComponentWidth(ow);
      
		_XfeWarning(nw,MESSAGE52);
	}

	/* XmNmaxComponentHeight */
	if (_XfemMaxComponentHeight(nw) != _XfemMaxComponentHeight(ow))
	{
		_XfemMaxComponentHeight(nw) = _XfemMaxComponentHeight(ow);
      
		_XfeWarning(nw,MESSAGE53);
	}
	
	/* XmNtotalComponentWidth */
	if (_XfemTotalComponentWidth(nw) != _XfemTotalComponentWidth(ow))
	{
		_XfemTotalComponentWidth(nw) = _XfemTotalComponentWidth(ow);
      
		_XfeWarning(nw,MESSAGE54);
	}

	/* XmNtotalComponentHeight */
	if (_XfemTotalComponentHeight(nw) != _XfemTotalComponentHeight(ow))
	{
		_XfemTotalComponentHeight(nw) = _XfemTotalComponentHeight(ow);
      
		_XfeWarning(nw,MESSAGE55);
	}

	/* XmNstaticChildren */
	if (_XfemStaticChildren(nw) != _XfemStaticChildren(ow))
	{
		_XfemStaticChildren(nw) = _XfemStaticChildren(ow);
      
		_XfeWarning(nw,MESSAGE60);
	}

	/* XmNnumStaticChildren */
	if (_XfemNumStaticChildren(nw) != _XfemNumStaticChildren(ow))
	{
		_XfemNumStaticChildren(nw) = _XfemNumStaticChildren(ow);
      
		_XfeWarning(nw,MESSAGE61);
	}

	/* XmNmaxStaticWidth */
	if (_XfemMaxStaticWidth(nw) != _XfemMaxStaticWidth(ow))
	{
		_XfemMaxStaticWidth(nw) = _XfemMaxStaticWidth(ow);
      
		_XfeWarning(nw,MESSAGE62);
	}

	/* XmNmaxStaticHeight */
	if (_XfemMaxStaticHeight(nw) != _XfemMaxStaticHeight(ow))
	{
		_XfemMaxStaticHeight(nw) = _XfemMaxStaticHeight(ow);
      
		_XfeWarning(nw,MESSAGE63);
	}

	/* XmNtotalStaticWidth */
	if (_XfemTotalStaticWidth(nw) != _XfemTotalStaticWidth(ow))
	{
		_XfemTotalStaticWidth(nw) = _XfemTotalStaticWidth(ow);
      
		_XfeWarning(nw,MESSAGE64);
	}

	/* XmNtotalStaticHeight */
	if (_XfemTotalStaticHeight(nw) != _XfemTotalStaticHeight(ow))
	{
		_XfemTotalStaticHeight(nw) = _XfemTotalStaticHeight(ow);
      
		_XfeWarning(nw,MESSAGE65);
	}

	/* height */
	if (_XfeHeight(nw) != _XfeHeight(ow))
	{
		/* if resize_heigh is True, we dont allow width changes */
		if (_XfemUsePreferredHeight(nw)) 
		{
			_XfeHeight(nw) = _XfeHeight(ow);
		}
		else
		{
			_XfemConfigFlags(nw) |= XfeConfigGLE;
		}
	}
   
	/* width */
	if (_XfeWidth(nw) != _XfeWidth(ow))
	{
		/* if resize_width is True, we dont allow width changes */
		if (_XfemUsePreferredWidth(nw)) 
		{
			_XfeWidth(nw) = _XfeWidth(ow);
		}
		else
		{
			_XfemConfigFlags(nw) |= XfeConfigGLE;
		}
	}

	/* Changes in sensitivity */
	if (_XfeSensitive(nw) != _XfeSensitive(ow))
	{
		Widget *	pw;
		int			i;

		/* Make sure all the children have the same sensitivity */
		for (i = 0, pw = _XfemChildren(nw); i < _XfemNumChildren(nw); i++,pw++)
		{
			if (_XfeIsAlive(*pw))
			{
				if (XtIsSensitive(*pw) != XtIsSensitive(nw))
				{
					XtSetSensitive(*pw,_XfeSensitive(nw));
				}
			}
		}
	}

	/* busy */
	if (_XfemBusy(nw) != _XfemBusy(ow))
	{
		if (_XfemBusy(nw))
		{
			if (_XfemBusyCursor(nw))
			{
				XfeCursorDefine(nw,_XfemBusyCursor(nw));
			}
		}
		else
		{
			XfeCursorUndefine(nw);
		}
	}
   
	/* busy_cursor */
	if (_XfemBusyCursor(nw) != _XfemBusyCursor(ow))
	{
		if (_XfemBusyCursorOn(nw) && _XfemBusyCursor(nw) && XtIsRealized(nw))
		{
			XfeCursorDefine(nw,_XfemBusyCursor(nw));
		}
	}

	/* busy_cursor_on */
	if (_XfemBusyCursorOn(nw) != _XfemBusyCursorOn(ow))
	{
		/* Undefine the old cursor if needed */
		if (_XfemBusyCursorOn(ow) && _XfemBusyCursor(ow) && XtIsRealized(ow))
		{
			XfeCursorUndefine(ow);
		}
		else if (_XfemBusyCursorOn(nw) && 
				 _XfemBusyCursor(nw) && 
				 XtIsRealized(nw))
		{
			XfeCursorDefine(nw,_XfemBusyCursor(nw));
		}
	}

	/* Changes that affect the layout and geometry */
	if ((_XfemMarginTop(nw)			!= _XfemMarginTop(ow)) ||
		(_XfemMarginBottom(nw)		!= _XfemMarginBottom(ow)) ||
		(_XfemMarginLeft(nw)		!= _XfemMarginLeft(ow)) ||
		(_XfemMarginRight(nw)		!= _XfemMarginRight(ow)) ||       
		(_XfemShadowThickness(nw)	!= _XfemShadowThickness(ow)) ||
		(_XfemUnitType(nw)			!= _XfemUnitType(ow)))
	{
		_XfemConfigFlags(nw) |= XfeConfigGLE;
	}

	/* shadow_type */
	if (_XfemShadowType(nw) != _XfemShadowType(ow))
	{
		/* Make sure the new shadow type is ok */
		XfeRepTypeCheck(nw,XmRShadowType,&_XfemShadowType(nw),XmSHADOW_OUT);

		_XfemConfigFlags(nw) |= XfeConfigExpose;
	}

	return _XfeManagerChainSetValues(ow,rw,nw,xfeManagerWidgetClass);
}
/*----------------------------------------------------------------------*/
static XtGeometryResult
CoreQueryGeometry(Widget w,XtWidgetGeometry	* req,XtWidgetGeometry * reply)
{
	reply->request_mode		= CWWidth | CWHeight;
	reply->width			= _XfemPreferredWidth(w);
	reply->height			= _XfemPreferredHeight(w);

	/* XtGeometryYes: Request matches Prefered Geometry. */
	if (((req->request_mode & CWWidth) && (req->width == reply->width)) &&
		((req->request_mode & CWHeight) && (req->height == reply->height)))
	{
		return XtGeometryYes;
	}
	
	/* XtGeometryNo: Reply matches current Geometry. */
	if ((reply->width == _XfeWidth(w)) && (reply->height == _XfeHeight(w)))
	{
		return XtGeometryNo; 
	}
	
	/* XtGeometryAlmost: One of reply fields doesn't match cur/req Geometry. */
	return XtGeometryAlmost; 
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
    Widget                  w = XtParent(child);
    XmManagerWidgetClass    mwc = (XmManagerWidgetClass) xmManagerWidgetClass;

#ifdef DEBUG
	XfeDebugPrintfFunction(w,
						   "CompositeInsertChild",
						   "%s for %s",
						   XtName(w),XtName(child));
#endif

	/*
	 * Component children:
	 * -------------------
	 *
	 * If the _XfemComponentFlag is set, the given child is being created
	 * from one of the CoreInitialize() methods for this particular 
	 * WidgetClass.
	 *
	 * The widget should be flagged as a component so that it can 
	 * receive special treatment.
	 */
    if (_XfemComponentFlag(w) || 
        (_XfeConstraintManagerChildType(child) == XmMANAGER_COMPONENT_CHILD))
	{
		/* Mark the child COMPONENT */
		_XfeConstraintManagerChildType(child) = XmMANAGER_COMPONENT_CHILD;
		
		/* Create the component children list if needed */
		if (_XfemComponentChildren(w) == NULL)
		{
			_XfemComponentChildren(w) = XfeLinkedConstruct();
		}

		/* Add the child to the component children list */
		XfeLinkedInsertAtTail(_XfemComponentChildren(w),child);

		/* Update the component children count */
		_XfemNumComponentChildren(w) = 
			XfeLinkedCount(_XfemComponentChildren(w));

        /* Call XmManager's CompositeInsertChild to do the Xt magic */
        (*mwc->composite_class.insert_child)(child);
	}
	/*
	 * Static children:
	 * ----------------
	 *
	 * If the child is being created from somewhere other than a 
	 * CoreInitialize() method, we need to determine whether it
	 * is a static child.
	 *
	 * Static children are expected by the XfeManager class to exist
	 * in a pre-defined location within the Widget.  
	 *
	 * For example, a caption widget could contain up to two static
	 * children: A label and a text field.
	 *
	 * These two static children would always appear in the same 
	 * pre-defined positions regradless of the Manager widget's 
	 * geometry.
	 *
	 * It is up to XfeManager sub-classes to provide specialized 
	 * methods to determine which (if any) widgets are accepeted
	 * as static children.
	 */
	else if (_XfeManagerAcceptStaticChild(child))
	{
		/* Mark the child STATIC */
		_XfeConstraintManagerChildType(child) = XmMANAGER_STATIC_CHILD;

		/* Create the static children list if needed */
		if (_XfemStaticChildren(w) == NULL)
		{
			_XfemStaticChildren(w) = XfeLinkedConstruct();
		}
		
		/* Add the child to the static children list */
		XfeLinkedInsertAtTail(_XfemStaticChildren(w),child);

		/* Update the static children count */
		_XfemNumStaticChildren(w) = 
			XfeLinkedCount(_XfemStaticChildren(w));

        /* Call XmManager's CompositeInsertChild to do the Xt magic */
        (*mwc->composite_class.insert_child)(child);

        /* Insert the static child and relayout if needed */
        if (_XfeManagerInsertStaticChild(child))
        {
            XfeManagerLayout(w);
        }
	}
#if 0
	/*
	 * Dynamic children:
	 * -----------------
	 *
	 * For example, a toolbar could contain any number of dynamic
	 * children, which get placed according to a layout policy.
	 *
	 * It is up to XfeManager sub-classes to provide specialized 
	 * methods to determine which (if any) widgets are accepeted
	 * as static children.
	 */
	else if (_XfeManagerAcceptDynamicChild(child))
	{
		/* Mark the child STATIC */
		_XfeConstraintManagerChildType(child) = XmMANAGER_DYNAMIC_CHILD;

        /* Call XmManager's CompositeInsertChild to do the Xt magic */
        (*mwc->composite_class.insert_child)(child);

        /* Insert the dynamic child and relayout if needed */
        if (_XfeManagerInsertDynamicChild(child))
        {
            XfeManagerLayout(w);
        }
	}
#endif
	else
 	{
        _XfeArgWarning(w,MESSAGE14,XtName(child));
    }
}
/*----------------------------------------------------------------------*/
static void
CompositeDeleteChild(Widget child)
{
    Widget                  w = XtParent(child);
    XmManagerWidgetClass    mwc = (XmManagerWidgetClass) xmManagerWidgetClass;
	Boolean					need_layout = False;
	Boolean					is_managed = _XfeIsManaged(child);

	/*
	 * Component children:
	 * -------------------
	 *
	 * The only time when a component child will be deleted, is from
	 * the CoreDestroy() method of the manager widget.  Because the
	 * manager widget is being destroyed at this point, we dont need
	 * to do anything else (layout), other than removing the child
	 * from the appropiate lists
	 */
	if (_XfeConstraintManagerChildType(child) == XmMANAGER_COMPONENT_CHILD)
	{
		XfeLinkNode node;

		assert( _XfemComponentChildren(w) != NULL );

		node = XfeLinkedFindNodeByItem(_XfemComponentChildren(w),child);

		assert( node != NULL );

		if (node != NULL)
		{
			XfeLinkedRemoveNode(_XfemComponentChildren(w),node);

			/* Update the component children count */
			_XfemNumComponentChildren(w) = 
				XfeLinkedCount(_XfemComponentChildren(w));
		}
	}
	/*
	 * Static children:
	 * ----------------
	 *
	 */
	else if (_XfeConstraintManagerChildType(child) == XmMANAGER_STATIC_CHILD)
	{
		XfeLinkNode node;

		assert( _XfemStaticChildren(w) != NULL );
		
		node = XfeLinkedFindNodeByItem(_XfemStaticChildren(w),child);

		assert( node != NULL );

		if (node != NULL)
		{
			XfeLinkedRemoveNode(_XfemStaticChildren(w),node);

			/* Update the static children count */
			_XfemNumStaticChildren(w) = 
				XfeLinkedCount(_XfemStaticChildren(w));
		}

        /* Delete the static child */
        need_layout = _XfeManagerDeleteStaticChild(child);
	}
#if 0
	/*
	 * Dynamic children:
	 * ----------------
	 *
	 */
	else if (_XfeConstraintManagerChildType(child) == XmMANAGER_DYNAMIC_CHILD)
	{
        /* Delete the dynamic child */
        need_layout = _XfeManagerDeleteDynamicChild(child);
	}
#endif

	/* Call XmManager's CompositeDeleteChild to do the Xt magic */
	(*mwc->composite_class.delete_child)(child);

	/* Do layout if needed */
    if (need_layout && is_managed)
    {
        XfeManagerLayout(w);
    }
}
/*----------------------------------------------------------------------*/
static void
CompositeChangeManaged(Widget w)
{
#ifdef DEBUG
	XfeDebugPrintfFunction(w,
						   "CompositeChangeManaged",
						   "%s",
						   (_XfemLayoutFrozen(w) ? " - ignored" : ""),
						   "NULL");
#endif

    /* Update widget geometry only if layout_frozen is False */
    if (!_XfemLayoutFrozen(w))
    {
		Boolean		change_width = False;
		Boolean		change_height = False;

		/* Update the children info */
		_XfeManagerUpdateChildrenInfo(w);

		/* Invoke the change_managed method */
		_XfeManagerChangeManaged(w);

		/* Obtain our new preferred geometry */
		_XfeManagerPreferredGeometry(w,
									 &_XfemPreferredWidth(w),
									 &_XfemPreferredHeight(w));
		
		/* Check for changes in preferred width if needed */
		if (_XfemUsePreferredWidth(w) && (_XfeWidth(w) != _XfemPreferredWidth(w)))
		{
			change_width = True;
		}

		/* Check for changes in preferred height if needed */
		if (_XfemUsePreferredHeight(w) && (_XfeHeight(w) != _XfemPreferredHeight(w)))
		{
			change_height = True;
		}
		
		/* Check for geometry changes */
		if (change_width || change_height)
		{
			XtGeometryResult	result;
			XtWidgetGeometry	request;

			request.request_mode = 0;

			/* Request a width change */
			if (change_width)
			{
				request.width = _XfemPreferredWidth(w);
				request.request_mode |= CWWidth;
			}

			/* Request a width height */
			if (change_height)
			{
				request.height = _XfemPreferredHeight(w);
				request.request_mode |= CWHeight;
			}

			/* Make the request */
			result = _XmMakeGeometryRequest(w,&request);

			/* Adjust geometry accordingly */
			if (result == XtGeometryYes)
			{
				/* Goody */
			}
			else if(result == XtGeometryNo)
			{
				/* Too bad */
			}
		}

		/* Update the widget boundary */
		_XfeManagerUpdateBoundary(w);
		
		/* Layout the widget */
		_XfeManagerLayoutWidget(w);
    }
	else
	{
/*		printf("Change managed for %s - ignored \n",XtName(w));*/
	}

    /* Invoke change managed callbacks */
	_XfeInvokeCallbacks(w,_XfemChangeManagedCallback(w),
						XmCR_CHANGE_MANAGED,NULL,False);
	
    /* Update keyboard traversal */
    _XmNavigChangeManaged(w);
}
/*----------------------------------------------------------------------*/
static XtGeometryResult
CompositeGeometryManager(Widget				child,
						 XtWidgetGeometry *	request,
						 XtWidgetGeometry *	reply)
{
#if 1
	Widget				w = XtParent(child);
	XtGeometryMask		mask = request->request_mode;
	XtGeometryResult	our_result = XtGeometryNo;

#ifdef DEBUG
	XfeDebugPrintfFunction(w,"CompositeGeometryManager",NULL,NULL);
#endif

	/* Ignore x changes */
	if (mask & CWX)
	{
		mask |= ~CWX;

		/*_XfeWarning(w,MESSAGE11);*/
	}

	/* Ignore y changes */
	if (mask & CWY)
	{
		mask |= ~CWY;
		
		/*_XfeWarning(w,MESSAGE12);*/
	}

	/* Ignore border_width changes */
	if (mask & CWBorderWidth)
	{
		mask |= ~CWBorderWidth;

		/*_XfeWarning(w,MESSAGE13);*/
	}

	/* Check for something else being requested */
	if ((mask & CWWidth) || (mask & CWHeight))
	{
		Boolean				change_width = False;
		Boolean				change_height = False;
		Dimension			old_width;
		Dimension			old_height;
		Dimension			pref_width;
		Dimension			pref_height;

		/* Remember the child's old dimensions */
		old_width = _XfeWidth(child);
		old_height = _XfeHeight(child);

		/* Adjust the child's dimensions _TEMPORARLY_ */
		if (mask & CWWidth)
		{
			_XfeWidth(child) = request->width;
		}

		if (mask & CWHeight)
		{
			_XfeHeight(child) = request->height;
		}

		/* Obtain the preferred geometry to support the new child */
		_XfeManagerPreferredGeometry(w,&pref_width,&pref_height);

		/* Restore the child's dimensions */
		_XfeWidth(child) = old_width;
		_XfeHeight(child) = old_height;
			
		/* Check for changes in preferred width if needed */
		if (_XfemUsePreferredWidth(w) && (_XfeWidth(w) != pref_width))
		{
			change_width = True;
		}

		/* Check for changes in preferred height if needed */
		if (_XfemUsePreferredHeight(w) && (_XfeHeight(w) != pref_height))
		{
			change_height = True;
		}

		/* Check for geometry changes to ourselves */
		if (change_width || change_height)
		{
			XtGeometryResult	parent_result;
			XtWidgetGeometry	parent_request;
				
			parent_request.request_mode = 0;
				
			/* Request a width change */
			if (change_width)
			{
				parent_request.width = pref_width;
				parent_request.request_mode |= CWWidth;
			}
				
			/* Request a width height */
			if (change_height)
			{
				parent_request.height = pref_height;
				parent_request.request_mode |= CWHeight;
			}

			/* Make the request */
			parent_result = _XmMakeGeometryRequest(w,&parent_request);

			/* Adjust geometry accordingly */
			if (parent_result == XtGeometryYes)
			{
				/* Goody */
				if (mask & CWWidth)
				{
					_XfeWidth(child) = request->width;

					_XfemPreferredWidth(w) = pref_width;
				}

				if (mask & CWHeight)
				{
					_XfeHeight(child) = request->height;

					_XfemPreferredHeight(w) = pref_height;
				}

				XfeResize(w);

				our_result = XtGeometryYes;

			}
			else if(parent_result == XtGeometryNo)
			{
				/* Too bad */
			}
		}
	}

	return our_result;

#else

	Widget				w = XtParent(child);
	XtGeometryMask		mask = request->request_mode;
	Boolean				set_almost = False;
	XtGeometryResult 	result = XtGeometryYes;
	XtWidgetGeometry 	geo_request;
	XtWidgetGeometry 	geo_reply;
	Boolean				dim_flag = False;
	Boolean				x_flag = False;
	Boolean				y_flag = False;
	Position			x;
	Position			y;

	/*
    *We do not allow children to change position, since that is the
    * whole point of this widget!  
    */
	if (!(mask & (CWWidth | CWHeight)))
	{
		return XtGeometryNo;
	}
	
	/*
	 *The child can also request a combination of width/height
	 * and/or border changes.  In this situation, we will have to
	 * return XtGeometryAlmost
	 */ 
	
	if ((mask & CWX) && (request->x != w->core.x))
	{
		reply->x = w->core.x;
		reply->request_mode |= CWX;
		set_almost = True;
	}
	
	if ((mask & CWY) && (request->y != w->core.y))
	{
		reply->y = w->core.y;
		reply->request_mode |= CWY;
		set_almost = True;
	}
	
	if ((mask & CWBorderWidth) && 
       (request->border_width != w->core.border_width))
	{
		reply->y = w->core.border_width;
		reply->request_mode |= CWBorderWidth;
		set_almost = True;
	}
	
	/* Check for queryonly or requests other than width and height.
	 * If the request more than just width and height, we must
	 * return GeometryAlmost. This means the request to our parent must be
	 * a queryonly request to prevent our parent from reconfiguring our
	 * geometry.
	 */
	if ((mask & XtCWQueryOnly) || set_almost)
	{
		geo_request.request_mode = XtCWQueryOnly;
	}
	else
	{
		geo_request.request_mode = 0 ;
	}
   
	if (mask & CWWidth)
	{
		/* If the request width causes the column to grow or
		 * the request width may cause the column to shrink
		 * see if the change will affect the grid's geometry.
		 * Otherwise expexp the request and change the childs width.
		 */
		w->core.width = request->width;
		reply->width = w->core.width;
		
		dim_flag = True;
	}
	
	
	if (mask & CWHeight)
	{
		/* If the request width causes the column to grow or
		 * the request width may cause the column to shrink
		 * see if the change will affect the grid's geometry.
		 * Otherwise expexp the request and change the childs width.
		 */
		w->core.height = request->height;
		reply->height = w->core.height;
		
		dim_flag = True;
	}
	
	if (set_almost) 
	{
		return XtGeometryAlmost;
	}
	
	if (dim_flag)
	{
		if (!_XfemLayoutFrozen(XtParent(w)))
		{
			XfeResize(XtParent(w));
		}
	}
	
	return XtGeometryYes;
#endif
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite XtOrderProc for XmNinsertPosition							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Cardinal
CompositeInsertPosition(Widget child)
{
	Widget		w = _XfeParent(child);
	Cardinal	num_component_children = 0;

 	/* Insert component children at the end of the composite children array */
    if (_XfeConstraintManagerChildType(child) == XmMANAGER_COMPONENT_CHILD)
    {
		return _XfemNumChildren(w);
    }

	if (_XfemComponentChildren(w) != NULL)
	{
		num_component_children = XfeLinkedCount(_XfemComponentChildren(w));
	}

	/* Insert other children before component children */
	if (num_component_children > 0)
	{
		return _XfemNumChildren(w) - num_component_children;
	}

	/* Otherwise return the default position - tail */
	return _XfemNumChildren(w);
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
/*	XfeManagerConstraintPart *	cp = _XfeManagerConstraintPart(nw);*/

	/* Finish constraint initialization */
	_XfeConstraintChainInitialize(rw,nw,xfeManagerWidgetClass);
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
	Widget						w = XtParent(nw);
 	XfeManagerConstraintPart *	np = _XfeManagerConstraintPart(nw);
 	XfeManagerConstraintPart *	op = _XfeManagerConstraintPart(ow);

	/* Reset the Configuration Flags */
	_XfemConfigFlags(w) = XfeConfigNone;

	/* Reset the preparation Flags */
	_XfemPrepareFlags(w) = XfePrepareNone;

	/* XmNmanagerChildType */
	if (np->manager_child_type != op->manager_child_type)
	{
		np->manager_child_type = op->manager_child_type;

		_XfeWarning(nw,MESSAGE20);
	}

	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(ow,rw,nw,xfeManagerWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension *width,Dimension *height)
{
	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);
}
/*----------------------------------------------------------------------*/
static void
UpdateBoundary(Widget w)
{
    XfeRectSet(&_XfemBoundary(w),
			   
			   _XfemOffsetLeft(w),
			   
			   _XfemOffsetTop(w),
			   
			   _XfeWidth(w) - _XfemOffsetLeft(w) - _XfemOffsetRight(w),
			   
			   _XfeHeight(w) - _XfemOffsetTop(w) - _XfemOffsetBottom(w));
}
/*----------------------------------------------------------------------*/
static void
UpdateChildrenInfo(Widget w)
{
	/* Update the component children */
	_XfeManagerUpdateComponentChildrenInfo(w);

	/* Update the static children */
	_XfeManagerUpdateStaticChildrenInfo(w);
}
/*----------------------------------------------------------------------*/
static void
LayoutWidget(Widget w)
{
    /* Layout the components */
    _XfeManagerLayoutComponents(w);

    /* Layout the static children */
    _XfeManagerLayoutStaticChildren(w);
}
/*----------------------------------------------------------------------*/
static void
DrawShadow(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
	if (!_XfemShadowThickness(w))
 	{
 		return;
 	}

    /* Draw the shadow */
    _XmDrawShadows(XtDisplay(w),
				   _XfeWindow(w),
				   _XfemTopShadowGC(w),_XfemBottomShadowGC(w),
				   0,0,
				   _XfeWidth(w),_XfeHeight(w),
				   _XfemShadowThickness(w),
				   _XfemShadowType(w));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite hook functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
CoreInitializePostHook(Widget rw,Widget nw)
{
	assert( nw != NULL );
	assert( _XfeIsAlive(nw) );
	assert( XfeIsManager(nw) );

    _XfemPrepareFlags(nw) = XfePrepareAll;
    
    /* Prepare the Widget */
    _XfeManagerPrepareComponents(nw,_XfemPrepareFlags(nw));
    
    /* Obtain the preferred dimensions for the first time. */
    _XfeManagerPreferredGeometry(nw,
								 &_XfemPreferredWidth(nw),
								 &_XfemPreferredHeight(nw));
    
    /* Force the preferred width if needed */
    if (_XfemUsePreferredWidth(nw) || 
		(_XfeWidth(nw) == XfeDEFAULT_PREFERRED_WIDTH))
    {
		_XfeWidth(nw) = _XfemPreferredWidth(nw);
    }

    /* Force the preferred height if needed */
    if (_XfemUsePreferredHeight(nw) || 
		(_XfeHeight(nw) == XfeDEFAULT_PREFERRED_HEIGHT))
    {
		_XfeHeight(nw) = _XfemPreferredHeight(nw);
    }
    
    /* Update the widget boundary */
    _XfeManagerUpdateBoundary(nw);
    
    /* Layout the widget */
    _XfeManagerLayoutWidget(nw);
    
    _XfemComponentFlag(nw) = False;
    _XfemPrepareFlags(nw) = XfePrepareNone;

	_XfemOldWidth(nw) = XfeGEOMETRY_INVALID_DIMENSION;
	_XfemOldHeight(nw) = XfeGEOMETRY_INVALID_DIMENSION;
}
/*----------------------------------------------------------------------*/
static Boolean
CoreSetValuesPostHook(Widget ow,Widget rw,Widget nw)
{
	assert( nw != NULL );
	assert( _XfeIsAlive(nw) );
	assert( XfeIsManager(nw) );

	/* Prepare the Widget if needed */
	if (_XfemPrepareFlags(nw))
	{
		_XfeManagerPrepareComponents(nw,_XfemPrepareFlags(nw));
	}
	
	/* Obtain the preferred dimensions if needed */
	if (_XfemConfigFlags(nw) & XfeConfigGeometry)
	{
		_XfeManagerPreferredGeometry(nw,
									 &_XfemPreferredWidth(nw),
									 &_XfemPreferredHeight(nw));
		
		/* Set the preferred dimensions if resize flags are True */
		if (_XfemUsePreferredWidth(nw))
		{
			_XfeWidth(nw) = _XfemPreferredWidth(nw);
		}
		
		if (_XfemUsePreferredHeight(nw))
		{
			_XfeHeight(nw) = _XfemPreferredHeight(nw);
		}
	}
	
	/* Update the widget boundary */
	_XfeManagerUpdateBoundary(nw);

	/* Clear the window if it is dirty and realized */
	if (XtIsRealized(nw) && (_XfemConfigFlags(nw) & XfeConfigDirty))
	{
		XClearWindow(XtDisplay(nw),_XfeWindow(nw));
	}
	
	if (_XfemConfigFlags(nw) & XfeConfigLayout)
	{
		/* Layout the widget */
		_XfeManagerLayoutWidget(nw);
	}
	
	return (_XfemConfigFlags(nw) & XfeConfigExpose);
}
/*----------------------------------------------------------------------*/
static void
ConstraintInitializePostHook(Widget rw,Widget nw)
{
	Widget						w = XtParent(nw);
/* 	XfeManagerConstraintPart *	cp = _XfeManagerConstraintPart(nw); */

	assert( w != NULL );
	assert( _XfeIsAlive(w) );
	assert( XfeIsManager(w) );

	/* Nothing.  I wonder if some geometry magic is needed here ? */
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValuesPostHook(Widget oc,Widget rc,Widget nc)
{
	Widget 				w = XtParent(nc);

	assert( w != NULL );
	assert( _XfeIsAlive(w) );
	assert( XfeIsManager(w) );

	/* The following is the same as SetValuesPostHook() - combine ... */

	/* Prepare the Widget if needed */
	if (_XfemPrepareFlags(w))
	{
		_XfeManagerPrepareComponents(w,_XfemPrepareFlags(w));
	}
	
	/* Change in geometry */
	if (_XfemConfigFlags(w) & XfeConfigGeometry)
	{
		Dimension width	= _XfeWidth(w);
		Dimension height = _XfeHeight(w);

		/* Obtain the preferred dimensions */
		_XfeManagerPreferredGeometry(w,
									 &_XfemPreferredWidth(w),
									 &_XfemPreferredHeight(w));

		/* Assign the preferred width if needed */
		if (_XfemUsePreferredWidth(w))
		{
			width = _XfemPreferredWidth(w);
		}
		
		/* Assign the preferred height if needed */
		if (_XfemUsePreferredHeight(w))
		{
			height = _XfemPreferredHeight(w);
		}

		/*
		 * Request that we be resized to the new geometry.
		 *
		 * The _XfeMakeGeometryRequest() call becomes a NOOP
		 * if both width and height match the current dimensions.
		 *
		 * It should also handle any needed children re layout.
		 */
		_XfeMakeGeometryRequest(w,width,height);
	}
	
	/* Update the widget boundary */
	_XfeManagerUpdateBoundary(w);

	/* Clear the window if it is dirty and realized */
	if (XtIsRealized(w) && (_XfemConfigFlags(w) & XfeConfigDirty))
	{
		XClearWindow(XtDisplay(w),_XfeWindow(w));
	}
	
	if (_XfemConfigFlags(w) & XfeConfigLayout)
	{
		/* Layout the widget */
		_XfeManagerLayoutWidget(w);
	}
	
	return (_XfemConfigFlags(w) & XfeConfigExpose);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerPreferredGeometry(Widget w,Dimension *width,Dimension *height)
{
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);

	Dimension				min_width = 
		XfeMax(_XfemMinWidth(w),XfeMANAGER_DEFAULT_WIDTH);

	Dimension				min_height = 
		XfeMax(_XfemMinHeight(w),XfeMANAGER_DEFAULT_HEIGHT);
	
	assert( width != NULL );
	assert( height != NULL );

	if (mc->xfe_manager_class.preferred_geometry)
	{
		(*mc->xfe_manager_class.preferred_geometry)(w,width,height);
	}

	/* Make sure preferred width is greater than the min possible width */
	if (*width <= min_width)
	{
		*width = min_width;
	}

	/* Make sure preferred height is greater than the min possible height */
	if (*height <= min_height)
	{
		*height = min_height;
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerUpdateBoundary(Widget w)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.update_boundary)
	{
		(*mc->xfe_manager_class.update_boundary)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerUpdateChildrenInfo(Widget w)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.update_children_info)
	{
		(*mc->xfe_manager_class.update_children_info)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerLayoutWidget(Widget w)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.layout_widget)
	{
		(*mc->xfe_manager_class.layout_widget)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeManagerAcceptStaticChild(Widget child)
{
	Boolean					result = XtIsManaged(child);
	Widget					w = XtParent(child);
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.accept_static_child)
	{
		result = (*mc->xfe_manager_class.accept_static_child)(child);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeManagerInsertStaticChild(Widget child)
{
	Boolean					result = XtIsManaged(child);
	Widget					w = XtParent(child);
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.insert_static_child)
	{
		result = (*mc->xfe_manager_class.insert_static_child)(child);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeManagerDeleteStaticChild(Widget child)
{
	Boolean					result = XtIsManaged(child);
	Widget					w = XtParent(child);
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.delete_static_child)
	{
		result = (*mc->xfe_manager_class.delete_static_child)(child);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerLayoutStaticChildren(Widget w)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);
	
 	if ((_XfeWidth(w) <= MIN_LAYOUT_WIDTH) || 
		(_XfeHeight(w) <= MIN_LAYOUT_HEIGHT))
	{
		return;
	}
	
	if (mc->xfe_manager_class.layout_static_children)
	{
		(*mc->xfe_manager_class.layout_static_children)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerChangeManaged(Widget w)
{
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.change_managed)
	{
		(*mc->xfe_manager_class.change_managed)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerPrepareComponents(Widget w,int flags)
{
	WidgetClass				cc;
	XfeManagerWidgetClass	mc;
	Cardinal				i = 0;
	Cardinal				count = 0;

	/* Someone has to be nuts to have more than 32 subclasses... */
	static XfePrepareProc		proc_table[32];

	/* Traverse all classes until we find XmManager */
	for (cc = XtClass(w); 
		 cc != xmManagerWidgetClass; 
		 cc = cc->core_class.superclass)
	{
		mc = (XfeManagerWidgetClass) cc;

		/* Add the method to the table */
		if (mc->xfe_manager_class.prepare_components)
		{
			proc_table[count++] = mc->xfe_manager_class.prepare_components;
		}
	}

	/* Invoke the methods in reverse order */
	for (i = count; i; i--)
	{
		XfePrepareProc proc = proc_table[i - 1];

		/* Invoke the prepare_components */
		(*proc)(w,flags);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerLayoutComponents(Widget w)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);

 	if ((_XfeWidth(w) <= MIN_LAYOUT_WIDTH) || (_XfeHeight(w) <= MIN_LAYOUT_HEIGHT))
	{
		return;
	}
	
	if (mc->xfe_manager_class.layout_components)
	{
		(*mc->xfe_manager_class.layout_components)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerDrawBackground(Widget		w,
						  XEvent *		event,
						  Region		region,
						  XRectangle *	rect)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.draw_background)
	{
		(*mc->xfe_manager_class.draw_background)(w,event,region,rect);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerDrawComponents(Widget		w,
						  XEvent *		event,
						  Region		region,
						  XRectangle *	rect)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.draw_components)
	{
		(*mc->xfe_manager_class.draw_components)(w,event,region,rect);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerDrawShadow(Widget		w,
					  XEvent *		event,
					  Region		region,
					  XRectangle *	rect)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.draw_shadow)
	{
		(*mc->xfe_manager_class.draw_shadow)(w,event,region,rect);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager constraint chain functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerChainInitialize(Widget rw,Widget nw,WidgetClass wc)
{
	assert( nw != NULL );

	if (XtClass(nw) == wc)
	{
		CoreInitializePostHook(rw,nw);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeManagerChainSetValues(Widget ow,Widget rw,Widget nw,WidgetClass wc)
{
	assert( nw != NULL );

	if (XtClass(nw) == wc)
	{
		return CoreSetValuesPostHook(ow,rw,nw);
	}
	
	return False;
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeConstraintChainInitialize(Widget rw,Widget nw,WidgetClass wc)
{
	assert( nw != NULL );
	assert( XtParent(nw) != NULL );

	if (XtClass(XtParent(nw)) == wc)
	{
		ConstraintInitializePostHook(rw,nw);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeConstraintChainSetValues(Widget ow,Widget rw,Widget nw,WidgetClass wc)
{
	assert( nw != NULL );
	assert( XtParent(nw) != NULL );

	if (XtClass(XtParent(nw)) == wc)
	{
		return ConstraintSetValuesPostHook(ow,rw,nw);
	}
	
	return False;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager private functions											*/
/*																		*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager public functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeManagerLayout(Widget w)
{
	assert( _XfeIsAlive(w) );
	assert( XfeIsManager(w) );

	XfeResize(w);

#if 0	
   /* Setup the max children dimensions */
	_XfeManagerUpdateChildrenInfo(w);

	/* Make sure some components exist */
	if (!_XfemNumComponents(w))
	{
		return;
	}
	
	/* Layout the widget */
	_XfeManagerLayoutWidget(w);
#endif
}
/*----------------------------------------------------------------------*/
