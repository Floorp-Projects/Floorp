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
/* XfeTipStringObtainCallback											*/
/*																		*/
/* This callback is invoked when an item that has tip string support is	*/
/* ready to post a tooltip.  An item is ready to post a tooltip right	*/
/* after the following two events occur in am immediate sequence:		*/
/*																		*/
/* 1.  The pointer Enters the item.										*/
/* 2.  A timeout expires without an intervening cancellation.			*/
/*																		*/
/* A cancellation occurs when:											*/
/*																		*/
/* 1.  The pointer leaves the item before the timeout expires.			*/
/* 2.  The item receives a Button or KetPress event.					*/
/*																		*/
/* This callback should return the following:							*/
/*																		*/ 
/* An XmString in 'string_return'										*/
/* 	   	   	   															*/
/* Should that string should be freed after use in 'need_to_free_string'*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void
(*XfeTipStringObtainCallback)	(Widget				w,
								 XtPointer			client_data1,
								 XtPointer			client_data2,
								 XmString *			string_return,
								 Boolean *			need_to_free_string);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Callback Reasons														*/
/*																		*/
/* The 'reason' given to the XfeDocStringCallback callaback below.		*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef enum _XfeDocStringReason
{
    XfeDOC_STRING_CLEAR,							/* Clear (leave)	*/
	XfeDOC_STRING_SET								/* Set (enter)		*/
} XfeDocStringReason;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeDocStringCallback													*/
/*																		*/
/* This callback is invoked when the pointer enters an item that has	*/
/* doc string support and a valid doc string is obtain.  A valid doc	*/
/* string can be obtained in 2 ways:									*/
/*																		*/
/* 1.  Via an XfeTipStringObtainCallback callback which can be          */
/*	   installed by XfeDocStringSetObtainCallback()						*/
/*																		*/
/* 2.  Via the XmNdocumentationString resource for the item which can	*/
/*	   be installed in an application resources file.					*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void
(*XfeDocStringCallback)			(Widget					w,
								 XtPointer				client_data1,
								 XtPointer				client_data2,
								 XfeDocStringReason		reason,
								 XmString				string);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeTipStringAdd						(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeTipStringRemove					(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeTipStringSetEnabledState			(Widget		w,
									 Boolean	state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeTipStringGetEnabledState			(Widget		w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString callback functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeTipStringSetObtainCallback	(Widget							w,
								 XfeTipStringObtainCallback		callback,
								 XtPointer						client_data1,
								 XtPointer						client_data2);
/*----------------------------------------------------------------------*/
extern void
XfeTipStringClearObtainCallback	(Widget							w);

/*----------------------------------------------------------------------*/
/*																		*/
/* TipString global enabled / disable functions							*/
/*																		*/
/* Enable and disable tip strings on a global basis.  You can use these	*/
/* functions to diable tip strings everywhere.  The individual enabled	*/
/* state of items with tip string support is not affected by these		*/
/* functions.															*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void			
XfeTipStringGlobalSetEnabledState		(Boolean state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeTipStringGlobalGetEnabledState		(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* Check whether the global tooltip is showing							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeTipStringIsShowing					(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString public methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeDocStringAdd						(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeDocStringRemove					(Widget		w);
/*----------------------------------------------------------------------*/
extern void
XfeDocStringSetEnabledState			(Widget		w,
									 Boolean	state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeDocStringGetEnabledState			(Widget		w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString callback functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeDocStringSetObtainCallback		(Widget						w,
									 XfeTipStringObtainCallback	callback,
									 XtPointer					client_data1,
									 XtPointer					client_data2);
/*----------------------------------------------------------------------*/
extern void
XfeDocStringClearObtainCallback		(Widget						w);
/*----------------------------------------------------------------------*/
extern void
XfeDocStringSetCallback				(Widget						w,
									 XfeDocStringCallback		callback,
									 XtPointer					client_data1,
									 XtPointer					client_data2);
/*----------------------------------------------------------------------*/
extern void
XfeDocStringClearCallback			(Widget						w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* DocString global enabled / disable functions							*/
/*																		*/
/* Enable and disable doc strings on a global basis.  You can use these	*/
/* functions to diable doc strings everywhere.  The individual enabled	*/
/* state of items with doc string support is not affected by these		*/
/* functions.															*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void			
XfeDocStringGlobalSetEnabledState	(Boolean state);
/*----------------------------------------------------------------------*/
extern Boolean
XfeDocStringGlobalGetEnabledState	(void);

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ToolTip.h		*/
