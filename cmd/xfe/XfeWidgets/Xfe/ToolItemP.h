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
/* Name:		<Xfe/ToolItemP.h>										*/
/* Description:	XfeToolItem widget private header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeToolItemP_h_						/* start ToolItemP.h	*/
#define _XfeToolItemP_h_

#include <Xfe/ToolItem.h>
#include <Xfe/ManagerP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
   
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolItemClassPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer		extension;					/* Extension			*/ 
} XfeToolItemClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolItemClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolItemClassRec
{
	CoreClassPart				core_class;
	CompositeClassPart			composite_class;
	ConstraintClassPart			constraint_class;
	XmManagerClassPart			manager_class;
	XfeManagerClassPart			xfe_manager_class;
	XfeToolItemClassPart		xfe_tool_item_class;
} XfeToolItemClassRec;

externalref XfeToolItemClassRec xfeToolItemClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolItemPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolItemPart
{
    /* Resources */
	Widget				item;					/* Item					*/
    Widget				logo;					/* Logo					*/
    Dimension			spacing;				/* Spacing				*/

} XfeToolItemPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolItemRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolItemRec
{
   CorePart				core;
   CompositePart		composite;
   ConstraintPart		constraint;
   XmManagerPart		manager;
   XfeManagerPart		xfe_manager;
   XfeToolItemPart		xfe_tool_item;
} XfeToolItemRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolItemPart Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeToolItemPart(w) &(((XfeToolItemWidget) w) -> xfe_tool_item)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end ToolItemP.h		*/

