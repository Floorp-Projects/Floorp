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
/* Name:		<Xfe/DynamicManagerP.h>									*/
/* Description:	XfeDynamicManager widget private header file.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeDynamicManagerP_h_				/* start DynamicManagerP.h	*/
#define _XfeDynamicManagerP_h_

#include <Xfe/DynamicManager.h>
#include <Xfe/ManagerP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManagerClassPart											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	/* Dynamic children methods */
	XfeChildFunc		accept_dynamic_child;	/* accept_dynamic_child		*/
	XfeChildFunc		insert_dynamic_child;	/* insert_dynamic_child		*/
	XfeChildFunc		delete_dynamic_child;	/* delete_dynamic_child		*/
	XtWidgetProc		layout_dynamic_children;/* layout_dynamic_children	*/
	XfeGeometryProc		get_child_dimensions;	/* get_child_dimensions		*/
	XtPointer			extension;				/* extension				*/

} XfeDynamicManagerClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManagerClassRec											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDynamicManagerClassRec
{
	CoreClassPart				core_class;
	CompositeClassPart			composite_class;
	ConstraintClassPart			constraint_class;
	XmManagerClassPart			manager_class;
	XfeManagerClassPart			xfe_manager_class;
	XfeDynamicManagerClassPart	xfe_dynamic_manager_class;
} XfeDynamicManagerClassRec;

externalref XfeDynamicManagerClassRec xfeDynamicManagerClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManagerPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDynamicManagerPart
{
	/* Callback Resources */

	/* Dynamic children resources */
	XfeLinked			dynamic_children;		/* Dynamic children			*/

	Dimension			max_dyn_width;			/* Max dyn width			*/
	Dimension			max_dyn_height;			/* Max dyn height			*/

	Dimension			min_dyn_width;			/* Min dyn width			*/
	Dimension			min_dyn_height;			/* Min dyn height			*/

	Cardinal			num_dyn_children;		/* Num dyn children			*/
	Cardinal			num_managed_dyn_children;/* Num managed dyn children*/

	Dimension			total_dyn_width;		/* Total dyn width			*/
	Dimension			total_dyn_height;		/* Total dyn height			*/

	/* Private Data Members */

} XfeDynamicManagerPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManagerRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDynamicManagerRec
{
	CorePart				core;
	CompositePart			composite;
	ConstraintPart			constraint;
	XmManagerPart			manager;
	XfeManagerPart			xfe_manager;
	XfeDynamicManagerPart	xfe_dynamic_manager;
} XfeDynamicManagerRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManagerConstraintPart										*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDynamicManagerConstraintPart
{
    int					position_index;			/* Position Index		*/
	XfeLinkNode			link_node;				/* Link node			*/
} XfeDynamicManagerConstraintPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManagerConstraintRec										*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDynamicManagerConstraintRec
{
	XmManagerConstraintPart			manager;
	XfeManagerConstraintPart		xfe_manager;
	XfeDynamicManagerConstraintPart	xfe_dynamic_manager;
} XfeDynamicManagerConstraintRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager Method invocation functions						*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
_XfeDynamicManagerAcceptDynamicChild		(Widget			child);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeDynamicManagerInsertDynamicChild		(Widget			child);
/*----------------------------------------------------------------------*/
extern Boolean
_XfeDynamicManagerDeleteDynamicChild		(Widget			child);
/*----------------------------------------------------------------------*/
extern void
_XfeDynamicManagerLayoutDynamicChildren		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeDynamicManagerGetChildDimensions		(Widget			child,
											 Dimension *	width_out,
											 Dimension *	height_out);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager private functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeDynamicManagerChildrenInfo				(Widget			w,
											 Dimension *	max_width_out,
											 Dimension *	max_height_out,
											 Dimension *	total_width_out,
											 Dimension *	total_height_out,
											 Cardinal *		num_managed_out,
											 Cardinal *		num_components_out);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager member access										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemDynamicChildren(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . dynamic_children)
/*----------------------------------------------------------------------*/
#define _XfemNumDynamicChildren(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . num_dyn_children)
/*----------------------------------------------------------------------*/
#define _XfemNumManagedDynamicChildren(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . num_managed_dyn_children)
/*----------------------------------------------------------------------*/
#define _XfemMaxDynamicWidth(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . max_dyn_width)
/*----------------------------------------------------------------------*/
#define _XfemMaxDynamicHeight(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . max_dyn_height)
/*----------------------------------------------------------------------*/
#define _XfemMinDynamicWidth(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . min_dyn_width)
/*----------------------------------------------------------------------*/
#define _XfemMinDynamicHeight(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . min_dyn_height)
/*----------------------------------------------------------------------*/
#define _XfemTotalDynamicWidth(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . total_dyn_width)
/*----------------------------------------------------------------------*/
#define _XfemTotalDynamicHeight(w) \
(((XfeDynamicManagerWidget) (w))->xfe_dynamic_manager . total_dyn_height)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Dynamic children count												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemDynamicChildrenCount(w) \
(_XfemDynamicChildren(w) ? XfeLinkedCount(_XfemDynamicChildren(w)) : 0)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Dynamic children indexing macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfemDynamicChildrenIndex(w,i) \
(_XfemDynamicChildren(w) ? XfeLinkedItemAtIndex(_XfemDynamicChildren(w),i) : NULL)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager child constraint part access macro					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeDynamicManagerConstraintPart(w) \
(&(((XfeDynamicManagerConstraintRec *) _XfeConstraints(w)) -> xfe_dynamic_manager))

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDynamicManager child individual constraint resource access macro	*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeDynamicManagerPositionIndex(w) \
(_XfeDynamicManagerConstraintPart(w)) -> position_index
/*----------------------------------------------------------------------*/
#define _XfeDynamicManagerLinkNode(w) \
(_XfeDynamicManagerConstraintPart(w)) -> link_node
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif										/* end DynamicManagerP.h	*/
