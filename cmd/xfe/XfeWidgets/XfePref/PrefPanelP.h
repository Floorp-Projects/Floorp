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
/* Name:		<Xfe/PrefPanelP.h>										*/
/* Description:	XfePrefPanel widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfePrefPanelP_h_						/* start PrefPanelP.h	*/
#define _XfePrefPanelP_h_

#include <Xfe/PrefPanel.h>
#include <Xfe/ManagerP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanelClassPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfePrefPanelClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanelClassRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefPanelClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfePrefPanelClassPart		xfe_pref_panel_class;
} XfePrefPanelClassRec;

externalref XfePrefPanelClassRec xfePrefPanelClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanelPart														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefPanelPart
{
	/* Color resources */
	Pixel				title_background;		/* Title background		*/
	Pixel				title_foreground;		/* Title foreground		*/

	/* i18n resources */
    unsigned char		title_direction;		/* Title direction		*/
	Dimension			title_spacing;			/* Title spacing		*/

	/* Title resources */
	Widget				title;					/* Title				*/
	unsigned char		title_alignment;		/* Title alignment		*/
	XmFontList			title_font_list;		/* Title font list		*/
	XmString			title_string;			/* Title string			*/
	Widget				title_icon;				/* Title string			*/

	/* Sub title resources */
	Widget				sub_title;				/* Sub title			*/
	unsigned char		sub_title_alignment;	/* Sub title alignment	*/
	XmString			sub_title_string;		/* Sub title string		*/
	XmFontList			sub_title_font_list;	/* Sub title font list	*/

	/* Widget names */
	String				title_widget_name;		/* Title widget name	*/
	String				sub_title_widget_name;	/* Sub title widget name*/

    /* Private data -- Dont even look past this comment -- */

} XfePrefPanelPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanelRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefPanelRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XfeManagerPart		xfe_manager;
    XfePrefPanelPart	xfe_pref_panel;
} XfePrefPanelRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefPanelPart Access Macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePrefPanelPart(w) &(((XfePrefPanelWidget) w) -> xfe_pref_panel)

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefPanelP.h		*/

