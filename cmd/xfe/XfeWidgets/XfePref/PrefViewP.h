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
/* Name:		<Xfe/PrefViewP.h>										*/
/* Description:	XfePrefView widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfePrefViewP_h_						/* start PrefViewP.h	*/
#define _XfePrefViewP_h_

#include <Xfe/PrefView.h>
#include <Xfe/ManagerP.h>
#include <Xfe/Linked.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefViewClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfePrefViewClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefViewClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefViewClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfePrefViewClassPart		xfe_pref_view_class;
} XfePrefViewClassRec;

externalref XfePrefViewClassRec xfePrefViewClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefViewPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefViewPart
{
	/* Component Resources */
    Widget				panel_tree;				/* Panel tree			*/

	/* Panel resources */
	XfeLinked			panel_info_list;		/* Panel info list		*/

	Dimension			spacing;

    /* Private data -- Dont even look past this comment -- */
	GC					pref_GC;				/* Pref GC				*/

	XRectangle			pref_rect;				/* Pref rect			*/

} XfePrefViewPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefViewRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefViewRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfePrefViewPart		xfe_pref_view;
} XfePrefViewRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefViewConstraintPart											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefViewConstraintPart
{
	XmString		panel_title;				/* Panel's title		*/
	XmString		panel_sub_title;			/* Panel's sub title	*/
	Cardinal		num_groups;					/* Num groups in panel	*/
} XfePrefViewConstraintPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefViewConstraintRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefViewConstraintRec
{
    XmManagerConstraintPart			manager;
    XfeManagerConstraintPart		xfe_manager;
    XfePrefViewConstraintPart		xfe_pref_view;
} XfePrefViewConstraintRec;


/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefViewPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePrefViewPart(w) &(((XfePrefViewWidget) w) -> xfe_pref_view)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefViewPart child constraint part access macro					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePrefViewConstraintPart(w) \
(&(((XfePrefViewConstraintRec *) _XfeConstraints(w)) -> xfe_pref_view))

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefViewP.h		*/

