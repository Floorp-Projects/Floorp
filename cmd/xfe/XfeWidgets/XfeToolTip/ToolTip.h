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
/* Description:	XfeToolTip - TipString / DocString support.				*/
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
/* XfeToolTipStringCallback												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void
(*XfeToolTipStringObtainCallback)	(Widget				w,
									 XtPointer			clientData1,
									 XtPointer			clientData2,
									 XmString *			string_return,
									 Boolean *			need_to_free_string);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Callback Reasons														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef enum _XfeDocStringReason
{
    XfeDOC_STRING_CLEAR,							/* Clear (leave)	*/
	XfeDOC_STRING_SET								/* Set (enter)		*/
} XfeDocStringReason;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolTipStringCallback												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void
(*XfeToolTipDocStringCallback)		(Widget					w,
									 XtPointer				clientData1,
									 XtPointer				clientData2,
									 XfeDocStringReason		reason,
									 XmString				string);
/*----------------------------------------------------------------------*/

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
/* TipString public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeToolTipAddTipString				(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipRemoveTipString			(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipSetTipStringEnabledState	(Widget		w,
									 Boolean	state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipGetTipStringEnabledState	(Widget		w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString callback functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeToolTipSetTipStringCallback		(Widget				w,
									 XtCallbackProc		callback,
									 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipClearTipStringCallback	(Widget				w);


/*----------------------------------------------------------------------*/
/*																		*/
/* TipString global enabled / disable functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void			
XfeToolTipGlobalSetTipStringEnabledState	(Boolean state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipGlobalGetTipStringEnabledState	(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* Check whether the global tooltip is showing							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipIsTipStringShowing		(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeToolTipAddDocString				(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipRemoveDocString			(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipSetDocStringEnabledState	(Widget		w,
									 Boolean	state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipGetDocStringEnabledState	(Widget		w);

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString callback functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeToolTipSetDocStringCallback		(Widget				w,
									 XtCallbackProc		callback,
									 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern void
XfeToolTipClearDocStringCallback	(Widget				w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString global enabled / disable functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void			
XfeToolTipGlobalSetDocStringEnabledState	(Boolean state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeToolTipGlobalGetDocStringEnabledState	(void);

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ToolTip.h		*/
