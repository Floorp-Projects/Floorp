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
/* Name:		<Xfe/FrameShellP.h>										*/
/* Description:	XfeFrameShell widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeFrameShellP_h_						/* start FrameShellP.h	*/
#define _XfeFrameShellP_h_

#include <Xfe/FrameShell.h>
#include <Xfe/ManagerP.h>
#include <X11/ShellP.h>
#include <Xm/VendorSEP.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShellClassPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;					/* extension		*/ 
} XfeFrameShellClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShellClassRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFrameShellClassRec
{
  	CoreClassPart				core_class;
	CompositeClassPart			composite_class;
	ShellClassPart				shell_class;
	WMShellClassPart			wm_shell_class;
	VendorShellClassPart		vendor_shell_class;
	TopLevelShellClassPart		top_level_shell_class;
    XfeFrameShellClassPart		xfe_frame_shell_class;
} XfeFrameShellClassRec;

externalref XfeFrameShellClassRec xfeFrameShellClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShellPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFrameShellPart
{
	/* Realization callback resources */
    XtCallbackList		realize_callback;		/* Realize cb			*/
    XtCallbackList		before_realize_callback;/* Before realize cb	*/

	/* Configure callback resources */
    XtCallbackList		resize_callback;		/* Resize cb			*/
    XtCallbackList		before_resize_callback;	/* Before resize cb		*/
    XtCallbackList		move_callback;			/* Move cb				*/

	/* WM Protocol callback resources */
    XtCallbackList		delete_window_callback;	/* Delete window cb		*/
    XtCallbackList		save_yourself_callback;	/* Save yourself cb		*/

	/* Mapping callbacks */
    XtCallbackList		first_map_callback;		/* First map cb			*/
    XtCallbackList		map_callback;			/* Map cb				*/
    XtCallbackList		unmap_callback;			/* Unmap cb				*/

	/* Title changed callback */
    XtCallbackList		title_changed_callback;	/* Title changed cb		*/

	/* Tracking resources */
	Boolean				track_delete_window;	/* Track selete window ?*/
	Boolean				track_editres;			/* Track editres ?		*/
	Boolean				track_mapping;			/* Track mapping ?		*/
	Boolean				track_position;			/* Track position ?		*/
	Boolean				track_save_yourself;	/* Track session man ?	*/
	Boolean				track_size;				/* Track size ?			*/

	/* Other resources */
	Boolean				has_been_mapped;		/* Has been mapped ?	*/
	Boolean				start_iconic;			/* Start iconic ?		*/

	Widget				bypass_shell;			/* Bypass shell			*/


    /* Private data -- Dont even look past this comment -- */

} XfeFrameShellPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShellRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeFrameShellRec
{
	CorePart				core;
	CompositePart			composite;
	ShellPart				shell;
	WMShellPart				wm;
	VendorShellPart			vendor;
	TopLevelShellPart		top_level_shell;
    XfeFrameShellPart		xfe_frame_shell;
} XfeFrameShellRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeFrameShellPart Access Macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeFrameShellPart(w) &(((XfeFrameShellWidget) w) -> xfe_frame_shell)

/*----------------------------------------------------------------------*/
/*																		*/
/* XmVendorShell Access Macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeVendorShellPart(w) &(((XmVendorShellExtObject) w) -> vendor)

/*----------------------------------------------------------------------*/
/*																		*/
/* WMShellPart Access Macro												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _WMShellPart(w) &(((WMShellWidget) w) -> wm)

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end FrameShellP.h	*/

