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
/* Name:		<Xfe/PrefItemP.h>										*/
/* Description:	XfePrefItem widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfePrefItemP_h_						/* start PrefItemP.h	*/
#define _XfePrefItemP_h_

#include <Xfe/PrefItem.h>
#include <Xfe/ManagerP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefItemClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfePrefItemClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefItemClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefItemClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfePrefItemClassPart		xfe_pref_item_class;
} XfePrefItemClassRec;

externalref XfePrefItemClassRec xfePrefItemClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefItemPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefItemPart
{
	/* Frame resources */
	unsigned char		frame_type;				/* Frame type			*/
	Dimension			frame_thickness;		/* Frame thickness		*/

	unsigned char		item_type;				/* Item type			*/

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

} XfePrefItemPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefItemRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefItemRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfePrefItemPart		xfe_pref_item;
} XfePrefItemRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefItemPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePrefItemPart(w) &(((XfePrefItemWidget) w) -> xfe_pref_item)

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefItemP.h		*/

