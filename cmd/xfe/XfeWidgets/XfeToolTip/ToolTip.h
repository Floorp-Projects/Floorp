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
/* Name:		<Xfe/ToolTip.h>											*/
/* Description:	XfeToolTip - Tool tip and doc string support.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeToolTip_h_							/* start ToolTip.h		*/
#define _XfeToolTip_h_

#include <Xfe/ToolTipShell.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTip resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNdocumentationString			"documentationString"
#define XmNtipString					"tipString"

#define XmCDocumentationString			"DocumentationString"
#define XmCTipString					"TipString"

/*----------------------------------------------------------------------*/
/*																		*/
/* ToolTip label callback structure										*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
    int			reason;					/* Reason why CB was invoked	*/
    XEvent *	event;					/* Event that triggered CB		*/
    XmString 	label_return;			/* Label slot for caller		*/
    Boolean 	need_to_free_return;	/* Should the label be freed ?	*/
} XfeToolTipLabelCallbackStruct;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTip public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeToolTipAdd					(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipRemove				(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipSetEnabledState		(Widget		w,
								 Boolean	state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipGetEnabledState		(Widget		w);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Callback functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeToolTipSetStringCallback		(Widget				w,
								 XtCallbackProc		callback,
								 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipClearStringCallback	(Widget						w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipIsShowing() - Check whether the global tooltip is showing	*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipIsShowing				(void);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Enable / Disable functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void			XfeToolTipGlobalSetEnabledState		(Boolean state);
extern Boolean		XfeToolTipGlobalGetEnabledState		(void);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ToolTip.h		*/
