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
/* Name:		<Xfe/Linked.c>											*/
/* Description:	XfeLinked object tweaked to be used in widgets.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/LinkedP.h>
#include <assert.h>

/* Private Linked Prototypes */
static XfeLinkNode	InsertAtHead	(XfeLinked,XtPointer);
static XtPointer	RemoveAtHead	(XfeLinked);
static XtPointer	RemoveAtTail	(XfeLinked);

/*----------------------------------------------------------------------*/
/*																		*/
/* Memory macros														*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeLinkedMalloc(type,count) \
(type *) XtMalloc(sizeof(type) * (count))

#define _XfeLinkedFree(ptr) \
if (ptr) XtFree((char *) (ptr)) ; (ptr) = NULL

/*----------------------------------------------------------------------*/
/*																		*/
/* Constructor															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XfeLinked
XfeLinkedConstruct(void)
{
	/* Allocate the list */
	XfeLinked	list = list = _XfeLinkedMalloc(XfeLinkedRec,1);

	/* Initialize the list members */
	list->data	= NULL;
	list->head	= NULL;
	list->tail	= NULL;
	list->count	= 0;
   
	return list;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Destructor															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeLinkedDestroy(XfeLinked				list,
				 XfeLinkedApplyProc		proc,
				 XtPointer				data)
{
	XfeLinkNode	node = NULL;
	XfeLinkNode	next = NULL;
   
	assert( list != NULL );
   
	/* Traverse the list and free the nodes */
	for (node = list->head; node; node = next)
	{
		/* Remember the next link */
		next = node->next;
      
		/* Free the item data if a proc was given */
		if (proc)
		{
			(*proc)(node->item,data);
		}

		/* Free the actual node */
		_XfeLinkedFree(node);
	}

	/* Free the the list */
	_XfeLinkedFree(list);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Insert																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedInsertAfter(XfeLinked		list,
					 XfeLinkNode	node,
					 XtPointer		item)
{
	XfeLinkNodeRec *	new_node;
   
	assert( list != NULL );
	
	/* Insert the head item in the list */
	if (!node)
	{
		return InsertAtHead(list,item);
	}
   
	/* Allocate a new node */
	new_node = _XfeLinkedMalloc(XfeLinkNodeRec,1);

	/* Insert in the middle of the list */
	if (node->next)
	{
		/* Assign the Node components */
		new_node->item	= item;
		new_node->index = list->count;
		new_node->prev	= node;
		new_node->next	= node->next;

		/* Insert the new node */
		node->next->prev = new_node;
		node->next = new_node;
	}
	/* Insert an item on list tail */
	else
	{
		/* Assign the Node components */
		new_node->item	= item;
		new_node->index = list->count;
		new_node->prev	= node;
		new_node->next	= NULL;
      
		/* Insert the new node */
		node->next = new_node;
      
		/* Update the tail pointer */
		list->tail = new_node;
	}

	list->count++;

	return new_node;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedInsertBefore(XfeLinked		list,
					  XfeLinkNode	node,
					  XtPointer		item)
{
	XfeLinkNodeRec *	new_node;
   
	assert( list != NULL );
   
	/* Insert the head item in the list */
	if (!node) 
	{
		return InsertAtHead(list,item);
	}

	/* Allocate a new node */
	new_node = _XfeLinkedMalloc(XfeLinkNodeRec,1);

	
	/* Insert in the middle of the list */
	if (node->prev)
	{
		/* Assign the Node components */
		new_node->item	= item;
		new_node->index = list->count;
		new_node->prev	= node->prev;
		new_node->next	= node;
      
		/* Insert the new node */
		node->prev->next = new_node;
		node->prev = new_node;
	}
	/* Insert right before the list head */
	else
	{
		/* Assign the Node components */
		new_node->item	= item;
		new_node->index = list->count;
		new_node->prev	= NULL;
		new_node->next	= node;
      
		/* Insert the new node */
		node->prev = new_node;

		/* Update the head pointer */
		list->head = new_node;
	}

	list->count++;

	return new_node;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedInsertAtTail(XfeLinked list,XtPointer item)
{
	assert( list != NULL );

	/* Insert at the tail of the list and return new node */
	return XfeLinkedInsertAfter(list,XfeLinkedTail(list),item);
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedInsertAtHead(XfeLinked list,XtPointer item)
{
	assert( list != NULL );

	/* Insert at the head of the list and return new node */
	return XfeLinkedInsertAfter(list,XfeLinkedHead(list),item);
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedInsertOrdered(XfeLinked			list,
					   XfeLinkedCompareFunc	func,
					   XtPointer			item,
					   XtPointer			data)
{
	XfeLinkNode smaller = XfeLinkedFindLT(list,func,item,data);
	
	if (smaller)
	{
		return XfeLinkedInsertAfter(list,smaller,item);
	}
   
	return XfeLinkedInsertBefore(list,list->head,item);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Remove																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XtPointer
XfeLinkedRemoveNode(XfeLinked		list,
					XfeLinkNode		node)
{
	XtPointer	item;

	assert( list != NULL );
	assert( node != NULL );

	if (node == list->head)
	{
		return RemoveAtHead(list);
	}

	if (node == list->tail)
	{
		return RemoveAtTail(list);
	}

	/* Remove the node */
	node->prev->next = node->next;
	node->next->prev = node->prev;

	item = node->item;

	_XfeLinkedFree(node);

	/* Decrease the node count */
	list->count--;

	return item;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Clear																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeLinkedClear(XfeLinked				list,
			   XfeLinkedApplyProc		proc,
			   XtPointer				data)
{
	XfeLinkNode	node = NULL;
	XfeLinkNode	next = NULL;

	assert( list != NULL );

	/* Traverse the list and free the nodes */
	for (node = list->head; node; node = next)
	{
		/* Get the next link */
		next = node->next;

		/* Free the item data if a free_proc was given */
		if (proc) proc(node->item,data);

		/* Free the actual node */
		_XfeLinkedFree(node);
	}

	list->data		= NULL;
	list->head		= NULL;
	list->tail		= NULL;
	list->count	= 0;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Find																	*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedFindLT(XfeLinked				list,
				XfeLinkedCompareFunc	func,
				XtPointer				item,
				XtPointer				data)
{
	XfeLinkNode node;

	assert( list != NULL );
	assert( func != NULL );

	for (node = list->tail; node; node = node->prev)
	{
		if ((*func)(node->item,item,data) < 0)
		{
			return node;
		}
	}
   
	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedFindGT(XfeLinked				list,
				XfeLinkedCompareFunc	func,
				XtPointer				item,
				XtPointer				data)
{
	XfeLinkNode node;

	assert( list != NULL );
	assert( func != NULL );

	for (node = list->head; node; node = node->next)
	{
		if ((*func)(node->item,item,data) > 0)
		{
			return node;
		}
	}
   
	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode	
XfeLinkedFindLE(XfeLinked				list,
				XfeLinkedCompareFunc	func,
				XtPointer				item,
				XtPointer				data)
{
	XfeLinkNode node;

	assert( list != NULL );
	assert( func != NULL );

	for (node = list->tail; node; node = node->prev)
	{
		if ((*func)(node->item,item,data) <= 0)
		{
			return node;
		}
	}
   
	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedFindGE(XfeLinked				list,
				XfeLinkedCompareFunc	func,
				XtPointer				item,
				XtPointer				data)
{
	XfeLinkNode node;

	assert( list != NULL );
	assert( func != NULL );

	for (node = list->head; node; node = node->next)
	{
		if ((*func)(node->item,item,data) >= 0)
		{
			return node;
		}
	}
   
	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedFindEQ(XfeLinked				list,
				XfeLinkedCompareFunc	func,
				XtPointer				item,
				XtPointer				data)
{
	XfeLinkNode node;

	assert( list != NULL );
	assert( func != NULL );

	for (node = list->tail; node; node = node->prev)
	{
		if ((*func)(node->item,item,data) == 0)
		{
			return node;
		}
	}
   
	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedFind(XfeLinked			list,
			  XfeLinkedTestFunc	func,
			  XtPointer			data)
{
	XfeLinkNode node;
	
	assert( list != NULL );
	assert( func != NULL );

	/* Look for the head item equal to the given item data */
	for (node = list->tail; node; node = node->prev)
	{
		/* If the node is equalist, we found it */
		if ((*func)(node->item,data))
		{
			return node;
		}
	}
   
	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedFindNodeByItem(XfeLinked		list,
						XtPointer		item)
{
	XfeLinkNode node;

	assert( list != NULL );
   
	for (node = list->tail; node; node = node->prev)
	{
		if (node->item == item)
		{
			return node;
		}
	}
   
	return (XfeLinkNode) NULL;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Apply																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeLinkedApply(XfeLinked				list,
			   XfeLinkedApplyProc		proc,
			   XtPointer				data)
{
	XfeLinkNode node;

	assert( list != NULL );
	assert( proc != NULL );

	/* Traverse all the nodes */
	for (node = list->head; node; node = node->next)
	{
		/* Apply procedure to each item */
		(*proc)(node->item,data);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Access to private data												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Cardinal		
XfeLinkedCount(XfeLinked list)
{
	assert( list != NULL );

	return list->count;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode	
XfeLinkedHead(XfeLinked list)
{
	assert( list != NULL );

	return (XfeLinkNode) list->head;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode	
XfeLinkedTail(XfeLinked list)
{
	assert( list != NULL );

	return (XfeLinkNode) list->tail;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Index / Position														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkedNodeAtIndex(XfeLinked list,Cardinal i)
{
	XfeLinkNode	node;
	Cardinal	index = 0;
   
	assert( list != NULL );

	for (node = list->head; node; node = node->next)
	{
		if (index == i)
		{
			return node;
		}

		index++;
	}

	return (XfeLinkNode) NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeLinkedPosition(XfeLinked list,XtPointer item,Cardinal * pos)
{
	Cardinal		i;
	XfeLinkNodeRec *	node;

	assert( list != NULL );

	for (i=0,node=list->data; i < list->count; i++,node++)
	{
		if (node->item == item)
		{
			*pos = i;

			return True;
		}
	}

	return False;
}
/*----------------------------------------------------------------------*/
/* extern */ XtPointer
XfeLinkedItemAtIndex(XfeLinked list,Cardinal i)
{
	XfeLinkNode	node = XfeLinkedNodeAtIndex(list,i);

	if (node != NULL)
	{
		return node->item;
	}

	return (XtPointer) NULL;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Public functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkNodeNext(XfeLinkNode node)
{
	assert( node != NULL );

	return (XfeLinkNode) node->next;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeLinkNode
XfeLinkNodePrev(XfeLinkNode node)
{
	assert( node != NULL );

	return (XfeLinkNode) node->prev;
}
/*----------------------------------------------------------------------*/
/* extern */ XtPointer
XfeLinkNodeItem(XfeLinkNode node)
{
	assert( node != NULL );

	return node->item;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Pruvate functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XfeLinkNode
InsertAtHead(XfeLinked list,XtPointer item)
{
	XfeLinkNodeRec *	new_node;

	assert( list != NULL );

	/* Allocate a new node */
	new_node = _XfeLinkedMalloc(XfeLinkNodeRec,1);

	/* Assign the node components */
	new_node->item	= item;
	new_node->index	= 0;
	new_node->next	= NULL;
	new_node->prev	= NULL;

	/* Assign the list data */
	list->data = new_node;

	/* Update list components */
	list->count	= 1;
	list->head	= new_node;
	list->tail	= new_node;

	return new_node;
}
/*----------------------------------------------------------------------*/
static XtPointer
RemoveAtHead(XfeLinked list)
{
	XtPointer	item;
	XfeLinkNode	new_head;

	assert( list != NULL );

	/* Remember the item data */
	item = list->head->item;

	/* Make sure the head is not the only node */
	if (list->head->next)
	{
		/* The new head node */
		new_head = list->head->next;
      
		/* Forget the node being removed */
		new_head->prev = NULL;
      
		/* Free the node record */
		_XfeLinkedFree(list->head);
      
		/* Assign the list head pointer to the new head node */
		list->head = new_head;
	}
	/* Remove the one and only node */
	else
	{
		/* Free the node record */
		_XfeLinkedFree(list->head);

		/* Reset the list components */
		list->data = NULL;
		list->head = NULL;
		list->tail = NULL;
	}

	/* Decrease the node count */
	list->count--;
   
	return item;
}
/*----------------------------------------------------------------------*/
static XtPointer
RemoveAtTail(XfeLinked list)
{
	XtPointer	item;
	XfeLinkNode	new_tail;

	assert( list != NULL );

	/* Remember the item data */
	item = list->tail->item;

	/* Make sure the tail is not the only node */
	if (list->tail->prev)
	{
		/* The new tail node */
		new_tail = list->tail->prev;
      
		/* Forget the node being removed */
		new_tail->next = NULL;
      
		/* Free the node record */
		_XfeLinkedFree(list->tail);
      
		/* Assign the list tail pointer to the new tail node */
		list->tail = new_tail;
	}
	/* Remove the one and only node */
	else
	{
		/* Free the node record */
		_XfeLinkedFree(list->tail);

		/* Reset the list components */
		list->data = NULL;
		list->head = NULL;
		list->tail = NULL;
	}

	/* Decrease the node count */
	list->count--;

	return item;
}
/*----------------------------------------------------------------------*/
