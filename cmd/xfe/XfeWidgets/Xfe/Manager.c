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

#define MIN_LAYOUT_WIDTH	10
#define MIN_LAYOUT_HEIGHT	10

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class Methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void				Initialize		(Widget,Widget,ArgList,Cardinal *);
static void				ClassInitialize	();
static void				ClassPartInit	(WidgetClass);
static void				Destroy			(Widget);
static void				Resize			(Widget);
static void				Realize			(Widget,XtValueMask *,
										 XSetWindowAttributes *);
static void				Redisplay		(Widget,XEvent *,Region);
static Boolean			SetValues		(Widget,Widget,Widget,ArgList,
										 Cardinal *);
static XtGeometryResult QueryGeometry	(Widget,XtWidgetGeometry *,
										 XtWidgetGeometry *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void				InsertChild		(Widget);
static void				DeleteChild		(Widget);
static void				ChangeManaged	(Widget);
static XtGeometryResult GeometryManager	(Widget,XtWidgetGeometry *,
										 XtWidgetGeometry *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Composite XtOrderProc for XmNinsertPosition							*/
/*																		*/
/*----------------------------------------------------------------------*/
static Cardinal	InsertPosition		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			ConstraintInitialize	(Widget,Widget,ArgList,Cardinal *);
static Boolean		ConstraintSetValues		(Widget,Widget,Widget,ArgList,
											 Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		InitializePostHook			(Widget,Widget);
static Boolean	SetValuesPostHook			(Widget,Widget,Widget);
static void		ConstraintInitializePostHook(Widget,Widget);
static Boolean	ConstraintSetValuesPostHook	(Widget,Widget,Widget);
static void		PreferredGeometry			(Widget,Dimension *,Dimension *);
static void		MinimumGeometry			(Widget,Dimension *,Dimension *);
static void		UpdateRect					(Widget);
static void		DrawShadow					(Widget,XEvent *,Region,
											 XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Resources													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
	/* Callback resources */
    { 
		XmNlayoutCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeManagerRec , xfe_manager . layout_callback),
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
    { 
		XmNchangeManagedCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfeManagerRec , xfe_manager . change_managed_callback),
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


	/* Misc resources */
    { 
		XmNignoreConfigure,
		XmCIgnoreConfigure,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeManagerRec , xfe_manager . ignore_configure),
		XmRImmediate, 
		(XtPointer) False
    },
    { 
		XmNshadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfeManagerRec , xfe_manager . shadow_type),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_SHADOW_TYPE
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
		(XtPointer) XfeDEFAULT_MARGIN_BOTTOM
    },
    { 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_left),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_MARGIN_LEFT
    },
    { 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_right),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_MARGIN_RIGHT
    },
    { 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfeManagerRec , xfe_manager . margin_top),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_MARGIN_TOP
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

	/* Private Component resources */
	{ 
		XmNnumPrivateComponents,
		XmCReadOnly,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfeManagerRec , xfe_manager . num_private_components),
		XmRImmediate, 
		(XtPointer) 0
    },

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
		(XtPointer) InsertPosition
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
/*																		*/
/* XfeManager Constraint Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource constraint_resources[] = 
{
    { 
		XmNpositionIndex,
		XmCPositionIndex,
		XmRInt,
		sizeof(int),
		XtOffsetOf(XfeManagerConstraintRec , xfe_manager . position_index),
		XmRImmediate,
		(XtPointer) XmLAST_POSITION
    },
    { 
		XmNprivateComponent,
		XmCPrivateComponent,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfeManagerConstraintRec , xfe_manager . private_component),
		XmRImmediate,
		(XtPointer) False
    },
};   

/* Widget class record initialization. */
_XFE_WIDGET_CLASS_RECORD(manager,Manager) =
{
    {
		(WidgetClass) &xmManagerClassRec,		/* superclass			*/
		"XfeManager",							/* class_name			*/
		sizeof(XfeManagerRec),					/* widget_size			*/
		ClassInitialize,						/* class_initialize		*/
		ClassPartInit,							/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		Realize,								/* realize				*/
		NULL,									/* actions				*/
		0,										/* num_actions			*/
		resources,								/* resources			*/
		XtNumber(resources),					/* num_resources		*/
		NULLQUARK,								/* xrm_class			*/
		TRUE,									/* compress_motion		*/
		XtExposeCompressMaximal,				/* compress_exposure	*/
		TRUE,									/* compress_enterleave	*/
		FALSE,									/* visible_interest		*/
		Destroy,								/* destroy				*/
		Resize,									/* resize				*/
		Redisplay,								/* expose				*/
		SetValues,								/* set_values			*/
		NULL,									/* set_values_hook		*/
		XtInheritSetValuesAlmost,				/* set_values_almost	*/
		NULL,									/* get_values_hook		*/
		NULL,									/* accept_focus			*/
		XtVersion,								/* version				*/
		NULL,									/* callback_private		*/
		XtInheritTranslations,					/* tm_table				*/
		QueryGeometry,							/* query_geometry		*/
		XtInheritDisplayAccelerator,			/* display accel		*/
		NULL,									/* extension			*/
    },

    /* Composite Part */
    {
		GeometryManager,						/* geometry_manager		*/
		ChangeManaged,							/* change_managed		*/
		InsertChild,							/* insert_child			*/
		DeleteChild,							/* delete_child			*/
		NULL									/* extension			*/
    },
    
    /* Constraint Part */
    {
		constraint_resources,					/* resource list		*/
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
		syn_resources,							/* syn resources		*/
		XtNumber(syn_resources),				/* num syn_resources	*/
		NULL,									/* syn_cont_resources	*/
		0,										/* num_syn_cont_res		*/
		XmInheritParentProcess,					/* parent_process		*/
		NULL,									/* extension			*/
	},

    /* XfeManager Part */
    {
		ForgetGravity,							/* bit_gravity			*/
		PreferredGeometry,						/* preferred_geometry	*/
		MinimumGeometry,						/* minimum_geometry	*/
		UpdateRect,								/* update_rect			*/
		NULL,									/* accept_child			*/
		NULL,									/* insert_child			*/
		NULL,									/* delete_child			*/
		NULL,									/* change_managed		*/
		NULL,									/* prepare_components	*/
		NULL,									/* layout_components	*/
		NULL,									/* layout_children		*/
		NULL,									/* draw_background		*/
		DrawShadow,								/* draw_shadow			*/
		NULL,									/* draw_components		*/
		NULL									/* extension			*/
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
/* Core Class Methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ClassInitialize()
{
	/* Register Xfe Converters */
    XfeRegisterConverters();

    /* Register Representation Types */
    XfeRegisterRepresentationTypes();
}
/*----------------------------------------------------------------------*/
static void
ClassPartInit(WidgetClass wc)
{
    XfeManagerWidgetClass cc = (XfeManagerWidgetClass) wc;
    XfeManagerWidgetClass sc = (XfeManagerWidgetClass) wc->core_class.superclass;

    _XfeResolve(cc,sc,xfe_manager_class,bit_gravity,
				XfeInheritBitGravity);
   
    _XfeResolve(cc,sc,xfe_manager_class,preferred_geometry,
				XfeInheritPreferredGeometry);

    _XfeResolve(cc,sc,xfe_manager_class,minimum_geometry,
				XfeInheritMinimumGeometry);

    _XfeResolve(cc,sc,xfe_manager_class,update_rect,
				XfeInheritUpdateRect);

    _XfeResolve(cc,sc,xfe_manager_class,accept_child,
				XfeInheritAcceptChild);

    _XfeResolve(cc,sc,xfe_manager_class,insert_child,
				XfeInheritInsertChild);

    _XfeResolve(cc,sc,xfe_manager_class,delete_child,
				XfeInheritDeleteChild);

    _XfeResolve(cc,sc,xfe_manager_class,change_managed,
				XfeInheritChangeManaged);

    _XfeResolve(cc,sc,xfe_manager_class,layout_components,
				XfeInheritLayoutComponents);

    _XfeResolve(cc,sc,xfe_manager_class,layout_children,
				XfeInheritLayoutChildren);
   
    _XfeResolve(cc,sc,xfe_manager_class,draw_background,
				XfeInheritDrawBackground);
   
    _XfeResolve(cc,sc,xfe_manager_class,draw_shadow,
				XfeInheritDrawShadow);
   
    _XfeResolve(cc,sc,xfe_manager_class,draw_components,
				XfeInheritDrawComponents);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    /* Initialize private members */
    _XfemComponentFlag(nw)			= True;
  
    /* Make sure the shadow is ok */
    XfeRepTypeCheck(nw,XmRShadowType,&_XfemShadowType(nw),
					XfeDEFAULT_SHADOW_TYPE);

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfeManagerWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Realize(Widget w,XtValueMask *mask,XSetWindowAttributes* wa)
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
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    /* Remove all CallBacks */
    /* XtRemoveAllCallbacks(w,XmNlayoutCallback); */
    /* XtRemoveAllCallbacks(w,XmNresizeCallback); */
    /* XtRemoveAllCallbacks(w,XmNexposeCallback); */
}
/*----------------------------------------------------------------------*/
static void
Resize(Widget w)
{
	/*printf("%s: Resize to (%d,%d)\n",XtName(w),_XfeWidth(w),_XfeHeight(w));*/

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
    
    /* Update the widget rect */
    _XfeManagerUpdateRect(w);
    
    /* Layout the components */
    _XfeManagerLayoutComponents(w);
    
    /* Layout the children */
    _XfeManagerLayoutChildren(w);

    /* Invoke Resize Callbacks */
	_XfeInvokeCallbacks(w,_XfemResizeCallback(w),XmCR_RESIZE,NULL,False);
}
/*----------------------------------------------------------------------*/
static void
Redisplay(Widget w,XEvent *event,Region region)
{
	/* Make sure the widget is realized before drawing ! */
	if (!XtIsRealized(w)) return;
   
	/* Draw Background */ 
	_XfeManagerDrawBackground(w,event,region,&_XfemWidgetRect(w));

	/* Draw Shadow */ 
	_XfeManagerDrawShadow(w,event,region,&_XfemWidgetRect(w));

	/* Draw Components */ 
	_XfeManagerDrawComponents(w,event,region,&_XfemWidgetRect(w));

	/* Redisplay Gadgets */
	_XmRedisplayGadgets(w,event,region);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
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

	/* num_private_components */
	if (_XfemNumPrivateComponents(nw) != _XfemNumPrivateComponents(ow))
	{
		_XfemNumPrivateComponents(nw) = _XfemNumPrivateComponents(ow);
      
		_XfeWarning(nw,MESSAGE15);
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
		XfeRepTypeCheck(nw,XmRShadowType,&_XfemShadowType(nw),XfeDEFAULT_SHADOW_TYPE);

		_XfemConfigFlags(nw) |= XfeConfigExpose;
	}

	return _XfeManagerChainSetValues(ow,rw,nw,xfeManagerWidgetClass);
}
/*----------------------------------------------------------------------*/
static XtGeometryResult
QueryGeometry(Widget w,XtWidgetGeometry	*req,XtWidgetGeometry *reply)
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
/* Composite Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
InsertChild(Widget child)
{
    Widget                  w = XtParent(child);
    XmManagerWidgetClass    mwc = (XmManagerWidgetClass) xmManagerWidgetClass;

    /* Leave private components alone */
    if (_XfemComponentFlag(w) || _XfeManagerPrivateComponent(child))
	{
		/* Mark the child as a private component */
		_XfeManagerPrivateComponent(child) = True;

		/* Increment the private component count */
		_XfemNumPrivateComponents(w)++;

        /* Call XmManager's InsertChild */
        (*mwc->composite_class.insert_child)(child);
    }
    /* Accept or reject other children */
    else if (_XfeManagerAcceptChild(child))
    {
        /* Call XmManager's InsertChild */
        (*mwc->composite_class.insert_child)(child);

		/* Assign the position index for this child.
		 *
		 * This will probably turn out to be a not so trivial thing,
		 * cause of managing state and other stuff.  Fix as needed.
		 */
		_XfeManagerPositionIndex(child) = 
			_XfemNumChildren(w) - _XfemNumPrivateComponents(w) - 1;
        
        /* Insert the child and relayout if necessary */
        if (_XfeManagerInsertChild(child))
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
DeleteChild(Widget child)
{
    Widget                  w = XtParent(child);
    XmManagerWidgetClass    mwc = (XmManagerWidgetClass) xmManagerWidgetClass;
    Boolean                 layout = XtIsManaged(child);

    /* Leave private components alone */
    if (!_XfeManagerPrivateComponent(child))
    {
        /* Delete the child and relayout if necessary */
        layout = _XfeManagerDeleteChild(child);
	}
	else
	{
		/* Increment the private component count */
		_XfemNumPrivateComponents(w)++;
    }
    
    /* call manager DeleteChild to update child info */
    (*mwc->composite_class.delete_child)(child);

    if (layout)
    {
        XfeManagerLayout(w);
    }
}
/*----------------------------------------------------------------------*/
static void
ChangeManaged(Widget w)
{
    /* Update widget geometry only if ignore_configure is False */
    if (!_XfemIgnoreConfigure(w))
    {
		Boolean		change_width = False;
		Boolean		change_height = False;

#if 0
		/* Prepare the Widget */
		_XfeManagerPrepareComponents(w,_XfemPrepareFlags(w));
#endif

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
#if 0			
			/* Make a geometry request to accomodate new geometry */
			_XfeWidth(w) = _XfemPreferredWidth(w);
			_XfeHeight(w) = _XfemPreferredHeight(w);
#endif
		}

		/* Update the widget rect */
		_XfeManagerUpdateRect(w);
		
		/* Layout the components */
		_XfeManagerLayoutComponents(w);
		
		/* Layout the children */
		_XfeManagerLayoutChildren(w);
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
GeometryManager(Widget child,XtWidgetGeometry *request,XtWidgetGeometry *reply)
{
#if 1
	Widget				w = XtParent(child);
	XtGeometryMask		mask = request->request_mode;
	XtGeometryResult	our_result = XtGeometryNo;

/* 	printf("GeometryManager(%s) - child = %s\n",XtName(w),XtName(child)); */
	
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
		if (!_XfemIgnoreConfigure(XtParent(w)))
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
InsertPosition(Widget child)
{
	Widget w = _XfeParent(child);

 	/* Insert private components at the tail */
    if (_XfeManagerPrivateComponent(child))
    {
      return _XfemNumChildren(w);
    }

	/* Insert children before private components */
	if (_XfemNumPrivateComponents(w) > 0)
	{
		return _XfemNumChildren(w) - _XfemNumPrivateComponents(w);
	}

	/* Otherwise return the default position - tail */
	return _XfemNumChildren(w);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint Class Methods												*/
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
/* 	XfeManagerConstraintPart *	np = _XfeManagerConstraintPart(nw); */
/* 	XfeManagerConstraintPart *	op = _XfeManagerConstraintPart(ow); */

	/* Reset the Configuration Flags */
	_XfemConfigFlags(w) = XfeConfigNone;

	/* Reset the preparation Flags */
	_XfemPrepareFlags(w) = XfePrepareNone;


	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(ow,rw,nw,xfeManagerWidgetClass);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Class Methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
InitializePostHook(Widget rw,Widget nw)
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
    
    /* Update the widget rect */
    _XfeManagerUpdateRect(nw);
    
    /* Layout the components */
    _XfeManagerLayoutComponents(nw);
    
    /* Layout the children */
    _XfeManagerLayoutChildren(nw);
    
    _XfemComponentFlag(nw) = False;
    _XfemPrepareFlags(nw) = XfePrepareNone;

	_XfemOldWidth(nw) = XfeGEOMETRY_INVALID_DIMENSION;
	_XfemOldHeight(nw) = XfeGEOMETRY_INVALID_DIMENSION;
}
/*----------------------------------------------------------------------*/
static Boolean
SetValuesPostHook(Widget ow,Widget rw,Widget nw)
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
	
	/* Update the widget rect */
	_XfeManagerUpdateRect(nw);

	/* Clear the window if it is dirty and realized */
	if (XtIsRealized(nw) && (_XfemConfigFlags(nw) & XfeConfigDirty))
	{
		XClearWindow(XtDisplay(nw),_XfeWindow(nw));
	}
	
	if (_XfemConfigFlags(nw) & XfeConfigLayout)
	{
		/* Layout the components */
		_XfeManagerLayoutComponents(nw);
		
		/* Layout the children */
		_XfeManagerLayoutChildren(nw);
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
	
	/* Update the widget rect */
	_XfeManagerUpdateRect(w);

	/* Clear the window if it is dirty and realized */
	if (XtIsRealized(w) && (_XfemConfigFlags(w) & XfeConfigDirty))
	{
		XClearWindow(XtDisplay(w),_XfeWindow(w));
	}
	
	if (_XfemConfigFlags(w) & XfeConfigLayout)
	{
		/* Layout the components */
		_XfeManagerLayoutComponents(w);
		
		/* Layout the children */
		_XfeManagerLayoutChildren(w);
	}
	
	return (_XfemConfigFlags(w) & XfeConfigExpose);
}
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension *width,Dimension *height)
{
	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);
}
/*----------------------------------------------------------------------*/
static void
MinimumGeometry(Widget w,Dimension *width,Dimension *height)
{
	*width  = _XfemOffsetLeft(w) + _XfemOffsetRight(w);
	*height = _XfemOffsetTop(w) + _XfemOffsetBottom(w);
}
/*----------------------------------------------------------------------*/
static void
UpdateRect(Widget w)
{
    XfeRectSet(&_XfemWidgetRect(w),
			   
			   _XfemOffsetLeft(w),
			   
			   _XfemOffsetTop(w),
			   
			   _XfeWidth(w) - _XfemOffsetLeft(w) - _XfemOffsetRight(w),
			   
			   _XfeHeight(w) - _XfemOffsetTop(w) - _XfemOffsetBottom(w));
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
/* XfeManager Method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
void
_XfeManagerPreferredGeometry(Widget w,Dimension *width,Dimension *height)
{
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);

	Dimension				min_width = 
		XfeMax(_XfemMinWidth(w),XfeMANAGER_DEFAULT_WIDTH);

	Dimension				min_height = 
		XfeMax(_XfemMinHeight(w),XfeMANAGER_DEFAULT_HEIGHT);
	
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
void
_XfeManagerMinimumGeometry(Widget w,Dimension *width,Dimension *height)
{
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);

	if (mc->xfe_manager_class.minimum_geometry)
	{
		(*mc->xfe_manager_class.minimum_geometry)(w,width,height);
	}
}
/*----------------------------------------------------------------------*/
void
_XfeManagerUpdateRect(Widget w)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.update_rect)
	{
		(*mc->xfe_manager_class.update_rect)(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeManagerAcceptChild(Widget child)
{
	Boolean					result = True;
	Widget					w = XtParent(child);
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.accept_child)
	{
		result = (*mc->xfe_manager_class.accept_child)(child);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeManagerInsertChild(Widget child)
{
	Boolean					result = XtIsManaged(child);
	Widget					w = XtParent(child);
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.insert_child)
	{
		result = (*mc->xfe_manager_class.insert_child)(child);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeManagerDeleteChild(Widget child)
{
	Boolean					result = XtIsManaged(child);
	Widget					w = XtParent(child);
	XfeManagerWidgetClass	mc = (XfeManagerWidgetClass) XtClass(w);
	
	if (mc->xfe_manager_class.delete_child)
	{
		result = (*mc->xfe_manager_class.delete_child)(child);
	}

	return result;
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
void
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
void
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
void
_XfeManagerLayoutChildren(Widget w)
{
	XfeManagerWidgetClass mc = (XfeManagerWidgetClass) XtClass(w);

 	if ((_XfeWidth(w) <= MIN_LAYOUT_WIDTH) || (_XfeHeight(w) <= MIN_LAYOUT_HEIGHT))
	{
		return;
	}
	
	if (mc->xfe_manager_class.layout_children)
	{
		(*mc->xfe_manager_class.layout_children)(w);
	}
}
/*----------------------------------------------------------------------*/
void
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
void
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
void
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

/* Public Functions */
/*----------------------------------------------------------------------*/
void
XfeManagerLayout(Widget w)
{
	assert( _XfeIsAlive(w) );
	assert( XfeIsManager(w) );

	XfeResize(w);

#if 0	
   /* Setup the max children dimensions */
	_XfeManagerChildrenInfo(w,
							&_XfemMaxChildWidth(w),
							&_XfemMaxChildHeight(w),
							&_XfemTotalChildrenWidth(w),
							&_XfemTotalChildrenHeight(w),
							&_XfemNumManaged(w),
							&_XfemNumComponents(w));
	

	/* Make sure some components exist */
	if (!_XfemNumComponents(w))
	{
		return;
	}
	
	/* Layout the components */
	_XfeManagerLayoutComponents(w);
	
	/* Layout the children */
	_XfeManagerLayoutChildren(w);
#endif
}
/*----------------------------------------------------------------------*/
void
XfeManagerSetChildrenValues(Widget		w,
							ArgList		args,
							Cardinal	count,
							Boolean		only_managed)
{
	Cardinal i;
   
	assert(w != NULL);
   
	/* Make sure its a Manager */
	if (!XfeIsManager(w))
	{
		_XfeWarning(w,MESSAGE5);
		return;
	}

	_XfemIgnoreConfigure(w) = True;

	/* Iterate through all the items */
	for (i = 0; i < _XfemNumChildren(w); i++)
	{
		Widget obj = _XfemChildren(w)[i];

		if (_XfeIsAlive(obj))
		{
			if (only_managed && XtIsManaged(obj))
			{
				XtSetValues(obj,args,count);
			}
			else
			{
				XtSetValues(obj,args,count);
			}
		}
	}
   
	_XfemIgnoreConfigure(w) = False;

/*    XfeConfigure(w); */
}
/*----------------------------------------------------------------------*/
void
XfeManagerApply(Widget				w,
				XfeManagerApplyProc	proc,
				XtPointer			data,
				Boolean				only_managed)
{
   Cardinal i;

   assert(w != NULL);

   /* Make sure its a Manager */
   if (!XfeIsManager(w))
   {
      _XfeWarning(w,MESSAGE5);

      return;
   }

	/* Show the action button as needed */
	_XfemIgnoreConfigure(w) = True;

   /* Iterate through all the items */
   for (i = 0; i < _XfemNumChildren(w); i++)
   {
	   Widget child = _XfemChildren(w)[i];
	   
	   if (child && 
		   _XfeIsAlive(child) && 
		   !_XfeManagerPrivateComponent(child))
	   {
		   if (only_managed)
		   {
			   if (XtIsManaged(child))
			   {
				   proc(w,child,data);
			   }
		   }
		   else
		   {
			   proc(w,child,data);
		   }
	   }
   }

	_XfemIgnoreConfigure(w) = False;

	XfeManagerLayout(w);
}


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager private functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeManagerChainInitialize(Widget rw,Widget nw,WidgetClass wc)
{
	assert( nw != NULL );

	if (XtClass(nw) == wc)
	{
		InitializePostHook(rw,nw);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfeManagerChainSetValues(Widget ow,Widget rw,Widget nw,WidgetClass wc)
{
	assert( nw != NULL );

	if (XtClass(nw) == wc)
	{
		return SetValuesPostHook(ow,rw,nw);
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
/*																		*/
/* Name:		_XfeManagerChildrenInfo()								*/
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
_XfeManagerChildrenInfo(Widget			w,
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
		if (_XfeManagerPrivateComponent(child))
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
/* Name:		_XfeManagerComponentInfo()								*/
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
_XfeManagerComponentInfo(Widget				w,
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
		if (_XfeManagerPrivateComponent(child) &&
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
