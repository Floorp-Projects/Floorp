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
/* Name:		<Xfe/PrimitiveP.h>										*/
/* Description:	XfePrimitive widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfePrimitiveP_h_						/* start PrimitiveP.h	*/
#define _XfePrimitiveP_h_

#include <Xfe/XfeP.h>
#include <Xfe/Primitive.h>
#include <Xfe/PrepareP.h>
#include <Xm/PrimitiveP.h>
#include <Xm/DrawP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
    
/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitiveClassPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    XfeBitGravityType	bit_gravity;			/* bit_gravity			*/
    XfeGeometryProc		preferred_geometry;		/* preferred_geometry	*/
    XfeGeometryProc		minimum_geometry;		/* minimum_geometry		*/
    XtWidgetProc		update_rect;			/* update_rect			*/
    XfePrepareProc		prepare_components;		/* prepare_components	*/
    XtWidgetProc		layout_components;		/* layout_components	*/
    XfeExposeProc		draw_background;		/* draw_background		*/
    XfeExposeProc		draw_shadow;			/* draw_shadow			*/
    XfeExposeProc		draw_components;		/* draw_components		*/
    XtPointer			extension;				/* extension			*/
} XfePrimitiveClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitiveClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrimitiveClassRec
{
    CoreClassPart			core_class;
    XmPrimitiveClassPart	primitive_class;
    XfePrimitiveClassPart	xfe_primitive_class;
} XfePrimitiveClassRec;

externalref XfePrimitiveClassRec xfePrimitiveClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitivePart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrimitivePart
{
    /* Callback Resources */
    XtCallbackList		enter_callback;			/* Pointer Enter		*/
    XtCallbackList		focus_callback;			/* Keyboard Focus		*/
    XtCallbackList		leave_callback;			/* Pointer Leave		*/
    XtCallbackList		resize_callback;		/* Widget Resize		*/
    
	/* Cursor resources */
    Cursor				cursor;					/* Cursor				*/

	/* Appearance resources */
    unsigned char		shadow_type;			/* Shadow Type			*/
    unsigned char		buffer_type;			/* Buffer Type			*/
	Boolean				pretend_sensitive;		/* Pretend Sensitive	*/
    
	/* Geometry resources */
    Boolean				use_preferred_width;	/* Use preferred width	*/
    Boolean				use_preferred_height;	/* Use preferred height	*/
    Dimension			margin_left;			/* Margin Left			*/
    Dimension			margin_right;			/* Margin Right			*/
    Dimension			margin_top;				/* Margin Top			*/
    Dimension			margin_bottom;			/* Margin Bottom		*/

	/* For c++ usage */
	XtPointer			instance_pointer;		/* Instance pointer		*/

    /* Read Only Resources */
    Boolean				pointer_inside;			/* Pointer in Window	*/
    Dimension			preferred_width;		/* Preferred Width		*/
    Dimension			preferred_height;		/* Preferred Height		*/
    
    /* Private Data Members */
    int					config_flags;			/* Config Flags			*/
    int					prepare_flags;			/* Prepare Flags		*/
    XRectangle			widget_rect;			/* Widget Rect			*/
    GC					background_GC;			/* Background GC		*/
    Pixmap				buffer_pixmap;			/* Buffer pixmap		*/
} XfePrimitivePart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitiveRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrimitiveRec
{
    CorePart			core;					/* Core Part			*/
    XmPrimitivePart		primitive;				/* XmPrimitive Part		*/
    XfePrimitivePart	xfe_primitive;			/* XfePrimitive Part	*/
} XfePrimitiveRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XmPrimitive Method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveBorderHighlight		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveBorderUnhighlight		(Widget			w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveChainInitialize		(Widget			rw,
									 Widget			nw,
									 WidgetClass	wc);
/*----------------------------------------------------------------------*/
extern Boolean
_XfePrimitiveChainSetValues			(Widget			ow,
									 Widget			rw,
									 Widget			nw,
									 WidgetClass	wc);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitivePreferredGeometry		(Widget			w,
									 Dimension *	width,
									 Dimension *	height);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveUpdateRect				(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitivePrepareComponents		(Widget			w,
									 int			flags);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveLayoutComponents		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveDrawBackground			(Widget			w,
									 XEvent *		event,
									 Region			region,
									 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveDrawComponents			(Widget			w,
									 XEvent *		event,
									 Region			region,
									 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveDrawShadow				(Widget			w,
									 XEvent *		event,
									 Region			region,
									 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern Drawable
_XfePrimitiveDrawable				(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveDrawEverything			(Widget			w,
									 XEvent *		event,
									 Region			region);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveDrawBuffer				(Widget			w,
									 XEvent *		event,
									 Region			region);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Private XfePrimitive functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveClearBackground		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveAllocateBackgroundGC	(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveReleaseBackgroundGC	(Widget			w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive Action Procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveEnter					(Widget,XEvent *,char **,Cardinal *);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveLeave					(Widget,XEvent *,char **,Cardinal *);
/*----------------------------------------------------------------------*/
extern void
_XfePrimitiveFocus					(Widget,XEvent *,char **,Cardinal *);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitiveWidgetClass bit_gravity access macro						*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePrimitiveAccessBitGravity(w) \
(((XfePrimitiveWidgetClass) XtClass(w))->xfe_primitive_class.bit_gravity)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive resource access macros									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeCursor(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . cursor)
/*----------------------------------------------------------------------*/
#define _XfeEnterCallbacks(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . enter_callback)
/*----------------------------------------------------------------------*/
#define _XfeExposeCallbacks(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . expose_callback)
/*----------------------------------------------------------------------*/
#define _XfeFocusCallbacks(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . focus_callback)
/*----------------------------------------------------------------------*/
#define _XfeLeaveCallbacks(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . leave_callback)
/*----------------------------------------------------------------------*/
#define _XfeResizeCallbacks(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . resize_callback)
/*----------------------------------------------------------------------*/
#define _XfeUsePreferredHeight(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . use_preferred_height)
/*----------------------------------------------------------------------*/
#define _XfeUsePreferredWidth(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . use_preferred_width)
/*----------------------------------------------------------------------*/
#define _XfeShadowType(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . shadow_type)
/*----------------------------------------------------------------------*/
#define _XfeBufferType(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . buffer_type)
/*----------------------------------------------------------------------*/
#define _XfeBufferPixmap(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . buffer_pixmap)
/*----------------------------------------------------------------------*/
#define _XfeMarginLeft(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . margin_left)
/*----------------------------------------------------------------------*/
#define _XfeMarginRight(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . margin_right)
/*----------------------------------------------------------------------*/
#define _XfeMarginTop(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . margin_top)
/*----------------------------------------------------------------------*/
#define _XfeMarginBottom(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . margin_bottom)
/*----------------------------------------------------------------------*/
#define _XfePretendSensitive(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . pretend_sensitive)
/*----------------------------------------------------------------------*/
#define _XfeInstancePointer(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . instance_pointer)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive private member access macros							*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeConfigFlags(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . config_flags)
/*----------------------------------------------------------------------*/
#define _XfePrepareFlags(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . prepare_flags)
/*----------------------------------------------------------------------*/
#define _XfePointerInside(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . pointer_inside)
/*----------------------------------------------------------------------*/
#define _XfeWidgetRect(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . widget_rect)
/*----------------------------------------------------------------------*/
#define _XfeBackgroundGC(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . background_GC)
/*----------------------------------------------------------------------*/
#define _XfePointerInside(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . pointer_inside)
/*----------------------------------------------------------------------*/
#define _XfePreferredHeight(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . preferred_height)
/*----------------------------------------------------------------------*/
#define _XfePreferredWidth(w) \
(((XfePrimitiveWidget) (w))->xfe_primitive . preferred_width)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Xm Primitive member access											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeBottomShadowColor(w) \
(((XmPrimitiveWidget) (w))->primitive . bottom_shadow_color)
/*----------------------------------------------------------------------*/
#define _XfeBottomShadowGC(w) \
(((XmPrimitiveWidget) (w))->primitive . bottom_shadow_GC)
/*----------------------------------------------------------------------*/
#define _XfeBottomShadowPixmap(w) \
(((XmPrimitiveWidget) (w))->primitive . bottom_shadow_pixmap)
/*----------------------------------------------------------------------*/
#define _XfeConvertCallback(w) \
(((XmPrimitiveWidget) (w))->primitive . convert_callback)
/*----------------------------------------------------------------------*/
#define _XfeForeground(w) \
(((XmPrimitiveWidget) (w))->primitive . foreground)
/*----------------------------------------------------------------------*/
#define _XfeHaveTraversal(w) \
(((XmPrimitiveWidget) (w))->primitive . have_traversal)
/*----------------------------------------------------------------------*/
#define _XfeHelpCallback(w) \
(((XmPrimitiveWidget) (w))->primitive . help_callback)
/*----------------------------------------------------------------------*/
#define _XfeHighlightColor(w) \
(((XmPrimitiveWidget) (w))->primitive . highlight_color)
/*----------------------------------------------------------------------*/
#define _XfeHighlightDrawn(w) \
(((XmPrimitiveWidget) (w))->primitive . highlight_drawn)
/*----------------------------------------------------------------------*/
#define _XfeHighlightGC(w) \
(((XmPrimitiveWidget) (w))->primitive . highlight_GC)
/*----------------------------------------------------------------------*/
#define _XfeHighlightOn_Enter(w) \
(((XmPrimitiveWidget) (w))->primitive . highlight_on_enter)
/*----------------------------------------------------------------------*/
#define _XfeHighlightPixmap(w) \
(((XmPrimitiveWidget) (w))->primitive . highlight_pixmap)
/*----------------------------------------------------------------------*/
#define _XfeHighlightThickness(w) \
(((XmPrimitiveWidget) (w))->primitive . highlight_thickness)
/*----------------------------------------------------------------------*/
#define _XfeHighlighted(w) \
(((XmPrimitiveWidget) (w))->primitive . highlighted)
/*----------------------------------------------------------------------*/
#define _XfeLayoutDirection(w) \
(((XmPrimitiveWidget) (w))->primitive . layout_direction)
/*----------------------------------------------------------------------*/
#define _XfeNavigationYype(w) \
(((XmPrimitiveWidget) (w))->primitive . navigation_type)
/*----------------------------------------------------------------------*/
#define _XfePopupHandlerCallback(w) \
(((XmPrimitiveWidget) (w))->primitive . popup_handler_callback)
/*----------------------------------------------------------------------*/
#define _XfeShadowThickness(w) \
(((XmPrimitiveWidget) (w))->primitive . shadow_thickness)
/*----------------------------------------------------------------------*/
#define _XfeTopShadowColor(w) \
(((XmPrimitiveWidget) (w))->primitive . top_shadow_color)
/*----------------------------------------------------------------------*/
#define _XfeTopShadowGC(w) \
(((XmPrimitiveWidget) (w))->primitive . top_shadow_GC)
/*----------------------------------------------------------------------*/
#define _XfeTopShadowPixmap(w) \
(((XmPrimitiveWidget) (w))->primitive . top_shadow_pixmap)
/*----------------------------------------------------------------------*/
#define _XfeTraversalOn(w) \
(((XmPrimitiveWidget) (w))->primitive . traversal_on)
/*----------------------------------------------------------------------*/
#define _XfeUnitType(w) \
(((XmPrimitiveWidget) (w))->primitive . unit_type)
/*----------------------------------------------------------------------*/
#define _XfeUserData(w) \
(((XmPrimitiveWidget) (w))->primitive . user_data)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmPrimitive offset.  highlight_thickness + shadow_thickness			*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePrimitiveOffset(w) \
(_XfeHighlightThickness(w) + _XfeShadowThickness(w))

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive misc access macros										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeOffsetLeft(w)		(_XfePrimitiveOffset(w) + _XfeMarginLeft(w))
#define _XfeOffsetRight(w)		(_XfePrimitiveOffset(w) + _XfeMarginRight(w))
#define _XfeOffsetTop(w)		(_XfePrimitiveOffset(w) + _XfeMarginTop(w))
#define _XfeOffsetBottom(w)		(_XfePrimitiveOffset(w) + _XfeMarginBottom(w))
#define _XfeRectHeight(w)		(_XfeWidgetRect(w) . height)
#define _XfeRectWidth(w)		(_XfeWidgetRect(w) . width)
#define _XfeRectX(w)			(_XfeWidgetRect(w) . x)
#define _XfeRectY(w)			(_XfeWidgetRect(w) . y)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive sensitivity check										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeIsSensitive(w) (_XfePretendSensitive(w) && _XfeSensitive(w))
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive default dimensions										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfePRIMITIVE_DEFAULT_WIDTH	2
#define XfePRIMITIVE_DEFAULT_HEIGHT	2
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end PrimitiveP.h		*/
