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
#include <Xfe/Linked.h>
#include <Xm/ManagerP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager method inheritance macros									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeInheritUpdateChildrenInfo		((XtWidgetProc)			_XtInherit)
#define XfeInheritLayoutWidget				((XtWidgetProc)			_XtInherit)
#define XfeInheritAcceptStaticChild			((XfeChildFunc)			_XtInherit)
#define XfeInheritInsertStaticChild			((XfeChildFunc)			_XtInherit)
#define XfeInheritDeleteStaticChild			((XfeChildFunc)			_XtInherit)
#define XfeInheritLayoutStaticChildren		((XtWidgetProc)			_XtInherit)
#define XfeInheritChangeManaged				((XtWidgetProc)			_XtInherit)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	/* Bit gravity */
	XfeBitGravityType	bit_gravity;			/* bit_gravity				*/

	/* Geometry methods */
	XfeGeometryProc		preferred_geometry;		/* preferred_geometry		*/
	XtWidgetProc		update_boundary;		/* update_boundary			*/
	XtWidgetProc		update_children_info;	/* update_children_info		*/
	XtWidgetProc		layout_widget;			/* layout_widget			*/
	
	/* Static children methods */
	XfeChildFunc		accept_static_child;	/* accept_static_child		*/
	XfeChildFunc		insert_static_child;	/* insert_static_child		*/
	XfeChildFunc		delete_static_child;	/* delete_static_child		*/
	XtWidgetProc		layout_static_children;	/* layout_static_children   */

	/* Change managed method */
	XtWidgetProc		change_managed;			/* change_managed			*/

	/* Component methods */
	XfePrepareProc		prepare_components;		/* prepare_components		*/
	XtWidgetProc		layout_components;		/* layout_components		*/

	/* Rendering methods */
	XfeExposeProc		draw_background;		/* draw_background			*/
	XfeExposeProc		draw_shadow;			/* draw_shadow				*/
	XfeExposeProc		draw_components;		/* draw_components			*/
	XfeExposeProc		draw_accent_border;		/* draw_accent_border		*/

	XtPointer			extension;				/* extension				*/

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
	XtCallbackList		change_managed_callback;/* Change managed callback	*/
	XtCallbackList		resize_callback;		/* Resize callback			*/
	XtCallbackList		realize_callback;		/* Realize callback			*/
	
	/* Busy resources */
	Boolean				busy;					/* Busy  ?				*/
	Cursor				busy_cursor;			/* Busy Cursor			*/
	Boolean				busy_cursor_on;			/* Busy Cursor On ?		*/

	/* Shadow resources */
	XtEnum				shadow_type;			/* Shadow Type			*/

	/* Layout resources */
	Boolean				layout_frozen;		/* Layout frozen ?		*/

	/* Geometry resources */
	Dimension			preferred_width;		/* Preferred Width		*/
	Dimension			preferred_height;		/* Preferred Height		*/
	Boolean				use_preferred_width;	/* use preferred width	*/
	Boolean				use_preferred_height;	/* use preferred height	*/

	Dimension			min_width;				/* Min width			*/
	Dimension			min_height;				/* Min height			*/

	/* Accent border resources */
	Dimension			accent_border_thickness;/* Accent border thickness*/

	/* Margin resources */
	Dimension			margin_left;			/* Margin Left			*/
	Dimension			margin_right;			/* Margin Right			*/
	Dimension			margin_top;				/* Margin Top			*/
	Dimension			margin_bottom;			/* Margin Bottom		*/

	/* For c++ usage */
	XtPointer			instance_pointer;		/* Instance pointer		*/

	/* Component children resources */
	XfeLinked			component_children;		/* Component children	*/
	Cardinal			num_component_children;	/* Num component children*/
	Dimension			max_component_width;	/* Max component width	*/
	Dimension			max_component_height;	/* Max component height	*/
	Dimension			total_component_width;	/* Total component width*/
	Dimension			total_component_height;	/* Total component height*/
	
	/* Static children resources */
	XfeLinked			static_children;		/* Static children		*/
	Cardinal			num_static_children;	/* Num Static children	*/
	Dimension			max_static_width;		/* Max static width		*/
	Dimension			max_static_height;		/* Max static height	*/
	Dimension			total_static_width;		/* Total static width	*/
	Dimension			total_static_height;	/* Total static height	*/

	/* Debug resources */
#ifdef DEBUG
	Boolean				debug_trace;			/* Trace / debug		*/
#endif

	/* Private Data Members */
	int					config_flags;			/* Require Geometry		*/
	int					prepare_flags;			/* Require Geometry		*/
	Boolean				component_flag;			/* Components Layout ?	*/

	/* The widget's boundary */
	XRectangle			boundary;				/* Boundary				*/

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
	unsigned char		manager_child_type;		/* Manager Child type	*/
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
_XfeManagerPreferredGeometry		(Widget			w,
									 Dimension *	width_out,
									 Dimension *	height_out);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerUpdateBoundary			(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerUpdateChildrenInfo		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerLayoutWidget				(Widget			w);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeManagerInsertStaticChild		(Widget			child);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeManagerAcceptStaticChild		(Widget			child);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeManagerDeleteStaticChild		(Widget			child);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerLayoutStaticChildren		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerChangeManaged			(Widget			child);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerPrepareComponents		(Widget			w,
									 int			flags);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerLayoutComponents			(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerDrawBackground			(Widget			w,
									 XEvent *		event,
									 Region			region,
									 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerDrawShadow				(Widget			w,
									 XEvent *		event,
									 Region			region,
									 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerDrawComponents			(Widget			w,
									 XEvent *		event,
									 Region			region,
									 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerDrawAccentBorder			(Widget			w,
									 XEvent *		event,
									 Region			region,
									 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager constraint chain functions								*/
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

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager private functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeManagerPropagateSetValues		(Widget			ow,
									 Widget			nw,
									 Boolean		propagate_sensitive);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager children info functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeManagerGetChildrenInfo				(Widget				w,
										 XfeLinked			children,
										 int				mask,
										 XfeGeometryProc	proc,
										 Dimension *		max_width_out,
										 Dimension *		max_height_out,
										 Dimension *		min_width_out,
										 Dimension *		min_height_out,
										 Dimension *		total_width_out,
										 Dimension *		total_height_out,
										 Cardinal *			num_managed_out);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerUpdateComponentChildrenInfo	(Widget				w);
/*----------------------------------------------------------------------*/
extern void
_XfeManagerUpdateStaticChildrenInfo		(Widget				w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager private children apply functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeManagerApplyProcToChildren			(Widget					w,
										 XfeLinked				children,
										 int					mask,
										 XfeManagerApplyProc	proc,
										 XtPointer				data);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManagerWidgetClass bit_gravity access macro						*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeManagerAccessBitGravity(w) \
(((XfeManagerWidgetClass) XtClass(w))->xfe_manager_class . bit_gravity)

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
#define _XfemRealizeCallback(w) \
(((XfeManagerWidget) (w))->xfe_manager . realize_callback)
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
#define _XfemLayoutFrozen(w) \
(((XfeManagerWidget) (w))->xfe_manager . layout_frozen)
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
#define _XfemInstancePointer(w) \
(((XfeManagerWidget) (w))->xfe_manager . instance_pointer)
/*----------------------------------------------------------------------*/
#define _XfemOldWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . old_dimensions . width)
/*----------------------------------------------------------------------*/
#define _XfemOldHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . old_dimensions . height)
/*----------------------------------------------------------------------*/
#define _XfemComponentChildren(w) \
(((XfeManagerWidget) (w))->xfe_manager . component_children)
/*----------------------------------------------------------------------*/
#define _XfemNumComponentChildren(w) \
(((XfeManagerWidget) (w))->xfe_manager . num_component_children)
/*----------------------------------------------------------------------*/
#define _XfemMaxComponentWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . max_component_width)
/*----------------------------------------------------------------------*/
#define _XfemMaxComponentHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . max_component_height)
/*----------------------------------------------------------------------*/
#define _XfemTotalComponentWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . total_component_width)
/*----------------------------------------------------------------------*/
#define _XfemTotalComponentHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . total_component_height)
/*----------------------------------------------------------------------*/
#define _XfemStaticChildren(w) \
(((XfeManagerWidget) (w))->xfe_manager . static_children)
/*----------------------------------------------------------------------*/
#define _XfemNumStaticChildren(w) \
(((XfeManagerWidget) (w))->xfe_manager . num_static_children)
/*----------------------------------------------------------------------*/
#define _XfemMaxStaticWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . max_static_width)
/*----------------------------------------------------------------------*/
#define _XfemMaxStaticHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . max_static_height)
/*----------------------------------------------------------------------*/
#define _XfemTotalStaticWidth(w) \
(((XfeManagerWidget) (w))->xfe_manager . total_static_width)
/*----------------------------------------------------------------------*/
#define _XfemTotalStaticHeight(w) \
(((XfeManagerWidget) (w))->xfe_manager . total_static_height)
/*----------------------------------------------------------------------*/
#define _XfemAccentBorderThickness(w) \
(((XfeManagerWidget) (w))->xfe_manager . accent_border_thickness)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Component & Static children count									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemComponentChildrenCount(w) \
(_XfemComponentChildren(w) ? XfeLinkedCount(_XfemComponentChildren(w)) : 0)
/*----------------------------------------------------------------------*/
#define _XfemStaticChildrenCount(w) \
(_XfemStaticChildren(w) ? XfeLinkedCount(_XfemStaticChildren(w)) : 0)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Component children indexing macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemComponentChildrenIndex(w,i) \
(_XfemComponentChildren(w) ? XfeLinkedItemAtIndex(_XfemComponentChildren(w),i) : NULL)

/*----------------------------------------------------------------------*/
/*																		*/
/* Static children indexing macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemStaticChildrenIndex(w,i) \
(_XfemStaticChildren(w) ? XfeLinkedItemAtIndex(_XfemStaticChildren(w),i) : NULL)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager boundary access macros									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemBoundary(w) (((XfeManagerWidget) (w))->xfe_manager . boundary)

#define _XfemBoundaryHeight(w)		(_XfemBoundary(w) . height)
#define _XfemBoundaryWidth(w)		(_XfemBoundary(w) . width)
#define _XfemBoundaryX(w)			(_XfemBoundary(w) . x)
#define _XfemBoundaryY(w)			(_XfemBoundary(w) . y)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Access to debug resources											*/
/*																		*/
/*----------------------------------------------------------------------*/
#ifdef DEBUG
#define _XfemDebugTrace(w) \
(((XfeManagerWidget) (w))->xfe_manager . debug_trace)
/*----------------------------------------------------------------------*/
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager misc access macros										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemOffsetLeft(w)		(_XfemShadowThickness(w) +	\
								 _XfemMarginLeft(w) + \
								 _XfemAccentBorderThickness(w))
/*----------------------------------------------------------------------*/
#define _XfemOffsetRight(w)		(_XfemShadowThickness(w) +	\
								 _XfemMarginRight(w) + \
								 _XfemAccentBorderThickness(w))
/*----------------------------------------------------------------------*/
#define _XfemOffsetTop(w)		(_XfemShadowThickness(w) +	\
								 _XfemMarginTop(w) + \
								 _XfemAccentBorderThickness(w))
/*----------------------------------------------------------------------*/
#define _XfemOffsetBottom(w)	(_XfemShadowThickness(w) +	\
								 _XfemMarginBottom(w) + \
								 _XfemAccentBorderThickness(w))
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
#define _XfeConstraintManagerChildType(child) \
(_XfeManagerConstraintPart(child)) -> manager_child_type
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager default dimensions										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeMANAGER_DEFAULT_WIDTH	2
#define XfeMANAGER_DEFAULT_HEIGHT	2
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ManagerP.h		*/
