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
/* Name:		<Xfe/Linked.h>											*/
/* Description:	XfeLinked object tweaked to be used in widgets.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeLinked_h_							/* start Linked.h		*/
#define _XfeLinked_h_

#include <X11/Intrinsic.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLinkedApplyProc type												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void	(*XfeLinkedApplyProc)		(XtPointer		item,
											 XtPointer		client_data);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLinkedTestFunc type											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef Boolean	(*XfeLinkedTestFunc)		(XtPointer		item,
											 XtPointer		client_data);
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLinkedCompareFunc												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef int	(*XfeLinkedCompareFunc)			(XtPointer		one,
											 XtPointer		two,
											 XtPointer		client_data);


typedef struct _XfeLinkedRec *			XfeLinked;
typedef struct _XfeLinkNodeRec *		XfeLinkNode;

/*----------------------------------------------------------------------*/
/*																		*/
/* Public Linked List Functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Cardinal			
XfeLinkedCount					(XfeLinked				list);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedHead					(XfeLinked				list);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedTail					(XfeLinked				list);
/*----------------------------------------------------------------------*/
extern XfeLinked	
XfeLinkedConstruct				(void);
/*----------------------------------------------------------------------*/
extern void				
XfeLinkedDestroy				(XfeLinked				list,
								 XfeLinkedApplyProc		proc,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern void				
XfeLinkedApply					(XfeLinked				list,
								 XfeLinkedApplyProc		proc,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedInsertAfter			(XfeLinked				list,
								 XfeLinkNode				node,
								 XtPointer					item);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedInsertBefore			(XfeLinked				list,
								 XfeLinkNode				node,
								 XtPointer					item);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedInsertAtTail			(XfeLinked				list,
								 XtPointer					item);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedInsertAtHead			(XfeLinked				list,
								 XtPointer					item);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedInsertOrdered			(XfeLinked				list,
								 XfeLinkedCompareFunc	func,
								 XtPointer					item,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XtPointer		
XfeLinkedRemoveNode				(XfeLinked				list,
								 XfeLinkNode				node);
/*----------------------------------------------------------------------*/
extern void				
XfeLinkedClear					(XfeLinked				list,
								 XfeLinkedApplyProc		proc,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedFindLT					(XfeLinked				list,
								 XfeLinkedCompareFunc	func,
								 XtPointer					item,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedFindGT					(XfeLinked				list,
								 XfeLinkedCompareFunc	func,
								 XtPointer					item,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedFindLE					(XfeLinked				list,
								 XfeLinkedCompareFunc	func,
								 XtPointer					item,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedFindGE					(XfeLinked				list,
								 XfeLinkedCompareFunc	func,
								 XtPointer					item,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedFindEQ					(XfeLinked				list,
								 XfeLinkedCompareFunc	func,
								 XtPointer					item,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedFind					(XfeLinked				list,
								 XfeLinkedTestFunc		func,
								 XtPointer					data);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedFindItem				(XfeLinked				list,
								 XtPointer					item);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkedIndex					(XfeLinked				list,
								 Cardinal					i);
/*----------------------------------------------------------------------*/
extern Boolean			
XfeLinkedPosition				(XfeLinked				list,
								 XtPointer					item,
								 Cardinal *					pos);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Public Link Node Functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkNodeNext					(XfeLinkNode				node);
/*----------------------------------------------------------------------*/
extern XfeLinkNode		
XfeLinkNodePrev					(XfeLinkNode				node);
/*----------------------------------------------------------------------*/
extern XtPointer		
XfeLinkNodeItem					(XfeLinkNode				node);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Linked.h			*/
