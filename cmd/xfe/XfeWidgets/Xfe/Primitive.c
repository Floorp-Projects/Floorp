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
/* Name:		<Xfe/Primitive.c>										*/
/* Description:	XfePrimitive widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>

#include <Xfe/PrimitiveP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings and messages												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define MESSAGE0 "XfePrimitive is an abstract class and cannot be instanciated."
#define MESSAGE1 "Widget is not XfePrimitive."
#define MESSAGE2 "XmNpreferredHeight is a read-only resource."
#define MESSAGE3 "XmNpreferredWidth is a read-only resource."
#define MESSAGE4 "XmNpointerInside is a read-only resource."
#define MESSAGE5 "XmNnumPopupChildren is a read-only resource."
#define MESSAGE6 "XmNpopupChildren is a read-only resource."
#define MESSAGE7 "Cannot change XmNbufferType after initialization."
#define MESSAGE8 "Widget's width must be greater than 0."
#define MESSAGE9 "Widget's height must be greater than 0."

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class Methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void				ClassInitialize	();
static void				ClassPartInit	(WidgetClass);
static void				Initialize		(Widget,Widget,ArgList,Cardinal *);
static void				Destroy			(Widget);
static void				Redisplay		(Widget,XEvent *,Region);
static void				Realize			(Widget,XtValueMask *,
										 XSetWindowAttributes *);
static void				Resize			(Widget);
static Boolean			SetValues		(Widget,Widget,Widget,ArgList,
										 Cardinal *);
static XtGeometryResult	QueryGeometry	(Widget,XtWidgetGeometry *,
										 XtWidgetGeometry *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XmPrimitive Extension Methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean		WidgetDisplayRect	(Widget,XRectangle  *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Class Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void			InitializePostHook	(Widget,Widget);
static Boolean		SetValuesPostHook	(Widget,Widget,Widget);
static void			PreferredGeometry	(Widget,Dimension *,Dimension *);
static void			MinimumGeometry	(Widget,Dimension *,Dimension *);
static void			UpdateRect			(Widget);
static void			DrawShadow			(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Buffer Pixmap functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	BufferAllocate			(Widget);
static void 	BufferFree				(Widget);
static void 	BufferUpdate			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfePrimitive functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static Dimension		GetWidth		(Widget);
static Dimension		GetHeight		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtResource resources[] = 
{
	/* Callback resources */
	{ 
		XmNenterCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . enter_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNfocusCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . focus_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNleaveCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . leave_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},
	{ 
		XmNresizeCallback,
		XmCCallback,
		XmRCallback,
		sizeof(XtCallbackList),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . resize_callback),
		XmRImmediate, 
		(XtPointer) NULL,
	},

	/* Cursor resources */
	{ 
		XmNcursor,
		XmCCursor,
		XmRCursor,
		sizeof(Cursor),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . cursor),
		XmRImmediate, 
		(XtPointer) None
	},

	/* Appearance resources */
	{ 
		XmNshadowType,
		XmCShadowType,
		XmRShadowType,
		sizeof(unsigned char),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . shadow_type),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_SHADOW_TYPE
	},
	{ 
		XmNbufferType,
		XmCBufferType,
		XmRBufferType,
		sizeof(unsigned char),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . buffer_type),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_BUFFER_TYPE
	},
	{ 
		XmNpretendSensitive,
		XmCPretendSensitive,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . pretend_sensitive),
		XmRImmediate, 
		(XtPointer) True
	},

	/* Geometry resources */
	{
		XmNusePreferredHeight,
		XmCUsePreferredHeight,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . use_preferred_height),
		XmRImmediate, 
		(XtPointer) True
	},
	{
		XmNusePreferredWidth,
		XmCUsePreferredWidth,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . use_preferred_width),
		XmRImmediate, 
		(XtPointer) True
	},
	{ 
		XmNmarginBottom,
		XmCMarginBottom,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . margin_bottom),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_MARGIN_BOTTOM 
	},
	{ 
		XmNmarginLeft,
		XmCMarginLeft,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . margin_left),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_MARGIN_LEFT 
	},
	{ 
		XmNmarginRight,
		XmCMarginRight,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . margin_right),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_MARGIN_RIGHT 
	},
	{ 
		XmNmarginTop,
		XmCMarginTop,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . margin_top),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_MARGIN_TOP 
	},

	/* For c++ usage */
	{ 
		XmNinstancePointer,
		XmCInstancePointer,
		XmRPointer,
		sizeof(XtPointer),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . instance_pointer),
		XmRImmediate, 
		(XtPointer) NULL
	},


	/* Read-only resources */
	{ 
		XmNpointerInside,
		XmCReadOnly,
		XmRBoolean,
		sizeof(Boolean),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . pointer_inside),
		XmRImmediate, 
		(XtPointer) False
	},
	{ 
		XmNpreferredHeight,
		XmCReadOnly,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . preferred_height),
		XmRImmediate, 
		(XtPointer) True
	},
	{ 
		XmNpreferredWidth,
		XmCReadOnly,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . preferred_width),
		XmRImmediate, 
		(XtPointer) True
	},

	/* Core popup resources */
	{ 
		XmNnumPopupChildren,
		XmCReadOnly,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfePrimitiveRec , core . num_popups),
		XmRImmediate,
		(XtPointer) 0,
	},
	{ 
		XmNpopupChildren,
		XmCReadOnly,
		XmRWidgetList,
		sizeof(WidgetList),
		XtOffsetOf(XfePrimitiveRec , core . popup_list),
		XmRImmediate,
		(XtPointer) NULL,
	},

	/* Force a default shadow thickness */
	{ 
		XmNshadowThickness,
		XmCShadowThickness,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , primitive . shadow_thickness),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_SHADOW_THICKNESS 
	},

	/* Force the width and height to the preffered values */
	{ 
		XmNwidth,
		XmCWidth,
		XmRHorizontalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , core . width),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_PREFERRED_WIDTH
	},
	{ 
		XmNheight,
		XmCHeight,
		XmRVerticalDimension,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , core . height),
		XmRImmediate, 
		(XtPointer) XfeDEFAULT_PREFERRED_HEIGHT
	},
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Synthetic Resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmSyntheticResource syn_resources[] =
{
	{ 
		XmNmarginBottom,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . margin_bottom),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
	},
	{
		XmNmarginLeft,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . margin_left),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
	{ 
		XmNmarginRight,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . margin_right),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
	},
	{ 
		XmNmarginTop,
		sizeof(Dimension),
		XtOffsetOf(XfePrimitiveRec , xfe_primitive . margin_top),
		_XmFromVerticalPixels,
		_XmToVerticalPixels 
	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
	{ "Focus",		_XfePrimitiveFocus	},
	{ "Enter",		_XfePrimitiveEnter	},
	{ "Leave",		_XfePrimitiveLeave	},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XmPrimitive extension record initialization							*/
/*																		*/
/*----------------------------------------------------------------------*/
static XmPrimitiveClassExtRec primClassExtRec = 
{
	NULL,									/* next_extension			*/
	NULLQUARK,								/* record_type				*/
	XmPrimitiveClassExtVersion,				/* version					*/
	sizeof(XmPrimitiveClassExtRec),			/* record_size				*/
	NULL,									/* widget_baseline			*/
	WidgetDisplayRect,						/* widget_display_rect		*/
	NULL,									/* widget_margins			*/
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive widget class record initialization						*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(primitive,Primitive) =
{
    {
		/* Core Part */
		(WidgetClass) &xmPrimitiveClassRec,	/* superclass				*/
		"XfePrimitive",						/* class_name				*/
		sizeof(XfePrimitiveRec),			/* widget_size				*/
		ClassInitialize,					/* class_initialize			*/
		ClassPartInit,						/* class_part_initiali		*/
		FALSE,								/* class_inited				*/
		Initialize,							/* initialize				*/
		NULL,								/* initialize_hook			*/
		Realize,							/* realize					*/
		actions,							/* actions					*/
		XtNumber(actions),					/* num_actions				*/
		resources,							/* resources				*/
		XtNumber(resources),				/* num_resources			*/
		NULLQUARK,							/* xrm_class				*/
		TRUE,								/* compress_motion			*/
		XtExposeCompressMaximal,			/* compress_exposure		*/
		TRUE,								/* compress_enterleave		*/
		FALSE,								/* visible_interest			*/
		Destroy,							/* destroy					*/
		Resize,								/* resize					*/
		Redisplay,							/* expose					*/
		SetValues,							/* set_values				*/
		NULL,								/* set_values_hook			*/
		XtInheritSetValuesAlmost,			/* set_values_almost		*/
		NULL,								/* get_values_hook			*/
		NULL,								/* accept_focus				*/
		XtVersion,							/* version					*/
		NULL,								/* callback_private			*/
		_XfePrimitiveDefaultTranslations,	/* tm_table					*/
		QueryGeometry,						/* query_geometry			*/
		XtInheritDisplayAccelerator,		/* display accel			*/
		NULL,								/* extension				*/
    },

    /* XmPrimitive Part */
    {
		XmInheritBorderHighlight,			/* border_highlight			*/
		XmInheritBorderUnhighlight,			/* border_unhighlight		*/
		XtInheritTranslations,				/* translations				*/
		NULL,								/* arm_and_activate			*/
		syn_resources,						/* syn resources			*/
		XtNumber(syn_resources),			/* num syn_resources		*/
		(XtPointer) &primClassExtRec,		/* extension				*/
    },

    /* XfePrimitive Part */
    {
		ForgetGravity,						/* bit_gravity				*/
		PreferredGeometry,					/* preferred_geometry		*/
		MinimumGeometry,					/* minimum_geometry		*/
		UpdateRect,							/* update_rect				*/
		NULL,								/* prepare_components		*/
		NULL,								/* layout_components		*/
		NULL,								/* draw_background			*/
		DrawShadow,							/* draw_shadow				*/
		NULL,								/* draw_components			*/
		NULL								/* extension				*/
    }
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfePrimitiveWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(primitive,Primitive);

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
	XfePrimitiveWidgetClass cc = (XfePrimitiveWidgetClass) wc;
	XfePrimitiveWidgetClass sc = (XfePrimitiveWidgetClass) wc->core_class.superclass;

	/* Resolve inheritance of all XfePrimitive class methods */
	_XfeResolve(cc,sc,xfe_primitive_class,bit_gravity,
				XfeInheritBitGravity);
   
	_XfeResolve(cc,sc,xfe_primitive_class,preferred_geometry,
				XfeInheritPreferredGeometry);

	_XfeResolve(cc,sc,xfe_primitive_class,minimum_geometry,
				XfeInheritMinimumGeometry);
   
	_XfeResolve(cc,sc,xfe_primitive_class,update_rect,
				XfeInheritUpdateRect);

	_XfeResolve(cc,sc,xfe_primitive_class,layout_components,
				XfeInheritLayoutComponents);
   
	_XfeResolve(cc,sc,xfe_primitive_class,draw_background,
				XfeInheritDrawBackground);
   
	_XfeResolve(cc,sc,xfe_primitive_class,draw_shadow,
				XfeInheritDrawShadow);
   
	_XfeResolve(cc,sc,xfe_primitive_class,draw_components,
				XfeInheritDrawComponents);
}
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    /* Make sure the shadow is ok */
    XfeRepTypeCheck(nw,XmRShadowType,&_XfeShadowType(nw),XfeDEFAULT_SHADOW_TYPE);
    
    /* Make sure the buffer is ok */
    XfeRepTypeCheck(nw,XmRBufferType,&_XfeBufferType(nw),XfeDEFAULT_BUFFER_TYPE);

    /* Initialize private members */
    _XfeBufferPixmap(nw) = XmUNSPECIFIED_PIXMAP;
    _XfeBackgroundGC(nw) = NULL;

    /* Allocate the background gc if needed */
    if (_XfeBufferType(nw) != XmBUFFER_NONE)
    {
		_XfePrimitiveAllocateBackgroundGC(nw);
    }

	/* Finish initialization */
	_XfePrimitiveChainInitialize(rw,nw,xfePrimitiveWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
    /* Remove all CallBacks */
    /* XtRemoveAllCallbacks(w,XmNenterCallback); */
    /* XtRemoveAllCallbacks(w,XmNfocusCallback); */
    /* XtRemoveAllCallbacks(w,XmNleaveCallback); */
    /* XtRemoveAllCallbacks(w,XmNresizeCallback); */
	
    /* Free or release a possible buffer */
    BufferFree(w);

    /* Free background gc if needed */
    if (_XfeBackgroundGC(w))
    {
		_XfePrimitiveReleaseBackgroundGC(w);
    }
}
/*----------------------------------------------------------------------*/
static void
Redisplay(Widget w,XEvent *event,Region region)
{
   /* Make sure the widget is realized before drawing ! */
   if (!XtIsRealized(w))
   {
      return;
   }

   switch(_XfeBufferType(w))
   {
       /* No buffer: simply re-draw everything */
   case XmBUFFER_NONE:
       _XfePrimitiveDrawEverything(w,event,region);
       break;
       
       /* Single buffer: draw the buffer only */
   case XmBUFFER_PRIVATE:
       _XfePrimitiveDrawBuffer(w,event,region);
       break;
       
       /* Multiple buffer: update the buffer and draw it */
   case XmBUFFER_SHARED:
       _XfePrimitiveDrawEverything(w,event,region);
       _XfePrimitiveDrawBuffer(w,event,region);
       break;
   }
}
/*----------------------------------------------------------------------*/
static void
Realize(Widget w,XtValueMask * mask,XSetWindowAttributes* wa)
{
    XSetWindowAttributes attr;

    /* Make sure only subclasses of XfePrimitive get instanciated */
    if ((XtClass(w) == xfePrimitiveWidgetClass))
    {
		_XfeWarning(w,MESSAGE0);

		return;
    }

    /* Let XmPrimitive create the window */
    (*xmPrimitiveWidgetClass->core_class.realize)(w,mask,wa);
    
    /* Set the Bit Gravity */
    attr.bit_gravity = _XfePrimitiveAccessBitGravity(w);
    
    XChangeWindowAttributes(XtDisplay(w),_XfeWindow(w),CWBitGravity,&attr);

    /* Define the cursor if needed */
    if (_XfeCursorGood(_XfeCursor(w)) && _XfePointerInside(w))
	{
		XfeCursorDefine(w,_XfeCursor(w));
    }
}
/*----------------------------------------------------------------------*/
static void
Resize(Widget w)
{
	/*printf("%s: Resize to (%d,%d)\n",XtName(w),_XfeWidth(w),_XfeHeight(w));*/

    /* Obtain the Prefered Geometry */
    _XfePrimitivePreferredGeometry(w,
								   &_XfePreferredWidth(w),
								   &_XfePreferredHeight(w));
    
    /* Force the preferred dimensions if required */
    if (_XfeUsePreferredWidth(w))
    {
		_XfeWidth(w) = _XfePreferredWidth(w);
    }
    
    if (_XfeUsePreferredHeight(w))
    {
		_XfeHeight(w) = _XfePreferredHeight(w);
    }
    
    /* Update the widget rect */
    _XfePrimitiveUpdateRect(w);
    
    /* Layout the components */
    _XfePrimitiveLayoutComponents(w);
    
    switch(_XfeBufferType(w))
    {
		/* No buffer: nothing */
    case XmBUFFER_NONE:
		break;
	
		/* Single buffer: update the buffer size and contents */
    case XmBUFFER_PRIVATE:
		BufferUpdate(w);
		_XfePrimitiveDrawEverything(w,NULL,NULL);
		break;
	
	/* Multiple buffer: update the buffer size only */
    case XmBUFFER_SHARED:
		BufferUpdate(w);
		break;
    }
    
    /* Invoke Resize Callbacks */
    _XfeInvokeCallbacks(w,_XfeResizeCallbacks(w),XmCR_RESIZE,NULL,True);
}
/*----------------------------------------------------------------------*/
static XtGeometryResult
QueryGeometry(Widget w,XtWidgetGeometry	*req,XtWidgetGeometry *reply)
{
	assert( req != NULL );
	assert( reply != NULL );

	reply->request_mode	= CWWidth | CWHeight;

	/* Set the reply dimensions */
	reply->width  = GetWidth(w);

	reply->height = GetHeight(w);

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
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    /* Reset the configuration Flags */
    _XfeConfigFlags(nw) = XfeConfigNone;
    
    /* Reset the preparation Flags */
    _XfePrepareFlags(nw) = XfePrepareNone;
    
    /* buffer_type */
    if (_XfeBufferType(nw) != _XfeBufferType(ow))
    {
		_XfeBufferType(nw) = _XfeBufferType(ow);
	
		_XfeWarning(nw,MESSAGE7);
    }
    
    /* preferred_height */
    if (_XfePreferredHeight(nw) != _XfePreferredHeight(ow))
    {
		_XfePreferredHeight(nw) = _XfePreferredHeight(ow);
	
		_XfeWarning(nw,MESSAGE2);
    }
    
    /* preferred_width */
    if (_XfePreferredWidth(nw) != _XfePreferredWidth(ow))
    {
		_XfePreferredWidth(nw) = _XfePreferredWidth(ow);
	
		_XfeWarning(nw,MESSAGE3);
    }
    
    /* pointer_inside */
    if (_XfePointerInside(nw) != _XfePointerInside(ow))
    {
		_XfePointerInside(nw) = _XfePointerInside(ow);
	
		_XfeWarning(nw,MESSAGE4);
    }
    
    /* num_popups */
    if (_XfeNumPopups(nw) != _XfeNumPopups(ow))
    {
		_XfeNumPopups(nw) = _XfeNumPopups(ow);
	
		_XfeWarning(nw,MESSAGE5);
    }
    
    /* popup_list */
    if (_XfePopupList(nw) != _XfePopupList(ow))
    {
		_XfePopupList(nw) = _XfePopupList(ow);
	
		_XfeWarning(nw,MESSAGE6);
    }

    /* resize_width */
    if (_XfeUsePreferredWidth(nw) != _XfeUsePreferredWidth(ow))
    {
		if(_XfeUsePreferredWidth(nw))
		{
			_XfeConfigFlags(nw) |= (XfeConfigLayout|
									XfeConfigGeometry|
									XfeConfigExpose);
		}
    }

    /* resize_height */
    if (_XfeUsePreferredHeight(nw) != _XfeUsePreferredHeight(ow))
    {
		if(_XfeUsePreferredHeight(nw))
		{
			_XfeConfigFlags(nw) |= (XfeConfigLayout|
									XfeConfigGeometry|
									XfeConfigExpose);
		}
    }
    
    /* height */
    if (_XfeHeight(nw) != _XfeHeight(ow))
    {
		/* if resize_heigh is True, we dont allow width changes */
		if (_XfeUsePreferredHeight(nw)) 
		{
			_XfeHeight(nw) = _XfeHeight(ow);
		}
		else
		{
			_XfeConfigFlags(nw) |= (XfeConfigLayout|XfeConfigExpose);
		}
    }
    
    /* width */
    if (_XfeWidth(nw) != _XfeWidth(ow))
    {
		/* if resize_width is True, we dont allow width changes */
		if (_XfeUsePreferredWidth(nw)) 
		{
			_XfeWidth(nw) = _XfeWidth(ow);
		}
		else
		{
			_XfeConfigFlags(nw) |= (XfeConfigLayout|XfeConfigExpose);
		}
    }
    
    /* cursor */
    if (_XfeCursor(nw) != _XfeCursor(ow))
    {
		/* If the new cursor is good, define it */
		if (_XfeCursorGood(_XfeCursor(nw)))
		{
			XfeCursorDefine(nw,_XfeCursor(nw));
		}
		else
		{
			XfeCursorUndefine(nw);
		}
    }
    
    /* Changes that affect the layout and geometry */
    if ((_XfeHighlightThickness(nw)	!= _XfeHighlightThickness(ow)) ||
		(_XfeMarginTop(nw)		!= _XfeMarginTop(ow)) ||
		(_XfeMarginBottom(nw)		!= _XfeMarginBottom(ow)) ||
		(_XfeMarginLeft(nw)		!= _XfeMarginLeft(ow)) ||
		(_XfeMarginRight(nw)		!= _XfeMarginRight(ow)) ||       
		(_XfeShadowThickness(nw)		!= _XfeShadowThickness(ow)) ||
		(_XfeUnitType(nw)			!= _XfeUnitType(ow)))
    {
		_XfeConfigFlags(nw) |= (XfeConfigLayout|XfeConfigGeometry|XfeConfigExpose);
    }
    
    /* shadow_type */
    if (_XfeShadowType(nw) != _XfeShadowType(ow))
    {
		/* Make sure the new shadow type is ok */
		XfeRepTypeCheck(nw,XmRShadowType,&_XfeShadowType(nw),XfeDEFAULT_SHADOW_TYPE);
	
		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }
    
    /* sensitive */
    if (_XfeSensitive(nw) != _XfeSensitive(ow))
    {
		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }

    /* pretend_sensitive */
    if (_XfeIsSensitive(nw) != _XfeIsSensitive(ow))
    {
		_XfeConfigFlags(nw) |= XfeConfigExpose;
    }
    
    /* background_pixel or background_pixmap */
    if (((_XfeBackgroundPixel(nw) != _XfeBackgroundPixel(ow)) ||
		 (_XfeBackgroundPixmap(nw) != _XfeBackgroundPixmap(ow))) &&
		(_XfeBufferType(nw) != XmBUFFER_NONE))
    {
		/* Release the old background GC */
		_XfePrimitiveReleaseBackgroundGC(nw);
		
		/* Allocate the new background GC */
		_XfePrimitiveAllocateBackgroundGC(nw);
    }
	
    return _XfePrimitiveChainSetValues(ow,rw,nw,xfePrimitiveWidgetClass);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmPrimitive Extension Methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean
WidgetDisplayRect(Widget w,XRectangle *rect)
{
	rect->x	= _XfeRectX(w);
	rect->y	= _XfeRectY(w);
	rect->height = _XfeRectHeight(w);
	rect->width	= _XfeRectWidth(w);

	return (rect->width && rect->height);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Class Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
InitializePostHook(Widget rw,Widget nw)
{
    /* Set preparation flag so that all components get prepared */
    _XfePrepareFlags(nw) = XfePrepareAll;
    
    /* Prepare the Widget */
    _XfePrimitivePrepareComponents(nw,_XfePrepareFlags(nw));

    /* Obtain the preferred dimensions for the first time. */
    _XfePrimitivePreferredGeometry(nw,
								   &_XfePreferredWidth(nw),
								   &_XfePreferredHeight(nw));

	/* Set the new dimensions */
	_XfeWidth(nw)  = GetWidth(nw);

	_XfeHeight(nw) = GetHeight(nw);

    /* Update the widget rect */
    _XfePrimitiveUpdateRect(nw);
    
    /* Layout the Widget */
    _XfePrimitiveLayoutComponents(nw);
    
    switch(_XfeBufferType(nw))
    {
		/* No buffer: nothing */
    case XmBUFFER_NONE:
		break;
	
		/* Single buffer: update the buffer size and contents */
    case XmBUFFER_PRIVATE:
		BufferUpdate(nw);
		_XfePrimitiveDrawEverything(nw,NULL,NULL);
		break;
	
	/* Multiple buffer: update the buffer size only */
    case XmBUFFER_SHARED:
		BufferUpdate(nw);
		break;
    }
    
    /* Dont need to prepare components any more */
    _XfePrepareFlags(nw) = XfePrepareNone;
}
/*----------------------------------------------------------------------*/
static Boolean
SetValuesPostHook(Widget ow,Widget rw,Widget nw)
{
    Boolean result = False;

    /* Prepare the widget components if needed */
    if (_XfePrepareFlags(nw))
    {
		_XfePrimitivePrepareComponents(nw,_XfePrepareFlags(nw));
    }
    
    /* Update the widget's geometry if needed */
    if (_XfeConfigFlags(nw) & XfeConfigGeometry)
    {
		/* Obtain the preferred dimensions */
		_XfePrimitivePreferredGeometry(nw,
									   &_XfePreferredWidth(nw),
									   &_XfePreferredHeight(nw));

		/* Set the new dimensions */
		_XfeWidth(nw)  = GetWidth(nw);

		_XfeHeight(nw) = GetHeight(nw);
    }
    
    /* Update the widget rect */
    _XfePrimitiveUpdateRect(nw);
    
    /* Layout the Widget if needed */
    if (_XfeConfigFlags(nw) & XfeConfigLayout)
    {
		_XfePrimitiveLayoutComponents(nw);

		switch(_XfeBufferType(nw))
		{
			/* No buffer: nothing */
		case XmBUFFER_NONE:
			break;
	    
			/* Single buffer: update the buffer size and contents */
		case XmBUFFER_PRIVATE:
			BufferUpdate(nw);
			_XfePrimitiveDrawEverything(nw,NULL,NULL);
			break;
	    
			/* Multiple buffer: update the buffer size only */
		case XmBUFFER_SHARED:
			BufferUpdate(nw);
			break;
		}
    }
    
    /* Draw everything (into the buffer) */
	if (_XfeBufferType(nw) != XmBUFFER_NONE)
    {
		if ((_XfeConfigFlags(nw) & XfeConfigLayout) || 
			(_XfeConfigFlags(nw) & XfeConfigExpose))
		{
			/* Redraw the buffer */
			_XfePrimitiveDrawEverything(nw,NULL,NULL);
	    
			/* Draw the buffer onto the window */
			XfeExpose(nw,NULL,NULL);
		}
    }
    else
    {
		result = (_XfeConfigFlags(nw) & XfeConfigExpose);
    }

	return result;
}
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension *width,Dimension *height)
{
	*width  = _XfeOffsetLeft(w) + _XfeOffsetRight(w);
	*height = _XfeOffsetTop(w)  + _XfeOffsetBottom(w);
}
/*----------------------------------------------------------------------*/
static void
MinimumGeometry(Widget w,Dimension *width,Dimension *height)
{
	*width  = _XfeOffsetLeft(w) + _XfeOffsetRight(w);
	*height = _XfeOffsetTop(w)  + _XfeOffsetBottom(w);
}
/*----------------------------------------------------------------------*/
static void
DrawShadow(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
    /* Draw the shadow */
    _XmDrawShadows(XtDisplay(w),
				   _XfePrimitiveDrawable(w),
				   _XfeTopShadowGC(w),_XfeBottomShadowGC(w),
				   _XfeHighlightThickness(w),
				   _XfeHighlightThickness(w),
				   _XfeWidth(w) - 2 * _XfeHighlightThickness(w),
				   _XfeHeight(w) - 2 * _XfeHighlightThickness(w),
				   _XfeShadowThickness(w),
				   _XfeShadowType(w));
}
/*----------------------------------------------------------------------*/
static void
UpdateRect(Widget w)
{
	/* Assign the rect coordinates */
	XfeRectSet(&_XfeWidgetRect(w),
			   
			   _XfeOffsetLeft(w),
			   
			   _XfeOffsetTop(w),
			   
			   _XfeWidth(w) - _XfeOffsetLeft(w) - _XfeOffsetRight(w),
			   
			   _XfeHeight(w) - _XfeOffsetTop(w) - _XfeOffsetBottom(w));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Buffer Pixmap functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
BufferAllocate(Widget w)
{
    assert( _XfeBufferType(w) != XmBUFFER_NONE );

    switch(_XfeBufferType(w))
    {
		/* Single buffer: allocate a buffer big enough for widget */
    case XmBUFFER_PRIVATE:

		_XfeBufferPixmap(w) = XCreatePixmap(XtDisplay(w),
											DefaultRootWindow(XtDisplay(w)),
											_XfeWidth(w),
											_XfeHeight(w),
											_XfeDepth(w));


#if 0
		printf("%s: Allocating a private buffer at (%d,%d)\n",
			   XtName(w),
			   _XfeWidth(w),_XfeHeight(w));
#endif

		break;

		/* Multiple buffer: ask the buffer manager for a buffer */
    case XmBUFFER_SHARED:

#if 0
		printf("%s: Allocating a shared buffer at (%d,%d)\n",
			   XtName(w),
			   _XfeWidth(w),_XfeHeight(w));
#endif

		_XfeBufferPixmap(w) = _XfePixmapBufferAllocate(w);

		break;
    }

}
/*----------------------------------------------------------------------*/
static void
BufferFree(Widget w)
{
    /*if (_XfeBufferType(w) != XmBUFFER_NONE );*/

    switch(_XfeBufferType(w))
    {
		/* Single buffer: Free the buffer pixmap only if needed */
    case XmBUFFER_PRIVATE:

		if (_XfePixmapGood(_XfeBufferPixmap(w)))
		{
			XFreePixmap(XtDisplay(w),_XfeBufferPixmap(w));
		}

		break;

		/* Multiple buffer: release the shared buffer */
    case XmBUFFER_SHARED:

		/*_XfePixmapBufferRelease(w,_XfeBufferPixmap(w));*/

		break;

		/* Nothing */
    case XmBUFFER_NONE:
		break;
    }
}
/*----------------------------------------------------------------------*/
static void
BufferUpdate(Widget w)
{
    assert( _XfeBufferType(w) != XmBUFFER_NONE );

    BufferFree(w);
    BufferAllocate(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc XfePrimitive functions											*/
/*																		*/
/*----------------------------------------------------------------------*/

/*
 * The preferred width is computed as follows.  The criteria is listed in
 * order of precedence.
 *
 *  min_width		:= (2 * shadow + 2 * highlight) -	this is the min
 *														width set by the
 *														XmPrimitive class'
 *														Initialize()
 *
 *  default_width	:= XfeDEFAULT_PREFERRED_WIDTH -		Defined to 0 (zero)
 *
 *
 * 1.	If (XmNusePreferredWidth == true) then force the preferred width
 *
 * 2.	If (XmNusePreferredWidth == false) and 
 *		(width == XfeDEFAULT_PREFERRED_WIDTH) or (width <= min_width)
 *		then force the preferred width.
 *
 * 3.	If (XmNusePreferredWidth == false) and (width > min_width) then
 *		use the dimension given by w->core.width;
 *
 * The same logic applies to the height.  This assumes the preferred 
 * width is something reasonable; but that is up to the subclasses.
 *
 */

static Dimension
GetWidth(Widget w)
{
	Dimension	min_width;
	Dimension	width;

	/* Compute the min possible width */
	min_width = 2 * _XfeHighlightThickness(w) + 2 * _XfeShadowThickness(w);

	/* A reasonable preferred width is needed */
	assert( _XfePreferredWidth(w) > 0 );
	assert( _XfePreferredWidth(w) >= min_width );

    /* Force the preferred width if needed */
    if (_XfeUsePreferredWidth(w) || 
		(_XfeWidth(w) == XfeDEFAULT_PREFERRED_WIDTH) ||
		(_XfeWidth(w) <= min_width))
	{
		width = _XfePreferredWidth(w);
	}
	else
	{
		width = _XfeWidth(w);
	}

	return width;
}
/*----------------------------------------------------------------------*/
static Dimension
GetHeight(Widget w)
{
	Dimension	min_height;
	Dimension	height;

	/* Compute the min possible height */
	min_height = 2 * _XfeHighlightThickness(w) + 2 * _XfeShadowThickness(w);

	/* A reasonable preferred height is needed */
	assert( _XfePreferredHeight(w) > 0 );
	assert( _XfePreferredHeight(w) >= min_height );

    /* Force the preferred height if needed */
    if (_XfeUsePreferredHeight(w) || 
		(_XfeHeight(w) == XfeDEFAULT_PREFERRED_HEIGHT) ||
		(_XfeHeight(w) <= min_height))
	{
		height = _XfePreferredHeight(w);
	}
	else
	{
		height = _XfeHeight(w);
	}

	return height;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
void
_XfePrimitivePreferredGeometry(Widget w,Dimension *width,Dimension *height)
{
	XfePrimitiveWidgetClass pc = (XfePrimitiveWidgetClass) XtClass(w);

	if (pc->xfe_primitive_class.preferred_geometry)
	{
		(*pc->xfe_primitive_class.preferred_geometry)(w,width,height);
	}

	/* Make sure preferred width is greater than zero */
	if (*width == 0)
	{
		*width = XfePRIMITIVE_DEFAULT_WIDTH;
	}

	/* Make sure preferred height is greater than zero */
	if (*height == 0)
	{
		*height = XfePRIMITIVE_DEFAULT_HEIGHT;
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveMinimumGeometry(Widget w,Dimension *width,Dimension *height)
{
	XfePrimitiveWidgetClass pc = (XfePrimitiveWidgetClass) XtClass(w);

	if (pc->xfe_primitive_class.minimum_geometry)
	{
		(*pc->xfe_primitive_class.minimum_geometry)(w,width,height);
	}

	/* Make sure preferred width is greater than zero */
	if (*width == 0)
	{
		*width = XfePRIMITIVE_DEFAULT_WIDTH;
	}

	/* Make sure preferred height is greater than zero */
	if (*height == 0)
	{
		*height = XfePRIMITIVE_DEFAULT_HEIGHT;
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitivePrepareComponents(Widget w,int flags)
{
	WidgetClass					cc;
	XfePrimitiveWidgetClass		pc;
	Cardinal					i;
	Cardinal					count;

	/* Someone has to be nuts to have more than 32 subclasses... */
	static XfePrepareProc		proc_table[32];

	/* Traverse all classes until we find XmPrimitive */
	for (cc = XtClass(w),count = 0; 
		 cc != xmPrimitiveWidgetClass; 
		 cc = cc->core_class.superclass)
	{
		pc = (XfePrimitiveWidgetClass) cc;

		/* Add the method to the table as long as it is not NULL */
		if (pc->xfe_primitive_class.prepare_components)
		{
			proc_table[count++] = pc->xfe_primitive_class.prepare_components;
		}
	}

	/* Invoke the methods in reverse order */
	for (i = count; i; i--)
	{
		XfePrepareProc proc = proc_table[i - 1];

		/* Invoke the prepare components method for this class */
		(*proc)(w,flags);
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveUpdateRect(Widget w)
{
	XfePrimitiveWidgetClass pc = (XfePrimitiveWidgetClass) XtClass(w);

	if (pc->xfe_primitive_class.update_rect)
	{
		(*pc->xfe_primitive_class.update_rect)(w);
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveLayoutComponents(Widget w)
{
	XfePrimitiveWidgetClass pc = (XfePrimitiveWidgetClass) XtClass(w);

	if (pc->xfe_primitive_class.layout_components)
	{
		(*pc->xfe_primitive_class.layout_components)(w);
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveDrawBackground(Widget	w,
							XEvent *	event,
							Region	region,
							XRectangle *	clip_rect)
{
	XfePrimitiveWidgetClass	pc = (XfePrimitiveWidgetClass) XtClass(w);

	if (pc->xfe_primitive_class.draw_background)
	{
		(*pc->xfe_primitive_class.draw_background)(w,event,region,clip_rect);
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveDrawComponents(Widget	w,
							XEvent *	event,
							Region	region,
							XRectangle *	clip_rect)
{
	XfePrimitiveWidgetClass	pc = (XfePrimitiveWidgetClass) XtClass(w);

	if (pc->xfe_primitive_class.draw_components)
	{
		(*pc->xfe_primitive_class.draw_components)(w,event,region,clip_rect);
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveDrawShadow(Widget		w,
						XEvent *		event,
						Region		region,
						XRectangle *	rect)
{
	XfePrimitiveWidgetClass pc = (XfePrimitiveWidgetClass) XtClass(w);

	if (pc->xfe_primitive_class.draw_shadow)
	{
		(*pc->xfe_primitive_class.draw_shadow)(w,event,region,rect);
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveBorderHighlight(Widget w)
{
	XmPrimitiveWidgetClass pc = (XmPrimitiveWidgetClass) XtClass(w);

	if (pc->primitive_class.border_highlight)
	{
		(*pc->primitive_class.border_highlight)(w);
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveBorderUnhighlight(Widget w)
{
	XmPrimitiveWidgetClass pc = (XmPrimitiveWidgetClass) XtClass(w);

	if (pc->primitive_class.border_unhighlight)
	{
		(*pc->primitive_class.border_unhighlight)(w);
	}
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Private XfePrimitive functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfePrimitiveChainInitialize(Widget rw,Widget nw,WidgetClass wc)
{
	assert( nw != NULL );

	if (XtClass(nw) == wc)
	{
		InitializePostHook(rw,nw);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
_XfePrimitiveChainSetValues(Widget ow,Widget rw,Widget nw,WidgetClass wc)
{
	assert( nw != NULL );

	if (XtClass(nw) == wc)
	{
		return SetValuesPostHook(ow,rw,nw);
	}

	return False;
}
/*----------------------------------------------------------------------*/
Drawable
_XfePrimitiveDrawable(Widget w)
{
    Drawable d = None;

    switch(_XfeBufferType(w))
    {
    case XmBUFFER_NONE:
		d = _XfeWindow(w);
		break;

    case XmBUFFER_PRIVATE:
		d = _XfeBufferPixmap(w);
		break;

    case XmBUFFER_SHARED:
		d = _XfePixmapBufferAccess();
		break;
    }

    return d;
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveDrawEverything(Widget w,XEvent * event,Region region)
{
    static XRectangle rect;

    /* Clear the buffer background if needed */
    if (_XfeBufferType(w) != XmBUFFER_NONE)
    {
		_XfePrimitiveClearBackground(w);
    }

    /* Draw Background */ 
    XfeRectSet(&rect,
			   _XfeRectX(w) - _XfeMarginLeft(w),
			   _XfeRectY(w) - _XfeMarginTop(w),
			   _XfeRectWidth(w) + _XfeMarginLeft(w) + _XfeMarginRight(w),
			   _XfeRectHeight(w) + _XfeMarginTop(w) + _XfeMarginBottom(w));
    
    _XfePrimitiveDrawBackground(w,event,region,&rect);
    
    /* Draw -or Erase the Widget's highlight if needed */
    if (_XfeHighlighted(w) && _XfeHighlightThickness(w))
    {
		_XfePrimitiveBorderHighlight(w);
    }
    else
    {
		_XfePrimitiveBorderUnhighlight(w);
    }
    
    /* Draw Shadow */ 
    _XfePrimitiveDrawShadow(w,event,region,&_XfeWidgetRect(w));
    
    /* Draw Components */ 
    _XfePrimitiveDrawComponents(w,event,region,&_XfeWidgetRect(w));
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveClearBackground(Widget w)
{
    /* Clear the widget background including margins */
    if (_XfeBufferType(w) == XmBUFFER_NONE)
    {
		XClearArea(XtDisplay(w),_XfeWindow(w),
				   _XfeHighlightThickness(w),_XfeHighlightThickness(w),
				   _XfeWidth(w) - 2 * _XfeHighlightThickness(w),
				   _XfeHeight(w) - 2 * _XfeHighlightThickness(w),
				   False);

		/*XClearWindow(XtDisplay(w),_XfeWindow(w));*/
    }
    else
    {
		XFillRectangle(XtDisplay(w),
					   _XfePrimitiveDrawable(w),
					   _XfeBackgroundGC(w),
					   0,0,
					   _XfeWidth(w),_XfeHeight(w));
    }
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveAllocateBackgroundGC(Widget w)
{
	/* Make sure the background gc gets allocated only once */
	if (_XfeBackgroundGC(w))
	{
		return;
	}

	if (_XfePixmapGood(_XfeBackgroundPixmap(w)))
	{
		_XfeBackgroundGC(w) = XfeAllocateTileGc(w,_XfeBackgroundPixmap(w));
	}
	else
	{
		_XfeBackgroundGC(w) = XfeAllocateColorGc(w,_XfeBackgroundPixel(w),
												 None,True);
	}
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveReleaseBackgroundGC(Widget w)
{
	/* Make sure the gc has been allocated */
	if (!_XfeBackgroundGC(w))
	{
		return;
	}

    /* Free the background gc */
    XtReleaseGC(w,_XfeBackgroundGC(w));
}
/*----------------------------------------------------------------------*/
void
_XfePrimitiveDrawBuffer(Widget w,XEvent * event,Region region)
{
    XCopyArea(XtDisplay(w),
			  _XfePrimitiveDrawable(w),
			  _XfeWindow(w),
			  _XfeBackgroundGC(w),
			  _XfeHighlightThickness(w),_XfeHighlightThickness(w),
			  _XfeWidth(w) - 2 * _XfeHighlightThickness(w),
			  _XfeHeight(w) - 2 * _XfeHighlightThickness(w),
			  _XfeHighlightThickness(w),_XfeHighlightThickness(w));
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Action Procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfePrimitiveEnter(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	_XfePointerInside(w) = True;

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	/* Call the XmPrimitive Enter() action */
    _XmPrimitiveEnter(w,event,params,nparams);

	/* Define the cursor if needed */
	if (_XfeCursorGood(_XfeCursor(w)))
	{
		XfeCursorDefine(w,_XfeCursor(w));
	}

	/* Call enter callbacks */
	_XfeInvokeCallbacks(w,_XfeEnterCallbacks(w),XmCR_ENTER,event,False);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfePrimitiveLeave(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	/* Make sure the pointer is indeed inside for this action */
	if (!_XfePointerInside(w))
	{
		return;
	}
	
	_XfePointerInside(w) = False;

	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	/* Call the XmPrimitive Leave() action */
    _XmPrimitiveLeave(w,event,params,nparams);

	/* Undefine the cursor if needed */
	if (_XfeCursorGood(_XfeCursor(w)))
	{
		XfeCursorUndefine(w);
	}

	/* Call leave callbacks */
	_XfeInvokeCallbacks(w,_XfeLeaveCallbacks(w),XmCR_LEAVE,event,False);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfePrimitiveFocus(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
	/* Make sure we are not pretending to be insensitive */
	if (!_XfeIsSensitive(w))
	{
		return;
	}

	XmProcessTraversal(w,XmTRAVERSE_CURRENT);

	/* Invoke Focus Callbacks */
	_XfeInvokeCallbacks(w,_XfeFocusCallbacks(w),XmCR_FOCUS,event,False);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Public pretend sensitive methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeSetPretendSensitive(Widget w,Boolean state)
{
	Arg xargs[1];

	assert( _XfeIsAlive(w) );
	assert( XfeIsPrimitive(w) );

	if (!_XfeIsAlive(w))
	{
		return;
	}

	/* Make sure the state is different */
	if (_XfePretendSensitive(w) == state)
	{
		return;
	}

	XtSetArg(xargs[0],XmNpretendSensitive,state);

	XtSetValues(w,xargs,1);
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeIsPretendSensitive(Widget w)
{
	assert( XfeIsPrimitive(w) );

	if (!_XfeIsAlive(w))
	{
		return False;
	}

	return _XfePretendSensitive(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeIsSensitive(Widget w)
{
	if (!_XfeIsAlive(w))
	{
		return False;
	}

	if (XfeIsPrimitive(w))
	{
		return _XfeIsSensitive(w);
	}

	return _XfeSensitive(w);
}
/*----------------------------------------------------------------------*/

