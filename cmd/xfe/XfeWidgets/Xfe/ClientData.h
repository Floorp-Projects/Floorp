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
/* Name:		<Xfe/ClientData.h>										*/
/* Description:	Manager of Arbitrary Client Data for widgets & gadget.	*/
/*																		*/
/*				API for associating arbitrary data with widget and 		*/
/*				gadgets.  The widget/gadget realized, managed or alive	*/
/*				state does not matter.  The association can occur at  	*/
/*				any stage in the widget/gadget life via                 */
/*				XfeClientDataRemove().									*/
/*																		*/
/*				The associated client data will be valid until the 		*/
/*				widget/gadget is destroyed or it is removed via			*/
/*				XfeClientDataRemove().									*/
/*																		*/
/*				A simple garbage collection mechaism is provided via	*/
/*				a callback that is invoked when the widget/gadget is	*/
/*				destroyed (XfeClientDataFreeFunc)						*/
/*																		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeClientData_h_						/* start ClientData.h	*/
#define _XfeClientData_h_

#include <Xfe/Xfe.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeClientDataFreeFunc												*/
/*																		*/
/* This function is invoked when an item is being freed.  This usually	*/
/* happens when the associated widget or gadget is destroyed.			*/
/*																		*/
/* This function can be use to free any memory allocated by the client	*/
/* and stored in the client_data.										*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void
(*XfeClientDataFreeFunc)		(Widget				w,
								 XtPointer			client_data);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeClientDataKey														*/
/*																		*/
/* All XfeClientData operations are performed with the help of a unique */
/* key.  Use XfeClientGetUniqueKey() to obtain a unique key.            */
/*																		*/
/*----------------------------------------------------------------------*/
typedef int XfeClientDataKey;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeClientDataAdd														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeClientDataAdd				(Widget					w,
								 XfeClientDataKey		key,
								 XtPointer				client_data,
								 XfeClientDataFreeFunc	free_func);
/*----------------------------------------------------------------------*/
extern XtPointer
XfeClientDataRemove				(Widget					w,
								 XfeClientDataKey		key);
/*----------------------------------------------------------------------*/
extern XtPointer
XfeClientDataGet				(Widget					w,
								 XfeClientDataKey		key);
/*----------------------------------------------------------------------*/
extern void
XfeClientDataSet				(Widget					w,
								 XfeClientDataKey		key,
								 XtPointer				client_data,
								 XfeClientDataFreeFunc	free_func);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeClientGetUniqueKey												*/
/*																		*/
/* Obtain a unique key to use with XfeClientData operations.            */
/*																		*/
/*----------------------------------------------------------------------*/
extern XfeClientDataKey
XfeClientGetUniqueKey			(void);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ClientData.h		*/
