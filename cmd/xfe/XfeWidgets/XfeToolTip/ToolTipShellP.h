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
/* Name:		<Xfe/ToolTipShellP.h>									*/
/* Description:	XfeToolTipShell widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeToolTipShellP_h_					/* start ToolTipShellP.h*/
#define _XfeToolTipShellP_h_

#include <Xfe/ToolTipShell.h>
#include <Xfe/BypassShellP.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShellClassPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer				extension;			/* extension			*/
} XfeToolTipShellClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShellClassRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolTipShellClassRec
{
	CoreClassPart				core_class;
	CompositeClassPart			composite_class;
	ShellClassPart				shell_class;
	WMShellClassPart			wm_shell_class;
	VendorShellClassPart		vendor_shell_class;
    XfeBypassShellClassPart		xfe_bypass_shell_class;
    XfeToolTipShellClassPart	xfe_tool_tip_shell_class;
} XfeToolTipShellClassRec;

externalref XfeToolTipShellClassRec xfeToolTipShellClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShellPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolTipShellPart
{
    /* Tool tip resources */
	XmFontList			font_list;				/* Title font list		*/
	Widget				tool_tip_label;			/* Tool tip label		*/
	int					tool_tip_timeout;		/* Tool tip timeout		*/

	/* Enumeration resources */
    unsigned char		tool_tip_type;			/* Tool tip type		*/
    unsigned char		tool_tip_placement;		/* Tool tip placement	*/

	/* Offset resources */
	int					horizontal_offset;		/* Horizontal offset	*/
	int					vertical_offset;		/* Vertical offset		*/

    /* Private data -- Dont even look past this comment -- */
	XtIntervalId		tool_tip_timer_id;		/* Tool tip timer id	*/

} XfeToolTipShellPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShellRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeToolTipShellRec
{
	CorePart				core;
	CompositePart			composite;
	ShellPart				shell;
	WMShellPart				wm;
	VendorShellPart			vendor;
    XfeBypassShellPart		xfe_bypass_shell;
    XfeToolTipShellPart		xfe_tool_tip_shell;
} XfeToolTipShellRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShellPart access macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeToolTipShellPart(w) \
&(((XfeToolTipShellWidget) w) -> xfe_tool_tip_shell)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShell method invocation functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeToolTipShellLayoutTitle			(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeToolTipShellLayoutArrow			(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfeToolTipShellDrawHighlight		(Widget			w,
								 XEvent *		event,
								 Region			region,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
_XfeToolTipShellDrawTitleShadow		(Widget			w,
								 XEvent *		event,
								 Region			region,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipShell - superclass = XfeManager							*/
/*																		*/
/* Component preparation macros.										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_PREPARE_ARROW							XfePrepare1

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ToolTipShellP.h	*/

