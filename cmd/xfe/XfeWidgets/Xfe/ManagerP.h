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
/* Name:		<Xfe/ManagerP.h>										*/
/* Description:	XfeManager widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#ifndef _XfeManagerP_h_							/* start ManagerP.h		*/
#define _XfeManagerP_h_

#include <Xfe/XfeP.h>
#include <Xfe/PrimitiveP.h>
#include <Xfe/Manager.h>
#include <Xfe/PrepareP.h>
#include <Xm/ManagerP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XfeBitGravityType	bit_gravity;			/* bit_gravity			*/
	XfeGeometryProc		preferred_geometry;		/* preferred_geometry	*/
	XfeGeometryProc		minimum_geometry;		/* minimum_geometry		*/
	XtWidgetProc		update_rect;			/* update_rect			*/
	XfeChildFunc		accept_child;			/* accept_child			*/
	XfeChildFunc		insert_child;			/* insert_child			*/
	XfeChildFunc		delete_child;			/* delete_child			*/
	XtWidgetProc		change_managed;			/* change_managed	    */
	XfePrepareProc		prepare_components;		/* prepare_components	*/
	XtWidgetProc		layout_components;		/* layout_components	*/
	XtWidgetProc		layout_children;		/* layout_children	    */
	XfeExposeProc		draw_background;		/* draw_background		*/
	XfeExposeProc		draw_shadow;			/* draw_shadow		    */
	XfeExposeProc		draw_components;		/* draw_components		*/
	XtPointer			extension;				/* extension			*/

} XfeManagerClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeManagerClassRec
{
	CoreClassPart			core_class;
	CompositeClassPart		composite_class;
	ConstraintClassPart		constraint_class;
	XmManagerClassPart		manager_class;
	XfeManagerClassPart		xfe_manager_class;
} XfeManagerClassRec;

externalref XfeManagerClassRec xfeManagerClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeManagerPart
{
	/* Callback Resources */
	XtCallbackList		resize_callback;		/* Widget Resize		*/
	XtCallbackList		change_managed_callback;/* Widget Resize		*/
	XtCallbackList		layout_callback;		/* Widget Layout Request*/
	
	/* Busy resources */
	Boolean				busy;					/* Busy  ?				*/
	Cursor				busy_cursor;			/* Busy Cursor			*/
	Boolean				busy_cursor_on;			/* Busy Cursor On ?		*/

	/* Misc resources */
	XtEnum				shadow_type;			/* Shadow Type			*/
	Boolean				ignore_configure;		/* Ignore Configure		*/

	/* Geometry resources */
	Boolean				use_preferred_width;	/* use preferred width	*/
	Boolean				use_preferred_height;	/* use preferred height	*/

	/* Margin resources */
	Dimension			margin_left;			/* Margin Left			*/
	Dimension			margin_right;			/* Margin Right			*/
	Dimension			margin_top;				/* Margin Top			*/
	Dimension			margin_bottom;			/* Margin Bottom		*/

	Dimension			min_width;				/* Min width			*/
	Dimension			min_height;				/* Min height			*/

	/* Read Only Resources */
	Dimension			preferred_width;		/* Preferred Width		*/
	Dimension			preferred_height;		/* Preferred Height		*/

	/* For c++ usage */
	XtPointer			instance_pointer;		/* Instance pointer		*/
	
	/* Private Data Members */
	int					config_flags;			/* Require Geometry		*/
	int					prepare_flags;			/* Require Geometry		*/
	Boolean				component_flag;			/* Components Layout ?	*/

	XRectangle			widget_rect;			/* Widget Rect			*/
	XfeDimensionsRec	old_dimensions;			/* Old dimensions		*/
} XfeManagerPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeManagerRec
{
	CorePart			core;
	CompositePart		composite;
	ConstraintPart		constraint;
	XmManagerPart		manager;
	XfeManagerPart		xfe_manager;
} XfeManagerRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerConstraintPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeManagerConstraintPart
{
    int					position_index;			/* Position Index		*/
    Boolean				private_component;		/* Private Component	*/
} XfeManagerConstraintPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerConstraintRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeManagerConstraintRec
{
	XmManagerConstraintPart		manager;
	XfeManagerConstraintPart	xfe_manager;
} XfeManagerConstraintRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager Method invocation functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeManagerChainInitialize		(Widget			rw,
								 Widget			nw,
								 WidgetClass	wc);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeManagerChainSetValues		(Widget			ow,
								 Widget			rw,
								 Widget			nw,
								 WidgetClass	wc);
/*----------------------------------------------------------------------*/
extern void
_XfeConstraintChainInitialize	(Widget			rw,
								 Widget			nw,
								 WidgetClass	wc);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeConstraintChainSetValues	(Widget			ow,
								 Widget			rw,
								 Widget			nw,
								 WidgetClass	wc);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerPreferredGeometry	(Widget			w,
								 Dimension *	width_out,
								 Dimension *	height_out);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerMinimumGeometry		(Widget			w,
								 Dimension *	width_out,
								 Dimension *	height_out);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerUpdateRect			(Widget			w);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeManagerAcceptChild			(Widget			child);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeManagerInsertChild			(Widget			child);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeManagerDeleteChild			(Widget			child);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerChangeManaged		(Widget			child);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerPrepareComponents	(Widget			w,
								 int			flags);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerLayoutComponents		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerLayoutChildren		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerDrawBackground		(Widget			w,
								 XEvent *		event,
								 Region			region,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerDrawComponents		(Widget			w,
								 XEvent *		event,
								 Region			region,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerDrawShadow			(Widget			w,
								 XEvent *		event,
								 Region			region,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager private functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeManagerChildrenInfo			(Widget			w,
								 Dimension *	max_width_out,
								 Dimension *	max_height_out,
								 Dimension *	total_width_out,
								 Dimension *	total_height_out,
								 Cardinal *		num_managed_out,
								 Cardinal *		num_components_out);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerComponentInfo		(Widget			w,
								 Dimension *	max_width_out,
								 Dimension *	max_height_out);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerWidgetClass bit_gravity access macro						*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeManagerAccessBitGravity(w) \
(((XfeManagerWidgetClass) XtClass(w))->xfe_manager_class.bit_gravity)

/*----------------------------------------------------------------------*/
/*																		*/
/* Xt Composite member access											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemChildren(w) \
(((CompositeWidget) (w))->composite . children)
/*----------------------------------------------------------------------*/
#define _XfemNumChildren(w) \
(((CompositeWidget) (w))->composite . num_children)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Xm Manager member access												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemAcceleratorWidget(w) \
(((XmManagerWidget) (w))->manager . accelerator_widget)
/*----------------------------------------------------------------------*/
#define _XfemActiveChild(w) \
(((XmManagerWidget) (w))->manager . active_child)
/*----------------------------------------------------------------------*/
#define _XfemBackgroundGC(w) \
(((XmManagerWidget) (w))->manager . background_GC)
/*----------------------------------------------------------------------*/
#define _XfemBottomShadowGC(w) \
(((XmManagerWidget) (w))->manager . bottom_shadow_GC)
/*----------------------------------------------------------------------*/
#define _XfemBottomShadowColor(w) \
(((XmManagerWidget) (w))->manager . bottom_shadow_color)
/*----------------------------------------------------------------------*/
#define _XfemBottomShadowPixmap(w) \
(((XmManagerWidget) (w))->manager . bottom_shadow_pixmap)
/*----------------------------------------------------------------------*/
#define _XfemEligibleForMultiButtonEvent(w) \
(((XmManagerWidget) (w))->manager . eligible_for_multi_button_event)
/*----------------------------------------------------------------------*/
#define _XfemEventHandlerAdded(w) \
(((XmManagerWidget) (w))->manager . event_handler_added)
/*----------------------------------------------------------------------*/
#define _XfemForeground(w) \
(((XmManagerWidget) (w))->manager . foreground)
/*----------------------------------------------------------------------*/
#define _XfemHasFocus(w) \
(((XmManagerWidget) (w))->manager . has_focus)
/*----------------------------------------------------------------------*/
#define _XfemHelpCallback(w) \
(((XmManagerWidget) (w))->manager . help_callback)
/*----------------------------------------------------------------------*/
#define _XfemHighlightGC(w) \
(((XmManagerWidget) (w))->manager . highlight_GC)
/*----------------------------------------------------------------------*/
#define _XfemHighlightColor(w) \
(((XmManagerWidget) (w))->manager . highlight_color)
/*----------------------------------------------------------------------*/
#define _XfemHighlightPixmap(w) \
(((XmManagerWidget) (w))->manager . highlight_pixmap)
/*----------------------------------------------------------------------*/
#define _XfemHighlightedWidget(w) \
(((XmManagerWidget) (w))->manager . highlighted_widget)
/*----------------------------------------------------------------------*/
#define _XfemInitialFocus(w) \
(((XmManagerWidget) (w))->manager . initial_focus)
/*----------------------------------------------------------------------*/
#define _XfemKeyboardList(w) \
(((XmManagerWidget) (w))->manager . keyboard_list)
/*----------------------------------------------------------------------*/
#define _XfemNavigationType(w) \
(((XmManagerWidget) (w))->manager . navigation_type)
/*----------------------------------------------------------------------*/
#define _XfemNumKeyboardEntries(w) \
(((XmManagerWidget) (w))->manager . num_keyboard_entries)
/*----------------------------------------------------------------------*/
#define _XfemPopupHandlerCallback(w) \
(((XmManagerWidget) (w))->manager . popup_handler_callback)
/*----------------------------------------------------------------------*/
#define _XfemSelectedGadget(w) \
(((XmManagerWidget) (w))->manager . selected_gadget)
/*----------------------------------------------------------------------*/
#define _XfemShadowThickness(w) \
(((XmManagerWidget) (w))->manager . shadow_thickness)
/*----------------------------------------------------------------------*/
#define _XfemSizeKeyboardList(w) \
(((XmManagerWidget) (w))->manager . size_keyboard_list)
/*----------------------------------------------------------------------*/
#define _XfemStringDirection(w) \
(((XmManagerWidget) (w))->manager . string_direction)
/*----------------------------------------------------------------------*/
#define _XfemTopShadowGC(w) \
(((XmManagerWidget) (w))->manager . top_shadow_GC)
/*----------------------------------------------------------------------*/
#define _XfemTopShadowColor(w) \
(((XmManagerWidget) (w))->manager . top_shadow_color)
/*----------------------------------------------------------------------*/
#define _XfemTopShadowPixmap(w) \
(((XmManagerWidget) (w))->manager . top_shadow_pixmap)
/*----------------------------------------------------------------------*/
#define _XfemTraversalOn(w) \
(((XmManagerWidget) (w))->manager . traversal_on)
/*----------------------------------------------------------------------*/
#define _XfemUnitType(w) \
(((XmManagerWidget) (w))->manager . unit_type)
/*----------------------------------------------------------------------*/
#define _XfemUserData(w) \
(((XmManagerWidget) (w))->manager . user_data)
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager member access												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemChangeManagedCallback(w) \
(((XfeManagerWidget) (w))->xfe_manager . change_managed_callback)
/*----------------------------------------------------------------------*/
#define _XfemResizeCallback(w) \
(((XfeManagerWidget) (w))->xfe_manager . resize_callback)
/*----------------------------------------------------------------------*/
#define _XfemLayoutCallback(w) \
(((XfeManagerWidget) (w))->xfe_manager . layout_callback)
/*----------------------------------------------------------------------*/
#define _XfemBusyCursor(w) \
(((XfeManagerWidget) (w))->xfe_manager . busy_cursor)
/*----------------------------------------------------------------------*/
#define _XfemBusy(w) \
(((XfeManagerWidget) (w))->xfe_manager . busy)
/*----------------------------------------------------------------------*/
#define _XfemBusyCursorOn(w) \
(((XfeManagerWidget) (w))->xfe_manager . busy_cursor_on)
/*----------------------------------------------------------------------*/
#define _XfemShadowType(w) \
(((XfeManagerWidget) (w))->xfe_manager . shadow_type)
/*----------------------------------------------------------------------*/
#define _XfemMarginLeft(w) \
(((XfeManagerWidget) (w))->xfe_manager . margin_left)
/*----------------------------------------------------------------------*/
#define _XfemMarginRight(w) \
(((XfeManagerWidget) (w))->xfe_manager . margin_right)
/*----------------------------------------------------------------------*/
#define _XfemMarginTop(w) \
(((XfeManagerWidget) (w))->xfe_manager . margin_top)
/*----------------------------------------------------------------------*/
#define _XfemMarginBottom(w) \
(((XfeManagerWidget) (w))->xfe_manager . margin_bottom)
/*----------------------------------------------------------------------*/
#define _XfemOrderFunction(w) \
(((XfeManagerWidget) (w))->xfe_manager . order_function)
/*----------------------------------------------------------------------*/
#define _XfemOrderPolicy(w) \
(((XfeManagerWidget) (w))->xfe_manager . order_policy)
/*----------------------------------------------------------------------*/
#define _XfemOrderData(w) \
(((XfeManagerWidget) (w))->xfe_manager . order_data)
/*----------------------------------------------------------------------*/
#define _XfemIgnoreConfigure(w) \
(((XfeManagerWidget) (w))->xfe_manager . ignore_configure)
/*----------------------------------------------------------------------*/
#define _XfemPreferredWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . preferred_width)
/*----------------------------------------------------------------------*/
#define _XfemPreferredHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . preferred_height)
/*----------------------------------------------------------------------*/
#define _XfemMinWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . min_width)
/*----------------------------------------------------------------------*/
#define _XfemMinHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . min_height)
/*----------------------------------------------------------------------*/
#define _XfemUsePreferredWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . use_preferred_width)
/*----------------------------------------------------------------------*/
#define _XfemUsePreferredHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . use_preferred_height)
/*----------------------------------------------------------------------*/
#define _XfemConfigFlags(w) \
(((XfeManagerWidget) (w))->xfe_manager . config_flags)
/*----------------------------------------------------------------------*/
#define _XfemPrepareFlags(w) \
(((XfeManagerWidget) (w))->xfe_manager . prepare_flags)
/*----------------------------------------------------------------------*/
#define _XfemComponentFlag(w) \
(((XfeManagerWidget) (w))->xfe_manager . component_flag)
/*----------------------------------------------------------------------*/
#define _XfemWidgetRect(w) \
(((XfeManagerWidget) (w))->xfe_manager . widget_rect)
/*----------------------------------------------------------------------*/
#define _XfemInstancePointer(w) \
(((XfeManagerWidget) (w))->xfe_manager . instance_pointer)
/*----------------------------------------------------------------------*/
#define _XfemOldWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . old_dimensions . width)
/*----------------------------------------------------------------------*/
#define _XfemOldHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . old_dimensions . height)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager misc access macros										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemOffsetLeft(w)		(_XfemShadowThickness(w) +	\
								 _XfemMarginLeft(w))
/*----------------------------------------------------------------------*/
#define _XfemOffsetRight(w)		(_XfemShadowThickness(w) +	\
								 _XfemMarginRight(w))
/*----------------------------------------------------------------------*/
#define _XfemOffsetTop(w)		(_XfemShadowThickness(w) +	\
								 _XfemMarginTop(w))
/*----------------------------------------------------------------------*/
#define _XfemOffsetBottom(w)		(_XfemShadowThickness(w) +	\
									 _XfemMarginBottom(w))
/*----------------------------------------------------------------------*/
#define _XfemRectHeight(w)		(_XfemWidgetRect(w) . height)
/*----------------------------------------------------------------------*/
#define _XfemRectWidth(w)		(_XfemWidgetRect(w) . width)
/*----------------------------------------------------------------------*/
#define _XfemRectX(w)			(_XfemWidgetRect(w) . x)
/*----------------------------------------------------------------------*/
#define _XfemRectY(w)			(_XfemWidgetRect(w) . y)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Children indexing macro - Assuming w is Composite					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeChildrenIndex(w,n)							\
(((n < _XfemNumChildren(w)) && _XfemChildren(w))		\
? _XfemChildren(w)[n] : NULL)

/*----------------------------------------------------------------------*/
/*																		*/
/* Child is Shown - Both alive and managed								*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeChildIsShown(w) (_XfeIsAlive(w) && _XfeIsManaged(w))

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager child constraint part access macro						*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeManagerConstraintPart(w) \
(&(((XfeManagerConstraintRec *) _XfeConstraints(w)) -> xfe_manager))


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager child individual constraint resource access macro			*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeManagerPrivateComponent(w) \
(_XfeManagerConstraintPart(w)) -> private_component
/*----------------------------------------------------------------------*/
#define _XfeManagerPositionIndex(w) \
(_XfeManagerConstraintPart(w)) -> position_index
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager default dimensions										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeMANAGER_DEFAULT_WIDTH	2
#define XfeMANAGER_DEFAULT_HEIGHT	2
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ManagerP.h		*/
