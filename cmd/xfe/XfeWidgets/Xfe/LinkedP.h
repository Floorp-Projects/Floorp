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
/* Name:		<Xfe/LinkedP.h>											*/
/* Description:	XfeLinked object tweaked to be used in widgets.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeLinkedP_h_							/* LinkedP.h			*/
#define _XfeLinkedP_h_

#include <Xfe/Linked.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLinkNodeRec														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeLinkNodeRec
{
   Cardinal						index;		/* Index of current node	*/
   XtPointer					item;		/* User's Data				*/
   struct _XfeLinkNodeRec *		next;		/* Next Item in List		*/
   struct _XfeLinkNodeRec *		prev;		/* Previous Item in List	*/
} XfeLinkNodeRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLinkedRec															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeLinkedRec
{
   XfeLinkNodeRec *				data;		/* Linked list of items		*/
   XfeLinkNodeRec *				head;		/* Index of head bucket		*/
   XfeLinkNodeRec *				tail;		/* Index of tail bucket		*/
   Cardinal						count;		/* Item Count				*/
} XfeLinkedRec; 

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif /* LinkedP.h */

