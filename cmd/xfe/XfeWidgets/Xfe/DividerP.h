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
/* Name:		<Xfe/DividerP.h>										*/
/* Description:	XfeDivider widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeDividerP_h_							/* start DividerP.h		*/
#define _XfeDividerP_h_

#include <Xfe/Divider.h>
#include <Xfe/OrientedP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDividerClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfeDividerClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDividerClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDividerClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
	XfeDynamicManagerClassPart	xfe_dynamic_manager_class;
    XfeOrientedClassPart		xfe_oriented_class;
    XfeDividerClassPart			xfe_divider_class;
} XfeDividerClassRec;

externalref XfeDividerClassRec xfeDividerClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDividerPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDividerPart
{
	/* Divider resources */
    Dimension			divider_fixed_size;		/* Divider fixed size	*/
    int					divider_percentage;		/* Divider percentage	*/
    Cardinal			divider_target;			/* Divider target		*/
    unsigned char		divider_type;			/* Sash shadow type		*/

    /* Private data -- Dont even look past this comment -- */
} XfeDividerPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDividerRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeDividerRec
{
    CorePart				core;
    CompositePart			composite;
    ConstraintPart			constraint;
    XmManagerPart			manager;
    XfeManagerPart			xfe_manager;
    XfeDynamicManagerPart	xfe_dynamic_manager;
    XfeOrientedPart			xfe_oriented;
    XfeDividerPart			xfe_divider;
} XfeDividerRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDividerPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeDividerPart(w) &(((XfeDividerWidget) w) -> xfe_divider)

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end DividerP.h		*/

