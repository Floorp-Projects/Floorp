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
/* Name:		<Xfe/PrefGroupP.h>										*/
/* Description:	XfePrefGroup widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfePrefGroupP_h_						/* start PrefGroupP.h	*/
#define _XfePrefGroupP_h_

#include <Xfe/PrefGroup.h>
#include <Xfe/ManagerP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroupClassPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfePrefGroupClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroupClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefGroupClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfePrefGroupClassPart		xfe_pref_group_class;
} XfePrefGroupClassRec;

externalref XfePrefGroupClassRec xfePrefGroupClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroupPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefGroupPart
{
	/* Frame resources */
	unsigned char		frame_type;				/* Frame type			*/
	Dimension			frame_thickness;		/* Frame thickness		*/

	/* Title resources */
	Widget				title;					/* Title				*/
	unsigned char		title_alignment;		/* Title alignment		*/
    unsigned char		title_direction;		/* Title direction		*/
	XmFontList			title_font_list;		/* Title font list		*/
	Dimension			title_offset;			/* Title offset			*/
	XmString			title_string;			/* Title string			*/

	/* Widget names */
	String				title_widget_name;		/* Title widget name	*/
	String				frame_widget_name;		/* Frame widget name	*/

	Dimension			title_spacing;			/* Title spacing		*/

    /* Private data -- Dont even look past this comment -- */

} XfePrefGroupPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroupRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefGroupRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfePrefGroupPart	xfe_pref_group;
} XfePrefGroupRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefGroupPart Access Macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePrefGroupPart(w) &(((XfePrefGroupWidget) w) -> xfe_pref_group)

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefGroupP.h		*/

