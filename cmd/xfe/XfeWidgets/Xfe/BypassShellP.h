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
/* Name:		<Xfe/BypassShellP.h>									*/
/* Description:	XfeBypassShell widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeBypassShellP_h_						/* start BypassShellP.h	*/
#define _XfeBypassShellP_h_

#include <Xfe/BypassShell.h>
#include <Xfe/ManagerP.h>
#include <X11/ShellP.h>
#include <Xm/VendorSEP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShellClassPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfeBypassShellClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShellClassRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBypassShellClassRec
{
  	CoreClassPart				core_class;
	CompositeClassPart			composite_class;
	ShellClassPart				shell_class;
	WMShellClassPart			wm_shell_class;
	VendorShellClassPart		vendor_shell_class;
    XfeBypassShellClassPart		xfe_bypass_shell_class;
} XfeBypassShellClassRec;

externalref XfeBypassShellClassRec xfeBypassShellClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShellPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBypassShellPart
{
	/* Realization callback resources */
    XtCallbackList		realize_callback;		/* Realize cb			*/
    XtCallbackList		before_realize_callback;/* Before realize cb	*/

	/* Mapping callbacks */
    XtCallbackList		map_callback;			/* Map cb				*/
    XtCallbackList		unmap_callback;			/* Unmap cb				*/

	XtCallbackList		change_managed_callback;/* Change managed cb	*/

	/* Shadow resources */
	Pixel				bottom_shadow_color;	/* Bottom shadow color	*/
	Pixel				top_shadow_color;		/* Top shadow color		*/
    Dimension			shadow_thickness;		/* Shadow Thickness		*/
    unsigned char		shadow_type;			/* Shadow Type			*/

	/* Cursor resources */
	Cursor				cursor;					/* Cursor				*/

	/* Other resources */
	Boolean				ignore_exposures;		/* Ignore_exposures 	*/

    /* Private data -- Dont even look past this comment -- */
	GC					bottom_shadow_GC;		/* Bottom shadow GC		*/
	GC					top_shadow_GC;			/* Top shadow GC		*/
	Widget				managed_child;			/* Managed child		*/

} XfeBypassShellPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShellRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeBypassShellRec
{
	CorePart				core;
	CompositePart			composite;
	ShellPart				shell;
	WMShellPart				wm;
	VendorShellPart			vendor;
    XfeBypassShellPart		xfe_bypass_shell;
} XfeBypassShellRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShellPart Access Macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeBypassShellPart(w) \
&(((XfeBypassShellWidget) w) -> xfe_bypass_shell)

/*----------------------------------------------------------------------*/
/*																		*/
/* XmVendorShell Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeVendorShellPart(w) &(((XmVendorShellExtObject) w) -> vendor)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShell private methods										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
_XfeBypassShellGlobalIsAlive				(void);
/*----------------------------------------------------------------------*/
extern Widget
_XfeBypassShellGlobalAccess				(void);
/*----------------------------------------------------------------------*/
extern Widget
_XfeBypassShellGlobalInitialize			(Widget		pw,
										 String		name,
										 Arg *		av,
										 Cardinal	ac);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end BypassShellP.h	*/

